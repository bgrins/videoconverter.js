;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl vp8_get8x8var_ppc
    .globl vp8_get16x16var_ppc
    .globl vp8_mse16x16_ppc
    .globl vp8_variance16x16_ppc
    .globl vp8_variance16x8_ppc
    .globl vp8_variance8x16_ppc
    .globl vp8_variance8x8_ppc
    .globl vp8_variance4x4_ppc

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

    vspltisw v7, 0              ;# zero for merging
    vspltisw v8, 0              ;# zero out total to start
    vspltisw v9, 0              ;# zero out total for dif^2
.endm

.macro epilogue
    addi    r1, r1, 32          ;# recover stack

    mtspr   256, r11            ;# reset old VRSAVE
.endm

.macro compute_sum_sse
    ;# Compute sum first.  Unpack to so signed subract
    ;#  can be used.  Only have a half word signed
    ;#  subract.  Do high, then low.
    vmrghb  v2, v7, v4
    vmrghb  v3, v7, v5
    vsubshs v2, v2, v3
    vsum4shs v8, v2, v8

    vmrglb  v2, v7, v4
    vmrglb  v3, v7, v5
    vsubshs v2, v2, v3
    vsum4shs v8, v2, v8

    ;# Now compute sse.
    vsububs v2, v4, v5
    vsububs v3, v5, v4
    vor     v2, v2, v3

    vmsumubm v9, v2, v2, v9
.endm

.macro variance_16 DS loop_label store_sum
\loop_label:
    ;# only one of the inputs should need to be aligned.
    load_aligned_16 v4, r3, r10
    load_aligned_16 v5, r5, r10

    ;# move onto the next line
    add     r3, r3, r4
    add     r5, r5, r6

    compute_sum_sse

    bdnz    \loop_label

    vsumsws v8, v8, v7
    vsumsws v9, v9, v7

    stvx    v8, 0, r1
    lwz     r3, 12(r1)

    stvx    v9, 0, r1
    lwz     r4, 12(r1)

.if \store_sum
    stw     r3, 0(r8)           ;# sum
.endif
    stw     r4, 0(r7)           ;# sse

    mullw   r3, r3, r3          ;# sum*sum
    srlwi   r3, r3, \DS         ;# (sum*sum) >> DS
    subf    r3, r3, r4          ;# sse - ((sum*sum) >> DS)
.endm

.macro variance_8 DS loop_label store_sum
\loop_label:
    ;# only one of the inputs should need to be aligned.
    load_aligned_16 v4, r3, r10
    load_aligned_16 v5, r5, r10

    ;# move onto the next line
    add     r3, r3, r4
    add     r5, r5, r6

    ;# only one of the inputs should need to be aligned.
    load_aligned_16 v6, r3, r10
    load_aligned_16 v0, r5, r10

    ;# move onto the next line
    add     r3, r3, r4
    add     r5, r5, r6

    vmrghb  v4, v4, v6
    vmrghb  v5, v5, v0

    compute_sum_sse

    bdnz    \loop_label

    vsumsws v8, v8, v7
    vsumsws v9, v9, v7

    stvx    v8, 0, r1
    lwz     r3, 12(r1)

    stvx    v9, 0, r1
    lwz     r4, 12(r1)

.if \store_sum
    stw     r3, 0(r8)           ;# sum
.endif
    stw     r4, 0(r7)           ;# sse

    mullw   r3, r3, r3          ;# sum*sum
    srlwi   r3, r3, \DS         ;# (sum*sum) >> 8
    subf    r3, r3, r4          ;# sse - ((sum*sum) >> 8)
.endm

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  source_stride
;# r5 unsigned char *ref_ptr
;# r6 int  recon_stride
;# r7 unsigned int *SSE
;# r8 int *Sum
;#
;# r3 return value
vp8_get8x8var_ppc:

    prologue

    li      r9, 4
    mtctr   r9

    variance_8 6, get8x8var_loop, 1

    epilogue

    blr

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  source_stride
;# r5 unsigned char *ref_ptr
;# r6 int  recon_stride
;# r7 unsigned int *SSE
;# r8 int *Sum
;#
;# r3 return value
vp8_get16x16var_ppc:

    prologue

    mtctr   r10

    variance_16 8, get16x16var_loop, 1

    epilogue

    blr

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  source_stride
;# r5 unsigned char *ref_ptr
;# r6 int  recon_stride
;# r7 unsigned int *sse
;#
;# r 3 return value
vp8_mse16x16_ppc:
    prologue

    mtctr   r10

mse16x16_loop:
    ;# only one of the inputs should need to be aligned.
    load_aligned_16 v4, r3, r10
    load_aligned_16 v5, r5, r10

    ;# move onto the next line
    add     r3, r3, r4
    add     r5, r5, r6

    ;# Now compute sse.
    vsububs v2, v4, v5
    vsububs v3, v5, v4
    vor     v2, v2, v3

    vmsumubm v9, v2, v2, v9

    bdnz    mse16x16_loop

    vsumsws v9, v9, v7

    stvx    v9, 0, r1
    lwz     r3, 12(r1)

    stvx    v9, 0, r1
    lwz     r3, 12(r1)

    stw     r3, 0(r7)           ;# sse

    epilogue

    blr

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  source_stride
;# r5 unsigned char *ref_ptr
;# r6 int  recon_stride
;# r7 unsigned int *sse
;#
;# r3 return value
vp8_variance16x16_ppc:

    prologue

    mtctr   r10

    variance_16 8, variance16x16_loop, 0

    epilogue

    blr

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  source_stride
;# r5 unsigned char *ref_ptr
;# r6 int  recon_stride
;# r7 unsigned int *sse
;#
;# r3 return value
vp8_variance16x8_ppc:

    prologue

    li      r9, 8
    mtctr   r9

    variance_16 7, variance16x8_loop, 0

    epilogue

    blr

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  source_stride
;# r5 unsigned char *ref_ptr
;# r6 int  recon_stride
;# r7 unsigned int *sse
;#
;# r3 return value
vp8_variance8x16_ppc:

    prologue

    li      r9, 8
    mtctr   r9

    variance_8 7, variance8x16_loop, 0

    epilogue

    blr

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  source_stride
;# r5 unsigned char *ref_ptr
;# r6 int  recon_stride
;# r7 unsigned int *sse
;#
;# r3 return value
vp8_variance8x8_ppc:

    prologue

    li      r9, 4
    mtctr   r9

    variance_8 6, variance8x8_loop, 0

    epilogue

    blr

.macro transfer_4x4 I P
    lwz     r0, 0(\I)
    add     \I, \I, \P

    lwz     r10,0(\I)
    add     \I, \I, \P

    lwz     r8, 0(\I)
    add     \I, \I, \P

    lwz     r9, 0(\I)

    stw     r0,  0(r1)
    stw     r10, 4(r1)
    stw     r8,  8(r1)
    stw     r9, 12(r1)
.endm

    .align 2
;# r3 unsigned char *src_ptr
;# r4 int  source_stride
;# r5 unsigned char *ref_ptr
;# r6 int  recon_stride
;# r7 unsigned int *sse
;#
;# r3 return value
vp8_variance4x4_ppc:

    prologue

    transfer_4x4 r3, r4
    lvx     v4, 0, r1

    transfer_4x4 r5, r6
    lvx     v5, 0, r1

    compute_sum_sse

    vsumsws v8, v8, v7
    vsumsws v9, v9, v7

    stvx    v8, 0, r1
    lwz     r3, 12(r1)

    stvx    v9, 0, r1
    lwz     r4, 12(r1)

    stw     r4, 0(r7)           ;# sse

    mullw   r3, r3, r3          ;# sum*sum
    srlwi   r3, r3, 4           ;# (sum*sum) >> 4
    subf    r3, r3, r4          ;# sse - ((sum*sum) >> 4)

    epilogue

    blr
