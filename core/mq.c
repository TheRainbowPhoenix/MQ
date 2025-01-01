//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/mq.h>
#include <mq/hooks.h>

static int moduleCount = 0;

void mq_init(void)
{
    mq_callhook_init();
}

void mq_quit(void)
{
    mq_callhook_quit();
}

int mq_module_register(void)
{
    return moduleCount++;
}

int mq_module_count(void)
{
    return moduleCount;
}
