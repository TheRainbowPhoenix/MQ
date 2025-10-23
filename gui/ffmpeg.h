#ifndef MQ_UI_FFMPEG_H
#define MQ_UI_FFMPEG_H

#include <string>
#include <vector>
using namespace std;

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

class mqFFmpeg
{
public:
    mqFFmpeg();
    std::vector<std::string> const&encoderTable();
    std::string encoder(unsigned int encoder_idx) const;
    int stats(mqFFmpegStats *stats);

    int start(
        mqDisplay *display,
        char const *pathname,
        char const *encoder,
        int scale
    );
    int frame_add(mqDisplay const*display);
    int unpause();
    int pause();
    int stop();
    int debug();

    std::string err2str(int err);

private:
    int ffmpeg_hwdevice_config(
        AVCodecContext *codec_ctx,
        AVBufferRef **hw_device_ctx,
        enum AVHWDeviceType hw_device_type
    );
    int ffmpeg_codec_exist(char const *codec);
    int ffmpeg_codec_config(char const *encoder_name);
    int ffmpeg_output_config(char const *filename);
    int ffmpeg_output_frame_write(AVFrame *frame);
    int ffmpeg_scale_config();
    int ffmpeg_scale_conv(AVFrame **frame_out, mqDisplay const *display);
    int ffmpeg_error(int averror, char const *format, ...);

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
    int m_error_errno;
    std::string m_error_info;
};

#endif /* MQ_UI_FFMPEG_H */
