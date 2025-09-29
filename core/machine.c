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

mqMachine *mq_machine_create(void)
{
    mqMachine *mach = calloc(1, sizeof *mach);
    mq_cpu_reset(&mach->cpu);
    mach->memory = mq_memory_create();
    return mach;
}

void mq_machine_reset(mqMachine *mach)
{
    mq_cpu_reset(&mach->cpu);
    mq_memory_reset(mach->memory);
    mach->initialized = false;
    mach->stuck = false;

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
    free(mach);
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

static void mq_machine_setupPeripheralModules_sh7305(
    mqMachine *mach, int initializeKind)
{
    mq_cpg_setup(mach, initializeKind);

    mq_intc_setup(mach, initializeKind);

    mq_keysc_setup(mach);

    mq_dma_setup(mach);

    mq_cmod_setup(mach);

    mq_tmu_setup(mach);

    mq_rtc_setup(mach, initializeKind);
}

void mq_machine_initialize(mqMachine *mach, int initializeKind)
{
    mq_machine_reset(mach);
    if(mq_module_count())
        mach->modules = calloc(mq_module_count(), sizeof *mach->modules);
    if(mq_process_count())
        mach->processes = calloc(mq_process_count(), sizeof *mach->processes);

    mach->processFrequency = 64;
    mach->processTimer = mach->processFrequency;

    if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_FX) {
        u32 layout_addin    = 0x80300000; /* @ 3 MB (in fs for OS 2.xx) */
        u32 layout_uram_p1  = 0x88020000; /* @ 128 kB */
        u32 layout_uram_p2  = 0xa8020000;

        mq_cpu_initialize(&mach->cpu, MQ_CPU_INITIALIZE_ADDIN_FX);
        mq_cpu_setup(&mach->cpu, mach->memory);

        mq_machine_setupOnChipMemory_sh4aldsp(mach);

        /* Set the stack pointer to be P1 instead of MMU, as the OS does */
        mach->cpu.r[15] = layout_uram_p1 + (32 << 10);

        // TODO[machine]: More precise memory setup for FX add-ins
        // TODO[machine]: Setup for SH3 models

        /* P1 program code */
        void *addin = mq_memory_allocBuffer(mach->memory, "ADDIN", 512 << 10);
        mq_memory_createBlock(mach->memory, layout_addin, 512 << 10, addin);
        /* P1 user RAM */
        void *uram = mq_memory_allocBuffer(mach->memory, "URAM", 32 << 10);
        mq_memory_createBlock(mach->memory, layout_uram_p1, 32 << 10, uram);
        mq_memory_createBlock(mach->memory, layout_uram_p2, 32 << 10, uram);

        mach->display = mq_display_create();
        mq_display_setFormat(mach->display, MQ_DISPLAY_FORMAT_L8, 128, 64);

        mach->keyboard = mq_keyboard_create();
        mq_keyboard_initialize(mach->keyboard, MQ_KEYBOARD_STANDARD_LAYOUT_FX);

        // TODO[machine]: Add the NULL page to TLB
        mq_mmu_setup(mach);
        mq_mmu_map(mach, 0x00300000, layout_addin,    0, 0x10000, 8);
        mq_mmu_map(mach, 0x08100000, layout_uram_p1, 55, 0x1000,  8);
        mq_mmu_bind(mach);

        mq_machine_setupPeripheralModules_sh7305(
            mach,
            MQ_MACHINE_INITIALIZE_ADDIN_FX
        );

        mq_t6k11_setup(mach);
        mq_casiowin_setup(mach, MQ_CASIOWIN_FX205);
    }
    else if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_CG) {
        u32 layout_addin    = 0x81800000; /* @ 24 MB, somewhere in fs */
        u32 layout_uram_p1  = 0x8c170000; /* @ 1.5 MB - 64 kB (contiguity) */
        u32 layout_uram_p2  = 0xac170000;

        mq_cpu_initialize(&mach->cpu, MQ_CPU_INITIALIZE_ADDIN_CG);
        mq_cpu_setup(&mach->cpu, mach->memory);

        mq_machine_setupOnChipMemory_sh4aldsp(mach);

        /* Set the stack pointer to be P1 instead of MMU, as the OS does */
        mach->cpu.r[15] = layout_uram_p1 + (512 << 10);

        // TODO[machine]: More precise memory setup for CG add-in

        /* P1 program code */
        void *addin = mq_memory_allocBuffer(mach->memory, "ADDIN", 2 << 20);
        mq_memory_createBlock(mach->memory, layout_addin, 2 << 20, addin);
        /* P1 user RAM */
        void *uram = mq_memory_allocBuffer(mach->memory, "URAM", 512 << 10);
        mq_memory_createBlock(mach->memory, layout_uram_p1, 512 << 10, uram);
        mq_memory_createBlock(mach->memory, layout_uram_p2, 512 << 10, uram);
        /* OS stack */
        void *ostk = mq_memory_allocBuffer(mach->memory, "OSTK", 512 << 10);
        mq_memory_createBlock(mach->memory, 0x8c0e0000, 512 << 10, ostk);
        mq_memory_createBlock(mach->memory, 0xac0e0000, 512 << 10, ostk);
        /* Additional RAM not used by OS */
        void *eram = mq_memory_allocBuffer(mach->memory, "ERAM", 2 << 20);
        mq_memory_createBlock(mach->memory, 0x8c200000, 2 << 20, eram);
        mq_memory_createBlock(mach->memory, 0xac200000, 2 << 20, eram);

        mach->display = mq_display_create();
        mq_display_setFormat(mach->display, MQ_DISPLAY_FORMAT_RGB565, 396, 224);

        mach->keyboard = mq_keyboard_create();
        mq_keyboard_initialize(mach->keyboard, MQ_KEYBOARD_STANDARD_LAYOUT_FX);

        // TODO[machine]: Handle the NULL page with MMU so it shows up in TLB
        mq_mmu_setup(mach);
        mq_mmu_map(mach, 0x00300000, layout_addin,    0, 0x100000, 2);
        mq_mmu_map(mach, 0x08100000, layout_uram_p1, 55,  0x10000, 8);
        mq_mmu_bind(mach);

        mq_machine_setupPeripheralModules_sh7305(
            mach,
            MQ_MACHINE_INITIALIZE_ADDIN_CG
        );

        mq_r61524_setup(mach);
        mq_casiowin_setup(mach, MQ_CASIOWIN_CG380);
    }

    mach->initialized = true;
    mach->stuck = false;
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

int mq_machine_cycle(mqMachine *mach, int cycles)
{
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
            if(MQ_UNLIKELY(mach->stuck))
                break;
            mq_cpu_cycle(mach, &mach->cpu);
            if(MQ_UNLIKELY(mach->stuck))
                break;
            mq_cpu_cycle(mach, &mach->cpu);
            if(MQ_UNLIKELY(mach->stuck))
                break;
            mq_cpu_cycle(mach, &mach->cpu);
            if(MQ_UNLIKELY(mach->stuck))
                break;
            mq_cpu_cycle(mach, &mach->cpu);

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
        if(MQ_UNLIKELY(mach->stuck))
            break;
        mq_cpu_cycle(mach, &mach->cpu);

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
    TracyCZoneN(_ctx, "processes", true);

    for(int i = 0; i < mq_process_count(); i++) {
        mq_process_t *proc = mach->processes[i];
        if(proc)
            proc(mach, cyclesElapsed);
    }

    mach->processTimer += mach->processFrequency;
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
