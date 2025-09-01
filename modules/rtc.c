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
    (void)value;

    // do not handle flag and interrupt here, move to the common refresh
    // function
    RTC->RCR1 = value & 0x99;

    // force refresh the whole module to handle potential interruption
    // rtc_core_refresh(RTC);
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

    // same as RCR1, handle interruption in the refresh function
    RTC->RCR3 = value & 0x80;
    // rtc_core_refresh(RTC);
}

//=====================================//

static void mq_rtc_process(mqMachine *mach, int cyclesElapsed)
{
    mqRTC *RTC = mach->modules[moduleID];
    (void)cyclesElapsed;

    u64 ticks = mq_timer_update(&RTC->internalTimer);
    if (ticks == 0)
        return;
    RTC->R64CNT += ticks;
    if(RTC->R64CNT >= 128) {
        RTC->R64CNT = 0;
        RTC->RSECCNT = bcd8(int8(RTC->RSECCNT) + 1);
        RTC->RCR1 |= 0x80;
    }
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
