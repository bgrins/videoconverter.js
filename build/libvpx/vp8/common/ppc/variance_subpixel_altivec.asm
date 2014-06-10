;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl vp8_sub_pixel_variance4x4_ppc
    .globl vp8_sub_pixel_variance8x8_ppc
    .globl vp8_sub_pixel_variance8x16_ppc
    .globl vp8_sub_pixel_variance16x8_ppc
    .globl vp8_sub_pixel_variance16x16_ppc

.macro load_c V, LABEL, OFF, R0, R1
    lis     \R0, \LABEL@ha
    la      \R1, \LABEL@l(\R0)
    lvx     \V, \OFF, \R1
.endm

.macro load_vfilter V0, V1
    load_c \V0, vfilter_b, r6, r12, r10

    addi    r6,  r6, 16
    lvx     \V1, r6, r10
.endm

.macro HProlog jump_label
    ;# load up horizontal filter
    slwi.   r5, r5, 4           ;# index into horizontal filter array

    ;# index to the next set of vectors in the row.
    li      r10, 16

    ;# downshift by 7 ( divide by 128 ) at the end
    vspltish v19, 7

    ;# If there isn't any filtering to be done for the horizontal, then
    ;#  just skip to the second pass.
    beq     \jump_label

    load_c v20, hfilter_b, r5, r12, r0

    ;# setup constants
    ;# v14 permutation value for alignment
    load_c v28, b_hperm_b, 0, r12, r0

    ;# index to the next set of vectors in the row.
    li      r12, 32

    ;# rounding added in on the multiply
    vspltisw v21, 8
    vspltisw v18, 3
    vslw    v18, v21, v18       ;# 0x00000040000000400000004000000040

    slwi.   r6, r6, 5           ;# index into vertical filter array
.endm

;# Filters a horizontal line
;# expects:
;#  r3  src_ptr
;#  r4  pitch
;#  r10 16
;#  r12 32
;#  v17 perm intput
;#  v18 rounding
;#  v19 shift
;#  v20 filter taps
;#  v21 tmp
;#  v22 tmp
;#  v23 tmp
;#  v24 tmp
;#  v25 tmp
;#  v26 tmp
;#  v27 tmp
;#  v28 perm output
;#

.macro hfilter_8 V, hp, lp, increment_counter
    lvsl    v17,  0, r3         ;# permutate value for alignment

    ;# input to filter is 9 bytes wide, output is 8 bytes.
    lvx     v21,   0, r3
    lvx     v22, r10, r3

.if \increment_counter
    add     r3, r3, r4
.endif
    vperm   v21, v21, v22, v17

    vperm   v24, v21, v21, \hp  ;# v20 = 0123 1234 2345 3456
    vperm   v25, v21, v21, \lp  ;# v21 = 4567 5678 6789 789A

    vmsummbm v24, v20, v24, v18
    vmsummbm v25, v20, v25, v18

    vpkswus v24, v24, v25       ;# v24 = 0 4 8 C 1 5 9 D (16-bit)

    vsrh    v24, v24, v19       ;# divide v0, v1 by 128

    vpkuhus \V, v24, v24        ;# \V = scrambled 8-bit result
.endm

.macro vfilter_16 P0 P1
    vmuleub v22, \P0, v20       ;# 64 + 4 positive taps
    vadduhm v22, v18, v22
    vmuloub v23, \P0, v20
    vadduhm v23, v18, v23

    vmuleub v24, \P1, v21
    vadduhm v22, v22, v24       ;# Re = evens, saturation unnecessary
    vmuloub v25, \P1, v21
    vadduhm v23, v23, v25       ;# Ro = odds

    vsrh    v22, v22, v19       ;# divide by 128
    vsrh    v23, v23, v19       ;# v16 v17 = evens, odds
    vmrghh  \P0, v22, v23       ;# v18 v19 = 16-bit result in order
    vmrglh  v23, v22, v23
    vpkuhus \P0, \P0, v23       ;# P0 = 8-bit result
.endm

.macro compute_sum_sse src, ref, sum, sse, t1, t2, z0
    ;# Compute sum first.  Unpack to so signed subract
    ;#  can be used.  Only have a half word signed
    ;#  subract.  Do high, then low.
    vmrghb  \t1, \z0, \src
    vmrghb  \t2, \z0, \ref
    vsubshs \t1, \t1, \t2
    vsum4shs \sum, \t1, \sum

    vmrglb  \t1, \z0, \src
    vmrglb  \t2, \z0, \ref
    vsubshs \t1, \t1, \t2
    vsum4shs \sum, \t1, \sum

    ;# Now compute sse.
    vsububs \t1, \src, \ref
    vsububs \t2, \ref, \src
    vor     \t1, \t1, \t2

    vmsumubm \sse, \t1, \t1, \sse
.endm

.macro variance_final sum, sse, z0, DS
    vsumsws \sum, \sum, \z0
    vsumsws \sse, \sse, \z0

    stvx    \sum, 0, r1
    lwz     r3, 12(r1)

    stvx    \sse, 0, r1
    lwz     r4, 12(r1)

    stw     r4, 0(r9)           ;# sse

    mullw   r3, r3, r3          ;# sum*sum
    srlwi   r3, r3, \DS         ;# (sum*sum) >> 8
    subf    r3, r3, r4          ;# sse - ((sum*sum) >> 8)
.endm

.macro compute_sum_sse_16 V, increment_counter
    load_and_align_16  v16, r7, r8, \increment_counter
    compute_sum_sse \V, v16, v18, v19, v20, v21, v23
.endm

.macro load_and_align_16 V, R, P, increment_counter
    lvsl    v17,  0, \R         ;# permutate value for alignment

    ;# input to filter is 21 bytes wide, output is 16 bytes.
    ;#  input will can span three vectors if not aligned correctly.
    lvx     v21,   0, \R
    lvx     v22, r10, \R

.if \increment_counter
    add     \R, \R, \P
.endif

    vperm   \V, v21, v22, v17
.endm

    .align 2
;# r3 unsigned char  *src_ptr
;# r4 int  src_pixels_per_line
;# r5 int  xoffset
;# r6 int  yoffset
;# r7 unsigned char *dst_ptr
;# r8 int dst_pixels_per_line
;# r9 unsigned int *sse
;#
;# r3 return value
vp8_sub_pixel_variance4x4_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xf830
    ori     r12, r12, 0xfff8
    mtspr   256, r12            ;# set VRSAVE

    stwu    r1,-32(r1)          ;# create space on the stack

    HProlog second_pass_4x4_pre_copy_b

    ;# Load up permutation constants
    load_c v10, b_0123_b, 0, r12, r0
    load_c v11, b_4567_b, 0, r12, r0

    hfilter_8 v0, v10, v11, 1
    hfilter_8 v1, v10, v11, 1
    hfilter_8 v2, v10, v11, 1
    hfilter_8 v3, v10, v11, 1

    ;# Finished filtering main horizontal block.  If there is no
    ;#  vertical filtering, jump to storing the data.  Otherwise
    ;#  load up and filter the additional line that is needed
    ;#  for the vertical filter.
    beq     compute_sum_sse_4x4_b

    hfilter_8 v4, v10, v11, 0

    b   second_pass_4x4_b

second_pass_4x4_pre_copy_b:
    slwi    r6, r6, 5           ;# index into vertical filter array

    load_and_align_16 v0, r3, r4, 1
    load_and_align_16 v1, r3, r4, 1
    load_and_align_16 v2, r3, r4, 1
    load_and_align_16 v3, r3, r4, 1
    load_and_align_16 v4, r3, r4, 0

second_pass_4x4_b:
    vspltish v20, 8
    vspltish v18, 3
    vslh    v18, v20, v18       ;# 0x0040 0040 0040 0040 0040 0040 0040 0040

    load_vfilter v20, v21

    vfilter_16 v0,  v1
    vfilter_16 v1,  v2
    vfilter_16 v2,  v3
    vfilter_16 v3,  v4

compute_sum_sse_4x4_b:
    vspltish v18, 0             ;# sum
    vspltish v19, 0             ;# sse
    vspltish v23, 0             ;# unpack
    li      r10, 16

    load_and_align_16 v4, r7, r8, 1
    load_and_align_16 v5, r7, r8, 1
    load_and_align_16 v6, r7, r8, 1
    load_and_align_16 v7, r7, r8, 1

    vmrghb  v0, v0, v1
    vmrghb  v1, v2, v3

    vmrghb  v2, v4, v5
    vmrghb  v3, v6, v7

    load_c v10, b_hilo_b, 0, r12, r0

    vperm   v0, v0, v1, v10
    vperm   v1, v2, v3, v10

    compute_sum_sse v0, v1, v18, v19, v20, v21, v23

    variance_final v18, v19, v23, 4

    addi    r1, r1, 32          ;# recover stack
    mtspr   256, r11            ;# reset old VRSAVE

    blr

    .align 2
;# r3 unsigned char  *src_ptr
;# r4 int  src_pixels_per_line
;# r5 int  xoffset
;# r6 int  yoffset
;# r7 unsigned char *dst_ptr
;# r8 int dst_pixels_per_line
;# r9 unsigned int *sse
;#
;# r3 return value
vp8_sub_pixel_variance8x8_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xfff0
    ori     r12, r12, 0xffff
    mtspr   256, r12            ;# set VRSAVE

    stwu    r1,-32(r1)          ;# create space on the stack

    HProlog second_pass_8x8_pre_copy_b

    ;# Load up permutation constants
    load_c v10, b_0123_b, 0, r12, r0
    load_c v11, b_4567_b, 0, r12, r0

    hfilter_8 v0, v10, v11, 1
    hfilter_8 v1, v10, v11, 1
    hfilter_8 v2, v10, v11, 1
    hfilter_8 v3, v10, v11, 1
    hfilter_8 v4, v10, v11, 1
    hfilter_8 v5, v10, v11, 1
    hfilter_8 v6, v10, v11, 1
    hfilter_8 v7, v10, v11, 1

    ;# Finished filtering main horizontal block.  If there is no
    ;#  vertical filtering, jump to storing the data.  Otherwise
    ;#  load up and filter the additional line that is needed
    ;#  for the vertical filter.
    beq     compute_sum_sse_8x8_b

    hfilter_8 v8, v10, v11, 0

    b   second_pass_8x8_b

second_pass_8x8_pre_copy_b:
    slwi.   r6, r6, 5           ;# index into vertical filter array

    load_and_align_16 v0, r3, r4, 1
    load_and_align_16 v1, r3, r4, 1
    load_and_align_16 v2, r3, r4, 1
    load_and_align_16 v3, r3, r4, 1
    load_and_align_16 v4, r3, r4, 1
    load_and_align_16 v5, r3, r4, 1
    load_and_align_16 v6, r3, r4, 1
    load_and_align_16 v7, r3, r4, 1
    load_and_align_16 v8, r3, r4, 0

    beq     compute_sum_sse_8x8_b

second_pass_8x8_b:
    vspltish v20, 8
    vspltish v18, 3
    vslh    v18, v20, v18   ;# 0x0040 0040 0040 0040 0040 0040 0040 0040

    load_vfilter v20, v21

    vfilter_16 v0, v1
    vfilter_16 v1, v2
    vfilter_16 v2, v3
    vfilter_16 v3, v4
    vfilter_16 v4, v5
    vfilter_16 v5, v6
    vfilter_16 v6, v7
    vfilter_16 v7, v8

compute_sum_sse_8x8_b:
    vspltish v18, 0             ;# sum
    vspltish v19, 0             ;# sse
    vspltish v23, 0             ;# unpack
    li      r10, 16

    vmrghb  v0, v0, v1
    vmrghb  v1, v2, v3
    vmrghb  v2, v4, v5
    vmrghb  v3, v6, v7

    load_and_align_16 v4,  r7, r8, 1
    load_and_align_16 v5,  r7, r8, 1
    load_and_align_16 v6,  r7, r8, 1
    load_and_align_16 v7,  r7, r8, 1
    load_and_align_16 v8,  r7, r8, 1
    load_and_align_16 v9,  r7, r8, 1
    load_and_align_16 v10, r7, r8, 1
    load_and_align_16 v11, r7, r8, 0

    vmrghb  v4, v4,  v5
    vmrghb  v5, v6,  v7
    vmrghb  v6, v8,  v9
    vmrghb  v7, v10, v11

    compute_sum_sse v0, v4, v18, v19, v20, v21, v23
    compute_sum_sse v1, v5, v18, v19, v20, v21, v23
    compute_sum_sse v2, v6, v18, v19, v20, v21, v23
    compute_sum_sse v3, v7, v18, v19, v20, v21, v23

    variance_final v18, v19, v23, 6

    addi    r1, r1, 32          ;# recover stack
    mtspr   256, r11            ;# reset old VRSAVE
    blr

    .align 2
;# r3 unsigned char  *src_ptr
;# r4 int  src_pixels_per_line
;# r5 int  xoffset
;# r6 int  yoffset
;# r7 unsigned char *dst_ptr
;# r8 int dst_pixels_per_line
;# r9 unsigned int *sse
;#
;# r3 return value
vp8_sub_pixel_variance8x16_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    ori     r12, r12, 0xfffc
    mtspr   256, r12            ;# set VRSAVE

    stwu    r1,-32(r1)          ;# create space on the stack

    HProlog second_pass_8x16_pre_copy_b

    ;# Load up permutation constants
    load_c v29, b_0123_b, 0, r12, r0
    load_c v30, b_4567_b, 0, r12, r0

    hfilter_8 v0,  v29, v30, 1
    hfilter_8 v1,  v29, v30, 1
    hfilter_8 v2,  v29, v30, 1
    hfilter_8 v3,  v29, v30, 1
    hfilter_8 v4,  v29, v30, 1
    hfilter_8 v5,  v29, v30, 1
    hfilter_8 v6,  v29, v30, 1
    hfilter_8 v7,  v29, v30, 1
    hfilter_8 v8,  v29, v30, 1
    hfilter_8 v9,  v29, v30, 1
    hfilter_8 v10, v29, v30, 1
    hfilter_8 v11, v29, v30, 1
    hfilter_8 v12, v29, v30, 1
    hfilter_8 v13, v29, v30, 1
    hfilter_8 v14, v29, v30, 1
    hfilter_8 v15, v29, v30, 1

    ;# Finished filtering main horizontal block.  If there is no
    ;#  vertical filtering, jump to storing the data.  Otherwise
    ;#  load up and filter the additional line that is needed
    ;#  for the vertical filter.
    beq     compute_sum_sse_8x16_b

    hfilter_8 v16, v29, v30, 0

    b   second_pass_8x16_b

second_pass_8x16_pre_copy_b:
    slwi.   r6, r6, 5           ;# index into vertical filter array

    load_and_align_16 v0,  r3, r4, 1
    load_and_align_16 v1,  r3, r4, 1
    load_and_align_16 v2,  r3, r4, 1
    load_and_align_16 v3,  r3, r4, 1
    load_and_align_16 v4,  r3, r4, 1
    load_and_align_16 v5,  r3, r4, 1
    load_and_align_16 v6,  r3, r4, 1
    load_and_align_16 v7,  r3, r4, 1
    load_and_align_16 v8,  r3, r4, 1
    load_and_align_16 v9,  r3, r4, 1
    load_and_align_16 v10, r3, r4, 1
    load_and_align_16 v11, r3, r4, 1
    load_and_align_16 v12, r3, r4, 1
    load_and_align_16 v13, r3, r4, 1
    load_and_align_16 v14, r3, r4, 1
    load_and_align_16 v15, r3, r4, 1
    load_and_align_16 v16, r3, r4, 0

    beq     compute_sum_sse_8x16_b

second_pass_8x16_b:
    vspltish v20, 8
    vspltish v18, 3
    vslh    v18, v20, v18   ;# 0x0040 0040 0040 0040 0040 0040 0040 0040

    load_vfilter v20, v21

    vfilter_16 v0,  v1
    vfilter_16 v1,  v2
    vfilter_16 v2,  v3
    vfilter_16 v3,  v4
    vfilter_16 v4,  v5
    vfilter_16 v5,  v6
    vfilter_16 v6,  v7
    vfilter_16 v7,  v8
    vfilter_16 v8,  v9
    vfilter_16 v9,  v10
    vfilter_16 v10, v11
    vfilter_16 v11, v12
    vfilter_16 v12, v13
    vfilter_16 v13, v14
    vfilter_16 v14, v15
    vfilter_16 v15, v16

compute_sum_sse_8x16_b:
    vspltish v18, 0             ;# sum
    vspltish v19, 0             ;# sse
    vspltish v23, 0             ;# unpack
    li      r10, 16

    vmrghb  v0, v0,  v1
    vmrghb  v1, v2,  v3
    vmrghb  v2, v4,  v5
    vmrghb  v3, v6,  v7
    vmrghb  v4, v8,  v9
    vmrghb  v5, v10, v11
    vmrghb  v6, v12, v13
    vmrghb  v7, v14, v15

    load_and_align_16 v8,  r7, r8, 1
    load_and_align_16 v9,  r7, r8, 1
    load_and_align_16 v10, r7, r8, 1
    load_and_align_16 v11, r7, r8, 1
    load_and_align_16 v12, r7, r8, 1
    load_and_align_16 v13, r7, r8, 1
    load_and_align_16 v14, r7, r8, 1
    load_and_align_16 v15, r7, r8, 1

    vmrghb  v8,  v8,  v9
    vmrghb  v9,  v10, v11
    vmrghb  v10, v12, v13
    vmrghb  v11, v14, v15

    compute_sum_sse v0, v8,  v18, v19, v20, v21, v23
    compute_sum_sse v1, v9,  v18, v19, v20, v21, v23
    compute_sum_sse v2, v10, v18, v19, v20, v21, v23
    compute_sum_sse v3, v11, v18, v19, v20, v21, v23

    load_and_align_16 v8,  r7, r8, 1
    load_and_align_16 v9,  r7, r8, 1
    load_and_align_16 v10, r7, r8, 1
    load_and_align_16 v11, r7, r8, 1
    load_and_align_16 v12, r7, r8, 1
    load_and_align_16 v13, r7, r8, 1
    load_and_align_16 v14, r7, r8, 1
    load_and_align_16 v15, r7, r8, 0

    vmrghb  v8,  v8,  v9
    vmrghb  v9,  v10, v11
    vmrghb  v10, v12, v13
    vmrghb  v11, v14, v15

    compute_sum_sse v4, v8,  v18, v19, v20, v21, v23
    compute_sum_sse v5, v9,  v18, v19, v20, v21, v23
    compute_sum_sse v6, v10, v18, v19, v20, v21, v23
    compute_sum_sse v7, v11, v18, v19, v20, v21, v23

    variance_final v18, v19, v23, 7

    addi    r1, r1, 32          ;# recover stack
    mtspr   256, r11            ;# reset old VRSAVE
    blr

;# Filters a horizontal line
;# expects:
;#  r3  src_ptr
;#  r4  pitch
;#  r10 16
;#  r12 32
;#  v17 perm intput
;#  v18 rounding
;#  v19 shift
;#  v20 filter taps
;#  v21 tmp
;#  v22 tmp
;#  v23 tmp
;#  v24 tmp
;#  v25 tmp
;#  v26 tmp
;#  v27 tmp
;#  v28 perm output
;#
.macro hfilter_16 V, increment_counter

    lvsl    v17,  0, r3         ;# permutate value for alignment

    ;# input to filter is 21 bytes wide, output is 16 bytes.
    ;#  input will can span three vectors if not aligned correctly.
    lvx     v21,   0, r3
    lvx     v22, r10, r3
    lvx     v23, r12, r3

.if \increment_counter
    add     r3, r3, r4
.endif
    vperm   v21, v21, v22, v17
    vperm   v22, v22, v23, v17  ;# v8 v9 = 21 input pixels left-justified

    ;# set 0
    vmsummbm v24, v20, v21, v18 ;# taps times elements

    ;# set 1
    vsldoi  v23, v21, v22, 1
    vmsummbm v25, v20, v23, v18

    ;# set 2
    vsldoi  v23, v21, v22, 2
    vmsummbm v26, v20, v23, v18

    ;# set 3
    vsldoi  v23, v21, v22, 3
    vmsummbm v27, v20, v23, v18

    vpkswus v24, v24, v25       ;# v24 = 0 4 8 C 1 5 9 D (16-bit)
    vpkswus v25, v26, v27       ;# v25 = 2 6 A E 3 7 B F

    vsrh    v24, v24, v19       ;# divide v0, v1 by 128
    vsrh    v25, v25, v19

    vpkuhus \V, v24, v25        ;# \V = scrambled 8-bit result
    vperm   \V, \V, v0, v28     ;# \V = correctly-ordered result
.endm

    .align 2
;# r3 unsigned char  *src_ptr
;# r4 int  src_pixels_per_line
;# r5 int  xoffset
;# r6 int  yoffset
;# r7 unsigned char *dst_ptr
;# r8 int dst_pixels_per_line
;# r9 unsigned int *sse
;#
;# r3 return value
vp8_sub_pixel_variance16x8_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    ori     r12, r12, 0xfff8
    mtspr   256, r12            ;# set VRSAVE

    stwu    r1, -32(r1)         ;# create space on the stack

    HProlog second_pass_16x8_pre_copy_b

    hfilter_16 v0, 1
    hfilter_16 v1, 1
    hfilter_16 v2, 1
    hfilter_16 v3, 1
    hfilter_16 v4, 1
    hfilter_16 v5, 1
    hfilter_16 v6, 1
    hfilter_16 v7, 1

    ;# Finished filtering main horizontal block.  If there is no
    ;#  vertical filtering, jump to storing the data.  Otherwise
    ;#  load up and filter the additional line that is needed
    ;#  for the vertical filter.
    beq     compute_sum_sse_16x8_b

    hfilter_16 v8, 0

    b   second_pass_16x8_b

second_pass_16x8_pre_copy_b:
    slwi.   r6, r6, 5           ;# index into vertical filter array

    load_and_align_16  v0,  r3, r4, 1
    load_and_align_16  v1,  r3, r4, 1
    load_and_align_16  v2,  r3, r4, 1
    load_and_align_16  v3,  r3, r4, 1
    load_and_align_16  v4,  r3, r4, 1
    load_and_align_16  v5,  r3, r4, 1
    load_and_align_16  v6,  r3, r4, 1
    load_and_align_16  v7,  r3, r4, 1
    load_and_align_16  v8,  r3, r4, 1

    beq     compute_sum_sse_16x8_b

second_pass_16x8_b:
    vspltish v20, 8
    vspltish v18, 3
    vslh    v18, v20, v18   ;# 0x0040 0040 0040 0040 0040 0040 0040 0040

    load_vfilter v20, v21

    vfilter_16 v0,  v1
    vfilter_16 v1,  v2
    vfilter_16 v2,  v3
    vfilter_16 v3,  v4
    vfilter_16 v4,  v5
    vfilter_16 v5,  v6
    vfilter_16 v6,  v7
    vfilter_16 v7,  v8

compute_sum_sse_16x8_b:
    vspltish v18, 0             ;# sum
    vspltish v19, 0             ;# sse
    vspltish v23, 0             ;# unpack
    li      r10, 16

    compute_sum_sse_16 v0, 1
    compute_sum_sse_16 v1, 1
    compute_sum_sse_16 v2, 1
    compute_sum_sse_16 v3, 1
    compute_sum_sse_16 v4, 1
    compute_sum_sse_16 v5, 1
    compute_sum_sse_16 v6, 1
    compute_sum_sse_16 v7, 0

    variance_final v18, v19, v23, 7

    addi    r1, r1, 32          ;# recover stack

    mtspr   256, r11            ;# reset old VRSAVE

    blr

    .align 2
;# r3 unsigned char  *src_ptr
;# r4 int  src_pixels_per_line
;# r5 int  xoffset
;# r6 int  yoffset
;# r7 unsigned char *dst_ptr
;# r8 int dst_pixels_per_line
;# r9 unsigned int *sse
;#
;# r3 return value
vp8_sub_pixel_variance16x16_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffff
    ori     r12, r12, 0xfff8
    mtspr   256, r12            ;# set VRSAVE

    stwu    r1, -32(r1)         ;# create space on the stack

    HProlog second_pass_16x16_pre_copy_b

    hfilter_16 v0,  1
    hfilter_16 v1,  1
    hfilter_16 v2,  1
    hfilter_16 v3,  1
    hfilter_16 v4,  1
    hfilter_16 v5,  1
    hfilter_16 v6,  1
    hfilter_16 v7,  1
    hfilter_16 v8,  1
    hfilter_16 v9,  1
    hfilter_16 v10, 1
    hfilter_16 v11, 1
    hfilter_16 v12, 1
    hfilter_16 v13, 1
    hfilter_16 v14, 1
    hfilter_16 v15, 1

    ;# Finished filtering main horizontal block.  If there is no
    ;#  vertical filtering, jump to storing the data.  Otherwise
    ;#  load up and filter the additional line that is needed
    ;#  for the vertical filter.
    beq     compute_sum_sse_16x16_b

    hfilter_16 v16, 0

    b   second_pass_16x16_b

second_pass_16x16_pre_copy_b:
    slwi.   r6, r6, 5           ;# index into vertical filter array

    load_and_align_16  v0,  r3, r4, 1
    load_and_align_16  v1,  r3, r4, 1
    load_and_align_16  v2,  r3, r4, 1
    load_and_align_16  v3,  r3, r4, 1
    load_and_align_16  v4,  r3, r4, 1
    load_and_align_16  v5,  r3, r4, 1
    load_and_align_16  v6,  r3, r4, 1
    load_and_align_16  v7,  r3, r4, 1
    load_and_align_16  v8,  r3, r4, 1
    load_and_align_16  v9,  r3, r4, 1
    load_and_align_16  v10, r3, r4, 1
    load_and_align_16  v11, r3, r4, 1
    load_and_align_16  v12, r3, r4, 1
    load_and_align_16  v13, r3, r4, 1
    load_and_align_16  v14, r3, r4, 1
    load_and_align_16  v15, r3, r4, 1
    load_and_align_16  v16, r3, r4, 0

    beq     compute_sum_sse_16x16_b

second_pass_16x16_b:
    vspltish v20, 8
    vspltish v18, 3
    vslh    v18, v20, v18   ;# 0x0040 0040 0040 0040 0040 0040 0040 0040

    load_vfilter v20, v21

    vfilter_16 v0,  v1
    vfilter_16 v1,  v2
    vfilter_16 v2,  v3
    vfilter_16 v3,  v4
    vfilter_16 v4,  v5
    vfilter_16 v5,  v6
    vfilter_16 v6,  v7
    vfilter_16 v7,  v8
    vfilter_16 v8,  v9
    vfilter_16 v9,  v10
    vfilter_16 v10, v11
    vfilter_16 v11, v12
    vfilter_16 v12, v13
    vfilter_16 v13, v14
    vfilter_16 v14, v15
    vfilter_16 v15, v16

compute_sum_sse_16x16_b:
    vspltish v18, 0             ;# sum
    vspltish v19, 0             ;# sse
    vspltish v23, 0             ;# unpack
    li      r10, 16

    compute_sum_sse_16 v0,  1
    compute_sum_sse_16 v1,  1
    compute_sum_sse_16 v2,  1
    compute_sum_sse_16 v3,  1
    compute_sum_sse_16 v4,  1
    compute_sum_sse_16 v5,  1
    compute_sum_sse_16 v6,  1
    compute_sum_sse_16 v7,  1
    compute_sum_sse_16 v8,  1
    compute_sum_sse_16 v9,  1
    compute_sum_sse_16 v10, 1
    compute_sum_sse_16 v11, 1
    compute_sum_sse_16 v12, 1
    compute_sum_sse_16 v13, 1
    compute_sum_sse_16 v14, 1
    compute_sum_sse_16 v15, 0

    variance_final v18, v19, v23, 8

    addi    r1, r1, 32          ;# recover stack

    mtspr   256, r11            ;# reset old VRSAVE

    blr

    .data

    .align 4
hfilter_b:
    .byte   128,  0,  0,  0,128,  0,  0,  0,128,  0,  0,  0,128,  0,  0,  0
    .byte   112, 16,  0,  0,112, 16,  0,  0,112, 16,  0,  0,112, 16,  0,  0
    .byte    96, 32,  0,  0, 96, 32,  0,  0, 96, 32,  0,  0, 96, 32,  0,  0
    .byte    80, 48,  0,  0, 80, 48,  0,  0, 80, 48,  0,  0, 80, 48,  0,  0
    .byte    64, 64,  0,  0, 64, 64,  0,  0, 64, 64,  0,  0, 64, 64,  0,  0
    .byte    48, 80,  0,  0, 48, 80,  0,  0, 48, 80,  0,  0, 48, 80,  0,  0
    .byte    32, 96,  0,  0, 32, 96,  0,  0, 32, 96,  0,  0, 32, 96,  0,  0
    .byte    16,112,  0,  0, 16,112,  0,  0, 16,112,  0,  0, 16,112,  0,  0

    .align 4
vfilter_b:
    .byte   128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
    .byte     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    .byte   112,112,112,112,112,112,112,112,112,112,112,112,112,112,112,112
    .byte    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
    .byte    96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96
    .byte    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32
    .byte    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80
    .byte    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48
    .byte    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    .byte    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    .byte    48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48
    .byte    80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80
    .byte    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32
    .byte    96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96
    .byte    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
    .byte   112,112,112,112,112,112,112,112,112,112,112,112,112,112,112,112

    .align 4
b_hperm_b:
    .byte     0,  4,  8, 12,  1,  5,  9, 13,  2,  6, 10, 14,  3,  7, 11, 15

    .align 4
b_0123_b:
    .byte     0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6

    .align 4
b_4567_b:
    .byte     4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10

b_hilo_b:
    .byte     0,  1,  2,  3,  4,  5,  6,  7, 16, 17, 18, 19, 20, 21, 22, 23
