//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/system/casiowin.h>
#include <mq/system/heap.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

static u32 syscall_handler(enum mqCasiowin_Version version)
{
    switch(version) {
    case MQ_CASIOWIN_FX205:     return 0x80010070;
    case MQ_CASIOWIN_CG380:     return 0x80020070;
    }
    mq_log(MQ_LOG_ERROR, "syscall_handler: missing for %d o(x_x)o", version);
    return 0;
}

static u32 os_base_address(enum mqCasiowin_Version version)
{
    switch(version) {
    case MQ_CASIOWIN_FX205:     return 0x80010000;
    case MQ_CASIOWIN_CG380:     return 0x80020000;
    }
    mq_log(MQ_LOG_ERROR, "os_base_address: missing for %d o(x_x)o", version);
    return -1;
}

static u32 os_footer_address(enum mqCasiowin_Version version)
{
    switch(version) {
    case MQ_CASIOWIN_FX205:
        return 0x8024ff18;
    case MQ_CASIOWIN_CG380:
        return 0x80b5ffe0;
    }
    mq_log(MQ_LOG_ERROR, "os_footer_address: missing for %d o(x_x)o", version);
    return -1;
}

static char const *os_version_string(enum mqCasiowin_Version version)
{
    switch(version) {
    case MQ_CASIOWIN_FX205:     return "02.05.0000";
    case MQ_CASIOWIN_CG380:     return "03.80.0000";
    }
    mq_log(MQ_LOG_ERROR, "os_version_string: missing for %d o(x_x)o", version);
    return "99.99.9999";
}

static char const *os_date_string(enum mqCasiowin_Version version)
{
    switch(version) {
    case MQ_CASIOWIN_FX205:     return "2015.0207.1555";
    case MQ_CASIOWIN_CG380:     return "2023.0419.1456";
    }
    mq_log(MQ_LOG_ERROR, "os_date_string: missing for %d o(x_x)o", version);
    return "9999.9999.9999";
}

bool mq_casiowin_setup(mqMachine *mach, enum mqCasiowin_Version version)
{
    mach->cpu.syscallHandler = syscall_handler(version);

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

static void syscall_fx(mqMachine *mach, mqCpu *cpu, u32 syscallID)
{
    mq_log(MQ_LOG_ERROR, "Unknown FX syscall %%%03x, getting stuck.",
        syscallID);
    mach->stuck = true;
}

static void syscall_cg(mqMachine *mach, mqCpu *cpu, u32 syscallID)
{
    // TODO: Check syscall API version

    /* Log except for syscalls that happen often */
    if(syscallID != 0x1e6 && syscallID != 0x25f && syscallID != 0x2c1 &&
       syscallID != 0x1dd0 && !(syscallID >= 0x1f41 && syscallID <= 0x1f46)
       && syscallID != 0x1170)
        mq_log(MQ_LOG_DEBUG, "Syscall! r0=%08x", syscallID);

    switch(syscallID) {
    case 0x0029: /* ??? - Glib_AddInAplExecutionCheck something like that. */
        mq_log(MQ_LOG_WARNING, "Ignoring %%029, what is that?");
        /* Just return 0. */
        cpu->r[0] = 0;
        return;

    case 0x01e6: /* GetVRAMAddress() */
        // FIXME: GetVRAMAddress() is normally in P2
        cpu->r[0] = 0x8c000000;
        return;

    case 0x025f: /* Bdisp_PutDisp_DD() */
        if(mqDisplay_setFormat(mach->display, MQ_DISPLAY_FORMAT_RGB565,
                               396, 224)) {
            u16 *src = mq_memory_access(mach->memory, 0x8c000000);
            u16 *dst = mach->display->data + 6;
            for(int y = 0; y < 216; y++) {
                for(int x = 0; x < 384; x++)
                    dst[x] = *(u16 *)((uintptr_t)(src++) ^ 2);
                dst += mach->display->width;
            }
            mqDisplay_setDirty(mach->display, true);
        }
        return;

    case 0x02c1: { /* RTC_GetTicks() */
        // FIXME: GetTicks() more than trivial counter
        static int ticks = 0;
        cpu->r[0] = ++ticks;
        return;
    }

    case 0x1170: { /* itoa() */
        int num = cpu->r[4];
        char *dst = mq_memory_access(mach->memory, cpu->r[5]);
        char str[32];
        int len = sprintf(str, "%d", num);
        for(int i = 0; i <= len; i++)
            *(u8 *)((uintptr_t)(dst++) ^ 3) = str[i];
        // This differs from SimLo prototype but likely right? I didn't check.
        cpu->r[0] = cpu->r[5];
        return;
    }

    case 0x1da3: /* Bfile_OpenFile_OS() */
        cpu->r[0] = -1;
        return;
    case 0x1db6: /* Bfile_FindFirst() */
        cpu->r[0] = -1;
        return;
    case 0x1dba: /* Bfile_FindClose() */
        cpu->r[0] = -1;
        return;
    case 0x01dae: /* Bfile_CreateEntry() */
        cpu->r[0] = -1;
        return;

    case 0x1dd0: { /* memcpy() */
        // TODO: Optimized aligned memcpy() + put that in mq_memory()
        u8 *dst = mq_memory_access(mach->memory, cpu->r[4]);
        u8 *src = mq_memory_access(mach->memory, cpu->r[5]);
        u32 len = cpu->r[6];
        for(u32 i = 0; i < len; i++)
            *(u8 *)((uintptr_t)(dst++) ^ 3) = *(u8 *)((uintptr_t)(src++) ^ 3);
        cpu->r[0] = cpu->r[4];
        return;
    }

    case 0x1f41:
    case 0x1f42: /* free() */
        mq_mach_initHeap(mach);
        mq_heap_free(cpu->r[4]);
        return;
    case 0x1f43:
    case 0x1f44: /* malloc() */
        mq_mach_initHeap(mach);
        cpu->r[0] = mq_heap_malloc(cpu->r[4]);
        return;
    case 0x1f45:
    case 0x1f46: /* realloc() */
        mq_mach_initHeap(mach);
        cpu->r[0] = mq_heap_realloc(cpu->r[4], cpu->r[5]);
        return;
    }

    mq_log(MQ_LOG_ERROR, "Unknown CG syscall %%%03x, getting stuck.",
        syscallID);
    mach->stuck = true;
}

void mq_casiowin_syscall(mqMachine *mach)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    if(!Casiowin)
        return;

    switch(Casiowin->version) {
    case MQ_CASIOWIN_FX205:
        syscall_fx(mach, &mach->cpu, mach->cpu.r[0]);
        mach->cpu.pc = mach->cpu.spRegs[SH_PR];
        break;
    case MQ_CASIOWIN_CG380:
        syscall_cg(mach, &mach->cpu, mach->cpu.r[0]);
        mach->cpu.pc = mach->cpu.spRegs[SH_PR];
        break;
    }
}
