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
#include <pthread.h>
#include <setjmp.h>
MQ_START_DEFS

struct mqMemory;
struct mqFilesystem;
struct mqBfile;

/* Type of a background process that runs every few CPU cycles.
   TODO: Provide background process hooks with more precising timing info */
typedef void mq_process_t(struct mqMachine *mach, int cyclesElapsed);

// TODO
struct mqMachine
{
    mqCpu cpu;
    struct mqMemory *memory;

    /* Mutex for access to the main structure. */
    pthread_mutex_t lock_access;
    /* Waiting room for lock_access. To get access, acquire lock_waiting, then
       acquire lock_access, then release lock_access. This prevents the
       emulation thread, which pretty much tries to acquire lock_access all the
       time, from denying the UI thread to use it. The UI thread will acquire
       lock_waiting while the emulation thread has lock_access but not
       lock_waiting. */
    pthread_mutex_t lock_waiting;
    /* Condition variable triggered when the machine had no work and now has
       work. This wakes the emulating up from sleep. Linked to lock_access. */
    pthread_cond_t cond_work_arrived;

    /* Whether instruction-level profiling is enabled. This results in *large*
       amounts of profiling allocations (like 1 GB/s) until Tracy's capture
       tool is started. */
    bool profilingCycles;

    /* Machine is initialized to a reasonable state. */
    bool initialized;
    /* Machine is stuck and cannot execute any further. */
    bool stuck;
    /* Jump buffer to jump to when the execution gets broken. This is used to
       exit. The jump buffer may or may not
       be present. */
    bool hasBreakJumpBuffer;
    jmp_buf breakJumpBuffer;

    /* Machine is internally paused for a limited time. This is a high-level
       emulation of sleep functions. While internally paused, the machine still
       runs background processes but no CPU instructions, and counts ticks from
       `internalPauseTimer`. When `internalPauseTicksRemaining` reaches 0 the
       internal pause ends automatically. */
    // TODO: Host system sleeps for long high-level sleeps (... but timers?)
    bool internallyPaused;
    mqTimer internalPauseTimer;
    int internalPauseTicksRemaining;

    /* Machine is internally blocked for a high-level reason, e.g. a blocking
       syscall, a timed pause, or stuck. This status does not expire
       automatically. */
    bool internallyBlocked;

    /* Number of cycles that the machine is scheduled to work for.
       mq_machine_cycle() runs for up to that amount and subtracts it. */
    int cyclesPending;
    /* Number of frames that the machine is scheduled to work for. */
    int framesPending;

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
    /* FileSystem; should not be NULL */
    struct mqFilesystem *fs;
    /* Bfile; should not be NULL */
    struct mqBfile *bfile;

    // TODO: Data source for mqTimer; to enable deterministic execution instead
    // of always using clock_gettime().
};

typedef struct mqMachine mqMachine;

/* CRD functions for mqMachine. The default state is the default CPU and memory
   state, not initialized, no modules or processes, no display or keyboard. */
mqMachine *mq_machine_create(void);
void mq_machine_reset(mqMachine *mach);
void mq_machine_destroy(mqMachine *mach);

/* Acquire or release the lock to use the machine. This is needed for all
   operations. Creating an observer must also be done while holding the lock,
   but once the observer is created it can be used with the lock released.
   (Some information, like memory contents, can be inaccurate in that case.) */
void mq_machine_lock(mqMachine *mach);
void mq_machine_unlock(mqMachine *mach);
/* If there is more work to do, unlock the machine. Otherwise, unlock the
   machine and wait on the condition variable (atomically). */
void mq_machine_unlockAndWaitForWork(mqMachine *mach);

/* Mark the machine as stuck and break the execution. */
void mq_machine_setStuck(mqMachine *mach);

/* Break the machine's execution. This signals that we have to stop executing
   instructions, either because the code is blocking (e.g. waiting for a
   background syscall to end) or because the machine is fully stuck.

   This function sets the machine's internallyBlocked flag which breaks the
   fetch-decode-execute loop in one of three ways:

   * If we're not executing instructions, there's nothing to break. Otherwise:
   * In builds where MQ_CONTROLLER_SETJMP is enabled, a break buffer is set up
     when executing instructions, and breaking longjmps back to it.
   * In other builds, internallyBlocked is checked after each instruction and
     control returns to the controller through a series of function returns.

   Because in some builds this function doesn't return, it should always be
   used as a tail call. It can only be used in functions whose caller are fine
   with not returning, so each use must be studied separately. */
void mq_machine_breakExecution(mqMachine *mach);

/* Set a jump buffer target that the machine unwinds to if for some reason we
   want to interrupt the machine fetch-decode-execute loop. The current context
   is saved in an internal jmp_buf in the machine. This macro contains a
   setjmp() and returns twice. setStuck() does the longjmp() back. */
#define mq_machine_setBreakJumpBuffer(mach) ({ \
   extern void mq_machine_setBreakJumpBufferAux(mqMachine *mach); \
   mq_machine_setBreakJumpBufferAux(mach); \
   setjmp(mach->breakJumpBuffer); \
})
/* Clear the jump buffer target that the machine unwinds to upon a break. */
void mq_machine_clearBreakJumpBuffer(mqMachine *mach);

/* Set the number of pending cycles. This controls the execution of the
   machine. Setting 0 pauses it. Setting a negative number makes it run with no
   limit. Setting a positive integer makes it run for that number of cycles.
   (Given the concurrent nature of emulation, setting a finite number is only
   really useful if the machine was previously paused.) */
void mq_machine_setCyclesPending(mqMachine *mach, int cyclesPending);

/* Set the number of pending frames. */
void mq_machine_setFramesPending(mqMachine *mach, int framesPending);

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

/* set the emulated filesystem root path */
bool mq_machine_setFilesystemRoot(mqMachine *mach, char const *pathname);

MQ_END_DEFS
#endif /* MQ_MACHINE_H */
