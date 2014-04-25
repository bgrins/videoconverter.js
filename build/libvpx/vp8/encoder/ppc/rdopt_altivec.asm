;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl vp8_block_error_ppc

    .align 2
;# r3 short *Coeff
;# r4 short *dqcoeff
vp8_block_error_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xf800
    mtspr   256, r12            ;# set VRSAVE

    stwu    r1,-32(r1)          ;# create space on the stack

    stw     r5, 12(r1)          ;# tranfer dc to vector register

    lvx     v0, 0, r3           ;# Coeff
    lvx     v1, 0, r4           ;# dqcoeff

    li      r10, 16

    vspltisw v3, 0

    vsubshs v0, v0, v1

    vmsumshm v2, v0, v0, v3     ;# multiply differences

    lvx     v0, r10, r3         ;# Coeff
    lvx     v1, r10, r4         ;# dqcoeff

    vsubshs v0, v0, v1

    vmsumshm v1, v0, v0, v2     ;# multiply differences
    vsumsws v1, v1, v3          ;# sum up

    stvx    v1, 0, r1
    lwz     r3, 12(r1)          ;# return value

    addi    r1, r1, 32          ;# recover stack
    mtspr   256, r11            ;# reset old VRSAVE

    blr
