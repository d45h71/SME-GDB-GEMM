// native local SME outer-product bodies. Darwin arm64 ABI for every body:
// x0=rounds (1..1024), x1=private output base. Each body is a leaf, touches
// only caller-saved state, enters/leaves streaming mode, and materializes all
// active accumulator tiles after (not inside) the arithmetic loop. There is
// intentionally no INT32-to-INT32 body: SME has no matching integer MOPA.
        .arch armv9-a+sme2+sme+sme-f64f64
        .text
        .p2align 2

        .globl _compute_sme_svl_bytes
_compute_sme_svl_bytes:
        // Same leaf form as the accepted source-provenance SVL helper.
        rdsvl   x0, #1
        ret

        .p2align 2
        .globl _compute_i8_t1
_compute_i8_t1:
        // SMOPA b->s, exactly 1 distinct ZA tile per loop trip.
        smstart
        ptrue   p0.b
        ptrue   p1.s
        dup     z0.b, #1
        dup     z1.b, #1
        zero    {za}
.compute_i8_t1_loop:
        smopa  za0.s, p0/m, p0/m, z0.b, z1.b
        subs    x0, x0, #1
        b.ne    .compute_i8_t1_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_i8_t2
_compute_i8_t2:
        // SMOPA b->s, exactly 2 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.b
        ptrue   p1.s
        dup     z0.b, #1
        dup     z1.b, #1
        zero    {za}
.compute_i8_t2_loop:
        smopa  za0.s, p0/m, p0/m, z0.b, z1.b
        smopa  za1.s, p0/m, p0/m, z0.b, z1.b
        subs    x0, x0, #1
        b.ne    .compute_i8_t2_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_i8_t3
_compute_i8_t3:
        // SMOPA b->s, exactly 3 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.b
        ptrue   p1.s
        dup     z0.b, #1
        dup     z1.b, #1
        zero    {za}
.compute_i8_t3_loop:
        smopa  za0.s, p0/m, p0/m, z0.b, z1.b
        smopa  za1.s, p0/m, p0/m, z0.b, z1.b
        smopa  za2.s, p0/m, p0/m, z0.b, z1.b
        subs    x0, x0, #1
        b.ne    .compute_i8_t3_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za2h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_i8_t4
_compute_i8_t4:
        // SMOPA b->s, exactly 4 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.b
        ptrue   p1.s
        dup     z0.b, #1
        dup     z1.b, #1
        zero    {za}
.compute_i8_t4_loop:
        smopa  za0.s, p0/m, p0/m, z0.b, z1.b
        smopa  za1.s, p0/m, p0/m, z0.b, z1.b
        smopa  za2.s, p0/m, p0/m, z0.b, z1.b
        smopa  za3.s, p0/m, p0/m, z0.b, z1.b
        subs    x0, x0, #1
        b.ne    .compute_i8_t4_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za2h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za3h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_i16_t1
_compute_i16_t1:
        // SMOPA h->s, exactly 1 distinct ZA tile per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        dup     z0.h, #1
        dup     z1.h, #1
        zero    {za}
.compute_i16_t1_loop:
        smopa  za0.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_i16_t1_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_i16_t2
_compute_i16_t2:
        // SMOPA h->s, exactly 2 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        dup     z0.h, #1
        dup     z1.h, #1
        zero    {za}
.compute_i16_t2_loop:
        smopa  za0.s, p0/m, p0/m, z0.h, z1.h
        smopa  za1.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_i16_t2_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_i16_t3
_compute_i16_t3:
        // SMOPA h->s, exactly 3 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        dup     z0.h, #1
        dup     z1.h, #1
        zero    {za}
.compute_i16_t3_loop:
        smopa  za0.s, p0/m, p0/m, z0.h, z1.h
        smopa  za1.s, p0/m, p0/m, z0.h, z1.h
        smopa  za2.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_i16_t3_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za2h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_i16_t4
_compute_i16_t4:
        // SMOPA h->s, exactly 4 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        dup     z0.h, #1
        dup     z1.h, #1
        zero    {za}
.compute_i16_t4_loop:
        smopa  za0.s, p0/m, p0/m, z0.h, z1.h
        smopa  za1.s, p0/m, p0/m, z0.h, z1.h
        smopa  za2.s, p0/m, p0/m, z0.h, z1.h
        smopa  za3.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_i16_t4_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za2h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za3h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp16_t1
_compute_fp16_t1:
        // FMOPA h->s, exactly 1 distinct ZA tile per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        fmov    z0.h, #1.0
        fmov    z1.h, #1.0
        zero    {za}
.compute_fp16_t1_loop:
        fmopa  za0.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_fp16_t1_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp16_t2
_compute_fp16_t2:
        // FMOPA h->s, exactly 2 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        fmov    z0.h, #1.0
        fmov    z1.h, #1.0
        zero    {za}
.compute_fp16_t2_loop:
        fmopa  za0.s, p0/m, p0/m, z0.h, z1.h
        fmopa  za1.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_fp16_t2_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp16_t3
_compute_fp16_t3:
        // FMOPA h->s, exactly 3 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        fmov    z0.h, #1.0
        fmov    z1.h, #1.0
        zero    {za}
.compute_fp16_t3_loop:
        fmopa  za0.s, p0/m, p0/m, z0.h, z1.h
        fmopa  za1.s, p0/m, p0/m, z0.h, z1.h
        fmopa  za2.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_fp16_t3_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za2h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp16_t4
_compute_fp16_t4:
        // FMOPA h->s, exactly 4 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        fmov    z0.h, #1.0
        fmov    z1.h, #1.0
        zero    {za}
.compute_fp16_t4_loop:
        fmopa  za0.s, p0/m, p0/m, z0.h, z1.h
        fmopa  za1.s, p0/m, p0/m, z0.h, z1.h
        fmopa  za2.s, p0/m, p0/m, z0.h, z1.h
        fmopa  za3.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_fp16_t4_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za2h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za3h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_bf16_t1
_compute_bf16_t1:
        // BFMOPA h->s, exactly 1 distinct ZA tile per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        mov     w9, #0x3f80
        dup     z0.h, w9
        dup     z1.h, w9
        zero    {za}
.compute_bf16_t1_loop:
        bfmopa za0.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_bf16_t1_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_bf16_t2
_compute_bf16_t2:
        // BFMOPA h->s, exactly 2 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        mov     w9, #0x3f80
        dup     z0.h, w9
        dup     z1.h, w9
        zero    {za}
.compute_bf16_t2_loop:
        bfmopa za0.s, p0/m, p0/m, z0.h, z1.h
        bfmopa za1.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_bf16_t2_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_bf16_t3
_compute_bf16_t3:
        // BFMOPA h->s, exactly 3 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        mov     w9, #0x3f80
        dup     z0.h, w9
        dup     z1.h, w9
        zero    {za}
.compute_bf16_t3_loop:
        bfmopa za0.s, p0/m, p0/m, z0.h, z1.h
        bfmopa za1.s, p0/m, p0/m, z0.h, z1.h
        bfmopa za2.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_bf16_t3_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za2h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_bf16_t4
_compute_bf16_t4:
        // BFMOPA h->s, exactly 4 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.h
        ptrue   p1.s
        mov     w9, #0x3f80
        dup     z0.h, w9
        dup     z1.h, w9
        zero    {za}
.compute_bf16_t4_loop:
        bfmopa za0.s, p0/m, p0/m, z0.h, z1.h
        bfmopa za1.s, p0/m, p0/m, z0.h, z1.h
        bfmopa za2.s, p0/m, p0/m, z0.h, z1.h
        bfmopa za3.s, p0/m, p0/m, z0.h, z1.h
        subs    x0, x0, #1
        b.ne    .compute_bf16_t4_loop
        mov     w12, #0
        mova    z2.s, p1/m, za0h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za1h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za2h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p1/m, za3h.s[w12, 0]
        st1w   {z2.s}, p1, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp32_t1
_compute_fp32_t1:
        // FMOPA s->s, exactly 1 distinct ZA tile per loop trip.
        smstart
        ptrue   p0.s
        fmov    z0.s, #1.0
        fmov    z1.s, #1.0
        zero    {za}
.compute_fp32_t1_loop:
        fmopa  za0.s, p0/m, p0/m, z0.s, z1.s
        subs    x0, x0, #1
        b.ne    .compute_fp32_t1_loop
        mov     w12, #0
        mova    z2.s, p0/m, za0h.s[w12, 0]
        st1w   {z2.s}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp32_t2
_compute_fp32_t2:
        // FMOPA s->s, exactly 2 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.s
        fmov    z0.s, #1.0
        fmov    z1.s, #1.0
        zero    {za}
.compute_fp32_t2_loop:
        fmopa  za0.s, p0/m, p0/m, z0.s, z1.s
        fmopa  za1.s, p0/m, p0/m, z0.s, z1.s
        subs    x0, x0, #1
        b.ne    .compute_fp32_t2_loop
        mov     w12, #0
        mova    z2.s, p0/m, za0h.s[w12, 0]
        st1w   {z2.s}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p0/m, za1h.s[w12, 0]
        st1w   {z2.s}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp32_t3
_compute_fp32_t3:
        // FMOPA s->s, exactly 3 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.s
        fmov    z0.s, #1.0
        fmov    z1.s, #1.0
        zero    {za}
.compute_fp32_t3_loop:
        fmopa  za0.s, p0/m, p0/m, z0.s, z1.s
        fmopa  za1.s, p0/m, p0/m, z0.s, z1.s
        fmopa  za2.s, p0/m, p0/m, z0.s, z1.s
        subs    x0, x0, #1
        b.ne    .compute_fp32_t3_loop
        mov     w12, #0
        mova    z2.s, p0/m, za0h.s[w12, 0]
        st1w   {z2.s}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p0/m, za1h.s[w12, 0]
        st1w   {z2.s}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p0/m, za2h.s[w12, 0]
        st1w   {z2.s}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp32_t4
_compute_fp32_t4:
        // FMOPA s->s, exactly 4 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.s
        fmov    z0.s, #1.0
        fmov    z1.s, #1.0
        zero    {za}
.compute_fp32_t4_loop:
        fmopa  za0.s, p0/m, p0/m, z0.s, z1.s
        fmopa  za1.s, p0/m, p0/m, z0.s, z1.s
        fmopa  za2.s, p0/m, p0/m, z0.s, z1.s
        fmopa  za3.s, p0/m, p0/m, z0.s, z1.s
        subs    x0, x0, #1
        b.ne    .compute_fp32_t4_loop
        mov     w12, #0
        mova    z2.s, p0/m, za0h.s[w12, 0]
        st1w   {z2.s}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p0/m, za1h.s[w12, 0]
        st1w   {z2.s}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p0/m, za2h.s[w12, 0]
        st1w   {z2.s}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.s, p0/m, za3h.s[w12, 0]
        st1w   {z2.s}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp64_t1
_compute_fp64_t1:
        // FMOPA d->d, exactly 1 distinct ZA tile per loop trip.
        smstart
        ptrue   p0.d
        fmov    z0.d, #1.0
        fmov    z1.d, #1.0
        zero    {za}
.compute_fp64_t1_loop:
        fmopa  za0.d, p0/m, p0/m, z0.d, z1.d
        subs    x0, x0, #1
        b.ne    .compute_fp64_t1_loop
        mov     w12, #0
        mova    z2.d, p0/m, za0h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp64_t2
_compute_fp64_t2:
        // FMOPA d->d, exactly 2 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.d
        fmov    z0.d, #1.0
        fmov    z1.d, #1.0
        zero    {za}
.compute_fp64_t2_loop:
        fmopa  za0.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za1.d, p0/m, p0/m, z0.d, z1.d
        subs    x0, x0, #1
        b.ne    .compute_fp64_t2_loop
        mov     w12, #0
        mova    z2.d, p0/m, za0h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za1h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp64_t3
_compute_fp64_t3:
        // FMOPA d->d, exactly 3 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.d
        fmov    z0.d, #1.0
        fmov    z1.d, #1.0
        zero    {za}
.compute_fp64_t3_loop:
        fmopa  za0.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za1.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za2.d, p0/m, p0/m, z0.d, z1.d
        subs    x0, x0, #1
        b.ne    .compute_fp64_t3_loop
        mov     w12, #0
        mova    z2.d, p0/m, za0h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za1h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za2h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp64_t4
_compute_fp64_t4:
        // FMOPA d->d, exactly 4 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.d
        fmov    z0.d, #1.0
        fmov    z1.d, #1.0
        zero    {za}
.compute_fp64_t4_loop:
        fmopa  za0.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za1.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za2.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za3.d, p0/m, p0/m, z0.d, z1.d
        subs    x0, x0, #1
        b.ne    .compute_fp64_t4_loop
        mov     w12, #0
        mova    z2.d, p0/m, za0h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za1h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za2h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za3h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp64_t5
_compute_fp64_t5:
        // FMOPA d->d, exactly 5 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.d
        fmov    z0.d, #1.0
        fmov    z1.d, #1.0
        zero    {za}
.compute_fp64_t5_loop:
        fmopa  za0.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za1.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za2.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za3.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za4.d, p0/m, p0/m, z0.d, z1.d
        subs    x0, x0, #1
        b.ne    .compute_fp64_t5_loop
        mov     w12, #0
        mova    z2.d, p0/m, za0h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za1h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za2h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za3h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za4h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp64_t6
_compute_fp64_t6:
        // FMOPA d->d, exactly 6 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.d
        fmov    z0.d, #1.0
        fmov    z1.d, #1.0
        zero    {za}
.compute_fp64_t6_loop:
        fmopa  za0.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za1.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za2.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za3.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za4.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za5.d, p0/m, p0/m, z0.d, z1.d
        subs    x0, x0, #1
        b.ne    .compute_fp64_t6_loop
        mov     w12, #0
        mova    z2.d, p0/m, za0h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za1h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za2h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za3h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za4h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za5h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp64_t7
_compute_fp64_t7:
        // FMOPA d->d, exactly 7 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.d
        fmov    z0.d, #1.0
        fmov    z1.d, #1.0
        zero    {za}
.compute_fp64_t7_loop:
        fmopa  za0.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za1.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za2.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za3.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za4.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za5.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za6.d, p0/m, p0/m, z0.d, z1.d
        subs    x0, x0, #1
        b.ne    .compute_fp64_t7_loop
        mov     w12, #0
        mova    z2.d, p0/m, za0h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za1h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za2h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za3h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za4h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za5h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za6h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        smstop
        ret
 
        .p2align 2
        .globl _compute_fp64_t8
_compute_fp64_t8:
        // FMOPA d->d, exactly 8 distinct ZA tiles per loop trip.
        smstart
        ptrue   p0.d
        fmov    z0.d, #1.0
        fmov    z1.d, #1.0
        zero    {za}
.compute_fp64_t8_loop:
        fmopa  za0.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za1.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za2.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za3.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za4.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za5.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za6.d, p0/m, p0/m, z0.d, z1.d
        fmopa  za7.d, p0/m, p0/m, z0.d, z1.d
        subs    x0, x0, #1
        b.ne    .compute_fp64_t8_loop
        mov     w12, #0
        mova    z2.d, p0/m, za0h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za1h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za2h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za3h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za4h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za5h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za6h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        addvl   x1, x1, #1
        mova    z2.d, p0/m, za7h.d[w12, 0]
        st1d   {z2.d}, p0, [x1]
        smstop
        ret
 
