//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/modules/dma.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdlib.h>

static int moduleID = -1;

static void inithook(void)
{
    moduleID = mq_module_register();
}
MQ_HOOK_REGISTER(init, inithook)

mqDMA *mq_dma_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static void write_SAR(mqDMA_Channel *ch, u32 value)
{
    ch->SAR = value;
}
static void write_DAR(mqDMA_Channel *ch, u32 value)
{
    ch->DAR = value;
}
static void write_TCR(mqDMA_Channel *ch, u32 value)
{
    ch->TCR = value;
}
static void write_CHCR(mqDMA_Channel *ch, u32 value)
{
    // TODO[dma]: Consequences of writing to CHCR
    ch->CHCR = value & 0x4fefffff;
    mq_log(MQ_LOG_ERROR, "not handling write to CHCR!");
}
static void write_DMAOR(mqMachine *mach, u32 value)
{
    mqDMA *DMA = mq_dma_get(mach);
    // TODO[dma]: Consequences of writing to DMAOR
    DMA->DMAOR = value & 0xf307;
    mq_log(MQ_LOG_ERROR, "not handling write to DMAOR!");
}

static bool mq_dma_mapChannel(mqMMIOPage *mmpg, mqDMA *DMA, int i, u32 addr)
{
    struct mqDMA_Channel *ch = &DMA->channels[i];

    bool ok = true;
    ok &= mq_page_mapRegister32(mmpg, "SAR", addr,
        NULL, write_SAR, &ch->SAR, ch);
    ok &= mq_page_mapRegister32(mmpg, "DAR", addr + 4,
        NULL, write_DAR, &ch->SAR, ch);
    ok &= mq_page_mapRegister32(mmpg, "TCR", addr + 8,
        NULL, write_TCR, &ch->SAR, ch);
    ok &= mq_page_mapRegister32(mmpg, "CHCR", addr + 12,
        NULL, write_CHCR, &ch->CHCR, ch);
    return ok;
}

bool mq_dma_setup(mqMachine *mach)
{
    mqChunk *ch = mq_memory_getOrCreateChunk(mach->memory, 0xfe000000);
    if(!ch)
        return false;
    mqMMIOPage *mmpg = mq_chunk_getOrCreateMMIOPage(ch, 0xfe008000, 0x90, 25);
    if(!mmpg)
        return false;

    mqDMA *DMA = calloc(1, sizeof *DMA);
    if(!DMA)
        return false;

    bool ok = true;
    ok &= mq_page_mapRegister16(mmpg, "DMAOR", 0xfe008060,
        NULL, write_DMAOR, &DMA->DMAOR, mach);

    u32 channelAddresses[6] = {
        0xfe008020, 0xfe008030, 0xfe008040, 0xfe008050, 0xfe008070,
        0xfe008080,
    };
    for(int i = 0; i < 6; i++)
        ok &= mq_dma_mapChannel(mmpg, DMA, i, channelAddresses[i]);

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
