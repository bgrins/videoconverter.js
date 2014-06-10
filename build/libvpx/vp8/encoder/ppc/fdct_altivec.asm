;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    .globl vp8_short_fdct4x4_ppc
    .globl vp8_short_fdct8x4_ppc

.macro load_c V, LABEL, OFF, R0, R1
    lis     \R0, \LABEL@ha
    la      \R1, \LABEL@l(\R0)
    lvx     \V, \OFF, \R1
.endm

;# Forward and inverse DCTs are nearly identical; only differences are
;#   in normalization (fwd is twice unitary, inv is half unitary)
;#   and that they are of course transposes of each other.
;#
;#   The following three accomplish most of implementation and
;#   are used only by ppc_idct.c and ppc_fdct.c.
.macro prologue
    mfspr   r11, 256            ;# get old VRSAVE
    oris    r12, r11, 0xfffc
    mtspr   256, r12            ;# set VRSAVE

    stwu    r1,-32(r1)          ;# create space on the stack

    li      r6, 16

    load_c v0, dct_tab, 0, r9, r10
    lvx     v1,   r6, r10
    addi    r10, r10, 32
    lvx     v2,    0, r10
    lvx     v3,   r6, r10

    load_c v4, ppc_dctperm_tab,  0, r9, r10
    load_c v5, ppc_dctperm_tab, r6, r9, r10

    load_c v6, round_tab, 0, r10, r9
.endm

.macro epilogue
    addi    r1, r1, 32          ;# recover stack

    mtspr   256, r11            ;# reset old VRSAVE
.endm

;# Do horiz xf on two rows of coeffs  v8 = a0 a1 a2 a3  b0 b1 b2 b3.
;#   a/A are the even rows 0,2   b/B are the odd rows 1,3
;#   For fwd transform, indices are horizontal positions, then frequencies.
;#   For inverse transform, frequencies then positions.
;#   The two resulting  A0..A3  B0..B3  are later combined
;#   and vertically transformed.

.macro two_rows_horiz Dst
    vperm   v9, v8, v8, v4      ;# v9 = a2 a3 a0 a1  b2 b3 b0 b1

    vmsumshm v10, v0, v8, v6
    vmsumshm v10, v1, v9, v10
    vsraw   v10, v10, v7        ;# v10 = A0 A1  B0 B1

    vmsumshm v11, v2, v8, v6
    vmsumshm v11, v3, v9, v11
    vsraw   v11, v11, v7        ;# v11 = A2 A3  B2 B3

    vpkuwum v10, v10, v11       ;# v10  = A0 A1  B0 B1  A2 A3  B2 B3
    vperm   \Dst, v10, v10, v5  ;# Dest = A0 B0  A1 B1  A2 B2  A3 B3
.endm

;# Vertical xf on two rows. DCT values in comments are for inverse transform;
;#   forward transform uses transpose.

.macro two_rows_vert Ceven, Codd
    vspltw  v8, \Ceven, 0       ;# v8 = c00 c10  or  c02 c12 four times
    vspltw  v9, \Codd,  0       ;# v9 = c20 c30  or  c22 c32 ""
    vmsumshm v8, v8, v12, v6
    vmsumshm v8, v9, v13, v8
    vsraw   v10, v8, v7

    vspltw  v8, \Codd,  1       ;# v8 = c01 c11  or  c03 c13
    vspltw  v9, \Ceven, 1       ;# v9 = c21 c31  or  c23 c33
    vmsumshm v8, v8, v12, v6
    vmsumshm v8, v9, v13, v8
    vsraw   v8, v8, v7

    vpkuwum v8, v10, v8         ;# v8 = rows 0,1  or 2,3
.endm

.macro two_rows_h Dest
    stw     r0,  0(r8)
    lwz     r0,  4(r3)
    stw     r0,  4(r8)
    lwzux   r0, r3,r5
    stw     r0,  8(r8)
    lwz     r0,  4(r3)
    stw     r0, 12(r8)
    lvx     v8,  0,r8
    two_rows_horiz \Dest
.endm

    .align 2
;# r3 short *input
;# r4 short *output
;# r5 int pitch
vp8_short_fdct4x4_ppc:

    prologue

    vspltisw v7, 14             ;# == 14, fits in 5 signed bits
    addi    r8, r1, 0


    lwz     r0, 0(r3)
    two_rows_h v12                ;# v12 = H00 H10  H01 H11  H02 H12  H03 H13

    lwzux   r0, r3, r5
    two_rows_h v13                ;# v13 = H20 H30  H21 H31  H22 H32  H23 H33

    lvx     v6, r6, r9          ;# v6 = Vround
    vspltisw v7, -16            ;# == 16 == -16, only low 5 bits matter

    two_rows_vert v0, v1
    stvx    v8, 0, r4
    two_rows_vert v2, v3
    stvx    v8, r6, r4

    epilogue

    blr

    .align 2
;# r3 short *input
;# r4 short *output
;# r5 int pitch
vp8_short_fdct8x4_ppc:
    prologue

    vspltisw v7, 14             ;# == 14, fits in 5 signed bits
    addi    r8,  r1, 0
    addi    r10, r3, 0

    lwz     r0, 0(r3)
    two_rows_h v12                ;# v12 = H00 H10  H01 H11  H02 H12  H03 H13

    lwzux   r0, r3, r5
    two_rows_h v13                ;# v13 = H20 H30  H21 H31  H22 H32  H23 H33

    lvx     v6, r6, r9          ;# v6 = Vround
    vspltisw v7, -16            ;# == 16 == -16, only low 5 bits matter

    two_rows_vert v0, v1
    stvx    v8, 0, r4
    two_rows_vert v2, v3
    stvx    v8, r6, r4

    ;# Next block
    addi    r3, r10, 8
    addi    r4, r4, 32
    lvx     v6, 0, r9           ;# v6 = Hround

    vspltisw v7, 14             ;# == 14, fits in 5 signed bits
    addi    r8, r1, 0

    lwz     r0, 0(r3)
    two_rows_h v12                ;# v12 = H00 H10  H01 H11  H02 H12  H03 H13

    lwzux   r0, r3, r5
    two_rows_h v13                ;# v13 = H20 H30  H21 H31  H22 H32  H23 H33

    lvx     v6, r6, r9          ;# v6 = Vround
    vspltisw v7, -16            ;# == 16 == -16, only low 5 bits matter

    two_rows_vert v0, v1
    stvx    v8, 0, r4
    two_rows_vert v2, v3
    stvx    v8, r6, r4

    epilogue

    blr

    .data
    .align 4
ppc_dctperm_tab:
    .byte 4,5,6,7, 0,1,2,3, 12,13,14,15, 8,9,10,11
    .byte 0,1,4,5, 2,3,6,7, 8,9,12,13, 10,11,14,15

    .align 4
dct_tab:
    .short  23170, 23170,-12540,-30274, 23170, 23170,-12540,-30274
    .short  23170, 23170, 30274, 12540, 23170, 23170, 30274, 12540

    .short  23170,-23170, 30274,-12540, 23170,-23170, 30274,-12540
    .short -23170, 23170, 12540,-30274,-23170, 23170, 12540,-30274

    .align 4
round_tab:
    .long (1 << (14-1)), (1 << (14-1)), (1 << (14-1)), (1 << (14-1))
    .long (1 << (16-1)), (1 << (16-1)), (1 << (16-1)), (1 << (16-1))
