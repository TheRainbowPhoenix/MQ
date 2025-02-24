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

    bool ok = true;

    mqChunk *ch1 = mq_memory_getOrCreateChunk(mach->memory, OSBase & ~0xfffff);
    if(!ch1)
        return false;
    mqMMIOPage *mmpg_os = mq_chunk_getOrCreateMMIOPage(ch1, OSBase, 0, 1);
    if(!mmpg_os)
        return false;
    mqMMIOPage *mmpg_eboot =
        mq_chunk_getOrCreateMMIOPage(ch1, OSBase - 0x1000, 0, 1);
    if(!mmpg_eboot)
        return false;

    mqChunk *ch2 = NULL;
    mqMMIOPage *mmpg_footer = NULL;
    if(footer != (u32)-1) {
        ch2 = mq_memory_getOrCreateChunk(mach->memory, footer & ~0xfffff);
        if(!ch2)
            return false;
        mmpg_footer = mq_chunk_getOrCreateMMIOPage(ch2, footer & ~0xfff, 0, 1);
        if(!mmpg_footer)
            return false;
    }

    mqCasiowin *Casiowin = calloc(1, sizeof *Casiowin);
    if(!Casiowin)
        goto end;

    Casiowin->version = version;
    memcpy(Casiowin->str_version, os_version_string(version), 10);
    memcpy(Casiowin->str_serial, "mq000000", 8);
    memcpy(Casiowin->str_date, os_date_string(version), 14);

    ok &= mq_page_mapString(mmpg_eboot, "CW_SERIAL", OSBase - 0x30,
        Casiowin->str_serial, 8);
    ok &= mq_page_mapString(mmpg_os, "CW_VERSION", OSBase + 0x20,
        Casiowin->str_version, 10);

    if(mmpg_footer) {
        ok &= mq_page_mapString(mmpg_footer, "CW_DATE", footer,
            Casiowin->str_date, 14);
    }

end:
    if(ok)
        mach->modules[moduleID] = Casiowin;
    else if(Casiowin)
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
