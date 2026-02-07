//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.controller: Machine controller running in its own thread
//---

#ifndef MQ_CONTROLLER_H
#define MQ_CONTROLLER_H

#include <mq/machine.h>
#include <pthread.h>
MQ_START_DEFS

/* A controller manages a machine. It's responsible for running the machine in
   a separate thread, stopping to allow other threads to make observers or send
   it instructions, and keeping track of time. The mqController API doesn't
   completely abstract away the mqMachine API: mqController views the machine
   as a black box, and access to the memory, CPU, I/O, etc should still go
   through mqMachine. */
struct mqController
{
    /* Identifier of the thread where the machine is running. Invalid until the
       controller is initialized. */
    pthread_t td;
    /* Whether the thread exists. */
    bool hasThread;

    /* Machine being run by the thread. When running it is locked must of the
       time, except in short periods where the GUI locks it to prepare frames
       then to update the machine. This is a constant after creation. */
    mqMachine *mach;
    /* Observer machine containing snapshots of the main machine's state, used
       by the GUI and (mostly) independent from the emulation thread. This
       pointer is never accessed by the emulation thread. */
    // TODO: make omach const in mqController to enforce observer semantics
    mqMachine *omach;

    // TODO: Not happy about the fact that new frames are notified through
    // controller but new pixels are still through mqDisplay.

    /* Data about frame timing.

       |=== Frame #n ===|== Frame #n+1 ==|
       +--------+-------+--------+-------+
       | Calcul | Pause | Calcul | Pause |
       +--------+-------+--------+-------+
                ^<-----><------->^
                | pause   delta  |
               ref              ref */
    struct {
        /* Set when the controller detects a new frame; cleared by GUI. */
        bool dirty;

        u64 timeRef; // when the frame was emitted
        u64 timeDelta; // how long we spent computing it
        u64 timePause; // how long we're gonna wait for FPS limiting

        u64 previousTimeRef; // when the previous frame was emitted
    } lastFrame;

    u64 requestFps;

    /* FPS limiter; enabled if > 0. This is the required minimum delay between
       two frames (in nanoseconds). If a new frame is completed and less that
       this time has elapsed since the previous frame completed, the machine
       will be paused until the time target is reached. */
    // TODO: Use frameTimeTarget instead of requestFps
    // u64 frameTimeTarget;
};

typedef struct mqController mqController;

/* CRD functions. The default state has no thread and no machine. Resetting
   destroys the machine if there was one. */
mqController *mq_controller_create(void);
void mq_controller_reset(mqController *controller);
void mq_controller_destroy(mqController *controller);

/* Take over a machine. This spins up a thread, although that thread will be
   idle if mach has no work to do. The controller takes ownership of the
   machine; after creation, the caller must conform to machine access patterns
   allowed by the controller. On error, returns false and doesn't take over the
   machine (caller should destroy it). */
bool mq_controller_startThread(mqController *controller, mqMachine *mach);

/* Stop the thread. In theory there could be a way to restart it or whatnot,
   but not right now. After stopping, machine's dead, reset the controller. */
void mq_controller_stopThread(mqController *controller);

// TODO: Controller should handle setting the number of cycles remaining
// (and should maybe even count them, not the machine)

// TODO: Controller should handle creation and refresh of observers

// TODO: Performance statistics

MQ_END_DEFS
#endif /* MQ_CONTROLLER_H */
