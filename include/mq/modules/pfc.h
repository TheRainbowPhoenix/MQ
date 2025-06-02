//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.pfc: Pin Function Controller for Keyboard PCB
// Reference -- 
//   https://bible.planet-casio.com/simlo/chm/v20/fx_legacy_schematics.htm
//---

#ifndef MQ_MODULES_PFC_H
#define MQ_MODULES_PFC_H

#include <mq/machine.h>
MQ_START_DEFS

#define KB_PORTB_CTRL        0xA4000102
#define KB_PORTM_CTRL        0xA4000118
#define KB_PORTA             0xA4000120
// #define KB_PORTB             0xA4000122
#define KB_PORTM             0xA4000138

struct mqPFC {
    u8 PORTA;
    u8 PORTM;
    u16 PORTB_CTRL;
    u16 PORTM_CTRL;
};

typedef struct mqPFC mqPFC;

/* Setup the PFC module for a given machine. */
bool mq_pfc_setup(mqMachine *mach);

/* Get the PFC module for a machine, NULL if there is none. */
mqPFC *mq_pfc_get(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_MODULES_PFC_H */
