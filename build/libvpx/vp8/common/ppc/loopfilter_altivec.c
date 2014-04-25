/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "loopfilter.h"
#include "onyxc_int.h"

typedef void loop_filter_function_y_ppc
(
    unsigned char *s,   // source pointer
    int p,              // pitch
    const signed char *flimit,
    const signed char *limit,
    const signed char *thresh
);

typedef void loop_filter_function_uv_ppc
(
    unsigned char *u,   // source pointer
    unsigned char *v,   // source pointer
    int p,              // pitch
    const signed char *flimit,
    const signed char *limit,
    const signed char *thresh
);

typedef void loop_filter_function_s_ppc
(
    unsigned char *s,   // source pointer
    int p,              // pitch
    const signed char *flimit
);

loop_filter_function_y_ppc mbloop_filter_horizontal_edge_y_ppc;
loop_filter_function_y_ppc mbloop_filter_vertical_edge_y_ppc;
loop_filter_function_y_ppc loop_filter_horizontal_edge_y_ppc;
loop_filter_function_y_ppc loop_filter_vertical_edge_y_ppc;

loop_filter_function_uv_ppc mbloop_filter_horizontal_edge_uv_ppc;
loop_filter_function_uv_ppc mbloop_filter_vertical_edge_uv_ppc;
loop_filter_function_uv_ppc loop_filter_horizontal_edge_uv_ppc;
loop_filter_function_uv_ppc loop_filter_vertical_edge_uv_ppc;

loop_filter_function_s_ppc loop_filter_simple_horizontal_edge_ppc;
loop_filter_function_s_ppc loop_filter_simple_vertical_edge_ppc;

// Horizontal MB filtering
void loop_filter_mbh_ppc(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                         int y_stride, int uv_stride, loop_filter_info *lfi)
{
    mbloop_filter_horizontal_edge_y_ppc(y_ptr, y_stride, lfi->mbflim, lfi->lim, lfi->thr);

    if (u_ptr)
        mbloop_filter_horizontal_edge_uv_ppc(u_ptr, v_ptr, uv_stride, lfi->mbflim, lfi->lim, lfi->thr);
}

void loop_filter_mbhs_ppc(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                          int y_stride, int uv_stride, loop_filter_info *lfi)
{
    (void)u_ptr;
    (void)v_ptr;
    (void)uv_stride;
    loop_filter_simple_horizontal_edge_ppc(y_ptr, y_stride, lfi->mbflim);
}

// Vertical MB Filtering
void loop_filter_mbv_ppc(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                         int y_stride, int uv_stride, loop_filter_info *lfi)
{
    mbloop_filter_vertical_edge_y_ppc(y_ptr, y_stride, lfi->mbflim, lfi->lim, lfi->thr);

    if (u_ptr)
        mbloop_filter_vertical_edge_uv_ppc(u_ptr, v_ptr, uv_stride, lfi->mbflim, lfi->lim, lfi->thr);
}

void loop_filter_mbvs_ppc(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                          int y_stride, int uv_stride, loop_filter_info *lfi)
{
    (void)u_ptr;
    (void)v_ptr;
    (void)uv_stride;
    loop_filter_simple_vertical_edge_ppc(y_ptr, y_stride, lfi->mbflim);
}

// Horizontal B Filtering
void loop_filter_bh_ppc(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                        int y_stride, int uv_stride, loop_filter_info *lfi)
{
    // These should all be done at once with one call, instead of 3
    loop_filter_horizontal_edge_y_ppc(y_ptr + 4 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr);
    loop_filter_horizontal_edge_y_ppc(y_ptr + 8 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr);
    loop_filter_horizontal_edge_y_ppc(y_ptr + 12 * y_stride, y_stride, lfi->flim, lfi->lim, lfi->thr);

    if (u_ptr)
        loop_filter_horizontal_edge_uv_ppc(u_ptr + 4 * uv_stride, v_ptr + 4 * uv_stride, uv_stride, lfi->flim, lfi->lim, lfi->thr);
}

void loop_filter_bhs_ppc(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                         int y_stride, int uv_stride, loop_filter_info *lfi)
{
    (void)u_ptr;
    (void)v_ptr;
    (void)uv_stride;
    loop_filter_simple_horizontal_edge_ppc(y_ptr + 4 * y_stride, y_stride, lfi->flim);
    loop_filter_simple_horizontal_edge_ppc(y_ptr + 8 * y_stride, y_stride, lfi->flim);
    loop_filter_simple_horizontal_edge_ppc(y_ptr + 12 * y_stride, y_stride, lfi->flim);
}

// Vertical B Filtering
void loop_filter_bv_ppc(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                        int y_stride, int uv_stride, loop_filter_info *lfi)
{
    loop_filter_vertical_edge_y_ppc(y_ptr, y_stride, lfi->flim, lfi->lim, lfi->thr);

    if (u_ptr)
        loop_filter_vertical_edge_uv_ppc(u_ptr + 4, v_ptr + 4, uv_stride, lfi->flim, lfi->lim, lfi->thr);
}

void loop_filter_bvs_ppc(unsigned char *y_ptr, unsigned char *u_ptr, unsigned char *v_ptr,
                         int y_stride, int uv_stride, loop_filter_info *lfi)
{
    (void)u_ptr;
    (void)v_ptr;
    (void)uv_stride;
    loop_filter_simple_vertical_edge_ppc(y_ptr + 4,  y_stride, lfi->flim);
    loop_filter_simple_vertical_edge_ppc(y_ptr + 8,  y_stride, lfi->flim);
    loop_filter_simple_vertical_edge_ppc(y_ptr + 12, y_stride, lfi->flim);
}
