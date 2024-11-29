//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/machine.h>
#include <stdlib.h>

mqMachine *mq_machine_alloc(void)
{
    mqMachine *mach = calloc(1, sizeof *mach);
    // mach->memory = mq_memory_alloc();
    return mach;
}

void mq_machine_free(mqMachine *mach)
{
    if(mach)
        ; // mq_memory_free(mach->memory);
    free(mach);
}

void mq_machine_initialize(mqMachine *mach, int initializeKind)
{
    if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_FX) {
        mq_cpu_initialize(&mach->cpu, MQ_CPU_INITIALIZE_ADDIN_FX);
        // mq_memory_init(mach->memory);

        // TODO: Set up memory
        // ...
    }
    else if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_CG) {
        mq_cpu_initialize(&mach->cpu, MQ_CPU_INITIALIZE_ADDIN_CG);
        // mq_memory_init(mach->memory);

        // TODO: Set up memory
        // ...
    }
}
