/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "./vp9_rtcd.h"

#include "vpx_mem/vpx_mem.h"

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_mvref_common.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_reconintra.h"

#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/encoder/vp9_rdopt.h"

static void full_pixel_motion_search(VP9_COMP *cpi, MACROBLOCK *x,
                                    const TileInfo *const tile,
                                    BLOCK_SIZE bsize, int mi_row, int mi_col,
                                    int_mv *tmp_mv, int *rate_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct buf_2d backup_yv12[MAX_MB_PLANE] = {{0}};
  int step_param;
  int sadpb = x->sadperbit16;
  MV mvp_full;
  int ref = mbmi->ref_frame[0];
  const MV ref_mv = mbmi->ref_mvs[ref][0].as_mv;
  int i;

  int tmp_col_min = x->mv_col_min;
  int tmp_col_max = x->mv_col_max;
  int tmp_row_min = x->mv_row_min;
  int tmp_row_max = x->mv_row_max;

  const YV12_BUFFER_CONFIG *scaled_ref_frame = vp9_get_scaled_ref_frame(cpi,
                                                                        ref);
  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[0];

    vp9_setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL);
  }

  vp9_set_mv_search_range(x, &ref_mv);

  // TODO(jingning) exploiting adaptive motion search control in non-RD
  // mode decision too.
  step_param = 6;

  for (i = LAST_FRAME; i <= LAST_FRAME && cpi->common.show_frame; ++i) {
    if ((x->pred_mv_sad[ref] >> 3) > x->pred_mv_sad[i]) {
      tmp_mv->as_int = INVALID_MV;

      if (scaled_ref_frame) {
        int i;
        for (i = 0; i < MAX_MB_PLANE; i++)
          xd->plane[i].pre[0] = backup_yv12[i];
      }
      return;
    }
  }
  assert(x->mv_best_ref_index[ref] <= 2);
  if (x->mv_best_ref_index[ref] < 2)
    mvp_full = mbmi->ref_mvs[ref][x->mv_best_ref_index[ref]].as_mv;
  else
    mvp_full = x->pred_mv[ref].as_mv;

  mvp_full.col >>= 3;
  mvp_full.row >>= 3;

  if (cpi->sf.search_method == FAST_DIAMOND) {
    // NOTE: this returns SAD
    vp9_fast_dia_search(x, &mvp_full, step_param, sadpb, 0,
                        &cpi->fn_ptr[bsize], 1,
                        &ref_mv, &tmp_mv->as_mv);
  } else if (cpi->sf.search_method == FAST_HEX) {
    // NOTE: this returns SAD
    vp9_fast_hex_search(x, &mvp_full, step_param, sadpb, 0,
                        &cpi->fn_ptr[bsize], 1,
                        &ref_mv, &tmp_mv->as_mv);
  } else if (cpi->sf.search_method == HEX) {
    // NOTE: this returns SAD
    vp9_hex_search(x, &mvp_full, step_param, sadpb, 1,
                   &cpi->fn_ptr[bsize], 1,
                   &ref_mv, &tmp_mv->as_mv);
  } else if (cpi->sf.search_method == SQUARE) {
    // NOTE: this returns SAD
    vp9_square_search(x, &mvp_full, step_param, sadpb, 1,
                      &cpi->fn_ptr[bsize], 1,
                      &ref_mv, &tmp_mv->as_mv);
  } else if (cpi->sf.search_method == BIGDIA) {
    // NOTE: this returns SAD
    vp9_bigdia_search(x, &mvp_full, step_param, sadpb, 1,
                      &cpi->fn_ptr[bsize], 1,
                      &ref_mv, &tmp_mv->as_mv);
  } else {
    int further_steps = (cpi->sf.max_step_search_steps - 1) - step_param;
    // NOTE: this returns variance
    vp9_full_pixel_diamond(cpi, x, &mvp_full, step_param,
                           sadpb, further_steps, 1,
                           &cpi->fn_ptr[bsize],
                           &ref_mv, &tmp_mv->as_mv);
  }
  x->mv_col_min = tmp_col_min;
  x->mv_col_max = tmp_col_max;
  x->mv_row_min = tmp_row_min;
  x->mv_row_max = tmp_row_max;

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[0] = backup_yv12[i];
  }

  // calculate the bit cost on motion vector
  mvp_full.row = tmp_mv->as_mv.row * 8;
  mvp_full.col = tmp_mv->as_mv.col * 8;
  *rate_mv = vp9_mv_bit_cost(&mvp_full, &ref_mv,
                             x->nmvjointcost, x->mvcost, MV_COST_WEIGHT);
}

static void sub_pixel_motion_search(VP9_COMP *cpi, MACROBLOCK *x,
                                    const TileInfo *const tile,
                                    BLOCK_SIZE bsize, int mi_row, int mi_col,
                                    MV *tmp_mv) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct buf_2d backup_yv12[MAX_MB_PLANE] = {{0}};
  int ref = mbmi->ref_frame[0];
  MV ref_mv = mbmi->ref_mvs[ref][0].as_mv;
  int dis;

  const YV12_BUFFER_CONFIG *scaled_ref_frame = vp9_get_scaled_ref_frame(cpi,
                                                                        ref);
  if (scaled_ref_frame) {
    int i;
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // motion search code to be used without additional modifications.
    for (i = 0; i < MAX_MB_PLANE; i++)
      backup_yv12[i] = xd->plane[i].pre[0];

    vp9_setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL);
  }

  cpi->find_fractional_mv_step(x, tmp_mv, &ref_mv,
                               cpi->common.allow_high_precision_mv,
                               x->errorperbit,
                               &cpi->fn_ptr[bsize],
                               cpi->sf.subpel_force_stop,
                               cpi->sf.subpel_iters_per_step,
                               x->nmvjointcost, x->mvcost,
                               &dis, &x->pred_sse[ref]);

  if (scaled_ref_frame) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; i++)
      xd->plane[i].pre[0] = backup_yv12[i];
  }

  x->pred_mv[ref].as_mv = *tmp_mv;
}

static void model_rd_for_sb_y(VP9_COMP *cpi, BLOCK_SIZE bsize,
                              MACROBLOCK *x, MACROBLOCKD *xd,
                              int *out_rate_sum, int64_t *out_dist_sum) {
  // Note our transform coeffs are 8 times an orthogonal transform.
  // Hence quantizer step is also 8 times. To get effective quantizer
  // we need to divide by 8 before sending to modeling function.
  unsigned int sse;
  int rate;
  int64_t dist;

  struct macroblock_plane *const p = &x->plane[0];
  struct macroblockd_plane *const pd = &xd->plane[0];

  int var = cpi->fn_ptr[bsize].vf(p->src.buf, p->src.stride,
                                  pd->dst.buf, pd->dst.stride, &sse);

  vp9_model_rd_from_var_lapndz(sse + var, 1 << num_pels_log2_lookup[bsize],
                               pd->dequant[1] >> 3, &rate, &dist);
  *out_rate_sum = rate;
  *out_dist_sum = dist << 3;
}

// TODO(jingning) placeholder for inter-frame non-RD mode decision.
// this needs various further optimizations. to be continued..
int64_t vp9_pick_inter_mode(VP9_COMP *cpi, MACROBLOCK *x,
                            const TileInfo *const tile,
                            int mi_row, int mi_col,
                            int *returnrate,
                            int64_t *returndistortion,
                            BLOCK_SIZE bsize) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct macroblock_plane *const p = &x->plane[0];
  struct macroblockd_plane *const pd = &xd->plane[0];
  MB_PREDICTION_MODE this_mode, best_mode = ZEROMV;
  MV_REFERENCE_FRAME ref_frame, best_ref_frame = LAST_FRAME;
  INTERP_FILTER best_pred_filter = EIGHTTAP;
  int_mv frame_mv[MB_MODE_COUNT][MAX_REF_FRAMES];
  struct buf_2d yv12_mb[4][MAX_MB_PLANE];
  static const int flag_list[4] = { 0, VP9_LAST_FLAG, VP9_GOLD_FLAG,
                                    VP9_ALT_FLAG };
  int64_t best_rd = INT64_MAX;
  int64_t this_rd = INT64_MAX;

  int rate = INT_MAX;
  int64_t dist = INT64_MAX;

  VP9_COMMON *cm = &cpi->common;
  int intra_cost_penalty = 20 * vp9_dc_quant(cm->base_qindex, cm->y_dc_delta_q);

  const int64_t inter_mode_thresh = RDCOST(x->rdmult, x->rddiv,
                                           intra_cost_penalty, 0);
  const int64_t intra_mode_cost = 50;

  unsigned char segment_id = mbmi->segment_id;
  const int *const rd_threshes = cpi->rd.threshes[segment_id][bsize];
  const int *const rd_thresh_freq_fact = cpi->rd.thresh_freq_fact[bsize];
  // Mode index conversion form THR_MODES to MB_PREDICTION_MODE for a ref frame.
  int mode_idx[MB_MODE_COUNT] = {0};
  INTERP_FILTER filter_ref = SWITCHABLE;

  x->skip_encode = cpi->sf.skip_encode_frame && x->q_index < QIDX_SKIP_THRESH;

  x->skip = 0;
  if (!x->in_active_map)
    x->skip = 1;
  // initialize mode decisions
  *returnrate = INT_MAX;
  *returndistortion = INT64_MAX;
  vpx_memset(mbmi, 0, sizeof(MB_MODE_INFO));
  mbmi->sb_type = bsize;
  mbmi->ref_frame[0] = NONE;
  mbmi->ref_frame[1] = NONE;
  mbmi->tx_size = MIN(max_txsize_lookup[bsize],
                      tx_mode_to_biggest_tx_size[cpi->common.tx_mode]);
  mbmi->interp_filter = cpi->common.interp_filter == SWITCHABLE ?
                        EIGHTTAP : cpi->common.interp_filter;
  mbmi->skip = 0;
  mbmi->segment_id = segment_id;

  for (ref_frame = LAST_FRAME; ref_frame <= LAST_FRAME ; ++ref_frame) {
    x->pred_mv_sad[ref_frame] = INT_MAX;
    if (cpi->ref_frame_flags & flag_list[ref_frame]) {
      vp9_setup_buffer_inter(cpi, x, tile,
                             ref_frame, bsize, mi_row, mi_col,
                             frame_mv[NEARESTMV], frame_mv[NEARMV], yv12_mb);
    }
    frame_mv[NEWMV][ref_frame].as_int = INVALID_MV;
    frame_mv[ZEROMV][ref_frame].as_int = 0;
  }

  if (xd->up_available)
    filter_ref = xd->mi[-xd->mi_stride]->mbmi.interp_filter;
  else if (xd->left_available)
    filter_ref = xd->mi[-1]->mbmi.interp_filter;

  for (ref_frame = LAST_FRAME; ref_frame <= LAST_FRAME ; ++ref_frame) {
    if (!(cpi->ref_frame_flags & flag_list[ref_frame]))
      continue;

    // Select prediction reference frames.
    xd->plane[0].pre[0] = yv12_mb[ref_frame][0];

    clamp_mv2(&frame_mv[NEARESTMV][ref_frame].as_mv, xd);
    clamp_mv2(&frame_mv[NEARMV][ref_frame].as_mv, xd);

    mbmi->ref_frame[0] = ref_frame;

    // Set conversion index for LAST_FRAME.
    if (ref_frame == LAST_FRAME) {
      mode_idx[NEARESTMV] = THR_NEARESTMV;   // LAST_FRAME, NEARESTMV
      mode_idx[NEARMV] = THR_NEARMV;         // LAST_FRAME, NEARMV
      mode_idx[ZEROMV] = THR_ZEROMV;         // LAST_FRAME, ZEROMV
      mode_idx[NEWMV] = THR_NEWMV;           // LAST_FRAME, NEWMV
    }

    for (this_mode = NEARESTMV; this_mode <= NEWMV; ++this_mode) {
      int rate_mv = 0;

      if (cpi->sf.disable_inter_mode_mask[bsize] &
          (1 << INTER_OFFSET(this_mode)))
        continue;

      if (best_rd < ((int64_t)rd_threshes[mode_idx[this_mode]] *
          rd_thresh_freq_fact[this_mode] >> 5) ||
          rd_threshes[mode_idx[this_mode]] == INT_MAX)
        continue;

      if (this_mode == NEWMV) {
        int rate_mode = 0;
        if (this_rd < (int64_t)(1 << num_pels_log2_lookup[bsize]))
          continue;

        full_pixel_motion_search(cpi, x, tile, bsize, mi_row, mi_col,
                                 &frame_mv[NEWMV][ref_frame], &rate_mv);

        if (frame_mv[NEWMV][ref_frame].as_int == INVALID_MV)
          continue;

        rate_mode = x->inter_mode_cost[mbmi->mode_context[ref_frame]]
                                      [INTER_OFFSET(this_mode)];
        if (RDCOST(x->rdmult, x->rddiv, rate_mv + rate_mode, 0) > best_rd)
          continue;

        sub_pixel_motion_search(cpi, x, tile, bsize, mi_row, mi_col,
                                &frame_mv[NEWMV][ref_frame].as_mv);
      }

      if (this_mode != NEARESTMV)
        if (frame_mv[this_mode][ref_frame].as_int ==
            frame_mv[NEARESTMV][ref_frame].as_int)
          continue;

      mbmi->mode = this_mode;
      mbmi->mv[0].as_int = frame_mv[this_mode][ref_frame].as_int;

      // Search for the best prediction filter type, when the resulting
      // motion vector is at sub-pixel accuracy level for luma component, i.e.,
      // the last three bits are all zeros.
      if ((this_mode == NEWMV || filter_ref == SWITCHABLE) &&
          ((mbmi->mv[0].as_mv.row & 0x07) != 0 ||
           (mbmi->mv[0].as_mv.col & 0x07) != 0)) {
        int64_t tmp_rdcost1 = INT64_MAX;
        int64_t tmp_rdcost2 = INT64_MAX;
        int64_t tmp_rdcost3 = INT64_MAX;
        int pf_rate[3];
        int64_t pf_dist[3];

        mbmi->interp_filter = EIGHTTAP;
        vp9_build_inter_predictors_sby(xd, mi_row, mi_col, bsize);
        model_rd_for_sb_y(cpi, bsize, x, xd, &pf_rate[EIGHTTAP],
                          &pf_dist[EIGHTTAP]);
        tmp_rdcost1 = RDCOST(x->rdmult, x->rddiv,
                             vp9_get_switchable_rate(x) + pf_rate[EIGHTTAP],
                             pf_dist[EIGHTTAP]);

        mbmi->interp_filter = EIGHTTAP_SHARP;
        vp9_build_inter_predictors_sby(xd, mi_row, mi_col, bsize);
        model_rd_for_sb_y(cpi, bsize, x, xd, &pf_rate[EIGHTTAP_SHARP],
                          &pf_dist[EIGHTTAP_SHARP]);
        tmp_rdcost2 = RDCOST(x->rdmult, x->rddiv,
                          vp9_get_switchable_rate(x) + pf_rate[EIGHTTAP_SHARP],
                          pf_dist[EIGHTTAP_SHARP]);

        mbmi->interp_filter = EIGHTTAP_SMOOTH;
        vp9_build_inter_predictors_sby(xd, mi_row, mi_col, bsize);
        model_rd_for_sb_y(cpi, bsize, x, xd, &pf_rate[EIGHTTAP_SMOOTH],
                          &pf_dist[EIGHTTAP_SMOOTH]);
        tmp_rdcost3 = RDCOST(x->rdmult, x->rddiv,
                          vp9_get_switchable_rate(x) + pf_rate[EIGHTTAP_SMOOTH],
                          pf_dist[EIGHTTAP_SMOOTH]);

        if (tmp_rdcost2 < tmp_rdcost1) {
          if (tmp_rdcost2 < tmp_rdcost3)
            mbmi->interp_filter = EIGHTTAP_SHARP;
          else
            mbmi->interp_filter = EIGHTTAP_SMOOTH;
        } else {
          if (tmp_rdcost1 < tmp_rdcost3)
            mbmi->interp_filter = EIGHTTAP;
          else
            mbmi->interp_filter = EIGHTTAP_SMOOTH;
        }

        rate = pf_rate[mbmi->interp_filter];
        dist = pf_dist[mbmi->interp_filter];
      } else {
        mbmi->interp_filter = (filter_ref == SWITCHABLE) ? EIGHTTAP: filter_ref;
        vp9_build_inter_predictors_sby(xd, mi_row, mi_col, bsize);
        model_rd_for_sb_y(cpi, bsize, x, xd, &rate, &dist);
      }

      rate += rate_mv;
      rate += x->inter_mode_cost[mbmi->mode_context[ref_frame]]
                                [INTER_OFFSET(this_mode)];
      this_rd = RDCOST(x->rdmult, x->rddiv, rate, dist);

      if (this_rd < best_rd) {
        best_rd = this_rd;
        *returnrate = rate;
        *returndistortion = dist;
        best_mode = this_mode;
        best_pred_filter = mbmi->interp_filter;
        best_ref_frame = ref_frame;
      }
    }
  }

  mbmi->mode = best_mode;
  mbmi->interp_filter = best_pred_filter;
  mbmi->ref_frame[0] = best_ref_frame;
  mbmi->mv[0].as_int = frame_mv[best_mode][best_ref_frame].as_int;
  xd->mi[0]->bmi[0].as_mv[0].as_int = mbmi->mv[0].as_int;

  // Perform intra prediction search, if the best SAD is above a certain
  // threshold.
  if (best_rd > inter_mode_thresh) {
    for (this_mode = DC_PRED; this_mode <= DC_PRED; ++this_mode) {
      vp9_predict_intra_block(xd, 0, b_width_log2(bsize),
                              mbmi->tx_size, this_mode,
                              &p->src.buf[0], p->src.stride,
                              &pd->dst.buf[0], pd->dst.stride, 0, 0, 0);

      model_rd_for_sb_y(cpi, bsize, x, xd, &rate, &dist);
      rate += x->mbmode_cost[this_mode];
      rate += intra_cost_penalty;
      this_rd = RDCOST(x->rdmult, x->rddiv, rate, dist);

      if (this_rd + intra_mode_cost < best_rd) {
        best_rd = this_rd;
        *returnrate = rate;
        *returndistortion = dist;
        mbmi->mode = this_mode;
        mbmi->ref_frame[0] = INTRA_FRAME;
        mbmi->uv_mode = this_mode;
        mbmi->mv[0].as_int = INVALID_MV;
      }
    }
  }

  return INT64_MAX;
}
