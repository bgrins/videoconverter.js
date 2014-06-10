/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_RDOPT_H_
#define VP9_ENCODER_VP9_RDOPT_H_

#include "vp9/encoder/vp9_onyx_int.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RDDIV_BITS          7

#define RDCOST(RM, DM, R, D) \
  (((128 + ((int64_t)R) * (RM)) >> 8) + (D << DM))
#define QIDX_SKIP_THRESH     115

#define MV_COST_WEIGHT      108
#define MV_COST_WEIGHT_SUB  120

#define INVALID_MV 0x80008000

struct TileInfo;

int vp9_compute_rd_mult(const VP9_COMP *cpi, int qindex);

void vp9_initialize_rd_consts(VP9_COMP *cpi);

void vp9_initialize_me_consts(VP9_COMP *cpi, int qindex);

void vp9_model_rd_from_var_lapndz(unsigned int var, unsigned int n,
                                  unsigned int qstep, int *rate,
                                  int64_t *dist);

int vp9_get_switchable_rate(const MACROBLOCK *x);

void vp9_setup_buffer_inter(VP9_COMP *cpi, MACROBLOCK *x,
                            const TileInfo *const tile,
                            MV_REFERENCE_FRAME ref_frame,
                            BLOCK_SIZE block_size,
                            int mi_row, int mi_col,
                            int_mv frame_nearest_mv[MAX_REF_FRAMES],
                            int_mv frame_near_mv[MAX_REF_FRAMES],
                            struct buf_2d yv12_mb[4][MAX_MB_PLANE]);

const YV12_BUFFER_CONFIG *vp9_get_scaled_ref_frame(const VP9_COMP *cpi,
                                                   int ref_frame);

void vp9_rd_pick_intra_mode_sb(VP9_COMP *cpi, MACROBLOCK *x,
                               int *r, int64_t *d, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd);

int64_t vp9_rd_pick_inter_mode_sb(VP9_COMP *cpi, MACROBLOCK *x,
                                  const struct TileInfo *const tile,
                                  int mi_row, int mi_col,
                                  int *returnrate,
                                  int64_t *returndistortion,
                                  BLOCK_SIZE bsize,
                                  PICK_MODE_CONTEXT *ctx,
                                  int64_t best_rd_so_far);

int64_t vp9_rd_pick_inter_mode_sub8x8(VP9_COMP *cpi, MACROBLOCK *x,
                                      const struct TileInfo *const tile,
                                      int mi_row, int mi_col,
                                      int *returnrate,
                                      int64_t *returndistortion,
                                      BLOCK_SIZE bsize,
                                      PICK_MODE_CONTEXT *ctx,
                                      int64_t best_rd_so_far);

void vp9_init_me_luts();

void vp9_get_entropy_contexts(BLOCK_SIZE bsize, TX_SIZE tx_size,
                              const struct macroblockd_plane *pd,
                              ENTROPY_CONTEXT t_above[16],
                              ENTROPY_CONTEXT t_left[16]);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_RDOPT_H_
