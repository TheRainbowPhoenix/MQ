//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.interfaces.timer: Generic interface for timers

#ifndef MQ_INTERFACES_TIMER_H
#define MQ_INTERFACES_TIMER_H

#include <mq/defs.h>
#include <time.h>
MQ_START_DEFS

/* A timer counting nanoseconds and dividing them up into "ticks" of user-
   defined length. */
struct mqTimer {
   /* Timestamp of when the timer last started or got updated (ns). */
   u64 lastUpdate;
   /* Tick resolution, i.e. how many nanoseconds per tick. */
   u64 tickResolution;
   /* Accumulated time over multiple runs of the timer, dividing in ticks and
      remainder (where remainder < tickResolution). */
   u64 ticks;
   u64 remainder;
   /* Whether the timer is running (set to counting mode by user). */
   bool running;
   /* Whether the timer is paused (counting is momentarily suspended by system
      because e.g. emulator is paused), regardless of user's wishes. */
   bool frozen;
};

typedef struct mqTimer mqTimer;

/* Reset the timer and set the tick duration to `resolution_ns` nanoseconds. */
void mq_timer_reset(mqTimer *timer, u64 resolution_ns);

/* Start tracking time in the timer. */
void mq_timer_start(mqTimer *timer);

/* Whether a time is running--true between start() and reset(). */
bool mq_timer_isRunning(mqTimer *timer);

/* Update the timer. This fetches the current time and adds any new ticks to
   the total counter. The timer only advances when this function is called.
   Returns the number of full ticks elapsed since the last update. */
u64 mq_timer_update(mqTimer *timer);
/* Update the timer, but only report up to `maxTicks` ticks elapsed. If time
   advanced more than that, extra ticks will be remembered and counted in a
   later update. maxTicks = 0 disables the limit, like mq_timer_update(). */
u64 mq_timer_updateWithLimit(mqTimer *timer, u64 maxTicks);

/* Number of full ticks elapsed between starting the timer and the last time
   the timer was updated. */
u64 mq_timer_elapsedTicks(mqTimer *timer);

//=== System control functions ===============================================//
// The following functions globally controls whether timers count or not. This
// is used when pausing the emulator and when rendering frames to avoid timers
// underflowing while the emulator is not actually processing things.
// TODO[timer]: Freezing inherently prevents reaching real-time speed!

u64 mq_timer_globalTime(void);

void mq_timer_freeze(void);

void mq_timer_unfreeze(void);

MQ_END_DEFS
#endif /* MQ_INTERFACES_TIMER_H */
