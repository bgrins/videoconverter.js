/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>

#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_speed_features.h"

#define ALL_INTRA_MODES ((1 << DC_PRED) | \
                         (1 << V_PRED) | (1 << H_PRED) | \
                         (1 << D45_PRED) | (1 << D135_PRED) | \
                         (1 << D117_PRED) | (1 << D153_PRED) | \
                         (1 << D207_PRED) | (1 << D63_PRED) | \
                         (1 << TM_PRED))
#define INTRA_DC_ONLY   (1 << DC_PRED)
#define INTRA_DC_TM     ((1 << TM_PRED) | (1 << DC_PRED))
#define INTRA_DC_H_V    ((1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED))
#define INTRA_DC_TM_H_V (INTRA_DC_TM | (1 << V_PRED) | (1 << H_PRED))

// Masks for partially or completely disabling split mode
#define DISABLE_ALL_INTER_SPLIT   ((1 << THR_COMP_GA) | \
                                   (1 << THR_COMP_LA) | \
                                   (1 << THR_ALTR) | \
                                   (1 << THR_GOLD) | \
                                   (1 << THR_LAST))

#define DISABLE_ALL_SPLIT         ((1 << THR_INTRA) | DISABLE_ALL_INTER_SPLIT)

#define DISABLE_COMPOUND_SPLIT    ((1 << THR_COMP_GA) | (1 << THR_COMP_LA))

#define LAST_AND_INTRA_SPLIT_ONLY ((1 << THR_COMP_GA) | \
                                   (1 << THR_COMP_LA) | \
                                   (1 << THR_ALTR) | \
                                   (1 << THR_GOLD))

static void set_good_speed_feature(VP9_COMP *cpi, VP9_COMMON *cm,
                                   SPEED_FEATURES *sf, int speed) {
  sf->adaptive_rd_thresh = 1;
  sf->recode_loop = (speed < 1) ? ALLOW_RECODE : ALLOW_RECODE_KFMAXBW;
  sf->allow_skip_recode = 1;

  if (speed >= 1) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check  = 1;
    sf->tx_size_search_method = frame_is_boosted(cpi) ? USE_FULL_RD
                                                      : USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = DISABLE_COMPOUND_SPLIT;
    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->auto_mv_step_size = 1;
    sf->adaptive_rd_thresh = 2;
    sf->subpel_iters_per_step = 1;
    sf->mode_skip_start = 10;
    sf->adaptive_pred_interp_filter = 1;

    sf->recode_loop = ALLOW_RECODE_KFARFGF;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_y_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;
  }

  if (speed >= 2) {
    sf->tx_size_search_method = frame_is_intra_only(cm) ? USE_FULL_RD
                                                        : USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = LAST_AND_INTRA_SPLIT_ONLY;

    sf->adaptive_pred_interp_filter = 2;
    sf->reference_masking = 1;
    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH |
                                 FLAG_SKIP_INTRA_BESTINTER |
                                 FLAG_SKIP_COMP_BESTINTRA |
                                 FLAG_SKIP_INTRA_LOWVAR;
    sf->disable_filter_search_var_thresh = 100;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;
    sf->auto_min_max_partition_size = RELAXED_NEIGHBORING_MIN_MAX;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_LOW_MOTION;
    sf->adjust_partitioning_from_last_frame = 1;
    sf->last_partitioning_redo_frequency = 3;
  }

  if (speed >= 3) {
    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = DISABLE_ALL_SPLIT;
    else
      sf->disable_split_mask = DISABLE_ALL_INTER_SPLIT;

    sf->recode_loop = ALLOW_RECODE_KFMAXBW;
    sf->adaptive_rd_thresh = 3;
    sf->mode_skip_start = 6;
    sf->use_fast_coef_updates = ONE_LOOP_REDUCED;
    sf->use_fast_coef_costing = 1;
  }

  if (speed >= 4) {
    sf->use_square_partition_only = 1;
    sf->tx_size_search_method = USE_LARGESTALL;
    sf->disable_split_mask = DISABLE_ALL_SPLIT;
    sf->adaptive_rd_thresh = 4;
    sf->mode_search_skip_flags |= FLAG_SKIP_COMP_REFMISMATCH |
                                  FLAG_EARLY_TERMINATE;
    sf->disable_filter_search_var_thresh = 200;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_ALL;
    sf->use_lp32x32fdct = 1;
  }

  if (speed >= 5) {
    int i;

    sf->partition_search_type = FIXED_PARTITION;
    sf->optimize_coefficients = 0;
    sf->search_method = HEX;
    sf->disable_filter_search_var_thresh = 500;
    for (i = 0; i < TX_SIZES; ++i) {
      sf->intra_y_mode_mask[i] = INTRA_DC_ONLY;
      sf->intra_uv_mode_mask[i] = INTRA_DC_ONLY;
    }
    cpi->allow_encode_breakout = ENCODE_BREAKOUT_ENABLED;
  }
}

static void set_rt_speed_feature(VP9_COMMON *cm, SPEED_FEATURES *sf,
                                 int speed) {
  sf->static_segmentation = 0;
  sf->adaptive_rd_thresh = 1;
  sf->encode_breakout_thresh = 1;
  sf->use_fast_coef_costing = 1;

  if (speed == 1) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check = 1;
    sf->tx_size_search_method = frame_is_intra_only(cm) ? USE_FULL_RD
                                                        : USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ? DISABLE_ALL_SPLIT
                                              : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = DISABLE_COMPOUND_SPLIT;

    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->adaptive_pred_interp_filter = 1;
    sf->auto_mv_step_size = 1;
    sf->adaptive_rd_thresh = 2;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->encode_breakout_thresh = 8;
  }

  if (speed >= 2) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check = 1;
    sf->tx_size_search_method = frame_is_intra_only(cm) ? USE_FULL_RD
                                                        : USE_LARGESTALL;
    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ?
        DISABLE_ALL_SPLIT : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = LAST_AND_INTRA_SPLIT_ONLY;

    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH |
                                 FLAG_SKIP_INTRA_BESTINTER |
                                 FLAG_SKIP_COMP_BESTINTRA |
                                 FLAG_SKIP_INTRA_LOWVAR;
    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->adaptive_pred_interp_filter = 2;
    sf->auto_mv_step_size = 1;
    sf->reference_masking = 1;

    sf->disable_filter_search_var_thresh = 50;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;

    sf->auto_min_max_partition_size = RELAXED_NEIGHBORING_MIN_MAX;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_LOW_MOTION;
    sf->adjust_partitioning_from_last_frame = 1;
    sf->last_partitioning_redo_frequency = 3;

    sf->adaptive_rd_thresh = 2;
    sf->use_lp32x32fdct = 1;
    sf->mode_skip_start = 11;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_y_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->encode_breakout_thresh = 200;
  }

  if (speed >= 3) {
    sf->use_square_partition_only = 1;
    sf->disable_filter_search_var_thresh = 100;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_ALL;
    sf->constrain_copy_partition = 1;
    sf->use_uv_intra_rd_estimate = 1;
    sf->skip_encode_sb = 1;
    sf->subpel_iters_per_step = 1;
    sf->use_fast_coef_updates = ONE_LOOP_REDUCED;
    sf->adaptive_rd_thresh = 4;
    sf->mode_skip_start = 6;
    sf->allow_skip_recode = 0;
    sf->optimize_coefficients = 0;
    sf->disable_split_mask = DISABLE_ALL_SPLIT;
    sf->lpf_pick = LPF_PICK_FROM_Q;
    sf->encode_breakout_thresh = 700;
  }

  if (speed >= 4) {
    int i;
    sf->last_partitioning_redo_frequency = 4;
    sf->adaptive_rd_thresh = 5;
    sf->use_fast_coef_costing = 0;
    sf->auto_min_max_partition_size = STRICT_NEIGHBORING_MIN_MAX;
    sf->adjust_partitioning_from_last_frame =
        cm->last_frame_type != cm->frame_type || (0 ==
        (cm->current_video_frame + 1) % sf->last_partitioning_redo_frequency);
    sf->subpel_force_stop = 1;
    for (i = 0; i < TX_SIZES; i++) {
      sf->intra_y_mode_mask[i] = INTRA_DC_H_V;
      sf->intra_uv_mode_mask[i] = INTRA_DC_ONLY;
    }
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_ONLY;
    sf->frame_parameter_update = 0;
    sf->encode_breakout_thresh = 1000;
    sf->search_method = FAST_HEX;
    sf->disable_inter_mode_mask[BLOCK_32X32] = 1 << INTER_OFFSET(ZEROMV);
    sf->disable_inter_mode_mask[BLOCK_32X64] = ~(1 << INTER_OFFSET(NEARESTMV));
    sf->disable_inter_mode_mask[BLOCK_64X32] = ~(1 << INTER_OFFSET(NEARESTMV));
    sf->disable_inter_mode_mask[BLOCK_64X64] = ~(1 << INTER_OFFSET(NEARESTMV));
    sf->max_intra_bsize = BLOCK_32X32;
    sf->allow_skip_recode = 1;
  }

  if (speed >= 5) {
    sf->max_partition_size = BLOCK_32X32;
    sf->min_partition_size = BLOCK_8X8;
    sf->partition_check =
        (cm->current_video_frame % sf->last_partitioning_redo_frequency == 1);
    sf->force_frame_boost = cm->frame_type == KEY_FRAME ||
        (cm->current_video_frame %
            (sf->last_partitioning_redo_frequency << 1) == 1);
    sf->max_delta_qindex = (cm->frame_type == KEY_FRAME) ? 20 : 15;
    sf->partition_search_type = REFERENCE_PARTITION;
    sf->use_nonrd_pick_mode = 1;
    sf->search_method = FAST_DIAMOND;
    sf->allow_skip_recode = 0;
  }

  if (speed >= 6) {
    // Adaptively switch between SOURCE_VAR_BASED_PARTITION and FIXED_PARTITION.
    sf->partition_search_type = SOURCE_VAR_BASED_PARTITION;
    sf->search_type_check_frequency = 50;
    sf->source_var_thresh = 360;
  }

  if (speed >= 7) {
    int i;
    for (i = 0; i < BLOCK_SIZES; ++i)
      sf->disable_inter_mode_mask[i] = ~(1 << INTER_OFFSET(NEARESTMV));
  }
}

void vp9_set_speed_features(VP9_COMP *cpi) {
  SPEED_FEATURES *const sf = &cpi->sf;
  VP9_COMMON *const cm = &cpi->common;
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  const int speed = cpi->speed < 0 ? -cpi->speed : cpi->speed;
  int i;

  // best quality defaults
  sf->frame_parameter_update = 1;
  sf->search_method = NSTEP;
  sf->recode_loop = ALLOW_RECODE;
  sf->subpel_search_method = SUBPEL_TREE;
  sf->subpel_iters_per_step = 2;
  sf->subpel_force_stop = 0;
  sf->optimize_coefficients = !oxcf->lossless;
  sf->reduce_first_step_size = 0;
  sf->auto_mv_step_size = 0;
  sf->max_step_search_steps = MAX_MVSEARCH_STEPS;
  sf->comp_inter_joint_search_thresh = BLOCK_4X4;
  sf->adaptive_rd_thresh = 0;
  sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_OFF;
  sf->tx_size_search_method = USE_FULL_RD;
  sf->use_lp32x32fdct = 0;
  sf->adaptive_motion_search = 0;
  sf->adaptive_pred_interp_filter = 0;
  sf->reference_masking = 0;
  sf->partition_search_type = SEARCH_PARTITION;
  sf->less_rectangular_check = 0;
  sf->use_square_partition_only = 0;
  sf->auto_min_max_partition_size = NOT_IN_USE;
  sf->max_partition_size = BLOCK_64X64;
  sf->min_partition_size = BLOCK_4X4;
  sf->adjust_partitioning_from_last_frame = 0;
  sf->last_partitioning_redo_frequency = 4;
  sf->constrain_copy_partition = 0;
  sf->disable_split_mask = 0;
  sf->mode_search_skip_flags = 0;
  sf->force_frame_boost = 0;
  sf->max_delta_qindex = 0;
  sf->disable_split_var_thresh = 0;
  sf->disable_filter_search_var_thresh = 0;
  for (i = 0; i < TX_SIZES; i++) {
    sf->intra_y_mode_mask[i] = ALL_INTRA_MODES;
    sf->intra_uv_mode_mask[i] = ALL_INTRA_MODES;
  }
  sf->use_rd_breakout = 0;
  sf->skip_encode_sb = 0;
  sf->use_uv_intra_rd_estimate = 0;
  sf->allow_skip_recode = 0;
  sf->lpf_pick = LPF_PICK_FROM_FULL_IMAGE;
  sf->use_fast_coef_updates = TWO_LOOP;
  sf->use_fast_coef_costing = 0;
  sf->mode_skip_start = MAX_MODES;  // Mode index at which mode skip mask set
  sf->use_nonrd_pick_mode = 0;
  sf->encode_breakout_thresh = 0;
  for (i = 0; i < BLOCK_SIZES; ++i)
    sf->disable_inter_mode_mask[i] = 0;
  sf->max_intra_bsize = BLOCK_64X64;
  // This setting only takes effect when partition_search_type is set
  // to FIXED_PARTITION.
  sf->always_this_block_size = BLOCK_16X16;
  sf->search_type_check_frequency = 50;
  sf->source_var_thresh = 100;

  // Recode loop tolerence %.
  sf->recode_tolerance = 25;

  switch (oxcf->mode) {
    case MODE_BESTQUALITY:
    case MODE_SECONDPASS_BEST:  // This is the best quality mode.
      cpi->diamond_search_sad = vp9_full_range_search;
      break;
    case MODE_FIRSTPASS:
    case MODE_GOODQUALITY:
    case MODE_SECONDPASS:
      set_good_speed_feature(cpi, cm, sf, speed);
      break;
    case MODE_REALTIME:
      set_rt_speed_feature(cm, sf, speed);
      break;
  }

  // Slow quant, dct and trellis not worthwhile for first pass
  // so make sure they are always turned off.
  if (cpi->pass == 1)
    sf->optimize_coefficients = 0;

  // No recode for 1 pass.
  if (cpi->pass == 0) {
    sf->recode_loop = DISALLOW_RECODE;
    sf->optimize_coefficients = 0;
  }

  if (sf->subpel_search_method == SUBPEL_TREE) {
    cpi->find_fractional_mv_step = vp9_find_best_sub_pixel_tree;
    cpi->find_fractional_mv_step_comp = vp9_find_best_sub_pixel_comp_tree;
  }

  cpi->mb.optimize = sf->optimize_coefficients == 1 && cpi->pass != 1;

  if (cpi->encode_breakout && oxcf->mode == MODE_REALTIME &&
      sf->encode_breakout_thresh > cpi->encode_breakout)
    cpi->encode_breakout = sf->encode_breakout_thresh;

  if (sf->disable_split_mask == DISABLE_ALL_SPLIT)
    sf->adaptive_pred_interp_filter = 0;

  if (!cpi->oxcf.frame_periodic_boost) {
    sf->max_delta_qindex = 0;
  }
}
