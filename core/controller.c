//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/controller.h>
#include <mq/system/casiowin.h>
#include <stdlib.h>
#include <string.h>

static void thread_syncFPS(mqController *controller)
{
    mqMachine *mach = controller->mach;

    u64 new_timeRef = mq_timer_getCurrentSystemTime();
    u64 new_timeDelta = 0;
    i64 new_timePause = 0;
    if(controller->lastFrame.timeRef > 0 && controller->requestFps > 0) {
        new_timeDelta = new_timeRef -
            (controller->lastFrame.timeRef + controller->lastFrame.timePause);
        new_timePause = ((1000 * 1000000) / controller->requestFps)
                        - new_timeDelta;
        /* Sadly, we can't rewind time if the frame is longer than desired. */
        if(new_timePause < 0)
            new_timePause = 0;

        /* mq_log(MQ_LOG_DEBUG,
            "new frame!\n"
            "bef: ref=%lld (delta=%lld) +  pause=%lld\n"
            "new: ref=%lld (delta=%lld) -> pause=%lld",
            controller->lastFrame.timeRef, controller->lastFrame.timeDelta,
            controller->lastFrame.timePause,
            new_timeRef, new_timeDelta, new_timePause);
        mq_log(MQ_LOG_DEBUG,
            "FPS=%lld (raw: %lld) (target: %llu)",
            1000 * 1000000 / (new_timeRef - controller->lastFrame.timeRef),
            1000 * 1000000 / new_timeDelta,
            controller->requestFps); */
    }

    if(new_timePause > 0)
        mq_machine_internalPauseMilliseconds(mach, new_timePause / 1000000);

    controller->lastFrame.dirty = true;
    controller->lastFrame.previousTimeRef = controller->lastFrame.timeRef;
    controller->lastFrame.timeDelta = new_timeDelta;
    controller->lastFrame.timeRef = new_timeRef;
    controller->lastFrame.timePause = new_timePause;

    mq_display_setFrameChanged(mach->display, false);
}

static void *thread_run(void *userdata)
{
    mqController *controller = userdata;
    mqMachine *mach = controller->mach;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    while(true) {
        // printf("[Emu] Locking machine for work\n");
        mq_machine_lock(mach);
        // printf("[Emu] Locked machine\n");

        int cycles = 20000;

#if MQ_CONTROLLER_SETJMP
        int rc = mq_machine_setBreakJumpBuffer(mach);

        if(rc == 0) {
            mq_timer_unfreeze();
            TracyCZoneN(_ctxA, "cycles", true);
            mq_machine_cycle(mach, cycles);
            TracyCZoneEnd(_ctxA);
            /* If we longjmp out, this call never finishes */
        }
        mq_timer_freeze();
        mq_machine_clearBreakJumpBuffer(mach);
#else
        TracyCZoneN(_ctxA, "cycles", true);
        mq_timer_unfreeze();
        mq_machine_cycle(mach, cycles);
        mq_timer_freeze();
        TracyCZoneEnd(_ctxA);
#endif

        mach->breakFlag = false;

        /* There was an execution break and now the machine is blocked from
           running more instructions. It's either waiting for a background
           process or stuck due to an unrecoverable error. */
        if(mach->stuck) {
            mq_log(MQ_LOG_WARNING, "machine is stuck!");
            mach->cyclesPending = 0;
        }
        else if(mach->display && mach->display->frameChanged)
            thread_syncFPS(controller);

        // printf("[Emu] Unlocking machine and waiting for work\n");
        TracyCZoneN(_ctxB, "wait", true);
        mq_machine_unlockAndWaitForWork(mach);
        TracyCZoneEnd(_ctxB);
        // printf("[Emu] Work has arrived!\n");
    }

    return NULL;
}

mqController *mq_controller_create(void)
{
    mqController *controller = calloc(1, sizeof *controller);
    return controller;
}

void mq_controller_reset(mqController *controller)
{
    mq_controller_stopThread(controller);

    if(controller->mach)
        mq_machine_destroy(controller->mach);
    if(controller->omach)
        mq_machine_destroyObserver(controller->omach);

    controller->mach = NULL;
    controller->omach = NULL;
}

void mq_controller_destroy(mqController *controller)
{
    if(!controller)
        return;

    mq_controller_reset(controller);
    free(controller);
}

bool mq_controller_startThread(mqController *controller, mqMachine *mach)
{
    controller->mach = mach;
    controller->omach = NULL;

    int rc = pthread_create(&controller->td, NULL, thread_run, controller);
    if(rc) {
        mq_log(MQ_LOG_ERROR, "could not start thread: %s\n", strerror(rc));
        return false;
    }

    controller->hasThread = true;
    return true;
}

void mq_controller_stopThread(mqController *controller)
{
    if(!controller->hasThread)
        return;

    pthread_cancel(controller->td);
    pthread_join(controller->td, NULL);
    /* 0 may not be an invalid thread ID, but it's recognizable at least */
    controller->td = 0;
    controller->hasThread = false;
}

