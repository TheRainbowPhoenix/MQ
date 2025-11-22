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

enum {
    MQ_RECORD_STATUS_NOTSTARTED,
    MQ_RECORD_STATUS_RECORDING,
    MQ_RECORD_STATUS_PAUSED,

/*  .---------> NOTSTARTED
    | major        ^  | click "Start"
    | error        |  | or continuous record
    |        click |  | -> file created
    |       "Stop" |  |
    |              |  v v---.  (automatic)
    |----------- RECORDING   | -> frames added
    |              ^  | `---'
    |        click |  |
    |    "Unpause" |  | click
    |              |  v "Pause"
    '------------ PAUSED */
};

class mqRecord
{
public:
    std::vector<std::string> const &encoderTable() {
        return m_ffmpeg.encoderTable();
    }
    std::string encoderName(unsigned int encoder_idx) const {
        return m_ffmpeg.encoder(encoder_idx);
    }
    void detectEncoders() { m_ffmpeg.detectHardwareEncoders(); }
    int softwareEncoderCount() { return m_ffmpeg.softwareEncoderCount(); }

    /* take a screenshot of the */
    bool screenshot(mqDisplay *display, int scale, std::string const &path);
    std::string screenshot_lasterror();

    /* Current status of the state machine; see enumeration for meaning */
    int status() const { return m_status; }

    /* Scaling factor for the input frame */
    int scale() const { return m_scale; }
    void setScale(int s) { m_scale = s; }

    /* Selected encoder */
    int encoder() const { return m_encoder; }
    void setEncoder(int e) { m_encoder = e; }

    /* Set whether we keep recording through add-in resets */
    bool continuousRecord() const { return m_continuousRecord; }
    void setContinuousRecord(bool cr) { m_continuousRecord = cr; }

    // TODO: Option: [x] Pause recording when program is paused

    /* video primitives */
    bool start(
        mqDisplay *display,
        std::string const &path,
        int fps,
        bool dyn_pts
    );
    bool frame_add(mqDisplay *display);
    bool unpause(mqDisplay *display);
    bool pause(mqDisplay *display);
    bool stop(mqDisplay *display);

    bool debug() { return m_ffmpeg.debug(); }
    std::string lasterror() { return m_ffmpeg.lasterror(); }

    /* Get recording statistics. Always returns a non-NULL pointer unless the
       current state is NOTSTARTED. Not all fields might be available. */
    mqRecordStats const *stats();

private:
    mqFFmpeg m_ffmpeg = mqFFmpeg();

    /* State machine */
    int m_status = MQ_RECORD_STATUS_NOTSTARTED;

    /* ID of the selected scaled multiplier in the scale table. */
    int m_scale = 1;
    /* ID of the selected encoder in the encoder table. */
    int m_encoder = 0;
    /* Continue recording to a new file after program resets */
    bool m_continuousRecord = false;
    /* Use dynamic PTS encoding */
    bool m_dyn_pts;
    /* record_stats */
    mqRecordStats m_record_stats;

};

#endif /* MQ_VIDEO_FFMPEG */

#endif /* MQ_UI_RECORD_H */
