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

    /* Window settings. On real hardware, HSA/HEA start from the right. The
       values stored here ("reversed" HSA/HEA) are equal to:
       * rHSA = 395 - HEA
       * rHEA = 395 - HSA
       I/O operations based on HSA/HEA do the conversion on-the-fly. Internal
       logic uses rHSA/rHEA which correspond to the intuitive direction of the
       displayed image. */
    u16 rHSA, rHEA, VSA, VEA;
    /* Current position */
    u16 HADDR, VADDR;
};

typedef struct mqR61524 mqR61524;

/* Setup the R61524 module for a given machine. */
bool mq_r61524_setup(mqMachine *mach);

/* Get the R61524 module for a machine, NULL if there is none. */
mqR61524 *mq_r61524_get(mqMachine *mach);

/* Check if the module is in pixel writing mode, i.e. that writes to the main
   address right now would send pixels. Also checks that the display for the
   machine is of the expected parameters. If so, incoming accesses may be
   optimized into a call to mq_r61524_writePixels(). */
bool mq_r61524_isWritingPixels(mqMachine *mach);

/* Send pixels directly from a memory buffer. The source/size must be at least
   4-aligned (to make the endian-swapping easier). Needs access to the machine
   because the display surface is involved. Can only write a single frame! */
void mq_r61524_writePixels(mqMachine *mach, void *ptr, int size);

MQ_END_DEFS
#endif /* MQ_MODULES_R61524_H */
