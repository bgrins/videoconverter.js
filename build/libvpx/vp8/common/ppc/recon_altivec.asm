;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl recon4b_ppc
    .globl recon2b_ppc
    .globl recon_b_ppc

.macro row_of16 Diff Pred Dst Stride
    lvx     v1,  0, \Pred           ;# v1 = pred = p0..p15
    addi    \Pred, \Pred, 16        ;# next pred
    vmrghb  v2, v0, v1              ;# v2 = 16-bit p0..p7
    lvx     v3,  0, \Diff           ;# v3 = d0..d7
    vaddshs v2, v2, v3              ;# v2 = r0..r7
    vmrglb  v1, v0, v1              ;# v1 = 16-bit p8..p15
    lvx     v3, r8, \Diff           ;# v3 = d8..d15
    addi    \Diff, \Diff, 32        ;# next diff
    vaddshs v3, v3, v1              ;# v3 = r8..r15
    vpkshus v2, v2, v3              ;# v2 = 8-bit r0..r15
    stvx    v2,  0, \Dst            ;# to dst
    add     \Dst, \Dst, \Stride     ;# next dst
.endm

    .text
    .align 2
;#  r3 = short *diff_ptr,
;#  r4 = unsigned char *pred_ptr,
;#  r5 = unsigned char *dst_ptr,
;#  r6 = int stride
recon4b_ppc:
    mfspr   r0, 256                     ;# get old VRSAVE
    stw     r0, -8(r1)                  ;# save old VRSAVE to stack
    oris    r0, r0, 0xf000
    mtspr   256,r0                      ;# set VRSAVE

    vxor    v0, v0, v0
    li      r8, 16

    row_of16 r3, r4, r5, r6
    row_of16 r3, r4, r5, r6
    row_of16 r3, r4, r5, r6
    row_of16 r3, r4, r5, r6

    lwz     r12, -8(r1)                 ;# restore old VRSAVE from stack
    mtspr   256, r12                    ;# reset old VRSAVE

    blr

.macro two_rows_of8 Diff Pred Dst Stride write_first_four_pels
    lvx     v1,  0, \Pred       ;# v1 = pred = p0..p15
    vmrghb  v2, v0, v1          ;# v2 = 16-bit p0..p7
    lvx     v3,  0, \Diff       ;# v3 = d0..d7
    vaddshs v2, v2, v3          ;# v2 = r0..r7
    vmrglb  v1, v0, v1          ;# v1 = 16-bit p8..p15
    lvx     v3, r8, \Diff       ;# v2 = d8..d15
    vaddshs v3, v3, v1          ;# v3 = r8..r15
    vpkshus v2, v2, v3          ;# v3 = 8-bit r0..r15
    stvx    v2,  0, r10         ;# 2 rows to dst from buf
    lwz     r0, 0(r10)
.if \write_first_four_pels
    stw     r0, 0(\Dst)
    .else
    stwux   r0, \Dst, \Stride
.endif
    lwz     r0, 4(r10)
    stw     r0, 4(\Dst)
    lwz     r0, 8(r10)
    stwux   r0, \Dst, \Stride       ;# advance dst to next row
    lwz     r0, 12(r10)
    stw     r0, 4(\Dst)
.endm

    .align 2
;#  r3 = short *diff_ptr,
;#  r4 = unsigned char *pred_ptr,
;#  r5 = unsigned char *dst_ptr,
;#  r6 = int stride

recon2b_ppc:
    mfspr   r0, 256                     ;# get old VRSAVE
    stw     r0, -8(r1)                  ;# save old VRSAVE to stack
    oris    r0, r0, 0xf000
    mtspr   256,r0                      ;# set VRSAVE

    vxor    v0, v0, v0
    li      r8, 16

    la      r10, -48(r1)                ;# buf

    two_rows_of8 r3, r4, r5, r6, 1

    addi    r4, r4, 16;                 ;# next pred
    addi    r3, r3, 32;                 ;# next diff

    two_rows_of8 r3, r4, r5, r6, 0

    lwz     r12, -8(r1)                 ;# restore old VRSAVE from stack
    mtspr   256, r12                    ;# reset old VRSAVE

    blr

.macro get_two_diff_rows
    stw     r0, 0(r10)
    lwz     r0, 4(r3)
    stw     r0, 4(r10)
    lwzu    r0, 32(r3)
    stw     r0, 8(r10)
    lwz     r0, 4(r3)
    stw     r0, 12(r10)
    lvx     v3, 0, r10
.endm

    .align 2
;#  r3 = short *diff_ptr,
;#  r4 = unsigned char *pred_ptr,
;#  r5 = unsigned char *dst_ptr,
;#  r6 = int stride
recon_b_ppc:
    mfspr   r0, 256                     ;# get old VRSAVE
    stw     r0, -8(r1)                  ;# save old VRSAVE to stack
    oris    r0, r0, 0xf000
    mtspr   256,r0                      ;# set VRSAVE

    vxor    v0, v0, v0

    la      r10, -48(r1)    ;# buf

    lwz     r0, 0(r4)
    stw     r0, 0(r10)
    lwz     r0, 16(r4)
    stw     r0, 4(r10)
    lwz     r0, 32(r4)
    stw     r0, 8(r10)
    lwz     r0, 48(r4)
    stw     r0, 12(r10)

    lvx     v1,  0, r10;    ;# v1 = pred = p0..p15

    lwz r0, 0(r3)           ;# v3 = d0..d7

    get_two_diff_rows

    vmrghb  v2, v0, v1;     ;# v2 = 16-bit p0..p7
    vaddshs v2, v2, v3;     ;# v2 = r0..r7

    lwzu r0, 32(r3)         ;# v3 = d8..d15

    get_two_diff_rows

    vmrglb  v1, v0, v1;     ;# v1 = 16-bit p8..p15
    vaddshs v3, v3, v1;     ;# v3 = r8..r15

    vpkshus v2, v2, v3;     ;# v2 = 8-bit r0..r15
    stvx    v2,  0, r10;    ;# 16 pels to dst from buf

    lwz     r0, 0(r10)
    stw     r0, 0(r5)
    lwz     r0, 4(r10)
    stwux   r0, r5, r6
    lwz     r0, 8(r10)
    stwux   r0, r5, r6
    lwz     r0, 12(r10)
    stwx    r0, r5, r6

    lwz     r12, -8(r1)                 ;# restore old VRSAVE from stack
    mtspr   256, r12                    ;# reset old VRSAVE

    blr
