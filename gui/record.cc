#include <mq/interfaces/display.h>
#if MQ_VIDEO_FFMPEG

#include <azur/stb_image_write.h>
#include <sys/stat.h>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <string>

#include "./record.h"
#include "./gui.h"

//=== screen shot ============================================================//

static int screenshot_aux(char const *pathname, mqDisplay *display, int scale)
{
    if(display == NULL || display->data == NULL)
        return -1;
    mq_log(MQ_LOG_DEBUG, "try to create a screenshot...");
    int i = 0;
    int comp = 3;
    size_t r_width = display->width * scale;
    size_t r_height = display->height * scale;
    u8 *vram_data = (u8*)malloc((r_width * 3) * r_height);
    if(vram_data == NULL)
        return -2;
    if(display->format == MQ_DISPLAY_FORMAT_L8) {
        u8 *data = (u8*)display->data;
        for (size_t y = 0 ; y < r_height ; y++) {
            for (size_t x = 0 ; x < r_width ; x++) {
                size_t src_y = y / scale;
                size_t src_x = x / scale;
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
                size_t src_y = y / scale;
                size_t src_x = x / scale;
                u16 color = data[(display->width*src_y) + src_x];
                vram_data[(i * 3) + 0] = ((color >> 11) & 0b011111) << 3;
                vram_data[(i * 3) + 1] = ((color >> 5)  & 0b111111) << 2;
                vram_data[(i * 3) + 2] = ((color >> 0)  & 0b011111) << 3;
                i += 1;
            }
        }
    }
    mq_log(MQ_LOG_DEBUG, "screenshot: exported at \"%s\"", pathname);
    int err = stbi_write_png(pathname, r_width, r_height, comp, vram_data, 0);
    // TODO: Check for errors here? Writing to /sth.png is reported to work
    mq_log(MQ_LOG_DEBUG, "try to create a screenshot...SUCCESS");
    free(vram_data);
    mq_log(MQ_LOG_DEBUG, "stbi_write_png(): %d", err);
    return (err == 0) ? -3 : 0;
}

bool mqRecord::screenshot(
    mqDisplay *display, int scale_idx, std::string const &path)
{
    int err = screenshot_aux(path.c_str(), display, scale(scale_idx));
    m_cache.screenshot_lasterror = err;
    return (err >= 0);
}

std::string mqRecord::screenshot_lasterror()
{
    switch(m_cache.screenshot_lasterror) {
        case 0:
            return "";
        case -1:
            return "no display data available";
        case -2:
            return "internal alloc fails";
        case -3:
            return "unable to export the screenshot";
        default:
            return "unknown error";
    }
}

//=== record =================================================================//

bool mqRecord::start(mqDisplay *display, int encoder_idx, int scale_idx,
    std::string const &filename)
{
    int err;

    mq_log(MQ_LOG_DEBUG, "mqRecord::start() : try to start recording");
    mq_log(MQ_LOG_DEBUG, "|-- filename: %s", filename.c_str());
    mq_log(MQ_LOG_DEBUG, "|-- encoder: %s", encoder(encoder_idx).c_str());
    mq_log(MQ_LOG_DEBUG, "`-- scale: %d", scale(scale_idx));
    err = m_ffmpeg.start(
        display,
        filename.c_str(),
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

int mqRecord::scale(uint scale_idx) const
{
    static int scales[5] = { 1, 2, 3, 4, 8 };
    return (scale_idx < 5) ? scales[scale_idx] : -1;
}

std::vector<std::string> const &mqRecord::scaleTable(mqDisplay *display)
{
    char buffer[512];

    if(m_cache.scale_type == 0 && display == NULL)
        return m_scale_info;
    if(m_cache.scale_type == 1 && display != NULL)
        return m_scale_info;

    mq_log(MQ_LOG_DEBUG, "mqRecord::getScale() - regenerate");
    m_scale_info.clear();

    if(display != NULL) {
        for(int i = 0; true; i++) {
            int s = scale(i);
            if(s < 0)
                break;
            snprintf(
                buffer, 512, "x%d (%dx%d)", s,
                display->width * s,
                display->height * s
            );
            m_scale_info.push_back(buffer);
        }
    }
    m_cache.scale_type = (display != NULL);
    return m_scale_info;
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

#endif /* MQ_VIDEO_FFMPEG */
