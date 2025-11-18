#include <mq/interfaces/display.h>
#if MQ_VIDEO_FFMPEG

#include <sys/stat.h>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <string>

#include "./record.h"
#include "./gui.h"

//=== record =================================================================//

bool mqRecord::start(mqDisplay *display, std::string const &filename)
{
    int err;
    int encoder_idx = encoder();

    mq_log(MQ_LOG_DEBUG, "mqRecord::start() : try to start recording");
    mq_log(MQ_LOG_DEBUG, "|-- filename: %s", filename.c_str());
    mq_log(MQ_LOG_DEBUG, "|-- encoder: %s", encoderName(encoder_idx).c_str());
    mq_log(MQ_LOG_DEBUG, "`-- scale: %d", scale());
    err = m_ffmpeg.start(
        display,
        filename.c_str(),
        encoderName(encoder_idx).c_str(),
        scale()
    );
    if(err != 0)
        mq_log(MQ_LOG_ERROR, "mqRecord::start() - unable to start recording");
    m_cache.record_lasterror = err;
    return (m_cache.record_lasterror == 0);
}

bool mqRecord::frame_add(mqDisplay *display)
{
    // mq_log(MQ_LOG_DEBUG, "mqRecord::frame_add() - try to add frame");
    m_cache.record_lasterror = m_ffmpeg.frame_add(display, false);
    if(m_cache.record_lasterror < 0) {
        mq_log(MQ_LOG_ERROR, "mqRecord::frame_add() - fails");
    } else{
        // workaround to avoid the special "positive" value returned by
        // `frame_add()` which indicate that the frame has not been written
        // due to a "too short amount of time between two frames"
        m_cache.record_lasterror = 0;
    }
    return (m_cache.record_lasterror == 0);
}

bool mqRecord::pause(mqDisplay *display)
{
    m_cache.record_lasterror = m_ffmpeg.pause(display);
    if(m_cache.record_lasterror != 0)
        return false;

    m_status = MQ_RECORD_STATUS_PAUSED;
    return true;
}

bool mqRecord::unpause(mqDisplay *display)
{
    m_cache.record_lasterror = m_ffmpeg.unpause(display);
    if(m_cache.record_lasterror != 0)
        return false;

    m_status = MQ_RECORD_STATUS_RECORDING;
    return true;
}

bool mqRecord::stop(mqDisplay *display)
{
    mq_log(MQ_LOG_DEBUG, "stopping recording");
    m_cache.record_lasterror = 0;

    if(m_status == MQ_RECORD_STATUS_RECORDING ||
       m_status == MQ_RECORD_STATUS_PAUSED) {
        m_cache.record_lasterror = m_ffmpeg.stop(display);
    }

    m_status = MQ_RECORD_STATUS_NOTSTARTED;
    return (m_cache.record_lasterror == 0);
}

bool mqRecord::debug()
{
    mq_log(MQ_LOG_DEBUG, "mqRecord:");
    mq_log(MQ_LOG_DEBUG, "`-- record_err: %d", m_cache.record_lasterror);
    m_ffmpeg.debug();
    return true;
}

std::string mqRecord::lasterror()
{
    if (m_cache.record_lasterror == 0)
        return "";
    return m_ffmpeg.err2str(m_cache.record_lasterror);
}

//=== misc ===================================================================//

mqRecordStats const *mqRecord::stats()
{
    struct mqFFmpegStats stats;

    m_ffmpeg.stats(&stats);
    m_cache.record_stats.time_min = stats.time_min;
    m_cache.record_stats.time_sec = stats.time_sec;
    m_cache.record_stats.time_ms  = stats.time_ms;
    return &m_cache.record_stats;
}

#endif /* MQ_VIDEO_FFMPEG */
