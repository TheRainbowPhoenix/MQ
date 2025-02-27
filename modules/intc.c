//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/modules/intc.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdlib.h>

// TODO: INTC should only send interrupts if higher level than IMASK
// TODO: INTC should only send interrupts if higher level than USERIMASK

static int moduleID = -1;
static u16 const IPR_masks[] = {
    0xfff0, 0xfff0, 0x000f, 0x0f00, 0xf0f0, 0xffff,
    0xfff0, 0xffff, 0xf0f0, 0xffff, 0xff00, 0xff00,
};
static u16 const IMR_masks[] = {
    0x07, 0x0f, 0x07, 0xfc, 0x79, 0xf7, 0x1b, 0xff,
    0x07, 0x12, 0x37, 0x01, 0x38,
};

static void inithook(void)
{
    moduleID = mq_module_register();
}
MQ_HOOK_REGISTER(init, inithook)

mqINTC *mq_intc_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static u32 read_IPRn(struct mqMMIO *io, u32 addr, int size)
{
    mqINTC *INTC = io->userdata;
    (void)size;
    return INTC->IPR[(addr & 0xfff) >> 2];
}

static void write_IPRn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqINTC *INTC = io->userdata;
    (void)size;
    int n = (addr & 0xfff) >> 2;
    INTC->IPR[n] = value & IPR_masks[n];
}

static u32 read_IMRn(struct mqMMIO *io, u32 addr, int size)
{
    mqINTC *INTC = io->userdata;
    (void)size;
    return INTC->IMR[((addr & 0xfff) - 0x80) >> 4];
}

static void write_IMRn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqINTC *INTC = io->userdata;
    (void)size;
    int n = ((addr & 0xfff) - 0x80) >> 4;
    INTC->IMR[n] |= (value & IMR_masks[n]);
}

static u32 read_IMCRn(void *data)
{
    (void)data;
    /* Reading IMCR returns an undefined value. */
    return 0;
}

static void write_IMCRn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqINTC *INTC = io->userdata;
    (void)size;
    int n = ((addr & 0xfff) - 0xc0) >> 4;
    INTC->IMR[n] &= (value & IMR_masks[n]);
}

bool mq_intc_setup(mqMachine *mach)
{
    mqMemory *mem = mach->memory;
    mqPage *pg405 = mq_memory_getPagePrealloc(mem, 0xa4050000, 0x1fb, 6);
    mqPage *pg408 = mq_memory_getPagePrealloc(mem, 0xa4080000, 0x0f1, 38);
    mqPage *pg414 = mq_memory_getPagePrealloc(mem, 0xa4140000, 0x0c1, 7);
    mqPage *pg470 = mq_memory_getPagePrealloc(mem, 0xa4700000, 1, 1);
    mqPage *pgff0 = mq_memory_getPage(mem, 0xff000000);
    if(!pg405 || !pg408 || !pg414 || !pg470 || !pgff0)
        return false;

    mqINTC *INTC = calloc(1, sizeof *INTC);
    if(!INTC)
        return false;

    /* Initial state at reset */
    INTC->ICR0 = 0x00c0;

    bool ok = true;
    int ioID;

    ioID = mq_page_addIO(
        pg408, "IPRn", MQ_MMIO_SIZE_2, read_IPRn, write_IPRn, NULL, INTC);
    ok &= mq_page_mapIO(pg408, ioID, 0xa4080000, 0x30, 4);

    ioID = mq_page_addIO(
        pg408, "IMRn", MQ_MMIO_SIZE_1, read_IMRn, write_IMRn, NULL, INTC);
    ok &= mq_page_mapIO(pg408, ioID, 0xa4080080, 0x34, 4);

    ioID = mq_page_addIO(
        pg408, "IMCRn", MQ_MMIO_SIZE_1, read_IMCRn, write_IMCRn, NULL, INTC);
    ok &= mq_page_mapIO(pg408, ioID, 0xa40800c0, 0x34, 4);

    // TODO: Plenty of INTC registers missing (mainly INTEVT)

    // 04050.1dc  PINTCRA
    // 04050.1de  PINTCRB
    // 04050.1ea  PINTSRA
    // 04050.1ec  PINTSRB
    // 04050.1ee  PINTSRC
    // 04050.1fa  PINTSRD
    //
    // 04140.000  ICR0
    // 04140.010  INTPRI00
    // 04140.01c  ICR1
    // 04140.024  INTREQ00
    // 04140.044  INTMSK00
    // 04140.064  INTMSKCLR00
    // 04140.0c0  NMIFCR
    // 04700.000  USERIMASK
    // ff000.028  INTEVT

    if(ok)
        mach->modules[moduleID] = INTC;
    else
        free(INTC);
    return ok;
}

static void mq_intc_cleanup(mqMachine *mach)
{
    mqINTC *INTC = mach->modules[moduleID];
    if(INTC)
        free(INTC);
}
MQ_HOOK_REGISTER(module_cleanup, mq_intc_cleanup)
