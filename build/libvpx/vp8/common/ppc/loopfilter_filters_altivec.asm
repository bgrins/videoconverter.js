;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl mbloop_filter_horizontal_edge_y_ppc
    .globl loop_filter_horizontal_edge_y_ppc
    .globl mbloop_filter_vertical_edge_y_ppc
    .globl loop_filter_vertical_edge_y_ppc

    .globl mbloop_filter_horizontal_edge_uv_ppc
    .globl loop_filter_horizontal_edge_uv_ppc
    .globl mbloop_filter_vertical_edge_uv_ppc
    .globl loop_filter_vertical_edge_uv_ppc

    .globl loop_filter_simple_horizontal_edge_ppc
    .globl loop_filter_simple_vertical_edge_ppc

    .text
;# We often need to perform transposes (and other transpose-like operations)
;#   on matrices of data.  This is simplified by the fact that we usually
;#   operate on hunks of data whose dimensions are powers of 2, or at least
;#   divisible by highish powers of 2.
;#
;#   These operations can be very confusing.  They become more straightforward
;#   when we think of them as permutations of address bits: Concatenate a
;#   group of vector registers and think of it as occupying a block of
;#   memory beginning at address zero.  The low four bits 0...3 of the
;#   address then correspond to position within a register, the higher-order
;#   address bits select the register.
;#
;#   Although register selection, at the code level, is arbitrary, things
;#   are simpler if we use contiguous ranges of register numbers, simpler
;#   still if the low-order bits of the register number correspond to
;#   conceptual address bits.  We do this whenever reasonable.
;#
;#   A 16x16 transpose can then be thought of as an operation on
;#   a 256-element block of memory.  It takes 8 bits 0...7 to address this
;#   memory and the effect of a transpose is to interchange address bit
;#   0 with 4, 1 with 5, 2 with 6, and 3 with 7.  Bits 0...3 index the
;#   column, which is interchanged with the row addressed by bits 4..7.
;#
;#   The altivec merge instructions provide a rapid means of effecting
;#   many of these transforms.  They operate at three widths (8,16,32).
;#   Writing V(x) for vector register #x, paired merges permute address
;#   indices as follows.
;#
;#   0->1  1->2  2->3  3->(4+d)  (4+s)->0:
;#
;#      vmrghb  V( x),          V( y), V( y + (1<<s))
;#      vmrglb  V( x + (1<<d)), V( y), V( y + (1<<s))
;#
;#
;#   =0=   1->2  2->3  3->(4+d)  (4+s)->1:
;#
;#      vmrghh  V( x),          V( y), V( y + (1<<s))
;#      vmrglh  V( x + (1<<d)), V( y), V( y + (1<<s))
;#
;#
;#   =0=   =1=   2->3  3->(4+d)  (4+s)->2:
;#
;#      vmrghw  V( x),          V( y), V( y + (1<<s))
;#      vmrglw  V( x + (1<<d)), V( y), V( y + (1<<s))
;#
;#
;#   Unfortunately, there is no doubleword merge instruction.
;#   The following sequence uses "vperm" is a substitute.
;#   Assuming that the selection masks b_hihi and b_lolo (defined in LFppc.c)
;#   are in registers Vhihi and Vlolo, we can also effect the permutation
;#
;#   =0=   =1=   =2=   3->(4+d)  (4+s)->3   by the sequence:
;#
;#      vperm   V( x),          V( y), V( y + (1<<s)), Vhihi
;#      vperm   V( x + (1<<d)), V( y), V( y + (1<<s)), Vlolo
;#
;#
;#   Except for bits s and d, the other relationships between register
;#   number (= high-order part of address) bits are at the disposal of
;#   the programmer.
;#

;# To avoid excess transposes, we filter all 3 vertical luma subblock
;#   edges together.  This requires a single 16x16 transpose, which, in
;#   the above language, amounts to the following permutation of address
;#   indices:  0<->4   1<->5  2<->6  3<->7, which we accomplish by
;#   4 iterations of the cyclic transform 0->1->2->3->4->5->6->7->0.
;#
;#   Except for the fact that the destination registers get written
;#   before we are done referencing the old contents, the cyclic transform
;#   is effected by
;#
;#      x = 0;  do {
;#          vmrghb V(2x),   V(x), V(x+8);
;#          vmrghb V(2x+1), V(x), V(x+8);
;#      } while( ++x < 8);
;#
;#   For clarity, and because we can afford it, we do this transpose
;#   using all 32 registers, alternating the banks 0..15  and  16 .. 31,
;#   leaving the final result in 16 .. 31, as the lower registers are
;#   used in the filtering itself.
;#
.macro Tpair A, B, X, Y
    vmrghb  \A, \X, \Y
    vmrglb  \B, \X, \Y
.endm

;# Each step takes 8*2 = 16 instructions

.macro t16_even
    Tpair v16,v17,  v0,v8
    Tpair v18,v19,  v1,v9
    Tpair v20,v21,  v2,v10
    Tpair v22,v23,  v3,v11
    Tpair v24,v25,  v4,v12
    Tpair v26,v27,  v5,v13
    Tpair v28,v29,  v6,v14
    Tpair v30,v31,  v7,v15
.endm

.macro t16_odd
    Tpair v0,v1, v16,v24
    Tpair v2,v3, v17,v25
    Tpair v4,v5, v18,v26
    Tpair v6,v7, v19,v27
    Tpair v8,v9, v20,v28
    Tpair v10,v11, v21,v29
    Tpair v12,v13, v22,v30
    Tpair v14,v15, v23,v31
.endm

;# Whole transpose takes 4*16 = 64 instructions

.macro t16_full
    t16_odd
    t16_even
    t16_odd
    t16_even
.endm

;# Vertical edge filtering requires transposes.  For the simple filter,
;#   we need to convert 16 rows of 4 pels each into 4 registers of 16 pels
;#   each.  Writing 0 ... 63 for the pixel indices, the desired result is:
;#
;#  v0 =  0  1 ... 14 15
;#  v1 = 16 17 ... 30 31
;#  v2 = 32 33 ... 47 48
;#  v3 = 49 50 ... 62 63
;#
;#  In frame-buffer memory, the layout is:
;#
;#     0  16  32  48
;#     1  17  33  49
;#     ...
;#    15  31  47  63.
;#
;#  We begin by reading the data 32 bits at a time (using scalar operations)
;#  into a temporary array, reading the rows of the array into vector registers,
;#  with the following layout:
;#
;#  v0 =  0 16 32 48  4 20 36 52  8 24 40 56  12 28 44 60
;#  v1 =  1 17 33 49  5 21 ...                      45 61
;#  v2 =  2 18 ...                                  46 62
;#  v3 =  3 19 ...                                  47 63
;#
;#  From the "address-bit" perspective discussed above, we simply need to
;#  interchange bits 0 <-> 4 and 1 <-> 5, leaving bits 2 and 3 alone.
;#  In other words, we transpose each of the four 4x4 submatrices.
;#
;#  This transformation is its own inverse, and we need to perform it
;#  again before writing the pixels back into the frame buffer.
;#
;#  It acts in place on registers v0...v3, uses v4...v7 as temporaries,
;#  and assumes that v14/v15 contain the b_hihi/b_lolo selectors
;#  defined above.  We think of both groups of 4 registers as having
;#  "addresses" {0,1,2,3} * 16.
;#
.macro Transpose4times4x4 Vlo, Vhi

    ;# d=s=0        0->1  1->2  2->3  3->4  4->0  =5=

    vmrghb  v4, v0, v1
    vmrglb  v5, v0, v1
    vmrghb  v6, v2, v3
    vmrglb  v7, v2, v3

    ;# d=0 s=1      =0=   1->2  2->3  3->4  4->5  5->1

    vmrghh  v0, v4, v6
    vmrglh  v1, v4, v6
    vmrghh  v2, v5, v7
    vmrglh  v3, v5, v7

    ;# d=s=0        =0=   =1=   2->3  3->4  4->2  =5=

    vmrghw  v4, v0, v1
    vmrglw  v5, v0, v1
    vmrghw  v6, v2, v3
    vmrglw  v7, v2, v3

    ;# d=0  s=1     =0=   =1=   =2=   3->4  4->5  5->3

    vperm   v0, v4, v6, \Vlo
    vperm   v1, v4, v6, \Vhi
    vperm   v2, v5, v7, \Vlo
    vperm   v3, v5, v7, \Vhi
.endm
;# end Transpose4times4x4


;# Normal mb vertical edge filter transpose.
;#
;#   We read 8 columns of data, initially in the following pattern:
;#
;#  (0,0)  (1,0) ... (7,0)  (0,1)  (1,1) ... (7,1)
;#  (0,2)  (1,2) ... (7,2)  (0,3)  (1,3) ... (7,3)
;#  ...
;#  (0,14) (1,14) .. (7,14) (0,15) (1,15) .. (7,15)
;#
;#   and wish to convert to:
;#
;#  (0,0) ... (0,15)
;#  (1,0) ... (1,15)
;#  ...
;#  (7,0) ... (7,15).
;#
;#  In "address bit" language, we wish to map
;#
;#  0->4  1->5  2->6  3->0  4->1  5->2  6->3, i.e., I -> (I+4) mod 7.
;#
;#  This can be accomplished by 4 iterations of the cyclic transform
;#
;#  I -> (I+1) mod 7;
;#
;#  each iteration can be realized by (d=0, s=2):
;#
;#  x = 0;  do  Tpair( V(2x),V(2x+1),  V(x),V(x+4))  while( ++x < 4);
;#
;#  The input/output is in registers v0...v7.  We use v10...v17 as mirrors;
;#  preserving v8 = sign converter.
;#
;#  Inverse transpose is similar, except here I -> (I+3) mod 7 and the
;#  result lands in the "mirror" registers v10...v17
;#
.macro t8x16_odd
    Tpair v10, v11,  v0, v4
    Tpair v12, v13,  v1, v5
    Tpair v14, v15,  v2, v6
    Tpair v16, v17,  v3, v7
.endm

.macro t8x16_even
    Tpair v0, v1,  v10, v14
    Tpair v2, v3,  v11, v15
    Tpair v4, v5,  v12, v16
    Tpair v6, v7,  v13, v17
.endm

.macro transpose8x16_fwd
    t8x16_odd
    t8x16_even
    t8x16_odd
    t8x16_even
.endm

.macro transpose8x16_inv
    t8x16_odd
    t8x16_even
    t8x16_odd
.endm

.macro Transpose16x16
    vmrghb  v0, v16, v24
    vmrglb  v1, v16, v24
    vmrghb  v2, v17, v25
    vmrglb  v3, v17, v25
    vmrghb  v4, v18, v26
    vmrglb  v5, v18, v26
    vmrghb  v6, v19, v27
    vmrglb  v7, v19, v27
    vmrghb  v8, v20, v28
    vmrglb  v9, v20, v28
    vmrghb  v10, v21, v29
    vmrglb  v11, v21, v29
    vmrghb  v12, v22, v30
    vmrglb  v13, v22, v30
    vmrghb  v14, v23, v31
    vmrglb  v15, v23, v31
    vmrghb  v16, v0, v8
    vmrglb  v17, v0, v8
    vmrghb  v18, v1, v9
    vmrglb  v19, v1, v9
    vmrghb  v20, v2, v10
    vmrglb  v21, v2, v10
    vmrghb  v22, v3, v11
    vmrglb  v23, v3, v11
    vmrghb  v24, v4, v12
    vmrglb  v25, v4, v12
    vmrghb  v26, v5, v13
    vmrglb  v27, v5, v13
    vmrghb  v28, v6, v14
    vmrglb  v29, v6, v14
    vmrghb  v30, v7, v15
    vmrglb  v31, v7, v15
    vmrghb  v0, v16, v24
    vmrglb  v1, v16, v24
    vmrghb  v2, v17, v25
    vmrglb  v3, v17, v25
    vmrghb  v4, v18, v26
    vmrglb  v5, v18, v26
    vmrghb  v6, v19, v27
    vmrglb  v7, v19, v27
    vmrghb  v8, v20, v28
    vmrglb  v9, v20, v28
    vmrghb  v10, v21, v29
    vmrglb  v11, v21, v29
    vmrghb  v12, v22, v30
    vmrglb  v13, v22, v30
    vmrghb  v14, v23, v31
    vmrglb  v15, v23, v31
    vmrghb  v16, v0, v8
    vmrglb  v17, v0, v8
    vmrghb  v18, v1, v9
    vmrglb  v19, v1, v9
    vmrghb  v20, v2, v10
    vmrglb  v21, v2, v10
    vmrghb  v22, v3, v11
    vmrglb  v23, v3, v11
    vmrghb  v24, v4, v12
    vmrglb  v25, v4, v12
    vmrghb  v26, v5, v13
    vmrglb  v27, v5, v13
    vmrghb  v28, v6, v14
    vmrglb  v29, v6, v14
    vmrghb  v30, v7, v15
    vmrglb  v31, v7, v15
.endm

;# load_g loads a global vector (whose address is in the local variable Gptr)
;#   into vector register Vreg.  Trashes r0
.macro load_g Vreg, Gptr
    lwz     r0, \Gptr
    lvx     \Vreg, 0, r0
.endm

;# exploit the saturation here.  if the answer is negative
;# it will be clamped to 0.  orring 0 with a positive
;# number will be the positive number (abs)
;# RES = abs( A-B), trashes TMP
.macro Abs RES, TMP, A, B
    vsububs \RES, \A, \B
    vsububs \TMP, \B, \A
    vor     \RES, \RES, \TMP
.endm

;# RES = Max( RES, abs( A-B)), trashes TMP
.macro max_abs RES, TMP, A, B
    vsububs \TMP, \A, \B
    vmaxub  \RES, \RES, \TMP
    vsububs \TMP, \B, \A
    vmaxub  \RES, \RES, \TMP
.endm

.macro Masks
    ;# build masks
    ;# input is all 8 bit unsigned (0-255).  need to
    ;# do abs(vala-valb) > limit.  but no need to compare each
    ;# value to the limit.  find the max of the absolute differences
    ;# and compare that to the limit.
    ;# First hev
    Abs     v14, v13, v2, v3    ;# |P1 - P0|
    max_abs  v14, v13, v5, v4    ;# |Q1 - Q0|

    vcmpgtub v10, v14, v10      ;# HEV = true if thresh exceeded

    ;# Next limit
    max_abs  v14, v13, v0, v1    ;# |P3 - P2|
    max_abs  v14, v13, v1, v2    ;# |P2 - P1|
    max_abs  v14, v13, v6, v5    ;# |Q2 - Q1|
    max_abs  v14, v13, v7, v6    ;# |Q3 - Q2|

    vcmpgtub v9, v14, v9        ;# R = true if limit exceeded

    ;# flimit
    Abs     v14, v13, v3, v4    ;# |P0 - Q0|

    vcmpgtub v8, v14, v8        ;# X = true if flimit exceeded

    vor     v8, v8, v9          ;# R = true if flimit or limit exceeded
    ;# done building masks
.endm

.macro build_constants RFL, RLI, RTH, FL, LI, TH
    ;# build constants
    lvx     \FL, 0, \RFL        ;# flimit
    lvx     \LI, 0, \RLI        ;# limit
    lvx     \TH, 0, \RTH        ;# thresh

    vspltisb v11, 8
    vspltisb v12, 4
    vslb    v11, v11, v12       ;# 0x80808080808080808080808080808080
.endm

.macro load_data_y
    ;# setup strides/pointers to be able to access
    ;# all of the data
    add     r5, r4, r4          ;# r5 = 2 * stride
    sub     r6, r3, r5          ;# r6 -> 2 rows back
    neg     r7, r4              ;# r7 = -stride

    ;# load 16 pixels worth of data to work on
    sub     r0, r6, r5          ;# r0 -> 4 rows back (temp)
    lvx     v0,  0, r0          ;# P3  (read only)
    lvx     v1, r7, r6          ;# P2
    lvx     v2,  0, r6          ;# P1
    lvx     v3, r7, r3          ;# P0
    lvx     v4,  0, r3          ;# Q0
    lvx     v5, r4, r3          ;# Q1
    lvx     v6, r5, r3          ;# Q2
    add     r0, r3, r5          ;# r0 -> 2 rows fwd (temp)
    lvx     v7, r4, r0          ;# Q3  (read only)
.endm

;# Expects
;#  v10 == HEV
;#  v13 == tmp
;#  v14 == tmp
.macro common_adjust P0, Q0, P1, Q1, HEV_PRESENT
    vxor    \P1, \P1, v11       ;# SP1
    vxor    \P0, \P0, v11       ;# SP0
    vxor    \Q0, \Q0, v11       ;# SQ0
    vxor    \Q1, \Q1, v11       ;# SQ1

    vsubsbs v13, \P1, \Q1       ;# f  = c (P1 - Q1)
.if \HEV_PRESENT
    vand    v13, v13, v10       ;# f &= hev
.endif
    vsubsbs v14, \Q0, \P0       ;# -126 <=  X = Q0-P0  <= +126
    vaddsbs v13, v13, v14
    vaddsbs v13, v13, v14
    vaddsbs v13, v13, v14       ;# A = c( c(P1-Q1) + 3*(Q0-P0))

    vandc   v13, v13, v8        ;# f &= mask

    vspltisb v8, 3
    vspltisb v9, 4

    vaddsbs v14, v13, v9        ;# f1 = c (f+4)
    vaddsbs v15, v13, v8        ;# f2 = c (f+3)

    vsrab   v13, v14, v8        ;# f1 >>= 3
    vsrab   v15, v15, v8        ;# f2 >>= 3

    vsubsbs \Q0, \Q0, v13       ;# u1 = c (SQ0 - f1)
    vaddsbs \P0, \P0, v15       ;# u2 = c (SP0 + f2)
.endm

.macro vp8_mbfilter
    Masks

    ;# start the fitering here
    vxor    v1, v1, v11         ;# SP2
    vxor    v2, v2, v11         ;# SP1
    vxor    v3, v3, v11         ;# SP0
    vxor    v4, v4, v11         ;# SQ0
    vxor    v5, v5, v11         ;# SQ1
    vxor    v6, v6, v11         ;# SQ2

    ;# add outer taps if we have high edge variance
    vsubsbs v13, v2, v5         ;# f  = c (SP1-SQ1)

    vsubsbs v14, v4, v3         ;# SQ0-SP0
    vaddsbs v13, v13, v14
    vaddsbs v13, v13, v14
    vaddsbs v13, v13, v14       ;# f  = c( c(SP1-SQ1) + 3*(SQ0-SP0))

    vandc   v13, v13, v8        ;# f &= mask
    vand    v15, v13, v10       ;# f2 = f & hev

    ;# save bottom 3 bits so that we round one side +4 and the other +3
    vspltisb v8, 3
    vspltisb v9, 4

    vaddsbs v14, v15, v9        ;# f1 = c (f+4)
    vaddsbs v15, v15, v8        ;# f2 = c (f+3)

    vsrab   v14, v14, v8        ;# f1 >>= 3
    vsrab   v15, v15, v8        ;# f2 >>= 3

    vsubsbs v4, v4, v14         ;# u1 = c (SQ0 - f1)
    vaddsbs v3, v3, v15         ;# u2 = c (SP0 + f2)

    ;# only apply wider filter if not high edge variance
    vandc   v13, v13, v10       ;# f &= ~hev

    vspltisb v9, 2
    vnor    v8, v8, v8
    vsrb    v9, v8, v9          ;# 0x3f3f3f3f3f3f3f3f3f3f3f3f3f3f3f3f
    vupkhsb v9, v9              ;# 0x003f003f003f003f003f003f003f003f
    vspltisb v8, 9

    ;# roughly 1/7th difference across boundary
    vspltish v10, 7
    vmulosb v14, v8, v13        ;# A = c( c(P1-Q1) + 3*(Q0-P0))
    vmulesb v15, v8, v13
    vaddshs v14, v14, v9        ;# +=  63
    vaddshs v15, v15, v9
    vsrah   v14, v14, v10       ;# >>= 7
    vsrah   v15, v15, v10
    vmrglh  v10, v15, v14
    vmrghh  v15, v15, v14

    vpkshss v10, v15, v10       ;# X = saturated down to bytes

    vsubsbs v6, v6, v10         ;# subtract from Q and add to P
    vaddsbs v1, v1, v10

    vxor    v6, v6, v11
    vxor    v1, v1, v11

    ;# roughly 2/7th difference across boundary
    vspltish v10, 7
    vaddubm v12, v8, v8
    vmulosb v14, v12, v13       ;# A = c( c(P1-Q1) + 3*(Q0-P0))
    vmulesb v15, v12, v13
    vaddshs v14, v14, v9
    vaddshs v15, v15, v9
    vsrah   v14, v14, v10       ;# >>= 7
    vsrah   v15, v15, v10
    vmrglh  v10, v15, v14
    vmrghh  v15, v15, v14

    vpkshss v10, v15, v10       ;# X = saturated down to bytes

    vsubsbs v5, v5, v10         ;# subtract from Q and add to P
    vaddsbs v2, v2, v10

    vxor    v5, v5, v11
    vxor    v2, v2, v11

    ;# roughly 3/7th difference across boundary
    vspltish v10, 7
    vaddubm v12, v12, v8
    vmulosb v14, v12, v13       ;# A = c( c(P1-Q1) + 3*(Q0-P0))
    vmulesb v15, v12, v13
    vaddshs v14, v14, v9
    vaddshs v15, v15, v9
    vsrah   v14, v14, v10       ;# >>= 7
    vsrah   v15, v15, v10
    vmrglh  v10, v15, v14
    vmrghh  v15, v15, v14

    vpkshss v10, v15, v10       ;# X = saturated down to bytes

    vsubsbs v4, v4, v10         ;# subtract from Q and add to P
    vaddsbs v3, v3, v10

    vxor    v4, v4, v11
    vxor    v3, v3, v11
.endm

.macro SBFilter
    Masks

    common_adjust v3, v4, v2, v5, 1

    ;# outer tap adjustments
    vspltisb v8, 1

    vaddubm v13, v13, v8        ;# f  += 1
    vsrab   v13, v13, v8        ;# f >>= 1

    vandc   v13, v13, v10       ;# f &= ~hev

    vsubsbs v5, v5, v13         ;# u1 = c (SQ1 - f)
    vaddsbs v2, v2, v13         ;# u2 = c (SP1 + f)

    vxor    v2, v2, v11
    vxor    v3, v3, v11
    vxor    v4, v4, v11
    vxor    v5, v5, v11
.endm

    .align 2
mbloop_filter_horizontal_edge_y_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    mtspr   256, r12            ;# set VRSAVE

    build_constants r5, r6, r7, v8, v9, v10

    load_data_y

    vp8_mbfilter

    stvx     v1, r7, r6         ;# P2
    stvx     v2,  0, r6         ;# P1
    stvx     v3, r7, r3         ;# P0
    stvx     v4,  0, r3         ;# Q0
    stvx     v5, r4, r3         ;# Q1
    stvx     v6, r5, r3         ;# Q2

    mtspr   256, r11            ;# reset old VRSAVE

    blr

    .align 2
;#  r3 unsigned char *s
;#  r4 int p
;#  r5 const signed char *flimit
;#  r6 const signed char *limit
;#  r7 const signed char *thresh
loop_filter_horizontal_edge_y_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    mtspr   256, r12            ;# set VRSAVE

    build_constants r5, r6, r7, v8, v9, v10

    load_data_y

    SBFilter

    stvx     v2,  0, r6         ;# P1
    stvx     v3, r7, r3         ;# P0
    stvx     v4,  0, r3         ;# Q0
    stvx     v5, r4, r3         ;# Q1

    mtspr   256, r11            ;# reset old VRSAVE

    blr

;# Filtering a vertical mb.  Each mb is aligned on a 16 byte boundary.
;#  So we can read in an entire mb aligned.  However if we want to filter the mb
;#  edge we run into problems.  For the loopfilter we require 4 bytes before the mb
;#  and 4 after for a total of 8 bytes.  Reading 16 bytes inorder to get 4 is a bit
;#  of a waste.  So this is an even uglier way to get around that.
;# Using the regular register file words are read in and then saved back out to
;#  memory to align and order them up.  Then they are read in using the
;#  vector register file.
.macro RLVmb V, R
    lwzux   r0, r3, r4
    stw     r0, 4(\R)
    lwz     r0,-4(r3)
    stw     r0, 0(\R)
    lwzux   r0, r3, r4
    stw     r0,12(\R)
    lwz     r0,-4(r3)
    stw     r0, 8(\R)
    lvx     \V, 0, \R
.endm

.macro WLVmb V, R
    stvx    \V, 0, \R
    lwz     r0,12(\R)
    stwux   r0, r3, r4
    lwz     r0, 8(\R)
    stw     r0,-4(r3)
    lwz     r0, 4(\R)
    stwux   r0, r3, r4
    lwz     r0, 0(\R)
    stw     r0,-4(r3)
.endm

    .align 2
;#  r3 unsigned char *s
;#  r4 int p
;#  r5 const signed char *flimit
;#  r6 const signed char *limit
;#  r7 const signed char *thresh
mbloop_filter_vertical_edge_y_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    ori     r12, r12, 0xc000
    mtspr   256, r12            ;# set VRSAVE

    la      r9, -48(r1)         ;# temporary space for reading in vectors
    sub     r3, r3, r4

    RLVmb v0, r9
    RLVmb v1, r9
    RLVmb v2, r9
    RLVmb v3, r9
    RLVmb v4, r9
    RLVmb v5, r9
    RLVmb v6, r9
    RLVmb v7, r9

    transpose8x16_fwd

    build_constants r5, r6, r7, v8, v9, v10

    vp8_mbfilter

    transpose8x16_inv

    add r3, r3, r4
    neg r4, r4

    WLVmb v17, r9
    WLVmb v16, r9
    WLVmb v15, r9
    WLVmb v14, r9
    WLVmb v13, r9
    WLVmb v12, r9
    WLVmb v11, r9
    WLVmb v10, r9

    mtspr   256, r11            ;# reset old VRSAVE

    blr

.macro RL V, R, P
    lvx     \V, 0,  \R
    add     \R, \R, \P
.endm

.macro WL V, R, P
    stvx    \V, 0,  \R
    add     \R, \R, \P
.endm

.macro Fil P3, P2, P1, P0, Q0, Q1, Q2, Q3
                                ;# K = |P0-P1| already
    Abs     v14, v13, \Q0, \Q1  ;# M = |Q0-Q1|
    vmaxub  v14, v14, v4        ;# M = max( |P0-P1|, |Q0-Q1|)
    vcmpgtub v10, v14, v0

    Abs     v4, v5, \Q2, \Q3    ;# K = |Q2-Q3| = next |P0-P1]

    max_abs  v14, v13, \Q1, \Q2  ;# M = max( M, |Q1-Q2|)
    max_abs  v14, v13, \P1, \P2  ;# M = max( M, |P1-P2|)
    max_abs  v14, v13, \P2, \P3  ;# M = max( M, |P2-P3|)

    vmaxub   v14, v14, v4       ;# M = max interior abs diff
    vcmpgtub v9, v14, v2        ;# M = true if int_l exceeded

    Abs     v14, v13, \P0, \Q0  ;# X = Abs( P0-Q0)
    vcmpgtub v8, v14, v3        ;# X = true if edge_l exceeded
    vor     v8, v8, v9          ;# M = true if edge_l or int_l exceeded

    ;# replace P1,Q1 w/signed versions
    common_adjust \P0, \Q0, \P1, \Q1, 1

    vaddubm v13, v13, v1        ;# -16 <= M <= 15, saturation irrelevant
    vsrab   v13, v13, v1
    vandc   v13, v13, v10       ;# adjust P1,Q1 by (M+1)>>1  if ! hev
    vsubsbs \Q1, \Q1, v13
    vaddsbs \P1, \P1, v13

    vxor    \P1, \P1, v11       ;# P1
    vxor    \P0, \P0, v11       ;# P0
    vxor    \Q0, \Q0, v11       ;# Q0
    vxor    \Q1, \Q1, v11       ;# Q1
.endm


    .align 2
;#  r3 unsigned char *s
;#  r4 int p
;#  r5 const signed char *flimit
;#  r6 const signed char *limit
;#  r7 const signed char *thresh
loop_filter_vertical_edge_y_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    ori     r12, r12, 0xffff
    mtspr   256, r12            ;# set VRSAVE

    addi    r9, r3, 0
    RL      v16, r9, r4
    RL      v17, r9, r4
    RL      v18, r9, r4
    RL      v19, r9, r4
    RL      v20, r9, r4
    RL      v21, r9, r4
    RL      v22, r9, r4
    RL      v23, r9, r4
    RL      v24, r9, r4
    RL      v25, r9, r4
    RL      v26, r9, r4
    RL      v27, r9, r4
    RL      v28, r9, r4
    RL      v29, r9, r4
    RL      v30, r9, r4
    lvx     v31, 0, r9

    Transpose16x16

    vspltisb v1, 1

    build_constants r5, r6, r7, v3, v2, v0

    Abs v4, v5, v19, v18                            ;# K(v14) = first |P0-P1|

    Fil v16, v17, v18, v19,  v20, v21, v22, v23
    Fil v20, v21, v22, v23,  v24, v25, v26, v27
    Fil v24, v25, v26, v27,  v28, v29, v30, v31

    Transpose16x16

    addi    r9, r3, 0
    WL      v16, r9, r4
    WL      v17, r9, r4
    WL      v18, r9, r4
    WL      v19, r9, r4
    WL      v20, r9, r4
    WL      v21, r9, r4
    WL      v22, r9, r4
    WL      v23, r9, r4
    WL      v24, r9, r4
    WL      v25, r9, r4
    WL      v26, r9, r4
    WL      v27, r9, r4
    WL      v28, r9, r4
    WL      v29, r9, r4
    WL      v30, r9, r4
    stvx    v31, 0, r9

    mtspr   256, r11            ;# reset old VRSAVE

    blr

;# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- UV FILTERING -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
.macro active_chroma_sel V
    andi.   r7, r3, 8       ;# row origin modulo 16
    add     r7, r7, r7      ;# selects selectors
    lis     r12, _chromaSelectors@ha
    la      r0,  _chromaSelectors@l(r12)
    lwzux   r0, r7, r0      ;# leave selector addr in r7

    lvx     \V, 0, r0       ;# mask to concatenate active U,V pels
.endm

.macro hread_uv Dest, U, V, Offs, VMask
    lvx     \U, \Offs, r3
    lvx     \V, \Offs, r4
    vperm   \Dest, \U, \V, \VMask   ;# Dest = active part of U then V
.endm

.macro hwrite_uv New, U, V, Offs, Umask, Vmask
    vperm   \U, \New, \U, \Umask    ;# Combine new pels with siblings
    vperm   \V, \New, \V, \Vmask
    stvx    \U, \Offs, r3           ;# Write to frame buffer
    stvx    \V, \Offs, r4
.endm

;# Process U,V in parallel.
.macro load_chroma_h
    neg     r9, r5          ;# r9 = -1 * stride
    add     r8, r9, r9      ;# r8 = -2 * stride
    add     r10, r5, r5     ;# r10 = 2 * stride

    active_chroma_sel v12

    ;# P3, Q3 are read-only; need not save addresses or sibling pels
    add     r6, r8, r8      ;# r6 = -4 * stride
    hread_uv v0, v14, v15, r6, v12
    add     r6, r10, r5     ;# r6 =  3 * stride
    hread_uv v7, v14, v15, r6, v12

    ;# Others are read/write; save addresses and sibling pels

    add     r6, r8, r9      ;# r6 = -3 * stride
    hread_uv v1, v16, v17, r6,  v12
    hread_uv v2, v18, v19, r8,  v12
    hread_uv v3, v20, v21, r9,  v12
    hread_uv v4, v22, v23, 0,   v12
    hread_uv v5, v24, v25, r5,  v12
    hread_uv v6, v26, v27, r10, v12
.endm

.macro uresult_sel V
    load_g   \V, 4(r7)
.endm

.macro vresult_sel V
    load_g   \V, 8(r7)
.endm

;# always write P1,P0,Q0,Q1
.macro store_chroma_h
    uresult_sel v11
    vresult_sel v12
    hwrite_uv v2, v18, v19, r8, v11, v12
    hwrite_uv v3, v20, v21, r9, v11, v12
    hwrite_uv v4, v22, v23, 0,  v11, v12
    hwrite_uv v5, v24, v25, r5, v11, v12
.endm

    .align 2
;#  r3 unsigned char *u
;#  r4 unsigned char *v
;#  r5 int p
;#  r6 const signed char *flimit
;#  r7 const signed char *limit
;#  r8 const signed char *thresh
mbloop_filter_horizontal_edge_uv_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    ori     r12, r12, 0xffff
    mtspr   256, r12            ;# set VRSAVE

    build_constants r6, r7, r8, v8, v9, v10

    load_chroma_h

    vp8_mbfilter

    store_chroma_h

    hwrite_uv v1, v16, v17, r6,  v11, v12    ;# v1 == P2
    hwrite_uv v6, v26, v27, r10, v11, v12    ;# v6 == Q2

    mtspr   256, r11            ;# reset old VRSAVE

    blr

    .align 2
;#  r3 unsigned char *u
;#  r4 unsigned char *v
;#  r5 int p
;#  r6 const signed char *flimit
;#  r7 const signed char *limit
;#  r8 const signed char *thresh
loop_filter_horizontal_edge_uv_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    ori     r12, r12, 0xffff
    mtspr   256, r12            ;# set VRSAVE

    build_constants r6, r7, r8, v8, v9, v10

    load_chroma_h

    SBFilter

    store_chroma_h

    mtspr   256, r11            ;# reset old VRSAVE

    blr

.macro R V, R
    lwzux   r0, r3, r5
    stw     r0, 4(\R)
    lwz     r0,-4(r3)
    stw     r0, 0(\R)
    lwzux   r0, r4, r5
    stw     r0,12(\R)
    lwz     r0,-4(r4)
    stw     r0, 8(\R)
    lvx     \V, 0, \R
.endm


.macro W V, R
    stvx    \V, 0, \R
    lwz     r0,12(\R)
    stwux   r0, r4, r5
    lwz     r0, 8(\R)
    stw     r0,-4(r4)
    lwz     r0, 4(\R)
    stwux   r0, r3, r5
    lwz     r0, 0(\R)
    stw     r0,-4(r3)
.endm

.macro chroma_vread R
    sub r3, r3, r5          ;# back up one line for simplicity
    sub r4, r4, r5

    R v0, \R
    R v1, \R
    R v2, \R
    R v3, \R
    R v4, \R
    R v5, \R
    R v6, \R
    R v7, \R

    transpose8x16_fwd
.endm

.macro chroma_vwrite R

    transpose8x16_inv

    add     r3, r3, r5
    add     r4, r4, r5
    neg     r5, r5          ;# Write rows back in reverse order

    W v17, \R
    W v16, \R
    W v15, \R
    W v14, \R
    W v13, \R
    W v12, \R
    W v11, \R
    W v10, \R
.endm

    .align 2
;#  r3 unsigned char *u
;#  r4 unsigned char *v
;#  r5 int p
;#  r6 const signed char *flimit
;#  r7 const signed char *limit
;#  r8 const signed char *thresh
mbloop_filter_vertical_edge_uv_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    ori     r12, r12, 0xc000
    mtspr   256, r12            ;# set VRSAVE

    la      r9, -48(r1)         ;# temporary space for reading in vectors

    chroma_vread r9

    build_constants r6, r7, r8, v8, v9, v10

    vp8_mbfilter

    chroma_vwrite r9

    mtspr   256, r11            ;# reset old VRSAVE

    blr

    .align 2
;#  r3 unsigned char *u
;#  r4 unsigned char *v
;#  r5 int p
;#  r6 const signed char *flimit
;#  r7 const signed char *limit
;#  r8 const signed char *thresh
loop_filter_vertical_edge_uv_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    ori     r12, r12, 0xc000
    mtspr   256, r12            ;# set VRSAVE

    la      r9, -48(r1)         ;# temporary space for reading in vectors

    chroma_vread r9

    build_constants r6, r7, r8, v8, v9, v10

    SBFilter

    chroma_vwrite r9

    mtspr   256, r11            ;# reset old VRSAVE

    blr

;# -=-=-=-=-=-=-=-=-=-=-=-=-=-= SIMPLE LOOP FILTER =-=-=-=-=-=-=-=-=-=-=-=-=-=-

.macro vp8_simple_filter
    Abs v14, v13, v1, v2    ;# M = abs( P0 - Q0)
    vcmpgtub v8, v14, v8    ;# v5 = true if _over_ limit

    ;# preserve unsigned v0 and v3
    common_adjust v1, v2, v0, v3, 0

    vxor v1, v1, v11
    vxor v2, v2, v11        ;# cvt Q0, P0 back to pels
.endm

.macro simple_vertical
    addi    r8,  0, 16
    addi    r7, r5, 32

    lvx     v0,  0, r5
    lvx     v1, r8, r5
    lvx     v2,  0, r7
    lvx     v3, r8, r7

    lis     r12, _B_hihi@ha
    la      r0,  _B_hihi@l(r12)
    lvx     v16, 0, r0

    lis     r12, _B_lolo@ha
    la      r0,  _B_lolo@l(r12)
    lvx     v17, 0, r0

    Transpose4times4x4 v16, v17
    vp8_simple_filter

    vxor v0, v0, v11
    vxor v3, v3, v11        ;# cvt Q0, P0 back to pels

    Transpose4times4x4 v16, v17

    stvx    v0,  0, r5
    stvx    v1, r8, r5
    stvx    v2,  0, r7
    stvx    v3, r8, r7
.endm

    .align 2
;#  r3 unsigned char *s
;#  r4 int p
;#  r5 const signed char *flimit
loop_filter_simple_horizontal_edge_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    mtspr   256, r12            ;# set VRSAVE

    ;# build constants
    lvx     v8, 0, r5           ;# flimit

    vspltisb v11, 8
    vspltisb v12, 4
    vslb    v11, v11, v12       ;# 0x80808080808080808080808080808080

    neg     r5, r4              ;# r5 = -1 * stride
    add     r6, r5, r5          ;# r6 = -2 * stride

    lvx     v0, r6, r3          ;# v0 = P1 = 16 pels two rows above edge
    lvx     v1, r5, r3          ;# v1 = P0 = 16 pels one row  above edge
    lvx     v2,  0, r3          ;# v2 = Q0 = 16 pels one row  below edge
    lvx     v3, r4, r3          ;# v3 = Q1 = 16 pels two rows below edge

    vp8_simple_filter

    stvx    v1, r5, r3          ;# store P0
    stvx    v2,  0, r3          ;# store Q0

    mtspr   256, r11            ;# reset old VRSAVE

    blr

.macro RLV Offs
    stw     r0, (\Offs*4)(r5)
    lwzux   r0, r7, r4
.endm

.macro WLV Offs
    lwz     r0, (\Offs*4)(r5)
    stwux   r0, r7, r4
.endm

    .align 2
;#  r3 unsigned char *s
;#  r4 int p
;#  r5 const signed char *flimit
loop_filter_simple_vertical_edge_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    ori     r12, r12, 0xc000
    mtspr   256, r12            ;# set VRSAVE

    ;# build constants
    lvx     v8, 0, r5           ;# flimit

    vspltisb v11, 8
    vspltisb v12, 4
    vslb    v11, v11, v12       ;# 0x80808080808080808080808080808080

    la r5, -96(r1)              ;# temporary space for reading in vectors

    ;# Store 4 pels at word "Offs" in temp array, then advance r7
    ;#   to next row and read another 4 pels from the frame buffer.

    subi    r7, r3,  2          ;# r7 -> 2 pels before start
    lwzx    r0,  0, r7          ;# read first 4 pels

    ;# 16 unaligned word accesses
    RLV 0
    RLV 4
    RLV 8
    RLV 12
    RLV 1
    RLV 5
    RLV 9
    RLV 13
    RLV 2
    RLV 6
    RLV 10
    RLV 14
    RLV 3
    RLV 7
    RLV 11

    stw     r0, (15*4)(r5)      ;# write last 4 pels

    simple_vertical

    ;# Read temp array, write frame buffer.
    subi    r7, r3,  2          ;# r7 -> 2 pels before start
    lwzx    r0,  0, r5          ;# read/write first 4 pels
    stwx    r0,  0, r7

    WLV 4
    WLV 8
    WLV 12
    WLV 1
    WLV 5
    WLV 9
    WLV 13
    WLV 2
    WLV 6
    WLV 10
    WLV 14
    WLV 3
    WLV 7
    WLV 11
    WLV 15

    mtspr   256, r11            ;# reset old VRSAVE

    blr

    .data

_chromaSelectors:
    .long   _B_hihi
    .long   _B_Ures0
    .long   _B_Vres0
    .long   0
    .long   _B_lolo
    .long   _B_Ures8
    .long   _B_Vres8
    .long   0

    .align 4
_B_Vres8:
    .byte   16, 17, 18, 19, 20, 21, 22, 23,  8,  9, 10, 11, 12, 13, 14, 15

    .align 4
_B_Ures8:
    .byte   16, 17, 18, 19, 20, 21, 22, 23,  0,  1,  2,  3,  4,  5,  6,  7

    .align 4
_B_lolo:
    .byte    8,  9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31

    .align 4
_B_Vres0:
    .byte    8,  9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31
    .align 4
_B_Ures0:
    .byte    0,  1,  2,  3,  4,  5,  6,  7, 24, 25, 26, 27, 28, 29, 30, 31

    .align 4
_B_hihi:
    .byte    0,  1,  2,  3,  4,  5,  6,  7, 16, 17, 18, 19, 20, 21, 22, 23
