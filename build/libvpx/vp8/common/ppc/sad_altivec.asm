;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl vp8_sad16x16_ppc
    .globl vp8_sad16x8_ppc
    .globl vp8_sad8x16_ppc
    .globl vp8_sad8x8_ppc
    .globl vp8_sad4x4_ppc

.macro load_aligned_16 V R O
    lvsl    v3,  0, \R          ;# permutate value for alignment

    lvx     v1,  0, \R
    lvx     v2, \O, \R

    vperm   \V, v1, v2, v3
.endm

.macro prologue
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xffc0
    mtspr   256, r12            ;# set VRSAVE

    stwu    r1, -32(r1)         ;# create space on the stack

    li      r10, 16             ;# load offset and loop counter

    vspltisw v8, 0              ;# zero out total to start
.endm

.macro epilogue
    addi    r1, r1, 32          ;# recover stack

    mtspr   256, r11            ;# reset old VRSAVE
.endm

.macro SAD_16
    ;# v6 = abs (v4 - v5)
    vsububs v6, v4, v5
    vsububs v7, v5, v4
    vor     v6, v6, v7

    ;# v8 += abs (v4 - v5)
    vsum4ubs v8, v6, v8
.endm

.macro sad_16_loop loop_label
    lvsl    v3,  0, r5          ;# only needs to be done once per block

    ;# preload a line of data before getting into the loop
    lvx     v4, 0, r3
    lvx     v1,  0, r5
    lvx     v2, r10, r5

    add     r5, r5, r6
    add     r3, r3, r4

    vperm   v5, v1, v2, v3

    .align 4
\loop_label:
    ;# compute difference on first row
    vsububs v6, v4, v5
    vsububs v7, v5, v4

    ;# load up next set of data
    lvx     v9, 0, r3
    lvx     v1,  0, r5
    lvx     v2, r10, r5

    ;# perform abs() of difference
    vor     v6, v6, v7
    add     r3, r3, r4

    ;# add to the running tally
    vsum4ubs v8, v6, v8

    ;# now onto the next line
    vperm   v5, v1, v2, v3
    add     r5, r5, r6
    lvx     v4, 0, r3

    ;# compute difference on second row
    vsububs v6, v9, v5
    lvx     v1,  0, r5
    vsububs v7, v5, v9
    lvx     v2, r10, r5
    vor     v6, v6, v7
    add     r3, r3, r4
    vsum4ubs v8, v6, v8
    vperm   v5, v1, v2, v3
    add     r5, r5, r6

    bdnz    \loop_label

    vspltisw v7, 0

    vsumsws v8, v8, v7

    stvx    v8, 0, r1
    lwz     r3, 12(r1)
.endm

.macro sad_8_loop loop_label
    .align 4
\loop_label:
    ;# only one of the inputs should need to be aligned.
    load_aligned_16 v4, r3, r10
    load_aligned_16 v5, r5, r10

    ;# move onto the next line
    add     r3, r3, r4
    add     r5, r5, r6

    ;# only one of the inputs should need to be aligned.
    load_aligned_16 v6, r3, r10
    load_aligned_16 v7, r5, r10

    ;# move onto the next line
    add     r3, r3, r4
    add     r5, r5, r6

    vmrghb  v4, v4, v6
    vmrghb  v5, v5, v7

    SAD_16

    bdnz    \loop_label

    vspltisw v7, 0

    vsumsws v8, v8, v7

    stvx    v8, 0, r1
    lwz     r3, 12(r1)
.endm

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  src_stride
;# r5 unsigned char *ref_ptr
;# r6 int  ref_stride
;#
;# r3 return value
vp8_sad16x16_ppc:

    prologue

    li      r9, 8
    mtctr   r9

    sad_16_loop sad16x16_loop

    epilogue

    blr

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  src_stride
;# r5 unsigned char *ref_ptr
;# r6 int  ref_stride
;#
;# r3 return value
vp8_sad16x8_ppc:

    prologue

    li      r9, 4
    mtctr   r9

    sad_16_loop sad16x8_loop

    epilogue

    blr

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  src_stride
;# r5 unsigned char *ref_ptr
;# r6 int  ref_stride
;#
;# r3 return value
vp8_sad8x16_ppc:

    prologue

    li      r9, 8
    mtctr   r9

    sad_8_loop sad8x16_loop

    epilogue

    blr

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  src_stride
;# r5 unsigned char *ref_ptr
;# r6 int  ref_stride
;#
;# r3 return value
vp8_sad8x8_ppc:

    prologue

    li      r9, 4
    mtctr   r9

    sad_8_loop sad8x8_loop

    epilogue

    blr

.macro transfer_4x4 I P
    lwz     r0, 0(\I)
    add     \I, \I, \P

    lwz     r7, 0(\I)
    add     \I, \I, \P

    lwz     r8, 0(\I)
    add     \I, \I, \P

    lwz     r9, 0(\I)

    stw     r0,  0(r1)
    stw     r7,  4(r1)
    stw     r8,  8(r1)
    stw     r9, 12(r1)
.endm

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  src_stride
;# r5 unsigned char *ref_ptr
;# r6 int  ref_stride
;#
;# r3 return value
vp8_sad4x4_ppc:

    prologue

    transfer_4x4 r3, r4
    lvx     v4, 0, r1

    transfer_4x4 r5, r6
    lvx     v5, 0, r1

    vspltisw v8, 0              ;# zero out total to start

    ;# v6 = abs (v4 - v5)
    vsububs v6, v4, v5
    vsububs v7, v5, v4
    vor     v6, v6, v7

    ;# v8 += abs (v4 - v5)
    vsum4ubs v7, v6, v8
    vsumsws v7, v7, v8

    stvx    v7, 0, r1
    lwz     r3, 12(r1)

    epilogue

    blr
