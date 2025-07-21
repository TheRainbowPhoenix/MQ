//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.tmu: Timer Unit
// Reference -- SH7724 manual, Section 20
//   https://bible.planet-casio.com/common/hardware/mpu/sh7724.pdf
//---

#ifndef MQ_MODULES_TMU_H
#define MQ_MODULES_TMU_H

#include <mq/machine.h>
#include <mq/interfaces/timer.h>
MQ_START_DEFS

struct mqTMU_Timer {
    u32 TCOR;       // Timer Constant Register
    u32 TCNT;       // Timer Counter Register
    u16 TCR;        // Timer Control Register
};

struct mqTMU {
    u8 TSTR;        // Timer Start Register
    struct mqTMU_Timer timers[3];
    mqTimer internalTimers[3];
};

typedef struct mqTMU mqTMU;
typedef struct mqTMU_Timer mqTMU_Timer;

/* Setup the TMU module for a given machine. */
bool mq_tmu_setup(mqMachine *mach);

/* Get the TMU module for a machine, NULL if there is none. */
mqTMU *mq_tmu_get(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_MODULES_TMU_H */
