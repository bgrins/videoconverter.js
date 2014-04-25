/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "subpixel.h"
#include "loopfilter.h"
#include "recon.h"
#include "onyxc_int.h"

extern void (*vp8_post_proc_down_and_across_mb_row)(
    unsigned char *src_ptr,
    unsigned char *dst_ptr,
    int src_pixels_per_line,
    int dst_pixels_per_line,
    int cols,
    unsigned char *f,
    int size
);

extern void (*vp8_mbpost_proc_down)(unsigned char *dst, int pitch, int rows, int cols, int flimit);
extern void vp8_mbpost_proc_down_c(unsigned char *dst, int pitch, int rows, int cols, int flimit);
extern void (*vp8_mbpost_proc_across_ip)(unsigned char *src, int pitch, int rows, int cols, int flimit);
extern void vp8_mbpost_proc_across_ip_c(unsigned char *src, int pitch, int rows, int cols, int flimit);

extern void vp8_post_proc_down_and_across_mb_row_c
(
    unsigned char *src_ptr,
    unsigned char *dst_ptr,
    int src_pixels_per_line,
    int dst_pixels_per_line,
    int cols,
    unsigned char *f,
    int size
);
void vp8_plane_add_noise_c(unsigned char *Start, unsigned int Width, unsigned int Height, int Pitch, int q, int a);

extern copy_mem_block_function *vp8_copy_mem16x16;
extern copy_mem_block_function *vp8_copy_mem8x8;
extern copy_mem_block_function *vp8_copy_mem8x4;

// PPC
extern subpixel_predict_function sixtap_predict_ppc;
extern subpixel_predict_function sixtap_predict8x4_ppc;
extern subpixel_predict_function sixtap_predict8x8_ppc;
extern subpixel_predict_function sixtap_predict16x16_ppc;
extern subpixel_predict_function bilinear_predict4x4_ppc;
extern subpixel_predict_function bilinear_predict8x4_ppc;
extern subpixel_predict_function bilinear_predict8x8_ppc;
extern subpixel_predict_function bilinear_predict16x16_ppc;

extern copy_mem_block_function copy_mem16x16_ppc;

void recon_b_ppc(short *diff_ptr, unsigned char *pred_ptr, unsigned char *dst_ptr, int stride);
void recon2b_ppc(short *diff_ptr, unsigned char *pred_ptr, unsigned char *dst_ptr, int stride);
void recon4b_ppc(short *diff_ptr, unsigned char *pred_ptr, unsigned char *dst_ptr, int stride);

extern void short_idct4x4llm_ppc(short *input, short *output, int pitch);

// Generic C
extern subpixel_predict_function vp8_sixtap_predict_c;
extern subpixel_predict_function vp8_sixtap_predict8x4_c;
extern subpixel_predict_function vp8_sixtap_predict8x8_c;
extern subpixel_predict_function vp8_sixtap_predict16x16_c;
extern subpixel_predict_function vp8_bilinear_predict4x4_c;
extern subpixel_predict_function vp8_bilinear_predict8x4_c;
extern subpixel_predict_function vp8_bilinear_predict8x8_c;
extern subpixel_predict_function vp8_bilinear_predict16x16_c;

extern copy_mem_block_function vp8_copy_mem16x16_c;
extern copy_mem_block_function vp8_copy_mem8x8_c;
extern copy_mem_block_function vp8_copy_mem8x4_c;

void vp8_recon_b_c(short *diff_ptr, unsigned char *pred_ptr, unsigned char *dst_ptr, int stride);
void vp8_recon2b_c(short *diff_ptr, unsigned char *pred_ptr, unsigned char *dst_ptr, int stride);
void vp8_recon4b_c(short *diff_ptr, unsigned char *pred_ptr, unsigned char *dst_ptr, int stride);

extern void vp8_short_idct4x4llm_1_c(short *input, short *output, int pitch);
extern void vp8_short_idct4x4llm_c(short *input, short *output, int pitch);
extern void vp8_dc_only_idct_c(short input_dc, short *output, int pitch);

// PPC
extern loop_filter_block_function loop_filter_mbv_ppc;
extern loop_filter_block_function loop_filter_bv_ppc;
extern loop_filter_block_function loop_filter_mbh_ppc;
extern loop_filter_block_function loop_filter_bh_ppc;

extern loop_filter_block_function loop_filter_mbvs_ppc;
extern loop_filter_block_function loop_filter_bvs_ppc;
extern loop_filter_block_function loop_filter_mbhs_ppc;
extern loop_filter_block_function loop_filter_bhs_ppc;

// Generic C
extern loop_filter_block_function vp8_loop_filter_mbv_c;
extern loop_filter_block_function vp8_loop_filter_bv_c;
extern loop_filter_block_function vp8_loop_filter_mbh_c;
extern loop_filter_block_function vp8_loop_filter_bh_c;

extern loop_filter_block_function vp8_loop_filter_mbvs_c;
extern loop_filter_block_function vp8_loop_filter_bvs_c;
extern loop_filter_block_function vp8_loop_filter_mbhs_c;
extern loop_filter_block_function vp8_loop_filter_bhs_c;

extern loop_filter_block_function *vp8_lf_mbvfull;
extern loop_filter_block_function *vp8_lf_mbhfull;
extern loop_filter_block_function *vp8_lf_bvfull;
extern loop_filter_block_function *vp8_lf_bhfull;

extern loop_filter_block_function *vp8_lf_mbvsimple;
extern loop_filter_block_function *vp8_lf_mbhsimple;
extern loop_filter_block_function *vp8_lf_bvsimple;
extern loop_filter_block_function *vp8_lf_bhsimple;

void vp8_clear_c(void)
{
}

void vp8_machine_specific_config(void)
{
    // Pure C:
    vp8_clear_system_state                = vp8_clear_c;
    vp8_recon_b                          = vp8_recon_b_c;
    vp8_recon4b                         = vp8_recon4b_c;
    vp8_recon2b                         = vp8_recon2b_c;

    vp8_bilinear_predict16x16            = bilinear_predict16x16_ppc;
    vp8_bilinear_predict8x8              = bilinear_predict8x8_ppc;
    vp8_bilinear_predict8x4              = bilinear_predict8x4_ppc;
    vp8_bilinear_predict                 = bilinear_predict4x4_ppc;

    vp8_sixtap_predict16x16              = sixtap_predict16x16_ppc;
    vp8_sixtap_predict8x8                = sixtap_predict8x8_ppc;
    vp8_sixtap_predict8x4                = sixtap_predict8x4_ppc;
    vp8_sixtap_predict                   = sixtap_predict_ppc;

    vp8_short_idct4x4_1                  = vp8_short_idct4x4llm_1_c;
    vp8_short_idct4x4                    = short_idct4x4llm_ppc;
    vp8_dc_only_idct                      = vp8_dc_only_idct_c;

    vp8_lf_mbvfull                       = loop_filter_mbv_ppc;
    vp8_lf_bvfull                        = loop_filter_bv_ppc;
    vp8_lf_mbhfull                       = loop_filter_mbh_ppc;
    vp8_lf_bhfull                        = loop_filter_bh_ppc;

    vp8_lf_mbvsimple                     = loop_filter_mbvs_ppc;
    vp8_lf_bvsimple                      = loop_filter_bvs_ppc;
    vp8_lf_mbhsimple                     = loop_filter_mbhs_ppc;
    vp8_lf_bhsimple                      = loop_filter_bhs_ppc;

    vp8_post_proc_down_and_across_mb_row = vp8_post_proc_down_and_across_mb_row_c;
    vp8_mbpost_proc_down                  = vp8_mbpost_proc_down_c;
    vp8_mbpost_proc_across_ip              = vp8_mbpost_proc_across_ip_c;
    vp8_plane_add_noise                   = vp8_plane_add_noise_c;

    vp8_copy_mem16x16                    = copy_mem16x16_ppc;
    vp8_copy_mem8x8                      = vp8_copy_mem8x8_c;
    vp8_copy_mem8x4                      = vp8_copy_mem8x4_c;

}
