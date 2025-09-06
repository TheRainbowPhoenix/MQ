// header for all recording

#ifndef MQ_UI_RECORD_H
#define MQ_UI_RECORD_H

#include <mq/machine.h>

/* record information */
struct mqRecord {
    bool start = false;
    bool initialized = false;
    char const *error = nullptr;
};
typedef struct mqRecord mqRecord;

//=== Record API ============================================================//

/* initialize the record backend */
int record_init(mqRecord *record, mqMachine *mach);

/* add a frame to the current video */
int record_add_frame(mqRecord *record, mqMachine *mach);

/* quit record */
int record_quit(mqRecord *record);

#endif /* MQ_UI_RECORD_H */
