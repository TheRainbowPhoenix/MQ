//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.r61524: R61524 Display Controller
// Reference -- Close to the R61509
//   https://bible.planet-casio.com/common/hardware/lcd/R61509.pdf
//---

#ifndef MQ_MODULES_R61524_H
#define MQ_MODULES_R61524_H

#include <mq/machine.h>
MQ_START_DEFS

struct mqR61524 {
    // TODO[r61524]: PRDR should not be managed by R61524 driver
    u8 PRDR;

    /* Currently-selected register */
    u16 selectedRegister;

    /* Window settings. Note: HSA/HEA starts from the right! */
    u16 HSA, HEA, VSA, VEA;
    /* Current position */
    u16 HADDR, VADDR;
};

typedef struct mqR61524 mqR61524;

/* Setup the R61524 module for a given machine. */
bool mq_r61524_setup(mqMachine *mach);

/* Get the R61524 module for a machine, NULL if there is none. */
mqR61524 *mq_r61524_get(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_MODULES_R61524_H */
