//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/cpu.h>
#include <mq/memory.h>
#include <mq/machine.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void mq_cpu_reset(mqCpu *cpu)
{
    memset(cpu, 0x00, sizeof *cpu);
}

void mq_cpu_initialize(mqCpu *cpu, int initializeKind)
{
    mq_cpu_reset(cpu);

    if(initializeKind == MQ_CPU_INITIALIZE_POWERON) {
        cpu->spRegs[SH_SR] = 0x700000f0; // MD=1 RB=1 BL=1 IMASK=15
        cpu->spRegs[SH_VBR] = 0x00000000;
        cpu->spRegs[SH_DSR] = 0x0000;
        cpu->pc = 0xa0000000;
        cpu->syscallHandler = 0;
    }
    else if(initializeKind == MQ_CPU_INITIALIZE_ADDIN_FX) {
        cpu->spRegs[SH_SR] = 0x40000000; // MD=1
        cpu->r[4] = 0; // isAppli
        cpu->r[5] = 0; // optNum
        // TODO: Initial r15
        cpu->pc = 0x00300200;
        cpu->syscallHandler = 0x80010070;
    }
    else if(initializeKind == MQ_CPU_INITIALIZE_ADDIN_CG) {
        cpu->spRegs[SH_SR] = 0x40000000; // MD=1
        cpu->r[4] = 0; // isAppli
        cpu->r[5] = 0; // optNum
        // TODO: Initial r15 directly in P1
        cpu->r[15] = 0x08100000 + (512 << 10);
        cpu->pc = 0x00300000;
        cpu->syscallHandler = 0x80020070;
    }

    /* MPU registers are initialized here (most to 0). */
}

static void write_TRA(mqCpu *cpu, u32 value)
{
    cpu->TRA = value & 0x000003fc;
}

static void write_EXPEVT(mqCpu *cpu, u32 value)
{
    cpu->EXPEVT = value & 0x00000fff;
}

static void write_INTEVT(mqCpu *cpu, u32 value)
{
    cpu->INTEVT = value & 0x00003fff;
}

static u32 read_PVR(void)
{
    return 0x10300b00;
}

static u32 read_PRR(void)
{
    return 0x00002c00;
}

bool mq_cpu_setupModule(mqCpu *cpu, mqMemory *mem)
{
    // TODO: Area 7 addresses for MMIO?
    mqChunk *ch = mq_memory_createChunk(mem, 0xff000000);
    if(!ch)
        return false;
    mqMMIOPage *mmpg = mq_chunk_createMMIOPage(ch, 0xff000000, 0, 0);
    if(!mmpg)
        return false;

    bool b = true;
    b &= mq_page_mapRegister32(mmpg, "TRA", 0xff000020,
        NULL, write_TRA, &cpu->TRA, cpu);
    b &= mq_page_mapRegister32(mmpg, "EXPEVT", 0xff000024,
        NULL, write_EXPEVT, &cpu->EXPEVT, cpu);
    b &= mq_page_mapRegister32(mmpg, "INTEVT", 0xff000028,
        NULL, write_INTEVT, &cpu->INTEVT, cpu);
    b &= mq_page_mapRegister32(mmpg, "PVR", 0xff000030,
        read_PVR, NULL, NULL, NULL);
    b &= mq_page_mapRegister32(mmpg, "PRR", 0xff000044,
        read_PRR, NULL, NULL, NULL);
    return b;
}

static u16 const exc_CodeTable[SH_NUM_EXCEPTIONS] = {
    0x000, 0x000, 0x020, 0x140, 0x140, 0x1e0, 0x0e0, 0x040, 0x0a0, 0x180,
    0x1a0, 0x160, 0x0e0, 0x100, 0x040, 0x060, 0x0a0, 0x0c0, 0x080, 0x1e0,
    0x1c0, 0xfff,
};

static u32 exc_exceptionCode(int exc)
{
    return (uint)exc >= SH_NUM_EXCEPTIONS ? 0xfff : exc_CodeTable[exc];
}

static u32 exc_VBROffset(int exc)
{
    if(exc >= SH_EXC_NMI)
        return 0x600;
    if(exc == SH_EXC_INS_TLBMISS || exc == SH_EXC_READ_TLBMISS ||
       exc == SH_EXC_WRITE_TLBMISS)
        return 0x400;
    return 0x100;
}

static bool exc_isReexecutionType(int exc)
{
    if(exc == SH_EXC_INS_TLBMISS || exc == SH_EXC_READ_TLBMISS ||
       exc == SH_EXC_WRITE_TLBMISS)
        return true;

    return exc >= SH_EXC_BREAK_BEFORE && exc <= SH_EXC_INITIAL_PAGE_WRITE
           && exc != SH_EXC_TRAP;
}

static bool exc_isInterrupt(int exc)
{
    return exc >= SH_EXC_NMI;
}

/* Find the highest exception priority in the mask; -1 if excMask == 0. */
static int highestPriorityException(u32 excMask)
{
    for(int i = 0; i < 32; i++) {
        if(excMask & (1 << i))
            return i;
    }
    return -1;
}

static bool handleException(mqCpu *cpu, int exc, u32 previousPC)
{
    // TODO: Does SR.BL=1 double fault or simply wait to raise the exception?
    if(cpu->spRegs[SH_SR] & 0x10000000) {
        mq_log(MQ_LOG_ERROR, "Double fault!");
        return false;
    }

    if(exc_isInterrupt(exc)) {
        // TODO: Set INTEVT. Requires INTC providing the interrupt code.
    }
    else {
        cpu->EXPEVT = exc_exceptionCode(exc) & 0xfff;
    }

    // TODO: Break from sleep

    mq_log(MQ_LOG_DEBUG, "Handling exception %s", mq_cpu_exceptionName(exc));

    cpu->spRegs[SH_SPC] = exc_isReexecutionType(exc) ? previousPC : cpu->pc;
    cpu->spRegs[SH_SSR] = cpu->spRegs[SH_SR];
    cpu->spRegs[SH_SGR] = cpu->r[15];

    /* Set BL=1 */
    cpu->spRegs[SH_SR] |= 0x10000000;
    // TODO: Set BL=1 MD=1

    cpu->pc = cpu->spRegs[SH_VBR] + exc_VBROffset(exc);

    if(exc == SH_EXC_BREAK_BEFORE || exc == SH_EXC_BREAK_AFTER)
        // TODO: Send UBC breaks to DBR if CBCR.UBDE = 1
        ; // cpu->pc = cpu->spRegs[DBR];
    else if(exc >= SH_EXC_POWERON_RESET && exc <= SH_EXC_TLB_DATA_MULTIHIT)
        cpu->pc = 0xa0000000;

    cpu->excMask &= ~(1 << exc);
    return true;
}

void mq_cpu_raiseException(mqCpu *cpu, int exc, u32 value)
{
    mq_log(MQ_LOG_DEBUG, "[PC=%08x] Exception raised! %s (%08x)", cpu->pc,
        mq_cpu_exceptionName(exc), value);

    cpu->excMask |= (1 << exc);
    if(exc == SH_EXC_INS_ADDR
       || exc == SH_EXC_INS_TLBMISS
       || exc == SH_EXC_INS_TLBPROT
       || exc == SH_EXC_READ_ADDR
       || exc == SH_EXC_READ_TLBMISS
       || exc == SH_EXC_READ_TLBPROT
       || exc == SH_EXC_WRITE_ADDR
       || exc == SH_EXC_WRITE_TLBMISS
       || exc == SH_EXC_WRITE_TLBPROT)
        ; // TODO: TEA = value;
    else if(exc == SH_EXC_TRAP)
        cpu->TRA = (value & 0xff) << 2;
}

bool mq_cpu_raiseException_false(mqCpu *cpu, int exc, u32 value)
{
    mq_cpu_raiseException(cpu, exc, value);
    return false;
}

void mq_cpu_cycle(struct mqMachine *mach, mqCpu *cpu)
{
    u32 previousPC = cpu->pc;

    // printf("Cycle: pc=%08x\n", cpu->pc);

    /* Fetch the next instruction. */
    // TODO: Same-basic-block prefetching optimization.
    u32 ins = mq_memory_read_opcode(cpu, mach->memory, cpu->pc);
    if(MQ_LIKELY(ins != 0)) {
        // printf("  -> ins=%04x\n", ins);
        /* Decode and execute the instruction. */
        _mq_cpu_execute(mach, cpu, ins);
    }
    /* Only check for the syscall handler if the read fails. This means we can
       only emulate syscalls if we don't map the syscall stub. If we do map it,
       then we can always set it to jump somewhere we don't and set the syscall
       handler to that address. */
    else if(cpu->pc == cpu->syscallHandler && cpu->pc) {
        mq_mach_syscall(mach);
        goto endCycle;
    }
    else {
        mq_cpu_raiseException_false(cpu, SH_EXC_INS_ADDR, cpu->pc);
    }

    /* In case of a delay slot, continue. */
    if(MQ_UNLIKELY(cpu->inDelaySlot)) {
        ins = mq_memory_read_opcode(cpu, mach->memory, cpu->pc);
        if(MQ_LIKELY(ins != 0)) {
            // printf("  -> delay ins=%04x\n", ins);
            _mq_cpu_execute(mach, cpu, ins);
            cpu->pc = cpu->delaySlotTarget;
            cpu->inDelaySlot = false;
        }
        else {
            // TODO: Should that be illegal slot?
            mq_cpu_raiseException_false(cpu, SH_EXC_INS_ADDR, cpu->pc);
        }
    }

endCycle:
    /* Check for exceptions or interrupts. This is done *after* running the
       instruction because some exceptions are re-execution type. */
    if(MQ_UNLIKELY(cpu->excMask)) {
        int exc = highestPriorityException(cpu->excMask);
        if(!handleException(cpu, exc, previousPC))
            mach->stuck = true;
    }
}

char const *mq_cpu_specialRegisterName(int spReg)
{
    static char const spreg_names[SH_NUM_SPECIAL_REGS][8] = {
        "sr",       "gbr",      "vbr",      "ssr",
        "spc",      "mod",      "rs",       "re",
        "r0_bank",  "r1_bank",  "r2_bank",  "r3_bank",
        "r4_bank",  "r5_bank",  "r6_bank",  "r7_bank",
        "mach",     "macl",     "pr",       "sgr",
        "",         "",         "dsr",      "a0",
        "x0",       "x1",       "y0",       "y1",
        "",         "",         "",         "dbr",
        "a0g",      "a1",       "a1g",      "m0",
        "m1",
    };

    char const *name =
        ((uint)spReg < SH_NUM_SPECIAL_REGS) ? spreg_names[spReg] : NULL;
    return name && *name ? name : NULL;
}

char const *mq_cpu_exceptionName(int exc)
{
    char const *exc_names[SH_NUM_EXCEPTIONS] = {
        "POWERON_RESET",
        "HUDI_RESET",
        "MANUAL_RESET",
        "TLB_INS_MULTIHIT",
        "TLB_DATA_MULTIHIT",
        "BREAK_BEFORE",
        "INS_ADDR",
        "INS_TLBMISS",
        "INS_TLBPROT",
        "ILLEGAL",
        "ILLEGAL_SLOT",
        "TRAP",
        "READ_ADDR",
        "WRITE_ADDR",
        "READ_TLBMISS",
        "WRITE_TLBMISS",
        "READ_TLBPROT",
        "WRITE_TLBPROT",
        "INITIAL_PAGE_WRITE",
        "BREAK_AFTER",
        "NMI",
        "INTERRUPT",
    };

    return ((uint)exc < SH_NUM_EXCEPTIONS) ? exc_names[exc] : NULL;
}
