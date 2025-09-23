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

static int moduleID = -1;
static u16 const IPR_masks[] = {
    0xfff0, 0xfff0, 0x000f, 0x0f00, 0xf0f0, 0xffff,
    0xfff0, 0xffff, 0xf0f0, 0xffff, 0xff00, 0xff00,
};
static u16 const IMR_masks[] = {
    0x07, 0x0f, 0x07, 0xfc, 0x79, 0xf7, 0x1b, 0xff,
    0x07, 0x12, 0x37, 0x01, 0x38,
};

mqINTC_InterruptInfo interrupts[MQ_INT_NUM] = {
    { 31 /* = 16 */, 0, 14 /* ICR0 */, 0x4000, 0x1c0 },  // MQ_INT_NMI
    { 30 /* = 15 */, 0, 15 /* none */,   0x00, 0x5e0 },  // MQ_INT_41
    { 12 /* INTPRI00 */, 28, 13 /* INTMSK00 */, 0x80, 0x600 },  // MQ_INT_IRQ0
    { 12 /* INTPRI00 */, 24, 13 /* INTMSK00 */, 0x40, 0x620 },  // MQ_INT_IRQ1
    { 12 /* INTPRI00 */, 20, 13 /* INTMSK00 */, 0x20, 0x640 },  // MQ_INT_IRQ2
    { 12 /* INTPRI00 */, 16, 13 /* INTMSK00 */, 0x10, 0x660 },  // MQ_INT_IRQ3
    {  1 /* IPRB */,  4,  3, 0x10, 0x700 },  // MQ_INT_5a
    {  1 /* IPRB */,  4,  3, 0x20, 0x720 },  // MQ_INT_5b
    {  1 /* IPRB */,  4,  3, 0x40, 0x740 },  // MQ_INT_5c
    {  1 /* IPRB */,  4,  3, 0x80, 0x760 },  // MQ_INT_5d
    {  4 /* IPRE */, 12,  1, 0x01, 0x800 },  // MQ_INT_DMA_DEI0
    {  4 /* IPRE */, 12,  1, 0x02, 0x820 },  // MQ_INT_DMA_DEI1
    {  4 /* IPRE */, 12,  1, 0x04, 0x840 },  // MQ_INT_DMA_DEI2
    {  4 /* IPRE */, 12,  1, 0x08, 0x860 },  // MQ_INT_DMA_DEI3
    {  4 /* IPRE */,  4,  2, 0x01, 0x900 },  // MQ_INT_Cmod_TUNI3
    {  9 /* IPRJ */, 12,  6, 0x08, 0x9e0 },  // MQ_INT_Cmod_TUNI0
    {  5 /* IPRF */,  4,  9, 0x02, 0xa20 },  // MQ_INT_USB_USI
    { 10 /* IPRK */, 12, 10, 0x01, 0xa80 },  // MQ_INT_RTC_ATI
    { 10 /* IPRK */, 12, 10, 0x02, 0xaa0 },  // MQ_INT_RTC_PRI
    { 10 /* IPRK */, 12, 10, 0x04, 0xac0 },  // MQ_INT_RTC_CUI
    { 10 /* IPRK */,  8, 10, 0x10, 0xb00 },  // MQ_INT_SDC_7a
    { 10 /* IPRK */,  8, 10, 0x20, 0xb20 },  // MQ_INT_SDC_7b
    {  5 /* IPRF */,  8,  5, 0x10, 0xb80 },  // MQ_INT_DMA_DEI4
    {  5 /* IPRF */,  8,  5, 0x20, 0xba0 },  // MQ_INT_DMA_DEI5
    {  5 /* IPRF */,  8,  5, 0x40, 0xbc0 },  // MQ_INT_DMA_DADERR
    {  5 /* IPRF */, 12,  5, 0x80, 0xbe0 },  // MQ_INT_KEYSC
    {  6 /* IPRG */, 12,  5, 0x01, 0xc00 },  // MQ_INT_82
    {  6 /* IPRG */,  8,  5, 0x02, 0xc20 },  // MQ_INT_Cmod_TUNI1
    {  6 /* IPRG */,  4,  5, 0x04, 0xc40 },  // MQ_INT_Cmod_TUNI2
    {  7 /* IPRH */, 12,  6, 0x01, 0xc80 },  // MQ_INT_86
    {  7 /* IPRH */,  8,  6, 0x02, 0xca0 },  // MQ_INT_87
    {  8 /* IPRI */, 12,  6, 0x10, 0xd00 },  // MQ_INT_Cmod_TUNI4
    {  7 /* IPRH */,  4,  7, 0x04, 0xd80 },  // MQ_INT_8e
    {  7 /* IPRH */,  4,  7, 0x08, 0xda0 },  // MQ_INT_FLCTL_TE
    {  7 /* IPRH */,  4,  7, 0x01, 0xdc0 },  // MQ_INT_90
    {  7 /* IPRH */,  4,  7, 0x02, 0xde0 },  // MQ_INT_91
    {  7 /* IPRH */,  0,  7, 0x10, 0xe00 },  // MQ_INT_I2C_AL
    {  7 /* IPRH */,  0,  7, 0x20, 0xe20 },  // MQ_INT_I2C_NACK
    {  7 /* IPRH */,  0,  7, 0x40, 0xe40 },  // MQ_INT_I2C_WAIT
    {  7 /* IPRH */,  0,  7, 0x80, 0xe60 },  // MQ_INT_I2C_TE
    {  5 /* IPRF */,  0,  9, 0x10, 0xf00 },  // MQ_INT_CMT
    {  8 /* IPRI */,  4, 11, 0x01, 0xf20 },  // MQ_INT_ECC_ECCSR
    {  1 /* IPRB */,  8,  4, 0x01, 0xf40 },  // MQ_INT_BSC
    {  9 /* IPRJ */,  4,  8, 0x01, 0xf80 },  // MQ_INT_FSI
    { 11 /* IPRL */, 12,  8, 0x02, 0xfa0 },  // MQ_INT_Cmod_TUNI5
    { 11 /* IPRL */,  8,  8, 0x04, 0xfc0 },  // MQ_INT_a0
    {  0 /* IPRA */, 12,  4, 0x10, 0x400 },  // MQ_INT_TMU_TUNI0
    {  0 /* IPRA */,  8,  4, 0x20, 0x420 },  // MQ_INT_TMU_TUNI1
    {  0 /* IPRA */,  4,  4, 0x40, 0x440 },  // MQ_INT_TMU_TUNI2
    {  9 /* IPRJ */,  0,  0, 0x02, 0x4e0 },  // MQ_INT_c7
    {  9 /* IPRJ */,  0,  0, 0x04, 0x500 },  // MQ_INT_c8
    {  9 /* IPRJ */,  0,  0, 0x01, 0x520 },  // MQ_INT_c9
    {  1 /* IPRB */, 12,  4, 0x08, 0x560 },  // MQ_INT_ADC_CE
    {  3 /* IPRD */,  8, 12, 0x08, 0x580 },  // MQ_INT_cc
    {  3 /* IPRD */,  8, 12, 0x10, 0x5a0 },  // MQ_INT_cd
    {  3 /* IPRD */,  8, 12, 0x20, 0x5c0 },  // MQ_INT_ce
    {  2 /* IPRC */,  0,  3, 0x04, 0xcc0 },  // MQ_INT_DSP0
    {  2 /* IPRC */,  0,  3, 0x08, 0xce0 },  // MQ_INT_DSP1
    {  9 /* IPRJ */,  8,  2, 0x02, 0xd40 },  // MQ_INT_d4
    {  9 /* IPRJ */,  8,  2, 0x04, 0xd60 },  // MQ_INT_d5
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

int mq_intc_interruptPriority(mqINTC *INTC, mqInt num)
{
    int IPRnum = interrupts[num].IPR;
    int IPRpos = interrupts[num].IPRpos;

    if(MQ_LIKELY(IPRnum < 12))
        return (INTC->IPR[IPRnum] >> IPRpos) & 0xf;
    if(IPRnum == 12)
        return (INTC->INTPRI00 >> IPRpos) & 0xf;
    if(IPRnum >= 16)
        return IPRnum - 16;
    mq_log(MQ_LOG_ERROR, "invalid interrupt setting (%d): IPR=%d/%d",
        num, IPRnum, IPRpos);
    return 0;
}

bool mq_intc_isInterruptMasked(mqINTC *INTC, mqInt num)
{
    int IMRnum = interrupts[num].IMR;
    u32 IMRmask = interrupts[num].IMRmask;

    if(MQ_LIKELY(IMRnum < 13))
        return (INTC->IMR[IMRnum] & IMRmask) != 0;
    if(IMRnum == 13)
        return (INTC->INTMSK00 & IMRmask) != 0;
    if(IMRnum == 14)
        return (INTC->ICR0 & IMRmask) != 0;
    if(IMRnum != 15) {
        mq_log(MQ_LOG_ERROR, "invalid interrupt setting (%d): IMR=%d/%x",
            num, IMRnum, IMRmask);
    }
    return false;
}

static void notifyCPU(mqMachine *mach)
{
    mqINTC *INTC = mach->modules[moduleID];
    if(!INTC)
        return;

    int interrupt = INTC->nextInterrupt;
    if(interrupt < 0) {
        mq_cpu_setIncomingInterrupt(&mach->cpu, -1, 0, 0);
        return;
    }

    u32 INTEVT = interrupts[interrupt].INTEVT;
    int priority = mq_intc_interruptPriority(INTC, interrupt);
    mq_cpu_setIncomingInterrupt(&mach->cpu, interrupt, INTEVT, priority);
}

void mq_intc_updateLogic(mqMachine *mach)
{
    mqINTC *INTC = mach->modules[moduleID];
    if(!INTC)
        return;

    int USERIMASK = (INTC->USERIMASK >> 4) & 0xf;
    mqInt highestInterrupt = -1;
    int highestPriority = 0;

    for(mqInt num = 0; num < MQ_INT_NUM; num++) {
        if(!INTC->interruptStatus[num] || mq_intc_isInterruptMasked(INTC, num))
            continue;

        int prio = mq_intc_interruptPriority(INTC, num);
        if(prio > USERIMASK && prio > highestPriority) {
            highestInterrupt = num;
            highestPriority = prio;
        }
    }

    INTC->nextInterrupt = highestInterrupt;
    INTC->nextInterruptPriority = highestPriority;
    notifyCPU(mach);
}

void mq_intc_statsAcceptInterrupt(mqINTC *INTC, mqInt interrupt)
{
    if((uint)interrupt < MQ_INT_NUM)
        INTC->statsInterruptCount[interrupt]++;
}

static u32 read_IPRn(mqMMIO *io, u32 addr, int size)
{
    mqMachine *mach = io->userdata;
    mqINTC *INTC = mach->modules[moduleID];
    (void)size;
    return INTC->IPR[(addr & 0xfff) >> 2];
}

static void write_IPRn(mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqINTC *INTC = mach->modules[moduleID];
    (void)size;
    int n = (addr & 0xfff) >> 2;
    INTC->IPR[n] = value & IPR_masks[n];
    mq_intc_updateLogic(mach);
}

static u32 read_IMRn(mqMMIO *io, u32 addr, int size)
{
    mqMachine *mach = io->userdata;
    mqINTC *INTC = mach->modules[moduleID];
    (void)size;
    return INTC->IMR[((addr & 0xfff) - 0x80) >> 2];
}

static void write_IMRn(mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqINTC *INTC = mach->modules[moduleID];
    (void)size;
    int n = ((addr & 0xfff) - 0x80) >> 2;
    INTC->IMR[n] |= (value & IMR_masks[n]);
    mq_intc_updateLogic(mach);
}

static u32 read_IMCRn(void *data)
{
    (void)data;
    /* Reading IMCR returns an undefined value. */
    return 0;
}

static void write_IMCRn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqINTC *INTC = mach->modules[moduleID];
    (void)size;
    int n = ((addr & 0xfff) - 0xc0) >> 2;
    INTC->IMR[n] &= ~(value & IMR_masks[n]);
    mq_intc_updateLogic(mach);
}

bool mq_intc_setup(mqMachine *mach, int initializeKind)
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
    INTC->nextInterrupt = -1;

    bool ok = true;
    int ioID;

    ioID = mq_page_addIO(
        pg408, "IPRn", MQ_MMIO_SIZE_2, read_IPRn, write_IPRn, NULL, mach);
    ok &= mq_page_mapIO(pg408, ioID, 0xa4080000, 0x30, 4);

    ioID = mq_page_addIO(
        pg408, "IMRn", MQ_MMIO_SIZE_1, read_IMRn, write_IMRn, NULL, mach);
    ok &= mq_page_mapIO(pg408, ioID, 0xa4080080, 0x34, 4);

    ioID = mq_page_addIO(
        pg408, "IMCRn", MQ_MMIO_SIZE_1, read_IMCRn, write_IMCRn, NULL, mach);
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

    /* Load initial OS state */
    // TODO: Move to Casiowin module, as this is version-dependent!
    INTC->IPR[0] = 0x0800;
    INTC->IPR[1] = 0xc000;
    INTC->IPR[5] = 0xd000;

    INTC->IMR[0] = 0x07;
    INTC->IMR[1] = 0x0f;
    INTC->IMR[2] = 0x07;
    INTC->IMR[3] = 0xfc;
    // IMR4
    INTC->IMR[5] = 0x77;
    INTC->IMR[6] = 0x1b;
    INTC->IMR[7] = 0xff;
    INTC->IMR[8] = 0x07;
    INTC->IMR[9] = 0x12;
    // IMR10
    INTC->IMR[11] = 0x01;
    INTC->IMR[12] = 0x38;

    if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_FX) {
        INTC->IPR[10] = 0x8d00;
        INTC->IMR[4] = 0x70;
        INTC->IMR[10] = 0x14;
    }
    else if(initializeKind == MQ_MACHINE_INITIALIZE_ADDIN_CG) {
        INTC->IPR[10] = 0x8000;
        INTC->IMR[4] = 0x00;
        INTC->IMR[10] = 0x34;
    }

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

void mq_intc_setInterruptStatus(mqMachine *mach, mqInt interrupt, bool raised)
{
    mqINTC *INTC = mach->modules[moduleID];
    if(!INTC || (uint)interrupt >= MQ_INT_NUM)
        return;
    if(INTC->interruptStatus[interrupt] == (int)raised)
        return;

    INTC->interruptStatus[interrupt] = raised;
    // INTC->numRaisedInterrupts += raised - !raised;

    // TODO[intc]: Smarter interrupt update procedure?
    mq_intc_updateLogic(mach);
}

char const *mq_intc_interruptName(mqInt interrupt)
{
    char const *int_names[MQ_INT_NUM] = {
        "NMI",
        "int_41",
        "IRQ0",
        "IRQ1",
        "IRQ2",
        "IRQ3",
        "int_5a",
        "int_5b",
        "int_5c",
        "int_5d",
        "DMA Channel 0",
        "DMA Channel 1",
        "DMA Channel 2",
        "DMA Channel 3",
        "ETMU3 Underflow",
        "ETMU0 Underflow",
        "USB USI",
        "RTC Alarm",
        "RTC Periodic",
        "RTC Carry",
        "sdc7a",
        "sdc7b",
        "DMA Channel 4",
        "DMA Channel 5",
        "DMA Address Error",
        "KEYSC",
        "int_82",
        "ETMU1 Underflow",
        "ETMU2 Underflow",
        "int_86",
        "int_87",
        "ETMU4 Underflow",
        "int_8e",
        "FLCTL Transfer End",
        "int_90",
        "int_91",
        "I2C Arbitration Lost",
        "I2C NACK",
        "I2C Wait",
        "I2C Transmit Enable",
        "CMT Compare Match",
        "ECC ECCSR",
        "BSC",
        "FSI",
        "ETMU5 Underflow",
        "int_a0",
        "TMU2 Underflow",
        "TMU1 Underflow",
        "TMU0 Underflow",
        "int_c7",
        "int_c8",
        "int_c9",
        "ADC Conversion End",
        "int_cc",
        "int_cd",
        "int_ce",
        "DSP0",
        "DSP1",
        "int_d4",
        "int_d5",
    };

    return ((uint)interrupt < MQ_INT_NUM) ? int_names[interrupt] : NULL;
}

mqInt mq_intc_interruptForEventCode(u32 INTEVT)
{
    for(int i = 0; i < MQ_INT_NUM; i++) {
        if(interrupts[i].INTEVT == INTEVT)
            return (mqInt)i;
    }
    return (mqInt)-1;
}

mqINTC_InterruptInfo const *mq_intc_interruptInfo(mqInt interrupt)
{
    static mqINTC_InterruptInfo zeroInfo = { 0 };
    return (uint)interrupt < MQ_INT_NUM ? &interrupts[interrupt] : &zeroInfo;
}
