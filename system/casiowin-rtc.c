//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
#include <mq/system/casiowin.h>
#include <mq/modules/rtc.h>

/* RTC_GetTicks() - get RTC ticks */
int mq_casiowin_rtc_getticks(mqMachine *mach, u32 *ret)
{
    mqRTC *RTC = mq_rtc_get(mach);
    if (RTC == NULL)
        return -1;
    *ret  = 0;
    *ret += mq_rtc_int8(RTC->RMINCNT) * 0x1e00;
    *ret += mq_rtc_int8(RTC->RSECCNT) * 0x80;
    *ret += mq_rtc_int8(RTC->RHRCNT)  * 0x70800;
    *ret += RTC->R64CNT;
    return 0;
}
