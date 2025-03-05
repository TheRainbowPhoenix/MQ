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

enum mqInt {
    MQ_INT_NMI,             // 40, 16, ICR0.MAI, 0x1c0
    MQ_INT_41,              // 41, 15, (no mask), 0x5e0
    MQ_INT_IRQ0,            // 52, INTPRI00 & f0000000, INTMSK00 & 80, 0x600
    MQ_INT_IRQ1,            // 53, INTPRI00 & 0f000000, INTMSK00 & 40, 0x620
    MQ_INT_IRQ2,            // 54, INTPRI00 & 00f00000, INTMSK00 & 20, 0x640
    MQ_INT_IRQ3,            // 55, INTPRI00 & 000f0000, INTMSK00 & 10, 0x660
    MQ_INT_5a,              // 5a, IPRB & 00f0, IMR3  & 10, 0x700
    MQ_INT_5b,              // 5b, IPRB & 00f0, IMR3  & 20, 0x720
    MQ_INT_5c,              // 5c, IPRB & 00f0, IMR3  & 40, 0x740
    MQ_INT_5d,              // 5d, IPRB & 00f0, IMR3  & 80, 0x760
    MQ_INT_DMA_DEI0,        // 62, IPRE & f000, IMR1  & 01, 0x800
    MQ_INT_DMA_DEI1,        // 63, IPRE & f000, IMR1  & 02, 0x820
    MQ_INT_DMA_DEI2,        // 64, IPRE & f000, IMR1  & 04, 0x840
    MQ_INT_DMA_DEI3,        // 65, IPRE & f000, IMR1  & 08, 0x860
    MQ_INT_Cmod_TUNI3,      // 6a, IPRE & 00f0, IMR2  & 01, 0x900
    MQ_INT_Cmod_TUNI0,      // 71, IPRJ & f000, IMR6  & 08, 0x9e0
    MQ_INT_USB_USI,         // 73, IPRF & 00f0, IMR9  & 02, 0xa20
    MQ_INT_RTC_ATI,         // 76, IPRK & f000, IMR10 & 04, 0xa80
    MQ_INT_RTC_PRI,         // 77, IPRK & f000, IMR10 & 02, 0xaa0
    MQ_INT_RTC_CUI,         // 78, IPRK & f000, IMR10 & 01, 0xac0
    MQ_INT_SDC_7a,          // 7a, IPRK & 0f00, IMR10 & 10, 0xb00
    MQ_INT_SDC_7b,          // 7b, IPRK & 0f00, IMR10 & 20, 0xb20
    MQ_INT_DMA_DEI4,        // 7e, IPRF & 0f00, IMR5  & 10, 0xb80
    MQ_INT_DMA_DEI5,        // 7f, IPRF & 0f00, IMR5  & 20, 0xba0
    MQ_INT_DMA_DADERR,      // 80, IPRF & 0f00, IMR5  & 40, 0xbc0
    MQ_INT_KEYSC,           // 81, IPRF & f000, IMR5  & 80, 0xbe0
    MQ_INT_82,              // 82, IPRG & f000, IMR5  & 01, 0xc00 (SCIF)
    MQ_INT_Cmod_TUNI1,      // 83, IPRG & 0f00, IMR5  & 02, 0xc20
    MQ_INT_Cmod_TUNI2,      // 84, IPRG & 00f0, IMR5  & 04, 0xc40
    MQ_INT_86,              // 86, IPRH & f000, IMR6  & 01, 0xc80 (MSIOF0)
    MQ_INT_87,              // 87, IPRH & 0f00, IMR6  & 02, 0xca0
    MQ_INT_Cmod_TUNI4,      // 8a, IPRI & f000, IMR6  & 10, 0xd00
    MQ_INT_8e,              // 8e, IPRH & 00f0, IMR7  & 04, 0xd80
    MQ_INT_FLCTL_TE,        // 8f, IPRH & 00f0, IMR7  & 08, 0xda0
    MQ_INT_90,              // 90, IPRH & 00f0, IMR7  & 01, 0xdc0
    MQ_INT_91,              // 91, IPRH & 00f0, IMR7  & 02, 0xde0
    MQ_INT_I2C_AL,          // 92, IPRH & 000f, IMR7  & 10, 0xe00
    MQ_INT_I2C_NACK,        // 93, IPRH & 000f, IMR7  & 20, 0xe20
    MQ_INT_I2C_WAIT,        // 94, IPRH & 000f, IMR7  & 40, 0xe40
    MQ_INT_I2C_TE,          // 95, IPRH & 000f, IMR7  & 80, 0xe60
    MQ_INT_CMT,             // 9a, IPRF & 000f, IMR9  & 10, 0xf00
    MQ_INT_ECC_ECCSR,       // 9b, IPRI & 00f0, IMR11 & 01, 0xf20
    MQ_INT_BSC,             // 9c, IPRB & 0f00, IMR4  & 01, 0xf40
    MQ_INT_FSI,             // 9e, IPRJ & 00f0, IMR8  & 01, 0xf80
    MQ_INT_Cmod_TUNI5,      // 9f, IPRL & f000, IMR8  & 02, 0xfa0
    MQ_INT_a0,              // a0, IPRL & 0f00, IMR8  & 04, 0xfc0
    MQ_INT_TMU_TUNI2,       // c0, IPRA & f000, IMR4  & 10, 0x400
    MQ_INT_TMU_TUNI1,       // c1, IPRA & 0f00, IMR4  & 20, 0x420
    MQ_INT_TMU_TUNI0,       // c2, IPRA & 00f0, IMR4  & 40, 0x440
    MQ_INT_c7,              // c7, IPRJ & 000f, IMR0  & 02, 0x4e0
    MQ_INT_c8,              // c8, IPRJ & 000f, IMR0  & 04, 0x500
    MQ_INT_c9,              // c9, IPRJ & 000f, IMR0  & 01, 0x520
    MQ_INT_ADC_CE,          // cb, IPRB & f000, IMR4  & 08, 0x560
    MQ_INT_cc,              // cc, IPRD & 0f00, IMR12 & 08, 0x580
    MQ_INT_cd,              // cd, IPRD & 0f00, IMR12 & 10, 0x5a0
    MQ_INT_ce,              // ce, IPRD & 0f00, IMR12 & 20, 0x5c0
    MQ_INT_DSP0,            // d0, IPRC & 000f, IMR3  & 04, 0xcc0
    MQ_INT_DSP1,            // d1, IPRC & 000f, IMR3  & 08, 0xce0
    MQ_INT_d4,              // d4, IPRJ & 0f00, IMR2  & 02, 0xd40 (Cmod2a)
    MQ_INT_d5,              // d5, IPRJ & 0f00, IMR2  & 04, 0xd60 (Cmod2a)

    MQ_INT_NUM,
};

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

    /* Whether each interrupt is being raised. This is generally redundant with
       some module flags (e.g INTREQ00 for NMI) and has to be kept up-to-date
       with mq_intc_setInterruptStatus() whenever there is a change in the
       requesting module. */
    u8 interruptStatus[MQ_INT_NUM];

    /* Next interrupt to be raised, USERIMASK; or -1 */
    enum mqInt nextInterrupt;
    /* Priority of that interrupt; or 0 */
    int nextInterruptPriority;
};

struct mqINTC_InterruptInfo {
    /* IPR register number, 12 for INTPRI00, 30/31 for fixed priority 15/16 */
    u8 IPR;
    /* Bit location of the 4-bit priority value in the IPR register */
    u8 IPRpos;
    /* IMR register number, 13 for INTMSK00, 14 for ICR0, 15 for none */
    u8 IMR;
    /* Byte value of the mask bit in the IMR register */
    u16 IMRmask;
    /* Event code */
    u16 INTEVT;
};

typedef enum mqInt mqInt;
typedef struct mqINTC mqINTC;
typedef struct mqINTC_InterruptInfo mqINTC_InterruptInfo;

/* Setup the INTC module for a given machine. */
bool mq_intc_setup(mqMachine *mach);

/* Get the INTC module for a machine, NULL if there is none. */
mqINTC *mq_intc_get(mqMachine *mach);

/* Set an interrupt's raised status. */
void mq_intc_setInterruptStatus(mqMachine *mach, mqInt interrupt, bool raised);

/* Check an interrupt's current priority and mask. */
int mq_intc_interruptPriority(mqINTC *INTC, mqInt interrupt);
bool mq_intc_isInterruptMasked(mqINTC *INTC, mqInt interrupt);

/* Determine what the next interrut and its priority will be. Should not need
   to be called from the outside. */
void mq_intc_updateLogic(mqMachine *mach);

/* Propagate changs to SR.BL, SR.IMASK and USERIMASK to the interrupt signal.
   Should be called whenever these change. */
void mq_intc_updateBlockLogic(mqMachine *mach);

//=== Misc. information ======================================================//

/* Enumeration name of an interrupt number. */
char const *mq_intc_interruptName(mqInt interrupt);

/* Get interrupt number by event code (-1 if none matches). */
mqInt mq_intc_interruptForEventCode(u32 INTEVT);

/* Get the static data for an interrupt. All 0 for invalid interrupts. */
mqINTC_InterruptInfo const *mq_intc_interruptInfo(mqInt interrupt);

MQ_END_DEFS
#endif /* MQ_MODULES_INTC_H */
