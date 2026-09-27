    .text
    .align  4

// Fixed-M FP32 row kernels for the four-vector main-N route.  The public
// dispatcher has already proved the exact outer M before entering these bodies;
// x0 (mc) is therefore deliberately not used by any body below.

.macro LOWM16_DIRECT_K1
    ld1w    {z0.s-z3.s}, pn9/z, [x19]
    ld1w    {z4.s}, p4/z, [x24]
    fmopa   za0.s, p4/m, p0/m, z4.s, z0.s
    fmopa   za1.s, p4/m, p1/m, z4.s, z1.s
    fmopa   za2.s, p4/m, p2/m, z4.s, z2.s
    fmopa   za3.s, p4/m, p3/m, z4.s, z3.s
    st1w    {z0.s-z3.s}, pn9, [x10]
    add     x19, x19, x8, lsl #2
    addvl   x24, x24, #1
    addvl   x10, x10, #4
.endm

.macro LOWM16_DIRECT_K16
    .rept 16
        LOWM16_DIRECT_K1
    .endr
.endm

.macro LOWM16_PACKED_K1
    ld1w    {z0.s-z3.s}, pn9/z, [x10]
    ld1w    {z4.s}, p4/z, [x24]
    fmopa   za0.s, p4/m, p0/m, z4.s, z0.s
    fmopa   za1.s, p4/m, p1/m, z4.s, z1.s
    fmopa   za2.s, p4/m, p2/m, z4.s, z2.s
    fmopa   za3.s, p4/m, p3/m, z4.s, z3.s
    addvl   x24, x24, #1
    addvl   x10, x10, #4
.endm

.macro LOWM16_PACKED_K16
    .rept 16
        LOWM16_PACKED_K1
    .endr
.endm

// x11 is the first C row.  Compile-time repetition emits a transfer only for
// rows present in this exact-M panel.
.macro LOWM16_LOAD_C rows
    mov     w12, #0
    .rept \rows
        ld1w    {z0.s}, p0/z, [x11]
        mova    za0h.s[w12, 0], p0/m, z0.s
        ld1w    {z0.s}, p1/z, [x11, #1, MUL VL]
        mova    za1h.s[w12, 0], p1/m, z0.s
        ld1w    {z0.s}, p2/z, [x11, #2, MUL VL]
        mova    za2h.s[w12, 0], p2/m, z0.s
        ld1w    {z0.s}, p3/z, [x11, #3, MUL VL]
        mova    za3h.s[w12, 0], p3/m, z0.s
        add     x11, x11, x8, lsl #2
        add     w12, w12, #1
    .endr
.endm

.macro LOWM16_STORE_C rows
    mov     w12, #0
    .rept \rows
        mova    z0.s, p0/m, za0h.s[w12, 0]
        st1w    {z0.s}, p0, [x11]
        mova    z0.s, p1/m, za1h.s[w12, 0]
        st1w    {z0.s}, p1, [x11, #1, MUL VL]
        mova    z0.s, p2/m, za2h.s[w12, 0]
        st1w    {z0.s}, p2, [x11, #2, MUL VL]
        mova    z0.s, p3/m, za3h.s[w12, 0]
        st1w    {z0.s}, p3, [x11, #3, MUL VL]
        add     x11, x11, x8, lsl #2
        add     w12, w12, #1
    .endr
.endm

.macro LOWM16_DIRECT_COMPUTE
    lsr     x23, x1, #4
    and     x13, x1, #0xf
    cbz     x23, .Llowm16_direct_tail_\@
.Llowm16_direct_k16_\@:
    LOWM16_DIRECT_K16
    subs    x23, x23, #1
    b.ne    .Llowm16_direct_k16_\@
.Llowm16_direct_tail_\@:
    cbz     x13, .Llowm16_direct_done_\@
.Llowm16_direct_k1_\@:
    LOWM16_DIRECT_K1
    subs    x13, x13, #1
    b.ne    .Llowm16_direct_k1_\@
.Llowm16_direct_done_\@:
.endm

.macro LOWM16_PACKED_COMPUTE
    lsr     x23, x1, #4
    and     x13, x1, #0xf
    cbz     x23, .Llowm16_packed_tail_\@
.Llowm16_packed_k16_\@:
    LOWM16_PACKED_K16
    subs    x23, x23, #1
    b.ne    .Llowm16_packed_k16_\@
.Llowm16_packed_tail_\@:
    cbz     x13, .Llowm16_packed_done_\@
.Llowm16_packed_k1_\@:
    LOWM16_PACKED_K1
    subs    x13, x13, #1
    b.ne    .Llowm16_packed_k1_\@
.Llowm16_packed_done_\@:
.endm

// The two-panel expansion is used only for exact M=8 when the architectural
// streaming FP32 vector length is four lanes.  It is fixed control, not an mc
// loop: panel zero materializes B and panel one consumes that packed image.
.macro LOWM16_N_TRAVERSAL panel_rows, two_panels
    mov     x20, x6
    mov     x16, x3
    mov     x10, x5
    mov     x17, #0
    whilelt pn9.s, x17, x2, vlx4
.Llowm16_n_\@:
    pext    {p0.s, p1.s}, pn9[0]
    pext    {p2.s, p3.s}, pn9[1]
    mov     x21, x10

    zero    {za}
    cmp     x9, x7
    b.lt    .Llowm16_first_pc_\@
    mov     x11, x20
    LOWM16_LOAD_C \panel_rows
.Llowm16_first_pc_\@:
    mov     x19, x16
    mov     x24, x4
    LOWM16_DIRECT_COMPUTE
    mov     x11, x20
    LOWM16_STORE_C \panel_rows

    .if \two_panels
        zero    {za}
        mul     x22, x12, x8
        add     x11, x20, x22, lsl #2
        cmp     x9, x7
        b.lt    .Llowm16_second_first_pc_\@
        LOWM16_LOAD_C \panel_rows
.Llowm16_second_first_pc_\@:
        mul     x22, x12, x1
        add     x24, x4, x22, lsl #2
        mov     x10, x21
        LOWM16_PACKED_COMPUTE
        mul     x22, x12, x8
        add     x11, x20, x22, lsl #2
        LOWM16_STORE_C \panel_rows
    .endif

    addvl   x16, x16, #4
    addvl   x20, x20, #4
    incw    x17, all, mul #4
    whilelt pn9.s, x17, x2, vlx4
    b.first .Llowm16_n_\@
.endm

.macro LOWM16_BODY exact_m
    // Added M=5/6 bodies are single-panel only. Fall back before SMSTART
    // if the actual streaming vector cannot contain every fixed row.
    // This also protects direct calls through the ABI-preserving wrappers.
    .if (\exact_m == 5) || (\exact_m == 6)
        rdsvl   x16, #1
        cmp     x16, #(\exact_m * 4)
        b.ge    .Llowm16_added_svl_ok_\@
        b       _row_kernel_16x64
.Llowm16_added_svl_ok_\@:
    .endif
    // x0:mc (ignored), x1:kc, x2:nc, x3:XB, x4:packa, x5:packb,
    // x6:c, x7:KC, stack:ldc,pc.
    ldr     w8, [sp]
    ldr     w9, [sp, #4]
    stp     x19, x20, [sp, #-48]!
    stp     x21, x22, [sp, #16]
    stp     x23, x24, [sp, #32]
    smstart
    cntw    x12

    .if \exact_m == 8
        cmp     x12, #8
        b.lt    .Llowm16_two_panels_\@
    .endif
    mov     x14, #\exact_m
    mov     x13, #0
    whilelt p4.s, x13, x14
    LOWM16_N_TRAVERSAL \exact_m, 0
    .if \exact_m == 8
        b       .Llowm16_body_done_\@
.Llowm16_two_panels_\@:
        mov     x14, #4
        mov     x13, #0
        whilelt p4.s, x13, x14
        LOWM16_N_TRAVERSAL 4, 1
.Llowm16_body_done_\@:
    .endif

    smstop
    ldp     x23, x24, [sp, #32]
    ldp     x21, x22, [sp, #16]
    ldp     x19, x20, [sp], #48
    ret
.endm

    .global _row_kernel_m1_16x64
_row_kernel_m1_16x64:
    LOWM16_BODY 1

    .global _row_kernel_m2_16x64
_row_kernel_m2_16x64:
    LOWM16_BODY 2

    .global _row_kernel_m3_16x64
_row_kernel_m3_16x64:
    LOWM16_BODY 3

    .global _row_kernel_m4_16x64
_row_kernel_m4_16x64:
    LOWM16_BODY 4

    .global _row_kernel_m5_16x64
_row_kernel_m5_16x64:
    LOWM16_BODY 5

    .global _row_kernel_m6_16x64
_row_kernel_m6_16x64:
    LOWM16_BODY 6

    .global _row_kernel_m8_16x64
_row_kernel_m8_16x64:
    LOWM16_BODY 8
