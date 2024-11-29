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

void mq_cpu_initialize(mqCpu *cpu, int initializeKind)
{
    memset(cpu, 0x00, sizeof *cpu);

    if(initializeKind == MQ_CPU_INITIALIZE_POWERON) {
        cpu->spRegs[SH_SR] = 0x700000f0; // MD=1 RB=1 BL=1 IMASK=15
        cpu->spRegs[SH_VBR] = 0x00000000;
        cpu->spRegs[SH_DSR] = 0x0000;
        cpu->pc = 0xa0000000;
    }
    else if(initializeKind == MQ_CPU_INITIALIZE_ADDIN_FX) {
        cpu->spRegs[SH_SR] = 0x40000000; // MD=1
        cpu->gpRegs[4] = 0; // isAppli
        cpu->gpRegs[5] = 0; // optNum
        // TODO: Initial r15
        cpu->pc = 0x00300200;
    }
    else if(initializeKind == MQ_CPU_INITIALIZE_ADDIN_CG) {
        cpu->spRegs[SH_SR] = 0x40000000; // MD=1
        cpu->gpRegs[4] = 0; // isAppli
        cpu->gpRegs[5] = 0; // optNum
        // TODO: Initial r15
        cpu->pc = 0x00300000;
    }
}

static u16 const exc_CodeTable[SH_NUM_EXCEPTIONS] = {
    0x000, 0x000, 0x020, 0x140, 0x140, 0x1e0, 0x0e0, 0x040, 0x0a0, 0x180,
    0x1a0, 0x160, 0x0e0, 0x100, 0x040, 0x060, 0x0a0, 0x0c0, 0x080, 0x1e0,
    0x1c0, 0xfff,
};

static u32 exc_ExceptionCode(int exc)
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

static void handleException(mqCpu *cpu, int exc, u32 previousPC)
{
    // TODO: Does SR.BL=1 double fault or simply wait to raise the exception?

    // TODO: Set EXPEVT (if exception), otherwise set INTEVT.
    // TODO: Requires interface for INTC to provide interrupt code.

    cpu->spRegs[SH_SPC] = exc_isReexecutionType(exc) ? previousPC : cpu->pc;
    cpu->spRegs[SH_SSR] = cpu->spRegs[SH_SR];
    cpu->spRegs[SH_SGR] = cpu->gpRegs[15];

    cpu->pc = cpu->spRegs[SH_VBR] + exc_VBROffset(exc);

    if(exc == SH_EXC_BREAK_BEFORE || exc == SH_EXC_BREAK_AFTER)
        // TODO: Send UBC breaks to DBR if CBCR.UBDE = 1
        ; // cpu->pc = cpu->spRegs[DBR];
    if(exc >= SH_EXC_POWERON_RESET && exc <= SH_EXC_TLB_DATA_MULTIHIT)
        cpu->pc = 0xa0000000;

    cpu->excMask &= ~(1 << exc);
}

void mq_cpu_raiseException(mqCpu *cpu, int exc, u32 value)
{
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
        ; // TODO: TRA = value << 2
}

#include <stdio.h>

void mq_cpu_cycle(struct mqMachine *mach, mqCpu *cpu)
{
    u32 previousPC = cpu->pc;

    /* Fetch the next instruction. */
    // TODO: Same-basic-block prefetching optimization.
    u32 ins = mq_memory_read32(cpu, mach->memory, cpu->pc);
    fprintf(stderr, "nope, read16\n");
    abort();

    /* Decode and execute the instruction. */
    _mq_cpu_execute(mach, cpu, ins);

    /* Check for exceptions or interrupts. This is done *after* running the
       instruction because some exceptions are re-execution type. */
    if(cpu->excMask) {
        int exc = highestPriorityException(cpu->excMask);
        handleException(cpu, exc, previousPC);
    }
}

char const *mq_cpu_spreg_name(int spReg)
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
