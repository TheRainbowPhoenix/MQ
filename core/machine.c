//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/machine.h>
#include <mq/system/heap.h>
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
    mach->stuck = false;

    if(mach->display)
        mq_display_destroy(mach->display);
    mach->display = NULL;

    mq_heap_reset();
}

void mq_machine_destroy(mqMachine *mach)
{
    mq_cpu_reset(&mach->cpu);
    mq_memory_destroy(mach->memory);
    if(mach->display)
        mq_display_destroy(mach->display);
    free(mach);
}

void mq_machine_initialize(mqMachine *mach, int initializeKind)
{
    mq_machine_reset(mach);

    if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_FX) {
        mq_cpu_initialize(&mach->cpu, MQ_CPU_INITIALIZE_ADDIN_FX);

        // TODO[machine]: Memory setup for FX add-in
    }
    else if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_CG) {
        mq_cpu_initialize(&mach->cpu, MQ_CPU_INITIALIZE_ADDIN_CG);

        /* P0 program code */
        void *addin = mq_memory_allocBuffer(mach->memory, "ADDIN", 2 << 20);
        mq_memory_createBlock(mach->memory, 0x00300000, 2 << 20, addin);
        /* P0 userspace RAM */
        void *uram = mq_memory_allocBuffer(mach->memory, "URAM", 512 << 10);
        mq_memory_createBlock(mach->memory, 0x08100000, 512 << 10, uram);
        /* VRAM */
        u32 VRAMsize = 384 * 216 * 2 + 1024; // margin for buffer overflows...
        void *vram = mq_memory_allocBuffer(mach->memory, "VRAM", VRAMsize);
        mq_memory_createBlock(mach->memory, 0x8c000000, VRAMsize, vram);

        // TODO[machine]: Reasonable heap address on fx-CG?!
        mach->system.heapAddress = 0x8c100000;
        mach->system.heapSize = 128 << 10;

        // TODO[machine]: More precise memory setup for CG add-in

        mach->display = mq_display_create();
        mqDisplay_setFormat(mach->display, MQ_DISPLAY_FORMAT_RGB565, 396, 224);
    }

    mach->stuck = false;
}

bool mq_machine_load_g3a(mqMachine *mach, char const *path)
{
    FILE *fp = fopen(path, "r");
    void *data = NULL;
    if(!fp) goto err;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    data = malloc(size);
    if(!data) goto err;

    if(fread(data, size, 1, fp) != 1) goto err;
    fclose(fp);

    bool x = mq_memory_load(
        mach->memory, 0x00300000, data + 0x7000, size - 0x7000);
    free(data);
    return x;

err:
    perror("mq_machine_load_g3a");
    if(fp)
        fclose(fp);
    if(data)
        free(data);
    mach->stuck = true;
    return false;
}

int mq_machine_cycle(mqMachine *mach, int cycles)
{
    for(int i = 0; i < cycles; i++) {
        if(mach->stuck)
            return i;
        mq_cpu_cycle(mach, &mach->cpu);
    }
    return cycles;
}

void mq_mach_syscall(mqMachine *mach)
{
    u32 syscallID = mach->cpu.r[0];

    // TODO: Check syscall API version

    if(syscallID != 0x1e6 /* happens too often */)
        printf("Syscall! r0=%08x\n", syscallID);

    switch(syscallID) {
    case 0x0029: /* ??? */
        printf("Ignoring %%029, what is that?\n");
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
            // TODO: Much faster memcpy() is needed here
            u32 src = 0x8c000000;
            u16 *dst = mach->display->data + 6;
            for(int y = 0; y < 216; y++) {
                for(int x = 0; x < 384; x++) {
                    u32 word;
                    mq_memory_read16(&mach->cpu, mach->memory, src, &word);
                    src += 2;
                    dst[x] = word;
                }
                dst += mach->display->width;
            }
            mqDisplay_setDirty(mach->display, true);
        }
        break;

    case 0x02c1: /* RTC_GetTicks() */
        // FIXME: GetTicks() more than trivial counter
        static int ticks = 0;
        mach->cpu.r[0] = ++ticks;
        break;

    case 0x1dd0: { /* memcpy() */
        // TODO: This is a super slow memcpy()
        u32 dst = mach->cpu.r[4];
        u32 src = mach->cpu.r[5];
        u32 len = mach->cpu.r[6];
        for(u32 i = 0; i < len; i++) {
            u32 byte;
            if(!mq_memory_read8(&mach->cpu, mach->memory, src + i, &byte))
                break;
            if(!mq_memory_write(&mach->cpu, mach->memory, dst + i, 1, byte))
                break;
        }
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
        printf("Unknown sycall, getting stuck.\n");
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
