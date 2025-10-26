//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/interfaces/timer.h>
#include <string.h>
#include <time.h>

static u64 globalDelay = 0;
static u64 globalFreezeStart = 0;

/* Ugly hack to improve perf on web
 * (Timers are sloow) */
// EMSCRIPTEN IFDEF
int _timer_lock = 0;
u64 _locked_time;
// ENDIF

static u64 currentTime(void)
{
// EMSCRIPTEN IFDEF
    if(_timer_lock > 1)
        return _locked_time;
// ENDIF

    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    u64 time = (1000000000ull * tp.tv_sec) + tp.tv_nsec;

// EMSCRIPTEN IFDEF
    if(_timer_lock){
      _locked_time = time;
      _timer_lock = 2;
    }
// ENDIF

    return time;
}

void mq_timer_freeze(void)
{
    globalFreezeStart = currentTime();
    globalFreezeStart += (globalFreezeStart == 0);
}

void mq_timer_unfreeze(void)
{
    if(!globalFreezeStart)
        return;
    globalDelay += currentTime() - globalFreezeStart;
    globalFreezeStart = 0;
}

u64 mq_timer_globalTime(void)
{
    if(globalFreezeStart)
        mq_log(MQ_LOG_ERROR, "getting global time while frozen!");
    return currentTime() - globalDelay;
}

void mq_timer_reset(mqTimer *timer, u64 resolution_ns)
{
    memset(timer, 0, sizeof *timer);
    timer->tickResolution = resolution_ns + (resolution_ns == 0);
}

void mq_timer_start(mqTimer *timer)
{
    timer->lastUpdate = mq_timer_globalTime();
    timer->running = true;
}

bool mq_timer_isRunning(mqTimer *timer)
{
    return timer->running;
}

static u64 mq_timer_update_ns(mqTimer *timer)
{
    u64 now = mq_timer_globalTime();
    u64 difference = now - timer->lastUpdate;
    timer->lastUpdate = now;
    return difference;
}

u64 mq_timer_update(mqTimer *timer)
{
    u64 ns = mq_timer_update_ns(timer);
    timer->remainder += ns;
    u64 fullTicks = timer->remainder / timer->tickResolution;
    timer->remainder %= timer->tickResolution;
    timer->ticks += fullTicks;
    return fullTicks;
}

u64 mq_timer_elapsedTicks(mqTimer *timer)
{
    return timer->ticks;
}
