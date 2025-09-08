// header for all recording

#ifndef MQ_UI_RECORD_H
#define MQ_UI_RECORD_H

#include <mq/defs.h>
#include <mq/machine.h>

/* record internal status */
enum mqRecordStatus {
    MQ_RECORD_STATUS_UNINIT = 0,
    MQ_RECORD_STATUS_START = 1,
    MQ_RECORD_STATUS_PAUSED = 2,
};

/* record information */
struct mqRecord {
    mqRecordStatus status = MQ_RECORD_STATUS_UNINIT;
    char const *error = nullptr;
    struct {
        uint iframe   = 0;
        uint time_ms  = 0;
        uint time_sec = 0;
        uint time_min = 0;
        uint total_ms = 0;
        uint nb_error = 0;
        char const *pathname_out = nullptr;
        char const *status = nullptr;
    } stats;
};
typedef struct mqRecord mqRecord;

/* record request */
struct mqRecordRequest {
    uint frameRate;
    uint scale_factor;
    char const *filename;
};
typedef struct mqRecordRequest mqRecordRequest;

//=== Record API ============================================================//

/* initialize the record backend */
int record_init(
    mqRecord *record,
    mqRecordRequest *request,
    mqMachine *mach
);

/* add a frame to the current video */
int record_add_frame(mqRecord *record, mqMachine *mach);

/* debug record backend */
void record_show(mqRecord *record);

/* handle internal error (log, stats, return, ...) */
int record_set_error(mqRecord *record, int ret, char const *error);

/* update iframe information (stats, ...) */
int record_set_iframe(mqRecord *record, uint iframe);

/* update error information */
int record_set_error(mqRecord *record, int ret, char const *error);

/* update errro information */
int record_set_status(mqRecord *record, mqRecordStatus status);

/* quit record */
int record_quit(mqRecord *record);

#endif /* MQ_UI_RECORD_H */
