#include <mq/interfaces/display.h>
#if MQ_VIDEO_FFMPEG

#include <sys/stat.h>
#include <string>

#include "./record.h"

mqRecord::mqRecord()
{
    m_ffmpegStatic = std::make_unique<mqFFmpeg>();

    /* Try and load the system libraries... */
    auto F = mqFFmpeg::loadSystemLibraries();
    m_ffmpegSystem = F ? std::make_unique<mqFFmpeg>(F) : nullptr;

    setEncoder(0);
}

std::vector<std::string> mqRecord::encoderTable() const
{
    std::vector<std::string> table;

    for(auto const &encoder: m_ffmpegStatic->encoderTable())
        table.push_back(encoder + " (builtin)");

    if(m_ffmpegSystem) {
        for(auto const &encoder: m_ffmpegSystem->encoderTable())
            table.push_back(encoder + " (system)");
    }

    return table;
}

std::string mqRecord::encoderName(unsigned int encoder_idx) const
{
    unsigned int staticCount = m_ffmpegStatic->encoderCount();
    unsigned int systemCount =
        m_ffmpegSystem ? m_ffmpegSystem->encoderCount() : 0;

    if(encoder_idx < staticCount)
        return m_ffmpegStatic->encoder(encoder_idx);
    encoder_idx -= staticCount;

    if(encoder_idx < systemCount)
        return m_ffmpegSystem->encoder(encoder_idx);

    return "";
}

void mqRecord::detectEncoders()
{
    if(m_ffmpegSystem)
        m_ffmpegSystem->detectHardwareEncoders();
}

int mqRecord::softwareEncoderCount()
{
    int total = m_ffmpegStatic->softwareEncoderCount();
    if(m_ffmpegSystem)
        total += m_ffmpegSystem->softwareEncoderCount();
    return total;
}

void mqRecord::setEncoder(int e)
{
    m_encoder = e;
    m_ffmpeg = (e < (int)m_ffmpegStatic->encoderCount())
             ? m_ffmpegStatic.get()
             : m_ffmpegSystem.get();
}

//=== record =================================================================//

bool mqRecord::start(
    mqDisplay *display,
    std::string const &filename,
    int fps,
    bool dyn_pts
) {
    int encoder_idx = encoder();

    mq_log(MQ_LOG_DEBUG, "mqRecord::start() : try to start recording");
    mq_log(MQ_LOG_DEBUG, "|-- filename: %s", filename.c_str());
    mq_log(MQ_LOG_DEBUG, "|-- encoder: %s", encoderName(encoder_idx).c_str());
    mq_log(MQ_LOG_DEBUG, "`-- scale: %d", scale());
    bool err = m_ffmpeg->start(
        display,
        filename.c_str(),
        encoderName(encoder_idx).c_str(),
        scale(),
        fps
    );
    if(!err) {
        mq_log(MQ_LOG_ERROR, "mqRecord::start() - unable to start recording");
        m_status = MQ_RECORD_STATUS_NOTSTARTED;
        debug();
        return false;
    }
    m_dyn_pts = dyn_pts;
    m_status = MQ_RECORD_STATUS_RECORDING;
    return true;
}

bool mqRecord::frame_add(mqDisplay *display)
{
    if(!m_ffmpeg->frame_add(display, m_dyn_pts, false)) {
        mq_log(MQ_LOG_ERROR, "mqRecord::frame_add(): unable to send frame");
        return false;
    }
    return true;
}

bool mqRecord::pause(mqDisplay *display)
{
    // when we use the dynamic PTS encoding, we need to force write the
    // frame before pausing to properly update the previous frame duration
    // (e.g `gintctl` only refresh the display when needed)
    (void)display;

    // pause encoding
    if(!m_ffmpeg->pause())
        return false;
    m_status = MQ_RECORD_STATUS_PAUSED;
    return true;
}

bool mqRecord::unpause(mqDisplay *display)
{
    // same as `pause()`, force-write the frame to properly update the
    // previous frame duration
    (void)display;

    // unpause encoding
    if(!m_ffmpeg->unpause())
        return false;
    m_status = MQ_RECORD_STATUS_RECORDING;
    return true;
}

bool mqRecord::stop(mqDisplay *display)
{
    // same as `pause()`, force-write the frame to properly update the
    // previous frame duration
    (void)display;

    mq_log(MQ_LOG_DEBUG, "stopping recording");
    if(m_status == MQ_RECORD_STATUS_RECORDING ||
       m_status == MQ_RECORD_STATUS_PAUSED) {
        if(!m_ffmpeg->stop())
            return false;
    }
    m_status = MQ_RECORD_STATUS_NOTSTARTED;
    return true;
}

//=== misc ===================================================================//

mqRecordStats const *mqRecord::stats()
{
    struct mqFFmpegStats stats;

    m_ffmpeg->stats(&stats);
    m_record_stats.time_min = stats.time_min;
    m_record_stats.time_sec = stats.time_sec;
    m_record_stats.time_ms  = stats.time_ms;
    return &m_record_stats;
}

#endif /* MQ_VIDEO_FFMPEG */
