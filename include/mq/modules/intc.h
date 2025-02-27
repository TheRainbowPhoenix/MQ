//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.intc: Interrupt Controller
// Reference -- SH7724 manual, Section 13
//   https://bible.planet-casio.com/common/hardware/mpu/sh7724.pdf
//---

#ifndef MQ_MODULES_INTC_H
#define MQ_MODULES_INTC_H

#include <mq/machine.h>
MQ_START_DEFS

struct mqINTC {
    u16 ICR0;           // Interrupt Control Register 0
    u16 ICR1;           // Interrupt Control Register 1

    u16 IPR[12];        // Interrupt Priority Register A to L (IPRA..IPRL)
    u8 IMR[13];         // Interrupt Mask Register 0 to 12 (IMR0..IMR12)

    u16 PINTCR[2];      // PINT Control Register A and B (PINTCRA, PINTCRB)
    u8 PINTSR[4];       // PINT Status Register A to D (PINTSRA..PINTSRD)

    u32 INTPRI00;       // Interrupt Priority Register 00 (IRQ)
    u8 INTREQ00;        // Interrupt Request Register 00 (IRQ)
    u8 INTMSK00;        // Interrupt Mask Register 00 (IRQ)

    u32 USERIMASK;      // User Interrupt Mask
    u16 NMIFCR;         // NMI Flag Control Register
};

typedef struct mqINTC mqINTC;

/* Setup the INTC module for a given machine. */
bool mq_intc_setup(mqMachine *mach);

/* Get the INTC module for a machine, NULL if there is none. */
mqINTC *mq_intc_get(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_MODULES_INTC_H */
