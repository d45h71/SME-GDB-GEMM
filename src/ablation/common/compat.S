// Column-only Darwin SME compatibility boundary.
.text
.p2align 2

.macro SAVE_MP5_ABLATION_FRAME
    sub sp, sp, #240
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
.endm

.macro RESTORE_MP5_ABLATION_FRAME
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
    add sp, sp, #240
.endm

.globl _mp5_ablation_safe_col_kernel_16x64
_mp5_ablation_safe_col_kernel_16x64:
    SAVE_MP5_ABLATION_FRAME
    ldr x16, [sp, #240]
    str x16, [sp]
    bl _col_kernel_16x64
    RESTORE_MP5_ABLATION_FRAME
    ret

.globl _mp5_ablation_safe_col_kernel_64x16
_mp5_ablation_safe_col_kernel_64x16:
    SAVE_MP5_ABLATION_FRAME
    ldr x16, [sp, #240]
    str x16, [sp]
    bl _col_kernel_64x16
    RESTORE_MP5_ABLATION_FRAME
    ret

.globl _mp5_ablation_safe_col_packb
_mp5_ablation_safe_col_packb:
    SAVE_MP5_ABLATION_FRAME
    bl _col_packb
    RESTORE_MP5_ABLATION_FRAME
    ret

#if MP5_ABLATION_OFFLINE_PACK
.globl _mp5_ablation_safe_packa_64
_mp5_ablation_safe_packa_64:
    SAVE_MP5_ABLATION_FRAME
    bl _mp5_ablation_packa_64
    RESTORE_MP5_ABLATION_FRAME
    ret

.globl _mp5_ablation_safe_packa_16
_mp5_ablation_safe_packa_16:
    SAVE_MP5_ABLATION_FRAME
    bl _mp5_ablation_packa_16
    RESTORE_MP5_ABLATION_FRAME
    ret
#endif
