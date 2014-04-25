;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl vp8_subtract_mbuv_ppc
    .globl vp8_subtract_mby_ppc

;# r3 short *diff
;# r4 unsigned char *usrc
;# r5 unsigned char *vsrc
;# r6 unsigned char *pred
;# r7 int stride
vp8_subtract_mbuv_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xf000
    mtspr   256, r12            ;# set VRSAVE

    li      r9, 256
    add     r3, r3, r9
    add     r3, r3, r9
    add     r6, r6, r9

    li      r10, 16
    li      r9,  4
    mtctr   r9

    vspltisw v0, 0

mbu_loop:
    lvsl    v5, 0, r4           ;# permutate value for alignment
    lvx     v1, 0, r4           ;# src
    lvx     v2, 0, r6           ;# pred

    add     r4, r4, r7
    addi    r6, r6, 16

    vperm   v1, v1, v0, v5

    vmrghb  v3, v0, v1          ;# unpack high src  to short
    vmrghb  v4, v0, v2          ;# unpack high pred to short

    lvsl    v5, 0, r4           ;# permutate value for alignment
    lvx     v1, 0, r4           ;# src

    add     r4, r4, r7

    vsubshs v3, v3, v4

    stvx    v3, 0, r3           ;# store out diff

    vperm   v1, v1, v0, v5

    vmrghb  v3, v0, v1          ;# unpack high src  to short
    vmrglb  v4, v0, v2          ;# unpack high pred to short

    vsubshs v3, v3, v4

    stvx    v3, r10, r3         ;# store out diff

    addi    r3, r3, 32

    bdnz    mbu_loop

    mtctr   r9

mbv_loop:
    lvsl    v5, 0, r5           ;# permutate value for alignment
    lvx     v1, 0, r5           ;# src
    lvx     v2, 0, r6           ;# pred

    add     r5, r5, r7
    addi    r6, r6, 16

    vperm   v1, v1, v0, v5

    vmrghb  v3, v0, v1          ;# unpack high src  to short
    vmrghb  v4, v0, v2          ;# unpack high pred to short

    lvsl    v5, 0, r5           ;# permutate value for alignment
    lvx     v1, 0, r5           ;# src

    add     r5, r5, r7

    vsubshs v3, v3, v4

    stvx    v3, 0, r3           ;# store out diff

    vperm   v1, v1, v0, v5

    vmrghb  v3, v0, v1          ;# unpack high src  to short
    vmrglb  v4, v0, v2          ;# unpack high pred to short

    vsubshs v3, v3, v4

    stvx    v3, r10, r3         ;# store out diff

    addi    r3, r3, 32

    bdnz    mbv_loop

    mtspr   256, r11            ;# reset old VRSAVE

    blr

;# r3 short *diff
;# r4 unsigned char *src
;# r5 unsigned char *pred
;# r6 int stride
vp8_subtract_mby_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xf800
    mtspr   256, r12            ;# set VRSAVE

    li      r10, 16
    mtctr   r10

    vspltisw v0, 0

mby_loop:
    lvx     v1, 0, r4           ;# src
    lvx     v2, 0, r5           ;# pred

    add     r4, r4, r6
    addi    r5, r5, 16

    vmrghb  v3, v0, v1          ;# unpack high src  to short
    vmrghb  v4, v0, v2          ;# unpack high pred to short

    vsubshs v3, v3, v4

    stvx    v3, 0, r3           ;# store out diff

    vmrglb  v3, v0, v1          ;# unpack low src  to short
    vmrglb  v4, v0, v2          ;# unpack low pred to short

    vsubshs v3, v3, v4

    stvx    v3, r10, r3         ;# store out diff

    addi    r3, r3, 32

    bdnz    mby_loop

    mtspr   256, r11            ;# reset old VRSAVE

    blr
