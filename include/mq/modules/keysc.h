//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.keysc: Key Scan Interface
// Reference -- https://bible.planet-casio.com/yatis/hardware/sh7305/keysc.html
//
// This module is present on the SH7305 and has absolutely nothing to do with
// the SH7724 KEYSC. Because the mechanics of this module are largely obscure,
// MQ only emulates the high-level part that's been known and used for a long
// time, i.e. the KIUDATA* registers.
//---

#ifndef MQ_MODULES_KEYSC_H
#define MQ_MODULES_KEYSC_H

#include <mq/machine.h>
#include <mq/interfaces/keyboard.h>
MQ_START_DEFS

struct mqKEYSC {
    /* KIUDATA (key status) is not represented and is instead computed from the
       machine's keyboard interface at each access. */

    u16 KIUCNTREG;      /* Scan control */
    u16 KIAUTOFIXREG;   /* Automatic key bounce setting */
    u16 KIUMODEREG;     /* Scan mode setting */
    u16 KIUSTATEREG;    /* Scan state */
    u16 KIUINTREG;      /* Interrupt setting */
    u16 KIUWSETREG;     /* Scan wait time setting */
    u16 KIUINTERVALREG; /* Scan interval time setting */
    u16 KOUTPINSET;     /* KOUT line function setting */
    u16 KINPINSET;      /* KIN line function setting */
};

typedef struct mqKEYSC mqKEYSC;

/* Setup the KEYSC module for a given machine. */
bool mq_keysc_setup(mqMachine *mach);

/* Get the KEYSC module info of a machine, NULL if the module is not used. */
mqKEYSC *mq_keysc_get(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_MODULES_KEYSC_H */
