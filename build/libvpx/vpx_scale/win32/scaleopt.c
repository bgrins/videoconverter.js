/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/****************************************************************************
*
*   Module Title :     scaleopt.cpp
*
*   Description  :     Optimized scaling functions
*
****************************************************************************/
#include "pragmas.h"

/****************************************************************************
*  Module Statics
****************************************************************************/
__declspec(align(16)) const static unsigned short round_values[] = { 128, 128, 128, 128 };

#include "vpx_scale/vpx_scale.h"
#include "vpx_mem/vpx_mem.h"

__declspec(align(16)) const static unsigned short const54_2[] = {  0,  64, 128, 192 };
__declspec(align(16)) const static unsigned short const54_1[] = {256, 192, 128,  64 };


/****************************************************************************
 *
 *  ROUTINE       : horizontal_line_5_4_scale_mmx
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width    : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width      : Stride of destination (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 4 to 5.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
static
void horizontal_line_5_4_scale_mmx
(
  const unsigned char *source,
  unsigned int source_width,
  unsigned char *dest,
  unsigned int dest_width
) {
  /*
  unsigned i;
  unsigned int a, b, c, d, e;
  unsigned char *des = dest;
  const unsigned char *src = source;

  (void) dest_width;

  for ( i=0; i<source_width; i+=5 )
  {
      a = src[0];
      b = src[1];
      c = src[2];
      d = src[3];
      e = src[4];

      des[0] = a;
      des[1] = ((b*192 + c* 64 + 128)>>8);
      des[2] = ((c*128 + d*128 + 128)>>8);
      des[3] = ((d* 64 + e*192 + 128)>>8);

      src += 5;
      des += 4;
  }
  */
  (void) dest_width;

  __asm {

    mov         esi,        source;
    mov         edi,        dest;

    mov         ecx,        source_width;
    movq        mm5,        const54_1;

    pxor        mm7,        mm7;
    movq        mm6,        const54_2;

    movq        mm4,        round_values;
    lea         edx,        [esi+ecx];
    horizontal_line_5_4_loop:

    movq        mm0,        QWORD PTR  [esi];
    00 01 02 03 04 05 06 07
    movq        mm1,        mm0;
    00 01 02 03 04 05 06 07

    psrlq       mm0,        8;
    01 02 03 04 05 06 07 xx
    punpcklbw   mm1,        mm7;
    xx 00 xx 01 xx 02 xx 03

    punpcklbw   mm0,        mm7;
    xx 01 xx 02 xx 03 xx 04
    pmullw      mm1,        mm5

    pmullw      mm0,        mm6
    add         esi,        5

    add         edi,        4
    paddw       mm1,        mm0

    paddw       mm1,        mm4
    psrlw       mm1,        8

    cmp         esi,        edx
    packuswb    mm1,        mm7

    movd        DWORD PTR [edi-4], mm1

    jl          horizontal_line_5_4_loop

  }

}
__declspec(align(16)) const static unsigned short one_fourths[]   = {  64,  64,  64, 64  };
__declspec(align(16)) const static unsigned short two_fourths[]   = { 128, 128, 128, 128 };
__declspec(align(16)) const static unsigned short three_fourths[] = { 192, 192, 192, 192 };

static
void vertical_band_5_4_scale_mmx(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width) {

  __asm {
    push        ebx

    mov         esi,    source                    // Get the source and destination pointer
    mov         ecx,    src_pitch               // Get the pitch size

    mov         edi,    dest                    // tow lines below
    pxor        mm7,    mm7                     // clear out mm7

    mov         edx,    dest_pitch               // Loop counter
    mov         ebx,    dest_width

    vs_5_4_loop:

    movd        mm0,    DWORD ptr [esi]         // src[0];
    movd        mm1,    DWORD ptr [esi+ecx]     // src[1];

    movd        mm2,    DWORD ptr [esi+ecx*2]
    lea         eax,    [esi+ecx*2]             //

    punpcklbw   mm1,    mm7
    punpcklbw   mm2,    mm7

    movq        mm3,    mm2
    pmullw      mm1,    three_fourths

    pmullw      mm2,    one_fourths
    movd        mm4,    [eax+ecx]

    pmullw      mm3,    two_fourths
    punpcklbw   mm4,    mm7

    movq        mm5,    mm4
    pmullw      mm4,    two_fourths

    paddw       mm1,    mm2
    movd        mm6,    [eax+ecx*2]

    pmullw      mm5,    one_fourths
    paddw       mm1,    round_values;

    paddw       mm3,    mm4
    psrlw       mm1,    8

    punpcklbw   mm6,    mm7
    paddw       mm3,    round_values

    pmullw      mm6,    three_fourths
    psrlw       mm3,    8

    packuswb    mm1,    mm7
    packuswb    mm3,    mm7

    movd        DWORD PTR [edi], mm0
    movd        DWORD PTR [edi+edx], mm1


    paddw       mm5,    mm6
    movd        DWORD PTR [edi+edx*2], mm3

    lea         eax,    [edi+edx*2]
    paddw       mm5,    round_values

    psrlw       mm5,    8
    add         edi,    4

    packuswb    mm5,    mm7
    movd        DWORD PTR [eax+edx], mm5

    add         esi,    4
    sub         ebx,    4

    jg         vs_5_4_loop

    pop         ebx
  }
}


__declspec(align(16)) const static unsigned short const53_1[] = {  0,  85, 171, 0 };
__declspec(align(16)) const static unsigned short const53_2[] = {256, 171,  85, 0 };


static
void horizontal_line_5_3_scale_mmx
(
  const unsigned char *source,
  unsigned int source_width,
  unsigned char *dest,
  unsigned int dest_width
) {

  (void) dest_width;
  __asm {

    mov         esi,        source;
    mov         edi,        dest;

    mov         ecx,        source_width;
    movq        mm5,        const53_1;

    pxor        mm7,        mm7;
    movq        mm6,        const53_2;

    movq        mm4,        round_values;
    lea         edx,        [esi+ecx-5];
    horizontal_line_5_3_loop:

    movq        mm0,        QWORD PTR  [esi];
    00 01 02 03 04 05 06 07
    movq        mm1,        mm0;
    00 01 02 03 04 05 06 07

    psllw       mm0,        8;
    xx 00 xx 02 xx 04 xx 06
    psrlw       mm1,        8;
    01 xx 03 xx 05 xx 07 xx

    psrlw       mm0,        8;
    00 xx 02 xx 04 xx 06 xx
    psllq       mm1,        16;
    xx xx 01 xx 03 xx 05 xx

    pmullw      mm0,        mm6

    pmullw      mm1,        mm5
    add         esi,        5

    add         edi,        3
    paddw       mm1,        mm0

    paddw       mm1,        mm4
    psrlw       mm1,        8

    cmp         esi,        edx
    packuswb    mm1,        mm7

    movd        DWORD PTR [edi-3], mm1
    jl          horizontal_line_5_3_loop

// exit condition
    movq        mm0,        QWORD PTR  [esi];
    00 01 02 03 04 05 06 07
    movq        mm1,        mm0;
    00 01 02 03 04 05 06 07

    psllw       mm0,        8;
    xx 00 xx 02 xx 04 xx 06
    psrlw       mm1,        8;
    01 xx 03 xx 05 xx 07 xx

    psrlw       mm0,        8;
    00 xx 02 xx 04 xx 06 xx
    psllq       mm1,        16;
    xx xx 01 xx 03 xx 05 xx

    pmullw      mm0,        mm6

    pmullw      mm1,        mm5
    paddw       mm1,        mm0

    paddw       mm1,        mm4
    psrlw       mm1,        8

    packuswb    mm1,        mm7
    movd        eax,        mm1

    mov         edx,        eax
    shr         edx,        16

    mov         WORD PTR[edi],   ax
    mov         BYTE PTR[edi+2], dl

  }

}

__declspec(align(16)) const static unsigned short one_thirds[] = {  85,  85,  85,  85 };
__declspec(align(16)) const static unsigned short two_thirds[] = { 171, 171, 171, 171 };

static
void vertical_band_5_3_scale_mmx(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width) {

  __asm {
    push        ebx

    mov         esi,    source                    // Get the source and destination pointer
    mov         ecx,    src_pitch               // Get the pitch size

    mov         edi,    dest                    // tow lines below
    pxor        mm7,    mm7                     // clear out mm7

    mov         edx,    dest_pitch               // Loop counter
    movq        mm5,    one_thirds

    movq        mm6,    two_thirds
    mov         ebx,    dest_width;

    vs_5_3_loop:

    movd        mm0,    DWORD ptr [esi]         // src[0];
    movd        mm1,    DWORD ptr [esi+ecx]     // src[1];

    movd        mm2,    DWORD ptr [esi+ecx*2]
    lea         eax,    [esi+ecx*2]             //

    punpcklbw   mm1,    mm7
    punpcklbw   mm2,    mm7

    pmullw      mm1,    mm5
    pmullw      mm2,    mm6

    movd        mm3,    DWORD ptr [eax+ecx]
    movd        mm4,    DWORD ptr [eax+ecx*2]

    punpcklbw   mm3,    mm7
    punpcklbw   mm4,    mm7

    pmullw      mm3,    mm6
    pmullw      mm4,    mm5


    movd        DWORD PTR [edi], mm0
    paddw       mm1,    mm2

    paddw       mm1,    round_values
    psrlw       mm1,    8

    packuswb    mm1,    mm7
    paddw       mm3,    mm4

    paddw       mm3,    round_values
    movd        DWORD PTR [edi+edx], mm1

    psrlw       mm3,    8
    packuswb    mm3,    mm7

    movd        DWORD PTR [edi+edx*2], mm3


    add         edi,    4
    add         esi,    4

    sub         ebx,    4
    jg          vs_5_3_loop

    pop         ebx
  }
}




/****************************************************************************
 *
 *  ROUTINE       : horizontal_line_2_1_scale
 *
 *  INPUTS        : const unsigned char *source :
 *                  unsigned int source_width    :
 *                  unsigned char *dest         :
 *                  unsigned int dest_width      :
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : 1 to 2 up-scaling of a horizontal line of pixels.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
static
void horizontal_line_2_1_scale_mmx
(
  const unsigned char *source,
  unsigned int source_width,
  unsigned char *dest,
  unsigned int dest_width
) {
  (void) dest_width;
  (void) source_width;
  __asm {
    mov         esi,    source
    mov         edi,    dest

    pxor        mm7,    mm7
    mov         ecx,    dest_width

    xor         edx,    edx
    hs_2_1_loop:

    movq        mm0,    [esi+edx*2]
    psllw       mm0,    8

    psrlw       mm0,    8
    packuswb    mm0,    mm7

    movd        DWORD Ptr [edi+edx], mm0;
    add         edx,    4

    cmp         edx,    ecx
    jl          hs_2_1_loop

  }
}



static
void vertical_band_2_1_scale_mmx(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width) {
  (void) dest_pitch;
  (void) src_pitch;
  vpx_memcpy(dest, source, dest_width);
}


__declspec(align(16)) const static unsigned short three_sixteenths[] = {  48,  48,  48,  48 };
__declspec(align(16)) const static unsigned short ten_sixteenths[]   = { 160, 160, 160, 160 };

static
void vertical_band_2_1_scale_i_mmx(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width) {

  (void) dest_pitch;
  __asm {
    mov         esi,        source
    mov         edi,        dest

    mov         eax,        src_pitch
    mov         edx,        dest_width

    pxor        mm7,        mm7
    sub         esi,        eax             // back one line


    lea         ecx,        [esi+edx];
    movq        mm6,        round_values;

    movq        mm5,        three_sixteenths;
    movq        mm4,        ten_sixteenths;

    vs_2_1_i_loop:
    movd        mm0,        [esi]           //
    movd        mm1,        [esi+eax]       //

    movd        mm2,        [esi+eax*2]     //
    punpcklbw   mm0,        mm7

    pmullw      mm0,        mm5
    punpcklbw   mm1,        mm7

    pmullw      mm1,        mm4
    punpcklbw   mm2,        mm7

    pmullw      mm2,        mm5
    paddw       mm0,        round_values

    paddw       mm1,        mm2
    paddw       mm0,        mm1

    psrlw       mm0,        8
    packuswb    mm0,        mm7

    movd        DWORD PTR [edi],        mm0
    add         esi,        4

    add         edi,        4;
    cmp         esi,        ecx
    jl          vs_2_1_i_loop

  }
}



void
register_mmxscalers(void) {
  vp8_vertical_band_5_4_scale           = vertical_band_5_4_scale_mmx;
  vp8_vertical_band_5_3_scale           = vertical_band_5_3_scale_mmx;
  vp8_vertical_band_2_1_scale           = vertical_band_2_1_scale_mmx;
  vp8_vertical_band_2_1_scale_i         = vertical_band_2_1_scale_i_mmx;
  vp8_horizontal_line_2_1_scale         = horizontal_line_2_1_scale_mmx;
  vp8_horizontal_line_5_3_scale         = horizontal_line_5_3_scale_mmx;
  vp8_horizontal_line_5_4_scale         = horizontal_line_5_4_scale_mmx;
}
