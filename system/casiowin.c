//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/system/casiowin.h>
#include <mq/system/heap.h>
#include <mq/system/bfile.h>
#include <mq/modules/mmu.h>
#include <mq/modules/rtc.h>
#include <mq/modules/intc.h>
#include <mq/modules/cpg.h>
#include <mq/cpu.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int moduleID = -1;
static int processID_bgsyscall = -1;

static void inithook(void)
{
    moduleID = mq_module_register();
    processID_bgsyscall = mq_process_register();
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
    .OSSeries               = MQ_CASIOWIN_SERIES_FX,
    .OSBaseAddress          = 0x80010000,
    .OSFooterAddress        = 0x8024ff18,
    .versionString          = "02.05.0000",
    .dateString             = "2015.0207.1555",
    .syscallStubAddress     = 0x80010070,
    .heapAddress            = 0x88030000, /* @ 192 kB */
    .heapSize               = 48 << 10,

    .addinAddress           = 0x80300000, /* @ 3 MB (in fs for OS 2.xx) */
    .addinSize              = 512 << 10,

    .uramAddress            = 0x88020000, /* @ 128ko */
    .uramSize               = 32 << 10,

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
    .OSSeries               = MQ_CASIOWIN_SERIES_CG,
    .OSBaseAddress          = 0x80020000,
    .OSFooterAddress        = 0x80b5ffe0,
    .versionString          = "03.80.0000",
    .dateString             = "2023.0419.1456",
    .syscallStubAddress     = 0x80020070,
    .heapAddress            = 0x8c0b0000, /* @ 704 kB */
    .heapSize               = 128 << 10,

    .systemStackAddress     = 0x8c0e0000,
    .systemStackSize        = 512 << 10,

    .addinAddress           = 0x81800000, /* @ 24 MB, somewhere in fs */
    .addinSize              = 2 << 20,

    .uramAddress            = 0x8c170000, /* @1.5 MB - 64 kB (contiguity) */
    .uramSize               = 512 << 10,

    .eramAddress            = 0x8c200000,
    .eramSize               = 2 << 20,

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
        mq_machine_setStuck(mach);
    }
}

static void setupDataArea(mqMachine *mach, mqCasiowin *Casiowin, void *buffer)
{
    mqCasiowin_OSInfo const *info = Casiowin->info;
    u32 data = info->dataAreaAddress;

    (void)buffer;

    /* VRAM
     * Note that we explicitly un-align the VRAM by 1 byte if we are
     * emulating an FX device. This is to fix weird display bugs with
     * MonochromLib which does not properly handle 4-aligned VRAM */
    if(Casiowin->info->OSSeries == MQ_CASIOWIN_SERIES_FX)
        data += 1;
    for(int i = 0; i < info->dataVramCount; i++) {
        Casiowin->dataVramAddresses[i] = data;
        data += info->dataVramSize;
    }
    if(info->dataVramCount) {
        Casiowin->vramLE = mq_memory_access(
            mach->memory,
            Casiowin->dataVramAddresses[0]
        );
    }
    if(Casiowin->info->OSSeries == MQ_CASIOWIN_SERIES_FX)
        data += 3;

    // TODO: VRAM backups

    if(data - info->dataAreaAddress > info->dataAreaSize) {
        mq_log(MQ_LOG_ERROR, "OS data area overflow!");
        mq_machine_setStuck(mach);
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
    Casiowin->bgs = calloc(1, sizeof *Casiowin->bgs);
    if(!Casiowin->bgs)
        return false;

    Casiowin->info = info;
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

    /* addin area */
    u32 addinSize = info->addinSize;
    u32 addinAddr = info->addinAddress;
    void *addin = mq_memory_allocBuffer(mach->memory, "ADDIN", addinSize);
    ok &= mq_memory_createBlock(mach->memory, addinAddr, addinSize, addin);

    /* User RAM area */
    u32 uramSize = info->uramSize;
    u32 uramP1 = info->uramAddress;
    u32 uramP2 = (uramP1 & 0x1fffffff) | 0xa0000000;
    void *uram = mq_memory_allocBuffer(mach->memory, "URAM", uramSize);
    ok &= mq_memory_createBlock(mach->memory, uramP1, uramSize, uram);
    ok &= mq_memory_createBlock(mach->memory, uramP2, uramSize, uram);

    /* system Stack, used only for other device than fx */
    if(info->systemStackAddress != 0x00000000) {
        u32 ostkSize = info->systemStackSize;
        u32 ostkP1 = info->systemStackAddress;
        u32 ostkP2 = (ostkP1 & 0x1fffffff) | 0xa0000000;
        void *ostk = mq_memory_allocBuffer(mach->memory, "OSTK", ostkSize);
        ok &= mq_memory_createBlock(mach->memory, ostkP1, ostkSize, ostk);
        ok &= mq_memory_createBlock(mach->memory, ostkP2, ostkSize, ostk);
    }

    if(ok)
        mach->modules[moduleID] = Casiowin;
    else
        free(Casiowin);

    return ok;
}

void mq_casiowin_initialize(mqMachine *mach)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    if(!Casiowin)
        return;

    mqCasiowin_OSInfo const *info = Casiowin->info;

    mach->cpu.CPUOPM = 0x00000320;

    /* Export some of the data to other components for optimization purposes */
    mach->cpu.syscallHandler = info->syscallStubAddress;

    /* Set the stack pointer to be P1 instead of MMU, as the OS does */
    mach->cpu.r[15] = info->uramAddress + info->uramSize;

    // TODO[casiowin]: Handle the NULL page with MMU so it shows up in TLB

    if(Casiowin->info->OSSeries == MQ_CASIOWIN_SERIES_FX) {
        mach->cpu.spRegs[SH_SR] = 0x40000000; // MD=1
        mach->cpu.r[4] = 0; // isAppli
        mach->cpu.r[5] = 0; // optNum
        mach->cpu.pc = 0x00300200;

        mq_mmu_map(mach, 0x00300000, info->addinAddress, 0, 0x10000, 8);
        mq_mmu_map(mach, 0x08100000, info->uramAddress, 55,  0x1000, 8);
        mq_mmu_bind(mach);

        /* CPG
         * - fixed Graph35+E configuration (OS 02.05) */
        mqCPG *CPG = mq_cpg_get(mach);
        CPG->FRQCR      = 0x0f212213;
        CPG->FSICLKCR   = 0x00000157;
        CPG->DDCLKCR    = 0x00000198;
        CPG->USBCLKCR   = 0x00000100;
        CPG->PLLCR      = 0x00005000;
        CPG->PLL2CR     = 0x00000000;
        CPG->SPUCLKCR   = 0x00000103;
        CPG->SSCGCR     = 0x00000000;
        CPG->FLLFRQ     = 0x00004384;
        CPG->LSTATUS    = 0x00000000;
        /* INTC
         * - load initial OS state */
        mqINTC *INTC = mq_intc_get(mach);
        INTC->IPR[0]    = 0x0800;
        INTC->IPR[1]    = 0xc000;
        INTC->IPR[5]    = 0xd000;
        INTC->IPR[10]   = 0x8d00;
        INTC->IMR[0]    = 0x07;
        INTC->IMR[1]    = 0x0f;
        INTC->IMR[2]    = 0x07;
        INTC->IMR[3]    = 0xfc;
        INTC->IMR[4]    = 0x70;
        INTC->IMR[5]    = 0x77;
        INTC->IMR[6]    = 0x1b;
        INTC->IMR[7]    = 0xff;
        INTC->IMR[8]    = 0x07;
        INTC->IMR[9]    = 0x12;
        INTC->IMR[10]   = 0x14;
        INTC->IMR[11]   = 0x01;
        INTC->IMR[12]   = 0x38;
        //todo: dump KEYSC config?
        //todo: dump DMA config?
        //todo: dump Cmod config?
        //todo: dump TMU config?
        /* RTC
         * - Keep PES_period initialized to a non-zero value
         * - copy default date information (exact same info than %11e1) */
        mqRTC *RTC = mq_rtc_get(mach);
        RTC->RCR2       = 0x09;
        RTC->RWKCNT     = 0x00;
        RTC->RDAYCNT    = 0x01;
        RTC->RMONCNT    = 0x11;
        RTC->RYRCNT     = 0x2010;
    }
    else if(Casiowin->info->OSSeries == MQ_CASIOWIN_SERIES_CG) {
        mach->cpu.spRegs[SH_SR] = 0x40000000; // MD=1
        mach->cpu.r[4] = 0; // isAppli
        mach->cpu.r[5] = 0; // optNum
        mach->cpu.pc = 0x00300000;

        mq_mmu_map(mach, 0x00300000, info->addinAddress, 0, 0x100000, 2);
        mq_mmu_map(mach, 0x08100000, info->uramAddress, 55,  0x10000, 8);
        mq_mmu_bind(mach);

        /* CPG
         * - fixed fx-CG 50 configuration (from OS 3.80) */
        mqCPG *CPG = mq_cpg_get(mach);
        CPG->FRQCR      = 0x0f011112;
        CPG->FSICLKCR   = 0x00000057;
        CPG->DDCLKCR    = 0x00000198;
        CPG->USBCLKCR   = 0x00000100;
        CPG->PLLCR      = 0x00005000;
        CPG->PLL2CR     = 0x00000000;
        CPG->SPUCLKCR   = 0x00000003;
        CPG->SSCGCR     = 0x10000000;
        CPG->FLLFRQ     = 0x00004384;
        CPG->LSTATUS    = 0x00000000;
        /* INTC
         * - load initial OS state */
        mqINTC *INTC = mq_intc_get(mach);
        INTC->IPR[0]    = 0x0800;
        INTC->IPR[1]    = 0xc000;
        INTC->IPR[5]    = 0xd000;
        INTC->IPR[10]   = 0x8000;
        INTC->IMR[0]    = 0x07;
        INTC->IMR[1]    = 0x0f;
        INTC->IMR[2]    = 0x07;
        INTC->IMR[3]    = 0xfc;
        INTC->IMR[4]    = 0x00;
        INTC->IMR[5]    = 0x77;
        INTC->IMR[6]    = 0x1b;
        INTC->IMR[7]    = 0xff;
        INTC->IMR[8]    = 0x07;
        INTC->IMR[9]    = 0x12;
        INTC->IMR[10]   = 0x34;
        INTC->IMR[11]   = 0x01;
        INTC->IMR[12]   = 0x38;
        /* RTC
         * - keep PES_period initialized to a non-zero value
         * - use syscall %11e1 `RTC_GetDateDefault()` */
        mqRTC *RTC = mq_rtc_get(mach);
        RTC->RCR2       = 0x09;
        RTC->RWKCNT     = 0x00;
        RTC->RDAYCNT    = 0x01;
        RTC->RMONCNT    = 0x11;
        RTC->RYRCNT     = 0x2010;
    }
}

static void mq_casiowin_cleanup(mqMachine *mach)
{
    mqCasiowin *Casiowin = mach->modules[moduleID];
    if(Casiowin) {
        free(Casiowin->bgs);
        free(Casiowin);
    }

    // TODO[casiowin]: mq_heap_reset: Should be bound to machine, not global!
    mq_heap_reset();
}
MQ_HOOK_REGISTER(module_cleanup, mq_casiowin_cleanup)

static void mq_casiowin_createObserver(mqMachine *omach, mqMachine const *mach)
{
    mqCasiowin const *Casiowin = mach->modules[moduleID];
    mqCasiowin *oCasiowin = memdup(Casiowin, sizeof *Casiowin);
    omach->modules[moduleID] = oCasiowin;
    if(!oCasiowin)
        return;

    /* Also duplicate the background syscall memory */
    oCasiowin->bgs = memdup(Casiowin->bgs, sizeof *Casiowin->bgs);
}
MQ_HOOK_REGISTER(module_createObserver, mq_casiowin_createObserver)

static void mq_casiowin_destroyObserver(mqMachine *omach)
{
    mqCasiowin *oCasiowin = omach->modules[moduleID];
    if(oCasiowin) {
        free(oCasiowin->bgs);
        free(oCasiowin);
    }
}
MQ_HOOK_REGISTER(module_destroyObserver, mq_casiowin_destroyObserver)

static bool readStack32(mqMachine *mach, int offset, void *ptr)
{
    mqCpu *cpu = &mach->cpu;
    return mq_memory_read32(mach, mach->memory, cpu->r[15] + offset, ptr);
}

static bool readTShape(
    mqMachine *mach, u32 address, struct mqCasiowin_TShape *shape)
{
    mqMemory *mem = mach->memory;
    u32 const_2, type, f2, f3;

    if(!mq_memory_read32(mach, mem, address +  0, &shape->x1) ||
       !mq_memory_read32(mach, mem, address +  4, &shape->y1) ||
       !mq_memory_read32(mach, mem, address +  8, &shape->x2) ||
       !mq_memory_read32(mach, mem, address + 12, &shape->y2) ||
       !mq_memory_read8 (mach, mem, address + 16, &const_2) ||
       !mq_memory_read8 (mach, mem, address + 17, &type) ||
       !mq_memory_read8 (mach, mem, address + 18, &f2) ||
       !mq_memory_read8 (mach, mem, address + 19, &f3) ||
       !mq_memory_read32(mach, mem, address + 20, (u32 *)&shape->on_bits) ||
       !mq_memory_read32(mach, mem, address + 24, (u32 *)&shape->off_bits))
       return false;

    shape->const_2 = const_2;
    shape->type = type;
    shape->f2 = f2;
    shape->f3 = f3;
    return true;
}

static bool readTShapePixelInfo(
    mqMachine *mach, u32 address, struct mqCasiowin_TShapePixelInfo *info)
{
    mqMemory *mem = mach->memory;
    u32 mode;

    if(!mq_memory_read8 (mach, mem, address +  0, &mode) ||
       !mq_memory_read32(mach, mem, address +  4, (u32 *)&info->x) ||
       !mq_memory_read32(mach, mem, address +  8, (u32 *)&info->y) ||
       !mq_memory_read32(mach, mem, address + 12, (u32 *)&info->dash_counter))
        return false;

    info->mode = mode;
    return true;
}

static bool writeTShapePixelInfo(
    mqMachine *mach, u32 address, struct mqCasiowin_TShapePixelInfo *info)
{
    mqMemory *mem = mach->memory;
    return mq_memory_write(mach, mem, address +  0, 1, info->mode) &&
           mq_memory_write(mach, mem, address +  4, 4, info->x) &&
           mq_memory_write(mach, mem, address +  8, 4, info->y) &&
           mq_memory_write(mach, mem, address + 12, 4, info->dash_counter);
}

static void syscall_fx(mqMachine *mach, mqCpu *cpu, u32 syscallID)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    u32 r4 = cpu->r[4], r5 = cpu->r[5], r6 = cpu->r[6], r7 = cpu->r[7];

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

    case 0x028: /* Bdisp_PutDisp_DD() */
        mq_casiowin_mono_dupdate(mach);
        return;

    case 0x02b: /* Bdisp_DrawShape() */
    case 0x02c: /* Bdisp_DrawShapeLine() */
    case 0x02d: /* Bdisp_DrawShapeRect() */
    case 0x02e: /* Bdisp_DrawShapeCircle() */
    case 0x129: { /* Bdisp_DrawShapePoint() */
        struct mqCasiowin_TShapePixelInfo pixelinfo;
        struct mqCasiowin_TShape shape;
        if(!readTShapePixelInfo(mach, r4, &pixelinfo) ||
           !readTShape(mach, r5, &shape))
            return;

        if(syscallID == 0x02b)
            mq_casiowin_DrawShape(mach, &pixelinfo, &shape);
        if(syscallID == 0x02c)
            mq_casiowin_DrawShapeLine(mach, &pixelinfo, &shape);
        if(syscallID == 0x02d)
            mq_casiowin_DrawShapeRect(mach, &pixelinfo, &shape);
        if(syscallID == 0x02e)
            mq_casiowin_DrawShapeCircle(mach, &pixelinfo, &shape);
        if(syscallID == 0x129)
            mq_casiowin_DrawShapePoint(mach, &pixelinfo, &shape);

        writeTShapePixelInfo(mach, r4, &pixelinfo);
        return;
    }

    case 0x002f: { /* Bdisp_ShapeToVRAM() */
        struct mqCasiowin_TShape shape;
        if(readTShape(mach, r4, &shape))
            mq_casiowin_ShapeToVRAM(mach, &shape);
        return;
    }

    case 0x0030: /* Bdisp_DrawLineVRAM() */
        return mq_casiowin_LineToVRAM(mach, r4, r5, r6, r7, 1);
    case 0x0031: /* Bdisp_ClearLineVRAM() */
        return mq_casiowin_LineToVRAM(mach, r4, r5, r6, r7, 2);

    case 0x0032: { /* Bdisp_ShapeToDD() */
        struct mqCasiowin_TShape shape;
        if(readTShape(mach, r4, &shape))
            mq_casiowin_ShapeToDD(mach, &shape);
        return;
    }
    case 0x0033: { /* Bdisp_ShapeToDDVRAM() */
        struct mqCasiowin_TShape shape;
        if(readTShape(mach, r4, &shape))
            mq_casiowin_ShapeToDDVRAM(mach, &shape);
        return;
    }

    case 0x0036: {
        int mode;
        if(readStack32(mach, +0, &mode))
            mq_casiowin_LineToVRAM(mach, r4, r5, r6, r7, mode);
        return;
    }

    case 0x0039: /* RTC_Reset() */
        if(!mq_casiowin_rtc_reset(mach, cpu->r[4]))
            mq_log(MQ_LOG_ERROR, "RTC_Reset(): internal error");
        return;
    case 0x003a: /* RTC_GetTime() */
        if(!mq_casiowin_rtc_gettime(mach,
                    cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7]))
            mq_log(MQ_LOG_ERROR, "RTC_GetTime(): internal error");
        return;
    case 0x003b: /* RTC_GetTicks() */
        if(!mq_casiowin_rtc_getticks(mach, &cpu->r[0]))
            mq_log(MQ_LOG_ERROR, "RTC_GetTicks(): internal error");
        return;

    case 0x0135: /* GetVRAMAddress() */
        cpu->r[0] = Casiowin->dataVramAddresses[0];
        return;

    case 0x0138: /* Cursor_SetPosition() */
        cpu->r[0] = mq_casiowin_Cursor_SetPosition(mach, r4, r5);
        return;

    case 0x0143: /* Bdisp_AllClr_VRAM() */
    case 0x0144: /* Bdisp_AllClr_DDVRAM() */
        /* TODO: mq_memory_memset() */
        for(unsigned int i = 0 ; i < Casiowin->info->dataVramSize ; i++)
            *(u8*)(((uintptr_t)Casiowin->vramLE + i) ^ 3) = 0x00;

        if(syscallID == 0x0144)
            mq_casiowin_mono_dupdate(mach);
        return;

    case 0x0146: /* Bdisp_SetPoint_VRAM() */
        mq_casiowin_mono_set_pixel((u8*)Casiowin->vramLE,
            cpu->r[4], cpu->r[5], cpu->r[6]);
        return;

    // case 0x014d: /* Bdisp_AreaReverseVRAM() */
    //     return;

    case 0x0247: { /* Keyboard_GetKeyWait() */
        struct mqCasiowin_GetKeyWaitArgs args = {
            .ptr_i32_col = cpu->r[4],
            .ptr_i32_row = cpu->r[5],
            .waitType = cpu->r[6],
            .timeout = cpu->r[7],
        };
        if(readStack32(mach, +0, &args.menu) &&
           readStack32(mach, +4, &args.ptr_u16_key))
            mq_casiowin_GetKeyWait(mach, args, false);
        else
            mq_machine_setStuck(mach);
        return;
    }

    case 0x024c: /* Keyboard_IsSpecialKeyDown */
        mq_log(
            MQ_LOG_ERROR,
            "unsupported syscall %%24c Keyboard_IsSpecialKeyDown()"
        );
        mq_machine_setStuck(mach);
        return;

    case 0x03ed: /* Interrupt_SetOrClrStatusFlagsy() */
        mq_log(
            MQ_LOG_ERROR,
            "unsupported syscall %%3ed Interrupt_SetOrClrStatusFlags()"
        );
        mq_machine_setStuck(mach);
        return;

    case 0x03fa: /* Hmem_SetMMU() */
        cpu->r[0] = 0;
        return;

    case 0x0420: /* OS_InnerSleep_ms() */
        mq_machine_internalPauseMilliseconds(mach, cpu->r[4]);
        return;

    case 0x042c: /* Bfile_OpenFile() */
        cpu->r[0] = mq_bfile_OpenFile(mach, cpu->r[4], cpu->r[5]);
        return;
    case 0x042d: /* Bfile_CloseFile() */
        cpu->r[0] = mq_bfile_CloseFile(mach, cpu->r[4]);
        return;
    case 0x042f: /* Bfile_GetFileSize_OS() */
        cpu->r[0] = mq_bfile_GetFileSize(mach, cpu->r[4]);
        return;
    case 0x0431: /* Bfile_SeekFile_OS() */
        cpu->r[0] = mq_bfile_SeekFile(mach, cpu->r[4], cpu->r[5]);
        return;
    case 0x0432: /* Bfile_ReadFile() */
        cpu->r[0] = mq_bfile_ReadFile(mach,
                cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7]);
        return;
    case 0x0434: /* Bfile_CreateEntry() */
        cpu->r[0] = mq_bfile_CreateEntry(mach,
                cpu->r[4], cpu->r[5], cpu->r[6]);
        return;
    case 0x0435: /* Bfile_WriteFile() */
        cpu->r[0] = mq_bfile_WriteFile(mach,
                cpu->r[4], cpu->r[5], cpu->r[6]);
        return;
    case 0x0438: /* Bfile_RenameEntry() */
        cpu->r[0] = mq_bfile_RenameEntry(mach,
                cpu->r[4], cpu->r[5]);
        return;
    case 0x0439: /* Bfile_DeleteEntry() */
        cpu->r[0] = mq_bfile_DeleteEntry(mach, cpu->r[4]);
        return;
    case 0x043b: /* Bfile_FindFirst */
        cpu->r[0] = mq_bfile_FindFirst(mach,
                cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7]);
        return;
    case 0x043c: /* Bfile_FindNext() */
        cpu->r[0] = mq_bfile_FindNext(mach,
                cpu->r[4], cpu->r[5], cpu->r[6]);
        return;
    case 0x043d: /* Bfile_FindClose() */
        cpu->r[0] = mq_bfile_FindClose(mach, cpu->r[4]);
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

    case 0x090f: { /* GetKey() */
        mq_casiowin_mono_dupdate(mach);
        mq_casiowin_GetKey(mach, r4);
        return;
    }

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

    case 0x0e6b: { /* calloc() */
        mq_casiowin_initHeap(mach);
        cpu->r[0] = mq_heap_malloc(cpu->r[4]);
        if (cpu->r[0] != 0x00000000) {
            for(u32 i = 0; i < cpu->r[4]; i++)
                mq_memory_write(mach, mach->memory, cpu->r[0], 1, 0x00);
        }
        return;
    }

    case 0x1032: /* Get keymap for keycode/matrix code conv. (since 1.05) */
        cpu->r[0] = Casiowin->rodataKeymapAddress;
        return;
    }

    mq_log(MQ_LOG_ERROR, "Unknown FX syscall %%%03x, getting stuck.",
        syscallID);
    mq_machine_setStuck(mach);
}

static void syscall_cg(mqMachine *mach, mqCpu *cpu, u32 syscallID)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    // TODO: Check syscall API version

    switch(syscallID) {
    case 0x0029: /* ??? - Glib_AddInAplExecutionCheck something like that. */
        mq_log(MQ_LOG_WARNING, "Ignoring %%029, what is that?");
        /* Just return 0. */
        cpu->r[0] = 0;
        return;

    case 0x012b: /* FKey_Mapping1() */
        mq_log(MQ_LOG_ERROR, "syscall FKey_Mapping1() not supported");
        mq_machine_setStuck(mach);
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
            mach->newFrame.blocked = true;
        }
        return;

    case 0x0272: { /* Bdisp_AllClr_VRAM() */
        u16 *dst = mq_memory_access(mach->memory, 0x8c000000);
        /* Since this is aligned, we can memset it */
        memset(dst, 0xff, 384 * 216 * 2);
        return;
    }

    case 0x02a3: /* FrameColor() */
        mq_log(MQ_LOG_ERROR, "syscall %%2a3 FrameColor() ignored");
        cpu->r[0] = 0;
        return;
    case 0x02a8: /* DrawFrame() */
        mq_log(MQ_LOG_ERROR, "syscall %%2a8 DrawFrame() not supported");
        mq_machine_setStuck(mach);
        return;

    case 0x02b7: /* EnableStatusArea() */
        mq_log(MQ_LOG_ERROR, "syscall %%2b7 EnableStatusArea() ignored");
        cpu->r[0] = 0;
        return;
    case 0x02b8: /* DefineStatusAreaFlags() */
        mq_log(MQ_LOG_ERROR, "syscall %%2b8 DefineStatusAreaFlags() ignored");
        cpu->r[0] = 0;
        return;

    case 0x02bf: /* RTC_Reset() */
        if(!mq_casiowin_rtc_reset(mach, cpu->r[4]))
            mq_log(MQ_LOG_ERROR, "RTC_GetReset(): internal error");
        return;
    case 0x02c0: /* RTC_GetTime() */
        if(!mq_casiowin_rtc_gettime(mach,
                    cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7]))
            mq_log(MQ_LOG_ERROR, "RTC_GetTime(): internal error");
        return;
    case 0x02c1: /* RTC_GetTicks() */
        if(!mq_casiowin_rtc_getticks(mach, &cpu->r[0]))
            mq_log(MQ_LOG_ERROR, "RTC_GetTicks(): internal error");
        return;

    case 0x0921: /* EnableColors() */
        mq_log(MQ_LOG_ERROR, "syscall %%921 EnableColor() ignored");
        cpu->r[0] = 0;
        return;

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

    case 0x12bf: { /* GetKeyWait_OS() */
        struct mqCasiowin_GetKeyWaitArgs args = {
            .ptr_i32_col = cpu->r[4],
            .ptr_i32_row = cpu->r[5],
            .waitType = cpu->r[6],
            .timeout = cpu->r[7],
        };
        if(readStack32(mach, +0, &args.menu) &&
           readStack32(mach, +4, &args.ptr_u16_key))
            mq_casiowin_GetKeyWait(mach, args, false);
        else
            mq_machine_setStuck(mach);
        return;
    }

    case 0x1511: /* memset() */
        /* We can't memset if it's not aligned, because the endianness makes
           the storage non-contiguous! */
        for(u32 i = 0; i < cpu->r[6]; i++)
            mq_memory_write(mach, mach->memory, cpu->r[4], 1, cpu->r[5]);
        cpu->r[0] = 0;
        return;

    case 0x1562: /* MCSGetDLen2() */
        cpu->r[0] = 0x40; // does not exist
        return;

    case 0x18f9: /* PrintXY() */
        mq_log(MQ_LOG_ERROR, "syscall %%18f9 PrintXY() ignored");
        cpu->r[0] = 0;
        return;

    case 0x1d77: /* DefineStatusMessage() */
        mq_log(MQ_LOG_ERROR, "syscall %%1d77 DefineStatusMessage() ignored");
        cpu->r[0] = 0;
        return;
    case 0x1d81: /* DisplayStatusArea() */
        mq_log(MQ_LOG_ERROR, "syscall %%1d81 DisplayStatusArea() ignored");
        cpu->r[0] = 0;
        return;

    case 0x1da3: /* Bfile_OpenFile() */
        cpu->r[0] = mq_bfile_OpenFile(mach, cpu->r[4], cpu->r[5]);
        return;
    case 0x1da4: /* Bfile_CloseFile() */
        cpu->r[0] = mq_bfile_CloseFile(mach, cpu->r[4]);
        return;
    case 0x1da6: /* Bfile_GetFileSize_OS() */
        cpu->r[0] = mq_bfile_GetFileSize(mach, cpu->r[4]);
        return;
    case 0x1da7: /* Bfile_GetFileInfo() */
        cpu->r[0] = mq_bfile_GetFileInfo(mach, cpu->r[4], cpu->r[5]);
        return;
    case 0x1da9: /* Bfile_SeekFile_OS() */
        cpu->r[0] = mq_bfile_SeekFile(mach, cpu->r[4], cpu->r[5]);
        return;
    case 0x1dab: /* Bfile_FilePos() */
        cpu->r[0] = mq_bfile_Filepos(mach, cpu->r[4]);
        return;
    case 0x1dac: /* Bfile_ReadFile_OS() */
        cpu->r[0] = mq_bfile_ReadFile(mach,
                cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7]);
        return;
    case 0x01dae: /* Bfile_CreateEntry() */
        cpu->r[0] = mq_bfile_CreateEntry(mach,
                cpu->r[4], cpu->r[5], cpu->r[6]);
        return;
    case 0x1daf: /* Bfile_WriteFile_OS() */
        cpu->r[0] = mq_bfile_WriteFile(mach,
                cpu->r[4], cpu->r[5], cpu->r[6]);
        return;
    case 0x1db3: /* Bfile_RenameEntry() */
        cpu->r[0] = mq_bfile_RenameEntry(mach,
                cpu->r[4], cpu->r[5]);
        return;
    case 0x1db4: /* Bfile_DeleteEntry() */
        cpu->r[0] = mq_bfile_DeleteEntry(mach, cpu->r[4]);
        return;
    case 0x1db6: /* Bfile_FindFirst_FAT() */
    case 0x1db7: /* Bfile_FindFirst() */
        cpu->r[0] = mq_bfile_FindFirst(mach,
                cpu->r[4], cpu->r[5], cpu->r[6], cpu->r[7]);
        return;
    case 0x1db8: /* Bfile_FindNext() */
        cpu->r[0] = mq_bfile_FindNext(mach,
                cpu->r[4], cpu->r[5], cpu->r[6]);
        return;
    case 0x1dba: /* Bfile_FindClose() */
        cpu->r[0] = mq_bfile_FindClose(mach, cpu->r[4]);
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
    mq_machine_setStuck(mach);
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

static void mq_casiowin_process_bgsyscall(mqMachine *mach, int cyclesElapsed)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    (void)cyclesElapsed;

    bool done = Casiowin->bgsyscall(mach);
    if(done) {
        Casiowin->bgsyscall = NULL;
        mach->processes[processID_bgsyscall] = NULL;
        mach->internallyBlocked = false;
    }
}

bool mq_casiowin_runBackgroundSyscall(
    mqMachine *mach, mq_casiowin_bgsyscall_t *bgsyscall)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    if(!Casiowin || mach->processes[processID_bgsyscall])
        return false;

    /* Try to run it once before setting up the background process */
    if(bgsyscall(mach))
        return true;

    Casiowin->bgsyscall = bgsyscall;
    mach->processes[processID_bgsyscall] = mq_casiowin_process_bgsyscall;
    mach->internallyBlocked = true;
    return true;
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

    if(Casiowin->bgsyscall)
        mq_machine_breakExecution(mach);
}
