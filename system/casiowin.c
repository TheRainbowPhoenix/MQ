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

static int keymap_fx[7 * 12] = {
    0x753f, -1,     -1,     -1,     -1,     -1,     -1,
    -1,     -1,     0x7534, 0x87,   0x0f,   '.',    '0',
    -1,     -1,     0x99,   0x89,   '3',    '2',    '1',
    -1,     -1,     0xb9,   0xa9,   '6',    '5',    '4',
    -1,     -1,     -1,     0x7549, '9',    '8',    '7',
    -1,     0x0e,   ',',    ')',    '(',    0x755e, 0xbb,
    -1,     0x83,   0x82,   0x81,   0x85,   0x95,   0x7531,
    -1,     0x7545, 0x7547, 0x7532, 0xa8,   0x8b,   0x7537,
    -1,     0x7542, 0x7544, 0x7533, 0x7540, 0x7538, 0x7536,
    -1,     0x753e, 0x753d, 0x753c, 0x753b, 0x753a, 0x7539,
    -1,     -1,     -1,     -1,     -1,     -1,     -1,
    -1,     -1,     -1,     -1,     -1,     -1,     -1,
};

static struct mqCasiowin_OSInfo OSInfo_FX205 = {
    .OSBaseAddress          = 0x80010000,
    .OSFooterAddress        = 0x8024ff18,
    .versionString          = "02.05.0000",
    .dateString             = "2015.0207.1555",
    .syscallStubAddress     = 0x80010070,
    .heapAddress            = 0x88030000, /* @ 192 kB */
    .heapSize               = 48 << 10,

    .rodataAreaAddress      = 0x80240000, /* @ -64 kB, approximately */
    .rodataAreaSize         = 4 << 10,
    .rodataKeymap           = keymap_fx,
    .rodataKeymapSize       = 7 * 12 * 4,

    .dataAreaAddress        = 0x88001000, /* @ 4 kB; VRAM will be aligned */
    .dataAreaSize           = 8 << 10,
    .dataVramSize           = 1024,
    .dataVramCount          = 4,
};
static struct mqCasiowin_OSInfo OSInfo_CG380 = {
    .OSBaseAddress          = 0x80020000,
    .OSFooterAddress        = 0x80b5ffe0,
    .versionString          = "03.80.0000",
    .dateString             = "2023.0419.1456",
    .syscallStubAddress     = 0x80020070,
    .heapAddress            = 0x8c0c0000, /* @ 768 kB */
    .heapSize               = 128 << 10,

    .rodataAreaAddress      = 0x80b40000, /* @ -128 kB, approximately */
    .rodataAreaSize         = 4 << 10,
    .rodataKeymap           = NULL,
    .rodataKeymapSize       = 0,

    .dataAreaAddress        = 0x8c000000, /* @ 0 MB */
    .dataAreaSize           = 0x52000,
    .dataVramSize           = 384 * 216 * 2,
    .dataVramCount          = 2,
};

mqCasiowin_OSInfo const *mq_casiowin_getOSInfo(mqCasiowin_Version version)
{
    switch(version) {
    case MQ_CASIOWIN_FX205:
        return &OSInfo_FX205;
    case MQ_CASIOWIN_CG380:
        return &OSInfo_CG380;
    }
    return NULL;
}

static void setupRodataArea(mqMachine *mach, mqCasiowin *Casiowin)
{
    mqCasiowin_OSInfo const *info = Casiowin->info;
    u32 rodata = info->rodataAreaAddress;

    if(info->rodataKeymap) {
        Casiowin->rodataKeymapAddress = rodata;

        for(int i = 0; i < info->rodataKeymapSize / 4; i++) {
            int key = info->rodataKeymap[i];
            mq_memory_write(mach, mach->memory, rodata, 4, key);
            rodata += 4;
        }
    }

    if(rodata - info->rodataAreaAddress > info->rodataAreaSize) {
        mq_log(MQ_LOG_ERROR, "OS rodata area overflow!");
        mach->stuck = true;
    }
}

static void setupDataArea(mqMachine *mach, mqCasiowin *Casiowin, void *buffer)
{
    mqCasiowin_OSInfo const *info = Casiowin->info;
    u32 data = info->dataAreaAddress;

    /* VRAM */
    if(info->dataVramCount)
        Casiowin->vramLE = buffer;
    for(int i = 0; i < info->dataVramCount; i++) {
        Casiowin->dataVramAddresses[i] = data;
        data += info->dataVramSize;
    }

    // TODO: VRAM backups

    if(data - info->dataAreaAddress > info->dataAreaSize) {
        mq_log(MQ_LOG_ERROR, "OS data area overflow!");
        mach->stuck = true;
    }
}

bool mq_casiowin_setup(mqMachine *mach, mqCasiowin_Version version)
{
    mqCasiowin_OSInfo const *info = mq_casiowin_getOSInfo(version);
    if(!info || !info->OSBaseAddress || (info->OSBaseAddress & 0xfff))
        return false;

    u32 OSBase = info->OSBaseAddress;
    u32 footer = info->OSFooterAddress;

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

    Casiowin->info = mq_casiowin_getOSInfo(version);
    Casiowin->version = version;

    bool ok = true;

    /* Fixed-address data accessed via emulated I/O */
    ok &= mq_page_mapString(pg_eboot, "CW_SERIAL", OSBase - 0x30,
        "mq000000", 8);
    ok &= mq_page_mapString(pg_os, "CW_VERSION", OSBase + 0x20,
        info->versionString, 10);

    if(pg_footer) {
        ok &= mq_page_mapString(pg_footer, "CW_DATE", footer,
            info->dateString, 14);
    }

    /* Flexible-address read-only data mapped in a fictional "data area" */
    void *buf =
        mq_memory_allocBuffer(mach->memory, "OSRODATA", info->rodataAreaSize);
    if(mq_memory_createBlock(mach->memory, info->rodataAreaAddress,
        info->rodataAreaSize, buf)) {
        setupRodataArea(mach, Casiowin);
    }
    else ok = false;

    /* Flexible-address read-write data. Map it to both P1 and P2 since VRAM is
       commonly accessed through P2. */
    buf = mq_memory_allocBuffer(mach->memory, "OSDATA", info->dataAreaSize);
    u32 dataP1 = info->dataAreaAddress;
    u32 dataP2 = (dataP1 & 0x1fffffff) | 0xa0000000;
    if(mq_memory_createBlock(mach->memory, dataP1, info->dataAreaSize, buf)
    && mq_memory_createBlock(mach->memory, dataP2, info->dataAreaSize, buf)) {
        setupDataArea(mach, Casiowin, buf);
    }
    else ok = false;

    /* Export some of the data to other components for optimization purposes */
    mach->cpu.syscallHandler = info->syscallStubAddress;

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

    // TODO[casiowin]: mq_heap_reset: Should be bound to machine, not global!
    mq_heap_reset();
}
MQ_HOOK_REGISTER(module_cleanup, mq_casiowin_cleanup)

static void syscall_fx(mqMachine *mach, mqCpu *cpu, u32 syscallID)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);

    /* Log except for syscalls that happen often */
    if(syscallID != 0x015 && syscallID != 0x135 && syscallID != 0x420 &&
       syscallID != 0xc4f)
        mq_log(MQ_LOG_DEBUG, "Syscall! r0=%08x", syscallID);

    switch(syscallID) {
    case 0x0013: /* GlibAddinAplExecutionCheck() */
        cpu->r[0] = 0;
        return;

    case 0x0014: /* GlibGetAddinLibInf() */
        // No idea what this does, honestly. Decompiled from fx_3.40
        mq_memory_write(mach, mach->memory, mach->cpu.r[4], 4, 0x0);
        mq_memory_write(mach, mach->memory, mach->cpu.r[5], 4, 0x1);
        mq_memory_write(mach, mach->memory, mach->cpu.r[6], 4, 0x1);
        return;

    case 0x0015: /* GlibGetOSVersionInfo() */
        mq_memory_write(mach, mach->memory, mach->cpu.r[4], 1, 0x02);
        mq_memory_write(mach, mach->memory, mach->cpu.r[5], 1, 0x05);
        mq_memory_write(mach, mach->memory, mach->cpu.r[6], 2, 0x2201);
        mq_memory_write(mach, mach->memory, mach->cpu.r[7], 2, 0x0000);
        return;

    case 0x003b: /* RTC_GetTicks() */
        // FIXME: GetTicks() more than trivial counter (also on CG!)
        static int ticks = 0;
        cpu->r[0] = ++ticks;
        return;

    case 0x0135: /* GetVRAMAddress() */
        cpu->r[0] = Casiowin->dataVramAddresses[0];
        return;

    case 0x0146: /* Bdisp_SetPoint_VRAM() */
        mq_casiowin_mono_set_pixel(Casiowin->vramLE,
            cpu->r[4], cpu->r[5], cpu->r[6]);
        return;

    // case 0x014d: /* Bdisp_AreaReverseVRAM() */
    //     return;

    case 0x03fa: /* Hmem_SetMMU() */
        cpu->r[0] = 0;
        return;

    case 0x0420: /* OS_InnerSleep_ms() */
        mq_machine_internalPauseMilliseconds(mach, cpu->r[4]);
        return;

    case 0x042c: /* Bfile_OpenFile() */
        cpu->r[0] = -1;
        return;
    case 0x042d: /* Bfile_CloseFile() */
        cpu->r[0] = -1;
        return;
    case 0x0432: /* Bfile_ReadFile() */
        cpu->r[0] = -1;
        return;
    case 0x0434: /* Bfile_CreateEntry() */
        cpu->r[0] = -1;
        return;
    case 0x0435: /* Bfile_WriteFile() */
        cpu->r[0] = -1;
        return;
    case 0x0439: /* Bfile_DeleteEntry() */
        cpu->r[0] = -1;
        return;

    case 0x0494: /* SetQuitHandler() */
        // TODO: SetQuitHandler() syscall (for saves)
        return;

    case 0x0807: /* Locate() */
        int x = mach->cpu.r[4];
        int y = mach->cpu.r[5];
        if(x >= 1 && x <= 21 && y >= 1 && y <= 8) {
            Casiowin->BdispCursorX = x - 1;
            Casiowin->BdispCursorY = y - 1;
        }
        return;

    case 0x0808: /* Print() */
        mq_casiowin_mono_Print(mach, mach->cpu.r[4], 64);
        return;

    case 0x0813: /* SaveDisp() */
        mq_casiowin_mono_SaveDisp(mach, mach->cpu.r[4]);
        return;

    case 0x0814: /* RestoreDisp() */
        mq_casiowin_mono_RestoreDisp(mach, mach->cpu.r[4]);
        return;

    // case 0x08fe: /* PopupWin() */
    //     return;

    // case 0x090f: /* GetKey() */
    //     return;

    case 0x09ad: /* PrintXY() */
        mq_casiowin_mono_PrintXY(mach,
            mach->cpu.r[4], mach->cpu.r[5], mach->cpu.r[6], mach->cpu.r[7]);
        return;

    case 0x0acc: /* free() */
        mq_casiowin_initHeap(mach);
        mq_heap_free(cpu->r[4]);
        return;
    case 0x0acd: /* malloc() */
        mq_casiowin_initHeap(mach);
        cpu->r[0] = mq_heap_malloc(cpu->r[4]);
        return;

    case 0x0c4f: /* PrintMini() */
        mq_casiowin_mono_PrintMini(mach,
            mach->cpu.r[4], mach->cpu.r[5], mach->cpu.r[6], mach->cpu.r[7]);
        return;

    case 0x0e6d: /* realloc() */
        mq_casiowin_initHeap(mach);
        cpu->r[0] = mq_heap_realloc(cpu->r[4], cpu->r[5]);
        return;

    case 0x1032: /* Get keymap for keycode/matrix code conv. (since 1.05) */
        cpu->r[0] = Casiowin->rodataKeymapAddress;
        return;
    }

    mq_log(MQ_LOG_ERROR, "Unknown FX syscall %%%03x, getting stuck.",
        syscallID);
    mach->stuck = true;
}

static void syscall_cg(mqMachine *mach, mqCpu *cpu, u32 syscallID)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
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

    case 0x012b: /* FKey_Mapping1() */
        mq_log(MQ_LOG_ERROR, "syscall FKey_Mapping1() not supported");
        mach->stuck = true;
        return;

    case 0x01e6: /* GetVRAMAddress() */
        cpu->r[0] = (Casiowin->dataVramAddresses[0] & 0x1fffffff) | 0xa0000000;
        return;

    case 0x025f: /* Bdisp_PutDisp_DD() */
        if(mq_display_setFormat(mach->display, MQ_DISPLAY_FORMAT_RGB565,
                               396, 224)) {
            u16 *src = mq_memory_access(mach->memory, 0x8c000000);
            u16 *dst = mach->display->data + 6;
            for(int y = 0; y < 216; y++) {
                for(int x = 0; x < 384; x++)
                    dst[x] = *(u16 *)((uintptr_t)(src++) ^ 2);
                dst += mach->display->width;
            }
            mq_display_setDirty(mach->display, true);
        }
        return;

    case 0x0272: { /* Bdisp_AllClr_VRAM() */
        u16 *dst = mq_memory_access(mach->memory, 0x8c000000);
        /* Since this is aligned, we can memset it */
        memset(dst, 0xff, 384 * 216 * 2);
        return;
    }

    case 0x02a8: /* DrawFrame() */
        mq_log(MQ_LOG_ERROR, "syscall %%2a8 DrawFrame() not supported");
        mach->stuck = true;
        return;

    case 0x02c1: { /* RTC_GetTicks() */
        // FIXME: GetTicks() more than trivial counter (also on FX!)
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

    case 0x1511: /* memset() */
        /* We can't memset if it's not aligned, because the endianness makes
           the storage non-contiguous! */
        for(u32 i = 0; i < cpu->r[6]; i++)
            mq_memory_write(mach, mach->memory, cpu->r[4], 1, cpu->r[5]);
        cpu->r[0] = 0;
        return;

    case 0x1da3: /* Bfile_OpenFile() */
        cpu->r[0] = -1;
        return;
    case 0x1db4: /* Bfile_DeleteEntry() */
        cpu->r[0] = -1;
        return;
    case 0x1db6: /* Bfile_FindFirst_FAT() */
    case 0x1db7: /* Bfile_FindFirst() */
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
        mq_casiowin_initHeap(mach);
        mq_heap_free(cpu->r[4]);
        return;
    case 0x1f43:
    case 0x1f44: /* malloc() */
        mq_casiowin_initHeap(mach);
        cpu->r[0] = mq_heap_malloc(cpu->r[4]);
        return;
    case 0x1f45:
    case 0x1f46: /* realloc() */
        mq_casiowin_initHeap(mach);
        cpu->r[0] = mq_heap_realloc(cpu->r[4], cpu->r[5]);
        return;
    }

    mq_log(MQ_LOG_ERROR, "Unknown CG syscall %%%03x, getting stuck.",
        syscallID);
    mach->stuck = true;
}

bool mq_casiowin_initHeap(mqMachine *mach)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    if(!Casiowin)
        return false;

    u32 start = Casiowin->info->heapAddress;
    u32 size = Casiowin->info->heapSize;

    if(!start || !size || mq_heap_isInitialized(NULL, NULL))
        return false;

    void *buffer = mq_memory_allocBuffer(mach->memory, "HEAP", size);
    if(!buffer)
        return false;

    if(!mq_memory_createBlock(mach->memory, start, size, buffer))
        return false;

    return mq_heap_init(start, start + size, buffer);
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
