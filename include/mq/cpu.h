//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.cpu: Processor state and execution model
//---

#ifndef MQ_CPU_H
#define MQ_CPU_H

#include <mq/defs.h>
MQ_START_DEFS

struct mqMachine;

/* Control and system registers. The numbering for 0..31 follows the encoding
   of lds/ldc instructions, 32 and above are in a random order. */
enum {
    SH_SR      = 0,
    SH_GBR     = 1,
    SH_VBR     = 2,
    SH_SSR     = 3,
    SH_SPC     = 4,
    SH_MOD     = 5,   // !dsp
    SH_RS      = 6,   // !dsp
    SH_RE      = 7,   // !dsp
    SH_RnBANK  = 8,
    // 8..15 occupied by SH_RnBANK+i (0 ≤ i < 8)

    SH_MACH    = 16,
    SH_MACL    = 17,
    SH_PR      = 18,
    SH_SGR     = 19,  // !sh4
    // 20, 21 unused
    SH_DSR     = 22,  // !dsp
    SH_A0      = 23,  // !dsp
    SH_X0      = 24,  // !dsp
    SH_X1      = 25,  // !dsp
    SH_Y0      = 26,  // !dsp
    SH_Y1      = 27,  // !dsp
    // 28..30 unused
    SH_DBR     = 31,  // !sh4

    SH_A0G     = 32,  // !dsp (8-bit)
    SH_A1      = 33,  // !dsp
    SH_A1G     = 34,  // !dsp (8-bit)
    SH_M0      = 35,  // !dsp
    SH_M1      = 36,  // !dsp

    SH_NUM_SPECIAL_REGS,
};

/* List of exception codes defined by the CPU.
   WARNING: The order within this list enforces a lot of structures, including
            for VBR offsets and priority. */
enum {
    /* Resets */
    SH_EXC_POWERON_RESET,
    SH_EXC_HUDI_RESET,
    SH_EXC_MANUAL_RESET,
    SH_EXC_TLB_INS_MULTIHIT,
    SH_EXC_TLB_DATA_MULTIHIT,
    /* General exceptions */
    SH_EXC_BREAK_BEFORE,
    SH_EXC_INS_ADDR,
    SH_EXC_INS_TLBMISS,
    SH_EXC_INS_TLBPROT,
    SH_EXC_ILLEGAL,
    SH_EXC_ILLEGAL_SLOT,
    SH_EXC_TRAP,
    SH_EXC_READ_ADDR,
    SH_EXC_WRITE_ADDR,
    SH_EXC_READ_TLBMISS,
    SH_EXC_WRITE_TLBMISS,
    SH_EXC_READ_TLBPROT,
    SH_EXC_WRITE_TLBPROT,
    SH_EXC_INITIAL_PAGE_WRITE,
    SH_EXC_BREAK_AFTER,
    /* Interrupts */
    SH_EXC_NMI,
    SH_EXC_INTERRUPT,

    SH_NUM_EXCEPTIONS,
};
MQ_STATIC_ASSERT(SH_NUM_EXCEPTIONS <= 32, "Too many exceptions for u32 mask");

struct mqCpu
{
    /* Registers */
    u32 r[16];  // r0..r15
    u32 spRegs[SH_NUM_SPECIAL_REGS];

    /* Control flow: PC, whether the next instruction should be executed as a
       delay slot, and the target to jump to after said delay slot. */
    u32 pc;
    bool inDelaySlot;
    u32 delaySlotTarget;

    /* Mask of pending exceptions */
    u32 excMask;

    /* Syscall emulation address. If this address is hit a syscall will be
       emulated. This is set to 0 when syscall emulation is disabled */
    u32 syscallHandler;
};

typedef struct mqCpu mqCpu;

//=== CPU construction and initialization ====================================//

enum {
    MQ_CPU_INITIALIZE_POWERON,
    MQ_CPU_INITIALIZE_ADDIN_FX,
    MQ_CPU_INITIALIZE_ADDIN_CG,
};

void mq_cpu_initialize(mqCpu *cpu, int initializeKind);

//=== Emulation routines =====================================================//

/* Raise an exception. The exception will be handled at the next appropriate
   time. The meaning of `value` depends on the exception type:
   - SH_EXC_{INS,READ,WRITE}_{ADDR,TLBMISS,TLBPROT}: Offending address.
   - SH_EXC_TRAP: Trap number.
   - Other exceptions: ignored, specify 0. */
void mq_cpu_raiseException(mqCpu *cpu, int exc, u32 value);

/* Raise an exception and return false. This is used to reduce code size by
   making exception paths terminal calls, notably in memory access code. */
bool mq_cpu_raiseException_false(mqCpu *cpu, int exc, u32 value);

void mq_cpu_cycle(struct mqMachine *mach, mqCpu *cpu);

void _mq_cpu_execute(struct mqMachine *mach, mqCpu *cpu, u16 opcode);

/* Set the value of the T bit; the value provided must be 0 or 1. */
MQ_INLINE void mq_cpu_setT(mqCpu *cpu, int T)
{
    cpu->spRegs[SH_SR] = (cpu->spRegs[SH_SR] & -2) + T;
}
/* Get the value of the T bit. */
MQ_INLINE int mq_cpu_getT(mqCpu *cpu)
{
    return cpu->spRegs[SH_SR] & 1;
}

/* Set a delay slot with the given target destination. */
MQ_INLINE void mq_cpu_setDelaySlot(mqCpu *cpu, u32 targetAddress)
{
    cpu->inDelaySlot = true;
    cpu->delaySlotTarget = targetAddress;
    cpu->pc += 2;
}
/* Check if the current instructions is running in a delay slot. This is only
   for instruction emulation functions. */
MQ_INLINE bool mq_cpu_inDelaySlot(mqCpu *cpu)
{
    return cpu->inDelaySlot;
}

//=== Misc. information ======================================================//

/* Lowercase name of a special register. */
char const *mq_cpu_specialRegisterName(int spReg);

/* Enumeration name of an exception number. */
char const *mq_cpu_exceptionName(int exc);

MQ_END_DEFS
#endif /* MQ_CPU_H */
