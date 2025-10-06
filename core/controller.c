//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/controller.h>
#include <stdlib.h>
#include <string.h>

static void *thread_run(void *userdata)
{
    mqController *controller = userdata;
    mqMachine *mach = controller->mach;

    while(true) {
        // printf("[Emu] Locking machine for work\n");
        mq_machine_lock(mach);
        // printf("[Emu] Locked machine\n");

        int cycles = 20000;
        TracyCZoneN(_ctxA, "cycles", true);
        mq_machine_cycle(mach, cycles);
        TracyCZoneEnd(_ctxA);

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
    int rc = pthread_create(&controller->td, NULL, thread_run, controller);
    if(rc) {
        mq_log(MQ_LOG_ERROR, "could not start thread: %s\n", strerror(rc));
        return false;
    }

    controller->mach = mach;
    controller->omach = NULL;
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

