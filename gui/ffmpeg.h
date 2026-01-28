#ifndef MQ_UI_FFMPEG_H
#define MQ_UI_FFMPEG_H

#include <mq/defs.h>

#if MQ_VIDEO_FFMPEG

#include <string>
#include <vector>

#include <mq/interfaces/display.h>

// ffmpeg does not properly export information
extern "C" {
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
}

struct mqFFmpegStats
{
    int iframe;
    int total_ms;
    int time_ms;
    int time_sec;
    int time_min;
};

#define MQ_FFMPEG_INTERFACE_FUNCTIONS(X) \
    X(avutil, av_hwdevice_ctx_create) \
    X(avutil, av_hwdevice_iterate_types) \
    X(avutil, av_hwdevice_get_type_name) \
    X(avutil, av_hwframe_ctx_alloc) \
    X(avutil, av_hwframe_ctx_init) \
    X(avutil, av_hwframe_get_buffer) \
    X(avutil, av_hwframe_transfer_data) \
    X(avutil, av_buffer_ref) \
    X(avutil, av_buffer_unref) \
    X(avutil, av_dict_set) \
    X(avutil, av_dict_free) \
    X(avcodec, av_packet_alloc) \
    X(avcodec, av_packet_unref) \
    X(avcodec, av_packet_free) \
    X(avcodec, av_packet_rescale_ts) \
    X(avutil, av_frame_alloc) \
    X(avutil, av_frame_get_buffer) \
    X(avutil, av_frame_free) \
    X(avformat, av_interleaved_write_frame) \
    X(avformat, av_write_trailer) \
    X(avutil, av_log_set_level) \
    X(avcodec, av_codec_iterate) \
    X(avcodec, av_codec_is_encoder) \
    X(avformat, avio_open) \
    X(avformat, avio_flush) \
    X(avformat, avio_close) \
    X(avcodec, avcodec_find_encoder_by_name) \
    X(avcodec, avcodec_alloc_context3) \
    X(avcodec, avcodec_free_context) \
    X(avcodec, avcodec_open2) \
    X(avcodec, avcodec_parameters_from_context) \
    X(avcodec, avcodec_send_frame) \
    X(avcodec, avcodec_receive_packet) \
    X(avformat, avformat_alloc_output_context2) \
    X(avformat, avformat_free_context) \
    X(avformat, avformat_new_stream) \
    X(avformat, avformat_write_header) \

struct mqFFmpegInterface
{
#define DEFINE_FUNCTION_POINTER(LIBRARY, NAME) \
    decltype(::NAME) *NAME;
MQ_FFMPEG_INTERFACE_FUNCTIONS(DEFINE_FUNCTION_POINTER)
#undef MAKE_FUNCTION_POINTER
};

class mqFFmpeg
{
public:
    /* Try and dlopen system libraries to get access to a full build of ffmpeg
       with hardware encoders. Requires the system to have the appropriate
       dynamic libraries for the major version of ffmpeg that MQ was built for.
       Prints error messages on failure. The result is cached: the search only
       runs once, unless force=true. */
    const static struct mqFFmpegInterface *loadSystemLibraries(
        bool force=false);

    /* Construct an FFmpeg intrace. If `F` is not given, uses the built-in
       static library with basic features. The alternative is to load and use
       the system libraries, but of course they may not be available. */
    mqFFmpeg(struct mqFFmpegInterface const *F = nullptr);

    /* List of encoders. Software encoders come first, then hardware encoders
       (once detected by `detectHardwareEncoders()`. The number of software
       encoders at the start is given by `softwareEncoderCount()`. */
    std::vector<std::string> const&encoderTable() { return m_encoders; }
    void detectHardwareEncoders();
    int softwareEncoderCount() { return m_software_encoder_count; }
    /* Name of encoder #idx in the `encoderTable`. */
    std::string encoder(uint idx) const;

    void stats(mqFFmpegStats *stats);

    /* encoding interface */
    bool start(
        mqDisplay const *display,
        char const *pathname,
        char const *encoder,
        int scale,
        int fps
    );
    bool frame_add(mqDisplay const *display, bool dyn_pts, bool force);
    bool unpause();
    bool pause();
    bool stop();
    bool debug();

    std::string lasterror() { return m_error_info; }

private:
    struct mqFFmpegInterface const *F; // functions

    int ffmpeg_hwdevice_config(
        AVCodecContext *codec_ctx,
        AVBufferRef **hw_device_ctx,
        enum AVHWDeviceType hw_device_type
    );
    bool ffmpeg_codec_exist(char const *codec);
    bool ffmpeg_codec_config(char const *encoder_name);
    bool ffmpeg_file_config(char const *filename);
    int ffmpeg_file_write_frame(AVFrame *frame);
    bool ffmpeg_scale_config();
    bool ffmpeg_scale_conv(mqDisplay const *display);
    bool ffmpeg_scale_get_frame(AVFrame **frame_out, bool dyn_pts, bool force);
    bool ffmpeg_error(int err, char const *format, ...);

    // core information
    struct {
        AVCodecContext *codec_ctx;
        AVDictionary *codec_opt;
        AVBufferRef *hwdevice_ctx;
        AVFormatContext *format_ctx;
        AVStream *stream_video;
        AVPacket *packet;
        struct SwsContext *scale_ctx;
        AVFrame *frame_out_hw;
        AVFrame *frame_out;
        AVFrame *frame_in;
        enum AVPixelFormat pix_fmt_in;
        enum AVPixelFormat pix_fmt_out;
        AVRational framerate;
        AVRational time_base;
        u64 time_ms_ref;
        int iframe;
    } m_core;

    // config / request information
    struct {
        uint width_in;
        uint height_in;
        uint width_out;
        uint height_out;
        mqDisplay_format vram_format;
        int scale;
        int fps;
    } m_config;

    std::vector<std::string> m_encoders;
    bool m_hardware_encoders_detected = false;
    int m_software_encoder_count = 0;

    std::string m_error_info;
    u64 m_time_ms_ref = 0;
    bool m_paused = false;
};

#endif /* MQ_VIDEO_FFMPEG */

#endif /* MQ_UI_FFMPEG_H */
