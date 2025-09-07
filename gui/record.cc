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
}

#include <mq/defs.h>

//---
// ffmpeg backend
//---

/* internal ffmpeg information */
struct _ffmpeg {
    struct SwsContext *scale_ctx;
    const AVOutputFormat *format_out;
    AVFormatContext *format_ctx;
    AVStream *stream;
    AVCodecContext *codec_ctx;
    AVFrame *rgbpic;
    AVFrame *yuvpic;
    AVPacket *packet;

    int iframe;

    int frameRate;
    uint height_in;
    uint width_in;
    uint width_out;
    uint height_out;
    mqDisplay_format vram_fmt;

    char const *error;
    bool initialized;
};
static struct _ffmpeg ffmpeg;


/* _ffmpeg_init() - initialize ffmpeg information */
static int _ffmpeg_init(struct _ffmpeg *ffmpeg, mqDisplay *display, char const *filename)
{
    int ret;

    // prepare ffmpeg information
    // todo: dynamic frameRate?
    memset(ffmpeg, 0x00, sizeof(struct _ffmpeg));
    ffmpeg->frameRate = 60;
    ffmpeg->height_in = display->height;
    ffmpeg->width_in = display->width;
    ffmpeg->vram_fmt = display->format;
    ffmpeg->error = NULL;
    ffmpeg->initialized = false;

    // prepare scalling/conversion context
    //
    // @notes
    // - this will be used to convert raw data into YUV format
    // - use raw VRAM data geometry/encoding of the fxcg50
    // - use same geometry but with RGB24 (arbitrary???)
    // @docs
    // - libswscale/swscale.h   - prototype information
    // - libavutils/pixfmt.h    - AVPixelFormat information
    if(ffmpeg->vram_fmt == MQ_DISPLAY_FORMAT_L8) {
        ffmpeg->width_out = 512;
        ffmpeg->height_out = 256;
    } else {
        ffmpeg->width_out = 792;
        ffmpeg->height_out = 448;
    }
    ffmpeg->scale_ctx = sws_getContext(
        ffmpeg->width_in, ffmpeg->height_in, AV_PIX_FMT_RGB565LE,
        ffmpeg->width_out, ffmpeg->height_out, AV_PIX_FMT_YUV420P,
        SWS_POINT,
        NULL, NULL,
        NULL
    );
    if (ffmpeg->scale_ctx == NULL) {
        ffmpeg->error = "Could not allocate scale/conv context";
        return -1;
    }

    // Preparing the data concerning the format and codec in order to
    // write properly the header, frame data and end of file.
    // @notes
    // - use guessing feature to find mp4 information
    ffmpeg->format_out = av_guess_format("mp4", NULL, NULL);
    if (ffmpeg->format_out == NULL) {
        ffmpeg->error = "Could not guess format";
        return -2;
    }
    ret = avformat_alloc_output_context2(
        &ffmpeg->format_ctx, NULL, NULL, filename
    );
    if (ret < 0) {
        ffmpeg->error = "Could not allocate format context";
        return -3;
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
        ffmpeg->error = "Could not allocate codec";
        return -4;
    }
    AVDictionary* codec_opt = NULL;
    if (av_dict_set(&codec_opt, "crf", "32", 0) < 0) {
        ffmpeg->error = "could not generate codec options";
        return -5;
    }

    // setup stream
    // @docs
    // - create a new stream, only a video one
    // - indicate the frame-rate
    ffmpeg->stream = avformat_new_stream(ffmpeg->format_ctx, codec);
    if (ffmpeg->stream == NULL) {
        ffmpeg->error = "Could not allocate stream memory";
        return -6;
    }
    ffmpeg->stream->time_base = (AVRational){ 1, ffmpeg->frameRate };

    // setup codec context
    // @docs
    // - indicate frame geometry and color encoding
    // - indicate frame-rate
    // - initialize the codec context (avcodec_open2())
    // - initialize the stream codec parameters
    ffmpeg->codec_ctx = avcodec_alloc_context3(codec);
    if (ffmpeg->codec_ctx == NULL) {
        ffmpeg->error = "Could not allocate video codec context";
        return -7;
    }
    ffmpeg->codec_ctx->width = ffmpeg->width_out;
    ffmpeg->codec_ctx->height = ffmpeg->height_out;
    ffmpeg->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ffmpeg->codec_ctx->time_base = (AVRational){ 1, ffmpeg->frameRate };
    if (avcodec_open2(ffmpeg->codec_ctx, codec, &codec_opt) < 0) {
        ffmpeg->error = "Could not open codec";
        return -8;
    }
    ret = avcodec_parameters_from_context(
        ffmpeg->stream->codecpar,
        ffmpeg->codec_ctx
    );
    if (ret < 0) {
        ffmpeg->error = "Could not initialize stream parameters\n";
        return -9;
    }

    // Once the codec is set up, we need to let the container know
    // which codec are the streams using, in this case the only (video)
    // stream.
    // av_dump_format - display detailed information
    // avio_open()    - request file to be openned in write-only mode
    // av_dump_format(ffmpeg->format_ctx, 0, filename, 1);
    ret = avio_open(&(ffmpeg->format_ctx->pb), filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
        ffmpeg->error = "could not open avio";
        return -10;
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
    if (avformat_write_header(ffmpeg->format_ctx, &codec_opt) < 0) {
        ffmpeg->error = "write header error";
        return -11;
    }
    av_dict_free(&codec_opt);

    // Allocating memory for the conversion's input frame (raw data)
    // @notes
    // - allocate frame info
    // - init basic frame info
    // - allocate internal frame data memory (lets ffmpeg choose align)
    ffmpeg->rgbpic = av_frame_alloc();
    if (ffmpeg->rgbpic == NULL) {
        ffmpeg->error = "could not allocate RGB frame";
        return -12;
    }
    ffmpeg->rgbpic->format = AV_PIX_FMT_RGB565LE;
    ffmpeg->rgbpic->width = ffmpeg->width_in;
    ffmpeg->rgbpic->height = ffmpeg->height_in;
    if (av_frame_get_buffer(ffmpeg->rgbpic, 0) != 0) {
        ffmpeg->error = "could not finish allocate RGB frame";
        return -13;
    }

    // Allocating memory for each conversion output YUV frame.
    ffmpeg->yuvpic = av_frame_alloc();
    if (ffmpeg->yuvpic == NULL) {
        ffmpeg->error = "could not allocate YUV frame";
        return -14;
    }
    ffmpeg->yuvpic->format = AV_PIX_FMT_YUV420P;
    ffmpeg->yuvpic->width = ffmpeg->width_out;
    ffmpeg->yuvpic->height = ffmpeg->height_out;
    if (av_frame_get_buffer(ffmpeg->yuvpic, 0) != 0) {
        ffmpeg->error = "could not finish allocate YUV frame";
        return -15;
    }

    // Allocating packet
    // @docs
    // - used to communicate with ffmpeg during the sending of a frame
    ffmpeg->packet = av_packet_alloc();
    if (ffmpeg->packet == NULL) {
        ffmpeg->error = "Unable to init packet!!";
        return -16;
    }
    ffmpeg->packet->data = NULL;
    ffmpeg->packet->size = 0;

    // indicate that we have fully initialized the backend. This will be
    // used in `_ffmpeg_quit()` to know if we must send the last frames
    ffmpeg->initialized = true;
    return 0;
}

/* convert fxcg50 vram data */
static int _ffmpeg_frame_conv_rgb565(struct _ffmpeg *ffmpeg, void *vram)
{
    uint8_t *vram8 = (uint8_t*)vram;
    int rgbp_idx;
    int vram_idx;

    for (uint y = 0; y < ffmpeg->height_in; y++) {
        rgbp_idx = (y * ffmpeg->rgbpic->linesize[0]);
        vram_idx = (y * 2) * ffmpeg->width_in;
        for (uint x = 0; x < ffmpeg->width_in; x++) {
            ffmpeg->rgbpic->data[0][rgbp_idx + 0] = vram8[vram_idx + 0];
            ffmpeg->rgbpic->data[0][rgbp_idx + 1] = vram8[vram_idx + 1];
            rgbp_idx += 2;
            vram_idx += 2;
        }
    }
    return 0;
}

/* convert mono vram data */
static int _ffmpeg_frame_conv_mono(struct _ffmpeg *ffmpeg, void *vram)
{
    int rgbp_idx;
    int vram_idx;

    for (uint y = 0; y < ffmpeg->height_in; y++) {
        rgbp_idx = y * ffmpeg->rgbpic->linesize[0];
        vram_idx = y * ffmpeg->width_in;
        for (uint x = 0; x < ffmpeg->width_in; x++) {
            if(((uint8_t*)vram)[vram_idx] == 0xff) {
                ffmpeg->rgbpic->data[0][rgbp_idx + 0] = 0xff;
                ffmpeg->rgbpic->data[0][rgbp_idx + 1] = 0xff;
            } else {
                ffmpeg->rgbpic->data[0][rgbp_idx + 0] = 0x00;
                ffmpeg->rgbpic->data[0][rgbp_idx + 1] = 0x00;
            }
            rgbp_idx += 2;
            vram_idx += 1;
        }
    }
    return 0;
}

/* _ffmpeg_frame_add() - add a new frame to the file */
static int _ffmpeg_frame_add(struct _ffmpeg *ffmpeg, mqDisplay *display)
{
    const AVFrame *yuvframe;
    int ret;

    if(ffmpeg == NULL) {
        mq_log(MQ_LOG_ERROR, "ffmpeg: frame_add: invalid arguments");
        return -1;
    }

    // allow NULL vram to be requested. This is usefull to "force-flush"
    // potential pending frame at the closing file
    yuvframe = NULL;
    if (display != NULL)
    {
        if(
            ffmpeg->width_in != display->width ||
            ffmpeg->height_in != display->height ||
            ffmpeg->vram_fmt != display->format
        ) {
            ffmpeg->error = "ffmpeg: mq internal display has changed";
            return -2;
        }
        if(ffmpeg->vram_fmt == MQ_DISPLAY_FORMAT_L8) {
            _ffmpeg_frame_conv_mono(ffmpeg, display->data);
        } else {
            _ffmpeg_frame_conv_rgb565(ffmpeg, display->data);
        }
        ret = sws_scale_frame(
            ffmpeg->scale_ctx,
            ffmpeg->yuvpic,
            ffmpeg->rgbpic
        );
        if (ret < 0) {
            ffmpeg->error = "unable to convert RGB to YUV";
            return -3;
        }
        // The PTS of the frame are just in a reference unit,
        // unrelated to the format we are using. We set them,
        // for instance, as the corresponding frame number.
        ffmpeg->yuvpic->pts = ffmpeg->iframe++;
        yuvframe = ffmpeg->yuvpic;
    }

    // send the frame and check error
    // @notes
    // - when we want to force flush pending frames, the EOF error occur
    //      add a special handle to help `_ffmpeg_quit()` to know that
    //      the flush is finished
    ret = avcodec_send_frame(ffmpeg->codec_ctx, yuvframe);
    if (display == NULL && ret == AVERROR_EOF)
        return 1;
    if (ret < 0) {
        ffmpeg->error = "Error sending frame to codec\n";
        return -4;
    }

    // check status
    // fixme: memory leak??
    ffmpeg->packet->data = NULL;
    ffmpeg->packet->size = 0;
    ret = avcodec_receive_packet(ffmpeg->codec_ctx, ffmpeg->packet);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        ffmpeg->error = "Error receiving packet from codec\n";
        return -5;
    }
    if (ret >= 0)
    {
        // We set the packet PTS and DTS taking in the account our FPS
        // (second argument), and the time base that our selected format
        // uses (third argument).
        av_packet_rescale_ts(
            ffmpeg->packet,
            (AVRational){ 1, ffmpeg->frameRate },
            ffmpeg->stream->time_base
        );
        ffmpeg->packet->stream_index = ffmpeg->stream->index;

        // Write the encoded frame to the mp4 file.
        av_interleaved_write_frame(ffmpeg->format_ctx, ffmpeg->packet);
        av_packet_unref(ffmpeg->packet);
        return 0;
    }
    ffmpeg->error = "weird receive packet error\n";
    return -6;
}

static void _ffmpeg_quit(struct _ffmpeg *ffmpeg)
{
    int ret;

    // force flush pending frames
    if(ffmpeg->initialized) {
        while (true) {
            ret = _ffmpeg_frame_add(ffmpeg, NULL);
            if (ret == 1)
                break;
            if (ret < 0) {
                ffmpeg->error = "Error sending NULL frame to codec\n";
                break;
            }
        }
    }

    // Writing the end of the file.
    if(ffmpeg->format_ctx != NULL)
        av_write_trailer(ffmpeg->format_ctx);

    // Closing the file.
    if(ffmpeg->format_out != NULL && ffmpeg->format_ctx != NULL) {
        if(!(ffmpeg->format_out->flags & AVFMT_NOFILE))
            avio_closep(&ffmpeg->format_ctx->pb);
    }

    // Freeing all the allocated memory:
    if(ffmpeg->packet != NULL)
        av_packet_free(&ffmpeg->packet);
    if(ffmpeg->rgbpic != NULL)
        av_frame_free(&ffmpeg->rgbpic);
    if(ffmpeg->yuvpic != NULL)
        av_frame_free(&ffmpeg->yuvpic);
    if(ffmpeg->codec_ctx != NULL)
        avcodec_free_context(&ffmpeg->codec_ctx);
    if(ffmpeg->format_ctx != NULL) {
        avformat_free_context(ffmpeg->format_ctx);
        ffmpeg->format_ctx = NULL;
    }
    if(ffmpeg->scale_ctx != NULL) {
        sws_freeContext(ffmpeg->scale_ctx);
        ffmpeg->scale_ctx = NULL;
    }

    // reset all information
    memset(ffmpeg, 0x00, sizeof(struct _ffmpeg));

    mq_log(MQ_LOG_DEBUG, "ffmpeg backend uninit success");
}

//=== Record interface ======================================================//

int record_quit(mqRecord *record)
{
    if(!record->initialized)
        return 0;
    _ffmpeg_quit(&ffmpeg);
    record->start = false;
    record->initialized = false;
    return 0;
}

int record_add_frame(mqRecord *record, mqMachine *mach)
{
    if(mach == nullptr || !mach->initialized || mach->display == nullptr) {
        record->error = "Machine not initialized";
        return -99;
    }
    _ffmpeg_frame_add(&ffmpeg, mach->display);
    return 0;
}

int record_init(mqRecord *record, mqMachine *mach)
{
    if(mach == nullptr || !mach->initialized || mach->display == nullptr) {
        record->error = "Machine not initialized";
        return -99;
    }
    record->start = false;
    record->initialized = false;
    if(_ffmpeg_init(&ffmpeg, mach->display, "record.mp4") < 0) {
        mq_log(MQ_LOG_ERROR, "ffmpeg: %s", ffmpeg.error);
        _ffmpeg_quit(&ffmpeg);
        return -1;
    }
    record->initialized = true;
    return 0;
}

