//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.cmod: Custom Module
// Reference -- None! Just crumbs.
//   https://bible.planet-casio.com/lephenixnoir/en/sh7305/modules/cmod-etmu
//
// TODO[cmod]: Clock settings/control
// TODO[cmod]: BCD module
// TODO[cmod]: Tick down RTC timers
//---

#ifndef MQ_MODULES_CMOD_H
#define MQ_MODULES_CMOD_H

#include <mq/machine.h>
#include <mq/interfaces/timer.h>
MQ_START_DEFS

struct mqCmod_RTCTimer {
    u8 RTSTR;       // RTC Timer Start Register
    u8 RTCR;        // RTC Timer Control Register
    u32 RTCOR;      // RTC Timer Constant Register
    u32 RTCNT;      // RTC Timer Counter Register
};

struct mqCmod {
    u16 DDCLKR0;    // External CLK1 setting
    u16 DDCLKR1;    // External CLK2 setting
    u16 DDCLKR2;    // External CLK3 setting

    u8 DDCK_CNTR;   // External clock control
    u8 DDCS_CNTR;   // External CS control
    u8 HIZ_CNTR;    // Interrupt pin level control

    u16 FASCR;      // BCD Calculation Control Register
    u32 FASSRA;     // BCD Calculation Source Register A
    u32 FASSRB;     // BCD Calculation Source Register B
    u32 FASDR;      // BCD Calculation Result Register

    struct mqCmod_RTCTimer timers[6];
    mqTimer internalTimers[6];
};

typedef struct mqCmod mqCmod;
typedef struct mqCmod_RTCTimer mqCmod_RTCTimer;

/* Setup the Cmod module for a given machine. */
bool mq_cmod_setup(mqMachine *mach);

/* Get the Cmod module for a machine, NULL if there is none. */
mqCmod *mq_cmod_get(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_MODULES_CMOD_H */
