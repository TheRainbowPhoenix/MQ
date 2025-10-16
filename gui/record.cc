#include "./record.h"

#include <string.h>
#include <stdbool.h>

// ffmpeg does not properly export information
extern "C" {
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>

#include <stb_image_write.h>
}

#include <mq/defs.h>

//---
// ffmpeg backend
//---

/* internal ffmpeg information */
struct _mqFfmpeg {
    struct SwsContext *scale_ctx;
    const AVOutputFormat *format_out;
    AVFormatContext *format_ctx;
    AVStream *stream;
    AVCodecContext *codec_ctx;
    AVFrame *rgbpic;
    AVFrame *yuvpic;
    AVPacket *packet;

    uint iframe;

    uint frameRate;
    uint height_in;
    uint width_in;
    uint width_out;
    uint height_out;
    mqDisplay_format vram_fmt;
    uint scale_factor;

    bool initialized;
};
static struct _mqFfmpeg ffmpeg;

/* _ffmpeg_init() - initialize ffmpeg information */
static int _ffmpeg_init(
    mqRecord *record,
    mqRecordRequest *request,
    mqDisplay *display
) {
    int ret;

    //fixme: assert record != NULL
    if(display == nullptr || request == nullptr)
        return record_set_error(record, -99, "ffmpeg_init: broken args");

    // register the backend now, to have potential error context as soon
    // as possible even with a broken backend
    // todo: use alloc instead of static?

    // prepare ffmpeg information
    memset(&ffmpeg, 0x00, sizeof(struct _mqFfmpeg));
    ffmpeg.frameRate = request->frameRate;
    ffmpeg.height_in = display->height;
    ffmpeg.width_in = display->width;
    ffmpeg.vram_fmt = display->format;
    ffmpeg.initialized = false;
    ffmpeg.scale_factor = request->scale_factor;

    // prepare scalling/conversion context
    // - this will be used to convert raw data into YUV format
    // - use YUV420P format because the VP9 only support YUV* and GRB*
    // - prepare scalling request
    ffmpeg.width_out = ffmpeg.width_in * ffmpeg.scale_factor;
    ffmpeg.height_out = ffmpeg.height_in * ffmpeg.scale_factor;
    ffmpeg.scale_ctx = sws_getContext(
        ffmpeg.width_in, ffmpeg.height_in, AV_PIX_FMT_RGB565LE,
        ffmpeg.width_out, ffmpeg.height_out, AV_PIX_FMT_YUV420P,
        SWS_POINT,
        NULL, NULL,
        NULL
    );
    if (ffmpeg.scale_ctx == NULL) {
        return record_set_error(
            record, -1,
            "ffmpeg_init: could not allocate scale/conv context"
        );
    }

    // Preparing the data concerning the format and codec in order to
    // write properly the header, frame data and end of file.
    // @notes
    // - use guessing feature to find mp4 information
    ffmpeg.format_out = av_guess_format("mp4", NULL, NULL);
    if (ffmpeg.format_out == NULL) {
        return record_set_error(
            record, -2,
            "ffmpeg_init: could not guess output file format"
        );
    }
    ret = avformat_alloc_output_context2(
        &ffmpeg.format_ctx, NULL, NULL, request->filename
    );
    if (ret < 0) {
        return record_set_error(
            record, -3,
            "ffmpeg_init: could not allocate format context"
        );
    }

    // Setting up the codec:
    // @docs
    // - use the VP9 video codec in combination with container mp4 format
    // - we need to configure at least the CRF codec option, otherwise a
    //      warning will be displayer saying "Neither bitrate nor constrained
    //      quality specified, using default CRF of 32". So, use the default
    //      value
    const AVCodec *codec = avcodec_find_encoder_by_name("libvpx-vp9");
    if (codec == NULL) {
        return record_set_error(
            record, -4,
            "ffmpeg_init: could not allocate codec"
        );
    }
    AVDictionary* codec_opt = NULL;
    if (av_dict_set(&codec_opt, "crf", "32", 0) < 0) {
        return record_set_error(
            record, -5,
            "ffmpeg_init: could not generate codec options"
        );
    }

    // setup stream
    // @docs
    // - create a new stream, only a video one
    // - indicate the frame-rate
    ffmpeg.stream = avformat_new_stream(ffmpeg.format_ctx, codec);
    if (ffmpeg.stream == NULL) {
        return record_set_error(
            record, -6,
            "ffmpeg_init: could not allocate stream memory"
        );
    }
    ffmpeg.stream->time_base = (AVRational){ 1, (int)ffmpeg.frameRate };

    // setup codec context
    // @docs
    // - indicate frame geometry and color encoding
    // - indicate frame-rate
    // - initialize the codec context (avcodec_open2())
    // - initialize the stream codec parameters
    ffmpeg.codec_ctx = avcodec_alloc_context3(codec);
    if (ffmpeg.codec_ctx == NULL) {
        return record_set_error(
            record, -7,
            "ffmpeg_init: could not alloc video codec context"
        );
    }
    ffmpeg.codec_ctx->width = ffmpeg.width_out;
    ffmpeg.codec_ctx->height = ffmpeg.height_out;
    ffmpeg.codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ffmpeg.codec_ctx->time_base = (AVRational){ 1, (int)ffmpeg.frameRate };
    if (avcodec_open2(ffmpeg.codec_ctx, codec, &codec_opt) < 0) {
        return record_set_error(
            record, -8,
            "ffmpeg_init: could not open codec"
        );
    }
    ret = avcodec_parameters_from_context(
        ffmpeg.stream->codecpar,
        ffmpeg.codec_ctx
    );
    if (ret < 0) {
        return record_set_error(
            record, -9,
            "ffmpeg_init: could not initialize stream parameters"
        );
    }

    // Once the codec is set up, we need to let the container know
    // which codec are the streams using, in this case the only (video)
    // stream.
    // av_dump_format - display detailed information
    // avio_open()    - request file to be openned in write-only mode
    // av_dump_format(ffmpeg.format_ctx, 0, filename, 1);
    ret = avio_open(
        &(ffmpeg.format_ctx->pb), request->filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
        return record_set_error(
            record, -10,
            "ffmpeg_init: could not open avio"
        );
    }

    // allocate the stream private data and write the stream header to
    // an output media files.
    //
    // @docs
    // - write output file header information
    // - freed allocated codec option (not needed anymore)
    //
    // @todo
    // - get a dict containing options that were not found and remove this
    //      from the original one
    if (avformat_write_header(ffmpeg.format_ctx, &codec_opt) < 0) {
        return record_set_error(
            record, -11,
            "ffmpeg_init: write header error"
        );
    }
    av_dict_free(&codec_opt);

    // Allocating memory for the conversion's input frame (raw data)
    // @notes
    // - allocate frame info
    // - init basic frame info
    // - allocate internal frame data memory (lets ffmpeg choose align)
    ffmpeg.rgbpic = av_frame_alloc();
    if (ffmpeg.rgbpic == NULL) {
        return record_set_error(
            record, -12,
            "ffmpeg_init: could not allocate RGB frame"
        );
    }
    ffmpeg.rgbpic->format = AV_PIX_FMT_RGB565LE;
    ffmpeg.rgbpic->width = ffmpeg.width_in;
    ffmpeg.rgbpic->height = ffmpeg.height_in;
    if (av_frame_get_buffer(ffmpeg.rgbpic, 0) != 0) {
        return record_set_error(
            record, -13,
            "ffmpeg_init: could not finish allocate RGB frame"
        );
    }

    // Allocating memory for each conversion output YUV frame.
    ffmpeg.yuvpic = av_frame_alloc();
    if (ffmpeg.yuvpic == NULL) {
        return record_set_error(
            record, -14,
            "ffmpeg_init: could not allocate YUV frame"
        );
    }
    ffmpeg.yuvpic->format = AV_PIX_FMT_YUV420P;
    ffmpeg.yuvpic->width = ffmpeg.width_out;
    ffmpeg.yuvpic->height = ffmpeg.height_out;
    if(av_frame_get_buffer(ffmpeg.yuvpic, 0) != 0) {
        return record_set_error(
            record, -15,
            "ffmpeg_init: could not finish allocate YUV frame"
        );
    }

    // Allocating packet
    // @docs
    // - used to communicate with ffmpeg during the sending of a frame
    ffmpeg.packet = av_packet_alloc();
    if (ffmpeg.packet == NULL) {
        return record_set_error(
            record, -16,
            "ffmpeg_init: unable to init packet!!"
        );
    }
    ffmpeg.packet->data = NULL;
    ffmpeg.packet->size = 0;

    // indicate that we have fully initialized the backend. This will be
    // used in `_ffmpeg_quit()` to know if we must send the last frames
    ffmpeg.initialized = true;

    mq_log(MQ_LOG_DEBUG, "ffmpeg init");
    return 0;
}

/* convert fxcg50 vram data */
static int _ffmpeg_frame_conv_rgb565(void *vram)
{
    uint8_t *vram8 = (uint8_t*)vram;
    int rgbp_idx;
    int vram_idx;

    for (uint y = 0; y < ffmpeg.height_in; y++) {
        rgbp_idx = (y * ffmpeg.rgbpic->linesize[0]);
        vram_idx = (y * 2) * ffmpeg.width_in;
        for (uint x = 0; x < ffmpeg.width_in; x++) {
            ffmpeg.rgbpic->data[0][rgbp_idx + 0] = vram8[vram_idx + 0];
            ffmpeg.rgbpic->data[0][rgbp_idx + 1] = vram8[vram_idx + 1];
            rgbp_idx += 2;
            vram_idx += 2;
        }
    }
    return 0;
}

/* convert mono vram data */
static int _ffmpeg_frame_conv_mono(void *vram)
{
    int rgbp_idx;
    int vram_idx;

    for (uint y = 0; y < ffmpeg.height_in; y++) {
        rgbp_idx = y * ffmpeg.rgbpic->linesize[0];
        vram_idx = y * ffmpeg.width_in;
        for (uint x = 0; x < ffmpeg.width_in; x++) {
            if(((uint8_t*)vram)[vram_idx] == 0xff) {
                ffmpeg.rgbpic->data[0][rgbp_idx + 0] = 0xff;
                ffmpeg.rgbpic->data[0][rgbp_idx + 1] = 0xff;
            } else {
                ffmpeg.rgbpic->data[0][rgbp_idx + 0] = 0x00;
                ffmpeg.rgbpic->data[0][rgbp_idx + 1] = 0x00;
            }
            rgbp_idx += 2;
            vram_idx += 1;
        }
    }
    return 0;
}

/* _ffmpeg_frame_add() - add a new frame to the file */
static int _ffmpeg_frame_add(mqRecord *record, mqDisplay *display)
{
    const AVFrame *yuvframe;
    int ret;

    // allow NULL vram to be requested. This is usefull to "force-flush"
    // potential pending frame at the closing file
    yuvframe = NULL;
    if (display != NULL)
    {
        if(ffmpeg.width_in != display->width) {
            mq_log(
                MQ_LOG_ERROR,
                "ffmpeg_frame_add: width %d != %d",
                ffmpeg.width_in,
                display->width
            );
            return record_set_error(
                record, -2,
                "ffmpeg_frame_add: mq display has changed (width)"
            );
        }
        if(ffmpeg.height_in != display->height) {
            return record_set_error(
                record, -2,
                "ffmpeg_frame_add: mq display has changed (height)"
            );
        }
        if(ffmpeg.vram_fmt != display->format) {
            return record_set_error(
                record, -2,
                "ffmpeg_frame_add: mq display has changed (format)"
            );
        }
        if(ffmpeg.vram_fmt == MQ_DISPLAY_FORMAT_L8) {
            _ffmpeg_frame_conv_mono(display->data);
        } else {
            _ffmpeg_frame_conv_rgb565(display->data);
        }
        ret = sws_scale_frame(
            ffmpeg.scale_ctx,
            ffmpeg.yuvpic,
            ffmpeg.rgbpic
        );
        if (ret < 0) {
            return record_set_error(
                record, -3,
                "ffmpeg_frame_add: unable to convert RGB to YUV"
            );
        }
        // The PTS of the frame are just in a reference unit,
        // unrelated to the format we are using. We set them,
        // for instance, as the corresponding frame number.
        ffmpeg.yuvpic->pts = ffmpeg.iframe++;
        yuvframe = ffmpeg.yuvpic;

        // update stat information (iframe, total_ms, ...)
        record_set_iframe(record, ffmpeg.iframe);
    }

    // send the frame and check error
    // @notes
    // - when we want to force flush pending frames, the EOF error occur
    //      add a special handle to help `_ffmpeg_quit()` to know that
    //      the flush is finished
    ret = avcodec_send_frame(ffmpeg.codec_ctx, yuvframe);
    if (display == NULL && ret == AVERROR_EOF)
        return 1;
    if (ret < 0) {
        return record_set_error(
            record, -4,
            "ffmpeg_frame_add: error sending frame to codec"
        );
    }

    // check status
    // - special check for EAGAIN -> return 0 (fake success write)
    // - special check for EOF    -> return 1
    // fixme: there is no memory leak here, but why?
    // fixme: how to correctly handle EAGAIN?!
    // fixme: update record error counter?
    ffmpeg.packet->data = NULL;
    ffmpeg.packet->size = 0;
    ret = avcodec_receive_packet(ffmpeg.codec_ctx, ffmpeg.packet);
    if(ret == AVERROR(EAGAIN)){
        mq_log(MQ_LOG_WARNING, "ffmpeg_frame_add: EAGAIN error, ignored");
        return 0;
    }
    if(ret == AVERROR_EOF) {
        mq_log(MQ_LOG_WARNING, "ffmpeg_frame_add: EOF error, ignored");
        return 1;
    }
    if(ret < 0) {
        return record_set_error(
            record, -5,
            "ffmpeg_frame_add: error receiving packet from codec"
        );
    }

    // handle PTS and DTS
    // We set the packet PTS and DTS taking in the account our FPS
    // (second argument), and the time base that our selected format
    // uses (third argument).
    av_packet_rescale_ts(
        ffmpeg.packet,
        (AVRational){ 1, (int)ffmpeg.frameRate },
        ffmpeg.stream->time_base
    );
    ffmpeg.packet->stream_index = ffmpeg.stream->index;

    // Write the encoded frame to the mp4 file.
    av_interleaved_write_frame(ffmpeg.format_ctx, ffmpeg.packet);
    av_packet_unref(ffmpeg.packet);
    return 0;
}

static int _ffmpeg_quit(mqRecord *record)
{
    //fixme: assert record != NULL

    // force flush pending frames
    if(ffmpeg.initialized) {
        while (true) {
            int ret = _ffmpeg_frame_add(record, NULL);
            if (ret == 1)
                break;
            if (ret < 0) {
                record_set_error(
                    record, ret,
                    "ffmpeg_quit: error sending NULL frame to codec"
                );
                break;
            }
        }
        if(ffmpeg.format_ctx != NULL)
            av_write_trailer(ffmpeg.format_ctx);
    }


    // Closing the file.
    if(ffmpeg.format_out != NULL && ffmpeg.format_ctx != NULL) {
        if(!(ffmpeg.format_out->flags & AVFMT_NOFILE))
            avio_closep(&ffmpeg.format_ctx->pb);
    }

    // Freeing all the allocated memory:
    if(ffmpeg.packet != NULL)
        av_packet_free(&ffmpeg.packet);
    if(ffmpeg.rgbpic != NULL)
        av_frame_free(&ffmpeg.rgbpic);
    if(ffmpeg.yuvpic != NULL)
        av_frame_free(&ffmpeg.yuvpic);
    if(ffmpeg.codec_ctx != NULL)
        avcodec_free_context(&ffmpeg.codec_ctx);
    if(ffmpeg.format_ctx != NULL) {
        avformat_free_context(ffmpeg.format_ctx);
        ffmpeg.format_ctx = NULL;
    }
    if(ffmpeg.scale_ctx != NULL) {
        sws_freeContext(ffmpeg.scale_ctx);
        ffmpeg.scale_ctx = NULL;
    }

    // reset all information
    memset(&ffmpeg, 0x00, sizeof(struct _mqFfmpeg));

    mq_log(MQ_LOG_DEBUG, "ffmpeg backend uninit success");
    return 0;
}

//=== Record setter =========================================================//

int record_set_iframe(mqRecord *record, uint iframe)
{
    //fixme: assert record != NULL
    record->stats.iframe   = iframe;
    record->stats.total_ms = record->stats.iframe * 17;
    record->stats.time_ms  = record->stats.total_ms % 1000;
    record->stats.time_sec = (record->stats.total_ms / 1000) % 60;
    record->stats.time_min = ((record->stats.total_ms / 1000) / 60) % 60;
    return 0;
}

int record_set_error(mqRecord *record, int ret, char const *error)
{
    //fixme: assert record != NULL
    record->error = error;
    if(error != nullptr) {
        mq_log(MQ_LOG_ERROR, "%s", error);
        record->stats.nb_error += 1;
    }
    return ret;
}

int record_set_status(mqRecord *record, mqRecordStatus status)
{
    //fixme: assert record != NULL
    switch(status) {
    case MQ_RECORD_STATUS_UNINIT:
        record->status = status;
        record->stats.status = "not initialized";
        return 0;
    case MQ_RECORD_STATUS_INIT:
        record->status = status;
        record->stats.status = "initialization...";
        return 0;
    case MQ_RECORD_STATUS_INIT_DELAY:
        record->status = status;
        record->stats.status = "initialization (delay)...";
        return 0;
    case MQ_RECORD_STATUS_START:
        record->status = status;
        record->stats.status = "recording...";
        return 0;
    case MQ_RECORD_STATUS_PAUSED:
        record->status = status;
        record->stats.status = "paused";
        return 0;
    default:
        return record_set_error(record, -99, "unknown status");
    }
}

int record_set_pathname(mqRecord *record, char const *pathname)
{
    //fixme: assert record != NULL
    //todo: expand path
    //todo: ensure path exists
    //todo: ensure filename format (date, ffmpeg?)
    record->stats.pathname_out = pathname;
    return 0;
}

//=== Record interface =======================================================//

int record_screenshot(
    mqRecord *record,
    mqRecordRequest *request,
    mqDisplay *display
) {
    if(request == nullptr || display == nullptr)
        return record_set_error(record, -99, "Arguments error");
    mq_log(MQ_LOG_DEBUG, "try to create a screenshot...");
    int i = 0;
    int comp = 3;
    size_t r_width = display->width * request->scale_factor;
    size_t r_height = display->height * request->scale_factor;
    u8 *vram_data = (u8*)malloc((r_width * comp) * r_height);
    if(!vram_data)
        return record_set_error(record, -1, "screenshot: image alloc fail");
    mq_log(MQ_LOG_DEBUG, "- convert data...");
    if(display->format == MQ_DISPLAY_FORMAT_L8) {
        u8 *data = (u8*)display->data;
        for (size_t y = 0 ; y < r_height ; y++) {
            for (size_t x = 0 ; x < r_width ; x++) {
                size_t src_y = y / request->scale_factor;
                size_t src_x = x / request->scale_factor;
                vram_data[i] = data[(display->width*src_y) + src_x];
                i += 1;
            }
        }
        comp = 1;
    }
    else {
        u16 *data = (u16*)display->data;
        for (size_t y = 0 ; y < r_height ; y++) {
            for (size_t x = 0 ; x < r_width ; x++) {
                size_t src_y = y / request->scale_factor;
                size_t src_x = x / request->scale_factor;
                u16 color = data[(display->width*src_y) + src_x];
                vram_data[(i * 3) + 0] = ((color >> 11) & 0b011111) << 3;
                vram_data[(i * 3) + 1] = ((color >> 5)  & 0b111111) << 2;
                vram_data[(i * 3) + 2] = ((color >> 0)  & 0b011111) << 3;
                i += 1;
            }
        }
    }
    mq_log(MQ_LOG_DEBUG, "screenshot: exported at \"%s\"", request->filename);
    stbi_write_png(
        request->filename,
        r_width,
        r_height,
        comp,
        vram_data,
        0
    );
    mq_log(MQ_LOG_DEBUG, "try to create a screenshot...SUCCESS");
    free(vram_data);
    return 0;
}

bool record_encoder_check(mqRecord *record, std::string const&encoder)
{
    char const *command_template = NULL;
    char buffer[256];

    (void)record;
    if(encoder == "hevc_qsv") {
        command_template = (
            "ffmpeg -hide_banner -f lavfi -i color=s=640x360 -frames 1 -an "
            "-load_plugin hevc_hw "
            "-c:v %s -f rawvideo pipe: "
            "-- > /dev/null 2>&1"
        );
    }
    else if(encoder.ends_with("_vaapi")) {
        command_template = (
            "ffmpeg -hide_banner -f lavfi -i color=s=640x360 -frames 1 -an "
            "-init_hw_device vaapi=vaapi0: -filter_hw_device vaapi0 -vf "
            "format=nv12,hwupload "
            "-c:v %s -f rawvideo pipe: "
            "-- > /dev/null 2>&1"
        );
    }
    else if(encoder.ends_with("_videotoolbox")) {
        command_template = (
            "ffmpeg -hide_banner -f lavfi -i color=s=640x360 -frames 1 -an "
            "-pix_fmt nv12 "
            "-c:v %s -f rawvideo pipe: "
            "-- > /dev/null 2>&1"
        );
    }
    else if(encoder.ends_with("_mf")) {
        command_template = (
            "ffmpeg -hide_banner -f lavfi -i color=s=640x360 -frames 1 -an "
            "-pix_fmt nv12 -hw_encoding true "
            "-c:v %s -f rawvideo pipe: "
            "-- > /dev/null 2>&1"
        );
    }
    else {
        command_template = (
            "ffmpeg -hide_banner -f lavfi -i color=s=640x360 -frames 1 -an "
            "-c:v %s -f rawvideo pipe: "
            "-- > /dev/null 2>&1"
        );
    }
    snprintf(buffer, 256, command_template, encoder.c_str());
    mq_log(MQ_LOG_DEBUG, buffer);
    return (system(buffer) == 0);
}

int record_init(
    mqRecord *record,
    mqRecordRequest *request,
    mqDisplay *display
) {
    //fixme: assert record != NULL
    if(request == nullptr || display == nullptr)
        return record_set_error(record, -99, "Arguments error");
    mq_log(MQ_LOG_DEBUG, "filename == %s", request->filename);
    record_set_status(record, MQ_RECORD_STATUS_UNINIT);
    record_set_error(record, 0, nullptr);
    record_set_iframe(record, 0);
    record_set_pathname(record, request->filename);
    int ret = _ffmpeg_init(record, request, display);
    if(ret < 0) {
        _ffmpeg_quit(record);
        return ret;
    }
    record_set_status(record, MQ_RECORD_STATUS_START);
    return 0;
}

int record_add_frame(mqRecord *record, mqDisplay *display)
{
    //fixme: assert record != NULL
    if(display == nullptr)
        return record_set_error(record, -99, "Machine not initialized");
    if(record->status == MQ_RECORD_STATUS_UNINIT)
        return record_set_error(record, -99, "Record not initialized");
    if(record->status != MQ_RECORD_STATUS_START)
        return 0;
    return _ffmpeg_frame_add(record, display);
}


void record_show(mqRecord *record)
{
    //fixme: assert record != NULL
    mq_log(MQ_LOG_DEBUG, "record:");
    mq_log(MQ_LOG_DEBUG, "|-- status = %d", record->status);
    mq_log(MQ_LOG_DEBUG, "`-- error = %s", record->error);
    mq_log(MQ_LOG_DEBUG, "ffmpeg:");
    mq_log(MQ_LOG_DEBUG, "|-- iframe = %d", ffmpeg.iframe);
    mq_log(MQ_LOG_DEBUG, "|-- width_in = %d", ffmpeg.width_in);
    mq_log(MQ_LOG_DEBUG, "|-- height_in = %d", ffmpeg.height_in);
    mq_log(MQ_LOG_DEBUG, "|-- width_out = %d", ffmpeg.width_out);
    mq_log(MQ_LOG_DEBUG, "|-- height_out = %d", ffmpeg.height_out);
    mq_log(MQ_LOG_DEBUG, "|-- scale = %d", ffmpeg.scale_factor);
    mq_log(MQ_LOG_DEBUG, "`-- format = %d", ffmpeg.vram_fmt);
}

int record_quit(mqRecord *record)
{
    //fixme: assert record != NULL
    if(record->status == MQ_RECORD_STATUS_UNINIT)
        return 0;
    //fixme: error handling
    _ffmpeg_quit(record);
    record_set_status(record, MQ_RECORD_STATUS_UNINIT);
    return 0;
}
