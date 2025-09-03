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

//=== MQ ====================================================================//

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

//=== CORE ==================================================================//

static void rtc_refresh_interrupts(mqMachine *mach, mqRTC *RTC)
{
    int alarm_match;
    int alarm_count;

    // handle carry interrupt
    // - the carry flag (CF) is handled in `rtc_process()`
    if((RTC->RCR1 & 0x10) && (RTC->RCR1 & 0x80))
        mq_intc_setInterruptStatus(mach, MQ_INT_RTC_CUI, true);

    // handle alarm flag and interrupt
    // - we need to check all registers before raising interrupt and flag
    // - assume that all R*CNT registers are "valid"
    alarm_count = 0;
    alarm_match = 0;
    if(RTC->RSECAR & 0x80) {
        alarm_match += ((RTC->RSECAR & 0x7f) == RTC->RSECCNT);
        alarm_count += 1;
    }
    if(RTC->RMINAR & 0x80) {
        alarm_match += ((RTC->RMINAR & 0x7f) == RTC->RMINCNT);
        alarm_count += 1;
    }
    if(RTC->RHRAR & 0x80) {
        alarm_match += ((RTC->RHRAR & 0x3f) == RTC->RHRCNT);
        alarm_count += 1;
    }
    if(RTC->RWKAR & 0x80) {
        alarm_match += ((RTC->RSECAR & 0x03) == RTC->RWKCNT);
        alarm_count += 1;
    }
    if(RTC->RDAYAR & 0x80) {
        alarm_match += ((RTC->RDAYAR & 0x3f) == RTC->RDAYCNT);
        alarm_count += 1;
    }
    if(RTC->RMONAR & 0x80) {
        alarm_match += ((RTC->RMONAR & 0x1f) == RTC->RMONCNT);
        alarm_count += 1;
    }
    if(RTC->RCR3 & 0x80) {
        alarm_match += (RTC->RYRAR == RTC->RYRCNT);
        alarm_count += 1;
    }
    if((alarm_count > 0) && (alarm_match == alarm_count))
        RTC->RCR1 |= 0x01;
    if((RTC->RCR1 & 0x08) && (RTC->RCR1 & 0x01))
        mq_intc_setInterruptStatus(mach, MQ_INT_RTC_ATI, true);
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
static void rtc_refresh_counters(mqMachine *mach, mqRTC *RTC)
{
    bool carry;
    u8 year;
    u8 day;
    u8 month;
    u8 p1;
    u8 p2;

    // second (0 to 59)
    // - handle 60+ second overflow
    carry = false;
    p1 = ((RTC->RSECCNT >> 4) & 0x7);
    p2 = ((RTC->RSECCNT >> 0) & 0xf);
    if(p2 >= 10) {
        p1 += 1;
        p2 = 0;
    }
    if(p1 >= 6) {
        p1 = 0;
        p2 = 0;
        carry = true;
    }
    RTC->RSECCNT = (p1 << 4) | p2;
    if(!carry) {
        rtc_refresh_interrupts(mach, RTC);
        return;
    }
    // minute (00 to 59)
    // - handle 60+ minute overflow
    carry = false;
    p1 = ((RTC->RMINCNT >> 4) & 0x7);
    p2 = ((RTC->RMINCNT >> 0) & 0xf) + 1;
    if(p2 >= 10) {
        p1 += 1;
        p2 = 0;
    }
    if(p1 >= 6) {
        p1 = 0;
        p2 = 0;
        carry = true;
    }
    RTC->RMINCNT = (p1 << 4) | p2;
    if(!carry) {
        rtc_refresh_interrupts(mach, RTC);
        return;
    }
    // hour (00 to 23)
    // - handle 24+ hour overflow
    carry = false;
    p1 = ((RTC->RHRCNT >> 4) & 0x3);
    p2 = ((RTC->RHRCNT >> 0) & 0xf) + 1;
    if(p2 >= 10) {
        p1 += 1;
        p2 = 0;
    }
    if(p1 >= 3 || (p1 == 2 && p2 >= 4)) {
        p1 = 0;
        p2 = 0;
        carry = true;
    }
    RTC->RHRCNT = (p1 << 4) | p2;
    if(!carry) {
        rtc_refresh_interrupts(mach, RTC);
        return;
    }
    // day (01 to 31)
    // - special handle for February that can be 28 or 29 with leap years
    // - handle months that have 31 days
    // - handle months that have 30 days
    carry = false;
    p1 = ((RTC->RDAYCNT >> 4) & 0x3);
    p2 = ((RTC->RDAYCNT >> 0) & 0xf) + 1;
    if(p2 >= 10) {
        p1 += 1;
        p2 = 1;
    }
    day   = p1 * 10 + p2;
    month = mq_rtc_bcd8(RTC->RMONCNT);
    year  = mq_rtc_bcd16(RTC->RYRCNT);
    if(month == 2) {
        if ((year % 4) == 0 && (year % 100) == 0 && (year % 400) == 0) {
            carry = (day > 29);
        } else {
            carry = (day > 28);
        }
    } else if(
        month == 2 || month == 4 ||
        month == 6 || month == 9 ||
        month == 11
    ) {
        carry = (day > 30);
    } else {
        carry = (day > 31);
    }
    if(carry) {
        p1 = 0;
        p2 = 1;
    }
    RTC->RDAYCNT = (p1 << 4) | p2;
    if(!carry) {
        rtc_refresh_interrupts(mach, RTC);
        return;
    }
    // day of week (0 to 6)
    // - do not reset the RDAYCNT carry since it is also used by RMONCNT
    // - no carry is performed here
    p2 = (RTC->RWKCNT & 0x03) + 1;
    if(p2 >= 7)
        p2 = 0;
    RTC->RWKCNT = p2;
    // month (01 to 12)
    // - handle 13+ month overflow
    carry = false;
    p1 = ((RTC->RMONCNT >> 4) & 0x1);
    p2 = ((RTC->RMONCNT >> 0) & 0xf) + 1;
    if(p2 >= 10) {
        p1 += 1;
        p2 = 1;
    }
    if (p1 >= 2 || (p1 == 1 && p2 >= 3)) {
        p1 = 0;
        p2 = 1;
        carry = true;
    }
    RTC->RMONCNT = (p1 << 4) | p2;
    // year
    // - no carry is generated
    // - no check is performed
    if(carry)
        RTC->RYRCNT = mq_rtc_bcd16(mq_rtc_int16(RTC->RYRCNT) + 1);
    // refresh interrupt status
    rtc_refresh_interrupts(mach, RTC);
}

static void rtc_process(mqMachine *mach, int cyclesElapsed)
{
    mqRTC *RTC = mach->modules[moduleID];
    u64 ticks;

    (void)cyclesElapsed;

    // get the 256Hz timer ticks
    ticks = mq_timer_update(&RTC->internalTimer_256HZ);
    if (ticks == 0)
        return;

    // handle periodic interrupt if requested
    // - work even when RCR1.START=0
    // - generate the interruption here to avoid potential sync errors with
    //     R*CNT registers. If we invoke `rtc_refresh_interrupts()` now, all
    //     potential alarm and carry interrupts can occur. But, alarm and
    //     carry interrupts must be synced with the R64CNT carry.
    if(RTC->RCR2 & 0x70) {
        RTC->PES_cnt += ticks;
        if(RTC->PES_cnt >= RTC->PES_max) {
            RTC->RCR2 |= 0x80;
            RTC->PES_cnt = 0;
            mq_intc_setInterruptStatus(mach, MQ_INT_RTC_PRI, true);
        }
    }

    // 64Hz counter
    // - convert 256Hz ticks into 128Hz ticks
    // - add the Carry Flag (CF) only if RCR2.START=1
    // - handle the RCR2.START bit
    bool carry = false;
    RTC->R256_cnt += ticks;
    RTC->R64CNT = RTC->R256_cnt / 2;
    if(RTC->R64CNT >= 128) {
        RTC->R64CNT = 0;
        RTC->R256_cnt = 0;
        carry = true;
    }
    if(!carry)
        return;
    if((RTC->RCR2 & 0x01) == 0)
        return;
    RTC->RCR1 |= 0x80;

    // manually add the RSECCNT carry and request a complete refresh
    u8 p1 = ((RTC->RSECCNT >> 4) & 0x7);
    u8 p2 = ((RTC->RSECCNT >> 0) & 0xf) + 1;
    RTC->RSECCNT = (p1 << 4) | p2;
    rtc_refresh_counters(mach, RTC);
}

//=== REGS ==================================================================//

static void write_RSECCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

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

static void write_RMINCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RMINCNT = value & 0x7f;
}

static void write_RHRCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RHRCNT = value & 0x3f;
}

static void write_RWKCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RWKCNT = value & 0x07;
}

static void write_RDAYCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RDAYCNT = value & 0x3f;
}

static void write_RMONCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RMONCNT = value & 0x1f;
}

static void write_RYRCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RYRCNT = value;
}

static void write_RSECAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // no check is performed on data validity on the real hardware even
    // when the ENB bit is set. Note that you can trigger the Alarm Flag
    // (AF) with non-valid data like 0x7f if the RSECCNT also has 0x7f
    // (which is possible only between two R64CNT carry updates (two seconds
    // for RSECCNT))
    // This note is also valid for all other R*AR registers
    RTC->RSECAR = value;
}

static void write_RMINAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RMINAR = value;
}

static void write_RHRAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RHRAR = value & 0xbf;
}

static void write_RWKAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RWKAR = value & 0x87;
}

static void write_RDAYAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RDAYAR = value & 0xbf;
}

static void write_RMONAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RMONAR = value & 0x9f;
}

static void write_RYRAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RYRAR = value;
}

static void write_RCR1(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    u8 old = RTC->RCR1;
    RTC->RCR1 = value & 0x99;

    // handle interruption clear
    // notes
    //   if you enable alarm interrupt which must match the current
    //   configuration (e.g RSECAR == RSECCNT), the interrupt will never
    //   occur. This is because, on the real hardware, the RTC alarm is
    //   checked each time a R64CNT carry appears. This is why we do not
    //   perform interruption refresh here.
    u8 diff = RTC->RCR1 ^ old;
    if((diff & 0x80) && (old & 0x80))
        mq_intc_setInterruptStatus(mach, MQ_INT_RTC_CUI, false);
    if((diff & 0x10) && (old & 0x10))
        mq_intc_setInterruptStatus(mach, MQ_INT_RTC_CUI, false);
    if((diff & 0x08) && (old & 0x08))
        mq_intc_setInterruptStatus(mach, MQ_INT_RTC_ATI, false);
    if((diff & 0x01) && (old & 0x01))
        mq_intc_setInterruptStatus(mach, MQ_INT_RTC_ATI, false);
}

static void write_RCR2(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    u8 old = RTC->RCR2;
    RTC->RCR2 = (value & 0xf7) | 0x08;
    u8 diff = RTC->RCR2 ^ old;

    // RESET bit
    if(RTC->RCR2 & 0x02) {
        RTC->R64CNT = 0x00;
        RTC->R256_cnt = 0;
        RTC->RCR2 ^= 0x02;
    }
    // ADJ bit
    if(RTC->RCR2 & 0x04) {
        RTC->RSECCNT = (mq_rtc_bcd8(RTC->RSECCNT) < 30) ? 0x00 : 0x7f;
        rtc_refresh_counters(mach, RTC);
        RTC->RCR2 ^= 0x04;
    }
    // PEF bit
    if((diff & 0x80) && (old & 0x80))
        mq_intc_setInterruptStatus(mach, MQ_INT_RTC_PRI, false);
    // PES bit
    // Note that the Periodic interrupt is synced to the R64CNT timing. To
    // replicate this behaviour, we use an internal MQ timer that runs at
    // 256Hz which is the lowest frequency at which a request can be
    // performed and it is used to sync the R64CNT register (see
    // `rtc_process()`). Then we simply adjust the current 256Hz timer info
    // to our request and it does the trick
    if(diff & 0x70) {
        u16 conf_max[8] = {
            0xffff,
            256/256,
            256/64,
            256/16,
            256/4,
            256/2,
            256*1,
            256*2,
        };
        u8 select = ((RTC->RCR2 >> 4) & 0x7);
        RTC->PES_max = conf_max[select];
        RTC->PES_cnt = RTC->R256_cnt % RTC->PES_max;
    }
}

static void write_RCR3(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RCR3 = value & 0x80;
    rtc_refresh_interrupts(mach, RTC);
}

//=== MODULE ================================================================//

bool mq_rtc_setup(mqMachine *mach, int initializeKind)
{
    mqMemory *mem = mach->memory;
    mqPage *pg413 = mq_memory_getPagePrealloc(mem, 0xa413fec0, 0x28, 18);
    if(!pg413)
        return false;

    mqRTC *RTC = calloc(1, sizeof *RTC);
    if(!RTC)
        return false;

    // initialize with default value found when reseting the device on fx
    // device which sets the date to Sunday, November 01 2010 at 00:00:00.
    // Note that the cg device does not reset the peripheral. So, stick
    // with the mono config for now
    (void)initializeKind;
    RTC->RCR2    = 0x09;
    RTC->RWKCNT  = mq_rtc_bcd8(0);
    RTC->RDAYCNT = mq_rtc_bcd8(1);
    RTC->RMONCNT = mq_rtc_bcd8(11);
    RTC->RYRCNT  = mq_rtc_bcd16(2010);

    mq_timer_reset(&RTC->internalTimer_256HZ, RESOLUTION_NS_256HZ);
    mq_timer_start(&RTC->internalTimer_256HZ);
    mach->processes[processID] = rtc_process;

    bool ok = true;
    int ioID;

    ioID = mq_page_addIO(pg413, "R64CNT", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, NULL, &RTC->R64CNT, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fec0, 1, 0);

    ioID = mq_page_addIO(pg413, "RSECCNT", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RSECCNT, &RTC->RSECCNT, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fec2, 1, 0);

    ioID = mq_page_addIO(pg413, "RMINCNT", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RMINCNT, &RTC->RMINCNT, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fec4, 1, 0);

    ioID = mq_page_addIO(pg413, "RHRCNT", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RHRCNT, &RTC->RHRCNT, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fec6, 1, 0);

    ioID = mq_page_addIO(pg413, "RWKCNT", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RWKCNT, &RTC->RWKCNT, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fec8, 1, 0);

    ioID = mq_page_addIO(pg413, "RDAYCNT", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RDAYCNT, &RTC->RDAYCNT, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413feca, 1, 0);

    ioID = mq_page_addIO(pg413, "RMONCNT", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RMONCNT, &RTC->RMONCNT, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fecc, 1, 0);

    ioID = mq_page_addIO(pg413, "RYRCNT", MQ_MMIO_SIZE_2 | MQ_MMIO_READU16,
        NULL, write_RYRCNT, &RTC->RYRCNT, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fece, 2, 0);

    ioID = mq_page_addIO(pg413, "RSECAR", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RSECAR, &RTC->RSECAR, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fed0, 1, 0);

    ioID = mq_page_addIO(pg413, "RMINAR", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RMINAR, &RTC->RMINAR, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fed2, 1, 0);

    ioID = mq_page_addIO(pg413, "RHRAR", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RHRAR, &RTC->RHRAR, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fed4, 1, 0);

    ioID = mq_page_addIO(pg413, "RWKAR", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RWKAR, &RTC->RWKAR, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fed6, 1, 0);

    ioID = mq_page_addIO(pg413, "RDAYAR", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RDAYAR, &RTC->RDAYAR, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fed8, 1, 0);

    ioID = mq_page_addIO(pg413, "RMONAR", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RMONAR, &RTC->RMONAR, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413feda, 1, 0);

    ioID = mq_page_addIO(pg413, "RCR1", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RCR1, &RTC->RCR1, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fedc, 1, 0);

    ioID = mq_page_addIO(pg413, "RCR2", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RCR2, &RTC->RCR2, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fede, 1, 0);

    ioID = mq_page_addIO(pg413, "RYRAR", MQ_MMIO_SIZE_2 | MQ_MMIO_READU16,
        NULL, write_RYRAR, &RTC->RYRAR, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fee0, 2, 0);

    ioID = mq_page_addIO(pg413, "RCR3", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
        NULL, write_RCR3, &RTC->RCR3, mach);
    ok &= mq_page_mapIO(pg413, ioID, 0xa413fee4, 1, 0);

    if(ok)
        mach->modules[moduleID] = RTC;
    else
        free(RTC);
    return ok;
}

mqRTC *mq_rtc_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

//=== UTILS ================================================================//

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
