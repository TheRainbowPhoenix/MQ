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

mqMMU *mq_mmu_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

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

bool mq_mmu_setup(mqMachine *mach)
{
    bool ok = true;

    mqChunk *ch = mq_memory_getOrCreateChunk(mach->memory, 0xff000000);
    if(!ch)
        return false;
    mqMMIOPage *mmpg = mq_chunk_getOrCreateMMIOPage(ch, 0xff000000, 0, 0);
    if(!mmpg)
        return false;

    mqMMU *MMU = calloc(1, sizeof *MMU);
    if(!MMU)
        goto end;

    /* Initial values after a reset, as per manual */
    MMU->MMUCR = 0x00000000;
    MMU->PASCR = 0x00000082;
    MMU->IRMCR = 0x00000000;

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

end:
    if(ok)
        mach->modules[moduleID] = MMU;
    else if(MMU)
        free(MMU);
    return ok;
}

static void mq_mmu_cleanup(mqMachine *mach)
{
    mqMMU *MMU = mach->modules[moduleID];
    if(MMU)
        free(MMU);
}
MQ_HOOK_REGISTER(module_cleanup, mq_mmu_cleanup)

static bool readhook(
    struct mqMachine *mach, struct mqMemory *mem, u32 addr, int sz, u32 *res)
{
    mqMMU *MMU = mach->modules[moduleID];
    if(!MMU || sz != 4)
        return false;
    (void)mem;

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
        int index = (addr >> 8) & 0x3f;
        *res = MMU->UTLB[index].addr;
        return true;
    }
    if((addr & 0xfff00003) == 0xf7000000) {
        int index = (addr >> 8) & 0x3f;
        *res = MMU->UTLB[index].data;
        return true;
    }
    return false;
}
MQ_HOOK_REGISTER(memory_read, readhook)

bool mq_mmu_map(
    mqMachine *mach, u32 VPN, u32 PPN, int index, int pageSize, int pageCount)
{
    mqMMU *MMU = mach->modules[moduleID];
    if(!MMU)
        return false;
    if(VPN & (pageSize - 1) || PPN & (pageSize - 1))
        return false;
    if(index < 0 || index + pageCount > 64)
        return false;

    int SZ;
    if(pageSize == 0x400) // 1 kB
        SZ = 0;
    else if(pageSize == 0x1000) // 4 kB
        SZ = 1;
    else if(pageSize == 0x10000) // 64 kB
        SZ = 2;
    else if(pageSize == 0x100000) // 1 MB
        SZ = 3;
    else
        return false;

    struct mqMMU_Address addr = { 0 };
    struct mqMMU_Data data = { 0 };

    addr.VPN = VPN >> 10;
    addr.V = 1;
    data.PPN = (PPN & 0x1fffffff) >> 10;
    data.V = 1;
    data.SZ1 = SZ >> 1;
    data.PR = 3; // User read-write
    data.SZ0 = SZ & 1;
    data.C = 1;

    for(int i = 0; i < pageCount; i++) {
        MMU->UTLB[index].addr = mq_mmu_encode_address(addr);
        MMU->UTLB[index].data = mq_mmu_encode_data(data);
        addr.VPN += pageSize >> 10;
        data.PPN += pageSize >> 10;
        index++;
    }

    return true;
}

void mq_mmu_bind(mqMachine *mach)
{
    mqMMU *MMU = mq_mmu_get(mach);
    if(!MMU)
        return;

    // TODO[mq_machine_bindMMU]: Diagnose TLB multihits
    // TODO[mq_machine_bindMMU]: Move this to the MMU module

    for(int i = 0; i < 64; i++) {
        struct mqMMU_Address addr = mq_mmu_decode_address(MMU->UTLB[i].addr);
        struct mqMMU_Data data = mq_mmu_decode_data(MMU->UTLB[i].data);
        if(!addr.V || !data.V)
            continue;

        int SZ = (data.SZ1 << 1) + data.SZ0;
        u32 VPN = addr.VPN << 10;
        u32 PPN = data.PPN << 10;

        // TODO[machine/mmu]: Annotate permissions in MMU bindings
        // | In order to faithfully emulate the MMU this requires more than one
        // | bit, unless we update the bindings when changing SR.MD.
        int PR = data.PR;
        (void)PR;

        // TODO[mmu/memory]: 1-kB MMU pages by using appropriate MMIO
        if(SZ == 0) {
            mq_log(MQ_LOG_ERROR,
                "1kB UTLB page #%d (%08x -> %08x) not supported yet o(x_x)o",
                i, VPN, PPN);
            continue;
        }
        else if(SZ == 1 || SZ == 2) {
            /* For 4-kB and 64-kB MMU pages, copy memory pages. */
            int pageCount = (SZ == 1) ? 1 : 16;

            for(int i = 0; i < pageCount; i++) {
                mq_memory_copyBuffersInPage(mach->memory, PPN, VPN);
                VPN += (1 << 12);
                PPN += (1 << 12);
            }
        }
        else if(SZ == 3) {
            /* For 1-MB pages, copy the entire chunk's configuration. */
            mq_memory_copyBuffersInChunk(mach->memory, PPN, VPN);
        }
    }
}

void mq_mmu_unbind(mqMachine *mach)
{
    mqMMU *MMU = mq_mmu_get(mach);
    if(!MMU)
        return;
    // TODO[mq_machine_unbindMMU]: Move this to the MMU module

    /* Purge all buffer chunks and buffer pages in MMU-managed areas. */
    mq_memory_unbindArea(mach->memory, 0x00000000, 0x80000000);
    mq_memory_unbindArea(mach->memory, 0xc0000000, 0x20000000);
}

u32 mq_mmu_encode_address(struct mqMMU_Address addr)
{
    return (addr.VPN << 10) + (addr.D << 9) + (addr.V << 8) + addr.ASID;
}

u32 mq_mmu_encode_data(struct mqMMU_Data data)
{
    return (data.PPN << 10) + (data.V << 8) + (data.SZ1 << 7) +
           (data.PR << 5) + (data.SZ0 << 4) + (data.C << 3) +
           (data.D << 2) + (data.SH << 1) + data.WT;
}

struct mqMMU_Address mq_mmu_decode_address(u32 u)
{
    struct mqMMU_Address addr;
    addr.VPN = u >> 10;
    addr.D = u >> 9;
    addr.V = u >> 8;
    addr.ASID = u;
    return addr;
}

struct mqMMU_Data mq_mmu_decode_data(u32 u)
{
    struct mqMMU_Data data;
    data.PPN = u >> 10;
    data.V = u >> 8;
    data.SZ1 = u >> 7;
    data.PR = u >> 5;
    data.SZ0 = u >> 4;
    data.C = u >> 3;
    data.D = u >> 2;
    data.SH = u >> 1;
    data.WT = u;
    return data;
}
