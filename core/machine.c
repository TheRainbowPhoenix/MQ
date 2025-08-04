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
#include <mq/system/heap.h>
#include <mq/modules/cmod.h>
#include <mq/modules/dma.h>
#include <mq/modules/intc.h>
#include <mq/modules/keysc.h>
#include <mq/modules/mmu.h>
#include <mq/modules/r61524.h>
#include <mq/modules/tmu.h>
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
    memset(&mach->system, 0, sizeof mach->system);
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

    mq_heap_reset();
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
    mq_memory_createBlock(mach->memory, 0xe5007000, 8 << 10, yram);
    mq_memory_createBlock(mach->memory, 0xe5017000, 8 << 10, yram);
    // TODO[machine]: XYRAM @ 0xe5000000, repeat for 64k, block repeats for 4M
}

static void mq_machine_setupPeripheralModules_sh7305(mqMachine *mach)
{
    mq_intc_setup(mach);

    mq_keysc_setup(mach);

    mq_dma_setup(mach);

    mq_cmod_setup(mach);

    mq_tmu_setup(mach);
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
        u32 layout_heap     = 0x88030000; /* @ 192 kB */

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
        /* VRAM */
        // TODO

        mach->system.heapAddress = layout_heap;
        mach->system.heapSize = 48 << 10;

        mach->display = mq_display_create();
        mqDisplay_setFormat(mach->display, MQ_DISPLAY_FORMAT_L8, 128, 64);

        mach->keyboard = mq_keyboard_create();
        mq_keyboard_initialize(mach->keyboard, MQ_KEYBOARD_STANDARD_LAYOUT_FX);

        // TODO[machine]: Add the NULL page to TLB
        mq_mmu_setup(mach);
        mq_mmu_map(mach, 0x00300000, layout_addin,    0, 0x10000,  8);
        mq_mmu_map(mach, 0x08100000, layout_uram_p1, 55, 0x1000,  32);
        mq_mmu_bind(mach);

        mq_machine_setupPeripheralModules_sh7305(mach);

        // TODO: T6K11 display module
        // mq_t6k11_setup(mach);

        mq_casiowin_setup(mach, MQ_CASIOWIN_FX205);
    }
    else if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_CG) {
        u32 layout_addin    = 0x81800000; /* @ 24 MB, somewhere in fs */
        u32 layout_uram_p1  = 0x8c180000; /* @ 1.5 MB */
        u32 layout_uram_p2  = 0xac180000;
        u32 layout_heap     = 0x8c0c0000; /* @ 768 kB */

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
        /* VRAM */
        u32 VRAMsize = 384 * 216 * 2 + 1024; // margin for buffer overflows...
        VRAMsize = ((VRAMsize - 1) | (4096 - 1)) + 1;
        void *vram = mq_memory_allocBuffer(mach->memory, "VRAM", VRAMsize);
        mq_memory_createBlock(mach->memory, 0x8c000000, VRAMsize, vram);
        mq_memory_createBlock(mach->memory, 0xac000000, VRAMsize, vram);
        /* OS stack */
        void *ostk = mq_memory_allocBuffer(mach->memory, "OSTK", 512 << 10);
        mq_memory_createBlock(mach->memory, 0x8c0f0000, 512 << 10, ostk);
        mq_memory_createBlock(mach->memory, 0xac0f0000, 512 << 10, ostk);

        mach->system.heapAddress = layout_heap;
        mach->system.heapSize = 128 << 10;

        mach->display = mq_display_create();
        mqDisplay_setFormat(mach->display, MQ_DISPLAY_FORMAT_RGB565, 396, 224);

        mach->keyboard = mq_keyboard_create();
        mq_keyboard_initialize(mach->keyboard, MQ_KEYBOARD_STANDARD_LAYOUT_FX);

        // TODO[machine]: Handle the NULL page with MMU so it shows up in TLB
        mq_mmu_setup(mach);
        mq_mmu_map(mach, 0x00300000, layout_addin,    0, 0x100000, 2);
        mq_mmu_map(mach, 0x08100000, layout_uram_p1, 55,  0x10000, 8);
        mq_mmu_bind(mach);

        mq_machine_setupPeripheralModules_sh7305(mach);

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
    if(!mach->initialized)
        return 0;

    int cyclesRequested = cycles;
    int cyclesRemaining = cycles;
    mq_timer_unfreeze();

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

    while(cyclesRemaining > 0) {
        if(MQ_UNLIKELY(mach->stuck))
            break;
        mq_cpu_cycle(mach, &mach->cpu);

        if(--mach->processTimer == 0)
            mq_machine_runProcesses(mach, mach->processFrequency);
        cyclesRemaining--;
    }

    mq_timer_freeze();
    return (cyclesRequested - cyclesRemaining);
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

void mq_mach_syscall(mqMachine *mach)
{
    u32 syscallID = mach->cpu.r[0];

    // TODO: Check syscall API version

    /* Log except for syscalls that happen often */
    if(syscallID != 0x1e6 && syscallID != 0x25f && syscallID != 0x2c1 &&
       syscallID != 0x1dd0 && !(syscallID >= 0x1f41 && syscallID <= 0x1f46)
       && syscallID != 0x1170)
        mq_log(MQ_LOG_DEBUG, "Syscall! r0=%08x", syscallID);

    switch(syscallID) {
    case 0x0029: /* ??? - Glib_AddInAplExecutionCheck something like that. */
        mq_log(MQ_LOG_WARNING, "Ignoring %%029, what is that?");
        /* Just return 0. */
        mach->cpu.r[0] = 0;
        break;

    case 0x01e6: /* GetVRAMAddress() */
        // FIXME: GetVRAMAddress() is normally in P2
        mach->cpu.r[0] = 0x8c000000;
        break;

    case 0x025f: /* Bdisp_PutDisp_DD() */
        if(mqDisplay_setFormat(mach->display, MQ_DISPLAY_FORMAT_RGB565,
                               396, 224)) {
            u16 *src = mq_memory_access(mach->memory, 0x8c000000);
            u16 *dst = mach->display->data + 6;
            for(int y = 0; y < 216; y++) {
                for(int x = 0; x < 384; x++)
                    dst[x] = *(u16 *)((uintptr_t)(src++) ^ 2);
                dst += mach->display->width;
            }
            mqDisplay_setDirty(mach->display, true);
        }
        break;

    case 0x02c1: { /* RTC_GetTicks() */
        // FIXME: GetTicks() more than trivial counter
        static int ticks = 0;
        mach->cpu.r[0] = ++ticks;
        break;
    }

    case 0x1170: { /* itoa() */
        int num = mach->cpu.r[4];
        char *dst = mq_memory_access(mach->memory, mach->cpu.r[5]);
        char str[32];
        int len = sprintf(str, "%d", num);
        for(int i = 0; i <= len; i++)
            *(u8 *)((uintptr_t)(dst++) ^ 3) = str[i];
        // This differs from SimLo prototype but likely right? I didn't check.
        mach->cpu.r[0] = mach->cpu.r[5];
        break;
    }

    case 0x1da3: /* Bfile_OpenFile_OS() */
        mach->cpu.r[0] = -1;
        break;
    case 0x1db6: /* Bfile_FindFirst() */
        mach->cpu.r[0] = -1;
        break;
    case 0x1dba: /* Bfile_FindClose() */
        mach->cpu.r[0] = -1;
        break;
    case 0x01dae: /* Bfile_CreateEntry() */
        mach->cpu.r[0] = -1;
        break;

    case 0x1dd0: { /* memcpy() */
        // TODO: Optimized aligned memcpy() + put that in mq_memory()
        u8 *dst = mq_memory_access(mach->memory, mach->cpu.r[4]);
        u8 *src = mq_memory_access(mach->memory, mach->cpu.r[5]);
        u32 len = mach->cpu.r[6];
        for(u32 i = 0; i < len; i++)
            *(u8 *)((uintptr_t)(dst++) ^ 3) = *(u8 *)((uintptr_t)(src++) ^ 3);
        mach->cpu.r[0] = mach->cpu.r[4];
        break;
    }

    case 0x1f41:
    case 0x1f42: /* free() */
        mq_mach_initHeap(mach);
        mq_heap_free(mach->cpu.r[4]);
        break;
    case 0x1f43:
    case 0x1f44: /* malloc() */
        mq_mach_initHeap(mach);
        mach->cpu.r[0] = mq_heap_malloc(mach->cpu.r[4]);
        break;
    case 0x1f45:
    case 0x1f46: /* realloc() */
        mq_mach_initHeap(mach);
        mach->cpu.r[0] = mq_heap_realloc(mach->cpu.r[4], mach->cpu.r[5]);
        break;

    default:
        mq_log(MQ_LOG_ERROR, "Unknown sycall %08x, getting stuck.", syscallID);
        mach->stuck = true;
        return;
    }

    mach->cpu.pc = mach->cpu.spRegs[SH_PR];
}

bool mq_mach_initHeap(mqMachine *mach)
{
    u32 start = mach->system.heapAddress;
    u32 size = mach->system.heapSize;

    if(!start || !size || mq_heap_isInitialized(NULL, NULL))
        return false;

    void *buffer = mq_memory_allocBuffer(mach->memory, "HEAP", size);
    if(!buffer)
        return false;

    if(!mq_memory_createBlock(mach->memory, start, size, buffer))
        return false;

    return mq_heap_init(start, start + size, buffer);
}
