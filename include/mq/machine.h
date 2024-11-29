//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.machine: Root machine structure
//---

#ifndef MQ_MACHINE_H
#define MQ_MACHINE_H

#include <mq/cpu.h>
#include <mq/memory.h>
MQ_START_DEFS

// TODO
struct mqMachine
{
    mqCpu cpu;
    mqMemory *memory;
};

typedef struct mqMachine mqMachine;

mqMachine *mq_machine_alloc(void);

void mq_machine_free(mqMachine *mach);

enum {
    MQ_MACHINE_INITIALIZE_ADDIN_FX,
    MQ_MACHINE_INITIALIZE_ADDIN_CG,
};

void mq_machine_initialize(mqMachine *mach, int initializeKind);

MQ_END_DEFS
#endif /* MQ_MACHINE_H */
