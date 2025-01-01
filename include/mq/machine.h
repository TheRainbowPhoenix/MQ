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
#include <mq/interfaces/display.h>
#include <mq/interfaces/keyboard.h>
MQ_START_DEFS

// TODO
struct mqMachine
{
    mqCpu cpu;
    mqMemory *memory;

    /* Machine is initialized to a reasonable state. */
    bool initialized;
    /* Machine is stuck and cannot execute any further. */
    bool stuck;

    /* TODO: MPU details/peripheral modules */

    /* System emulation details */
    struct {
        // TODO: System API version
        u32 heapAddress;
        u32 heapSize;
    } system;

    /* Generic devices associated with the machine to interface either with the
       user or with host system resources. */

    /* Display; may be NULL. */
    mqDisplay *display;
    // Keyboard; may be NULL.
    mqKeyboard *keyboard;
    // TODO: Real-time tiemr; may be NULL.
    // mqTimer *timer;
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
