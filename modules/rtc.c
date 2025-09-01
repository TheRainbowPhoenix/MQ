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

//================= MODULES =================//

#define RESOLUTION_NS_128HZ (1000000000ull / 128ull)

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


mqRTC *mq_rtc_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

//===================== UTILS =============//

/* bcd8(), bcd16(): Convert integer to BCD (from gint) */
static uint8_t bcd8(int integer)
{
    integer %= 100;
    return ((integer / 10) << 4) | (integer % 10);
}
static uint16_t bcd16(int integer)
{
    integer %= 10000;
    return (bcd8(integer / 100) << 8) | bcd8(integer % 100);
}

/* int8(), int16(): Convert BCD to integer */
static int int8(uint8_t bcd)
{
    return (bcd & 0x0f) + 10 * (bcd >> 4);
}
static int int16(uint16_t bcd)
{
    return (bcd & 0xf) + 10 * ((bcd >> 4) & 0xf) + 100 * ((bcd >> 8) & 0xf)
        + 1000 * (bcd >> 12);
}

// =================== Interrupts =======//

static void rtc_interrupt_refresh(mqMachine *mach, mqRTC *RTC)
{
    int alarm_match;
    int alarm_count;

    // handle carry interrupt
    // - the carry flag (CF) is handled in `mq_rtc_process()`
    if((RTC->RCR1 & 0x10) && (RTC->RCR1 & 0x80))
        mq_intc_setInterruptStatus(mach, MQ_INT_RTC_CUI, true);

    // handle alarm flag and interrupt
    // - we need to check all register before raising interrupt and flag
    // - assume that all R*CNT register are "valid"
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

//==================== REGS ===========//

static void write_RSECCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // on the real hardware, no verification is performed on the value. You
    // can write 0x7f and the register will not perform anything until a
    // new second is elapsed
    RTC->RSECCNT = value & 0x7f;
}

static void write_RMINCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECCNT, no verification until the next refresh
    RTC->RMINCNT = value & 0x7f;
}

static void write_RHRCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECCNT, no verification until the next refresh
    RTC->RHRCNT = value & 0x3f;
}

static void write_RWKCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECCNT, no verification until the next refresh
    RTC->RWKCNT = value & 0x07;
}

static void write_RDAYCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECCNT, no verification until the next refresh
    RTC->RDAYCNT = value & 0x3f;
}

static void write_RMONCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECCNT, no verification until the next refresh
    RTC->RMONCNT = value & 0x1f;
}

static void write_RYRCNT(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECCNT, no verification until the next refresh
    RTC->RYRCNT = value;
}

static void write_RSECAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // no check is performed on the real hardware even when the ENB bit is
    // set. Note that you can trigger the Alarm Flag (AF) with non-valid
    // data like 0x7f if the SECCNT have 0x7f (between two seconds)
    RTC->RSECAR = value;

    // force refresh the whole module to handle potential interruption
    // rtc_core_refresh(RTC);
}

static void write_RMINAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECAR, no check performed
    RTC->RMINAR = value;

    // force refresh the whole module to handle potential interruption
    // rtc_core_refresh(RTC);
}

static void write_RHRAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECAR, no check performed
    RTC->RHRAR = value & 0xbf;

    // force refresh the whole module to handle potential interruption
    // rtc_core_refresh(RTC);
}

static void write_RWKAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECAR, no check performed
    RTC->RWKAR = value & 0x87;

    // force refresh the whole module to handle potential interruption
    // rtc_core_refresh(RTC);
}

static void write_RDAYAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECAR, no check performed
    RTC->RDAYAR = value & 0xbf;

    // force refresh the whole module to handle potential interruption
    // rtc_core_refresh(RTC);
}

static void write_RMONAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECAR, no check performed
    RTC->RMONAR = value & 0x9f;

    // force refresh the whole module to handle potential interruption
    // rtc_core_refresh(RTC);
}

static void write_RYRAR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    // same as RSECAR, no check performed
    RTC->RYRAR = value;

    // force refresh the whole module to handle potential interruption
    // rtc_core_refresh(RTC);
}

static void write_RCR1(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    u8 old = RTC->RCR1;
    RTC->RCR1 = value & 0x99;

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

    RTC->RCR2 = value & 0xf3;
    // todo: handle this register
}

static void write_RCR3(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqRTC *RTC = mach->modules[moduleID];
    (void)addr;
    (void)size;

    RTC->RCR3 = value & 0x80;
    rtc_interrupt_refresh(mach, RTC);
}

//=====================================//

// note concerning the hardware behaviour
//   if a register, for example RMINCNT, have an invalid data like 0x0f,
//   then the register will be corrected only when the carry occur. In our
//   case the RMINCNT will remain 0x0f until the RSECCNT carry which in this
//   case will be converted into 0x10. Some other example:
//
//     RMINCNT 0x0f -> RSECCNT Carry -> 0x10
//     RMINCNT 0x5f -> RSECCNT Carry -> 0x00 + Carry
//     RMINCNT 0x7f -> RSECCNT Carry -> 0x00 + Carry
//
//  another special case is for RDAYCNT behaviour. This register must hold
//  number between 01 and 31, but if you set 0 and a RMINCNT carry occur,
//  then the register will set 01. Some other example:
//
//    RDAYCNT 0x00 -> RMINCNT Carry -> 0x01
//    RDAYCNT 0x31 -> RMINCNT Carry -> 0x01
//    RDAYCNT 0x30 -> RMINCNT Carry -> 0x11 (RMONCNT 0x00 -> invalid)
//    RDAYCNT 0x28 -> RMINCNT Carry -> 0x01 (RYRCNT 0x1900 -> no leap year)
//    RDAYCNT 0x28 -> RMINCNT Carry -> 0x29 (RYRCNT 0x2020 -> leap year)
static void mq_rtc_process(mqMachine *mach, int cyclesElapsed)
{
    mqRTC *RTC = mach->modules[moduleID];
    bool carry;
    u8 month;
    u8 year;
    u8 day;
    u8 ten;
    u8 sec;

    (void)cyclesElapsed;

    u64 ticks = mq_timer_update(&RTC->internalTimer);
    if (ticks == 0)
        return;
    // 64Hz
    // - add the Carry Flag (CF)
    // - reset the register
    carry = false;
    RTC->R64CNT += ticks;
    if(RTC->R64CNT >= 128) {
        RTC->R64CNT = 0;
        RTC->RCR1 |= 0x80;
        carry = true;
    }
    if(!carry) {
        //rtc_interrupt_refresh(mach, RTC);
        return;
    }
    // seconds (0 to 59)
    // - handle 60+ seconds overflow
    carry = false;
    ten = ((RTC->RSECCNT >> 4) & 0x7);
    sec = ((RTC->RSECCNT >> 0) & 0xf) + 1;
    if(sec >= 10) {
        ten += 1;
        sec = 0;
    }
    if(ten >= 6) {
        ten = 0;
        sec = 0;
        carry = true;
    }
    RTC->RSECCNT = (ten << 4) | sec;
    if(!carry) {
        rtc_interrupt_refresh(mach, RTC);
        return;
    }
    // minute (00 to 59)
    // - handle 60+ minutes overflow
    carry = false;
    ten = ((RTC->RMINCNT >> 4) & 0x7);
    sec = ((RTC->RMINCNT >> 0) & 0xf) + 1;
    if(sec >= 10) {
        ten += 1;
        sec = 0;
    }
    if(ten >= 6) {
        ten = 0;
        sec = 0;
        carry = true;
    }
    RTC->RMINCNT = (ten << 4) | sec;
    if(!carry) {
        rtc_interrupt_refresh(mach, RTC);
        return;
    }
    // hour (00 to 23)
    // - handle 24+ hours overflow
    carry = false;
    ten = ((RTC->RHRCNT >> 4) & 0x3);
    sec = ((RTC->RHRCNT >> 0) & 0xf) + 1;
    if(sec >= 10) {
        ten += 1;
        sec = 0;
    }
    if(ten >= 3 || (ten == 2 && sec >= 4)) {
        ten = 0;
        sec = 0;
        carry = true;
    }
    RTC->RHRCNT = (ten << 4) | sec;
    if(!carry) {
        rtc_interrupt_refresh(mach, RTC);
        return;
    }
    // day (01 to 31)
    // notes
    // - special handle for February that can be 28 or 29 with leap years
    // - handle month that have 31 days
    // - handle month that have 30 days
    carry = false;
    ten = ((RTC->RDAYCNT >> 4) & 0x3);
    sec = ((RTC->RDAYCNT >> 0) & 0xf) + 1;
    if(sec >= 10) {
        ten += 1;
        sec = 1;
    }
    day   = ten * 10 + sec;
    month = bcd8(RTC->RMONCNT);
    year  = bcd16(RTC->RYRCNT);
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
        ten = 0;
        sec = 1;
    }
    RTC->RDAYCNT = (ten << 4) | sec;
    if(!carry) {
        rtc_interrupt_refresh(mach, RTC);
        return;
    }
    // day of week (0 to 6)
    // - do not reset the RDAYCNT carry since it's also used by RMONCNT
    // - no carry is performed here
    sec = (RTC->RWKCNT & 0x03) + 1;
    if(sec >= 7)
        sec = 0;
    RTC->RWKCNT = sec;
    // month (01 to 12)
    // - handle 13+ months
    carry = false;
    ten = ((RTC->RMONCNT >> 4) & 0x1);
    sec = ((RTC->RMONCNT >> 0) & 0xf) + 1;
    if(sec >= 10) {
        ten += 1;
        sec = 1;
    }
    if (ten >= 2 || (ten == 1 && sec >= 3)) {
        ten = 0;
        sec = 1;
        carry = true;
    }
    RTC->RMONCNT = (ten << 4) | sec;
    // year
    // - no carry is generated
    // - no check is performed
    if(carry)
        RTC->RYRCNT = bcd16(int16(RTC->RYRCNT) + 1);
    // refresh interrupt status
    rtc_interrupt_refresh(mach, RTC);
}

bool mq_rtc_setup(mqMachine *mach, int initializeKind)
{
    mqMemory *mem = mach->memory;
    mqPage *pg413 = mq_memory_getPagePrealloc(mem, 0xa413fec0, 0x28, 18);
    if(!pg413)
        return false;

    mqRTC *RTC = calloc(1, sizeof *RTC);
    if(!RTC)
        return false;

    // initialize with default value found when reset the device on fx
    // device which set the date to Sunday, November 01 2010 at 00:00:00.
    // Note that the cg device does not reset the peripheral. So, stick
    // with the mono config for now
    (void)initializeKind;
    RTC->RWKCNT  = bcd8(0);
    RTC->RDAYCNT = bcd8(1);
    RTC->RMONCNT = bcd8(11);
    RTC->RYRCNT  = bcd16(2010);

    mq_timer_reset(&RTC->internalTimer, RESOLUTION_NS_128HZ);
    mq_timer_start(&RTC->internalTimer);
    mach->processes[processID] = mq_rtc_process;

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
