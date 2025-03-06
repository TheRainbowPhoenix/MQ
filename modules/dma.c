//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/modules/dma.h>
#include <mq/modules/intc.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdlib.h>

static int moduleID = -1;
static int processID = -1;

static void inithook(void)
{
    moduleID = mq_module_register();
    processID = mq_process_register();
}
MQ_HOOK_REGISTER(init, inithook)

mqDMA *mq_dma_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static bool isChannelRunning(mqDMA *DMA, uint i)
{
    if(i > 6 || !(DMA->DMAOR & 1))
        return false;

    mqDMA_Channel *ch = &DMA->channels[i];
    return (ch->CHCR & 1) != 0;
}

static bool isDMARunning(mqDMA *DMA)
{
    if(!(DMA->DMAOR & 1))
        return false;
    for(int i = 0; i < 6; i++) {
        if(DMA->channels[i].CHCR & 1)
            return true;
    }
    return false;
}

static void copyData(mqMachine *mach, u32 SAR, u32 DAR, int size)
{
    /* DMA operates in physical memory, but emulator uses virtual memory. The
       addresses we get are either < 512 MB (physical address space) or they
       represent on-chip memory/peripheral registers in P1--P4. The first kind
       needs to be normalized to P1. */
    SAR |= 0x80000000;
    DAR |= 0x80000000;

    // TODO[dma]: This data copy is SLOW AS HECK
    // Note: We need to support writing to MMIO here so it can't be too easy,
    // but usually one side will be a buffer and the other won't change so we
    // can at least try to resolve the memory layers first.

    u32 x;
    if(size == 1) {
        if(mq_memory_read8(mach, mach->memory, SAR, &x))
            mq_memory_write(mach, mach->memory, DAR, 1, x);
        return;
    }
    if(size == 2) {
        if(mq_memory_read16(mach, mach->memory, SAR, &x))
            mq_memory_write(mach, mach->memory, DAR, 2, x);
        return;
    }

    for(int i = 0; i < size / 4; i++) {
        if(mq_memory_read32(mach, mach->memory, SAR + 4 * i, &x))
            mq_memory_write(mach, mach->memory, DAR + 4 * i, 4, x);
    }
}

static void notifyINTC(mqMachine *mach, mqDMA_Channel *ch)
{
    static mqInt const channelInterruptCodes[] = {
        MQ_INT_DMA_DEI0, MQ_INT_DMA_DEI1, MQ_INT_DMA_DEI2,
        MQ_INT_DMA_DEI3, MQ_INT_DMA_DEI4, MQ_INT_DMA_DEI5,
    };
    mqInt channelInterruptCode = channelInterruptCodes[ch->index];

    u32 HE = (ch->CHCR >> 19) & 1;
    u32 HIE = (ch->CHCR >> 18) & 1;
    HE &= HIE;

    u32 TE = (ch->CHCR >> 1) & 1;
    u32 IE = ch->CHCR & 1;
    TE &= IE;

    mq_intc_setInterruptStatus(mach, channelInterruptCode, (HE | TE) != 0);
}

static void runChannel(mqMachine *mach, mqDMA_Channel *ch, uint cycles)
{
    if(cycles > ch->TCR)
        cycles = ch->TCR;

    u32 RPT = (ch->CHCR >> 25) & 0x7;
    u32 TS = (((ch->CHCR >> 20) & 0x3) << 2) + ((ch->CHCR >> 3) & 0x3);
    u32 DM = (ch->CHCR >> 14) & 0x3;
    u32 SM = (ch->CHCR >> 12) & 0x3;

    static int const transferSizes[16] = {
        1,  2,  4, 16, 32, -1, -1,  8, -1, -1, -1, 8, 16, -1, -1, -1,
    };
    int transferSize = transferSizes[TS];
    int transferDivisions = (TS == 11 || TS == 12) ? 2 : 1;

    // mq_log(MQ_LOG_DEBUG, "DMA Channel %d: running for %u cycles (TS=%dx%d)",
    //     ch->index, cycles, transferSize, transferDivisions);

    // TODO[dma]: Handle repeat mode (CHCR.RPT)
    if(RPT != 0) {
        mq_log(MQ_LOG_ERROR, "[dma] CHCR%d: RPT != 0 not supported o(x_x)o",
            ch->index);
        ch->CHCR &= 0xfffffffe;
        // TODO[easy-fix]: This doesn't stop the background process... (+2)
        return;
    }

    if(transferSize < 0) {
        mq_log(MQ_LOG_ERROR, "[dma] CHCR%d: invalid setting TS=%d (stopping)",
            ch->index, TS);
        ch->CHCR &= 0xfffffffe;
        return;
    }

    if((SM == 2 || DM == 2) && transferSize > 4) {
        mq_log(MQ_LOG_ERROR, "[dma] CHCR:%d invalid setting SM/DM decrement "
            "with TS=%d (stopping)", ch->index, TS);
        ch->CHCR &= 0xfffffffe;
        return;
    }

    // mq_log(MQ_LOG_DEBUG, "DMA: writing %08x -> %08x (%d) | CHCR%d=%08x",
    //     SAR, DAR, transferSize, ch->index, ch->CHCR);
    for(uint i = 0; i < cycles; i++) {
        u32 SAR = ch->SAR;
        u32 DAR = ch->DAR;

        /* This does a single iteration, except 2 when TS=11 or TS=12 */
        for(int j = 0; j < transferDivisions; j++) {
            copyData(mach, SAR, DAR, transferSize);
            SAR += (SM <= 1) ? transferSize : (SM == 2) ? -transferSize : 0;
            DAR += (DM <= 1) ? transferSize : (DM == 2) ? -transferSize : 0;
        }

        if(SM != 0)
            ch->SAR = SAR;
        if(DM != 0)
            ch->DAR = DAR;
    }

    ch->TCR -= cycles;

    /* Half-End flag */
    if(ch->TCR <= ch->startTCR >> 1) {
        ch->CHCR |= (1 << 19);
        if(ch->CHCR & (1 << 18)) { // HIE
            /* Emit Half-End interrupt */
            notifyINTC(mach, ch);
            mq_log(MQ_LOG_DEBUG, "DMA Channel %d: Half-End", ch->index);
        }
    }

    /* Transfer Ended flag */
    if(ch->TCR == 0) {
        mq_log(MQ_LOG_DEBUG, "DMA Channel %d: Transfer Ended", ch->index);
        ch->CHCR |= (1 << 1);
        /* Emit Transfer Ended interrupt */
        if(ch->CHCR & (1 << 2)) // IE
            notifyINTC(mach, ch);
    }
}

static void mq_dma_process(mqMachine *mach, int cyclesElapsed)
{
    mqDMA *DMA = mach->modules[moduleID];
    if(cyclesElapsed < 0 || !(DMA->DMAOR & 1))
        return;

    /* Make progress on any active channel */
    for(int i = 0; i < 6; i++) {
        mqDMA_Channel *ch = &DMA->channels[i];
        if((ch->CHCR & 1) && ch->TCR) {
            // mq_log(MQ_LOG_DEBUG, "Running DMA channel %d (%d cycles)", i,
            //     cyclesElapsed);
            runChannel(mach, ch, cyclesElapsed);
        }
    }
}

static void updateProcess(mqMachine *mach)
{
    mqDMA *DMA = mach->modules[moduleID];
    mach->processes[processID] = isDMARunning(DMA) ? mq_dma_process : NULL;
}

static void write_SAR(mqDMA_Channel *ch, u32 value)
{
    if(isChannelRunning(ch->DMA, ch->index)) {
        mq_log(MQ_LOG_WARNING, "[dma] writing to SAR%d while running!",
            ch->index);
    }
    ch->SAR = value;
}

static void write_DAR(mqDMA_Channel *ch, u32 value)
{
    if(isChannelRunning(ch->DMA, ch->index)) {
        mq_log(MQ_LOG_WARNING, "[dma] writing to DAR%d while running!",
            ch->index);
    }
    ch->DAR = value;
}

static void write_TCR(mqDMA_Channel *ch, u32 value)
{
    if(isChannelRunning(ch->DMA, ch->index)) {
        mq_log(MQ_LOG_WARNING, "[dma] writing to TCR%d while running!",
            ch->index);
    }
    ch->TCR = value;
}

static u32 read_CHCR(struct mqMMIO *io, u32 addr, int size)
{
    mqMachine *mach = io->userdata;
    mqDMA *DMA = mach->modules[moduleID];
    mqDMA_Channel *ch = &DMA->channels[(intptr_t)io->value];
    (void)addr, (void)size;

    return ch->CHCR;
}

static void write_CHCR(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqDMA *DMA = mach->modules[moduleID];
    mqDMA_Channel *ch = &DMA->channels[(intptr_t)io->value];
    (void)addr, (void)size;

    u32 new_CHCR = (ch->CHCR & value & 0x00080002) | (value & 0x4fb7fffd);
    bool interruptFlagsChanged = ((new_CHCR ^ ch->CHCR) & 0x00080002) != 0;

    /* Record TCR when DE is set to 1 to match the half-end interrupt. */
    if((ch->CHCR & 1) == 0 && (new_CHCR & 1) == 1)
        ch->startTCR = ch->TCR;

    ch->CHCR = new_CHCR;
    updateProcess(mach);

    if(interruptFlagsChanged)
        notifyINTC(mach, ch);
}

static void write_DMAOR(mqMachine *mach, u32 value)
{
    mqDMA *DMA = mq_dma_get(mach);
    DMA->DMAOR = (DMA->DMAOR & value & 0x0006) | (value & 0xf301);
    // TODO[dma]: "If DMAOR.DME is cleared (...) all transfers are terminated."
    // In what way exactly?
    if(!(DMA->DMAOR & 1)) {
        for(int i = 0; i < 6; i++)
            DMA->channels[i].CHCR &= 0xfffffffe;
    }
    updateProcess(mach);
}

static bool mq_dma_mapChannel(
    mqPage *pg, mqMachine *mach, mqDMA *DMA, int i, u32 addr)
{
    mqDMA_Channel *ch = &DMA->channels[i];

    bool ok = true;
    ok &= mq_page_mapRegister32(pg, "SAR", addr,
        NULL, write_SAR, &ch->SAR, ch);
    ok &= mq_page_mapRegister32(pg, "DAR", addr + 4,
        NULL, write_DAR, &ch->DAR, ch);
    ok &= mq_page_mapRegister32(pg, "TCR", addr + 8,
        NULL, write_TCR, &ch->TCR, ch);

    int ioID = mq_page_addIO(pg, "CHCR", MQ_MMIO_SIZE_4, read_CHCR,
        write_CHCR, (void *)(intptr_t)i, mach);
    ok &= mq_page_mapIO(pg, ioID, addr + 12, 1, 1);

    return ok;
}

bool mq_dma_setup(mqMachine *mach)
{
    mqPage *pg = mq_memory_getPagePrealloc(mach->memory, 0xfe008000, 0x90, 25);
    if(!pg)
        return false;

    mqDMA *DMA = calloc(1, sizeof *DMA);
    if(!DMA)
        return false;

    /* Initial state is everything equal to 0 */
    for(int i = 0; i < 6; i++) {
        DMA->channels[i].DMA = DMA;
        DMA->channels[i].index = i;
    }

    bool ok = true;
    ok &= mq_page_mapRegister16(pg, "DMAOR", 0xfe008060,
        NULL, write_DMAOR, &DMA->DMAOR, mach);

    u32 channelAddresses[6] = {
        0xfe008020, 0xfe008030, 0xfe008040, 0xfe008050, 0xfe008070,
        0xfe008080,
    };
    for(int i = 0; i < 6; i++)
        ok &= mq_dma_mapChannel(pg, mach, DMA, i, channelAddresses[i]);

    if(ok)
        mach->modules[moduleID] = DMA;
    else
        free(DMA);
    return ok;
}

static void mq_dma_cleanup(mqMachine *mach)
{
    mqDMA *DMA = mach->modules[moduleID];
    if(DMA)
        free(DMA);
}
MQ_HOOK_REGISTER(module_cleanup, mq_dma_cleanup)
