//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/machine.h>
#include <stdlib.h>
#include <stdio.h>

mqMachine *mq_machine_alloc(void)
{
    mqMachine *mach = calloc(1, sizeof *mach);
    mach->memory = mq_memory_alloc();
    return mach;
}

void mq_machine_free(mqMachine *mach)
{
    if(mach)
        mq_memory_free(mach->memory);
    free(mach);
}

void mq_machine_initialize(mqMachine *mach, int initializeKind)
{
    if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_FX) {
        mq_cpu_initialize(&mach->cpu, MQ_CPU_INITIALIZE_ADDIN_FX);
        mq_memory_init(mach->memory);

        // TODO[machine]: Memory setup for FX add-in
    }
    else if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_CG) {
        mq_cpu_initialize(&mach->cpu, MQ_CPU_INITIALIZE_ADDIN_CG);
        mq_memory_init(mach->memory);

        /* P0 program code */
        mq_memory_createBufferChunk(mach->memory, 0x00300000, NULL);
        mq_memory_createBufferChunk(mach->memory, 0x00400000, NULL);
        /* P0 userspace RAM */
        mq_memory_createBufferChunk(mach->memory, 0x08100000, NULL);

        // TODO[machine]: More precise memory setup for CG add-in
    }
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

    mq_memory_load(mach->memory, 0x00300000, data + 0x7000, size - 0x7000);
    return true;

err:
    perror("mq_machine_load_g3a");
    if(fp)
        fclose(fp);
    if(data)
        free(data);
    return false;
}

void mq_machine_cycle(mqMachine *mach)
{
    mq_cpu_cycle(mach, &mach->cpu);
}
