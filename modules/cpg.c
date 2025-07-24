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

#define mapReg32(addr, name)        \
    ok &= mq_page_mapRegister32(    \
        pg, #name, addr,            \
        NULL, (void*)write_##name,  \
        &CPG->name, CPG             \
    )

#define writeReg32(name, mask)                          \
static void write_##name(struct mqCPG *CPG, u32 value)  \
{                                                       \
    CPG->name = value & mask;                           \
}

//===========================================================================//

static int moduleID = -1;

static void inithook(void)
{
    moduleID = mq_module_register();
}
MQ_HOOK_REGISTER(init, inithook)

static void mq_cpg_cleanup(mqMachine *mach)
{
    mqCPG *CPG = mach->modules[moduleID];
    if(CPG)
        free(CPG);
}
MQ_HOOK_REGISTER(module_cleanup, mq_cpg_cleanup)

//===========================================================================//

static void write_FRQCR(struct mqCPG *CPG, u32 value)
{
    CPG->FRQCR = value & 0b10111111111100001111111100001111;
    CPG->FRQCR = CPG->FRQCR | 0x00010010;
}
writeReg32(FSICLKCR, 0b00000000000000001111110111111111)
writeReg32(DDCLKCR,  0b00000000000000000000000110111111)
writeReg32(USBCLKCR, 0b00000000000000000000000100000000)
writeReg32(PLLCR,    0b00000000000000000101000000000010)
writeReg32(PLL2CR,   0b11111111111111111111111111111111)
writeReg32(SPUCLKCR, 0b00000000000000000000000110111111)
writeReg32(SSCGCR,   0b10000000000000000000000000000000)
writeReg32(FLLFRQ,   0b00000000000000001100011111111111)
writeReg32(LSTATUS,  0b11111111111111111111111111111111)

//===========================================================================//

bool mq_cpg_setup(mqMachine *mach)
{
    mqPage *pg = mq_memory_getPagePrealloc(mach->memory, 0xa4150000, 0x64, 10);
    if(!pg)
        return false;

    mqCPG *CPG = calloc(1, sizeof *CPG);
    if(!CPG)
        return false;

    // use fxcg50 (03.08.2212) value
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

    bool ok = true;
    mapReg32(0xa4150000, FRQCR);
    mapReg32(0xa4150008, FSICLKCR);
    mapReg32(0xa4150010, DDCLKCR);
    mapReg32(0xa4150014, USBCLKCR);
    mapReg32(0xa4150024, PLLCR);
    mapReg32(0xa4150028, PLL2CR);
    mapReg32(0xa415003c, SPUCLKCR);
    mapReg32(0xa4150044, SSCGCR);
    mapReg32(0xa4150050, FLLFRQ);
    mapReg32(0xa4150060, LSTATUS);
    return ok;
}
