    .text
    .global _row_sme_streaming_vector_bytes
    .align  4

_row_sme_streaming_vector_bytes:
    rdsvl   x0, #1
    ret

.macro F06A_DIRECTB_M16N64_K1
    ld1w    {z0.s-z3.s}, pn9/z, [x19]       //b_ptr
    ld1w    {z4.s}, p4/z, [x24]       //packa

    fmopa   za0.s, p4/m, p0/m, z4.s, z0.s
    fmopa   za1.s, p4/m, p1/m, z4.s, z1.s
    fmopa   za2.s, p4/m, p2/m, z4.s, z2.s
    fmopa   za3.s, p4/m, p3/m, z4.s, z3.s


    add     x19, x19, x8, lsl #2    // b_ptr+= ldb
    addvl   x24, x24, #1            // packa += SVLb

.endm


.macro F06A_DIRECTB_M16N64_K16
    // K = 8
    ld1w    {z0.s-z3.s}, pn9/z, [x19]       //1 b_ptr
    ld1w    {z4.s-z7.s}, pn10/z, [x24]       //4 packa

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z8.s-z11.s}, pn9/z, [x19]      //2 b_ptr
    fmopa   za0.s, p4/m, p0/m, z4.s, z0.s
    fmopa   za1.s, p4/m, p1/m, z4.s, z1.s
    fmopa   za2.s, p4/m, p2/m, z4.s, z2.s
    fmopa   za3.s, p4/m, p3/m, z4.s, z3.s

    
    add     x19, x19, x8, lsl #2            
    ld1w    {z12.s-z15.s}, pn9/z, [x19]      //3 b_ptr
    fmopa   za0.s, p4/m, p0/m, z5.s, z8.s
    fmopa   za1.s, p4/m, p1/m, z5.s, z9.s
    fmopa   za2.s, p4/m, p2/m, z5.s, z10.s
    fmopa   za3.s, p4/m, p3/m, z5.s, z11.s

    
   
    add     x19, x19, x8, lsl #2          
    ld1w    {z16.s-z19.s}, pn9/z, [x19]      //4 b_ptr
    fmopa   za0.s, p4/m, p0/m, z6.s, z12.s
    fmopa   za1.s, p4/m, p1/m, z6.s, z13.s
    fmopa   za2.s, p4/m, p2/m, z6.s, z14.s
    fmopa   za3.s, p4/m, p3/m, z6.s, z15.s

   
    add     x19, x19, x8, lsl #2      
    ld1w    {z24.s-z27.s}, pn10/z, [x24, #4, MUL VL]       //8 packa      
    ld1w    {z20.s-z23.s}, pn9/z, [x19]      //5 b_ptr
    fmopa   za0.s, p4/m, p0/m, z7.s, z16.s
    fmopa   za1.s, p4/m, p1/m, z7.s, z17.s
    fmopa   za2.s, p4/m, p2/m, z7.s, z18.s
    fmopa   za3.s, p4/m, p3/m, z7.s, z19.s

    
    add     x19, x19, x8, lsl #2            
    ld1w    {z28.s-z31.s}, pn9/z, [x19]      //6 b_ptr
    fmopa   za0.s, p4/m, p0/m, z24.s, z20.s
    fmopa   za1.s, p4/m, p1/m, z24.s, z21.s
    fmopa   za2.s, p4/m, p2/m, z24.s, z22.s
    fmopa   za3.s, p4/m, p3/m, z24.s, z23.s

    
    add     x19, x19, x8, lsl #2            
    ld1w    {z0.s-z3.s}, pn9/z, [x19]      //7 b_ptr
    fmopa   za0.s, p4/m, p0/m, z25.s, z28.s
    fmopa   za1.s, p4/m, p1/m, z25.s, z29.s
    fmopa   za2.s, p4/m, p2/m, z25.s, z30.s
    fmopa   za3.s, p4/m, p3/m, z25.s, z31.s

   
    add     x19, x19, x8, lsl #2            
    ld1w    {z4.s-z7.s}, pn9/z, [x19]      //8 b_ptr
    fmopa   za0.s, p4/m, p0/m, z26.s, z0.s
    fmopa   za1.s, p4/m, p1/m, z26.s, z1.s
    fmopa   za2.s, p4/m, p2/m, z26.s, z2.s
    fmopa   za3.s, p4/m, p3/m, z26.s, z3.s

    
    add     x19, x19, x8, lsl #2  
    ld1w    {z12.s-z15.s}, pn10/z, [x24, #8, MUL VL]       //12 packa          
    ld1w    {z8.s-z11.s}, pn9/z, [x19]      //9 b_ptr
    fmopa   za0.s, p4/m, p0/m, z27.s, z4.s
    fmopa   za1.s, p4/m, p1/m, z27.s, z5.s
    fmopa   za2.s, p4/m, p2/m, z27.s, z6.s
    fmopa   za3.s, p4/m, p3/m, z27.s, z7.s
 
//----------------------------------------------------------------------

    add     x19, x19, x8, lsl #2  
    ld1w    {z16.s-z19.s}, pn9/z, [x19]      //10 b_ptr
    fmopa   za0.s, p4/m, p0/m, z12.s, z8.s
    fmopa   za1.s, p4/m, p1/m, z12.s, z9.s
    fmopa   za2.s, p4/m, p2/m, z12.s, z10.s
    fmopa   za3.s, p4/m, p3/m, z12.s, z11.s

         

    add     x19, x19, x8, lsl #2            
    ld1w    {z20.s-z23.s}, pn9/z, [x19]      //11 b_ptr
    fmopa   za0.s, p4/m, p0/m, z13.s, z16.s
    fmopa   za1.s, p4/m, p1/m, z13.s, z17.s
    fmopa   za2.s, p4/m, p2/m, z13.s, z18.s
    fmopa   za3.s, p4/m, p3/m, z13.s, z19.s

   
    add     x19, x19, x8, lsl #2            
    ld1w    {z24.s-z27.s}, pn9/z, [x19]      //12 b_ptr
    fmopa   za0.s, p4/m, p0/m, z14.s, z20.s
    fmopa   za1.s, p4/m, p1/m, z14.s, z21.s
    fmopa   za2.s, p4/m, p2/m, z14.s, z22.s
    fmopa   za3.s, p4/m, p3/m, z14.s, z23.s

   
    add     x19, x19, x8, lsl #2        
    ld1w    {z0.s-z3.s}, pn10/z, [x24, #12, MUL VL]       //16 packa 
    ld1w    {z28.s-z31.s}, pn9/z, [x19]      //13 b_ptr
    fmopa   za0.s, p4/m, p0/m, z15.s, z24.s
    fmopa   za1.s, p4/m, p1/m, z15.s, z25.s
    fmopa   za2.s, p4/m, p2/m, z15.s, z26.s
    fmopa   za3.s, p4/m, p3/m, z15.s, z27.s

   
    add     x19, x19, x8, lsl #2
    ld1w    {z4.s-z7.s}, pn9/z, [x19]      //14 b_ptr
    fmopa   za0.s, p4/m, p0/m, z0.s, z28.s
    fmopa   za1.s, p4/m, p1/m, z0.s, z29.s
    fmopa   za2.s, p4/m, p2/m, z0.s, z30.s
    fmopa   za3.s, p4/m, p3/m, z0.s, z31.s

    
    add     x19, x19, x8, lsl #2
    ld1w    {z8.s-z11.s}, pn9/z, [x19]      //15 b_ptr
    fmopa   za0.s, p4/m, p0/m, z1.s, z4.s
    fmopa   za1.s, p4/m, p1/m, z1.s, z5.s  
    fmopa   za2.s, p4/m, p2/m, z1.s, z6.s
    fmopa   za3.s, p4/m, p3/m, z1.s, z7.s

   
    add     x19, x19, x8, lsl #2
    ld1w    {z12.s-z15.s}, pn9/z, [x19]      //16 b_ptr
    fmopa   za0.s, p4/m, p0/m, z2.s, z8.s  
    fmopa   za1.s, p4/m, p1/m, z2.s, z9.s
    fmopa   za2.s, p4/m, p2/m, z2.s, z10.s
    fmopa   za3.s, p4/m, p3/m, z2.s, z11.s

    
    fmopa   za0.s, p4/m, p0/m, z3.s, z12.s
    fmopa   za1.s, p4/m, p1/m, z3.s, z13.s
    fmopa   za2.s, p4/m, p2/m, z3.s, z14.s
    fmopa   za3.s, p4/m, p3/m, z3.s, z15.s

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    addvl   x24, x24, #16         //packa += 16 * SVLb
.endm

    .global _row_kernel_nomaterialize_16x64
    .align  4

_row_kernel_nomaterialize_16x64:

    ldr     w8,  [sp]        // ldc = ldb = n
    ldr     w9,  [sp, #4]    // pc

    stp     x19, x20, [sp, #-64]!
    stp     x21, x22, [sp, #16]
    stp     x23, x24, [sp, #32]
    stp     x25, x26, [sp, #48]


    smstart


    cntw    x12                     //   SVLs -> 16

    mov    x20, x6                  // c_base

    lsl     x25, x8, #1   // ldc * 2
    lsl     x17, x8, #2   // ldc * 4
    add     x15, x25, x8  // ldc * 3

    mov x14, #0
    whilelt p4.s, x14, x0           // tiles predicate (M dimension)


    add    x21, x3, x2, lsl #2     // Exit condition for N loop
    mov    x16, x3                  // b_base
    whilelt pn9.b, x16, x21, vlx4   // tiles predicate (N dimension)

//--------------------NR : NC LOOP -> DIRECTB + FMOPA ------------------------------------

F06A16_LOOP_NR:

    pext   {p0.b, p1.b}, pn9[0]      // tiles predicate (N dimension)
    pext   {p2.b, p3.b}, pn9[1]      // tiles predicate (N dimension)

    zero {za}

    cmp     x9, x7
    b.lt    F06A16_DIRECT_B_KERNEL

//----------------LOAD  C---------------------
    mov     w13, #0
    mov     x11, x20      // XC_ptr

F06A16_LOAD_C_START:
    psel    pn10, pn9, p4.s[w13, 0]
    psel    pn11, pn9, p4.s[w13, 1]
    psel    pn12, pn9, p4.s[w13, 2]
    psel    pn13, pn9, p4.s[w13, 3]
    ld1w    {z0.s, z4.s, z8.s, z12.s}, pn10/z, [x11]               // c_ptr0
    ld1w    {z1.s, z5.s, z9.s, z13.s}, pn11/z, [x11, x8, lsl #2]   // c_ptr0 + ldc
    ld1w    {z2.s, z6.s, z10.s, z14.s}, pn12/z, [x11, x25, lsl #2]  // c_ptr0 + ldc*2
    ld1w    {z3.s, z7.s, z11.s, z15.s}, pn13/z, [x11, x15, lsl #2]  // c_ptr0 + ldc*3

    mova    za0h.s[w13,0:3], {z0.s-z3.s}
    mova    za1h.s[w13,0:3], {z4.s-z7.s}
    mova    za2h.s[w13,0:3], {z8.s-z11.s}
    mova    za3h.s[w13,0:3], {z12.s-z15.s}

    add     x11, x11, x17, lsl #2         //c_ptr0 += 4*ldc FP32 bytes
    add     w13, w13, #4

    cmp     w13, w12
    b.mi    F06A16_LOAD_C_START


//-------------------------------------------------------------------    
F06A16_DIRECT_B_KERNEL:
    mov x19, x16                     // b_ptr = b_base
    
    mov x24, x4                     // packa

    ptrue pn10.b

//---------------------KC LOOP DIRECTB------------------------------------------------
    lsr x23, x1, #4                 // KC/16
    and x13, x1, #0xF               // KC % 16
    
    cbz     x23, 1f
F06A16_DIRECTB_M16N64_LOOP_K16:
    F06A_DIRECTB_M16N64_K16
    subs  x23, x23, #1
    b.ne F06A16_DIRECTB_M16N64_LOOP_K16

1:
    cbz    x13, 2f
F06A16_DIRECTB_M16N64_LOOP_K1:
    F06A_DIRECTB_M16N64_K1
    subs    x13, x13, #1
    b.ne    F06A16_DIRECTB_M16N64_LOOP_K1       
    
2:
    //nop


//----------------STORE C-----------------------
    mov     w13, #0
    mov     x11, x20      // XC_ptr

F06A16_STORE_C_START:
    psel    pn10, pn9, p4.s[w13, 0]
    psel    pn11, pn9, p4.s[w13, 1]
    psel    pn12, pn9, p4.s[w13, 2]
    psel    pn13, pn9, p4.s[w13, 3]
    mova    {z0.s-z3.s}, za0h.s[w13,0:3]
    mova    {z4.s-z7.s}, za1h.s[w13,0:3]
    mova    {z8.s-z11.s}, za2h.s[w13,0:3]
    mova    {z12.s-z15.s}, za3h.s[w13,0:3]

    st1w    {z0.s, z4.s, z8.s, z12.s},  pn10, [x11]               // c_ptr0 
    st1w    {z1.s, z5.s, z9.s, z13.s},  pn11, [x11, x8, lsl #2]   // c_ptr0 + ldc
    st1w    {z2.s, z6.s, z10.s, z14.s},  pn12, [x11, x25, lsl #2]  // c_ptr0 + ldc*2
    st1w    {z3.s, z7.s, z11.s, z15.s},  pn13, [x11, x15, lsl #2]   // c_ptr0 + ldc*3

    add     x11, x11, x17, lsl #2         //c_ptr0 += 4 * ldc FP32 bytes
    add     w13, w13, #4

    cmp     w13, w12
    b.mi    F06A16_STORE_C_START


//----Jump to Loop_NR-------------------------
    addvl   x16, x16, #4            // b_base += 4*SVLb (bytes)
    addvl   x20, x20, #4            // c_base += 4*SVLb
    whilelt pn9.b, x16, x21, vlx4   // tile predicate (N dimension)
    b.first F06A16_LOOP_NR

//-------------------------DIRECTB END--------------------------------------------------

    smstop



    ldp     x25, x26, [sp, #48]
    ldp     x23, x24, [sp, #32]
    ldp     x21, x22, [sp, #16]
    ldp     x19, x20, [sp], #64
    

    ret
.macro F06A_DIRECTB_M64N16_K1
    ld1w    {z0.s}, p4/z, [x19]       //b_ptr
    ld1w    {z1.s}, p0/z, [x24]       //packa
    ld1w    {z2.s}, p1/z, [x24, x28, lsl #2]       //packa + SVLs * KC * 4
    ld1w    {z3.s}, p2/z, [x24, x29, lsl #2]       //packa + SVLs * KC * 2 * 4
    ld1w    {z4.s}, p3/z, [x24, x30, lsl #2]       //packa + SVLs * KC * 3 * 4

    fmopa   za0.s, p0/m, p4/m, z1.s, z0.s
    fmopa   za1.s, p1/m, p4/m, z2.s, z0.s
    fmopa   za2.s, p2/m, p4/m, z3.s, z0.s
    fmopa   za3.s, p3/m, p4/m, z4.s, z0.s


    add     x19, x19, x8, lsl #2    // b_ptr+= ldb
    addvl   x24, x24, #1            // packa += SVLb

.endm


.macro F06A_DIRECTB_M64N16_K16
    // K = 8
    ld1w    {z20.s}, p4/z, [x19]       //1 b_ptr
    ld1w    {z0.s-z3.s}, pn10/z, [x24]       //4-1 packa
    fmopa   za0.s, p0/m, p4/m, z0.s, z20.s

    ld1w    {z4.s-z7.s}, pn10/z, [x24, x28, lsl #2]       //4-2 packa = x24 + SVLs * KC * 4
    fmopa   za1.s, p1/m, p4/m, z4.s, z20.s

    ld1w    {z8.s-z11.s}, pn10/z, [x24, x29, lsl #2]       //4-3 packa = x24 + SVLs * KC * 2 * 4
    fmopa   za2.s, p2/m, p4/m, z8.s, z20.s

    ld1w    {z12.s-z15.s}, pn10/z, [x24, x30, lsl #2]       //4-4 packa = x24 + SVLs * KC * 3 * 4
    fmopa   za3.s, p3/m, p4/m, z12.s, z20.s

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z21.s}, p4/z, [x19]            //2 b_ptr

    fmopa   za0.s, p0/m, p4/m, z1.s, z21.s
    fmopa   za1.s, p1/m, p4/m, z5.s, z21.s
    fmopa   za2.s, p2/m, p4/m, z9.s, z21.s
    fmopa   za3.s, p3/m, p4/m, z13.s, z21.s

    addvl   x24, x24, #4            // packa += 4 * SVLb
    ld1w    {z0.s, z4.s, z8.s, z12.s}, pn10/z, [x24]       //8-1 packa

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z22.s}, p4/z, [x19]            //3 b_ptr

    fmopa   za0.s, p0/m, p4/m, z2.s, z22.s
    fmopa   za1.s, p1/m, p4/m, z6.s, z22.s
    fmopa   za2.s, p2/m, p4/m, z10.s, z22.s
    fmopa   za3.s, p3/m, p4/m, z14.s, z22.s

    ld1w    {z1.s, z5.s, z9.s, z13.s}, pn10/z, [x24, x28, lsl #2]       //8-2 packa
    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z23.s}, p4/z, [x19]            //4 b_ptr

    fmopa   za0.s, p0/m, p4/m, z3.s, z23.s
    fmopa   za1.s, p1/m, p4/m, z7.s, z23.s
    fmopa   za2.s, p2/m, p4/m, z11.s, z23.s
    fmopa   za3.s, p3/m, p4/m, z15.s, z23.s

//----------- 4 ----------

    ld1w    {z2.s, z6.s, z10.s, z14.s}, pn10/z, [x24, x29, lsl #2]       //8-3 packa
    
    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z24.s}, p4/z, [x19]            //5 b_ptr

    ld1w    {z3.s, z7.s, z11.s, z15.s}, pn10/z, [x24, x30, lsl #2]       //8-4 packa

    fmopa   za0.s, p0/m, p4/m, z0.s, z24.s
    fmopa   za1.s, p1/m, p4/m, z1.s, z24.s
    fmopa   za2.s, p2/m, p4/m, z2.s, z24.s
    fmopa   za3.s, p3/m, p4/m, z3.s, z24.s

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z25.s}, p4/z, [x19]            //6 b_ptr

    fmopa   za0.s, p0/m, p4/m, z4.s, z25.s
    fmopa   za1.s, p1/m, p4/m, z5.s, z25.s
    fmopa   za2.s, p2/m, p4/m, z6.s, z25.s
    fmopa   za3.s, p3/m, p4/m, z7.s, z25.s

    addvl   x24, x24, #4            // packa += 4 * SVLb
    ld1w    {z0.s-z3.s}, pn10/z, [x24]  //12-1

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z26.s}, p4/z, [x19]            //7 b_ptr

    fmopa   za0.s, p0/m, p4/m, z8.s, z26.s
    fmopa   za1.s, p1/m, p4/m, z9.s, z26.s
    fmopa   za2.s, p2/m, p4/m, z10.s, z26.s
    fmopa   za3.s, p3/m, p4/m, z11.s, z26.s

    ld1w    {z4.s-z7.s}, pn10/z, [x24, x28, lsl #2]  //12-2

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z27.s}, p4/z, [x19]            //8 b_ptr
    fmopa   za0.s, p0/m, p4/m, z12.s, z27.s
    fmopa   za1.s, p1/m, p4/m, z13.s, z27.s
    fmopa   za2.s, p2/m, p4/m, z14.s, z27.s
    fmopa   za3.s, p3/m, p4/m, z15.s, z27.s

    ld1w    {z8.s-z11.s}, pn10/z, [x24, x29, lsl #2]  //12-3

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    
//----------- 8 ----------
    ld1w    {z28.s}, p4/z, [x19]            //9 b_ptr

    ld1w    {z12.s-z15.s}, pn10/z, [x24, x30, lsl #2]  //12-4
    fmopa   za0.s, p0/m, p4/m, z0.s, z28.s
    fmopa   za1.s, p1/m, p4/m, z4.s, z28.s
    fmopa   za2.s, p2/m, p4/m, z8.s, z28.s
    fmopa   za3.s, p3/m, p4/m, z12.s, z28.s

    
    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z29.s}, p4/z, [x19]            //10 b_ptr

    fmopa   za0.s, p0/m, p4/m, z1.s, z29.s
    fmopa   za1.s, p1/m, p4/m, z5.s, z29.s
    fmopa   za2.s, p2/m, p4/m, z9.s, z29.s
    fmopa   za3.s, p3/m, p4/m, z13.s, z29.s

    addvl   x24, x24, #4            // packa += 4 * SVLb
    ld1w    {z0.s, z4.s, z8.s, z12.s}, pn10/z, [x24]  //16-1 packa

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z30.s}, p4/z, [x19]            //11 b_ptr

    fmopa   za0.s, p0/m, p4/m, z2.s, z30.s
    fmopa   za1.s, p1/m, p4/m, z6.s, z30.s
    fmopa   za2.s, p2/m, p4/m, z10.s, z30.s
    fmopa   za3.s, p3/m, p4/m, z14.s, z30.s

    ld1w    {z1.s, z5.s, z9.s, z13.s}, pn10/z, [x24, x28, lsl #2]  //16-2 packa

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z16.s}, p4/z, [x19]            //12 b_ptr

    fmopa   za0.s, p0/m, p4/m, z3.s, z16.s
    fmopa   za1.s, p1/m, p4/m, z7.s, z16.s
    fmopa   za2.s, p2/m, p4/m, z11.s, z16.s
    fmopa   za3.s, p3/m, p4/m, z15.s, z16.s

    ld1w    {z2.s, z6.s, z10.s, z14.s}, pn10/z, [x24, x29, lsl #2]  //16-3 packa

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z17.s}, p4/z, [x19]            //13 b_ptr

    ld1w    {z3.s, z7.s, z11.s, z15.s}, pn10/z, [x24, x30, lsl #2]  //16-3 packa
//----------- 12 ----------
    fmopa   za0.s, p0/m, p4/m, z0.s, z17.s
    fmopa   za1.s, p1/m, p4/m, z1.s, z17.s
    fmopa   za2.s, p2/m, p4/m, z2.s, z17.s
    fmopa   za3.s, p3/m, p4/m, z3.s, z17.s

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z18.s}, p4/z, [x19]            //14 b_ptr

    fmopa   za0.s, p0/m, p4/m, z4.s, z18.s
    fmopa   za1.s, p1/m, p4/m, z5.s, z18.s
    fmopa   za2.s, p2/m, p4/m, z6.s, z18.s
    fmopa   za3.s, p3/m, p4/m, z7.s, z18.s

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z19.s}, p4/z, [x19]            //15 b_ptr

    fmopa   za0.s, p0/m, p4/m, z8.s, z19.s
    fmopa   za1.s, p1/m, p4/m, z9.s, z19.s
    fmopa   za2.s, p2/m, p4/m, z10.s, z19.s
    fmopa   za3.s, p3/m, p4/m, z11.s, z19.s

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    ld1w    {z20.s}, p4/z, [x19]            //16 b_ptr

    fmopa   za0.s, p0/m, p4/m, z12.s, z20.s
    fmopa   za1.s, p1/m, p4/m, z13.s, z20.s
    fmopa   za2.s, p2/m, p4/m, z14.s, z20.s
    fmopa   za3.s, p3/m, p4/m, z15.s, z20.s

    add     x19, x19, x8, lsl #2            //b_ptr+= ldb
    addvl   x24, x24, #4            //packa += 4 * SVLb
.endm

    .global _row_kernel_nomaterialize_64x16
    .align  4

_row_kernel_nomaterialize_64x16:

    ldr     w8,  [sp]        // ldc = ldb = n
    ldr     w9,  [sp, #4]    // pc

    stp     x19, x20, [sp, #-96]!
    stp     x21, x22, [sp, #16]
    stp     x23, x24, [sp, #32]
    stp     x25, x26, [sp, #48]
    stp     x27, x28, [sp, #64]
    stp     x29, x30, [sp, #80]


    smstart


    cntw    x12                     //   SVLs -> 16

    mov     x20, x6                  // c_base

    lsl     x25, x8, #1   // ldc * 2
    lsl     x17, x8, #2   // ldc * 4
    add     x15, x25, x8   // ldc * 3
    mul     x27, x12, x8   // SVLs * ldc

    mov     x14, #0
    whilelt pn9.s, x14, x0, vlx4          // tiles predicate (M dimension)

    pext   {p0.s, p1.s}, pn9[0]      // tiles predicate (M dimension)
    pext   {p2.s, p3.s}, pn9[1]      // tiles predicate (M dimension)

    ptrue pn10.b

    add    x21, x3, x2, lsl #2     // Exit condition for N loop
    mov    x16, x3                 // b_base
    whilelt p4.b, x16, x21   // tiles predicate (N dimension)
//--------------------NR : NC LOOP -> DIRECTB + FMOPA ------------------------------------

F06A64_LOOP_NR:

    zero {za}   // beta = 0 -> zero {zero}
    
    cmp     x9, x7
    b.lt    F06A64_DIRECT_B_KERNEL

//----------------LOAD  C---------------------
    mov     w13, #0
    mov     x11, x20      // XC_ptr
    add     x28, x20, x27, lsl  #2     // XC_ptr + SVLs * ldc * 4      
    add     x29, x20, x27, lsl  #3     // XC_ptr + SVLs * ldc * 2 * 4
    add     x30, x28, x27, lsl  #3     // XC_ptr + SVLs * ldc * 3 * 4

F06A64_LOAD_C_START:
    psel    p5, p4, p0.s[w13, 0]
    psel    p6, p4, p0.s[w13, 1]
    psel    p7, p4, p0.s[w13, 2]
    ld1w    {z0.s}, p5/z, [x11]               // c_ptr0
    ld1w    {z1.s}, p6/z, [x11, x8, lsl #2]   // c_ptr0 + ldc
    ld1w    {z2.s}, p7/z, [x11, x25, lsl #2]  // c_ptr0 + ldc*2
    psel    p5, p4, p0.s[w13, 3]
    ld1w    {z3.s}, p5/z, [x11, x15, lsl #2]  // c_ptr0 + ldc*3
    mova    za0h.s[w13,0:3], {z0.s-z3.s}
    add     x11, x11, x17, lsl #2         //c_ptr0 += 4*ldc FP32 bytes

    psel    p5, p4, p1.s[w13, 0]
    psel    p6, p4, p1.s[w13, 1]
    psel    p7, p4, p1.s[w13, 2]
    ld1w    {z4.s}, p5/z, [x28]               // c_ptr1
    ld1w    {z5.s}, p6/z, [x28, x8, lsl #2]   // c_ptr1 + ldc
    ld1w    {z6.s}, p7/z, [x28, x25, lsl #2]  // c_ptr1 + ldc*2
    psel    p5, p4, p1.s[w13, 3]
    ld1w    {z7.s}, p5/z, [x28, x15, lsl #2]  // c_ptr1 + ldc*3
    mova    za1h.s[w13,0:3], {z4.s-z7.s}
    add     x28, x28, x17, lsl #2         //c_ptr1 += 4*ldc FP32 bytes

    psel    p5, p4, p2.s[w13, 0]
    psel    p6, p4, p2.s[w13, 1]
    psel    p7, p4, p2.s[w13, 2]
    ld1w    {z8.s}, p5/z, [x29]               // c_ptr2
    ld1w    {z9.s}, p6/z, [x29, x8, lsl #2]   // c_ptr2 + ldc
    ld1w    {z10.s}, p7/z, [x29, x25, lsl #2]  // c_ptr2 + ldc*2
    psel    p5, p4, p2.s[w13, 3]
    ld1w    {z11.s}, p5/z, [x29, x15, lsl #2]  // c_ptr2 + ldc*3
    mova    za2h.s[w13,0:3], {z8.s-z11.s}
    add     x29, x29, x17, lsl #2         //c_ptr2 += 4*ldc FP32 bytes

    psel    p5, p4, p3.s[w13, 0]
    psel    p6, p4, p3.s[w13, 1]
    psel    p7, p4, p3.s[w13, 2]
    ld1w    {z12.s}, p5/z, [x30]               // c_ptr3
    ld1w    {z13.s}, p6/z, [x30, x8, lsl #2]   // c_ptr3 + ldc
    ld1w    {z14.s}, p7/z, [x30, x25, lsl #2]  // c_ptr3 + ldc*2
    psel    p5, p4, p3.s[w13, 3]
    ld1w    {z15.s}, p5/z, [x30, x15, lsl #2]  // c_ptr3 + ldc*3
    mova    za3h.s[w13,0:3], {z12.s-z15.s}
    add     x30, x30, x17, lsl #2         //c_ptr3 += 4*ldc FP32 bytes

    add     w13, w13, #4

    cmp     w13, w12
    b.mi    F06A64_LOAD_C_START


//-------------------------------------------------------------------    
F06A64_DIRECT_B_KERNEL:
    mov x19, x16                     // b_ptr = b_base
    
    mov x24, x4                     // packa

    mul x28, x12, x1        // SVLs * KC * 1
    lsl x29, x28, #1         // SVLs * KC * 2
    add x30, x28, x29       // SVLs * KC * 3

    

//---------------------KC LOOP PACKA------------------------------------------------
    lsr x23, x1, #4                 // KC/16
    and x13, x1, #0xF               // KC % 16
    
    cbz     x23, 1f
F06A64_DIRECTB_M64N16_LOOP_K16:
    F06A_DIRECTB_M64N16_K16
    subs  x23, x23, #1
    b.ne F06A64_DIRECTB_M64N16_LOOP_K16

1:
    cbz    x13, 2f
F06A64_DIRECTB_M64N16_LOOP_K1:
    F06A_DIRECTB_M64N16_K1
    subs    x13, x13, #1
    b.ne    F06A64_DIRECTB_M64N16_LOOP_K1       
    
2:
    //nop


//----------------STORE C-----------------------
    mov     w13, #0
    mov     x11, x20      // XC_ptr
    add     x28, x20, x27, lsl  #2     // XC_ptr + SVLs * ldc * 4      
    add     x29, x20, x27, lsl  #3     // XC_ptr + SVLs * ldc * 2 * 4
    add     x30, x28, x27, lsl  #3     // XC_ptr + SVLs * ldc * 3 * 4

F06A64_STORE_C_START:
    mova    {z0.s-z3.s}, za0h.s[w13,0:3]
    mova    {z4.s-z7.s}, za1h.s[w13,0:3]
    mova    {z8.s-z11.s}, za2h.s[w13,0:3]
    mova    {z12.s-z15.s}, za3h.s[w13,0:3]
    
    psel    p5, p4, p0.s[w13, 0]
    psel    p6, p4, p0.s[w13, 1]
    psel    p7, p4, p0.s[w13, 2]
    st1w    {z0.s},  p5, [x11]               // c_ptr0 
    st1w    {z1.s},  p6, [x11, x8, lsl #2]   // c_ptr0 + ldc
    st1w    {z2.s},  p7, [x11, x25, lsl #2]   // c_ptr0 + ldc * 2
    psel    p5, p4, p0.s[w13, 3]
    st1w    {z3.s},  p5, [x11, x15, lsl #2]   // c_ptr0 + ldc * 3

    add     x11, x11, x17, lsl #2         //c_ptr0 += 4*ldc FP32 bytes

    psel    p5, p4, p1.s[w13, 0]
    psel    p6, p4, p1.s[w13, 1]
    psel    p7, p4, p1.s[w13, 2]
    st1w    {z4.s},  p5, [x28]               // c_ptr1
    st1w    {z5.s},  p6, [x28, x8, lsl #2]   // c_ptr1 + ldc
    st1w    {z6.s},  p7, [x28, x25, lsl #2]   // c_ptr1 + ldc * 2
    psel    p5, p4, p1.s[w13, 3]
    st1w    {z7.s},  p5, [x28, x15, lsl #2]   // c_ptr1 + ldc * 3

    add     x28, x28, x17, lsl #2         //c_ptr1 += 4*ldc FP32 bytes

    psel    p5, p4, p2.s[w13, 0]
    psel    p6, p4, p2.s[w13, 1]
    psel    p7, p4, p2.s[w13, 2]
    st1w    {z8.s},  p5, [x29]               // c_ptr2 
    st1w    {z9.s},  p6, [x29, x8, lsl #2]   // c_ptr2 + ldc
    st1w    {z10.s},  p7, [x29, x25, lsl #2]   // c_ptr2 + ldc * 2
    psel    p5, p4, p2.s[w13, 3]
    st1w    {z11.s},  p5, [x29, x15, lsl #2]   // c_ptr2 + ldc * 3

    add     x29, x29, x17, lsl #2         //c_ptr2 += 4*ldc FP32 bytes

    psel    p5, p4, p3.s[w13, 0]
    psel    p6, p4, p3.s[w13, 1]
    psel    p7, p4, p3.s[w13, 2]
    st1w    {z12.s},  p5, [x30]               // c_ptr3 
    st1w    {z13.s},  p6, [x30, x8, lsl #2]   // c_ptr3 + ldc
    st1w    {z14.s},  p7, [x30, x25, lsl #2]   // c_ptr3 + ldc * 2
    psel    p5, p4, p3.s[w13, 3]
    st1w    {z15.s},  p5, [x30, x15, lsl #2]   // c_ptr3 + ldc * 3

    add     x30, x30, x17, lsl #2         //c_ptr3 += 4*ldc FP32 bytes

    add     w13, w13, #4
    cmp     w13, w12
    b.mi    F06A64_STORE_C_START


//----Jump to Loop_NR-------------------------
    addvl   x16, x16, #1            // b_base += SVLb (bytes)
    addvl   x20, x20, #1            // c_base += SVLb
    whilelt p4.b, x16, x21          // tile predicate (N dimension)
    b.first F06A64_LOOP_NR

//-------------------------PACKA END--------------------------------------------------

    smstop

    ldp     x29, x30, [sp, #80]
    ldp     x27, x28, [sp, #64]
    ldp     x25, x26, [sp, #48]
    ldp     x23, x24, [sp, #32]
    ldp     x21, x22, [sp, #16]
    ldp     x19, x20, [sp], #96
    

    ret
