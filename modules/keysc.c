//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/modules/keysc.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdio.h>
#include <stdlib.h>

static int moduleID = -1;

static void inithook(void)
{
    moduleID = mq_module_register();
}
MQ_HOOK_REGISTER(init, inithook)

mqKEYSC *mq_keysc_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static u32 read_KIUDATA(mqMMIO *io, u32 addr, int size)
{
    (void)size;
    mqMachine *mach = io->data;
    mqKeyboard *kbd = mach->keyboard;
    if(!kbd)
        return 0;

    // KIUDATAi (with j=2i) contains (data[j+1] << 8) | data[j].
    // data[row] & (1 << col) identifies (row, col)
    // On fx-CG, keycode is (row << 4) + (7 - col)
    // -> F1 is row=9, col=6
    // -> F6 is row=9, col=1
    // -> 0 is row=1, col=6
    // -> EXE is row=1, col=2
    // -> AC/ON is row=0, col=0

    int j = addr & 0xf;
    u16 KIUDATA = 0;

    for(uint i = 0; i < kbd->keyCount; i++) {
        if(!kbd->keyStatus[i])
            continue;

        mqKeyboardKey *key = &kbd->keyInfo[i];
        if(key->row == j)
            KIUDATA |= (0x0001 << key->col);
        else if(key->row == j+1)
            KIUDATA |= (0x0100 << key->col);
    }

    // mq_log(MQ_LOG_WARNING, "[KEYSC KIUDATA @ %08x -> %04x", addr, KIUDATA);
    return KIUDATA;
}

static void write_KIUDATA(mqKEYSC *KEYSC, u32 addr, int size)
{
    (void)size;
    (void)addr;
    (void)KEYSC;
}

bool mq_keysc_setup(mqMachine *mach)
{
    mqChunk *ch = mq_memory_getOrCreateChunk(mach->memory, 0xa44b0000);
    if(!ch)
        return false;
    mqMMIOPage *mmpg = mq_chunk_getOrCreateMMIOPage(ch, 0xa44b0000, 0, 0);
    if(!mmpg)
        return false;

    mqKEYSC *KEYSC = calloc(1, sizeof *KEYSC);
    if(!KEYSC)
        return false;

    bool b = true;
    int ioID = mq_page_addIO(mmpg, "KIUDATA*", MQ_MMIO_SIZE_2, read_KIUDATA,
        write_KIUDATA, NULL, mach);
    for(int i = 0; i < 6; i++)
        b &= mq_page_mapIO(mmpg, ioID, 0xa44b0000 + 2*i, 1);

    // 0xa44b000c KIUCNTREG
    // 0xa44b000e KIAUTOFIXREG
    // 0xa44b0010 KIUMODEREG
    // 0xa44b0012 KIUSTATEREG
    // 0xa44b0014 KIUINTREG
    // 0xa44b0016 KIUWSETREG
    // 0xa44b0018 KIUINTERVALREG
    // 0xa44b001a KOUTPINSET
    // 0xa44b001c KINPINSET

    if(b)
        mach->modules[moduleID] = KEYSC;
    else
        free(KEYSC);
    return b;
}

static void mq_keysc_cleanup(mqMachine *mach)
{
    mqKEYSC *KEYSC = mach->modules[moduleID];
    if(KEYSC)
        free(KEYSC);
}
MQ_HOOK_REGISTER(module_cleanup, mq_keysc_cleanup)
