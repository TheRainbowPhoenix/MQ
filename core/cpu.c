//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/cpu.h>
#include <mq/memory.h>
#include <mq/machine.h>
#include <mq/modules/intc.h>
#include <mq/system/casiowin.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

MQ_LOG_REGISTER(MQ_LOG_INTERRUPT, "interrupt", 4, MQ_LOG_DEBUG, MQ_INT_NUM)

void mq_cpu_reset(mqCpu *cpu)
{
    memset(cpu, 0x00, sizeof *cpu);
}

void mq_cpu_makeObserver(mqCpu *ocpu, mqCpu const *cpu)
{
    memcpy(ocpu, cpu, sizeof *cpu);
}

void mq_cpu_cleanupObserver(mqCpu *ocpu)
{
    (void)ocpu;
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

static u32 read_PVR(MQ_UNUSED void *userdata)
{
    return 0x10300b00;
}

static u32 read_PRR(MQ_UNUSED void *userdata)
{
    return 0x00002c00;
}

static void write_CPUOPM(mqCpu *cpu, u32 value)
{
    cpu->CPUOPM = (value & 0x00000008) | 0x00000300;
}

static u16 PLCR_sh3_zero = 0;
static void write_PLCR_sh3(MQ_UNUSED void *userdata, MQ_UNUSED u32 value)
{
}

bool mq_cpu_setup(mqCpu *cpu, mqMemory *mem)
{
    // TODO: Area 7 addresses for MMIO?
    mqPage *pgff000 = mq_memory_getPage(mem, 0xff000000);
    mqPage *pgff2f0 = mq_memory_getPage(mem, 0xff2f0000);
    mqPage *pga4000 = mq_memory_getPage(mem, 0xa4000000);
    if(!pgff000 || !pgff2f0 || !pga4000)
        return false;

    bool b = true;
    b &= mq_page_mapRegister32(pgff000, "TRA", 0xff000020,
        NULL, write_TRA, &cpu->TRA, cpu);
    b &= mq_page_mapRegister32(pgff000, "EXPEVT", 0xff000024,
        NULL, write_EXPEVT, &cpu->EXPEVT, cpu);
    b &= mq_page_mapRegister32(pgff000, "INTEVT", 0xff000028,
        NULL, write_INTEVT, &cpu->INTEVT, cpu);
    b &= mq_page_mapRegister32(pgff000, "PVR", 0xff000030,
        read_PVR, NULL, NULL, NULL);
    b &= mq_page_mapRegister32(pgff000, "PRR", 0xff000044,
        read_PRR, NULL, NULL, NULL);
    b &= mq_page_mapRegister32(pgff2f0, "CPUOPM", 0xff2f0000,
        NULL, write_CPUOPM, &cpu->CPUOPM, cpu);

    /* Address of PLCR on SH7337 and SH7355. The register doesn't exist on the
       SH7305 but we don't want to print a warning if it gets accessed, since
       it's used for model detection. Make it available with value 0. */
    b &= mq_page_mapRegister16(pga4000, "PLCR_sh3", 0xa4000114,
        NULL, write_PLCR_sh3, &PLCR_sh3_zero, NULL);

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

void mq_cpu_handleException(mqMachine *mach, mqCpu *cpu)
{
    if(!cpu->excMask) {
        mq_log(MQ_LOG_ERROR, "handleException but there's no exception?!");
        return;
    }
    /* There shouldn't be a situation where we raise two exceptions before we
       handle the first one. */
    if(cpu->excMask & (cpu->excMask - 1)) {
        mq_log(MQ_LOG_WARNING, "simultaneous exceptions! %08x", cpu->excMask);
    }
    int exc = highestPriorityException(cpu->excMask);

    /* SR.BL double-faults if an exception occurs, but simply waits in the case
       of interrupts. However interrupts are already masked by the logic in
       updateIncomingInterrupt() if SR.BL=1, so we don't worry about it.
       TODO: Double fault logic when the exception is UBC? */
    // TODO: When sleeping, interrupts should be accepted even if SR.BL=1
    if(cpu->spRegs[SH_SR] & 0x10000000) {
        if(exc_isInterrupt(exc))
            mq_log(MQ_LOG_ERROR, "Handling interrupt while SR.BL=1?!");
        mq_log(MQ_LOG_ERROR, "Double fault!");
        mq_machine_setStuck(mach);
        return;
    }

    /* Break from sleep */
    cpu->sleeping = false;

    if(exc_isInterrupt(exc)) {
        mqINTC *INTC = mq_intc_get(mach);
        mq_intc_statsAcceptInterrupt(INTC, cpu->nextInterrupt);

        mq_logn(MQ_LOG_INTERRUPT + cpu->nextInterrupt,
            "Handling interrupt 0x%03x", cpu->INTEVT);
    }
    else {
        mq_log(MQ_LOG_DEBUG, "Handling exception %s",
            mq_cpu_exceptionName(exc));
    }

    cpu->spRegs[SH_SPC] =
        exc_isReexecutionType(exc) ? cpu->excPC : cpu->nextPC;
    cpu->spRegs[SH_SSR] = mq_cpu_getSR(cpu);
    cpu->spRegs[SH_SGR] = cpu->r[15];

    /* Set BL=1, RB=1, MD=1 */
    mq_cpu_setSystemSR(cpu, cpu->spRegs[SH_SR] | 0x70000000);

    if(exc_isInterrupt(exc)) {
        /* INTEVT and INTPRIO have been set before raising the interrupt. */
        if(cpu->CPUOPM & 0x00000008) {
            /* Set IMASK to the interrupt's priority level */
            u32 SystemSR = cpu->spRegs[SH_SR];
            SystemSR = (SystemSR & 0xffffff0f) + (cpu->INTPRIO << 4);
            mq_cpu_setSystemSR(cpu, SystemSR);
        }
    }
    else {
        cpu->EXPEVT = exc_exceptionCode(exc) & 0xfff;
    }

    cpu->pc = cpu->spRegs[SH_VBR] + exc_VBROffset(exc);

    if(exc == SH_EXC_BREAK_BEFORE || exc == SH_EXC_BREAK_AFTER)
        // TODO: Send UBC breaks to DBR if CBCR.UBDE = 1
        ; // cpu->pc = cpu->spRegs[DBR];
    else if(exc >= SH_EXC_POWERON_RESET && exc <= SH_EXC_TLB_DATA_MULTIHIT)
        cpu->pc = 0xa0000000;

    cpu->excMask &= ~(1 << exc);
}

void mq_cpu_raiseException(mqCpu *cpu, int exc, u32 value)
{
    mq_log(MQ_LOG_DEBUG, "[PC=%08x] Exception raised! %s (%08x)", cpu->pc,
        mq_cpu_exceptionName(exc), value);

    cpu->excPC = cpu->pc - 2 * cpu->inDelaySlot;
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

void mq_cpu_raiseException2(mqMachine *mach, mqCpu *cpu, int exc, u32 value)
{
    if(cpu->excMask) {
        mq_log(MQ_LOG_ERROR, "raiseAndHandleException: already an exception: "
            "%08x", cpu->excMask);
        mq_machine_setStuck(mach);
        return;
    }
    mq_cpu_raiseException(cpu, exc, value);
    mq_cpu_handleException(mach, cpu);
}

static void updateIncomingInterrupt(mqCpu *cpu)
{
    u32 SystemSR = cpu->spRegs[SH_SR];
    u32 BL = (SystemSR >> 28) & 1;
    int IMASK = (SystemSR >> 4) & 0xf;

    if(BL || !cpu->nextInterruptINTEVT || cpu->nextInterruptPriority <= IMASK) {
        cpu->excMask &= ~(1 << SH_EXC_INTERRUPT);
    }
    else {
        // mq_log(MQ_LOG_DEBUG, "Interrupt raised! 0x%03x (prio=%d)",
        //     cpu->nextInterruptINTEVT, cpu->nextInterruptPriority);
        cpu->excPC = cpu->pc;
        cpu->excMask |= (1 << SH_EXC_INTERRUPT);
        cpu->INTEVT = cpu->nextInterruptINTEVT;
        cpu->INTPRIO = cpu->nextInterruptPriority;
    }
}

MQ_INLINE void mq_cpu_checkDSPLoop(mqCpu *cpu)
{
    /* Check if this is the last instruction in a repeat control loop. */
    // TODO: DSP loop may expose wrong value of RC, RE & 1 during end inst.
    // RC should be decremented *after* the repeat end instruction. (To fix
    // this, generate the correct value of RC dynamically when reading SR.)
    if(MQ_UNLIKELY(cpu->pc == cpu->dspLoopPC)) {
        cpu->RC--;
        /* Setup the next loop iteration */
        if(cpu->RC)
            cpu->nextPC = cpu->spRegs[SH_RS];
        else
            cpu->dspLoopPC = -1;
    }
}

MQ_INLINE void mq_cpu_cycle_aux(mqMachine *mach, mqCpu *cpu)
{
    if(MQ_UNLIKELY(cpu->sleeping))
        return;

    /* Fetch the next instruction. */
    u32 ins = mq_memory_read_opcode(mach->memory, cpu->pc);
    if(MQ_LIKELY(ins != 0)) {
        cpu->nextPC = cpu->pc + 2;
        mq_cpu_checkDSPLoop(cpu);

        /* Decode and execute the instruction. */
        TracyCZoneN(_ctx, "exec", mach->profilingCycles);
        _mq_cpu_execute(mach, cpu, ins);
        TracyCZoneEnd(_ctx);
    }
    /* Only check for the syscall handler if the read fails. This means we can
       only emulate syscalls if we don't map the syscall stub. If we do map it,
       then we can always set it to jump somewhere we don't and set the syscall
       handler to that address. */
    else if(cpu->pc == cpu->syscallHandler && cpu->pc) {
        mq_casiowin_syscall(mach);
        /* If a background syscall, execution longjmps back to controller. */
        return;
    }
    else {
        return mq_cpu_raiseException2(mach, cpu, SH_EXC_INS_ADDR, cpu->pc);
    }

    /* Group the execution of delayed branches and their delay slots. */
    if(MQ_LIKELY(!cpu->inDelaySlot))
        return;

    /* Delayed branch instruction don't increment PC, which is not needed: all
       PC-dependent instructions are forbidden as delay slots. */
    u32 ins2 = mq_memory_read_opcode(mach->memory, cpu->pc + 2);
    if(MQ_LIKELY(ins2 != 0)) {
        mq_cpu_checkDSPLoop(cpu);

        TracyCZoneN(_ctx, "delay_slot", mach->profilingCycles);
        _mq_cpu_execute(mach, cpu, ins2);
        TracyCZoneEnd(_ctx);
        cpu->inDelaySlot = false;

        /* Handle interrupts from rte which have to wait until after the delay
           slot is executed. */
        if(MQ_UNLIKELY(ins == 0x002b /* rte */) && cpu->excMask)
            mq_cpu_handleException(mach, cpu);

        /* Handle execution break request */
        if(mach->internallyBlocked)
            mq_machine_breakExecution(mach);
    }
    else {
        return mq_cpu_raiseException2(mach, cpu, SH_EXC_INS_ADDR, cpu->pc + 2);
    }
}

void mq_cpu_cycle(mqMachine *mach, mqCpu *cpu)
{
    TracyCZoneN(_ctx, "cpu", mach->profilingCycles);
    mq_cpu_cycle_aux(mach, cpu);
    TracyCZoneEnd(_ctx);
}

void mq_cpu_sleep(mqCpu *cpu)
{
    cpu->sleeping = true;
}

u32 mq_cpu_getSR(mqCpu *cpu)
{
    return cpu->spRegs[SH_SR]
           | (cpu->T << 0) | (cpu->S << 1) | (cpu->RF << 2) | (cpu->Q << 8)
           | (cpu->M << 9) | (cpu->DMX << 10) | (cpu->DMY << 11)
           | (cpu->DSP << 12) | (cpu->RC << 16);
}

void mq_cpu_setSR(mqCpu *cpu, u32 SR)
{
    cpu->T = SR & 1;
    cpu->S = (SR >> 1) & 1;
    cpu->RF = (SR >> 2) & 3;
    cpu->Q = (SR >> 8) & 1;
    cpu->M = (SR >> 9) & 1;
    cpu->DMX = (SR >> 10) & 1;
    cpu->DMY = (SR >> 11) & 1;
    cpu->DSP = (SR >> 12) & 1;
    cpu->RC = (SR >> 16) & 0xfff;

    // TODO: mq_cpu_setSR: Block the DSP loop?
    mq_cpu_setSystemSR(cpu, SR & 0xf00000f0);
}

void mq_cpu_setSystemSR(mqCpu *cpu, u32 SystemSR)
{
    // TODO[cpu]: SR mask depends on processor type (SH3 vs. SH4AL-DSP)
    SystemSR &= 0x700000f0;

    /* Swap register banks if we change the value of RB */
    if((SystemSR ^ cpu->spRegs[SH_SR]) & 0x20000000) {
        for(int i = 0; i < 8; i++) {
            u32 tmp = cpu->r[i];
            cpu->r[i] = cpu->spRegs[SH_RnBANK + i];
            cpu->spRegs[SH_RnBANK + i] = tmp;
        }
    }

    /* Update the incoming interrupt logic if we change BL or IMASK */
    bool updateInterrupt = ((SystemSR ^ cpu->spRegs[SH_SR]) & 0x100000f0);

    cpu->spRegs[SH_SR] = SystemSR;

    if(updateInterrupt)
        updateIncomingInterrupt(cpu);
}

void mq_cpu_setIncomingInterrupt(
    mqCpu *cpu, int interrupt, u32 INTEVT, int priority)
{
    cpu->nextInterrupt = interrupt;
    cpu->nextInterruptINTEVT = INTEVT;
    cpu->nextInterruptPriority = priority;
    updateIncomingInterrupt(cpu);
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

int mq_cpu_exceptionForEventCode(int EXPEVT)
{
    for(int i = 0; i < SH_NUM_EXCEPTIONS; i++) {
        if(EXPEVT == exc_CodeTable[i])
            return i;
    }
    return -1;
}
