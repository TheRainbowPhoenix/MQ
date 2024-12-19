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

    bool stuck;

    /* System emulation details */
    struct {
        // TODO: System API version
        u32 heapAddress;
        u32 heapSize;
    } system;
};

typedef struct mqMachine mqMachine;

/* CRD functions for mqMachine. The default state is the default CPU and memory
   state. */
mqMachine *mq_machine_create(void);
void mq_machine_reset(mqMachine *mach);
void mq_machine_destroy(mqMachine *mach);

enum {
    MQ_MACHINE_INITIALIZE_ADDIN_FX,
    MQ_MACHINE_INITIALIZE_ADDIN_CG,
};

void mq_machine_initialize(mqMachine *mach, int initializeKind);

bool mq_machine_load_g3a(mqMachine *mach, char const *path);

int mq_machine_cycle(mqMachine *mach, int cycles);

void mq_mach_syscall(mqMachine *mach);

bool mq_mach_initHeap(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_MACHINE_H */
