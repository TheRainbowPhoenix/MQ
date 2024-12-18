//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/machine.h>
#include <stdlib.h>
#include <stdio.h>

mqMachine *mq_machine_create(void)
{
    mqMachine *mach = malloc(sizeof *mach);
    mq_cpu_reset(&mach->cpu);
    mach->memory = mq_memory_create();
    return mach;
}

void mq_machine_reset(mqMachine *mach)
{
    mq_cpu_reset(&mach->cpu);
    mq_memory_reset(mach->memory);
}

void mq_machine_destroy(mqMachine *mach)
{
    mq_cpu_reset(&mach->cpu);
    mq_memory_destroy(mach->memory);
    free(mach);
}

void mq_machine_initialize(mqMachine *mach, int initializeKind)
{
    if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_FX) {
        mq_cpu_initialize(&mach->cpu, MQ_CPU_INITIALIZE_ADDIN_FX);
        mq_memory_reset(mach->memory);

        // TODO[machine]: Memory setup for FX add-in
    }
    else if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_CG) {
        mq_cpu_initialize(&mach->cpu, MQ_CPU_INITIALIZE_ADDIN_CG);
        mq_memory_reset(mach->memory);

        /* P0 program code */
        mq_memory_createBufferChunk(mach->memory, 0x00300000, NULL);
        mq_memory_createBufferChunk(mach->memory, 0x00400000, NULL);
        /* P0 userspace RAM */
        mq_memory_createBufferChunk(mach->memory, 0x08100000, NULL);
        /* VRAM (way more than needed!) */
        mq_memory_createBufferChunk(mach->memory, 0x80000000, NULL);

        // TODO[machine]: More precise memory setup for CG add-in
    }

    mach->stuck = false;
}

bool mq_machine_load_g3a(mqMachine *mach, char const *path)
{
    FILE *fp = fopen(path, "r");
    void *data;
    if(!fp) goto err;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    data = malloc(size);
    if(!data) goto err;

    if(fread(data, size, 1, fp) != 1) goto err;
    fclose(fp);

    bool x = mq_memory_load(mach->memory, 0x00300000, data + 0x7000, size - 0x7000);
    printf("x = %s\n", x ? "true" : "false");
    return true;

err:
    perror("mq_machine_load_g3a");
    if(fp)
        fclose(fp);
    if(data)
        free(data);
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

    printf("Syscall! r0=%08x\n", mach->cpu.r[0]);

    if(syscallID == 0x0029) {
        printf("Ignoring %%029, what is that?\n");
        /* Just return 0. */
        mach->cpu.r[0] = 0;
    }
    /* GetVRAMAddress() */
    else if(syscallID == 0x1e6) {
        mach->cpu.r[0] = 0x80000000;
    }
    /* RTC_GetTicks() */
    else if(syscallID == 0x2c1) {
        // FIXME: GetTicks() more than trivial counter
        static int ticks = 0;
        mach->cpu.r[0] = ++ticks;
    }
    else {
        printf("Unknown sycall, getting stuck.\n");
        mach->stuck = true;
        return;
    }

    mach->cpu.pc = mach->cpu.spRegs[SH_PR];
}
