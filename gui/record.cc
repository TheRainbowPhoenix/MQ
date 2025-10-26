#include <sys/stat.h>
#include <iomanip>
#include <sstream>
#include <ctime>

#include "./stb_image.h"
#include "./record.h"
#include "./gui.h"

//=== screen shot ============================================================//

bool mqRecord::screenshot(mqDisplay *display, int scale_idx)
{
    int err = stb_image_screenshot(
        filename(".png").c_str(),
        display,
        scale(scale_idx)
    );
    m_cache.screenshot_lasterror = err;
    return (err >= 0);
}

std::string mqRecord::screenshot_lasterror()
{
    if(m_cache.screenshot_lasterror == 0)
        return "";
    return stb_image_screenshot_err2str(m_cache.screenshot_lasterror);
}

//=== record =================================================================//

bool mqRecord::start(mqDisplay *display, int encoder_idx, int scale_idx)
{
    int err;

    mq_log(MQ_LOG_DEBUG, "mqRecord::start() : try to start recording");
    mq_log(MQ_LOG_DEBUG, "|-- filename: %s", filename(".mp4").c_str());
    mq_log(MQ_LOG_DEBUG, "|-- encoder: %s", encoder(encoder_idx).c_str());
    mq_log(MQ_LOG_DEBUG, "`-- scale: %d", scale(scale_idx));
    err = m_ffmpeg.start(
        display,
        filename(".mp4").c_str(),
        encoder(encoder_idx).c_str(),
        scale(scale_idx)
    );
    if(err != 0)
        mq_log(MQ_LOG_ERROR, "mqRecord::start() - unable to start recording");
    m_cache.record_lasterror = err;
    return (m_cache.record_lasterror == 0);
}

bool mqRecord::frame_add(mqDisplay *display)
{
    // mq_log(MQ_LOG_DEBUG, "mqRecord::frame_add() - try to add frame");
    m_cache.record_lasterror = m_ffmpeg.frame_add(display);
    if(m_cache.record_lasterror != 0)
        mq_log(MQ_LOG_ERROR, "mqRecord::frame_add() - fails");
    return (m_cache.record_lasterror == 0);
}

bool mqRecord::pause()
{
    m_cache.record_lasterror = m_ffmpeg.pause();
    return (m_cache.record_lasterror == 0);
}

bool mqRecord::unpause()
{
    m_cache.record_lasterror = m_ffmpeg.unpause();
    return (m_cache.record_lasterror == 0);
}

bool mqRecord::stop()
{
    mq_log(MQ_LOG_DEBUG, "stopping recording");
    m_cache.record_lasterror = m_ffmpeg.stop();
    return (m_cache.record_lasterror == 0);
}

bool mqRecord::debug()
{
    mq_log(MQ_LOG_DEBUG, "mqRecord:");
    mq_log(MQ_LOG_DEBUG, "|-- scale_type: %d", m_cache.scale_type);
    mq_log(MQ_LOG_DEBUG, "|-- filename: %s", m_cache.filename.c_str());
    mq_log(MQ_LOG_DEBUG, "|-- filename_dirty: %d", m_cache.filename_dirty);
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

mqRecordStats const*mqRecord::stats()
{
    struct mqFFmpegStats stats;
    int err;

    err = m_ffmpeg.stats(&stats);
    if(err != 0)
        return NULL;
    m_cache.record_stats.time_min = stats.time_min;
    m_cache.record_stats.time_sec = stats.time_sec;
    m_cache.record_stats.time_ms  = stats.time_ms;
    return &m_cache.record_stats;
}

//=== scaling ================================================================//

std::vector<std::string> const &mqRecord::scaleTable(mqDisplay *display)
{
    char buffer[512];
    int scale;

    if(m_cache.scale_type == 0 && display == NULL)
        return m_scale_info;
    if(m_cache.scale_type == 1 && display != NULL)
        return m_scale_info;

    mq_log(MQ_LOG_DEBUG, "mqRecord::getScale() - regenerate");
    m_scale_info.clear();
    if(display != NULL) {
        for(int i = 0 ; i < 4 ; i++) {
            scale = (i == 3) ? 8 : i + 2;
            snprintf(
                buffer, 512, "x%d (%dx%d)", scale,
                display->width * scale,
                display->height * scale
            );
            m_scale_info.push_back(buffer);
        }
    }
    m_cache.scale_type = (display != NULL);
    return m_scale_info;
}

int mqRecord::scale(int scale_idx) const
{
    return (scale_idx == 3) ? 8 : scale_idx + 2;
}

//=== filename ===============================================================//

void mqRecord::filenameCacheRefresh()
{
    if(!m_cache.filename_dirty)
        return;
    mq_log(MQ_LOG_DEBUG, "mqRecord::filenameCacheRefresh() - regenerate");
    auto replace_all = [](
        std::string &text,
        std::string const &toReplace,
        std::string const &replaceWith
    ) {
        std::string buf;
        std::size_t pos = 0;
        std::size_t prevPos;

        buf.reserve(text.size());
        while (true) {
            prevPos = pos;
            pos = text.find(toReplace, pos);
            if (pos == std::string::npos)
                break;
            buf.append(text, prevPos, pos - prevPos);
            buf += replaceWith;
            pos += toReplace.size();
        }
        buf.append(text, prevPos, text.size() - prevPos);
        text.swap(buf);
    };
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%d%m%H%M%S");
    std::string program_name = "unknown";
    if (gui.current_program_path != "") {
        program_name = gui.current_program_path.stem();
    } else {
        mq_log(MQ_LOG_ERROR, "mqRecord::cache() - broken program name");
    }

    mq_log(MQ_LOG_DEBUG, "mqRecord::cache() - filename->%s", m_cache.filename.c_str());
    replace_all(m_cache.filename, "%ADDIN%", program_name);
    replace_all(m_cache.filename, "%DATE%", oss.str());

    mq_log(MQ_LOG_DEBUG, "mqRecord::cache() - filename->%s", m_cache.filename.c_str());
    m_cache.filename_dirty = false;
}

std::string mqRecord::filename(char const *ext)
{
    filenameCacheRefresh();
    return m_cache.filename + ext;

}

bool mqRecord::filenameExist(char const *ext)
{
    struct stat buffer;
    return stat(filename(ext).c_str(), &buffer) == 0;
}

void mqRecord::filenameUpdate(char const *format)
{
    m_cache.filename = format;
    m_cache.filename_dirty = true;
}

//=== encoder ================================================================//

std::vector<std::string> const&mqRecord::encoderTable()
{
    return m_ffmpeg.encoderTable();
}

std::string mqRecord::encoder(unsigned int encoder_idx) const
{
    return m_ffmpeg.encoder(encoder_idx);
}
