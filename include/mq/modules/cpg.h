//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.cpg: Clock Pulse Generator
// Reference -- SH7724 manual, Section 17
//   https://bible.planet-casio.com/common/hardware/mpu/sh7724.pdf
//---

#ifndef MQ_MODULES_CPG_H
#define MQ_MODULES_CPG_H

#include <mq/machine.h>
MQ_START_DEFS

struct mqCPG {
    u32 FRQCR;
    u32 FSICLKCR;
    u32 SPUCLKCR;
    u32 DDCLKCR;
    u32 USBCLKCR;
    u32 PLLCR;
    u32 PLL2CR;
    u32 FLLFRQ;
    u32 LSTATUS;
    u32 SSCGCR;
};
typedef struct mqCPG mqCPG;

/* Setup the CPG module for a given machine. */
bool mq_cpg_setup(mqMachine *mach, int initializeKind);

/* Get the CPG module for a machine, NULL if there is none. */
mqCPG *mq_cpg_get(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_MODULES_CPG_H */
