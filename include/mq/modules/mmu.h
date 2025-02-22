//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.mmu: Memory Management Interface
// Reference -- SH4AL-DSP Manual, Section 7
//   https://bible.planet-casio.com/common/hardware/mpu/sh4aldsp_manual.pdf
//---

#ifndef MQ_MODULES_MMU_H
#define MQ_MODULES_MMU_H

#include <mq/machine.h>
#include <mq/interfaces/keyboard.h>
MQ_START_DEFS

/* Setup the MMU module for a given machine. */
bool mq_module_mmu_setup(mqMachine *mach);

struct mqMMU {
   u32 PTEH, PTEL;
   u32 TTB;
   u32 TEA;
   u32 PASCR;
   u32 MMUCR;
   u32 IRMCR;
};

typedef struct mqMMU mqMMU;

MQ_END_DEFS
#endif /* MQ_MODULES_MMU_H */
