//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
#include <mq/modules/tmu.h>
#include <mq/modules/intc.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdlib.h>

/* That should be 34.44 units, but rounded down to 34, so about 1.3% off */
#define RESOLUTION_NS_29MHZ (1000000000ull / 29030000ull)

static int moduleID = -1;
static int processID = -1;

static void inithook(void)
{
    moduleID = mq_module_register();
    processID = mq_process_register();
}
MQ_HOOK_REGISTER(init, inithook)

mqTMU *mq_tmu_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static bool isAnyTimerRunning(mqTMU *TMU)
{
    for(int i = 0; i < 3; i++) {
        if(mq_timer_isRunning(&TMU->internalTimers[i]))
            return true;
    }
    return false;
}

static u64 tickResolutionForPrescaler(int prescalerBitfield)
{
    static int prescalerValue[8] = {
        4, 16, 64, 256, 1024, /* (prohibited) */ 1, 1, 1,
    };
    int scale = prescalerValue[prescalerBitfield];
    return RESOLUTION_NS_29MHZ * scale;
}

static void notifyINTC(mqMachine *mach, int n)
{
    static mqInt const timerInterruptCodes[] = {
        MQ_INT_TMU_TUNI0, MQ_INT_TMU_TUNI1, MQ_INT_TMU_TUNI2,
    };
    mqInt timerInterruptCode = timerInterruptCodes[n];

    mqTMU *TMU = mach->modules[moduleID];
    mqTMU_Timer *T = &TMU->timers[n];

    u32 UNF = (T->TCR >> 8) & 1;
    u32 UNIE = (T->TCR >> 5) & 1;
    UNF &= UNIE;

    mq_intc_setInterruptStatus(mach, timerInterruptCode, UNF);
}

static void mq_tmu_process(mqMachine *mach, int cyclesElapsed)
{
    mqTMU *TMU = mach->modules[moduleID];
    (void)cyclesElapsed;

    /* Ignore number of cycles elapsed and use real-time */
    for(int i = 0; i < 3; i++) {
        mqTMU_Timer *T = &TMU->timers[i];
        mqTimer *timer = &TMU->internalTimers[i];

        u8 TSTR_mask = TMU->TSTR & (1 << i);

        if((TSTR_mask != 0) != mq_timer_isRunning(timer)) {
            mq_log(MQ_LOG_ERROR, "RSTR%d disagrees with internal timer!", i);
            continue;
        }
        if(!TSTR_mask)
            continue;

        u64 new_ticks = mq_timer_update(timer);
        bool underflow = (T->TCNT <= new_ticks);
        T->TCNT -= new_ticks;
        if(!underflow)
            continue;

        int underflows = 1;
        T->TCNT += T->TCOR;
        /* Multi-underflow detection */
        while(T->TCNT > T->TCOR) {
            T->TCNT += T->TCOR;
            underflows++;
        }
        if(underflows > 1)
            mq_log(MQ_LOG_WARNING, "TMU%d underflowed x%d!", i, underflows);

        T->TCR |= (1 << 8); /* UNF */
        notifyINTC(mach, i);
    }
}

static void updateProcess(mqMachine *mach)
{
    mqTMU *TMU = mach->modules[moduleID];
    mach->processes[processID] = isAnyTimerRunning(TMU) ? mq_tmu_process : NULL;
}

static void write_TSTR(void *userdata, u32 value)
{
    mqMachine *mach = userdata;
    mqTMU *TMU = mach->modules[moduleID];

    /* Timers that changed status */
    int diff = (value ^ TMU->TSTR);

    for(int i = 0; i < 3; i++) {
        if(!(diff & (1 << i)))
            continue;

        mqTimer *Timer = &TMU->internalTimers[i];
        if(value & (1 << i))
            mq_timer_start(Timer);
        else
            mq_timer_reset(Timer, Timer->tickResolution);
    }

    TMU->TSTR = value & 0x07;
    updateProcess(mach);
}

static void write_TCORn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    // Note: Immediate underflow after writing to TCOR?
    mqTMU *TMU = io->userdata;
    mqTMU_Timer *T = &TMU->timers[((addr & 0xfff) - 0x08) / 0xc];
    (void)size;
    T->TCOR = value;
}

static void write_TCNTn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqTMU *TMU = io->userdata;
    int n = ((addr & 0xfff) - 0x0c) / 0xc;
    mqTMU_Timer *T = &TMU->timers[n];
    (void)size;

    if(TMU->TSTR & (1 << n))
        mq_log(MQ_LOG_WARNING, "setting TCNT%d while timer is running!", n);

    T->TCNT = value;
    // TODO[tmu]: Consequences of setting TCNT--immediate interrupt?
}

static void write_TCRn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqTMU *TMU = mach->modules[moduleID];
    int n = ((addr & 0xfff) - 0x10) / 0xc;
    mqTMU_Timer *T = &TMU->timers[n];
    mqTimer *Timer = &TMU->internalTimers[n];
    (void)size;

    /* If the prescaler is changed, change the timer resolution. */
    // TODO: Timer remainder is lost when setting prescaler (not too bad)
    int oldPrescaler = (T->TCR & 0x07);
    int newPrescaler = (value & 0x07);
    if(oldPrescaler != newPrescaler) {
        bool wasRunning = mq_timer_isRunning(Timer);
        mq_timer_reset(Timer, tickResolutionForPrescaler(newPrescaler));
        if(wasRunning)
            mq_timer_start(Timer);
    }

    T->TCR = (T->TCR & value & 0x100) | (value & 0x27);
    notifyINTC(mach, n);
    // TODO[tmu]: Consequences of writing to TCR
}

bool mq_tmu_setup(mqMachine *mach)
{
    mqMemory *mem = mach->memory;
    mqPage *pg449 = mq_memory_getPagePrealloc(mem, 0xa4490000, 0x2a, 10);
    if(!pg449)
        return false;

    mqTMU *TMU = calloc(1, sizeof *TMU);
    if(!TMU)
        return false;

    /* Initialize timer constants and timers to a predictable value. */
    for(int i = 0; i < 3; i++) {
        TMU->timers[i].TCOR = 0xffffffff;
        TMU->timers[i].TCNT = 0xffffffff;
        TMU->timers[i].TCR  = 0x0000;
        /* Initialize timer to the default prescaler set in TCR (we only update
           the timer frequency when the prescaler bitfield changes) */
        mq_timer_reset(&TMU->internalTimers[i],
            tickResolutionForPrescaler(TMU->timers[i].TCR & 0x7));
    }

    bool ok = true;
    int ioID;
    u32 Timer_addresses[3] = { 0xa4490008, 0xa4490014, 0xa4490020 };

    ioID = mq_page_addIO(pg449, "TSTR",
        MQ_MMIO_SIZE_1 | MQ_MMIO_READU8 | MQ_MMIO_RELOC,
        NULL, write_TSTR, &TMU->TSTR, mach);
    ok &= mq_page_mapIO(pg449, ioID, 0xa4490004, 1, 0);

    for(int i = 0; i < 3; i++) {
        u32 a = Timer_addresses[i];
        mqTMU_Timer *T = &TMU->timers[i];

        ioID = mq_page_addIO(pg449, "TCORn", MQ_MMIO_SIZE_4 | MQ_MMIO_READU32,
            NULL, write_TCORn, &T->TCOR, TMU);
        ok &= mq_page_mapIO(pg449, ioID, a, 1, 0);

        ioID = mq_page_addIO(pg449, "TCNTn", MQ_MMIO_SIZE_4 | MQ_MMIO_READU32,
            NULL, write_TCNTn, &T->TCNT, TMU);
        ok &= mq_page_mapIO(pg449, ioID, a+4, 1, 0);

        ioID = mq_page_addIO(pg449, "TCRn", MQ_MMIO_SIZE_2 | MQ_MMIO_READU16,
            NULL, write_TCRn, &T->TCR, mach);
        ok &= mq_page_mapIO(pg449, ioID, a+8, 1, 0);
    }

    if(ok)
        mach->modules[moduleID] = TMU;
    else
        free(TMU);
    return ok;
}

static void mq_tmu_cleanup(mqMachine *mach)
{
    mqTMU *TMU = mach->modules[moduleID];
    if(TMU)
        free(TMU);
}
MQ_HOOK_REGISTER(module_cleanup, mq_tmu_cleanup)
