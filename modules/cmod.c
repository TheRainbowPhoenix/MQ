//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/modules/cmod.h>
#include <mq/modules/intc.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdlib.h>

#define RESOLUTION_NS_32KHZ (1000000000ull / 32768ull)

static int moduleID = -1;
static int processID = -1;

static void inithook(void)
{
    moduleID = mq_module_register();
    processID = mq_process_register();
}
MQ_HOOK_REGISTER(init, inithook)

mqCmod *mq_cmod_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static bool isAnyTimerRunning(mqCmod *Cmod)
{
    for(int i = 0; i < 6; i++) {
        if(mq_timer_isRunning(&Cmod->internalTimers[i]))
            return true;
    }
    return false;
}

static void notifyINTC(mqMachine *mach, int n)
{
    static mqInt const timerInterruptCodes[] = {
        MQ_INT_Cmod_TUNI0, MQ_INT_Cmod_TUNI1, MQ_INT_Cmod_TUNI2,
        MQ_INT_Cmod_TUNI3, MQ_INT_Cmod_TUNI4, MQ_INT_Cmod_TUNI5,
    };
    mqInt timerInterruptCode = timerInterruptCodes[n];

    mqCmod *Cmod = mach->modules[moduleID];
    mqCmod_RTCTimer *RT = &Cmod->timers[n];

    u32 UNF = (RT->RTCR >> 1) & 1;
    u32 UNIE = RT->RTCR & 1;
    UNF &= UNIE;

    mq_intc_setInterruptStatus(mach, timerInterruptCode, UNF);
}

static void mq_cmod_process(mqMachine *mach, int cyclesElapsed)
{
    mqCmod *Cmod = mach->modules[moduleID];
    (void)cyclesElapsed;

    /* Ignore number of cycles elapsed and use real-time */
    for(int i = 0; i < 6; i++) {
        mqCmod_RTCTimer *RT = &Cmod->timers[i];
        mqTimer *timer = &Cmod->internalTimers[i];

        if((RT->RTSTR & 1) != mq_timer_isRunning(timer)) {
            mq_log(MQ_LOG_ERROR, "RSTR%d disagrees with internal timer!", i);
            continue;
        }
        if(!(RT->RTSTR & 1))
            continue;

        u64 new_ticks = mq_timer_update(timer);
        bool underflow = (RT->RTCNT <= new_ticks);
        RT->RTCNT -= new_ticks;
        if(!underflow)
            continue;

        int underflows = 1;
        RT->RTCNT += RT->RTCOR;
        /* Try to detect multi-underflows (cases where the timer undergoes
           multiple underflow cycles in a single update) to diagnose them. This
           either means that the time source is jumping around or that there is
           a very fast timer that we can't keep up with. */
        while(RT->RTCNT > RT->RTCOR) {
            RT->RTCNT += RT->RTCOR;
            underflows++;
        }
        if(underflows > 1)
            mq_log(MQ_LOG_WARNING, "ETMU%d underflowed x%d!", i, underflows);

        RT->RTCR |= (1 << 1); /* UNF */
        notifyINTC(mach, i);
    }
}

static void updateProcess(mqMachine *mach)
{
    mqCmod *Cmod = mach->modules[moduleID];
    mach->processes[processID] =
        isAnyTimerRunning(Cmod) ? mq_cmod_process : NULL;
}

static void write_RTSTRn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqCmod *Cmod = mach->modules[moduleID];
    int n = ((addr & 0xfff) - 0x30) >> 5;
    mqCmod_RTCTimer *RT = &Cmod->timers[n];
    (void)size;

    int diff = (value & 0x01) ^ RT->RTSTR;
    RT->RTSTR = value & 0x01;

    if(!diff)
        return;
    if(RT->RTSTR)
        mq_timer_start(&Cmod->internalTimers[n]);
    else
        mq_timer_reset(&Cmod->internalTimers[n], RESOLUTION_NS_32KHZ);

    updateProcess(mach);
}

static void write_RTCORn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    // Note: Immediate underflow after writing to RTCOR?
    mqCmod *Cmod = io->userdata;
    mqCmod_RTCTimer *RT = &Cmod->timers[((addr & 0xfff) - 0x34) >> 5];
    (void)size;
    RT->RTCOR = value;
}

static u32 read_RTCNTn(struct mqMMIO *io, u32 addr, int size)
{
    // TODO[cmod]: read_RTCNT: tick down upon reading?
    mqCmod *Cmod = io->userdata;
    mqCmod_RTCTimer *RT = &Cmod->timers[((addr & 0xfff) - 0x38) >> 5];
    (void)size;
    return RT->RTCNT;
}

static void write_RTCNTn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqCmod *Cmod = io->userdata;
    int n = ((addr & 0xfff) - 0x38) >> 5;
    mqCmod_RTCTimer *RT = &Cmod->timers[n];
    (void)size;

    if(RT->RTSTR & 1)
        mq_log(MQ_LOG_WARNING, "setting RTCNT%d while timer is running!", n);

    RT->RTCNT = value;
    // TODO[cmod]: Consequences of setting RTCNT--immediate interrupt?
    // TODO[cmod]: Raise interrupt when RTCNT=0 irrespective of RTSTR
}

static void write_RTCRn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqCmod *Cmod = mach->modules[moduleID];
    int n = ((addr & 0xfff) - 0x3c) >> 5;
    mqCmod_RTCTimer *RT = &Cmod->timers[n];
    (void)size;
    RT->RTCR = (RT->RTCR & value & 0x02) | (value & 0x01);
    notifyINTC(mach, n);
    // TODO[cmod]: Consequences of writing to RTCR
}

bool mq_cmod_setup(mqMachine *mach)
{
    mqMemory *mem = mach->memory;
    mqPage *pg44d = mq_memory_getPagePrealloc(mem, 0xa44d0000, 0xdc, 24);
    if(!pg44d)
        return false;

    mqCmod *Cmod = calloc(1, sizeof *Cmod);
    if(!Cmod)
        return false;

    /* Initialize timer constants and timers to a value where they don't
       automatically send out an interrupt. Not sure what the default value
       should be, but this certainly inspires more confidence. */
    for(int i = 0; i < 6; i++) {
        Cmod->timers[i].RTCOR = 0xffffffff;
        Cmod->timers[i].RTCNT = 0xffffffff;
        mq_timer_reset(&Cmod->internalTimers[i], RESOLUTION_NS_32KHZ);
    }

    bool ok = true;
    u32 RT_addresses[6] = {
        0xa44d0030, 0xa44d0050, 0xa44d0070, 0xa44d0090,
        0xa44d00b0, 0xa44d00d0,
    };

    for(int i = 0; i < 6; i++) {
        u32 a = RT_addresses[i];
        mqCmod_RTCTimer *RT = &Cmod->timers[i];
        int ioID;

        ioID = mq_page_addIO(pg44d, "RTSTRn", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
            NULL, write_RTSTRn, &RT->RTSTR, mach);
        ok &= mq_page_mapIO(pg44d, ioID, a, 1, 0);

        ioID = mq_page_addIO(pg44d, "RTCORn", MQ_MMIO_SIZE_4 | MQ_MMIO_READU32,
            NULL, write_RTCORn, &RT->RTCOR, Cmod);
        ok &= mq_page_mapIO(pg44d, ioID, a+4, 1, 0);

        ioID = mq_page_addIO(pg44d, "RTCNTn", MQ_MMIO_SIZE_4, read_RTCNTn,
            write_RTCNTn, NULL, Cmod);
        ok &= mq_page_mapIO(pg44d, ioID, a+8, 1, 0);

        ioID = mq_page_addIO(pg44d, "RTCRn", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
            NULL, write_RTCRn, &RT->RTCR, mach);
        ok &= mq_page_mapIO(pg44d, ioID, a+12, 1, 0);
    }

    if(ok)
        mach->modules[moduleID] = Cmod;
    else
        free(Cmod);
    return ok;
}

static void mq_cmod_cleanup(mqMachine *mach)
{
    mqCmod *Cmod = mach->modules[moduleID];
    if(Cmod)
        free(Cmod);
}
MQ_HOOK_REGISTER(module_cleanup, mq_cmod_cleanup)
