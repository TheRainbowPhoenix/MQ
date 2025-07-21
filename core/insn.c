//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.insn: Emulation code for SuperH instructions

#include <mq/machine.h>
#include <mq/memory.h>

#include <stdio.h>
#include <stdlib.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// TODO[insn]: None of the privileged instructions check for SR.MD yet!

/* addc/subc builtins aren't available on old-ish versions of GCC. */
#define ADDC(_A, _B, _CARRY_IN, _CARRY_OUT) \
    ({ __typeof__(_A) _s; \
       __typeof__(_A) _c1 = __builtin_add_overflow(_A, _B, &_s); \
       __typeof__(_A) _c2 = __builtin_add_overflow(_s, _CARRY_IN, &_s); \
       _CARRY_OUT = _c1 | _c2; \
       _s; })
#define SUBC(_A, _B, _CARRY_IN, _CARRY_OUT) \
    ({ __typeof__(_A) _s; \
       __typeof__(_A) _c1 = __builtin_sub_overflow(_A, _B, &_s); \
       __typeof__(_A) _c2 = __builtin_sub_overflow(_s, _CARRY_IN, &_s); \
       _CARRY_OUT = _c1 | _c2; \
       _s; })

// TODO[insn]: Assumption: @-rn/@rn+ will only inc/decrement if access succeeds
// TODO[insn]: Raise exceptions for privileged instructions while in user mode

MQ_INLINE void mov_imm(mqMachine *mach, mqCpu *cpu, int n, int imm) {
    /* mov #imm, rn */
    cpu->r[n] = (i8)imm;
    cpu->pc += 2;
}
MQ_INLINE void mov(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov rm, rn */
    cpu->r[n] = cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void nop(mqMachine *mach, mqCpu *cpu) {
    /* nop */
    cpu->pc += 2;
}

MQ_INLINE void add_imm(mqMachine *mach, mqCpu *cpu, int n, int imm) {
    /* add #imm, rn */
    cpu->r[n] += (i8)imm;
    cpu->pc += 2;
}
MQ_INLINE void add(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* add rm, rn */
    cpu->r[n] += cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void addc(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* addc rm, rn */
    uint T_out;
    cpu->r[n] = ADDC(cpu->r[n], cpu->r[m], mq_cpu_getT(cpu), T_out);
    mq_cpu_setT(cpu, T_out);
    cpu->pc += 2;
}
MQ_INLINE void addv(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* addv rm, rn */
    bool v = __builtin_add_overflow(cpu->r[m], cpu->r[n], &cpu->r[n]);
    mq_cpu_setT(cpu, v);
    cpu->pc += 2;
}
MQ_INLINE void sub(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* sub rm, rn */
    cpu->r[n] -= cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void subc(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* subc rm, rn */
    uint T_out;
    cpu->r[n] = SUBC(cpu->r[n], cpu->r[m], mq_cpu_getT(cpu), T_out);
    mq_cpu_setT(cpu, T_out);
    cpu->pc += 2;
}
MQ_INLINE void subv(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* subv rm, rn */
    bool v = __builtin_sub_overflow(cpu->r[n], cpu->r[m], &cpu->r[n]);
    mq_cpu_setT(cpu, v);
    cpu->pc += 2;
}
MQ_INLINE void neg(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* neg rm, rn */
    cpu->r[n] = -cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void negc(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* negc rm, rn */
    uint T_out;
    cpu->r[n] = SUBC(0, cpu->r[m], mq_cpu_getT(cpu), T_out);
    mq_cpu_setT(cpu, T_out);
    cpu->pc += 2;
}

MQ_INLINE void tst(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* tst rm, rn */
    mq_cpu_setT(cpu, (cpu->r[n] & cpu->r[m]) == 0);
    cpu->pc += 2;
}
MQ_INLINE void tst_imm_r0(mqMachine *mach, mqCpu *cpu, int imm) {
    /* tst #imm, r0 */
    mq_cpu_setT(cpu, (cpu->r[0] & imm) == 0);
    cpu->pc += 2;
}
MQ_INLINE void and(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* and rm, rn */
    cpu->r[n] &= cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void and_imm_r0(mqMachine *mach, mqCpu *cpu, int imm) {
    /* and #imm, r0 */
    cpu->r[0] &= imm;
    cpu->pc += 2;
}
MQ_INLINE void xor(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* xor rm, rn */
    cpu->r[n] ^= cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void xor_imm_r0(mqMachine *mach, mqCpu *cpu, int imm) {
    /* xor #imm, r0 */
    cpu->r[0] ^= imm;
    cpu->pc += 2;
}
MQ_INLINE void or(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* or rm, rn */
    cpu->r[n] |= cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void or_imm_r0(mqMachine *mach, mqCpu *cpu, int imm) {
    /* or #imm, r0 */
    cpu->r[0] |= imm;
    cpu->pc += 2;
}
MQ_INLINE void not(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* not rm, rn */
    cpu->r[n] = ~cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void swapb(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* swap.b rm, rn */
    u32 x = cpu->r[m];
    cpu->r[n] = (x & 0xffff0000) + (u16)(x << 8) + (u8)(x >> 8);
    cpu->pc += 2;
}
MQ_INLINE void swapw(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* swap.w rm, rn */
    cpu->r[n] = (cpu->r[m] << 16) + (cpu->r[m] >> 16);
    cpu->pc += 2;
}

MQ_INLINE void dt(mqMachine *mach, mqCpu *cpu, int n) {
    /* dt rn */
    cpu->r[n]--;
    mq_cpu_setT(cpu, cpu->r[n] == 0);
    cpu->pc += 2;
}
MQ_INLINE void cmp_eq(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* cmp/eq rm, rn */
    mq_cpu_setT(cpu, cpu->r[n] == cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void cmp_eq_imm_r0(mqMachine *mach, mqCpu *cpu, int imm) {
    /* cmp/eq #imm, r0 */
    mq_cpu_setT(cpu, (i32)cpu->r[0] == (i8)imm);
    cpu->pc += 2;
}
MQ_INLINE void cmp_hs(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* cmp/hs rm, rn */
    mq_cpu_setT(cpu, (u32)cpu->r[n] >= (u32)cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void cmp_ge(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* cmp/ge rm, rn */
    mq_cpu_setT(cpu, (i32)cpu->r[n] >= (i32)cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void cmp_hi(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* cmp/hi rm, rn */
    mq_cpu_setT(cpu, (u32)cpu->r[n] > (u32)cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void cmp_gt(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* cmp/gt rm, rn */
    mq_cpu_setT(cpu, (i32)cpu->r[n] > (i32)cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void cmp_pl(mqMachine *mach, mqCpu *cpu, int n) {
    /* cmp/pl rn */
    mq_cpu_setT(cpu, (i32)cpu->r[n] > 0);
    cpu->pc += 2;
}
MQ_INLINE void cmp_pz(mqMachine *mach, mqCpu *cpu, int n) {
    /* cmp/pz rn */
    mq_cpu_setT(cpu, (i32)cpu->r[n] >= 0);
    cpu->pc += 2;
}
MQ_INLINE void cmp_str(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* cmp/str rm, rn */
    u32 l = cpu->r[n];
    u32 r = cpu->r[m];
    bool T = false;
    for(int i = 0; i < 4; i++) {
        T |= ((u8)l == (u8)r);
        l >>= 8;
        r >>= 8;
    }
    mq_cpu_setT(cpu, T);
    cpu->pc += 2;
}
MQ_INLINE void extub(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* extu.b rm, rn */
    cpu->r[n] = (u8)cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void extuw(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* extu.w rm, rn */
    cpu->r[n] = (u16)cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void extsb(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* exts.b rm, rn */
    cpu->r[n] = (i8)cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void extsw(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* extw.b rm, rn */
    cpu->r[n] = (i16)cpu->r[m];
    cpu->pc += 2;
}

MQ_INLINE void shll(mqMachine *mach, mqCpu *cpu, int n) {
    /* shll rn */
    mq_cpu_setT(cpu, (i32)cpu->r[n] < 0);
    cpu->r[n] <<= 1;
    cpu->pc += 2;
}
MQ_INLINE void shal(mqMachine *mach, mqCpu *cpu, int n) {
    /* shal rn (same as shll) */
    mq_cpu_setT(cpu, (i32)cpu->r[n] < 0);
    cpu->r[n] <<= 1;
    cpu->pc += 2;
}
MQ_INLINE void shlr(mqMachine *mach, mqCpu *cpu, int n) {
    /* shlr rn */
    mq_cpu_setT(cpu, cpu->r[n] & 1);
    cpu->r[n] = (u32)cpu->r[n] >> 1;
    cpu->pc += 2;
}
MQ_INLINE void shar(mqMachine *mach, mqCpu *cpu, int n) {
    /* shar rn */
    mq_cpu_setT(cpu, cpu->r[n] & 1);
    cpu->r[n] = (i32)cpu->r[n] >> 1;
    cpu->pc += 2;
}
MQ_INLINE void shll2(mqMachine *mach, mqCpu *cpu, int n) {
    /* shll2 rn */
    cpu->r[n] <<= 2;
    cpu->pc += 2;
}
MQ_INLINE void shll8(mqMachine *mach, mqCpu *cpu, int n) {
    /* shll8 rn */
    cpu->r[n] <<= 8;
    cpu->pc += 2;
}
MQ_INLINE void shll16(mqMachine *mach, mqCpu *cpu, int n) {
    /* shll16 rn */
    cpu->r[n] <<= 16;
    cpu->pc += 2;
}
MQ_INLINE void shlr2(mqMachine *mach, mqCpu *cpu, int n) {
    /* shlr2 rn */
    cpu->r[n] = (u32)cpu->r[n] >> 2;
    cpu->pc += 2;
}
MQ_INLINE void shlr8(mqMachine *mach, mqCpu *cpu, int n) {
    /* shlr8 rn */
    cpu->r[n] = (u32)cpu->r[n] >> 8;
    cpu->pc += 2;
}
MQ_INLINE void shlr16(mqMachine *mach, mqCpu *cpu, int n) {
    /* shlr16 rn */
    cpu->r[n] = (u32)cpu->r[n] >> 16;
    cpu->pc += 2;
}
MQ_INLINE void shad(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* shad rm, rn */
    i32 shift = cpu->r[m];
    if(shift >= 0)
        cpu->r[n] <<= shift;
    else
        cpu->r[n] = (i32)cpu->r[n] >> -shift;
    cpu->pc += 2;
}
MQ_INLINE void shld(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* shld rm, rn */
    i32 shift = cpu->r[m];
    if(shift >= 0)
        cpu->r[n] <<= shift;
    else
        cpu->r[n] = (u32)cpu->r[n] >> -shift;
    cpu->pc += 2;
}
MQ_INLINE void rotl(mqMachine *mach, mqCpu *cpu, int n) {
    /* rotl rn */
    int T = (i32)cpu->r[n] < 0;
    cpu->r[n] = (cpu->r[n] << 1) | T;
    mq_cpu_setT(cpu, T);
    cpu->pc += 2;
}
MQ_INLINE void rotcl(mqMachine *mach, mqCpu *cpu, int n) {
    /* rotcl rn */
    int MSB = (i32)cpu->r[n] < 0;
    int T = mq_cpu_getT(cpu);
    cpu->r[n] = (cpu->r[n] << 1) | T;
    mq_cpu_setT(cpu, MSB);
    cpu->pc += 2;
}
MQ_INLINE void rotr(mqMachine *mach, mqCpu *cpu, int n) {
    /* rotr rn */
    u32 T = cpu->r[n] & 1;
    cpu->r[n] = (cpu->r[n] >> 1) | (T << 31);
    mq_cpu_setT(cpu, T);
    cpu->pc += 2;
}
MQ_INLINE void rotcr(mqMachine *mach, mqCpu *cpu, int n) {
    /* rotcr rn */
    int LSB = cpu->r[n] & 1;
    u32 T = mq_cpu_getT(cpu);
    cpu->r[n] = (cpu->r[n] >> 1) | (T << 31);
    mq_cpu_setT(cpu, LSB);
    cpu->pc += 2;
}
MQ_INLINE void xtrct(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* xtcrt rm, rn */
    cpu->r[n] = (cpu->r[m] << 16) + (cpu->r[n] >> 16);
    cpu->pc += 2;
}

MQ_INLINE void mulsw(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* muls.w rm, rn */
    cpu->spRegs[SH_MACL] = (i32)(i16)cpu->r[m] * (i32)(i16)cpu->r[n];
    cpu->pc += 2;
}
MQ_INLINE void muluw(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mulu.w rm, rn */
    cpu->spRegs[SH_MACL] = (u32)(u16)cpu->r[m] * (u32)(u16)cpu->r[n];
    cpu->pc += 2;
}
MQ_INLINE void mull(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mul.l rm, rn */
    cpu->spRegs[SH_MACL] = cpu->r[m] * cpu->r[n];
    cpu->pc += 2;
}
MQ_INLINE void dmulsl(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* dmuls.l rm, rn */
    i64 r = (i64)(i32)cpu->r[m] * (i64)(i32)cpu->r[n];
    cpu->spRegs[SH_MACL] = r;
    cpu->spRegs[SH_MACH] = r >> 32;
    cpu->pc += 2;
}
MQ_INLINE void dmulul(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* dmulu.l rm, rn */
    u64 r = (u64)cpu->r[m] * (u64)cpu->r[n];
    cpu->spRegs[SH_MACL] = r;
    cpu->spRegs[SH_MACH] = r >> 32;
    cpu->pc += 2;
}

MQ_INLINE void div0u(mqMachine *mach, mqCpu *cpu) {
    /* div0u */
    mq_cpu_setQ(cpu, 0);
    mq_cpu_setM(cpu, 0);
    mq_cpu_setT(cpu, 0);
    cpu->pc += 2;
}
MQ_INLINE void div0s(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* div0s rm, rn */
    int Q = (i32)cpu->r[n] < 0;
    int M = (i32)cpu->r[m] < 0;
    mq_cpu_setQ(cpu, Q);
    mq_cpu_setM(cpu, M);
    mq_cpu_setT(cpu, M != Q);
    cpu->pc += 2;
}
MQ_INLINE void div1(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* div1 rm, rn */
    int T = mq_cpu_getT(cpu);
    int Q = mq_cpu_getQ(cpu);
    int M = mq_cpu_getM(cpu);
    int old_q = Q;

    Q = (i32)(cpu->r[n]) < 0;
    cpu->r[n] = (cpu->r[n] << 1) | T;
    u32 rn = cpu->r[n];

    if(!old_q) {
        if(!M) {
            cpu->r[n] -= cpu->r[m];
            Q ^= (cpu->r[n] > rn);
        }
        else {
            cpu->r[n] += cpu->r[m];
            Q ^= !(cpu->r[n] < rn);
        }
    }
    else {
        if(!M) {
            cpu->r[n] += cpu->r[m];
            Q ^= (cpu->r[n] < rn);
        }
        else {
            cpu->r[n] -= cpu->r[m];
            Q ^= !(cpu->r[n] > rn);
        }
    }

    mq_cpu_setQ(cpu, Q);
    mq_cpu_setT(cpu, Q == M);
    cpu->pc += 2;
}

MQ_INLINE void mova(mqMachine *mach, mqCpu *cpu, int disp) {
    /* mova pc+disp, r0 */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    cpu->r[0] = (cpu->pc & -4) + 4 + (disp << 2);
    cpu->pc += 2;
}
MQ_INLINE void movb_r(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.b @rm, rn */
    if(mq_memory_read8(mach, mach->memory, cpu->r[m], &cpu->r[n]))
        cpu->r[n] = (i8)cpu->r[n];
    cpu->pc += 2;
}
MQ_INLINE void movw_r(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.w @rm, rn */
    if(mq_memory_read16(mach, mach->memory, cpu->r[m], &cpu->r[n]))
        cpu->r[n] = (i16)cpu->r[n];
    cpu->pc += 2;
}
MQ_INLINE void movl_r(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.l @rm, rn */
    mq_memory_read32(mach, mach->memory, cpu->r[m], &cpu->r[n]);
    cpu->pc += 2;
}
MQ_INLINE void movb_w(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.b rm, @rn */
    mq_memory_write(mach, mach->memory, cpu->r[n], 1, cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void movw_w(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.w rm, @rn */
    mq_memory_write(mach, mach->memory, cpu->r[n], 2, cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void movl_w(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.l rm, @rn */
    mq_memory_write(mach, mach->memory, cpu->r[n], 4, cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void movb_r_postinc(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.b @rm+, rn */
    if(mq_memory_read8(mach, mach->memory, cpu->r[m], &cpu->r[n])) {
        cpu->r[m] += 1;
        cpu->r[n] = (i8)cpu->r[n];
    }
    cpu->pc += 2;
}
MQ_INLINE void movw_r_postinc(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.w @rm+, rn */
    if(mq_memory_read16(mach, mach->memory, cpu->r[m], &cpu->r[n])) {
        cpu->r[m] += 2;
        cpu->r[n] = (i16)cpu->r[n];
    }
    cpu->pc += 2;
}
MQ_INLINE void movl_r_postinc(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.l @rm+, rn */
    if(mq_memory_read32(mach, mach->memory, cpu->r[m], &cpu->r[n]))
        cpu->r[m] += 4;
    cpu->pc += 2;
}
MQ_INLINE void movb_w_predec(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.b rm, @-rn */
    if(mq_memory_write(mach, mach->memory, cpu->r[n]-1, 1, cpu->r[m]))
        cpu->r[n] -= 1;
    cpu->pc += 2;
}
MQ_INLINE void movw_w_predec(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.w rm, @-rn */
    if(mq_memory_write(mach, mach->memory, cpu->r[n]-2, 2, cpu->r[m]))
        cpu->r[n] -= 2;
    cpu->pc += 2;
}
MQ_INLINE void movl_w_predec(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.l rm, @-rn */
    if(mq_memory_write(mach, mach->memory, cpu->r[n]-4, 4, cpu->r[m]))
        cpu->r[n] -= 4;
    cpu->pc += 2;
}
MQ_INLINE void movl_w_rm_drn(
    mqMachine *mach, mqCpu *cpu, int n, int m, int disp) {
    /* mov.l rm, @(disp, rn) */
    mq_memory_write(mach, mach->memory, cpu->r[n] + (disp << 2), 4, cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void movl_r_drm_rn(
    mqMachine *mach, mqCpu *cpu, int n, int m, int disp) {
    /* mov.l @(disp, rm), rn */
    mq_memory_read32(mach, mach->memory, cpu->r[m] + (disp << 2), &cpu->r[n]);
    cpu->pc += 2;
}
MQ_INLINE void movw_r_dpc_rn(mqMachine *mach, mqCpu *cpu, int n, int disp) {
    /* mov.w @(disp,pc), rn */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    u32 targetAddr = cpu->pc + 4 + (disp << 1);
    if(mq_memory_read16(mach, mach->memory, targetAddr, &cpu->r[n]))
        cpu->r[n] = (i16)cpu->r[n];
    cpu->pc += 2;
}
MQ_INLINE void movl_r_dpc_rn(mqMachine *mach, mqCpu *cpu, int n, int disp) {
    /* mov.l @(disp,pc), rn */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    u32 targetAddr = (cpu->pc & -4) + 4 + (disp << 2);
    mq_memory_read32(mach, mach->memory, targetAddr, &cpu->r[n]);
    cpu->pc += 2;
}
MQ_INLINE void movb_r_drm_r0(mqMachine *mach, mqCpu *cpu, int m, int disp) {
    /* mov.b @(disp,rm), r0 */
    if(mq_memory_read8(mach, mach->memory, cpu->r[m] + disp, &cpu->r[0]))
        cpu->r[0] = (i8)cpu->r[0];
    cpu->pc += 2;
}
MQ_INLINE void movw_r_drm_r0(mqMachine *mach, mqCpu *cpu, int m, int disp) {
    /* mov.w @(disp,rm), r0 */
    if(mq_memory_read16(mach, mach->memory, cpu->r[m] + (disp << 1), &cpu->r[0]))
        cpu->r[0] = (i16)cpu->r[0];
    cpu->pc += 2;
}
MQ_INLINE void movb_w_r0_drn(mqMachine *mach, mqCpu *cpu, int n, int disp) {
    /* mov.b r0, @(disp,rn) */
    mq_memory_write(mach, mach->memory, cpu->r[n] + disp, 1, cpu->r[0]);
    cpu->pc += 2;
}
MQ_INLINE void movw_w_r0_drn(mqMachine *mach, mqCpu *cpu, int n, int disp) {
    /* mov.w r0, @(disp,rn) */
    mq_memory_write(mach, mach->memory, cpu->r[n] + (disp << 1), 2, cpu->r[0]);
    cpu->pc += 2;
}
MQ_INLINE void movb_r_r0rm_rn(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.b @(r0,rm), rn */
    if(mq_memory_read8(mach, mach->memory, cpu->r[m] + cpu->r[0], &cpu->r[n]))
        cpu->r[n] = (i8)cpu->r[n];
    cpu->pc += 2;
}
MQ_INLINE void movw_r_r0rm_rn(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.w @(r0,rm), rn */
    if(mq_memory_read16(mach, mach->memory, cpu->r[m] + cpu->r[0], &cpu->r[n]))
        cpu->r[n] = (i16)cpu->r[n];
    cpu->pc += 2;
}
MQ_INLINE void movl_r_r0rm_rn(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.l @(r0,rm), rn */
    mq_memory_read32(mach, mach->memory, cpu->r[m] + cpu->r[0], &cpu->r[n]);
    cpu->pc += 2;
}
MQ_INLINE void movb_w_rm_r0rn(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.b rm, @(r0,rn) */
    mq_memory_write(mach, mach->memory, cpu->r[n] + cpu->r[0], 1, cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void movw_w_rm_r0rn(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.w rm, @(r0,rn) */
    mq_memory_write(mach, mach->memory, cpu->r[n] + cpu->r[0], 2, cpu->r[m]);
    cpu->pc += 2;
}
MQ_INLINE void movl_w_rm_r0rn(mqMachine *mach, mqCpu *cpu, int n, int m) {
    /* mov.l rm, @(r0,rn) */
    mq_memory_write(mach, mach->memory, cpu->r[n] + cpu->r[0], 4, cpu->r[m]);
    cpu->pc += 2;
}

MQ_INLINE void ldc(mqMachine *mach, mqCpu *cpu, int m, int c) {
    if(c == SH_SR) {
        if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
            return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
        mq_cpu_setSR(cpu, cpu->r[m]);
        cpu->pc += 2;
        return;
    }
    /* ldc rm, <control> */
    cpu->spRegs[c] = cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void lds(mqMachine *mach, mqCpu *cpu, int m, int s) {
    /* lds rm, <system> */
    cpu->spRegs[s + 16] = cpu->r[m];
    cpu->pc += 2;
}
MQ_INLINE void ldcl(mqMachine *mach, mqCpu *cpu, int m, int c) {
    /* ldc.l @rm+, <control> */
    if(c == SH_SR) {
        if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
            return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
        u32 SR;
        if(mq_memory_read32(mach, mach->memory, cpu->r[m], &SR)) {
            mq_cpu_setSR(cpu, SR);
            cpu->r[m] += 4;
        }
        cpu->pc += 2;
        return;
    }
    /* ldc.l @rm+, <control> */
    if(mq_memory_read32(mach, mach->memory, cpu->r[m], &cpu->spRegs[c]))
        cpu->r[m] += 4;
    cpu->pc += 2;
}
MQ_INLINE void ldsl(mqMachine *mach, mqCpu *cpu, int m, int s) {
    /* lds.l @rm+, <system> */
    if(mq_memory_read32(mach, mach->memory, cpu->r[m], &cpu->spRegs[s+16]))
        cpu->r[m] += 4;
    cpu->pc += 2;
}
MQ_INLINE void stc(mqMachine *mach, mqCpu *cpu, int n, int c) {
    /* stc <control>, rn */
    cpu->r[n] = cpu->spRegs[c];
    cpu->pc += 2;
}
MQ_INLINE void sts(mqMachine *mach, mqCpu *cpu, int n, int s) {
    /* sts <system>, rn */
    cpu->r[n] = cpu->spRegs[s + 16];
    cpu->pc += 2;
}
MQ_INLINE void stcl(mqMachine *mach, mqCpu *cpu, int n, int c) {
    /* stc.l <control>, @-rn */
    if(mq_memory_write(mach, mach->memory, cpu->r[n]-4, 4, cpu->spRegs[c]))
        cpu->r[n] -= 4;
    cpu->pc += 2;
}
MQ_INLINE void stsl(mqMachine *mach, mqCpu *cpu, int n, int s) {
    /* sts.l <system>, @-rn */
    if(mq_memory_write(mach, mach->memory, cpu->r[n]-4, 4,
                       cpu->spRegs[s + 16]))
        cpu->r[n] -= 4;
    cpu->pc += 2;
}

MQ_INLINE void bra(mqMachine *mach, mqCpu *cpu, int disp) {
    /* bra pc+disp */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    disp = ((i32)disp << 20) >> 20; // 12->32-bit sign extension
    mq_cpu_setDelaySlot(cpu, cpu->pc + 4 + (disp << 1));
}
MQ_INLINE void bsr(mqMachine *mach, mqCpu *cpu, int disp) {
    /* bsr pc+disp */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    disp = ((i32)disp << 20) >> 20; // 12->32-bit sign extension
    // TODO[insn]: bsr: Address of next instruction might be +6, not +4, if DSP
    cpu->spRegs[SH_PR] = cpu->pc + 4;
    mq_cpu_setDelaySlot(cpu, cpu->pc + 4 + (disp << 1));
}
MQ_INLINE void braf(mqMachine *mach, mqCpu *cpu, int m) {
    /* braf rm */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    mq_cpu_setDelaySlot(cpu, cpu->pc + 4 + cpu->r[m]);
}
MQ_INLINE void bsrf(mqMachine *mach, mqCpu *cpu, int m) {
    /* bsrf rm */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    // TODO[insn]: bsrf: Address of next instruction might be +6 not +4 if DSP
    cpu->spRegs[SH_PR] = cpu->pc + 4;
    mq_cpu_setDelaySlot(cpu, cpu->pc + 4 + cpu->r[m]);
}
MQ_INLINE void bt(mqMachine *mach, mqCpu *cpu, int disp) {
    /* bt pc+disp */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    if(mq_cpu_getT(cpu))
        cpu->pc += 4 + ((i8)disp << 1);
    else
        cpu->pc += 2;
}
MQ_INLINE void bf(mqMachine *mach, mqCpu *cpu, int disp) {
    /* bf pc+disp */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    if(mq_cpu_getT(cpu))
        cpu->pc += 2;
    else
        cpu->pc += 4 + ((i8)disp << 1);
}
MQ_INLINE void bt_s(mqMachine *mach, mqCpu *cpu, int disp) {
    /* bt.s pc+disp */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    if(mq_cpu_getT(cpu))
        mq_cpu_setDelaySlot(cpu, cpu->pc + 4 + ((i8)disp << 1));
    else
        cpu->pc += 2;
}
MQ_INLINE void bf_s(mqMachine *mach, mqCpu *cpu, int disp) {
    /* bf.s pc+disp */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    if(mq_cpu_getT(cpu))
        cpu->pc += 2;
    else
        mq_cpu_setDelaySlot(cpu, cpu->pc + 4 + ((i8)disp << 1));
}
MQ_INLINE void jmp(mqMachine *mach, mqCpu *cpu, int n) {
    /* jmp @rn */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    mq_cpu_setDelaySlot(cpu, cpu->r[n]);
}
MQ_INLINE void jsr(mqMachine *mach, mqCpu *cpu, int n) {
    /* jsr @rn */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    // TODO[insn]: jsr: Address of next instruction might be +6, not +4, if DSP
    cpu->spRegs[SH_PR] = cpu->pc + 4;
    mq_cpu_setDelaySlot(cpu, cpu->r[n]);
}
MQ_INLINE void rts(mqMachine *mach, mqCpu *cpu) {
    /* rts */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    mq_cpu_setDelaySlot(cpu, cpu->spRegs[SH_PR]);
}
MQ_INLINE void rte(mqMachine *mach, mqCpu *cpu) {
    /* rte */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    // TODO[rte]: Fetch the rte slot instruction with non-restored SR
    // (because it's generally gonna need to be a kernel mode fetch)
    mq_cpu_setSR(cpu, cpu->spRegs[SH_SSR]);
    mq_cpu_setDelaySlot(cpu, cpu->spRegs[SH_SPC]);
}

MQ_INLINE void clrt(mqMachine *mach, mqCpu *cpu) {
    /* clrt */
    mq_cpu_setT(cpu, 0);
    cpu->pc += 2;
}
MQ_INLINE void sett(mqMachine *mach, mqCpu *cpu) {
    /* sett */
    mq_cpu_setT(cpu, 1);
    cpu->pc += 2;
}
MQ_INLINE void clrmac(mqMachine *mach, mqCpu *cpu) {
    /* clrmac */
    cpu->spRegs[SH_MACL] = 0;
    cpu->spRegs[SH_MACH] = 0;
    cpu->pc += 2;
}
MQ_INLINE void clrs(mqMachine *mach, mqCpu *cpu) {
    /* clrs */
    cpu->spRegs[SH_SR] &= ~0x00000002;
    cpu->pc += 2;
}
MQ_INLINE void sets(mqMachine *mach, mqCpu *cpu) {
    /* sets */
    cpu->spRegs[SH_SR] |= 0x00000002;
    cpu->pc += 2;
}
MQ_INLINE void clrmdxy(mqMachine *mach, mqCpu *cpu) {
    /* clrmdxy */
    cpu->spRegs[SH_SR] &= ~0x00000c00;
    cpu->pc += 2;
}
MQ_INLINE void setmdx(mqMachine *mach, mqCpu *cpu) {
    /* setmdx */
    cpu->spRegs[SH_SR] |= 0x00000400;
    cpu->pc += 2;
}
MQ_INLINE void setmdy(mqMachine *mach, mqCpu *cpu) {
    /* setmdy */
    cpu->spRegs[SH_SR] |= 0x00000400;
    cpu->pc += 2;
}
MQ_INLINE void movt(mqMachine *mach, mqCpu *cpu, int n) {
    /* movt rn */
    cpu->r[n] = mq_cpu_getT(cpu);
    cpu->pc += 2;
}

MQ_INLINE void ldrs(mqMachine *mach, mqCpu *cpu, int disp) {
    /* ldrs pc+disp */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    cpu->spRegs[SH_RS] = cpu->pc + 4 + (disp << 1);
    cpu->pc += 2;
}
MQ_INLINE void ldre(mqMachine *mach, mqCpu *cpu, int disp) {
    /* ldre pc+disp */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    cpu->spRegs[SH_RE] = cpu->pc + 4 + (disp << 1);
    cpu->pc += 2;
}
MQ_INLINE void ldrc(mqMachine *mach, mqCpu *cpu, int m) {
    /* ldrc rm */
    // TODO: ldrc: Doesn't set SR.RF to disable setrc-style emulation
    mq_cpu_setRC(cpu, cpu->r[m] & 0xfff);
    cpu->spRegs[SH_RE] |= 1;
    cpu->pc += 2;
}
MQ_INLINE void ldrc_imm(mqMachine *mach, mqCpu *cpu, int imm) {
    /* ldrc #imm */
    // TODO: ldrc_imm: Doesn't set SR.RF to disable setrc-style emulation
    mq_cpu_setRC(cpu, imm & 0xfff);
    cpu->spRegs[SH_RE] |= 1;
    cpu->pc += 2;
}

MQ_INLINE void pref(mqMachine *mach, mqCpu *cpu, int n) {
    /* pref @rn */
    static bool done = false;
    if(!done)
        mq_log(MQ_LOG_DEBUG, "pref instruction used and ignored");
    done = true;
    cpu->pc += 2;
}
MQ_INLINE void ocbi(mqMachine *mach, mqCpu *cpu, int n) {
    /* ocbi @rn */
    static bool done = false;
    if(!done)
        mq_log(MQ_LOG_DEBUG, "ocbi instruction used and ignored");
    done = true;
    cpu->pc += 2;
}
MQ_INLINE void ocbp(mqMachine *mach, mqCpu *cpu, int n) {
    /* ocbp @rn */
    static bool done = false;
    if(!done)
        mq_log(MQ_LOG_DEBUG, "ocbp instruction used and ignored");
    done = true;
    cpu->pc += 2;
}
MQ_INLINE void ocbwb(mqMachine *mach, mqCpu *cpu, int n) {
    /* ocbwp @rn */
    static bool done = false;
    if(!done)
        mq_log(MQ_LOG_DEBUG, "ocbwb instruction used and ignored");
    done = true;
    cpu->pc += 2;
}
MQ_INLINE void prefi(mqMachine *mach, mqCpu *cpu, int n) {
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    static bool done = false;
    if(!done)
        mq_log(MQ_LOG_DEBUG, "prefi instruction used and ignored");
    done = true;
    cpu->pc += 2;
}
MQ_INLINE void icbi(mqMachine *mach, mqCpu *cpu, int n) {
    /* icbi @rn */
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    static bool done = false;
    if(!done)
        mq_log(MQ_LOG_DEBUG, "icbi instruction used and ignored");
    done = true;
    cpu->pc += 2;
}
MQ_INLINE void synco(mqMachine *mach, mqCpu *cpu) {
    /* synco */
    cpu->pc += 2;
}

MQ_INLINE void sleep(mqMachine *mach, mqCpu *cpu) {
    mq_cpu_sleep(cpu);
    cpu->pc += 2;
}

//===//

MQ_INLINE void movlil(mqMachine *mach, mqCpu *cpu, int m) {
    fprintf(stderr, "error: not implemented: movlil\n");
    mach->stuck = true;
}
MQ_INLINE void movcol(mqMachine *mach, mqCpu *cpu, int n) {
    fprintf(stderr, "error: not implemented: movcol\n");
    mach->stuck = true;
}
MQ_INLINE void movcal(mqMachine *mach, mqCpu *cpu, int n) {
    fprintf(stderr, "error: not implemented: movcal\n");
    mach->stuck = true;
}
MQ_INLINE void ldtlb(mqMachine *mach, mqCpu *cpu) {
    fprintf(stderr, "error: not implemented: ldtlb\n");
    mach->stuck = true;
}
MQ_INLINE void macl(mqMachine *mach, mqCpu *cpu, int n, int m) {
    fprintf(stderr, "error: not implemented: macl\n");
    mach->stuck = true;
}
MQ_INLINE void setrc(mqMachine *mach, mqCpu *cpu, int m) {
    fprintf(stderr, "error: not implemented: setrc\n");
    mach->stuck = true;
}
MQ_INLINE void movual_r(mqMachine *mach, mqCpu *cpu, int m) {
    fprintf(stderr, "error: not implemented: movual_r\n");
    mach->stuck = true;
}
MQ_INLINE void movual_r_postinc(mqMachine *mach, mqCpu *cpu, int m) {
    fprintf(stderr, "error: not implemented: movual_r_postinc\n");
    mach->stuck = true;
}
MQ_INLINE void tasb(mqMachine *mach, mqCpu *cpu, int n) {
    fprintf(stderr, "error: not implemented: tasb\n");
    mach->stuck = true;
}
MQ_INLINE void macw(mqMachine *mach, mqCpu *cpu, int n, int m) {
    fprintf(stderr, "error: not implemented: macw\n");
    mach->stuck = true;
}
MQ_INLINE void setrc_imm(mqMachine *mach, mqCpu *cpu, int imm) {
    fprintf(stderr, "error: not implemented: setrc_imm\n");
    mach->stuck = true;
}
MQ_INLINE void movb_w_r0_dgbr(mqMachine *mach, mqCpu *cpu, int disp) {
    fprintf(stderr, "error: not implemented: movb_w_r0_dgbr\n");
    mach->stuck = true;
}
MQ_INLINE void movw_w_r0_dgbr(mqMachine *mach, mqCpu *cpu, int disp) {
    fprintf(stderr, "error: not implemented: movw_w_r0_dgbr\n");
    mach->stuck = true;
}
MQ_INLINE void movl_w_r0_dgbr(mqMachine *mach, mqCpu *cpu, int disp) {
    fprintf(stderr, "error: not implemented: movl_w_r0_dgbr\n");
    mach->stuck = true;
}
MQ_INLINE void movb_r_dgbr_r0(mqMachine *mach, mqCpu *cpu, int disp) {
    fprintf(stderr, "error: not implemented: movb_r_dgbr_r0\n");
    mach->stuck = true;
}
MQ_INLINE void movw_r_dgbr_r0(mqMachine *mach, mqCpu *cpu, int disp) {
    fprintf(stderr, "error: not implemented: movw_r_dgbr_r0\n");
    mach->stuck = true;
}
MQ_INLINE void movl_r_dgbr_r0(mqMachine *mach, mqCpu *cpu, int disp) {
    fprintf(stderr, "error: not implemented: movl_r_dgbr_r0\n");
    mach->stuck = true;
}
MQ_INLINE void andb_imm_r0gbr(mqMachine *mach, mqCpu *cpu, int imm) {
    fprintf(stderr, "error: not implemented: andb_imm_r0gbr\n");
    mach->stuck = true;
}
MQ_INLINE void orb_imm_r0gbr(mqMachine *mach, mqCpu *cpu, int imm) {
    fprintf(stderr, "error: not implemented: orb_imm_r0gbr\n");
    mach->stuck = true;
}
MQ_INLINE void tstb_imm_r0gbr(mqMachine *mach, mqCpu *cpu, int imm) {
    fprintf(stderr, "error: not implemented: tstb_imm_r0gbr\n");
    mach->stuck = true;
}
MQ_INLINE void xorb_imm_r0gbr(mqMachine *mach, mqCpu *cpu, int imm) {
    fprintf(stderr, "error: not implemented: xorb_imm_r0gbr\n");
    mach->stuck = true;
}
MQ_INLINE void trapa(mqMachine *mach, mqCpu *cpu, int imm) {
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL_SLOT, 0);
    fprintf(stderr, "error: not implemented: trapa\n");
    mach->stuck = true;
}

#pragma GCC diagnostic pop

// TODO: Don't hardcode switch vs. table
#define MQ_DECODER_SWITCH 0
#define MQ_DECODER_TABLE 1

#if MQ_DECODER_TABLE
#include "autogen/sh-isa.inc"

void _mq_cpu_execute(mqMachine *mach, mqCpu *cpu, u16 opcode)
{
    // direct
    mq_inst_wrapper_table[opcode](mach, cpu, opcode);

//    // 2direct
//    u8 idx = mq_inst_translation_table[opcode];
//    mq_inst_wrapper_table[idx](mach, cpu, opcode);
}
#endif /* MQ_DECODER_TABLE */

#if MQ_DECODER_SWITCH
void _mq_cpu_execute(mqMachine *mach, mqCpu *cpu, u16 opcode)
{
#define _OPCODE opcode
#define _DECIDE(X, ...) return X(mach, cpu, ##__VA_ARGS__)
#include "autogen/sh-isa.inc"

    mq_cpu_raiseException(cpu, SH_EXC_ILLEGAL, 0);
}
#endif /* MQ_DECODER_SWITCH */
