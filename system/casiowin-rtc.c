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

int mq_casiowin_rtc_reset(mqMachine *mach, u32 mode)
{
    mqRTC *RTC = mq_rtc_get(mach);
    if (RTC == NULL)
        return -1;

    // [hack] Casio first request a reset of the RTC, but since we are in a
    // special case on which we cannot continue the emulation to wait that
    // the bit is handled, simulate the behaviour here.
    //
    // TODO: find a better way to handle register interaction outside emulation
    // TODO: RTC->RCR2 |= 0x0a;
    RTC->R64CNT = 0x00;
    RTC->R256_cnt = 0;
    if(mode != 0) {
        RTC->RSECCNT = 0;
        RTC->RMINCNT = 0;
        RTC->RHRCNT  = 0;
        RTC->RWKCNT  = 0;
        RTC->RDAYCNT = 0;
        RTC->RMONCNT = 0;
        RTC->RYRCNT  = 0;
        RTC->RSECAR  = 0;
        RTC->RMINAR  = 0;
        RTC->RHRAR   = 0;
        RTC->RWKAR   = 0;
        RTC->RDAYAR  = 0;
        RTC->RMONAR  = 0;
        RTC->RYRAR   = 0;
    }
    RTC->RCR2 = (RTC->RCR2 & 0xfd) | 0x09;
    return 0;
}
