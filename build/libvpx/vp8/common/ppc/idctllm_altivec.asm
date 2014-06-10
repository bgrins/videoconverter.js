;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl short_idct4x4llm_ppc

.macro load_c V, LABEL, OFF, R0, R1
    lis     \R0, \LABEL@ha
    la      \R1, \LABEL@l(\R0)
    lvx     \V, \OFF, \R1
.endm

;# r3 short *input
;# r4 short *output
;# r5 int pitch
    .align 2
short_idct4x4llm_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xfff8
    mtspr   256, r12            ;# set VRSAVE

    load_c v8, sinpi8sqrt2, 0, r9, r10
    load_c v9, cospi8sqrt2minus1, 0, r9, r10
    load_c v10, hi_hi, 0, r9, r10
    load_c v11, lo_lo, 0, r9, r10
    load_c v12, shift_16, 0, r9, r10

    li      r10,  16
    lvx     v0,   0, r3         ;# input ip[0], ip[ 4]
    lvx     v1, r10, r3         ;# input ip[8], ip[12]

    ;# first pass
    vupkhsh v2, v0
    vupkhsh v3, v1
    vaddsws v6, v2, v3          ;# a1 = ip[0]+ip[8]
    vsubsws v7, v2, v3          ;# b1 = ip[0]-ip[8]

    vupklsh v0, v0
    vmulosh v4, v0, v8
    vsraw   v4, v4, v12
    vaddsws v4, v4, v0          ;# ip[ 4] * sin(pi/8) * sqrt(2)

    vupklsh v1, v1
    vmulosh v5, v1, v9
    vsraw   v5, v5, v12         ;# ip[12] * cos(pi/8) * sqrt(2)
    vaddsws v5, v5, v1

    vsubsws v4, v4, v5          ;# c1

    vmulosh v3, v1, v8
    vsraw   v3, v3, v12
    vaddsws v3, v3, v1          ;# ip[12] * sin(pi/8) * sqrt(2)

    vmulosh v5, v0, v9
    vsraw   v5, v5, v12         ;# ip[ 4] * cos(pi/8) * sqrt(2)
    vaddsws v5, v5, v0

    vaddsws v3, v3, v5          ;# d1

    vaddsws v0, v6, v3          ;# a1 + d1
    vsubsws v3, v6, v3          ;# a1 - d1

    vaddsws v1, v7, v4          ;# b1 + c1
    vsubsws v2, v7, v4          ;# b1 - c1

    ;# transpose input
    vmrghw  v4, v0, v1          ;# a0 b0 a1 b1
    vmrghw  v5, v2, v3          ;# c0 d0 c1 d1

    vmrglw  v6, v0, v1          ;# a2 b2 a3 b3
    vmrglw  v7, v2, v3          ;# c2 d2 c3 d3

    vperm   v0, v4, v5, v10     ;# a0 b0 c0 d0
    vperm   v1, v4, v5, v11     ;# a1 b1 c1 d1

    vperm   v2, v6, v7, v10     ;# a2 b2 c2 d2
    vperm   v3, v6, v7, v11     ;# a3 b3 c3 d3

    ;# second pass
    vaddsws v6, v0, v2          ;# a1 = ip[0]+ip[8]
    vsubsws v7, v0, v2          ;# b1 = ip[0]-ip[8]

    vmulosh v4, v1, v8
    vsraw   v4, v4, v12
    vaddsws v4, v4, v1          ;# ip[ 4] * sin(pi/8) * sqrt(2)

    vmulosh v5, v3, v9
    vsraw   v5, v5, v12         ;# ip[12] * cos(pi/8) * sqrt(2)
    vaddsws v5, v5, v3

    vsubsws v4, v4, v5          ;# c1

    vmulosh v2, v3, v8
    vsraw   v2, v2, v12
    vaddsws v2, v2, v3          ;# ip[12] * sin(pi/8) * sqrt(2)

    vmulosh v5, v1, v9
    vsraw   v5, v5, v12         ;# ip[ 4] * cos(pi/8) * sqrt(2)
    vaddsws v5, v5, v1

    vaddsws v3, v2, v5          ;# d1

    vaddsws v0, v6, v3          ;# a1 + d1
    vsubsws v3, v6, v3          ;# a1 - d1

    vaddsws v1, v7, v4          ;# b1 + c1
    vsubsws v2, v7, v4          ;# b1 - c1

    vspltish v6, 4
    vspltish v7, 3

    vpkswss v0, v0, v1
    vpkswss v1, v2, v3

    vaddshs v0, v0, v6
    vaddshs v1, v1, v6

    vsrah   v0, v0, v7
    vsrah   v1, v1, v7

    ;# transpose output
    vmrghh  v2, v0, v1          ;# a0 c0 a1 c1 a2 c2 a3 c3
    vmrglh  v3, v0, v1          ;# b0 d0 b1 d1 b2 d2 b3 d3

    vmrghh  v0, v2, v3          ;# a0 b0 c0 d0 a1 b1 c1 d1
    vmrglh  v1, v2, v3          ;# a2 b2 c2 d2 a3 b3 c3 d3

    stwu    r1,-416(r1)         ;# create space on the stack

    stvx    v0,  0, r1
    lwz     r6, 0(r1)
    stw     r6, 0(r4)
    lwz     r6, 4(r1)
    stw     r6, 4(r4)

    add     r4, r4, r5

    lwz     r6,  8(r1)
    stw     r6,  0(r4)
    lwz     r6, 12(r1)
    stw     r6,  4(r4)

    add     r4, r4, r5

    stvx    v1,  0, r1
    lwz     r6, 0(r1)
    stw     r6, 0(r4)
    lwz     r6, 4(r1)
    stw     r6, 4(r4)

    add     r4, r4, r5

    lwz     r6,  8(r1)
    stw     r6,  0(r4)
    lwz     r6, 12(r1)
    stw     r6,  4(r4)

    addi    r1, r1, 416         ;# recover stack

    mtspr   256, r11            ;# reset old VRSAVE

    blr

    .align 4
sinpi8sqrt2:
    .short  35468, 35468, 35468, 35468, 35468, 35468, 35468, 35468

    .align 4
cospi8sqrt2minus1:
    .short  20091, 20091, 20091, 20091, 20091, 20091, 20091, 20091

    .align 4
shift_16:
    .long      16,    16,    16,    16

    .align 4
hi_hi:
    .byte     0,  1,  2,  3,  4,  5,  6,  7, 16, 17, 18, 19, 20, 21, 22, 23

    .align 4
lo_lo:
    .byte     8,  9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31
