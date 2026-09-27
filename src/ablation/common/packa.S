#ifndef MP5_ABLATION_GROUP4
#error MP5_ABLATION_GROUP4 is required
#endif

.text
.p2align 2
.globl _mp5_ablation_packa_64
_mp5_ablation_packa_64:
    // x0=mc (multiple of 4*SVL), x1=kc, x2=A, x3=lda, x4=packed A.
    smstart
    cntw x9
    lsl x10, x9, #2
    lsl x5, x3, #2
    mov x6, #0
    ptrue pn8.b
1:
    add x7, x2, x6, lsl #2
    mov x8, x1
2:
#if MP5_ABLATION_GROUP4
    ld1w {z0.s-z3.s}, pn8/z, [x7]
#else
    ptrue p0.s
    ld1w {z0.s}, p0/z, [x7]
    ld1w {z1.s}, p0/z, [x7, #1, MUL VL]
    ld1w {z2.s}, p0/z, [x7, #2, MUL VL]
    ld1w {z3.s}, p0/z, [x7, #3, MUL VL]
#endif
    st1w {z0.s-z3.s}, pn8, [x4]
    add x7, x7, x5
    addvl x4, x4, #4
    subs x8, x8, #1
    b.ne 2b
    add x6, x6, x10
    cmp x6, x0
    b.lt 1b
    smstop
    ret

.globl _mp5_ablation_packa_16
_mp5_ablation_packa_16:
    // x0=mc tail, x1=kc, x2=A, x3=lda, x4=packed A.
    smstart
    cntw x9
    lsl x5, x3, #2
    mov x6, #0
3:
    whilelt p0.s, x6, x0
    add x7, x2, x6, lsl #2
    mov x8, x1
4:
    ld1w {z0.s}, p0/z, [x7]
    st1w {z0.s}, p0, [x4]
    add x7, x7, x5
    addvl x4, x4, #1
    subs x8, x8, #1
    b.ne 4b
    add x6, x6, x9
    cmp x6, x0
    b.lt 3b
    smstop
    ret
