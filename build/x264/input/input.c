/*****************************************************************************
 * input.c: common input functions
 *****************************************************************************
 * Copyright (C) 2010-2014 x264 project
 *
 * Authors: Steven Walters <kemuri9@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/

#include "input.h"

const x264_cli_csp_t x264_cli_csps[] = {
    [X264_CSP_I420] = { "i420", 3, { 1, .5, .5 }, { 1, .5, .5 }, 2, 2 },
    [X264_CSP_I422] = { "i422", 3, { 1, .5, .5 }, { 1,  1,  1 }, 2, 1 },
    [X264_CSP_I444] = { "i444", 3, { 1,  1,  1 }, { 1,  1,  1 }, 1, 1 },
    [X264_CSP_YV12] = { "yv12", 3, { 1, .5, .5 }, { 1, .5, .5 }, 2, 2 },
    [X264_CSP_YV16] = { "yv16", 3, { 1, .5, .5 }, { 1,  1,  1 }, 2, 1 },
    [X264_CSP_YV24] = { "yv24", 3, { 1,  1,  1 }, { 1,  1,  1 }, 1, 1 },
    [X264_CSP_NV12] = { "nv12", 2, { 1,  1 },     { 1, .5 },     2, 2 },
    [X264_CSP_NV16] = { "nv16", 2, { 1,  1 },     { 1,  1 },     2, 1 },
    [X264_CSP_BGR]  = { "bgr",  1, { 3 },         { 1 },         1, 1 },
    [X264_CSP_BGRA] = { "bgra", 1, { 4 },         { 1 },         1, 1 },
    [X264_CSP_RGB]  = { "rgb",  1, { 3 },         { 1 },         1, 1 },
};

int x264_cli_csp_is_invalid( int csp )
{
    int csp_mask = csp & X264_CSP_MASK;
    return csp_mask <= X264_CSP_NONE || csp_mask >= X264_CSP_CLI_MAX ||
           csp_mask == X264_CSP_V210 || csp & X264_CSP_OTHER;
}

int x264_cli_csp_depth_factor( int csp )
{
    if( x264_cli_csp_is_invalid( csp ) )
        return 0;
    return (csp & X264_CSP_HIGH_DEPTH) ? 2 : 1;
}

uint64_t x264_cli_pic_plane_size( int csp, int width, int height, int plane )
{
    int csp_mask = csp & X264_CSP_MASK;
    if( x264_cli_csp_is_invalid( csp ) || plane < 0 || plane >= x264_cli_csps[csp_mask].planes )
        return 0;
    uint64_t size = (uint64_t)width * height;
    size *= x264_cli_csps[csp_mask].width[plane] * x264_cli_csps[csp_mask].height[plane];
    size *= x264_cli_csp_depth_factor( csp );
    return size;
}

uint64_t x264_cli_pic_size( int csp, int width, int height )
{
    if( x264_cli_csp_is_invalid( csp ) )
        return 0;
    uint64_t size = 0;
    int csp_mask = csp & X264_CSP_MASK;
    for( int i = 0; i < x264_cli_csps[csp_mask].planes; i++ )
        size += x264_cli_pic_plane_size( csp, width, height, i );
    return size;
}

static int x264_cli_pic_alloc_internal( cli_pic_t *pic, int csp, int width, int height, int align )
{
    memset( pic, 0, sizeof(cli_pic_t) );
    int csp_mask = csp & X264_CSP_MASK;
    if( x264_cli_csp_is_invalid( csp ) )
        pic->img.planes = 0;
    else
        pic->img.planes = x264_cli_csps[csp_mask].planes;
    pic->img.csp    = csp;
    pic->img.width  = width;
    pic->img.height = height;
    for( int i = 0; i < pic->img.planes; i++ )
    {
        int stride = width * x264_cli_csps[csp_mask].width[i];
        stride *= x264_cli_csp_depth_factor( csp );
        stride = ALIGN( stride, align );
        uint64_t size = (uint64_t)(height * x264_cli_csps[csp_mask].height[i]) * stride;
        pic->img.plane[i] = x264_malloc( size );
        if( !pic->img.plane[i] )
            return -1;
        pic->img.stride[i] = stride;
    }

    return 0;
}

int x264_cli_pic_alloc( cli_pic_t *pic, int csp, int width, int height )
{
    return x264_cli_pic_alloc_internal( pic, csp, width, height, 1 );
}

int x264_cli_pic_alloc_aligned( cli_pic_t *pic, int csp, int width, int height )
{
    return x264_cli_pic_alloc_internal( pic, csp, width, height, NATIVE_ALIGN );
}

void x264_cli_pic_clean( cli_pic_t *pic )
{
    for( int i = 0; i < pic->img.planes; i++ )
        x264_free( pic->img.plane[i] );
    memset( pic, 0, sizeof(cli_pic_t) );
}

const x264_cli_csp_t *x264_cli_get_csp( int csp )
{
    if( x264_cli_csp_is_invalid( csp ) )
        return NULL;
    return x264_cli_csps + (csp&X264_CSP_MASK);
}
