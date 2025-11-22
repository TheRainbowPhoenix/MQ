#include <mq/interfaces/display.h>
#if MQ_VIDEO_FFMPEG

#include <sys/stat.h>
#include <string>

#include "./record.h"

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
    bool err = m_ffmpeg.start(
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
    if(!m_ffmpeg.frame_add(display, m_dyn_pts, false)) {
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
    if(!m_ffmpeg.pause())
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
    if(!m_ffmpeg.unpause())
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
        if(!m_ffmpeg.stop())
            return false;
    }
    m_status = MQ_RECORD_STATUS_NOTSTARTED;
    return true;
}

//=== misc ===================================================================//

mqRecordStats const *mqRecord::stats()
{
    struct mqFFmpegStats stats;

    m_ffmpeg.stats(&stats);
    m_record_stats.time_min = stats.time_min;
    m_record_stats.time_sec = stats.time_sec;
    m_record_stats.time_ms  = stats.time_ms;
    return &m_record_stats;
}

#endif /* MQ_VIDEO_FFMPEG */
