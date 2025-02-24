//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.dma: Direct Memory Access Controller
// Reference -- SH7724 manual, Section 16
//   https://bible.planet-casio.com/common/hardware/mpu/sh7724.pdf
//---

#ifndef MQ_MODULES_DMA_H
#define MQ_MODULES_DMA_H

#include <mq/machine.h>
MQ_START_DEFS

struct mqDMA_Channel {
    u32 SAR;    // Source Address Register
    u32 DAR;    // Destination Address Register
    u32 TCR;    // Transfer Count Register
    u32 CHCR;   // CHannel Control Register
};

struct mqDMA {
    struct mqDMA_Channel channels[6];
    u16 DMAOR;  // DMA Operation Register

    // TODO[dma]: B-channels are not emulated
};

typedef struct mqDMA_Channel mqDMA_Channel;
typedef struct mqDMA mqDMA;

/* Setup the DMA module for a given machine. */
bool mq_dma_setup(mqMachine *mach);

/* Get the DMA module for a machine, NULL if there is none. */
mqDMA *mq_dma_get(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_MODULES_DMA_H */
