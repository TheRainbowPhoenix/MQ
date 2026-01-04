//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.t6k11: T6K11 Display Controller
// Reference -- The original manual
//   https://bible.planet-casio.com/common/hardware/lcd/T6K11.pdf
//---

#ifndef MQ_MODULES_T6K11_H
#define MQ_MODULES_T6K11_H

#include <mq/machine.h>
MQ_START_DEFS

enum mqT6K11Variant {
    T6K11_VARIANT_NONE,     /* used at startup to detect the variant */
    T6K11_VARIANT_T6K11,    /* historical "old" screen driver */
    T6K11_VARIANT_ML9801,   /* "recent" screen driver (e.g Graph35+EII) */
};

struct mqT6K11 {
    /* Currently-selected register */
    u8 REG;
    /* Current position (TODO: col in bytes?) */
    u16 row, col;
    /* Variant detected (T6K11 or ML9801) */
    enum mqT6K11Variant variant;
};

typedef struct mqT6K11 mqT6K11;

/* Setup the T6K11 module for a given machine. */
bool mq_t6k11_setup(mqMachine *mach);

/* Get the T6K11 module for a machine, NULL if there is none. */
mqT6K11 *mq_t6k11_get(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_MODULES_T6K11_H */ 
