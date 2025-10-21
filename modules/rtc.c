//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
#include <mq/modules/rtc.h>
#include <mq/modules/intc.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdlib.h>

#define RESOLUTION_NS_256HZ (1000000000ull / 256ull)

static int moduleID = -1;
static int processID = -1;

static void mq_rtc_inithook(void)
{
    moduleID = mq_module_register();
    processID = mq_process_register();
}
MQ_HOOK_REGISTER(init, mq_rtc_inithook)

static void mq_rtc_cleanup(mqMachine *mach)
{
    mqRTC *RTC = mach->modules[moduleID];
    if(RTC)
        free(RTC);
}
MQ_HOOK_REGISTER(module_cleanup, mq_rtc_cleanup)

//=== Core ===================================================================//

mqRTC *mq_rtc_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

void mq_rtc_resetDividerCircuit(mqMachine *mach)
{
    mqRTC *RTC = mach->modules[moduleID];
    RTC->R256_cnt = 0;
}

u8 mq_rtc_getR64CNT(mqMachine *mach)
{
    mqRTC *RTC = mach->modules[moduleID];
    return RTC->R256_cnt / 2;
}

static void updateAlarmFlag(mqRTC *RTC)
{
    bool matches = true;

    /* Each counter XCNT is included in the test if XAR & 0x80 is set. Since
       XCNT & 0x80 is always 0, XAR ^ XCNT has the 0x80 bit if the test is
       enabled, and the difference in the low bits. The test fails if it's
       enabled and has non-zero low bits, making it > 0x80. */
    matches &= ((RTC->RSECAR ^ RTC->RSECCNT) <= 0x80);
    matches &= ((RTC->RMINAR ^ RTC->RMINCNT) <= 0x80);
    matches &= ((RTC->RHRAR  ^ RTC->RHRCNT)  <= 0x80);
    matches &= ((RTC->RWKAR  ^ RTC->RWKCNT)  <= 0x80);
    matches &= ((RTC->RDAYAR ^ RTC->RDAYCNT) <= 0x80);
    matches &= ((RTC->RMONAR ^ RTC->RMONCNT) <= 0x80);
    matches &= ((RTC->RCR3 & 0x80) == 0 || (RTC->RYRAR == RTC->RYRCNT));

    if(matches)
        RTC->RCR1 |= 0x01;
}

static void notifyINTC(mqMachine *mach, mqRTC *RTC)
{
    int alarmSignal = (RTC->RCR1 & 0x08) && (RTC->RCR1 & 0x01);
    mq_intc_setInterruptStatus(mach, MQ_INT_RTC_ATI, alarmSignal);

    int periodicSignal = (RTC->RCR2 & 0x80) != 0;
    mq_intc_setInterruptStatus(mach, MQ_INT_RTC_PRI, periodicSignal);

    int carrySignal = (RTC->RCR1 & 0x10) && (RTC->RCR1 & 0x80);
    mq_intc_setInterruptStatus(mach, MQ_INT_RTC_CUI, carrySignal);
}

static bool modifyBCD2(u8 *value, int increment, int min, int max)
{
    u8 inputValue = *value + increment;
    int tens = (inputValue >> 4) & 0xf;
    int units = (inputValue & 0xf);

    int units_max = (max <= 9) ? max : 10;

    if(units >= units_max) {
        units = 0;
        tens++;
    }

    if(10 * tens + units >= max) {
        *value = mq_rtc_bcd8(min);
        return true;
    }

    *value = (tens << 4) | units;
    return false;
}

static int daysInMonth(int year, int month)
{
    bool isLeapYear =
        (year % 400) == 0 ||
        ((year % 4) == 0 && (year % 100) != 0);

    if(month == 2)
        return 28 + isLeapYear;
    else if(month == 4 || month == 6 || month == 9 || month == 11)
        return 30;
    else
        return 31;
}

// note concerning the hardware behaviour:
//   if a register, for example RMINCNT, has an invalid data like 0x0f,
//   then the register will be corrected only when the carry occurs. Here,
//   the RMINCNT will remain 0x0f until the RSECCNT carry which in this
//   case will be converted into 0x10. Some other examples:
//
//     RMINCNT 0x0f -> RSECCNT Carry -> 0x10
//     RMINCNT 0x5f -> RSECCNT Carry -> 0x00 + Carry
//     RMINCNT 0x7f -> RSECCNT Carry -> 0x00 + Carry
//
//   another special case is for RDAYCNT behaviour. This register must hold
//   a number between 01 and 31, but, if you set it as 0 and a RMINCNT carry
//   occurs, then the register will be set as 01. Some other examples:
//
//    RDAYCNT 0x00 -> RMINCNT Carry -> 0x01
//    RDAYCNT 0x31 -> RMINCNT Carry -> 0x01 (RMONCNT 0x01 (January))
//    RDAYCNT 0x30 -> RMINCNT Carry -> 0x31 (RMONCNT 0x00 -> invalid)
//    RDAYCNT 0x28 -> RMINCNT Carry -> 0x01 (RYRCNT 0x1900 -> no leap year)
//    RDAYCNT 0x28 -> RMINCNT Carry -> 0x29 (RYRCNT 0x2020 -> leap year)
static void rtc_refresh_counters(
    mqMachine *mach, mqRTC *RTC, int secondsIncrement)
{
    int maxDays =
        daysInMonth(mq_rtc_int16(RTC->RYRCNT), mq_rtc_int8(RTC->RMONCNT));

    // second (0 to 59)
    if(!modifyBCD2(&RTC->RSECCNT, secondsIncrement, 0, 60))
        goto end;

    // minute (00 to 59)
    if(!modifyBCD2(&RTC->RMINCNT, +1, 0, 60))
        goto end;

    // hour (00 to 23)
    if(!modifyBCD2(&RTC->RHRCNT, +1, 0, 24))
        goto end;

    // day (01 to 31)
    if(!modifyBCD2(&RTC->RDAYCNT, +1, 1, maxDays + 1))
        goto end;

    // day of week (0 to 6)
    // - carry is ignored, we propagate it only from DAY to MON
    modifyBCD2(&RTC->RWKCNT, +1, 0, 7);

    // month (01 to 12)
    if(!modifyBCD2(&RTC->RMONCNT, +1, 1, 13))
        goto end;

    // year
    // - no carry is generated
    // - no check is performed
    RTC->RYRCNT = mq_rtc_bcd16(mq_rtc_int16(RTC->RYRCNT) + 1);

end:
    // refresh interrupt status
    updateAlarmFlag(RTC);
    notifyINTC(mach, RTC);
}

static void rtc_process(mqMachine *mach, int cyclesElapsed)
{
    mqRTC *RTC = mach->modules[moduleID];
    (void)cyclesElapsed;

    // get the 256Hz timer ticks
    u64 ticks = mq_timer_update(&RTC->internalTimer_256HZ);
    if(ticks == 0)
        return;

    int oldPeriodCount = RTC->R256_cnt / RTC->PES_period;
    RTC->R256_cnt += ticks;
    int newPeriodCount = RTC->R256_cnt / RTC->PES_period;

    // handle periodic interrupt if requested
    // - work even when RCR1.START=0
    if(RTC->RCR2 & 0x70 && newPeriodCount > oldPeriodCount) {
        if(newPeriodCount >= oldPeriodCount + 2) {
            mq_log(MQ_LOG_WARNING, "RTC periodic triggerd x%d!",
                newPeriodCount - oldPeriodCount);
        }
        RTC->RCR2 |= 0x80;
    }

    // carry from the internal 256 Hz counter propagates to RSECCNT
    // - only if RCR2.START is enabled
    bool carry = (RTC->R256_cnt >= 256);
    RTC->R256_cnt %= 256;

    if(carry && (RTC->RCR2 & 0x01) /* START */) {
        RTC->RCR1 |= 0x80;
        rtc_refresh_counters(mach, RTC, +1);
    }

    notifyINTC(mach, RTC);
}

//=== Registers ==============================================================//

static u32 read_R64CNT(mqMachine *mach)
{
    return mq_rtc_getR64CNT(mach);
}

static void write_RSECCNT(mqRTC *RTC, u32 value)
{
    // on the real hardware, no verification is performed on the value.
    // You can write 0x7f and the register will not perform anything until
    // a R64CNT carry occurs which will correct the value with a basic
    // overflow:
    //
    //   RSECCNT 0x0f -> R64CNT Carry -> 0x10
    //   RSECCNT 0x7f -> R64CNT Carry -> 0x00 (and RMINCNT+1)
    //
    // this note is also valid for all other R*CNT registers
    RTC->RSECCNT = value & 0x7f;
}

static void write_RMINCNT(mqRTC *RTC, u32 value)
{
    RTC->RMINCNT = value & 0x7f;
}

static void write_RHRCNT(mqRTC *RTC, u32 value)
{
    RTC->RHRCNT = value & 0x3f;
}

static void write_RWKCNT(mqRTC *RTC, u32 value)
{
    RTC->RWKCNT = value & 0x07;
}

static void write_RDAYCNT(mqRTC *RTC, u32 value)
{
    RTC->RDAYCNT = value & 0x3f;
}

static void write_RMONCNT(mqRTC *RTC, u32 value)
{
    RTC->RMONCNT = value & 0x1f;
}

static void write_RYRCNT(mqRTC *RTC, u32 value)
{
    RTC->RYRCNT = value;
}

static void write_RSECAR(mqRTC *RTC, u32 value)
{
    // no check is performed on data validity on the real hardware even
    // when the ENB bit is set. Note that you can trigger the Alarm Flag
    // (AF) with non-valid data like 0x7f if the RSECCNT also has 0x7f
    // (which is possible only between two R64CNT carry updates (two seconds
    // for RSECCNT))
    // This note is also valid for all other R*AR registers
    RTC->RSECAR = value;
}

static void write_RMINAR(mqRTC *RTC, u32 value)
{
    RTC->RMINAR = value;
}

static void write_RHRAR(mqRTC *RTC, u32 value)
{
    RTC->RHRAR = value & 0xbf;
}

static void write_RWKAR(mqRTC *RTC, u32 value)
{
    RTC->RWKAR = value & 0x87;
}

static void write_RDAYAR(mqRTC *RTC, u32 value)
{
    RTC->RDAYAR = value & 0xbf;
}

static void write_RMONAR(mqRTC *RTC, u32 value)
{
    RTC->RMONAR = value & 0x9f;
}

static void write_RYRAR(mqRTC *RTC, u32 value)
{
    RTC->RYRAR = value;
}

static void write_RCR1(mqMachine *mach, u32 value)
{
    mqRTC *RTC = mach->modules[moduleID];
    RTC->RCR1 = value & 0x99;
    notifyINTC(mach, RTC);
}

static void write_RCR2(mqMachine *mach, u32 value)
{
    mqRTC *RTC = mach->modules[moduleID];

    u8 old = RTC->RCR2;
    RTC->RCR2 = (value & 0xf1) | 0x08;

    if(!(value & 0x08)) {
        mq_log(MQ_LOG_WARNING, "Write to RCR2 with value %02x doesn't bit #3",
            value);
    }

    // RESET bit
    if(value & 0x02)
        mq_rtc_resetDividerCircuit(mach);

    // ADJ bit
    if(value & 0x04) {
        RTC->RSECCNT = (mq_rtc_bcd8(RTC->RSECCNT) < 30) ? 0x00 : 0x7f;
        rtc_refresh_counters(mach, RTC, +0);
    }

    // PES bit
    // Note that the Periodic interrupt is synced to the R64CNT timing. To
    // replicate this behaviour, we use an internal MQ timer that runs at
    // 256Hz which is the lowest frequency at which a request can be
    // performed and it is used to sync the R64CNT register (see
    // `rtc_process()`). Then we simply adjust the current 256Hz timer info
    // to our request and it does the trick
    if((RTC->RCR2 ^ old) & 0x70) {
        u16 periodDurations[8] = { 0xffff, 1, 4, 16, 64, 128, 256, 512 };
        u8 select = ((RTC->RCR2 >> 4) & 0x7);
        RTC->PES_period = periodDurations[select];
    }

    notifyINTC(mach, RTC);
}

static void write_RCR3(mqMachine *mach, u32 value)
{
    mqRTC *RTC = mach->modules[moduleID];
    RTC->RCR3 = value & 0x80;
    updateAlarmFlag(RTC);
    notifyINTC(mach, RTC);
}

//=== Module =================================================================//

bool mq_rtc_setup(mqMachine *mach)
{
    mqMemory *mem = mach->memory;
    mqPage *pg413 = mq_memory_getPagePrealloc(mem, 0xa413fec0, 0x28, 18);
    if(!pg413)
        return false;

    mqRTC *RTC = calloc(1, sizeof *RTC);
    if(!RTC)
        return false;

    /* Keep internal PES period info to 0xffff to indicate that we don't
     * have periodic interrupt yet */
    RTC->PES_period = 0xffff;

    mq_timer_reset(&RTC->internalTimer_256HZ, RESOLUTION_NS_256HZ);
    mq_timer_start(&RTC->internalTimer_256HZ);
    mach->processes[processID] = rtc_process;

    bool ok = true;

    ok &= mq_page_mapRegister8(pg413, "R64CNT", 0xa413fec0,
        read_R64CNT, NULL, NULL, mach);
    ok &= mq_page_mapRegister8(pg413, "RSECCNT", 0xa413fec2, NULL,
        write_RSECCNT, &RTC->RSECCNT, RTC);
    ok &= mq_page_mapRegister8(pg413, "RMINCNT", 0xa413fec4, NULL,
        write_RMINCNT, &RTC->RMINCNT, RTC);
    ok &= mq_page_mapRegister8(pg413, "RHRCNT", 0xa413fec6, NULL,
        write_RHRCNT, &RTC->RHRCNT, RTC);
    ok &= mq_page_mapRegister8(pg413, "RWKCNT", 0xa413fec8, NULL,
        write_RWKCNT, &RTC->RWKCNT, RTC);
    ok &= mq_page_mapRegister8(pg413, "RDAYCNT", 0xa413feca, NULL,
        write_RDAYCNT, &RTC->RDAYCNT, RTC);
    ok &= mq_page_mapRegister8(pg413, "RMONCNT", 0xa413fecc, NULL,
        write_RMONCNT, &RTC->RMONCNT, RTC);
    ok &= mq_page_mapRegister16(pg413, "RYRCRN", 0xa413fece, NULL,
        write_RYRCNT, &RTC->RYRCNT, RTC);

    ok &= mq_page_mapRegister8(pg413, "RSECAR", 0xa413fed0, NULL,
        write_RSECAR, &RTC->RSECAR, RTC);
    ok &= mq_page_mapRegister8(pg413, "RMINAR", 0xa413fed2, NULL,
        write_RMINAR, &RTC->RMINAR, RTC);
    ok &= mq_page_mapRegister8(pg413, "RHRAR", 0xa413fed4, NULL,
        write_RHRAR, &RTC->RHRAR, RTC);
    ok &= mq_page_mapRegister8(pg413, "RWKAR", 0xa413fed6, NULL,
        write_RWKAR, &RTC->RWKAR, RTC);
    ok &= mq_page_mapRegister8(pg413, "RDAYAR", 0xa413fed8, NULL,
        write_RDAYAR, &RTC->RDAYAR, RTC);
    ok &= mq_page_mapRegister8(pg413, "RMONAR", 0xa413feda, NULL,
        write_RMONAR, &RTC->RMONAR, RTC);
    ok &= mq_page_mapRegister16(pg413, "RYRAR", 0xa413fee0, NULL,
        write_RYRAR, &RTC->RYRAR, RTC);

    ok &= mq_page_mapRegister8(pg413, "RCR1", 0xa413fedc, NULL,
        write_RCR1, &RTC->RCR1, mach);
    ok &= mq_page_mapRegister8(pg413, "RCR2", 0xa413fede, NULL,
        write_RCR2, &RTC->RCR2, mach);
    ok &= mq_page_mapRegister8(pg413, "RCR3", 0xa413fee4, NULL,
        write_RCR3, &RTC->RCR3, mach);

    if(ok)
        mach->modules[moduleID] = RTC;
    else
        free(RTC);
    return ok;
}

static void mq_rtc_createObserver(mqMachine *omach, mqMachine const *mach)
{
    omach->modules[moduleID] = memdup(mach->modules[moduleID], sizeof(mqRTC));
}
MQ_HOOK_REGISTER(module_createObserver, mq_rtc_createObserver)

static void mq_rtc_destroyObserver(mqMachine *omach)
{
    mqRTC *RTC = omach->modules[moduleID];
    if(RTC)
        free(RTC);
}
MQ_HOOK_REGISTER(module_destroyObserver, mq_rtc_destroyObserver)

//=== Utilities ==============================================================//

u8 mq_rtc_bcd8(int integer)
{
    integer %= 100;
    return ((integer / 10) << 4) | (integer % 10);
}

u16 mq_rtc_bcd16(int integer)
{
    integer %= 10000;
    return (mq_rtc_bcd8(integer / 100) << 8) | mq_rtc_bcd8(integer % 100);
}

int mq_rtc_int8(u8 bcd)
{
    return (bcd & 0x0f) + 10 * (bcd >> 4);
}

int mq_rtc_int16(u16 bcd)
{
    return (bcd & 0xf) + 10 * ((bcd >> 4) & 0xf) + 100 * ((bcd >> 8) & 0xf)
        + 1000 * (bcd >> 12);
}
