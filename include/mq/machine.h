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
#include <mq/interfaces/display.h>
#include <mq/interfaces/keyboard.h>
#include <mq/interfaces/timer.h>
MQ_START_DEFS

struct mqMemory;

/* Type of a background process that runs every few CPU cycles.
   TODO: Provide background process hooks with more precising timing info */
typedef void mq_process_t(struct mqMachine *mach, int cyclesElapsed);

// TODO
struct mqMachine
{
    mqCpu cpu;
    struct mqMemory *memory;

    /* Machine is initialized to a reasonable state. */
    bool initialized;
    /* Machine is stuck and cannot execute any further. */
    bool stuck;
    /* Machine is internally paused for a limited time. This is a high-level
       emulation of sleep functions. While internally paused, the machine still
       runs background processes but no CPU instructions, and counts ticks from
       `internalPauseTimer`. When `internalPauseTicksRemaining` reaches 0 the
       internal pause ends automatically. */
    // TODO: Host system sleeps for long high-level sleeps (... but timers?)
    bool internallyPaused;
    mqTimer internalPauseTimer;
    int internalPauseTicksRemaining;
    /* Machine is internally blocked for a high-level reason (e.g. a blocking
       syscall). Unlike internal pausing, this status does not expire
       automatically and must be cleared by blocking code. */
    bool internallyBlocked;

    /* Data from hardware modules; the array has size mq_module_count(). */
    void **modules;
    /* List of background processes; the array has size mq_process_count(). */
    mq_process_t **processes;
    /* Number of cycles left before running background processes */
    int processTimer;
    /* How many cycles between each run of background processes */
    int processFrequency;

    /* Generic devices associated with the machine to interface either with the
       user or with host system resources. */

    /* Display; may be NULL */
    mqDisplay *display;
    /* Keyboard; may be NULL */
    mqKeyboard *keyboard;

    // TODO: Data source for mqTimer; to enable deterministic execution instead
    // of always using clock_gettime().
};

typedef struct mqMachine mqMachine;

/* CRD functions for mqMachine. The default state is the default CPU and memory
   state, not initialized, no modules or processes, no display or keyboard. */
mqMachine *mq_machine_create(void);
void mq_machine_reset(mqMachine *mach);
void mq_machine_destroy(mqMachine *mach);

/* Observer functions for mqMachine. These functions make and destroy an
   "observer" copy of the machine with a snapshot of the metadata but no
   contractual ownership of large memory buffers and no execution abilities by
   contract (this isn't enforced API-wise). */
mqMachine *mq_machine_createObserver(mqMachine const *mach);
void mq_machine_destroyObserver(mqMachine *omach);

enum {
    MQ_MACHINE_HARDWARE_VIRT_ADDIN_FX,
    MQ_MACHINE_HARDWARE_VIRT_ADDIN_CG,
};
enum {
    MQ_MACHINE_INITIALIZE_ADDIN,
    //MQ_MACHINE_INITIALIZE_POWERON_RESET,
    //MQ_MACHINE_INITIALIZE_MANUAL_RESET,
};

void mq_machine_setupHardware(mqMachine *mach, int hardwareKind);
void mq_machine_initialize(mqMachine *mach, int initializeKind);

bool mq_machine_load_g1a(mqMachine *mach, void *data, long size);
bool mq_machine_load_g3a(mqMachine *mach, void *path, long size);

int mq_machine_cycle(mqMachine *mach, int cycles);

void mq_machine_runProcesses(mqMachine *mach, int cyclesElapsed);

/* Make the CPU wait the given amount of time. This simulates a sleep function.
   Background processes will still run during that time, but the CPU will not.

   (It would be great to yield the thread back to the system in this case. But
    timers make that difficult.)

   TODO: Test internal pauses in-depth and consider better implementations. */
void mq_machine_internalPauseMilliseconds(mqMachine *mach, int delay_ms);

MQ_END_DEFS
#endif /* MQ_MACHINE_H */
