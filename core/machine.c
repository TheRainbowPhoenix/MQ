//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/machine.h>
#include <mq/memory.h>
#include <mq/mq.h>
#include <mq/hooks.h>
#include <mq/system/casiowin.h>
#include <mq/modules/cpg.h>
#include <mq/modules/cmod.h>
#include <mq/modules/dma.h>
#include <mq/modules/intc.h>
#include <mq/modules/keysc.h>
#include <mq/modules/mmu.h>
#include <mq/modules/r61524.h>
#include <mq/modules/t6k11.h>
#include <mq/modules/tmu.h>
#include <mq/modules/rtc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

mqMachine *mq_machine_create(void)
{
    mqMachine *mach = calloc(1, sizeof *mach);
    mq_cpu_reset(&mach->cpu);
    mach->memory = mq_memory_create();

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mach->lock_access, &attr);
    pthread_mutex_init(&mach->lock_waiting, &attr);
    pthread_mutexattr_destroy(&attr);
    pthread_cond_init(&mach->cond_work_arrived, NULL);
    return mach;
}

void mq_machine_reset(mqMachine *mach)
{
    mq_cpu_reset(&mach->cpu);
    mq_memory_reset(mach->memory);
    mach->initialized = false;
    mach->stuck = false;

    mach->internallyPaused = false;
    mq_timer_reset(&mach->internalPauseTimer, 0);
    mach->internalPauseTicksRemaining = 0;

    mach->internallyBlocked = false;

    if(mach->modules) {
        mq_callhook_module_cleanup(mach);
        free(mach->modules);
    }
    mach->modules = NULL;

    if(mach->processes)
        free(mach->processes);
    mach->processes = NULL;

    if(mach->display)
        mq_display_destroy(mach->display);
    mach->display = NULL;

    if(mach->keyboard)
        mq_keyboard_destroy(mach->keyboard);
    mach->keyboard = NULL;
}

void mq_machine_destroy(mqMachine *mach)
{
    mq_cpu_reset(&mach->cpu);
    mq_memory_destroy(mach->memory);
    if(mach->modules) {
        mq_callhook_module_cleanup(mach);
        free(mach->modules);
    }
    if(mach->processes)
        free(mach->processes);
    if(mach->display)
        mq_display_destroy(mach->display);
    if(mach->keyboard)
        mq_keyboard_destroy(mach->keyboard);

    pthread_cond_destroy(&mach->cond_work_arrived);
    pthread_mutex_destroy(&mach->lock_waiting);
    pthread_mutex_destroy(&mach->lock_access);
    free(mach);
}

void mq_machine_lock(mqMachine *mach)
{
    // printf("[%d] Acquiring lock_waiting\n", gettid());
    pthread_mutex_lock(&mach->lock_waiting);
    // printf("[%d] Got lock_waiting, acquiring lock_access\n", gettid());
    pthread_mutex_lock(&mach->lock_access);
    // printf("[%d] Got lock_access, unlocking lock_waiting\n", gettid());
    pthread_mutex_unlock(&mach->lock_waiting);
}

void mq_machine_unlock(mqMachine *mach)
{
    // printf("[%d] Unlocking lock_access\n", gettid());
    pthread_mutex_unlock(&mach->lock_access);
}

void mq_machine_unlockAndWaitForWork(mqMachine *mach)
{
    bool hasWork = mach->cyclesPending < 0 || mach->cyclesPending > 0;
    if(hasWork) {
        // printf("[%d] Unlocking lock_access\n", gettid());
        pthread_mutex_unlock(&mach->lock_access);
    }
    else {
        // printf("[%d] Waiting for cond (& unlocking lock_access)\n", gettid());
        pthread_cond_wait(&mach->cond_work_arrived, &mach->lock_access);
        pthread_mutex_unlock(&mach->lock_access);
    }
}

void mq_machine_setStuck(mqMachine *mach)
{
    mach->stuck = true;
    mq_machine_breakExecution(mach);
}

void mq_machine_breakExecution(mqMachine *mach)
{
    mach->internallyBlocked = true;
    if(mach->hasBreakJumpBuffer)
        longjmp(mach->breakJumpBuffer, 1);
}

void mq_machine_setBreakJumpBufferAux(mqMachine *mach)
{
    if(mach->hasBreakJumpBuffer) {
        mq_log(MQ_LOG_ERROR, "double stuck jump buffer on a machine!");
        /* Try to stop execution before we unwind incorrectly and crash... */
        mach->stuck = true;
    }

    mach->hasBreakJumpBuffer = true;
}

void mq_machine_clearBreakJumpBuffer(mqMachine *mach)
{
    mach->hasBreakJumpBuffer = false;
    /* Purge the context copy to avoid accidental jumps back. */
    memset(mach->breakJumpBuffer, 0, sizeof mach->breakJumpBuffer);
}

void mq_machine_setCyclesPending(mqMachine *mach, int cyclesPending)
{
    /* Signal the condition variable for new work if we went from zero to a
       non-zero value. */
    bool hadWorkBefore = (mach->cyclesPending != 0);
    bool hasWorkNow = (cyclesPending != 0);

    mach->cyclesPending = cyclesPending;
    if(!hadWorkBefore && hasWorkNow)
        pthread_cond_signal(&mach->cond_work_arrived);
}

mqMachine *mq_machine_createObserver(mqMachine const *mach)
{
    mqMachine *omach = memdup(mach, sizeof *mach);
    if(!omach)
        return NULL;

    /* Create observers for the CPU and memory */
    mq_cpu_makeObserver(&omach->cpu, &mach->cpu);
    omach->memory = mq_memory_createObserver(mach->memory);

    /* Duplicate the modules and make observers for them too */
    if(mach->modules) {
        omach->modules =
            memdup(mach->modules, mq_module_count() * sizeof *mach->modules);
        mq_callhook_module_createObserver(omach, mach);
    }

    /* Duplicate the list of processes */
    omach->processes =
        memdup(mach->processes, mq_process_count() * sizeof *omach->processes);

    // TODO: Implement observers for display, keyboard, and modules
    // omach->display = mq_display_createObserver(mach->display);
    // omach->keyboard = mq_keyboard_createObserver(mach->keyboard);
    return omach;
}

void mq_machine_destroyObserver(mqMachine *omach)
{
    mq_cpu_cleanupObserver(&omach->cpu);
    mq_memory_destroyObserver(omach->memory);

    if(omach->modules) {
        mq_callhook_module_destroyObserver(omach);
        free(omach->modules);
    }
    if(omach->processes)
        free(omach->processes);

    // TODO: Implement observers for display, keyboard, and modules
    // if(omach->display)
    //     mq_display_destroyObserver(omach->display);
    // if(omach->keyboard)
    //     mq_keyboard_destroyObserver(omach->keyboard);
    free(omach);
}

static void mq_machine_setupOnChipMemory_sh4aldsp(mqMachine *mach)
{
    /* ILRAM occupies 4 kB at 0xe5200000 and repeats for 2 MB */
    void *ilram = mq_memory_allocBuffer(mach->memory, "ILRAM", 4 << 10);
    mq_memory_createBlock(mach->memory, 0xe5200000, 4 << 10, ilram);
    // TODO[machine]: ILRAM repeats for 2 MB

    /* XYRAM */
    void *xyram = mq_memory_allocBuffer(mach->memory, "XYRAM", 16 << 10);
    void *xram = xyram;
    void *yram = xram + (8 << 10);
    mq_memory_createBlock(mach->memory, 0xe500e000, 16 << 10, xyram);
    mq_memory_createBlock(mach->memory, 0xe5007000, 8 << 10, xram);
    mq_memory_createBlock(mach->memory, 0xe5017000, 8 << 10, yram);
    // TODO[machine]: XYRAM @ 0xe5000000, repeat for 64k, block repeats for 4M
}

static void mq_machine_setupPeripheralModules_sh7305(mqMachine *mach)
{
    mq_cpg_setup(mach);
    mq_intc_setup(mach);
    mq_keysc_setup(mach);
    mq_dma_setup(mach);
    mq_cmod_setup(mach);
    mq_tmu_setup(mach);
    mq_rtc_setup(mach);
}

void mq_machine_setupHardware(mqMachine *mach, int hardwareKind)
{
    mq_machine_reset(mach);
    if(mq_module_count())
        mach->modules = calloc(mq_module_count(), sizeof *mach->modules);
    if(mq_process_count())
        mach->processes = calloc(mq_process_count(), sizeof *mach->processes);

    mach->processFrequency = 64;
    mach->processTimer = mach->processFrequency;

    if(hardwareKind == MQ_MACHINE_HARDWARE_VIRT_ADDIN_FX) {
        mq_cpu_setup(&mach->cpu, mach->memory);
        mq_mmu_setup(mach);

        mq_machine_setupOnChipMemory_sh4aldsp(mach);

        mach->display = mq_display_create();
        mq_display_setFormat(mach->display, MQ_DISPLAY_FORMAT_L8, 128, 64);

        mach->keyboard = mq_keyboard_create();
        mq_keyboard_initialize(mach->keyboard, MQ_KEYBOARD_STANDARD_LAYOUT_FX);

        mq_machine_setupPeripheralModules_sh7305(mach);

        mq_t6k11_setup(mach);
        mq_casiowin_setup(mach, MQ_CASIOWIN_FX205);
    }
    else if(hardwareKind == MQ_MACHINE_HARDWARE_VIRT_ADDIN_CG) {
        mq_cpu_setup(&mach->cpu, mach->memory);
        mq_mmu_setup(mach);

        mq_machine_setupOnChipMemory_sh4aldsp(mach);

        /* Additional RAM not used by OS */
        void *eram = mq_memory_allocBuffer(mach->memory, "ERAM", 2 << 20);
        mq_memory_createBlock(mach->memory, 0x8c200000, 2 << 20, eram);
        mq_memory_createBlock(mach->memory, 0xac200000, 2 << 20, eram);

        mach->display = mq_display_create();
        mq_display_setFormat(mach->display, MQ_DISPLAY_FORMAT_RGB565, 396, 224);

        mach->keyboard = mq_keyboard_create();
        mq_keyboard_initialize(mach->keyboard, MQ_KEYBOARD_STANDARD_LAYOUT_FX);

        mq_machine_setupPeripheralModules_sh7305(mach);

        mq_r61524_setup(mach);
        mq_casiowin_setup(mach, MQ_CASIOWIN_CG380);
    }

    mach->initialized = true;
    mach->stuck = false;
}

void mq_machine_initialize(mqMachine *mach, int initializeKind)
{
    if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN) {
        mq_casiowin_initialize(mach);
    }
}

bool mq_machine_load_g1a(mqMachine *mach, void *data, long size)
{
    /* Brief sanity check */
    if(size <= 0x200 || size > (520 << 10))
        return false;

    return mq_memory_load(mach->memory, 0x80300000, data, size);
}

bool mq_machine_load_g3a(mqMachine *mach, void *data, long size)
{
    /* Brief sanity check */
    if(size <= 0x7004 || size > (2500 << 10))
        return false;

    return mq_memory_load(
        mach->memory, 0x81800000, data + 0x7000, size - 0x7000);
}

#if MQ_CONTROLLER_SETJMP
# define CHECK_BLOCKED() (void)0
#else
# define CHECK_BLOCKED() if(mach->internallyBlocked) goto endRun
#endif

int mq_machine_cycle(mqMachine *mach, int cycles)
{
    // printf("mq_machine_cycle: %d / %d\n", cycles, mach->cyclesPending);
    if(!mach->initialized || mach->stuck || !mach->cyclesPending)
        return 0;
    /* Cap to the currently-set number of cycles */
    if(mach->cyclesPending > 0 && mach->cyclesPending < cycles)
        cycles = mach->cyclesPending;

    int cyclesRequested = cycles;
    int cyclesRemaining = cycles;
    mq_timer_unfreeze();

    // TODO[machine]: Host system sleep for long high-level internal pauses
    // TODO[machine]: Not counting cycles during sleep hampers determinism
    if(mach->internallyPaused) {
        int ticks = mq_timer_update(&mach->internalPauseTimer);

        /* Stop the internal timer when reaching the end of the sleep period */
        if((mach->internalPauseTicksRemaining -= ticks) <= 0) {
            mach->internallyPaused = false;
            mq_timer_reset(&mach->internalPauseTimer, 0);
            mach->internalPauseTicksRemaining = 0;
        }
        /* Otherwise, run background processes and leave */
        else {
            if(--mach->processTimer == 0)
                mq_machine_runProcesses(mach, mach->processFrequency);
            return 0;
        }
    }

    /* While blocked, just run background processes */
    while(mach->internallyBlocked && cyclesRemaining >= mach->processTimer) {
        cyclesRemaining -= mach->processTimer;
        mach->processTimer = 0;
        mq_machine_runProcesses(mach, mach->processFrequency);
    }
    if(mach->internallyBlocked)
        goto endRun;

    /* Unroll a bit for speed, but only if the process timers are aligned,
       because we want to invoke processes at deterministic times and it has to
       be exactly the cycle we're checiking. */
    if(mach->processTimer % 4 == 0 && mach->processFrequency % 4 == 0) {
        while(cyclesRemaining > 4) {
            mq_cpu_cycle(mach, &mach->cpu);
            CHECK_BLOCKED();
            mq_cpu_cycle(mach, &mach->cpu);
            CHECK_BLOCKED();
            mq_cpu_cycle(mach, &mach->cpu);
            CHECK_BLOCKED();
            mq_cpu_cycle(mach, &mach->cpu);
            CHECK_BLOCKED();

            if((mach->processTimer -= 4) <= 0)
                mq_machine_runProcesses(mach, mach->processFrequency);
            cyclesRemaining -= 4;
        }
    }
    else if(cyclesRemaining > 100) {
        mq_log(MQ_LOG_WARNING,
            "slow run of %d cycles due to misaligned background processes",
            cyclesRemaining);
    }

    while(cyclesRemaining > 0) {
        mq_cpu_cycle(mach, &mach->cpu);
        CHECK_BLOCKED();

        if(--mach->processTimer == 0)
            mq_machine_runProcesses(mach, mach->processFrequency);
        cyclesRemaining--;
    }

endRun:
    mq_timer_freeze();
    int cyclesElapsed = (cyclesRequested - cyclesRemaining);
    if(mach->cyclesPending >= 0)
        mach->cyclesPending -= cyclesElapsed;
    return cyclesElapsed;
}

void mq_machine_runProcesses(mqMachine *mach, int cyclesElapsed)
{
    TracyCZoneN(_ctx, "processes", mach->profilingCycles);

    for(int i = 0; i < mq_process_count(); i++) {
        mq_process_t *proc = mach->processes[i];
        if(proc)
            proc(mach, cyclesElapsed);
    }

    mach->processTimer += mach->processFrequency;

    /* Take care of any exceptions or interrupts raised by modules. We'll clear
       them fully before resuming execution. If there are multiple pending, the
       next one will be accepted either after rte's delay slot or, in case the
       interrupt handlers itself allows interrupts, when ldc is used to reset
       BL to 0. */
    if(mach->cpu.excMask)
        mq_cpu_handleException(mach, &mach->cpu);
    TracyCZoneEnd(_ctx);
}

void mq_machine_internalPauseMilliseconds(mqMachine *mach, int delay_ms)
{
    if(delay_ms <= 0)
        return;

    mach->internallyPaused = true;
    mq_timer_reset(&mach->internalPauseTimer, 1000000 /* 1 ms */);
    mach->internalPauseTicksRemaining = delay_ms;
}
