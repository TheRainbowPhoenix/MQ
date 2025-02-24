//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.modules.mmu: Memory Management Interface
// Reference -- SH4AL-DSP Manual, Section 7
//   https://bible.planet-casio.com/common/hardware/mpu/sh4aldsp_manual.pdf
//
// Emulating the MMU is a little bit tricky. MQ tries pretty hard to not have
// an address translation step at access time because reads are already a very
// hot spot. So the entire design revolves around "inlining" the MMU address
// translation by copying the memory configuration of the targeted physical
// addresses at the chosen virtual addresses. The bindings can be reloaded
// dynamically whenever the UTLB or ASIC change, which is expected to be rare
// compared to accesses.
//
// There are currently rough edges:
// - Mapping 1-kB pages is not supported because the smallest mqMemory buffer
//   unit available is 4 kB so a special MMIO is required, which is TO-DO.
// - Mapping 4-kB or 64-kB pages to portions of buffer chunks is TO-DO as well.
// - Mapping to MMIO is unlikely to ever be supported.
// - The dirty bit (D) is not set. This requires keeping track of which UTLB
//   entries (if any) buffer chunks and pages map back to.
// - The protection bits (PR) are not honored. This requires more setup on
//   mqMemory to test writeability, and because only one bit is available, it
//   also requires an update to the mqMemory when we change SR.BL. This should
//   be possible with a traversal without unbinding/binding the MMU.
//---

#ifndef MQ_MODULES_MMU_H
#define MQ_MODULES_MMU_H

#include <mq/machine.h>
#include <mq/interfaces/keyboard.h>
MQ_START_DEFS

struct mqMMU {
    u32 PTEH, PTEL;
    u32 TTB;
    u32 TEA;
    u32 PASCR;
    u32 MMUCR;
    u32 IRMCR;

    // Current UTLB contents. If changed dynamically, this has to be kept in
    // sync with the actualy memory bindings of the machine!
    struct { u32 addr, data; } UTLB[64];
};

typedef struct mqMMU mqMMU;

/* Setup the MMU module for a given machine. */
bool mq_mmu_setup(mqMachine *mach);

/* Get the MMU module info of a machine, NULL if the module is not used. */
mqMMU *mq_mmu_get(mqMachine *mach);

/* Quick mapping function for simple setups. Maps the given regions to the UTLB
   interval starting at the given index with cache, non-shared, user read-write
   permissions. This only fills the TLB, to update actual memory mappings use
   mq_mmu_bind(). */
bool mq_mmu_map(
    mqMachine *mach, u32 VPN, u32 PPN, int index, int pageSize, int pageCount);

/* Create MMU bindings in the machine's memory based on the MMU module's
   configuration. Is a no-op if there is no MMU. The MMU regions are assumed to
   be wiped, which can be achieved with mq_mmu_unbind(). */
void mq_mmu_bind(mqMachine *mach);
/* Remove existing bindings in MMU range from the machine's memory. This is a
   no-op if there is no MMU module in the machine (in which case such bindings
   are likely intended to be permanent). */
void mq_mmu_unbind(mqMachine *mach);

//=== Internal MMU structures ================================================//

struct mqMMU_Address {
    u32 VPN     :22;    // Virtual Page Number
    u32 D       :1;     // Dirty (always 0 for ITLB)
    u32 V       :1;     // Valid
    u32 ASID    :8;     // Address Space IDentifier
};

struct mqMMU_Data {
    u32         :3;
    u32 PPN     :19;    // Physical Page Number
    u32         :1;
    u32 V       :1;     // Valid
    u32 SZ1     :1;     // SiZe (bit #1)
    u32 PR      :2;     // PRotection (only bit #1 matters for ITLB)
    u32 SZ0     :1;     // SiZe (bit #0)
    u32 C       :1;     // Cacheable
    u32 D       :1;     // Dirty (always 0 for ITLB)
    u32 SH      :1;     // SHared
    u32 WT      :1;     // Write Through (always 0 for ITLB)
};

/* Basic safety checks in case another compiler is used. */
MQ_STATIC_ASSERT(sizeof(struct mqMMU_Address) == 4);
MQ_STATIC_ASSERT(sizeof(struct mqMMU_Data) == 4);

/* Cast to and from u32. */
u32 mq_mmu_encode_address(struct mqMMU_Address addr);
u32 mq_mmu_encode_data(struct mqMMU_Data data);
struct mqMMU_Address mq_mmu_decode_address(u32 addr);
struct mqMMU_Data mq_mmu_decode_data(u32 data);

MQ_END_DEFS
#endif /* MQ_MODULES_MMU_H */
