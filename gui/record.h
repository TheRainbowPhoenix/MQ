// header for all recording

#ifndef MQ_UI_RECORD_H
#define MQ_UI_RECORD_H

#include <mq/defs.h>
#if MQ_VIDEO_FFMPEG

#include <string>
#include <vector>

#include "ffmpeg.h"
#include <mq/interfaces/display.h>

// TODO[record]: Pull screenshots out of ffmpeg abstractions

//=== Record class ===========================================================//

typedef struct mqRecordStats
{
    int time_min;
    int time_sec;
    int time_ms;
} mqRecordStats;

class mqRecord
{
public:
    std::vector<std::string> const &scaleTable(mqDisplay *display);
    int scale(uint scale_idx) const;
    std::vector<std::string> const &encoderTable();
    std::string encoder(unsigned int encoder_idx) const;

    std::string filename(char const *ext);
    void filenameUpdate(char const *format);
    bool filenameExist(char const *ext);

    /* take a screenshot of the */
    bool screenshot(mqDisplay *display, int scale_idx);
    std::string screenshot_lasterror();

    /* video primitives */
    bool start(mqDisplay *display, int encoder_idx, int scale_idx);
    bool frame_add(mqDisplay *display);
    bool unpause();
    bool pause();
    bool debug();
    bool stop();
    std::string lasterror();

    /* misc */
    mqRecordStats const *stats();

private:
    mqFFmpeg m_ffmpeg = mqFFmpeg();
    std::vector<std::string> m_scale_info;

    // hidden cache method
    void filenameCacheRefresh();

    // cache information
    struct {
        int scale_type;
        std::string filename;
        bool filename_dirty;
        int screenshot_lasterror;
        int record_lasterror;
        mqRecordStats record_stats;
    } m_cache;
};

#endif /* MQ_VIDEO_FFMPEG */

#endif /* MQ_UI_RECORD_H */
