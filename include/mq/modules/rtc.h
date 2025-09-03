//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.rtc: Real Time Clock Unit
// Reference -- SH7724 manual, Section 28
//   https://bible.planet-casio.com/common/hardware/mpu/sh7724.pdf
//---

#ifndef MQ_MODULES_RTC_H
#define MQ_MODULES_RTC_H

#include <mq/machine.h>
#include <mq/interfaces/timer.h>
MQ_START_DEFS

struct mqRTC {
    u8 R64CNT;
    u8 RSECCNT;
    u8 RMINCNT;
    u8 RHRCNT;
    u8 RWKCNT;
    u8 RDAYCNT;
    u8 RMONCNT;
    u16 RYRCNT;
    u8 RSECAR;
    u8 RMINAR;
    u8 RHRAR;
    u8 RWKAR;
    u8 RDAYAR;
    u8 RMONAR;
    u8 RCR1;
    u8 RCR2;
    u16 RYRAR;
    u8 RCR3;
    u16 RWTCNT;
    u16 RWTCSR;

    mqTimer internalTimer_256HZ;
    u16 R256_cnt;
    u16 PES_cnt;
    u16 PES_max;
};

typedef struct mqRTC mqRTC;

/* Setup the RTC module for a given machine. */
bool mq_rtc_setup(mqMachine *mach, int initializeKind);

/* Get the RTC module for a machine, NULL if there is none. */
mqRTC *mq_rtc_get(mqMachine *mach);

/* convert int to BCD8 */
u8 mq_rtc_bcd8(int data);

/* convert int to BCD16 */
u16 mq_rtc_bcd16(int integer);

/* convert BCD8 to int */
int mq_rtc_int8(u8 bcd);

/* convert BCD16 to int */
int mq_rtc_int16(u16 bcd);

MQ_END_DEFS
#endif /* MQ_MODULES_RTC_H */
