// native local reconstruction: fixed leaf interfaces make group cardinality an instruction-level fact.
// Loads fold every group, lane, and iteration into a group-labelled modulo-2^64 checksum.
// Stores use distinct tag-derived vectors per group and finish with DSB ISH before SMSTOP SM.
    .macro native_ENTER_SM
    // Ordinary ABI calls must retain the AAPCS64 callee-saved low halves of v8-v15.
    stp     d8, d9, [sp, #-64]!
    stp     d10, d11, [sp, #16]
    stp     d12, d13, [sp, #32]
    stp     d14, d15, [sp, #48]
    smstart sm
    .endm

    .macro native_LEAVE_SM
    smstop  sm
    ldp     d8, d9, [sp, #0]
    ldp     d10, d11, [sp, #16]
    ldp     d12, d13, [sp, #32]
    ldp     d14, d15, [sp, #48]
    add     sp, sp, #64
    ret
    .endm

    .text
    .globl _transfer_sme_vl_bytes
_transfer_sme_vl_bytes:
    rdsvl   x0, #1
    ret

    .globl _transfer_sme_load_b_g1
_transfer_sme_load_b_g1:
    cbz     x2, .Ltransfer_sme_load_b_g1_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   p0.b
    ptrue   p1.d
    dup     z28.d, #0
.Ltransfer_sme_load_b_g1_loop:
    ld1b    {z0.b}, p0/z, [x8]
    add     z28.d, z28.d, z0.d
    addvl   x8, x8, #1
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_b_g1_next
    mov     x8, x7
.Ltransfer_sme_load_b_g1_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_b_g1_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_b_g1_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_b_g2
_transfer_sme_load_b_g2:
    cbz     x2, .Ltransfer_sme_load_b_g2_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.b
    ptrue   p1.d
    dup     z28.d, #0
    dup     z29.d, #0
.Ltransfer_sme_load_b_g2_loop:
    ld1b    {z0.b-z1.b}, pn8/z, [x8]
    add     z28.d, z28.d, z0.d
    add     z29.d, z29.d, z1.d
    addvl   x8, x8, #2
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_b_g2_next
    mov     x8, x7
.Ltransfer_sme_load_b_g2_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_b_g2_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    uaddv   d1, p1, z29.d
    fmov    x10, d1
    ror     x10, x10, #13
    eor     x0, x0, x10
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_b_g2_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_b_g4
_transfer_sme_load_b_g4:
    cbz     x2, .Ltransfer_sme_load_b_g4_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.b
    ptrue   p1.d
    dup     z28.d, #0
    dup     z29.d, #0
    dup     z30.d, #0
    dup     z31.d, #0
.Ltransfer_sme_load_b_g4_loop:
    ld1b    {z0.b-z3.b}, pn8/z, [x8]
    add     z28.d, z28.d, z0.d
    add     z29.d, z29.d, z1.d
    add     z30.d, z30.d, z2.d
    add     z31.d, z31.d, z3.d
    addvl   x8, x8, #4
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_b_g4_next
    mov     x8, x7
.Ltransfer_sme_load_b_g4_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_b_g4_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    uaddv   d1, p1, z29.d
    fmov    x10, d1
    ror     x10, x10, #13
    eor     x0, x0, x10
    uaddv   d2, p1, z30.d
    fmov    x10, d2
    ror     x10, x10, #26
    eor     x0, x0, x10
    uaddv   d3, p1, z31.d
    fmov    x10, d3
    ror     x10, x10, #39
    eor     x0, x0, x10
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_b_g4_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_h_g1
_transfer_sme_load_h_g1:
    cbz     x2, .Ltransfer_sme_load_h_g1_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   p0.h
    ptrue   p1.d
    dup     z28.d, #0
.Ltransfer_sme_load_h_g1_loop:
    ld1h    {z0.h}, p0/z, [x8]
    add     z28.d, z28.d, z0.d
    addvl   x8, x8, #1
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_h_g1_next
    mov     x8, x7
.Ltransfer_sme_load_h_g1_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_h_g1_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_h_g1_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_h_g2
_transfer_sme_load_h_g2:
    cbz     x2, .Ltransfer_sme_load_h_g2_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.h
    ptrue   p1.d
    dup     z28.d, #0
    dup     z29.d, #0
.Ltransfer_sme_load_h_g2_loop:
    ld1h    {z0.h-z1.h}, pn8/z, [x8]
    add     z28.d, z28.d, z0.d
    add     z29.d, z29.d, z1.d
    addvl   x8, x8, #2
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_h_g2_next
    mov     x8, x7
.Ltransfer_sme_load_h_g2_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_h_g2_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    uaddv   d1, p1, z29.d
    fmov    x10, d1
    ror     x10, x10, #13
    eor     x0, x0, x10
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_h_g2_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_h_g4
_transfer_sme_load_h_g4:
    cbz     x2, .Ltransfer_sme_load_h_g4_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.h
    ptrue   p1.d
    dup     z28.d, #0
    dup     z29.d, #0
    dup     z30.d, #0
    dup     z31.d, #0
.Ltransfer_sme_load_h_g4_loop:
    ld1h    {z0.h-z3.h}, pn8/z, [x8]
    add     z28.d, z28.d, z0.d
    add     z29.d, z29.d, z1.d
    add     z30.d, z30.d, z2.d
    add     z31.d, z31.d, z3.d
    addvl   x8, x8, #4
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_h_g4_next
    mov     x8, x7
.Ltransfer_sme_load_h_g4_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_h_g4_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    uaddv   d1, p1, z29.d
    fmov    x10, d1
    ror     x10, x10, #13
    eor     x0, x0, x10
    uaddv   d2, p1, z30.d
    fmov    x10, d2
    ror     x10, x10, #26
    eor     x0, x0, x10
    uaddv   d3, p1, z31.d
    fmov    x10, d3
    ror     x10, x10, #39
    eor     x0, x0, x10
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_h_g4_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_w_g1
_transfer_sme_load_w_g1:
    cbz     x2, .Ltransfer_sme_load_w_g1_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   p0.s
    ptrue   p1.d
    dup     z28.d, #0
.Ltransfer_sme_load_w_g1_loop:
    ld1w    {z0.s}, p0/z, [x8]
    add     z28.d, z28.d, z0.d
    addvl   x8, x8, #1
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_w_g1_next
    mov     x8, x7
.Ltransfer_sme_load_w_g1_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_w_g1_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_w_g1_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_w_g2
_transfer_sme_load_w_g2:
    cbz     x2, .Ltransfer_sme_load_w_g2_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.s
    ptrue   p1.d
    dup     z28.d, #0
    dup     z29.d, #0
.Ltransfer_sme_load_w_g2_loop:
    ld1w    {z0.s-z1.s}, pn8/z, [x8]
    add     z28.d, z28.d, z0.d
    add     z29.d, z29.d, z1.d
    addvl   x8, x8, #2
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_w_g2_next
    mov     x8, x7
.Ltransfer_sme_load_w_g2_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_w_g2_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    uaddv   d1, p1, z29.d
    fmov    x10, d1
    ror     x10, x10, #13
    eor     x0, x0, x10
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_w_g2_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_w_g4
_transfer_sme_load_w_g4:
    cbz     x2, .Ltransfer_sme_load_w_g4_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.s
    ptrue   p1.d
    dup     z28.d, #0
    dup     z29.d, #0
    dup     z30.d, #0
    dup     z31.d, #0
.Ltransfer_sme_load_w_g4_loop:
    ld1w    {z0.s-z3.s}, pn8/z, [x8]
    add     z28.d, z28.d, z0.d
    add     z29.d, z29.d, z1.d
    add     z30.d, z30.d, z2.d
    add     z31.d, z31.d, z3.d
    addvl   x8, x8, #4
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_w_g4_next
    mov     x8, x7
.Ltransfer_sme_load_w_g4_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_w_g4_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    uaddv   d1, p1, z29.d
    fmov    x10, d1
    ror     x10, x10, #13
    eor     x0, x0, x10
    uaddv   d2, p1, z30.d
    fmov    x10, d2
    ror     x10, x10, #26
    eor     x0, x0, x10
    uaddv   d3, p1, z31.d
    fmov    x10, d3
    ror     x10, x10, #39
    eor     x0, x0, x10
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_w_g4_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_d_g1
_transfer_sme_load_d_g1:
    cbz     x2, .Ltransfer_sme_load_d_g1_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   p0.d
    ptrue   p1.d
    dup     z28.d, #0
.Ltransfer_sme_load_d_g1_loop:
    ld1d    {z0.d}, p0/z, [x8]
    add     z28.d, z28.d, z0.d
    addvl   x8, x8, #1
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_d_g1_next
    mov     x8, x7
.Ltransfer_sme_load_d_g1_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_d_g1_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_d_g1_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_d_g2
_transfer_sme_load_d_g2:
    cbz     x2, .Ltransfer_sme_load_d_g2_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.d
    ptrue   p1.d
    dup     z28.d, #0
    dup     z29.d, #0
.Ltransfer_sme_load_d_g2_loop:
    ld1d    {z0.d-z1.d}, pn8/z, [x8]
    add     z28.d, z28.d, z0.d
    add     z29.d, z29.d, z1.d
    addvl   x8, x8, #2
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_d_g2_next
    mov     x8, x7
.Ltransfer_sme_load_d_g2_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_d_g2_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    uaddv   d1, p1, z29.d
    fmov    x10, d1
    ror     x10, x10, #13
    eor     x0, x0, x10
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_d_g2_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_load_d_g4
_transfer_sme_load_d_g4:
    cbz     x2, .Ltransfer_sme_load_d_g4_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.d
    ptrue   p1.d
    dup     z28.d, #0
    dup     z29.d, #0
    dup     z30.d, #0
    dup     z31.d, #0
.Ltransfer_sme_load_d_g4_loop:
    ld1d    {z0.d-z3.d}, pn8/z, [x8]
    add     z28.d, z28.d, z0.d
    add     z29.d, z29.d, z1.d
    add     z30.d, z30.d, z2.d
    add     z31.d, z31.d, z3.d
    addvl   x8, x8, #4
    cmp     x8, x9
    b.lo    .Ltransfer_sme_load_d_g4_next
    mov     x8, x7
.Ltransfer_sme_load_d_g4_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_load_d_g4_loop
    uaddv   d0, p1, z28.d
    fmov    x0, d0
    uaddv   d1, p1, z29.d
    fmov    x10, d1
    ror     x10, x10, #13
    eor     x0, x0, x10
    uaddv   d2, p1, z30.d
    fmov    x10, d2
    ror     x10, x10, #26
    eor     x0, x0, x10
    uaddv   d3, p1, z31.d
    fmov    x10, d3
    ror     x10, x10, #39
    eor     x0, x0, x10
    eor     x0, x0, x6
    native_LEAVE_SM
.Ltransfer_sme_load_d_g4_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_b_g1
_transfer_sme_store_b_g1:
    cbz     x2, .Ltransfer_sme_store_b_g1_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   p0.b
    dup     z0.b, w3
.Ltransfer_sme_store_b_g1_loop:
    st1b    {z0.b}, p0, [x8]
    addvl   x8, x8, #1
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_b_g1_next
    mov     x8, x7
.Ltransfer_sme_store_b_g1_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_b_g1_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #1
    native_LEAVE_SM
.Ltransfer_sme_store_b_g1_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_b_g2
_transfer_sme_store_b_g2:
    cbz     x2, .Ltransfer_sme_store_b_g2_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.b
    dup     z0.b, w3
    add     w14, w3, #1
    dup     z1.b, w14
.Ltransfer_sme_store_b_g2_loop:
    st1b    {z0.b-z1.b}, pn8, [x8]
    addvl   x8, x8, #2
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_b_g2_next
    mov     x8, x7
.Ltransfer_sme_store_b_g2_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_b_g2_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #2
    native_LEAVE_SM
.Ltransfer_sme_store_b_g2_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_b_g4
_transfer_sme_store_b_g4:
    cbz     x2, .Ltransfer_sme_store_b_g4_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.b
    dup     z0.b, w3
    add     w14, w3, #1
    dup     z1.b, w14
    add     w15, w3, #2
    dup     z2.b, w15
    add     w16, w3, #3
    dup     z3.b, w16
.Ltransfer_sme_store_b_g4_loop:
    st1b    {z0.b-z3.b}, pn8, [x8]
    addvl   x8, x8, #4
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_b_g4_next
    mov     x8, x7
.Ltransfer_sme_store_b_g4_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_b_g4_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #4
    native_LEAVE_SM
.Ltransfer_sme_store_b_g4_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_h_g1
_transfer_sme_store_h_g1:
    cbz     x2, .Ltransfer_sme_store_h_g1_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   p0.h
    dup     z0.h, w3
.Ltransfer_sme_store_h_g1_loop:
    st1h    {z0.h}, p0, [x8]
    addvl   x8, x8, #1
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_h_g1_next
    mov     x8, x7
.Ltransfer_sme_store_h_g1_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_h_g1_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #1
    native_LEAVE_SM
.Ltransfer_sme_store_h_g1_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_h_g2
_transfer_sme_store_h_g2:
    cbz     x2, .Ltransfer_sme_store_h_g2_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.h
    dup     z0.h, w3
    add     w14, w3, #1
    dup     z1.h, w14
.Ltransfer_sme_store_h_g2_loop:
    st1h    {z0.h-z1.h}, pn8, [x8]
    addvl   x8, x8, #2
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_h_g2_next
    mov     x8, x7
.Ltransfer_sme_store_h_g2_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_h_g2_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #2
    native_LEAVE_SM
.Ltransfer_sme_store_h_g2_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_h_g4
_transfer_sme_store_h_g4:
    cbz     x2, .Ltransfer_sme_store_h_g4_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.h
    dup     z0.h, w3
    add     w14, w3, #1
    dup     z1.h, w14
    add     w15, w3, #2
    dup     z2.h, w15
    add     w16, w3, #3
    dup     z3.h, w16
.Ltransfer_sme_store_h_g4_loop:
    st1h    {z0.h-z3.h}, pn8, [x8]
    addvl   x8, x8, #4
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_h_g4_next
    mov     x8, x7
.Ltransfer_sme_store_h_g4_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_h_g4_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #4
    native_LEAVE_SM
.Ltransfer_sme_store_h_g4_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_w_g1
_transfer_sme_store_w_g1:
    cbz     x2, .Ltransfer_sme_store_w_g1_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   p0.s
    dup     z0.s, w3
.Ltransfer_sme_store_w_g1_loop:
    st1w    {z0.s}, p0, [x8]
    addvl   x8, x8, #1
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_w_g1_next
    mov     x8, x7
.Ltransfer_sme_store_w_g1_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_w_g1_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #1
    native_LEAVE_SM
.Ltransfer_sme_store_w_g1_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_w_g2
_transfer_sme_store_w_g2:
    cbz     x2, .Ltransfer_sme_store_w_g2_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.s
    dup     z0.s, w3
    add     w14, w3, #1
    dup     z1.s, w14
.Ltransfer_sme_store_w_g2_loop:
    st1w    {z0.s-z1.s}, pn8, [x8]
    addvl   x8, x8, #2
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_w_g2_next
    mov     x8, x7
.Ltransfer_sme_store_w_g2_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_w_g2_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #2
    native_LEAVE_SM
.Ltransfer_sme_store_w_g2_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_w_g4
_transfer_sme_store_w_g4:
    cbz     x2, .Ltransfer_sme_store_w_g4_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.s
    dup     z0.s, w3
    add     w14, w3, #1
    dup     z1.s, w14
    add     w15, w3, #2
    dup     z2.s, w15
    add     w16, w3, #3
    dup     z3.s, w16
.Ltransfer_sme_store_w_g4_loop:
    st1w    {z0.s-z3.s}, pn8, [x8]
    addvl   x8, x8, #4
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_w_g4_next
    mov     x8, x7
.Ltransfer_sme_store_w_g4_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_w_g4_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #4
    native_LEAVE_SM
.Ltransfer_sme_store_w_g4_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_d_g1
_transfer_sme_store_d_g1:
    cbz     x2, .Ltransfer_sme_store_d_g1_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   p0.d
    dup     z0.d, x3
.Ltransfer_sme_store_d_g1_loop:
    st1d    {z0.d}, p0, [x8]
    addvl   x8, x8, #1
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_d_g1_next
    mov     x8, x7
.Ltransfer_sme_store_d_g1_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_d_g1_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #1
    native_LEAVE_SM
.Ltransfer_sme_store_d_g1_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_d_g2
_transfer_sme_store_d_g2:
    cbz     x2, .Ltransfer_sme_store_d_g2_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.d
    dup     z0.d, x3
    add     x14, x3, #1
    dup     z1.d, x14
.Ltransfer_sme_store_d_g2_loop:
    st1d    {z0.d-z1.d}, pn8, [x8]
    addvl   x8, x8, #2
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_d_g2_next
    mov     x8, x7
.Ltransfer_sme_store_d_g2_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_d_g2_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #2
    native_LEAVE_SM
.Ltransfer_sme_store_d_g2_zero:
    mov     x0, #0
    ret

    .globl _transfer_sme_store_d_g4
_transfer_sme_store_d_g4:
    cbz     x2, .Ltransfer_sme_store_d_g4_zero
    mov     x6, x2
    mov     x7, x0
    add     x9, x0, x1
    mov     x8, x0
    native_ENTER_SM
    ptrue   pn8.d
    dup     z0.d, x3
    add     x14, x3, #1
    dup     z1.d, x14
    add     x15, x3, #2
    dup     z2.d, x15
    add     x16, x3, #3
    dup     z3.d, x16
.Ltransfer_sme_store_d_g4_loop:
    st1d    {z0.d-z3.d}, pn8, [x8]
    addvl   x8, x8, #4
    cmp     x8, x9
    b.lo    .Ltransfer_sme_store_d_g4_next
    mov     x8, x7
.Ltransfer_sme_store_d_g4_next:
    subs    x2, x2, #1
    b.ne    .Ltransfer_sme_store_d_g4_loop
    dsb     ish
    eor     x0, x3, x6
    eor     x0, x0, x1
    eor     x0, x0, #4
    native_LEAVE_SM
.Ltransfer_sme_store_d_g4_zero:
    mov     x0, #0
    ret
