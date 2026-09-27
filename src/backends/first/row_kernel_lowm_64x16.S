    .text
    .align  4

// Fixed-M FP32 row kernels for the one-vector N-tail route.  Runtime mc is not
// referenced: exact outer-M dispatch is the sole specialization selector.

.macro LOWM64_DIRECT_K1
    ld1w    {z0.s}, p4/z, [x19]
    ld1w    {z1.s}, p0/z, [x24]
    fmopa   za0.s, p0/m, p4/m, z1.s, z0.s
    st1w    {z0.s}, p4, [x10]
    add     x19, x19, x8, lsl #2
    addvl   x24, x24, #1
    addvl   x10, x10, #1
.endm

.macro LOWM64_DIRECT_K16
    .rept 16
        LOWM64_DIRECT_K1
    .endr
.endm

.macro LOWM64_PACKED_K1
    ld1w    {z0.s}, p4/z, [x10]
    ld1w    {z1.s}, p0/z, [x24]
    fmopa   za0.s, p0/m, p4/m, z1.s, z0.s
    addvl   x24, x24, #1
    addvl   x10, x10, #1
.endm

.macro LOWM64_PACKED_K16
    .rept 16
        LOWM64_PACKED_K1
    .endr
.endm

.macro LOWM64_LOAD_C rows
    mov     w12, #0
    .rept \rows
        ld1w    {z0.s}, p4/z, [x11]
        mova    za0h.s[w12, 0], p4/m, z0.s
        add     x11, x11, x8, lsl #2
        add     w12, w12, #1
    .endr
.endm

.macro LOWM64_STORE_C rows
    mov     w12, #0
    .rept \rows
        mova    z0.s, p4/m, za0h.s[w12, 0]
        st1w    {z0.s}, p4, [x11]
        add     x11, x11, x8, lsl #2
        add     w12, w12, #1
    .endr
.endm

.macro LOWM64_DIRECT_COMPUTE
    lsr     x23, x1, #4
    and     x13, x1, #0xf
    cbz     x23, .Llowm64_direct_tail_\@
.Llowm64_direct_k16_\@:
    LOWM64_DIRECT_K16
    subs    x23, x23, #1
    b.ne    .Llowm64_direct_k16_\@
.Llowm64_direct_tail_\@:
    cbz     x13, .Llowm64_direct_done_\@
.Llowm64_direct_k1_\@:
    LOWM64_DIRECT_K1
    subs    x13, x13, #1
    b.ne    .Llowm64_direct_k1_\@
.Llowm64_direct_done_\@:
.endm

.macro LOWM64_PACKED_COMPUTE
    lsr     x23, x1, #4
    and     x13, x1, #0xf
    cbz     x23, .Llowm64_packed_tail_\@
.Llowm64_packed_k16_\@:
    LOWM64_PACKED_K16
    subs    x23, x23, #1
    b.ne    .Llowm64_packed_k16_\@
.Llowm64_packed_tail_\@:
    cbz     x13, .Llowm64_packed_done_\@
.Llowm64_packed_k1_\@:
    LOWM64_PACKED_K1
    subs    x13, x13, #1
    b.ne    .Llowm64_packed_k1_\@
.Llowm64_packed_done_\@:
.endm

.macro LOWM64_N_TRAVERSAL panel_rows, two_panels
    mov     x20, x6
    mov     x16, x3
    mov     x10, x5
    mov     x17, #0
    whilelt p4.s, x17, x2
.Llowm64_n_\@:
    mov     x21, x10

    zero    {za}
    cmp     x9, x7
    b.lt    .Llowm64_first_pc_\@
    mov     x11, x20
    LOWM64_LOAD_C \panel_rows
.Llowm64_first_pc_\@:
    mov     x19, x16
    mov     x24, x4
    LOWM64_DIRECT_COMPUTE
    mov     x11, x20
    LOWM64_STORE_C \panel_rows

    .if \two_panels
        zero    {za}
        mul     x22, x12, x8
        add     x11, x20, x22, lsl #2
        cmp     x9, x7
        b.lt    .Llowm64_second_first_pc_\@
        LOWM64_LOAD_C \panel_rows
.Llowm64_second_first_pc_\@:
        mul     x22, x12, x1
        add     x24, x4, x22, lsl #2
        mov     x10, x21
        LOWM64_PACKED_COMPUTE
        mul     x22, x12, x8
        add     x11, x20, x22, lsl #2
        LOWM64_STORE_C \panel_rows
    .endif

    addvl   x16, x16, #1
    addvl   x20, x20, #1
    incw    x17
    whilelt p4.s, x17, x2
    b.first .Llowm64_n_\@
.endm

.macro LOWM64_BODY exact_m
    // Added M=5/6 bodies are single-panel only. Fall back before SMSTART
    // if the actual streaming vector cannot contain every fixed row.
    // This also protects direct calls through the ABI-preserving wrappers.
    .if (\exact_m == 5) || (\exact_m == 6)
        rdsvl   x16, #1
        cmp     x16, #(\exact_m * 4)
        b.ge    .Llowm64_added_svl_ok_\@
        b       _row_kernel_64x16
.Llowm64_added_svl_ok_\@:
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
        b.lt    .Llowm64_two_panels_\@
    .endif
    mov     x14, #\exact_m
    mov     x13, #0
    whilelt p0.s, x13, x14
    LOWM64_N_TRAVERSAL \exact_m, 0
    .if \exact_m == 8
        b       .Llowm64_body_done_\@
.Llowm64_two_panels_\@:
        mov     x14, #4
        mov     x13, #0
        whilelt p0.s, x13, x14
        LOWM64_N_TRAVERSAL 4, 1
.Llowm64_body_done_\@:
    .endif

    smstop
    ldp     x23, x24, [sp, #32]
    ldp     x21, x22, [sp, #16]
    ldp     x19, x20, [sp], #48
    ret
.endm

    .global _row_kernel_m1_64x16
_row_kernel_m1_64x16:
    LOWM64_BODY 1

    .global _row_kernel_m2_64x16
_row_kernel_m2_64x16:
    LOWM64_BODY 2

    .global _row_kernel_m3_64x16
_row_kernel_m3_64x16:
    LOWM64_BODY 3

    .global _row_kernel_m4_64x16
_row_kernel_m4_64x16:
    LOWM64_BODY 4

    .global _row_kernel_m5_64x16
_row_kernel_m5_64x16:
    LOWM64_BODY 5

    .global _row_kernel_m6_64x16
_row_kernel_m6_64x16:
    LOWM64_BODY 6

    .global _row_kernel_m8_64x16
_row_kernel_m8_64x16:
    LOWM64_BODY 8
