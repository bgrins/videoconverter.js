;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl copy_mem16x16_ppc

;# r3 unsigned char *src
;# r4 int src_stride
;# r5 unsigned char *dst
;# r6 int dst_stride

;# Make the assumption that input will not be aligned,
;#  but the output will be.  So two reads and a perm
;#  for the input, but only one store for the output.
copy_mem16x16_ppc:
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xe000
    mtspr   256, r12            ;# set VRSAVE

    li      r10, 16
    mtctr   r10

cp_16x16_loop:
    lvsl    v0,  0, r3          ;# permutate value for alignment

    lvx     v1,   0, r3
    lvx     v2, r10, r3

    vperm   v1, v1, v2, v0

    stvx    v1,  0, r5

    add     r3, r3, r4          ;# increment source pointer
    add     r5, r5, r6          ;# increment destination pointer

    bdnz    cp_16x16_loop

    mtspr   256, r11            ;# reset old VRSAVE

    blr
