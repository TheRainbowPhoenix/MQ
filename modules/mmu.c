//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/modules/mmu.h>
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

static void write_PTEH(mqMachine *mach, u32 value)
{
    mqMMU *MMU = mach->modules[moduleID];
    MMU->PTEH = value & 0xfffffcff;
}

static void write_PTEL(mqMachine *mach, u32 value)
{
    mqMMU *MMU = mach->modules[moduleID];
    MMU->PTEL = value & 0x1ffffdff;
}

static void write_TTB(mqMachine *mach, u32 value)
{
    mqMMU *MMU = mach->modules[moduleID];
    /* TTB has no meaning in hardware and is just used by software. */
    MMU->TTB = value;
}

static void write_TEA(mqMachine *mach, u32 value)
{
    mqMMU *MMU = mach->modules[moduleID];
    MMU->TEA = value;
}

static void write_MMUCR(mqMachine *mach, u32 value)
{
    mqMMU *MMU = mach->modules[moduleID];
    // TODO[mmu]: Consequences of writing to MMUCR
    MMU->MMUCR = value & 0xfcfcfdf5;
}

static void write_PASCR(mqMachine *mach, u32 value)
{
    mqMMU *MMU = mach->modules[moduleID];
    /* We don't emulate the low-level details of waiting during memory
       accesses so this has no effect for us. */
    MMU->PASCR = value & 0x000000ff;
}

static void write_IRMCR(mqMachine *mach, u32 value)
{
    mqMMU *MMU = mach->modules[moduleID];
    /* We don't emulate the low-level details of code fetches so this has no
       effect for us. */
    MMU->IRMCR = value & 0x0000001f;
}

bool mq_module_mmu_setup(mqMachine *mach)
{
    bool ok = false;

    mqChunk *ch = mq_memory_getOrCreateChunk(mach->memory, 0xff000000);
    if(!ch)
        return false;
    mqMMIOPage *mmpg = mq_chunk_getOrCreateMMIOPage(ch, 0xff000000, 0, 0);
    if(!mmpg)
        return false;

    mqMMU *MMU = calloc(1, sizeof *MMU);
    /* Initial values after a reset, as per manual */
    MMU->MMUCR = 0x00000000;
    MMU->PASCR = 0x00000082;
    MMU->IRMCR = 0x00000000;

    if(!MMU)
        goto end;

    ok &= mq_page_mapRegister32(mmpg, "PTEH", 0xff000000,
        NULL, write_PTEH, &MMU->PTEH, mach);
    ok &= mq_page_mapRegister32(mmpg, "PTEL", 0xff000004,
        NULL, write_PTEL, &MMU->PTEL, mach);
    ok &= mq_page_mapRegister32(mmpg, "TTB", 0xff000008,
        NULL, write_TTB, &MMU->TTB, mach);
    ok &= mq_page_mapRegister32(mmpg, "TEA", 0xff00000c,
        NULL, write_TEA, &MMU->TEA, mach);
    ok &= mq_page_mapRegister32(mmpg, "MMUCR", 0xff000010,
        NULL, write_MMUCR, &MMU->MMUCR, mach);
    ok &= mq_page_mapRegister32(mmpg, "PASCR", 0xff000070,
        NULL, write_PASCR, &MMU->PASCR, mach);
    ok &= mq_page_mapRegister32(mmpg, "IRMCR", 0xff000078,
        NULL, write_IRMCR, &MMU->IRMCR, mach);

    ok = true;

end:
    if(ok)
        mach->modules[moduleID] = MMU;
    else if(MMU)
        free(MMU);
    return ok;
}

static bool readhook(
    struct mqMachine *mach, struct mqMemory *mem, u32 addr, int sz, u32 *res)
{
    mqMMU *MMU = mach->modules[moduleID];
    if(!MMU || sz != 4)
        return false;

    if((addr & 0xff000003) == 0xf2000000) {
        mq_log(MQ_LOG_WARNING, "ITLB Address read @ %08x -> 0", addr);
        *res = 0;
        return true;
    }
    if((addr & 0xff800003) == 0xf3000000) {
        mq_log(MQ_LOG_WARNING, "ITLB Data read @ %08x -> 0", addr);
        *res = 0;
        return true;
    }
    if((addr & 0xfff00003) == 0xf6000000) {
        mq_log(MQ_LOG_WARNING, "UTLB Address read @ %08x -> 0", addr);
        *res = 0;
        return true;
    }
    if((addr & 0xfff00003) == 0xf7000000) {
        mq_log(MQ_LOG_WARNING, "UTLB Data read @ %08x -> 0", addr);
        *res = 0;
        return true;
    }
    return false;
}
MQ_HOOK_REGISTER(memory_read, readhook)

static void mq_module_mmu_cleanup(mqMachine *mach)
{
    mqMMU *MMU = mach->modules[moduleID];
    if(MMU)
        free(MMU);
}
MQ_HOOK_REGISTER(module_cleanup, mq_module_mmu_cleanup)
