//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/system/casiowin.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdlib.h>
#include <string.h>

static int moduleID = -1;

static void inithook(void)
{
    moduleID = mq_module_register();
}
MQ_HOOK_REGISTER(init, inithook)

mqCasiowin *mq_casiowin_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static u32 os_base_address(enum mqCasiowin_Version version)
{
    switch(version) {
    case MQ_CASIOWIN_CG380:
        return 0x80020000;
    }
    mq_log(MQ_LOG_ERROR, "os_base_address: missing for %d o(x_x)o", version);
    return -1;
}

static u32 os_footer_address(enum mqCasiowin_Version version)
{
    switch(version) {
    case MQ_CASIOWIN_CG380:
        return 0x80b5ffe0;
    }
    mq_log(MQ_LOG_ERROR, "os_footer_address: missing for %d o(x_x)o", version);
    return -1;
}

static char const *os_version_string(enum mqCasiowin_Version version)
{
    switch(version) {
    case MQ_CASIOWIN_CG380:     return "03.80.0000";
    }
    mq_log(MQ_LOG_ERROR, "os_version_string: missing for %d o(x_x)o", version);
    return "99.99.9999";
}

static char const *os_date_string(enum mqCasiowin_Version version)
{
    switch(version) {
    case MQ_CASIOWIN_CG380:     return "2023.0419.1456";
    }
    mq_log(MQ_LOG_ERROR, "os_date_string: missing for %d o(x_x)o", version);
    return "9999.9999.9999";
}

bool mq_casiowin_setup(mqMachine *mach, enum mqCasiowin_Version version)
{
    u32 OSBase = os_base_address(version);
    u32 footer = os_footer_address(version);
    if(!OSBase || (OSBase & 0xfff))
        return false;

    mqPage *pg_os = mq_memory_getPage(mach->memory, OSBase);
    mqPage *pg_eboot = mq_memory_getPage(mach->memory, OSBase - 0x1000);
    if(!pg_os || !pg_eboot)
        return false;

    mqPage *pg_footer = NULL;
    if(footer != (u32)-1) {
        pg_footer = mq_memory_getPage(mach->memory, footer & ~0xfff);
        if(!pg_footer)
            return false;
    }

    mqCasiowin *Casiowin = calloc(1, sizeof *Casiowin);
    if(!Casiowin)
        return false;

    Casiowin->version = version;
    memcpy(Casiowin->str_version, os_version_string(version), 10);
    memcpy(Casiowin->str_serial, "mq000000", 8);
    memcpy(Casiowin->str_date, os_date_string(version), 14);

    bool ok = true;
    ok &= mq_page_mapString(pg_eboot, "CW_SERIAL", OSBase - 0x30,
        Casiowin->str_serial, 8);
    ok &= mq_page_mapString(pg_os, "CW_VERSION", OSBase + 0x20,
        Casiowin->str_version, 10);

    if(pg_footer) {
        ok &= mq_page_mapString(pg_footer, "CW_DATE", footer,
            Casiowin->str_date, 14);
    }

    if(ok)
        mach->modules[moduleID] = Casiowin;
    else
        free(Casiowin);
    return ok;
}

static void mq_casiowin_cleanup(mqMachine *mach)
{
    mqCasiowin *Casiowin = mach->modules[moduleID];
    if(Casiowin)
        free(Casiowin);
}
MQ_HOOK_REGISTER(module_cleanup, mq_casiowin_cleanup)
