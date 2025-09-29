//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
#include <mq/modules/cpg.h>
#include <mq/hooks.h>
#include <mq/memory.h>
#include <mq/mq.h>
#include <stdlib.h>
#include <stdio.h>

static int moduleID = -1;

static void inithook(void)
{
    moduleID = mq_module_register();
}
MQ_HOOK_REGISTER(init, inithook)

static void write_FRQCR(struct mqCPG *CPG, u32 value)
{
    CPG->FRQCR = value & 0xbff0ff0f;
    /* Bits 16-19 are fixed by hardware at their initial value */
    CPG->FRQCR |= 0x1 << 16;
    /* Bits 4-7 read the same as SFC (12-15) */
    CPG->FRQCR |= ((CPG->FRQCR >> 12) & 0xf) << 4;
}

static void write_FSICLKCR(struct mqCPG *CPG, u32 value)
{
    CPG->FSICLKCR = value & 0x0000fdff;
}

static void write_DDCLKCR(struct mqCPG *CPG, u32 value)
{
    CPG->DDCLKCR = value & 0x000001bf;
}

static void write_USBCLKCR(struct mqCPG *CPG, u32 value)
{
    CPG->USBCLKCR = value & 0x00000100;
}

static void write_PLLCR(struct mqCPG *CPG, u32 value)
{
    CPG->PLLCR = value & 0x00005002;
}

static void write_PLL2CR(struct mqCPG *CPG, u32 value)
{
    CPG->PLL2CR = value;
}

static void write_SPUCLKCR(struct mqCPG *CPG, u32 value)
{
    CPG->SPUCLKCR = value & 0x000001bf;
}

static void write_SSCGCR(struct mqCPG *CPG, u32 value)
{
    CPG->SSCGCR = value & 0x80000000;
}

static void write_FLLFRQ(struct mqCPG *CPG, u32 value)
{
    CPG->FLLFRQ = value & 0x0000c7ff;
}

static void write_LSTATUS(struct mqCPG *CPG, u32 value)
{
    CPG->LSTATUS = value;
}

bool mq_cpg_setup(mqMachine *mach)
{
    mqPage *pg = mq_memory_getPagePrealloc(mach->memory, 0xa4150000, 0x64, 10);
    if(!pg)
        return false;

    mqCPG *CPG = calloc(1, sizeof *CPG);
    if(!CPG)
        return false;

    bool ok = true;
    ok &= mq_page_mapRegister32(pg, "FRQCR",    0xa4150000,
        NULL, write_FRQCR, &CPG->FRQCR, CPG);
    ok &= mq_page_mapRegister32(pg, "FSICLKCR", 0xa4150008,
        NULL, write_FSICLKCR, &CPG->FSICLKCR, CPG);
    ok &= mq_page_mapRegister32(pg, "DDCLKCR",  0xa4150010,
        NULL, write_DDCLKCR, &CPG->DDCLKCR, CPG);
    ok &= mq_page_mapRegister32(pg, "USBCLKCR", 0xa4150014,
        NULL, write_USBCLKCR, &CPG->USBCLKCR, CPG);
    ok &= mq_page_mapRegister32(pg, "PLLCR",    0xa4150024,
        NULL, write_PLLCR, &CPG->PLLCR, CPG);
    ok &= mq_page_mapRegister32(pg, "PLL2CR",   0xa4150028,
        NULL, write_PLL2CR, &CPG->PLL2CR, CPG);
    ok &= mq_page_mapRegister32(pg, "SPUCLKCR", 0xa415003c,
        NULL, write_SPUCLKCR, &CPG->SPUCLKCR, CPG);
    ok &= mq_page_mapRegister32(pg, "SSCGCR",   0xa4150044,
        NULL, write_SSCGCR, &CPG->SSCGCR, CPG);
    ok &= mq_page_mapRegister32(pg, "FLLFRQ",   0xa4150050,
        NULL, write_FLLFRQ, &CPG->FLLFRQ, CPG);
    ok &= mq_page_mapRegister32(pg, "LSTATUS",  0xa4150060,
        NULL, write_LSTATUS, &CPG->LSTATUS, CPG);

    if(ok)
        mach->modules[moduleID] = CPG;
    else
        free(CPG);
    return ok;
}

mqCPG *mq_cpg_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static void mq_cpg_cleanup(mqMachine *mach)
{
    mqCPG *CPG = mach->modules[moduleID];
    if(CPG)
        free(CPG);
}
MQ_HOOK_REGISTER(module_cleanup, mq_cpg_cleanup)

static void mq_cpg_createObserver(mqMachine *omach, mqMachine const *mach)
{
    omach->modules[moduleID] = memdup(mach->modules[moduleID], sizeof(mqCPG));
}
MQ_HOOK_REGISTER(module_createObserver, mq_cpg_createObserver)

static void mq_cpg_destroyObserver(mqMachine *omach)
{
    mqCPG *CPG = omach->modules[moduleID];
    if(CPG)
        free(CPG);
}
MQ_HOOK_REGISTER(module_destroyObserver, mq_cpg_destroyObserver)
