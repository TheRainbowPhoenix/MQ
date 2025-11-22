#include "./ffmpeg.h"

#if MQ_VIDEO_FFMPEG

//=== time ms ================================================================//

extern "C" {
#include <sys/time.h>
}

//todo: move me
//fixme: support window
// FIXME: Should probably use the machine's time source (once properly defined)
static u64 mq_utils_get_time_ms()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000LL + t.tv_usec / 1000;
}

//=== ffmpeg hwdevice ========================================================//

int mqFFmpeg::ffmpeg_hwdevice_config(
    AVCodecContext *codec_ctx,
    AVBufferRef **hw_device_ctx,
    enum AVHWDeviceType hw_device_type
) {
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *hw_frames_ctx;
    int err;

    hw_frames_ref = NULL;
    hw_frames_ctx = NULL;
    *hw_device_ctx = NULL;

    // create the hardware device context
    err = av_hwdevice_ctx_create(
        hw_device_ctx,
        hw_device_type,
        NULL,
        NULL,
        0
    );
    if (err < 0) {
        ffmpeg_error(err, "Failed to create a hwdevice");
        goto hwdevice_config_error;
    }
    // note: per-device output pixel format
    hw_frames_ref = av_hwframe_ctx_alloc(*hw_device_ctx);
    if (hw_frames_ref == NULL) {
        ffmpeg_error(ENOMEM, "Failed to create hwdevice frame");
        goto hwdevice_config_error;
    }
    hw_frames_ctx            = (AVHWFramesContext *)(hw_frames_ref->data);
    hw_frames_ctx->format    = codec_ctx->pix_fmt;
    hw_frames_ctx->sw_format = m_core.pix_fmt_out;
    hw_frames_ctx->width     = codec_ctx->width;
    hw_frames_ctx->height    = codec_ctx->height;
    hw_frames_ctx->initial_pool_size = 20;
    err = av_hwframe_ctx_init(hw_frames_ref);
    if(err < 0) {
        ffmpeg_error(ENOMEM, "Failed to initialize hwframe");
        goto hwdevice_config_error;
    }
    codec_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!codec_ctx->hw_frames_ctx) {
        ffmpeg_error(err, "Failed allocate hwframes");
        goto hwdevice_config_error;
    }
    av_buffer_unref(&hw_frames_ref);
    return 0;

hwdevice_config_error:
    av_buffer_unref(&hw_frames_ref);
    return err;
}

//=== ffmpeg codec ===========================================================//

bool mqFFmpeg::ffmpeg_codec_config(char const *encoder_name)
{
    AVDictionary *avcodec_opt   = NULL;
    AVCodecContext *avcodec_ctx = NULL;
    AVBufferRef *hwdevice_ctx   = NULL;
    const AVCodec *avcodec      = NULL;
    int err;

    m_core.codec_ctx = NULL;
    m_core.codec_opt = NULL;
    m_core.hwdevice_ctx = NULL;

    // generate PTS unit information
    m_core.time_base = (AVRational){ 1, m_config.fps };
    m_core.framerate = (AVRational){ m_config.fps, 1 };

    // try to find the provided codec
    avcodec = avcodec_find_encoder_by_name(encoder_name);
    if (avcodec == NULL)
        return ffmpeg_error(ENOMEM, "Could not find the provided codec");

    // allocate and default init codec context
    avcodec_ctx = avcodec_alloc_context3(avcodec);
    if(avcodec_ctx == NULL)
        return ffmpeg_error(ENOMEM, "avcodec avcodec_ctx alloc fail");
    avcodec_ctx->width     = m_config.width_out;
    avcodec_ctx->height    = m_config.height_out;
    avcodec_ctx->time_base = m_core.time_base;
    avcodec_ctx->framerate = m_core.framerate;

    // per-encoder codec context configuration
    avcodec_opt = NULL;
    err = av_dict_set(&avcodec_opt, "crf", "16", 0);
    if(err < 0) {
        ffmpeg_error(err, "Could not generate codec options");
        goto codec_config_error;
    }
    if(strstr(avcodec->name, "_vulkan")) {
        m_core.pix_fmt_out = AV_PIX_FMT_NV12;
        avcodec_ctx->pix_fmt = AV_PIX_FMT_VULKAN;
        err = ffmpeg_hwdevice_config(
            avcodec_ctx,
            &hwdevice_ctx,
            AV_HWDEVICE_TYPE_VULKAN
        );
        if(err < 0)
            goto codec_config_error;
    }
    else if(strstr(avcodec->name, "_vaapi")) {
        m_core.pix_fmt_out = AV_PIX_FMT_NV12;
        avcodec_ctx->pix_fmt = AV_PIX_FMT_VAAPI;
        err = ffmpeg_hwdevice_config(
            avcodec_ctx,
            &hwdevice_ctx,
            AV_HWDEVICE_TYPE_VAAPI
        );
        if(err < 0)
            goto codec_config_error;
    }
    else if(
        strstr(avcodec->name, "libvpx-vp9") ||
        strstr(avcodec->name, "libx264") ||
        strstr(avcodec->name, "nvenc")
    ) {
        m_core.pix_fmt_out = AV_PIX_FMT_YUV420P;
        avcodec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    }
    else {
        ffmpeg_error(EINVAL, "unsupported encoder \"%s\"", avcodec->name);
        goto codec_config_error;
    }

    // try to open the codec with configuration
    err = avcodec_open2(avcodec_ctx, avcodec, &avcodec_opt);
    if(err < 0) {
        ffmpeg_error(err, "unable to open the codec");
        goto codec_config_error;
    }

    // copy ffmpeg core info
    m_core.codec_ctx = avcodec_ctx;
    m_core.codec_opt = avcodec_opt;
    m_core.hwdevice_ctx = hwdevice_ctx;
    return true;

codec_config_error:
    av_dict_free(&avcodec_opt);
    return false;
}

/* be careful when calling this method. No check are performed on the
 * current backend state */
bool mqFFmpeg::ffmpeg_codec_exist(const char *codec_name)
{
    m_config.fps = 60;
    m_config.width_out = 640;
    m_config.height_out = 360;
    bool err = ffmpeg_codec_config(codec_name);
    avcodec_free_context(&m_core.codec_ctx);
    av_dict_free(&m_core.codec_opt);
    av_buffer_unref(&m_core.hwdevice_ctx);
    return err;
}

//=== file ===================================================================//

bool mqFFmpeg::ffmpeg_file_config(char const *pathname)
{
    AVFormatContext *format_ctx;
    AVStream *stream;
    AVPacket *packet;
    int err;

    format_ctx = NULL;
    stream     = NULL;
    packet     = NULL;

    // guess the output file format information based on the final file name
    err = avformat_alloc_output_context2(&format_ctx, NULL, NULL, pathname);
    if (err < 0)
        return ffmpeg_error(err, "Could not allocate format context");

    // create the video stream based on the selected codec
    stream = avformat_new_stream(format_ctx, NULL);
    if (stream == NULL) {
        ffmpeg_error(ENOMEM, "Could not allocate stream memory");
        goto file_config_error;
    }
    stream->time_base = m_core.time_base;
    err = avcodec_parameters_from_context(stream->codecpar, m_core.codec_ctx);
    if (err < 0) {
        ffmpeg_error(err, "Could not initialize stream parameters");
        goto file_config_error;
    }

    // try to open the output filename in write-only mode (since we will just
    // generate the video on-the-fly) and write the starting header
    // information
    err = avio_open(&(format_ctx->pb), pathname, AVIO_FLAG_WRITE);
    if (err < 0) {
        ffmpeg_error(err, "could not open avio");
        goto file_config_error;
    }
    err = avformat_write_header(format_ctx, &m_core.codec_opt);
    if (err < 0) {
        ffmpeg_error(err, "write header error");
        goto file_config_error;
    }

    // allocate packet used to send frame to the file
    packet = av_packet_alloc();
    if (packet == NULL) {
        ffmpeg_error(err, "Unable to init packet!!");
        goto file_config_error;
    }
    packet->data = NULL;
    packet->size = 0;

    // copy information
    m_core.format_ctx = format_ctx;
    m_core.stream_video = stream;
    m_core.packet = packet;
    m_core.iframe = 0;
    return true;

    // free all allocated memory
    // note that `stream` is freed during `format_ctx` cleanup
file_config_error:
    avio_close(format_ctx->pb);
    avformat_free_context(format_ctx);
    av_packet_free(&packet);
    return false;
}

int mqFFmpeg::ffmpeg_file_write_frame(AVFrame *frame)
{
    int err;

    // send the frame and check error
    err = avcodec_send_frame(m_core.codec_ctx, frame);
    if (err < 0)
        return ffmpeg_error(err, "Error sending frame to codec");

    // check status
    m_core.packet->data = NULL;
    m_core.packet->size = 0;
    while(err >= 0) {
        // receive the pending packet
        err = avcodec_receive_packet(m_core.codec_ctx, m_core.packet);
        if(err == AVERROR(EAGAIN))
            return 0;
        if(err == AVERROR_EOF)
            return 1;
        if(err < 0)
            return ffmpeg_error(err, "receive packet error");
        // We set the packet PTS and DTS taking in the account our FPS
        // (second argument), and the time base that our selected format
        // uses (third argument).
        av_packet_rescale_ts(
            m_core.packet,
            m_core.time_base,
            m_core.stream_video->time_base
        );
        m_core.packet->stream_index = m_core.stream_video->index;

        // Write the encoded frame to the mp4 file.
        err = av_interleaved_write_frame(m_core.format_ctx, m_core.packet);
        if (err != 0)
            ffmpeg_error(err, "av_interleaved_write_frame(): error");
        av_packet_unref(m_core.packet);
    }
    return err;
}

//=== scale ==================================================================//

bool mqFFmpeg::ffmpeg_scale_config()
{
    struct SwsContext *scale_ctx;
    AVFrame *frame_out_hw;
    AVFrame *frame_out;
    AVFrame *frame_in;
    int err;

    scale_ctx    = NULL;
    frame_out_hw = NULL;
    frame_out    = NULL;
    frame_in     = NULL;

    // vram-specific
    // note: use same between mono and color
    m_core.pix_fmt_in = AV_PIX_FMT_RGB565LE;

    // prepare scaling/conversion context
    scale_ctx = sws_getContext(
        m_config.width_in,
        m_config.height_in,
        m_core.pix_fmt_in,
        m_config.width_out,
        m_config.height_out,
        m_core.pix_fmt_out,
        SWS_POINT,
        NULL, NULL,
        NULL
    );
    if(scale_ctx == NULL) {
        ffmpeg_error(ENOMEM, "Could not allocate scale/conv context");
        goto scale_config_error;
    }

    // input frame allocation
    frame_in = av_frame_alloc();
    if (frame_in == NULL) {
        ffmpeg_error(ENOMEM, "could not allocate RGB frame");
        goto scale_config_error;
    }
    frame_in->format = m_core.pix_fmt_in;
    frame_in->height = m_config.height_in;
    frame_in->width  = m_config.width_in;
    err = av_frame_get_buffer(frame_in, 0);
    if (err < 0) {
        ffmpeg_error(err, "could not finish allocate IN frame");
        goto scale_config_error;
    }

    // output frame allocation
    frame_out = av_frame_alloc();
    if (frame_out == NULL) {
        ffmpeg_error(ENOMEM, "could not allocate OUT frame");
        goto scale_config_error;
    }
    frame_out->format = m_core.pix_fmt_out;
    frame_out->height = m_config.height_out;
    frame_out->width  = m_config.width_out;
    err = av_frame_get_buffer(frame_out, 0);
    if (err < 0) {
        ffmpeg_error(err, "could not finish allocate OUT frame");
        goto scale_config_error;
    }

    frame_out_hw = NULL;
    if(m_core.hwdevice_ctx) {
        frame_out_hw = av_frame_alloc();
        if (frame_out_hw == NULL) {
            ffmpeg_error(ENOMEM, "could not allocate OUT frame");
            goto scale_config_error;
        }
        err = av_hwframe_get_buffer(
            m_core.codec_ctx->hw_frames_ctx,
            frame_out_hw,
            0
        );
        if(err < 0) {
            ffmpeg_error(err, "could allocate OUT hardware frame");
            goto scale_config_error;
        }
    }

    // copy ffmpeg information
    m_core.scale_ctx = scale_ctx;
    m_core.frame_out = frame_out;
    m_core.frame_out_hw = frame_out_hw;
    m_core.frame_in  = frame_in;
    return true;

scale_config_error:
    sws_freeContext(scale_ctx);
    av_frame_free(&frame_out_hw);
    av_frame_free(&frame_out);
    av_frame_free(&frame_in);
    return false;
}

bool mqFFmpeg::ffmpeg_scale_conv(mqDisplay const *display)
{
    u8 *vram8;
    int idx_out;
    int idx_in;
    int err;

    if(display == NULL || display->data == NULL)
        return ffmpeg_error(EINVAL, "not display data available");
    if(display->width != m_config.width_in)
        return ffmpeg_error(EINVAL, "display width mismatch");
    if(display->height != m_config.height_in)
        return ffmpeg_error(EINVAL, "display height mismatch");
    if(display->format != m_config.vram_format)
        return ffmpeg_error(EINVAL, "display VRAM format mismatch");

    vram8 = (u8*)display->data;
    if(m_config.vram_format == MQ_DISPLAY_FORMAT_RGB565) {
        for (uint y = 0; y < m_config.height_in; y++) {
            idx_in = (y * 2) * m_config.width_in;
            idx_out = (y * m_core.frame_in->linesize[0]);
            for (uint x = 0; x < m_config.width_in; x++) {
                m_core.frame_in->data[0][idx_out + 0] = vram8[idx_in + 0];
                m_core.frame_in->data[0][idx_out + 1] = vram8[idx_in + 1];
                idx_out += 2;
                idx_in += 2;
            }
        }
    }
    else if(m_config.vram_format == MQ_DISPLAY_FORMAT_L8) {
        for (uint y = 0; y < m_config.height_in; y++) {
            idx_in = y * m_config.width_in;
            idx_out = y * m_core.frame_in->linesize[0];
            for (uint x = 0; x < m_config.width_in; x++) {
                if(vram8[idx_in] == 0xff) {
                    m_core.frame_in->data[0][idx_out + 0] = 0xff;
                    m_core.frame_in->data[0][idx_out + 1] = 0xff;
                } else {
                    m_core.frame_in->data[0][idx_out + 0] = 0x00;
                    m_core.frame_in->data[0][idx_out + 1] = 0x00;
                }
                idx_out += 2;
                idx_in += 1;
            }
        }
    }
    else {
        return ffmpeg_error(EINVAL, "unknown display format");
    }

    // convert ffmpeg orign vram into output vram
    err = sws_scale_frame(
        m_core.scale_ctx,
        m_core.frame_out,
        m_core.frame_in
    );
    if (err < 0)
        return ffmpeg_error(err, "unable to convert VRAM to OUT frame");
    return true;
}

bool mqFFmpeg::ffmpeg_scale_get_frame(
    AVFrame **frame_out,
    bool dyn_pts,
    bool force
) {
    u64 time_ms_ref_curr;
    int iframe;
    int err;

    if(frame_out == NULL)
        return ffmpeg_error(EINVAL, "missing frame_out (internal error)");
    *frame_out = NULL;

    // update frame time (PTS) information
    if(dyn_pts) {
        time_ms_ref_curr = mq_utils_get_time_ms();
        if(m_core.time_ms_ref == 0) {
            m_core.iframe = 0;
        } else {
            iframe  = (time_ms_ref_curr - m_core.time_ms_ref);
            iframe /= (1000 / m_config.fps);
            if(iframe == 0) {
                if(!force)
                    return true;
                iframe = 1;
            }
            m_core.iframe += iframe;
        }
        m_core.time_ms_ref = time_ms_ref_curr;
    } else {
        m_core.iframe++;
    }

    // get the final OUT frame and force PTS update
    *frame_out = m_core.frame_out;
    if(m_core.hwdevice_ctx) {
        m_core.frame_out->pts = m_core.iframe;
        err = av_hwframe_transfer_data(
            m_core.frame_out_hw,
            m_core.frame_out,
            0
        );
        if(err < 0)
            return ffmpeg_error(err, "unable to transfert OUT to OUT_HW");
        *frame_out = m_core.frame_out_hw;
    }
    (*frame_out)->pts = m_core.iframe;
    return true;
}

//=== mqFFmpeg RAII ==========================================================//

mqFFmpeg::mqFFmpeg()
{
    /* For now only count some software encoders. We'll extend the table with
       hardware encoders once we detect them later. */
    m_encoders.push_back("libx264");
    m_encoders.push_back("libvpx-vp9");
    m_software_encoder_count = m_encoders.size();
}

void mqFFmpeg::detectHardwareEncoders()
{
    if(m_hardware_encoders_detected)
        return;

    const AVCodec *codec;
    enum AVHWDeviceType hwdevice;
    std::vector<std::string> hwdevices;
    const char *hwdevice_name;
    std::string codec_name;
    void *i = 0;
    bool found;

    mq_log(MQ_LOG_DEBUG, "ffmpeg: detecting hardware encoders...");
    av_log_set_level(AV_LOG_QUIET);

    // detect all hardware device available. Note that the CUDA hardware
    // acceleration concern the NVENC encoder (and NVDEC/CUVID decoder not
    // used here) and all CUDA-related encoder use the nvenc name extension
    hwdevice = AV_HWDEVICE_TYPE_NONE;
    while ((hwdevice = av_hwdevice_iterate_types(hwdevice))) {
        if(hwdevice == AV_HWDEVICE_TYPE_NONE)
            break;
        hwdevice_name = av_hwdevice_get_type_name(hwdevice);
        if(strstr(hwdevice_name, "cuda"))
            hwdevices.push_back("nvenc");
        hwdevices.push_back(hwdevice_name);
    }

    while ((codec = av_codec_iterate(&i))) {
        if (!av_codec_is_encoder(codec))
            continue;
        if(codec->type != AVMEDIA_TYPE_VIDEO)
            continue;
        if(!(codec->capabilities & AV_CODEC_CAP_HARDWARE))
            continue;
        if(codec->capabilities & AV_CODEC_CAP_EXPERIMENTAL)
            continue;
        found = false;
        codec_name = codec->name;
        for (std::string const&hwdevice_name : hwdevices) {
            if(codec_name.ends_with(hwdevice_name)) {
                found = true;
                break;
            }
        }
        if(!found)
            continue;
        if(!ffmpeg_codec_exist(codec->name))
            continue;
        mq_log(MQ_LOG_DEBUG, "ffmpeg: successfully used '%s'", codec->name);
        m_encoders.push_back(codec_name);
    }

    m_hardware_encoders_detected = true;
}

//=== error handling =========================================================//

bool mqFFmpeg::ffmpeg_error(int averror, char const *format, ...)
{
    char buffer1[512];
    char buffer2[512];
    va_list ap;

    if(averror == EINVAL || averror == ENOMEM)
        averror = AVERROR(averror);
    va_start(ap, format);
    snprintf(buffer1, 512, "%s (%s)", format, av_err2str(averror));
    vsnprintf(buffer2, 512, buffer1, ap);
    va_end(ap);
    m_error_info = buffer2;
    return false;
}

//=== encoders ===============================================================//

std::string mqFFmpeg::encoder(unsigned int encoder_idx) const
{
    if(encoder_idx >= m_encoders.size())
        return "";
    return m_encoders[encoder_idx];
}

//=== recording ==============================================================//

bool mqFFmpeg::start(
    mqDisplay const *display,
    char const *pathname,
    char const *encoder_name,
    int scale,
    int fps
) {
    if(pathname == NULL || encoder_name == NULL)
        return ffmpeg_error(EINVAL, "ffmpeg::start(): broken argument");

    // input configuration
    m_config.fps = fps;
    m_config.width_in = display->width;
    m_config.height_in = display->height;
    m_config.width_out = display->width * scale;
    m_config.height_out = display->height * scale;
    m_config.vram_format = display->format;
    m_config.scale = scale;

    // manually reset core information to simplify cleanup
    m_core.codec_ctx = NULL;
    m_core.codec_opt = NULL;
    m_core.hwdevice_ctx = NULL;
    m_core.format_ctx = NULL;
    m_core.stream_video = NULL;
    m_core.packet = NULL;
    m_core.frame_out_hw = NULL;
    m_core.frame_out = NULL;
    m_core.frame_in = NULL;
    m_core.time_ms_ref = 0;
    m_core.iframe = 0;

    // encoding configuration
    if(!ffmpeg_codec_config(encoder_name)) {
        mq_log(MQ_LOG_ERROR, "mqFFmpeg::start(): codec config error");
        return false;
    }
    if(!ffmpeg_file_config(pathname)) {
        mq_log(MQ_LOG_ERROR, "mqFFmpeg::start(): file config error");
        return false;
    }
    if(!ffmpeg_scale_config()) {
        mq_log(MQ_LOG_ERROR, "mqFFmpeg::start(): scale config error");
        return false;
    }
    return true;
}

bool mqFFmpeg::frame_add(mqDisplay const *display, bool dyn_pts, bool force)
{
    AVFrame *frame_out;

    if(m_paused)
        return true;
    if(!ffmpeg_scale_conv(display)) {
        mq_log(MQ_LOG_ERROR, "mqFFmpeg::frame_add(): scale_conv() error");
        return false;
    }
    if(!ffmpeg_scale_get_frame(&frame_out, dyn_pts, force)) {
        mq_log(MQ_LOG_ERROR, "mqFFmpeg::frame_add(): scale_get() error");
        return false;
    }
    if(frame_out == NULL) {
        mq_log(MQ_LOG_DEBUG, "mqFFmpeg::frame_add(): frame skipped");
        return true;
    }
    if(ffmpeg_file_write_frame(frame_out) != 0) {
        mq_log(MQ_LOG_DEBUG, "mqFFmpeg::frame_add(): write frame error");
        return false;
    }
    return true;
}

bool mqFFmpeg::pause()
{
    m_paused = true;
    return true;
}

bool mqFFmpeg::unpause()
{
    m_paused = false;
    return true;
}

void mqFFmpeg::stats(struct mqFFmpegStats *stats)
{
    stats->iframe   = m_core.iframe;
    stats->total_ms = stats->iframe * (1000 / m_config.fps);
    stats->time_ms  = stats->total_ms % 1000;
    stats->time_sec = ((stats->total_ms / 1000) % 60);
    stats->time_min = ((stats->total_ms / 1000) / 60) % 60;
}

bool mqFFmpeg::stop()
{
    if(m_core.format_ctx) {
        // force-flush pending frame
        while (true) {
            int err = ffmpeg_file_write_frame(NULL);
            if(err > 0)
                break;
            if(err < 0) {
                mq_log(MQ_LOG_ERROR, "mqFFmpeg::stop() - flush fails");
                break;
            }
        }
        // Writing the end of the file.
        av_write_trailer(m_core.format_ctx);
        // Closing the file.
        avio_flush(m_core.format_ctx->pb);
        avio_close(m_core.format_ctx->pb);
    }

    // Freeing all the allocated memory:
    av_packet_free(&m_core.packet);
    sws_freeContext(m_core.scale_ctx);
    av_frame_free(&m_core.frame_out);
    av_frame_free(&m_core.frame_in);
    avformat_free_context(m_core.format_ctx);
    av_dict_free(&m_core.codec_opt);
    av_buffer_unref(&m_core.hwdevice_ctx);
    avcodec_free_context(&m_core.codec_ctx);

    // manually reset all information
    m_core.codec_ctx = NULL;
    m_core.codec_opt = NULL;
    m_core.hwdevice_ctx = NULL;
    m_core.format_ctx = NULL;
    m_core.stream_video = NULL;
    m_core.packet = NULL;
    m_core.frame_out_hw = NULL;
    m_core.frame_out = NULL;
    m_core.frame_in = NULL;
    m_core.iframe = 0;
    return true;
}

bool mqFFmpeg::debug()
{
    mq_log(MQ_LOG_DEBUG, "ffmpeg:");
    mq_log(MQ_LOG_DEBUG, "|-- core:");
    mq_log(MQ_LOG_DEBUG, "|   |-- codec_ctx: %p", m_core.codec_ctx);
    mq_log(MQ_LOG_DEBUG, "|   |-- codec_opt: %p", m_core.codec_opt);
    mq_log(MQ_LOG_DEBUG, "|   |-- hwdevice_ctx: %p", m_core.hwdevice_ctx);
    mq_log(MQ_LOG_DEBUG, "|   |-- format_ctx: %p", m_core.format_ctx);
    mq_log(MQ_LOG_DEBUG, "|   |-- stream_video: %p", m_core.stream_video);
    mq_log(MQ_LOG_DEBUG, "|   |-- packet: %p", m_core.packet);
    mq_log(MQ_LOG_DEBUG, "|   |-- scale_ctx: %p", m_core.scale_ctx);
    mq_log(MQ_LOG_DEBUG, "|   |-- frame_out_hw: %p", m_core.frame_out_hw);
    mq_log(MQ_LOG_DEBUG, "|   |-- frame_out: %p", m_core.frame_out);
    mq_log(MQ_LOG_DEBUG, "|   |-- frame_in: %p", m_core.frame_in);
    mq_log(MQ_LOG_DEBUG, "|   |-- pixfmt_out: %d", m_core.pix_fmt_out);
    mq_log(MQ_LOG_DEBUG, "|   |-- pixfmt_in: %d", m_core.pix_fmt_in);
    mq_log(MQ_LOG_DEBUG, "|   `-- iframe: %d", m_core.iframe);
    mq_log(MQ_LOG_DEBUG, "`-- config:");
    mq_log(MQ_LOG_DEBUG, "    |-- fps: %d", m_config.fps);
    mq_log(MQ_LOG_DEBUG, "    |-- height_in: %d", m_config.height_in);
    mq_log(MQ_LOG_DEBUG, "    |-- width_in: %d", m_config.width_in);
    mq_log(MQ_LOG_DEBUG, "    |-- height_out: %d", m_config.height_out);
    mq_log(MQ_LOG_DEBUG, "    |-- width_out: %d", m_config.width_out);
    mq_log(MQ_LOG_DEBUG, "    `-- scale: %d", m_config.scale);
    return true;
}

#endif /* MQ_VIDEO_FFMPEG */
