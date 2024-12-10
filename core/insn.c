//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.insn: Emulation code for SuperH instructions

#include <mq/cpu.h>

#include <stdio.h>
#include <stdlib.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"

MQ_INLINE void stc(int n, int c) {
    fprintf(stderr, "error: not implemented: stc\n");
    abort();
}
MQ_INLINE void bsrf(int m) {
    fprintf(stderr, "error: not implemented: bsrf\n");
    abort();
}
MQ_INLINE void braf(int m) {
    fprintf(stderr, "error: not implemented: braf\n");
    abort();
}
MQ_INLINE void movlil(int m) {
    fprintf(stderr, "error: not implemented: movlil\n");
    abort();
}
MQ_INLINE void movcol(int n) {
    fprintf(stderr, "error: not implemented: movcol\n");
    abort();
}
MQ_INLINE void pref(int n) {
    fprintf(stderr, "error: not implemented: pref\n");
    abort();
}
MQ_INLINE void ocbi(int n) {
    fprintf(stderr, "error: not implemented: ocbi\n");
    abort();
}
MQ_INLINE void ocbp(int n) {
    fprintf(stderr, "error: not implemented: ocbp\n");
    abort();
}
MQ_INLINE void ocbwb(int n) {
    fprintf(stderr, "error: not implemented: ocbwb\n");
    abort();
}
MQ_INLINE void movcal(int n) {
    fprintf(stderr, "error: not implemented: movcal\n");
    abort();
}
MQ_INLINE void prefi(int n) {
    fprintf(stderr, "error: not implemented: prefi\n");
    abort();
}
MQ_INLINE void icbi(int n) {
    fprintf(stderr, "error: not implemented: icbi\n");
    abort();
}
MQ_INLINE void movb_w_rm_r0rn(int n, int m) {
    fprintf(stderr, "error: not implemented: movb_w_rm_r0rn\n");
    abort();
}
MQ_INLINE void movw_w_rm_r0rn(int n, int m) {
    fprintf(stderr, "error: not implemented: movw_w_rm_r0rn\n");
    abort();
}
MQ_INLINE void movl_w_rm_r0rn(int n, int m) {
    fprintf(stderr, "error: not implemented: movl_w_rm_r0rn\n");
    abort();
}
MQ_INLINE void mull(int n, int m) {
    fprintf(stderr, "error: not implemented: mull\n");
    abort();
}
MQ_INLINE void clrt() {
    fprintf(stderr, "error: not implemented: clrt\n");
    abort();
}
MQ_INLINE void sett() {
    fprintf(stderr, "error: not implemented: sett\n");
    abort();
}
MQ_INLINE void clrmac() {
    fprintf(stderr, "error: not implemented: clrmac\n");
    abort();
}
MQ_INLINE void ldtlb() {
    fprintf(stderr, "error: not implemented: ldtlb\n");
    abort();
}
MQ_INLINE void clrs() {
    fprintf(stderr, "error: not implemented: clrs\n");
    abort();
}
MQ_INLINE void sets() {
    fprintf(stderr, "error: not implemented: sets\n");
    abort();
}
MQ_INLINE void clrmdxy() {
    fprintf(stderr, "error: not implemented: clrmdxy\n");
    abort();
}
MQ_INLINE void setmdx() {
    fprintf(stderr, "error: not implemented: setmdx\n");
    abort();
}
MQ_INLINE void setmdy() {
    fprintf(stderr, "error: not implemented: setmdy\n");
    abort();
}
MQ_INLINE void nop() {
    fprintf(stderr, "error: not implemented: nop\n");
    abort();
}
MQ_INLINE void div0u() {
    fprintf(stderr, "error: not implemented: div0u\n");
    abort();
}
MQ_INLINE void movt(int n) {
    fprintf(stderr, "error: not implemented: movt\n");
    abort();
}
MQ_INLINE void sts(int n, int s) {
    fprintf(stderr, "error: not implemented: sts\n");
    abort();
}
MQ_INLINE void rts() {
    fprintf(stderr, "error: not implemented: rts\n");
    abort();
}
MQ_INLINE void sleep() {
    fprintf(stderr, "error: not implemented: sleep\n");
    abort();
}
MQ_INLINE void rte() {
    fprintf(stderr, "error: not implemented: rte\n");
    abort();
}
MQ_INLINE void synco() {
    fprintf(stderr, "error: not implemented: synco\n");
    abort();
}
MQ_INLINE void movb_r_r0rm_rn(int n, int m) {
    fprintf(stderr, "error: not implemented: movb_r_r0rm_rn\n");
    abort();
}
MQ_INLINE void movw_r_r0rm_rn(int n, int m) {
    fprintf(stderr, "error: not implemented: movw_r_r0rm_rn\n");
    abort();
}
MQ_INLINE void movl_r_r0rm_rn(int n, int m) {
    fprintf(stderr, "error: not implemented: movl_r_r0rm_rn\n");
    abort();
}
MQ_INLINE void macl(int n, int m) {
    fprintf(stderr, "error: not implemented: macl\n");
    abort();
}
MQ_INLINE void movl_w_rm_drn(int n, int m, int disp) {
    fprintf(stderr, "error: not implemented: movl_w_rm_drn\n");
    abort();
}
MQ_INLINE void movb_w(int n, int m) {
    fprintf(stderr, "error: not implemented: movb_w\n");
    abort();
}
MQ_INLINE void movw_w(int n, int m) {
    fprintf(stderr, "error: not implemented: movw_w\n");
    abort();
}
MQ_INLINE void movl_w(int n, int m) {
    fprintf(stderr, "error: not implemented: movl_w\n");
    abort();
}
MQ_INLINE void movb_w_predec(int n, int m) {
    fprintf(stderr, "error: not implemented: movb_w_predec\n");
    abort();
}
MQ_INLINE void movw_w_predec(int n, int m) {
    fprintf(stderr, "error: not implemented: movw_w_predec\n");
    abort();
}
MQ_INLINE void movl_w_predec(int n, int m) {
    fprintf(stderr, "error: not implemented: movl_w_predec\n");
    abort();
}
MQ_INLINE void div0s(int n, int m) {
    fprintf(stderr, "error: not implemented: div0s\n");
    abort();
}
MQ_INLINE void tst(int n, int m) {
    fprintf(stderr, "error: not implemented: tst\n");
    abort();
}
MQ_INLINE void and(int n, int m) {
    fprintf(stderr, "error: not implemented: and\n");
    abort();
}
MQ_INLINE void xor(int n, int m) {
    fprintf(stderr, "error: not implemented: xor\n");
    abort();
}
MQ_INLINE void or(int n, int m) {
    fprintf(stderr, "error: not implemented: or\n");
    abort();
}
MQ_INLINE void cmp_str(int n, int m) {
    fprintf(stderr, "error: not implemented: cmp_str\n");
    abort();
}
MQ_INLINE void xtrct(int n, int m) {
    fprintf(stderr, "error: not implemented: xtrct\n");
    abort();
}
MQ_INLINE void muluw(int n, int m) {
    fprintf(stderr, "error: not implemented: muluw\n");
    abort();
}
MQ_INLINE void mulsw(int n, int m) {
    fprintf(stderr, "error: not implemented: mulsw\n");
    abort();
}
MQ_INLINE void cmp_eq(int n, int m) {
    fprintf(stderr, "error: not implemented: cmp_eq\n");
    abort();
}
MQ_INLINE void cmp_hs(int n, int m) {
    fprintf(stderr, "error: not implemented: cmp_hs\n");
    abort();
}
MQ_INLINE void cmp_ge(int n, int m) {
    fprintf(stderr, "error: not implemented: cmp_ge\n");
    abort();
}
MQ_INLINE void div1(int n, int m) {
    fprintf(stderr, "error: not implemented: div1\n");
    abort();
}
MQ_INLINE void dmulul(int n, int m) {
    fprintf(stderr, "error: not implemented: dmulul\n");
    abort();
}
MQ_INLINE void cmp_hi(int n, int m) {
    fprintf(stderr, "error: not implemented: cmp_hi\n");
    abort();
}
MQ_INLINE void cmp_gt(int n, int m) {
    fprintf(stderr, "error: not implemented: cmp_gt\n");
    abort();
}
MQ_INLINE void sub(int n, int m) {
    fprintf(stderr, "error: not implemented: sub\n");
    abort();
}
MQ_INLINE void subc(int n, int m) {
    fprintf(stderr, "error: not implemented: subc\n");
    abort();
}
MQ_INLINE void subv(int n, int m) {
    fprintf(stderr, "error: not implemented: subv\n");
    abort();
}
MQ_INLINE void add(int n, int m) {
    fprintf(stderr, "error: not implemented: add\n");
    abort();
}
MQ_INLINE void dmulsl(int n, int m) {
    fprintf(stderr, "error: not implemented: dmulsl\n");
    abort();
}
MQ_INLINE void addc(int n, int m) {
    fprintf(stderr, "error: not implemented: addc\n");
    abort();
}
MQ_INLINE void addv(int n, int m) {
    fprintf(stderr, "error: not implemented: addv\n");
    abort();
}
MQ_INLINE void shll(int n) {
    fprintf(stderr, "error: not implemented: shll\n");
    abort();
}
MQ_INLINE void dt(int n) {
    fprintf(stderr, "error: not implemented: dt\n");
    abort();
}
MQ_INLINE void shal(int n) {
    fprintf(stderr, "error: not implemented: shal\n");
    abort();
}
MQ_INLINE void shlr(int n) {
    fprintf(stderr, "error: not implemented: shlr\n");
    abort();
}
MQ_INLINE void cmp_pz(int n) {
    fprintf(stderr, "error: not implemented: cmp_pz\n");
    abort();
}
MQ_INLINE void shar(int n) {
    fprintf(stderr, "error: not implemented: shar\n");
    abort();
}
MQ_INLINE void stsl(int n, int s) {
    fprintf(stderr, "error: not implemented: stsl\n");
    abort();
}
MQ_INLINE void stcl(int n, int c) {
    fprintf(stderr, "error: not implemented: stcl\n");
    abort();
}
MQ_INLINE void rotl(int n) {
    fprintf(stderr, "error: not implemented: rotl\n");
    abort();
}
MQ_INLINE void setrc(int m) {
    fprintf(stderr, "error: not implemented: setrc\n");
    abort();
}
MQ_INLINE void rotcl(int n) {
    fprintf(stderr, "error: not implemented: rotcl\n");
    abort();
}
MQ_INLINE void ldrc(int m) {
    fprintf(stderr, "error: not implemented: ldrc\n");
    abort();
}
MQ_INLINE void cmp_pl(int n) {
    fprintf(stderr, "error: not implemented: cmp_pl\n");
    abort();
}
MQ_INLINE void rotr(int n) {
    fprintf(stderr, "error: not implemented: rotr\n");
    abort();
}
MQ_INLINE void rotcr(int n) {
    fprintf(stderr, "error: not implemented: rotcr\n");
    abort();
}
MQ_INLINE void ldsl(int m, int s) {
    fprintf(stderr, "error: not implemented: ldsl\n");
    abort();
}
MQ_INLINE void ldcl(int m, int c) {
    fprintf(stderr, "error: not implemented: ldcl\n");
    abort();
}
MQ_INLINE void shll2(int n) {
    fprintf(stderr, "error: not implemented: shll2\n");
    abort();
}
MQ_INLINE void shll8(int n) {
    fprintf(stderr, "error: not implemented: shll8\n");
    abort();
}
MQ_INLINE void shll16(int n) {
    fprintf(stderr, "error: not implemented: shll16\n");
    abort();
}
MQ_INLINE void shlr2(int n) {
    fprintf(stderr, "error: not implemented: shlr2\n");
    abort();
}
MQ_INLINE void shlr8(int n) {
    fprintf(stderr, "error: not implemented: shlr8\n");
    abort();
}
MQ_INLINE void shlr16(int n) {
    fprintf(stderr, "error: not implemented: shlr16\n");
    abort();
}
MQ_INLINE void movual_r(int m) {
    fprintf(stderr, "error: not implemented: movual_r\n");
    abort();
}
MQ_INLINE void movual_r_postinc(int m) {
    fprintf(stderr, "error: not implemented: movual_r_postinc\n");
    abort();
}
MQ_INLINE void lds(int m, int s) {
    fprintf(stderr, "error: not implemented: lds\n");
    abort();
}
MQ_INLINE void jmp(int n) {
    fprintf(stderr, "error: not implemented: jmp\n");
    abort();
}
MQ_INLINE void jsr(int n) {
    fprintf(stderr, "error: not implemented: jsr\n");
    abort();
}
MQ_INLINE void tasb(int n) {
    fprintf(stderr, "error: not implemented: tasb\n");
    abort();
}
MQ_INLINE void shad(int n, int m) {
    fprintf(stderr, "error: not implemented: shad\n");
    abort();
}
MQ_INLINE void shld(int n, int m) {
    fprintf(stderr, "error: not implemented: shld\n");
    abort();
}
MQ_INLINE void ldc(int n, int c) {
    fprintf(stderr, "error: not implemented: ldc\n");
    abort();
}
MQ_INLINE void macw(int n, int m) {
    fprintf(stderr, "error: not implemented: macw\n");
    abort();
}
MQ_INLINE void movl_r_drm_rn(int n, int m, int disp) {
    fprintf(stderr, "error: not implemented: movl_r_drm_rn\n");
    abort();
}
MQ_INLINE void movb_r(int n, int m) {
    fprintf(stderr, "error: not implemented: movb_r\n");
    abort();
}
MQ_INLINE void movw_r(int n, int m) {
    fprintf(stderr, "error: not implemented: movw_r\n");
    abort();
}
MQ_INLINE void movl_r(int n, int m) {
    fprintf(stderr, "error: not implemented: movl_r\n");
    abort();
}
MQ_INLINE void mov(int n, int m) {
    fprintf(stderr, "error: not implemented: mov\n");
    abort();
}
MQ_INLINE void movb_r_postinc(int n, int m) {
    fprintf(stderr, "error: not implemented: movb_r_postinc\n");
    abort();
}
MQ_INLINE void movw_r_postinc(int n, int m) {
    fprintf(stderr, "error: not implemented: movw_r_postinc\n");
    abort();
}
MQ_INLINE void movl_r_postinc(int n, int m) {
    fprintf(stderr, "error: not implemented: movl_r_postinc\n");
    abort();
}
MQ_INLINE void not(int n, int m) {
    fprintf(stderr, "error: not implemented: not\n");
    abort();
}
MQ_INLINE void swapb(int n, int m) {
    fprintf(stderr, "error: not implemented: swapb\n");
    abort();
}
MQ_INLINE void swapw(int n, int m) {
    fprintf(stderr, "error: not implemented: swapw\n");
    abort();
}
MQ_INLINE void negc(int n, int m) {
    fprintf(stderr, "error: not implemented: negc\n");
    abort();
}
MQ_INLINE void neg(int n, int m) {
    fprintf(stderr, "error: not implemented: neg\n");
    abort();
}
MQ_INLINE void extub(int n, int m) {
    fprintf(stderr, "error: not implemented: extub\n");
    abort();
}
MQ_INLINE void extuw(int n, int m) {
    fprintf(stderr, "error: not implemented: extuw\n");
    abort();
}
MQ_INLINE void extsb(int n, int m) {
    fprintf(stderr, "error: not implemented: extsb\n");
    abort();
}
MQ_INLINE void extsw(int n, int m) {
    fprintf(stderr, "error: not implemented: extsw\n");
    abort();
}
MQ_INLINE void add_imm(int n, int imm) {
    fprintf(stderr, "error: not implemented: add_imm\n");
    abort();
}
MQ_INLINE void movb_w_r0_drn(int n, int disp) {
    fprintf(stderr, "error: not implemented: movb_w_r0_drn\n");
    abort();
}
MQ_INLINE void movw_w_r0_drn(int n, int disp) {
    fprintf(stderr, "error: not implemented: movw_w_r0_drn\n");
    abort();
}
MQ_INLINE void setrc_imm(int imm) {
    fprintf(stderr, "error: not implemented: setrc_imm\n");
    abort();
}
MQ_INLINE void movb_r_drm_r0(int m, int disp) {
    fprintf(stderr, "error: not implemented: movb_r_drm_r0\n");
    abort();
}
MQ_INLINE void movw_r_drm_r0(int m, int disp) {
    fprintf(stderr, "error: not implemented: movw_r_drm_r0\n");
    abort();
}
MQ_INLINE void cmp_eq_imm_r0(int imm) {
    fprintf(stderr, "error: not implemented: cmp_eq_imm_r0\n");
    abort();
}
MQ_INLINE void bt(int disp) {
    fprintf(stderr, "error: not implemented: bt\n");
    abort();
}
MQ_INLINE void ldrc_imm(int imm) {
    fprintf(stderr, "error: not implemented: ldrc_imm\n");
    abort();
}
MQ_INLINE void bf(int disp) {
    fprintf(stderr, "error: not implemented: bf\n");
    abort();
}
MQ_INLINE void ldrs(int disp) {
    fprintf(stderr, "error: not implemented: ldrs\n");
    abort();
}
MQ_INLINE void bt_s(int disp) {
    fprintf(stderr, "error: not implemented: bt_s\n");
    abort();
}
MQ_INLINE void ldre(int disp) {
    fprintf(stderr, "error: not implemented: ldre\n");
    abort();
}
MQ_INLINE void bf_s(int disp) {
    fprintf(stderr, "error: not implemented: bf_s\n");
    abort();
}
MQ_INLINE void movw_r_dpc_rn(int n, int disp) {
    fprintf(stderr, "error: not implemented: movw_r_dpc_rn\n");
    abort();
}
MQ_INLINE void bra(int disp) {
    fprintf(stderr, "error: not implemented: bra\n");
    abort();
}
MQ_INLINE void bsr(int disp) {
    fprintf(stderr, "error: not implemented: bsr\n");
    abort();
}
MQ_INLINE void movb_w_r0_dgbr(int disp) {
    fprintf(stderr, "error: not implemented: movb_w_r0_dgbr\n");
    abort();
}
MQ_INLINE void movw_w_r0_dgbr(int disp) {
    fprintf(stderr, "error: not implemented: movw_w_r0_dgbr\n");
    abort();
}
MQ_INLINE void movl_w_r0_dgbr(int disp) {
    fprintf(stderr, "error: not implemented: movl_w_r0_dgbr\n");
    abort();
}
MQ_INLINE void movb_r_dgbr_r0(int disp) {
    fprintf(stderr, "error: not implemented: movb_r_dgbr_r0\n");
    abort();
}
MQ_INLINE void movw_r_dgbr_r0(int disp) {
    fprintf(stderr, "error: not implemented: movw_r_dgbr_r0\n");
    abort();
}
MQ_INLINE void movl_r_dgbr_r0(int disp) {
    fprintf(stderr, "error: not implemented: movl_r_dgbr_r0\n");
    abort();
}
MQ_INLINE void andb_imm_r0gbr(int imm) {
    fprintf(stderr, "error: not implemented: andb_imm_r0gbr\n");
    abort();
}
MQ_INLINE void orb_imm_r0gbr(int imm) {
    fprintf(stderr, "error: not implemented: orb_imm_r0gbr\n");
    abort();
}
MQ_INLINE void tstb_imm_r0gbr(int imm) {
    fprintf(stderr, "error: not implemented: tstb_imm_r0gbr\n");
    abort();
}
MQ_INLINE void xorb_imm_r0gbr(int imm) {
    fprintf(stderr, "error: not implemented: xorb_imm_r0gbr\n");
    abort();
}
MQ_INLINE void mova(int disp) {
    fprintf(stderr, "error: not implemented: mova\n");
    abort();
}
MQ_INLINE void and_imm_r0(int imm) {
    fprintf(stderr, "error: not implemented: and_imm_r0\n");
    abort();
}
MQ_INLINE void or_imm_r0(int imm) {
    fprintf(stderr, "error: not implemented: or_imm_r0\n");
    abort();
}
MQ_INLINE void tst_imm_r0(int imm) {
    fprintf(stderr, "error: not implemented: tst_imm_r0\n");
    abort();
}
MQ_INLINE void xor_imm_r0(int imm) {
    fprintf(stderr, "error: not implemented: xor_imm_r0\n");
    abort();
}
MQ_INLINE void trapa(int imm) {
    fprintf(stderr, "error: not implemented: trapa\n");
    abort();
}
MQ_INLINE void movl_r_dpc_rn(int n, int disp) {
    fprintf(stderr, "error: not implemented: movl_r_dpc_rn\n");
    abort();
}
MQ_INLINE void mov_imm(int n, int imm) {
    fprintf(stderr, "error: not implemented: mov_imm\n");
    abort();
}

#pragma GCC diagnostic pop

void _mq_cpu_execute(struct mqMachine *mach, mqCpu *cpu, u16 opcode)
{
#define _OPCODE opcode
#include "autogen/sh-isa.inc"
}
