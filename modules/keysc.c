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
    mqMachine *mach = io->userdata;
    mqKeyboard *kbd = mach->keyboard;
    if(!kbd)
        return 0;

    /* KIUDATA can be read with byte, word, double-word accesses. It reads
       like standard memory containing little-endian words, i.e. indices
         1, 0, 3, 2, 5, 4, ...
       So a byte access at KIUDATA+j returns data[j^1] and a word access at
       KIUDATA+j (j even) returns (data[j+1] << 8 | data[j]). */
    int j = addr & 0xf;
    u32 value = 0;

    for(uint i = 0; i < kbd->keyCount; i++) {
        if(!kbd->keyStatus[i])
            continue;

        mqKeyboardKey *key = &kbd->keyInfo[i];
        if(size == 1 && key->row == (j^1))
            value |= (0x0001 << key->col);
        else if(size == 2 && key->row == j)
            value |= (0x0001 << key->col);
        else if(size == 2 && key->row == j+1)
            value |= (0x0100 << key->col);
        else if(size == 4 && key->row == j)
            value |= (0x00010000 << key->col);
        else if(size == 4 && key->row == j+1)
            value |= (0x01000000 << key->col);
        else if(size == 4 && key->row == j+2)
            value |= (0x00000001 << key->col);
        else if(size == 4 && key->row == j+3)
            value |= (0x00000100 << key->col);
    }

    // mq_log(MQ_LOG_WARNING, "[KIUDATA @ %08x/%d -> %04x", addr, size, value);
    return value;
}

static u32 read_KIREGS(mqMMIO *io, u32 addr, int size)
{
    (void)io;
    (void)addr;
    (void)size;
    return 0;
}

bool mq_keysc_setup(mqMachine *mach)
{
    mqPage *pg = mq_memory_getPage(mach->memory, 0xa44b0000);
    if(!pg)
        return false;

    mqKEYSC *KEYSC = calloc(1, sizeof *KEYSC);
    if(!KEYSC)
        return false;

    bool ok = true;
    int ioID = mq_page_addIO(pg, "KIUDATA*",
        MQ_MMIO_SIZE_1 | MQ_MMIO_SIZE_2 | MQ_MMIO_SIZE_4,
        read_KIUDATA, NULL, NULL, mach);
    ok &= mq_page_mapIO(pg, ioID, 0xa44b0000, 12, 1);

    // 0xa44b000c KIUCNTREG
    // 0xa44b000e KIAUTOFIXREG
    // 0xa44b0010 KIUMODEREG
    // 0xa44b0012 KIUSTATEREG
    // 0xa44b0014 KIUINTREG
    // 0xa44b0016 KIUWSETREG
    // 0xa44b0018 KIUINTERVALREG
    // 0xa44b001a KOUTPINSET
    // 0xa44b001c KINPINSET

    int ioID2 = mq_page_addIO(pg, "KIREGS",
        MQ_MMIO_SIZE_1 | MQ_MMIO_SIZE_2 | MQ_MMIO_SIZE_4,
        read_KIREGS, NULL, NULL, mach);
    ok &= mq_page_mapIO(pg, ioID2, 0xa44b000c, 18, 1);

    if(ok)
        mach->modules[moduleID] = KEYSC;
    else
        free(KEYSC);
    return ok;
}

static void mq_keysc_cleanup(mqMachine *mach)
{
    mqKEYSC *KEYSC = mach->modules[moduleID];
    if(KEYSC)
        free(KEYSC);
}
MQ_HOOK_REGISTER(module_cleanup, mq_keysc_cleanup)

static void mq_keysc_createObserver(mqMachine *omach, mqMachine const *mach)
{
    omach->modules[moduleID] = memdup(mach->modules[moduleID], sizeof(mqKEYSC));
}
MQ_HOOK_REGISTER(module_createObserver, mq_keysc_createObserver)

static void mq_keysc_destroyObserver(mqMachine *omach)
{
    mqKEYSC *KEYSC = omach->modules[moduleID];
    if(KEYSC)
        free(KEYSC);
}
MQ_HOOK_REGISTER(module_destroyObserver, mq_keysc_destroyObserver)
