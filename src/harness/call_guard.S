// Darwin arm64. One non-reentrant main worker-thread invocation.
// Normal nonstreaming/no-live-ZA, default FPCR only. No C++ after an unsafe return.
// Direct verified integer-only mach_absolute_time leaf; boot-stable nonzero
// commpage selector is an explicit runtime assumption, checked before/after.
.text
.p2align 2
.globl _matrix_guard_call
_matrix_guard_call:
 mov x9, sp
 tst x9, #15
 b.ne Lentry_bad
 mrs x9, S3_3_C4_C2_2
 cbnz x9, Lentry_bad
 mrs x9, FPCR
 cbnz x9, Lentry_bad
 adrp x9, _matrix_guard_anchor@PAGE
 ldr x10, [x9, _matrix_guard_anchor@PAGEOFF]
 cbnz x10, Lentry_bad
 sub sp, sp, #384
 stp x19, x20, [sp, #16]
 stp x21, x22, [sp, #32]
 stp x23, x24, [sp, #48]
 stp x25, x26, [sp, #64]
 stp x27, x28, [sp, #80]
 stp x29, x30, [sp, #96]
 stp q8, q9, [sp, #112]
 stp q10, q11, [sp, #144]
 stp q12, q13, [sp, #176]
 stp q14, q15, [sp, #208]
 str x0, [sp, #240]
 mov x10, sp
 str x10, [x9, _matrix_guard_anchor@PAGEOFF]
 ldr x11, [x0, #112]
 str x10, [x11, #32]
 str xzr, [x11, #0]
 str xzr, [x11, #16]
 ldr x12, [x0, #72]
 str x12, [sp]
 mov x12, #0x5a6b
 str x12, [sp, #256]
 mov x19, #4865
 mov x20, #5121
 mov x21, #5377
 mov x22, #5633
 mov x23, #5889
 mov x24, #6145
 mov x25, #6401
 mov x26, #6657
 mov x27, #6913
 mov x28, #7169
 add x29, sp, #96
 mov x12, #2056
 fmov d8, x12
 mov x12, #2184
 ins v8.d[1], x12
 mov x12, #2313
 fmov d9, x12
 mov x12, #2441
 ins v9.d[1], x12
 mov x12, #2570
 fmov d10, x12
 mov x12, #2698
 ins v10.d[1], x12
 mov x12, #2827
 fmov d11, x12
 mov x12, #2955
 ins v11.d[1], x12
 mov x12, #3084
 fmov d12, x12
 mov x12, #3212
 ins v12.d[1], x12
 mov x12, #3341
 fmov d13, x12
 mov x12, #3469
 ins v13.d[1], x12
 mov x12, #3598
 fmov d14, x12
 mov x12, #3726
 ins v14.d[1], x12
 mov x12, #3855
 fmov d15, x12
 mov x12, #3983
 ins v15.d[1], x12
 ldr x12, [x0, #80]
 cbz x12, Larguments
 mov x3, #0xc090
 movk x3, #0xffff, lsl #16
 movk x3, #0xf, lsl #32
 ldrb w12, [x3]
 cbz w12, Lclock_bad
 str x12, [sp, #264]
 adrp x16, _matrix_clock_pointer@PAGE
 ldr x16, [x16, _matrix_clock_pointer@PAGEOFF]
 blr x16
 ldr x9, [sp, #240]
 str x0, [x9, #88]
Larguments:
 ldr x9, [sp, #240]
 ldr x16, [x9]
 ldp x0, x1, [x9, #8]
 ldp x2, x3, [x9, #24]
 ldp x4, x5, [x9, #40]
 ldp x6, x7, [x9, #56]
 blr x16
Ltarget_returned:
 // Clock proven to use only x0-x3 and NZCV in admitted nonzero selector paths.
 mov x17, x0
 adrp x9, _matrix_guard_anchor@PAGE
 ldr x9, [x9, _matrix_guard_anchor@PAGEOFF]
 str x30, [x9, #272]
 ldr x10, [x9, #240]
 ldr x11, [x10, #80]
 cbz x11, Lcaptured_tick
 adrp x16, _matrix_clock_pointer@PAGE
 ldr x16, [x16, _matrix_clock_pointer@PAGEOFF]
 blr x16
 mov x14, x0
 b Lafter_clock
Lcaptured_tick:
 mov x14, #0
Lafter_clock:
 mov x15, sp
 adrp x9, _matrix_guard_anchor@PAGE
 ldr x9, [x9, _matrix_guard_anchor@PAGEOFF]
 ldr x10, [x9, #240]
 str x17, [x10, #104]
 str x14, [x10, #96]
 ldr x11, [x10, #112]
 str x15, [x11, #40]
 mrs x12, S3_3_C4_C2_2
 str x12, [x11, #8]
 mrs x13, FPCR
 str x13, [x11, #24]
 mov x14, #1
 str x14, [x11, #72]
 cbnz x12, Lmode_fatal
 mov sp, x9
 // FPCR fault is recorded, then restore required original zero before SIMD/C++.
 msr FPCR, xzr
 mov x14, #0
 cmp x15, x9
 cset x14, ne
 cbz x13, Lfpcr_ok
 orr x14, x14, #2
Lfpcr_ok:
 ldr x12, [sp, #256]
 mov x13, #0x5a6b
 cmp x12, x13
 b.eq Lcanary_ok
 orr x14, x14, #4
Lcanary_ok:
 ldr x12, [x10, #80]
 cbz x12, Lselector_ok
 mov x3, #0xc090
 movk x3, #0xffff, lsl #16
 movk x3, #0xf, lsl #32
 ldrb w12, [x3]
 ldr x13, [sp, #264]
 cmp x12, x13
 b.eq Lselector_ok
 orr x14, x14, #8
Lselector_ok:
 str x14, [x11, #64]
 stp x19, x20, [x11, #80]
 stp x21, x22, [x11, #96]
 stp x23, x24, [x11, #112]
 stp x25, x26, [x11, #128]
 stp x27, x28, [x11, #144]
 stp x29, x30, [x11, #160]
 stp q8, q9, [x11, #176]
 stp q10, q11, [x11, #208]
 stp q12, q13, [x11, #240]
 stp q14, q15, [x11, #272]
 ldr x12, [sp, #272]
 str x12, [x11, #168]
 mov x14, #0
 mov x12, #4865
 cmp x19, x12
 cset x13, ne
 orr x14, x14, x13, lsl #0
 mov x12, #5121
 cmp x20, x12
 cset x13, ne
 orr x14, x14, x13, lsl #1
 mov x12, #5377
 cmp x21, x12
 cset x13, ne
 orr x14, x14, x13, lsl #2
 mov x12, #5633
 cmp x22, x12
 cset x13, ne
 orr x14, x14, x13, lsl #3
 mov x12, #5889
 cmp x23, x12
 cset x13, ne
 orr x14, x14, x13, lsl #4
 mov x12, #6145
 cmp x24, x12
 cset x13, ne
 orr x14, x14, x13, lsl #5
 mov x12, #6401
 cmp x25, x12
 cset x13, ne
 orr x14, x14, x13, lsl #6
 mov x12, #6657
 cmp x26, x12
 cset x13, ne
 orr x14, x14, x13, lsl #7
 mov x12, #6913
 cmp x27, x12
 cset x13, ne
 orr x14, x14, x13, lsl #8
 mov x12, #7169
 cmp x28, x12
 cset x13, ne
 orr x14, x14, x13, lsl #9
 add x12, sp, #96
 cmp x29, x12
 cset x13, ne
 orr x14, x14, x13, lsl #10
 str x14, [x11, #48]
 mov x14, #0
 ldr x12, [x11, #176]
 mov x13, #2056
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #0
 ldr x12, [x10, #128]
 cbz x12, Lhigh8
 ldr x12, [x11, #184]
 add x13, x13, #128
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #8
Lhigh8:
 ldr x12, [x11, #192]
 mov x13, #2313
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #1
 ldr x12, [x10, #128]
 cbz x12, Lhigh9
 ldr x12, [x11, #200]
 add x13, x13, #128
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #9
Lhigh9:
 ldr x12, [x11, #208]
 mov x13, #2570
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #2
 ldr x12, [x10, #128]
 cbz x12, Lhigh10
 ldr x12, [x11, #216]
 add x13, x13, #128
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #10
Lhigh10:
 ldr x12, [x11, #224]
 mov x13, #2827
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #3
 ldr x12, [x10, #128]
 cbz x12, Lhigh11
 ldr x12, [x11, #232]
 add x13, x13, #128
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #11
Lhigh11:
 ldr x12, [x11, #240]
 mov x13, #3084
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #4
 ldr x12, [x10, #128]
 cbz x12, Lhigh12
 ldr x12, [x11, #248]
 add x13, x13, #128
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #12
Lhigh12:
 ldr x12, [x11, #256]
 mov x13, #3341
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #5
 ldr x12, [x10, #128]
 cbz x12, Lhigh13
 ldr x12, [x11, #264]
 add x13, x13, #128
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #13
Lhigh13:
 ldr x12, [x11, #272]
 mov x13, #3598
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #6
 ldr x12, [x10, #128]
 cbz x12, Lhigh14
 ldr x12, [x11, #280]
 add x13, x13, #128
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #14
Lhigh14:
 ldr x12, [x11, #288]
 mov x13, #3855
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #7
 ldr x12, [x10, #128]
 cbz x12, Lhigh15
 ldr x12, [x11, #296]
 add x13, x13, #128
 cmp x12, x13
 cset x12, ne
 orr x14, x14, x12, lsl #15
Lhigh15:
 str x14, [x11, #56]
 ldr x12, [x11, #48]
 orr x14, x14, x12
 ldr x12, [x11, #64]
 orr x0, x14, x12
 ldp q8, q9, [sp, #112]
 ldp q10, q11, [sp, #144]
 ldp q12, q13, [sp, #176]
 ldp q14, q15, [sp, #208]
 ldp x19, x20, [sp, #16]
 ldp x21, x22, [sp, #32]
 ldp x23, x24, [sp, #48]
 ldp x25, x26, [sp, #64]
 ldp x27, x28, [sp, #80]
 ldp x29, x30, [sp, #96]
 add sp, sp, #384
 adrp x9, _matrix_guard_anchor@PAGE
 str xzr, [x9, _matrix_guard_anchor@PAGEOFF]
 ret
Lentry_bad:
 mov x10, x0
 ldr x11, [x10, #112]
 mrs x12, S3_3_C4_C2_2
 str x12, [x11]
 mrs x12, FPCR
 str x12, [x11, #16]
 mov x12, #64
 str x12, [x11, #64]
 b Lfatal
Lclock_bad:
 ldr x10, [sp, #240]
 ldr x11, [x10, #112]
 mov x12, #16
 str x12, [x11, #64]
 b Lfatal
Lmode_fatal:
 mov x12, #32
 str x12, [x11, #64]
Lfatal:
 // Fatal witness: raw syscall write then exit. No C++/SIMD/stack operations.
 ldr x0, [x10, #120]
 mov x1, x11
 mov x2, #304
 mov x16, #4
 svc #0x80
 mov x0, #110
 mov x16, #1
 svc #0x80
 brk #0
.data
.p2align 3
.globl _matrix_clock_pointer
_matrix_clock_pointer: .quad 0
_matrix_guard_anchor: .quad 0
