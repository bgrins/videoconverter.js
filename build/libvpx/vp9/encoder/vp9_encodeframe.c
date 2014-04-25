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
#include <math.h>
#include <stdio.h>

#include "./vp9_rtcd.h"
#include "./vpx_config.h"

#include "vpx_ports/vpx_timer.h"

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_entropy.h"
#include "vp9/common/vp9_entropymode.h"
#include "vp9/common/vp9_idct.h"
#include "vp9/common/vp9_mvref_common.h"
#include "vp9/common/vp9_pred_common.h"
#include "vp9/common/vp9_quant_common.h"
#include "vp9/common/vp9_reconintra.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_seg_common.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/common/vp9_tile_common.h"

#include "vp9/encoder/vp9_aq_complexity.h"
#include "vp9/encoder/vp9_aq_cyclicrefresh.h"
#include "vp9/encoder/vp9_aq_variance.h"
#include "vp9/encoder/vp9_encodeframe.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/encoder/vp9_extend.h"
#include "vp9/encoder/vp9_pickmode.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_segmentation.h"
#include "vp9/encoder/vp9_tokenize.h"

#define GF_ZEROMV_ZBIN_BOOST 0
#define LF_ZEROMV_ZBIN_BOOST 0
#define MV_ZBIN_BOOST        0
#define SPLIT_MV_ZBIN_BOOST  0
#define INTRA_ZBIN_BOOST     0

static INLINE uint8_t *get_sb_index(MACROBLOCK *x, BLOCK_SIZE subsize) {
  switch (subsize) {
    case BLOCK_64X64:
    case BLOCK_64X32:
    case BLOCK_32X64:
    case BLOCK_32X32:
      return &x->sb_index;
    case BLOCK_32X16:
    case BLOCK_16X32:
    case BLOCK_16X16:
      return &x->mb_index;
    case BLOCK_16X8:
    case BLOCK_8X16:
    case BLOCK_8X8:
      return &x->b_index;
    case BLOCK_8X4:
    case BLOCK_4X8:
    case BLOCK_4X4:
      return &x->ab_index;
    default:
      assert(0);
      return NULL;
  }
}

static void encode_superblock(VP9_COMP *cpi, TOKENEXTRA **t, int output_enabled,
                              int mi_row, int mi_col, BLOCK_SIZE bsize);

static void adjust_act_zbin(VP9_COMP *cpi, MACROBLOCK *x);

// activity_avg must be positive, or flat regions could get a zero weight
//  (infinite lambda), which confounds analysis.
// This also avoids the need for divide by zero checks in
//  vp9_activity_masking().
#define ACTIVITY_AVG_MIN 64

// Motion vector component magnitude threshold for defining fast motion.
#define FAST_MOTION_MV_THRESH 24

// This is used as a reference when computing the source variance for the
//  purposes of activity masking.
// Eventually this should be replaced by custom no-reference routines,
//  which will be faster.
static const uint8_t VP9_VAR_OFFS[64] = {
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128
};

static unsigned int get_sby_perpixel_variance(VP9_COMP *cpi,
                                              const struct buf_2d *ref,
                                              BLOCK_SIZE bs) {
  unsigned int sse;
  const unsigned int var = cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                                              VP9_VAR_OFFS, 0, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

static unsigned int get_sby_perpixel_diff_variance(VP9_COMP *cpi,
                                                   const struct buf_2d *ref,
                                                   int mi_row, int mi_col,
                                                   BLOCK_SIZE bs) {
  const YV12_BUFFER_CONFIG *last = get_ref_frame_buffer(cpi, LAST_FRAME);
  const uint8_t* last_y = &last->y_buffer[mi_row * MI_SIZE * last->y_stride +
                                              mi_col * MI_SIZE];
  unsigned int sse;
  const unsigned int var = cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                                              last_y, last->y_stride, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

static BLOCK_SIZE get_rd_var_based_fixed_partition(VP9_COMP *cpi,
                                                   int mi_row,
                                                   int mi_col) {
  unsigned int var = get_sby_perpixel_diff_variance(cpi, &cpi->mb.plane[0].src,
                                                    mi_row, mi_col,
                                                    BLOCK_64X64);
  if (var < 8)
    return BLOCK_64X64;
  else if (var < 128)
    return BLOCK_32X32;
  else if (var < 2048)
    return BLOCK_16X16;
  else
    return BLOCK_8X8;
}

static BLOCK_SIZE get_nonrd_var_based_fixed_partition(VP9_COMP *cpi,
                                                      int mi_row,
                                                      int mi_col) {
  unsigned int var = get_sby_perpixel_diff_variance(cpi, &cpi->mb.plane[0].src,
                                                    mi_row, mi_col,
                                                    BLOCK_64X64);
  if (var < 4)
    return BLOCK_64X64;
  else if (var < 10)
    return BLOCK_32X32;
  else
    return BLOCK_16X16;
}

// Lighter version of set_offsets that only sets the mode info
// pointers.
static INLINE void set_modeinfo_offsets(VP9_COMMON *const cm,
                                        MACROBLOCKD *const xd,
                                        int mi_row,
                                        int mi_col) {
  const int idx_str = xd->mi_stride * mi_row + mi_col;
  xd->mi = cm->mi_grid_visible + idx_str;
  xd->mi[0] = cm->mi + idx_str;
}

static int is_block_in_mb_map(const VP9_COMP *cpi, int mi_row, int mi_col,
                              BLOCK_SIZE bsize) {
  const VP9_COMMON *const cm = &cpi->common;
  const int mb_rows = cm->mb_rows;
  const int mb_cols = cm->mb_cols;
  const int mb_row = mi_row >> 1;
  const int mb_col = mi_col >> 1;
  const int mb_width = num_8x8_blocks_wide_lookup[bsize] >> 1;
  const int mb_height = num_8x8_blocks_high_lookup[bsize] >> 1;
  int r, c;
  if (bsize <= BLOCK_16X16) {
    return cpi->active_map[mb_row * mb_cols + mb_col];
  }
  for (r = 0; r < mb_height; ++r) {
    for (c = 0; c < mb_width; ++c) {
      int row = mb_row + r;
      int col = mb_col + c;
      if (row >= mb_rows || col >= mb_cols)
        continue;
      if (cpi->active_map[row * mb_cols + col])
        return 1;
    }
  }
  return 0;
}

static int check_active_map(const VP9_COMP *cpi, const MACROBLOCK *x,
                            int mi_row, int mi_col,
                            BLOCK_SIZE bsize) {
  if (cpi->active_map_enabled && !x->e_mbd.lossless) {
    return is_block_in_mb_map(cpi, mi_row, mi_col, bsize);
  } else {
    return 1;
  }
}

static void set_offsets(VP9_COMP *cpi, const TileInfo *const tile,
                        int mi_row, int mi_col, BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  const int mb_row = mi_row >> 1;
  const int mb_col = mi_col >> 1;
  const int idx_map = mb_row * cm->mb_cols + mb_col;
  const struct segmentation *const seg = &cm->seg;

  set_skip_context(xd, mi_row, mi_col);

  // Activity map pointer
  x->mb_activity_ptr = &cpi->mb_activity_map[idx_map];
  x->in_active_map = check_active_map(cpi, x, mi_row, mi_col, bsize);

  set_modeinfo_offsets(cm, xd, mi_row, mi_col);

  mbmi = &xd->mi[0]->mbmi;

  // Set up destination pointers.
  vp9_setup_dst_planes(xd, get_frame_new_buffer(cm), mi_row, mi_col);

  // Set up limit values for MV components.
  // Mv beyond the range do not produce new/different prediction block.
  x->mv_row_min = -(((mi_row + mi_height) * MI_SIZE) + VP9_INTERP_EXTEND);
  x->mv_col_min = -(((mi_col + mi_width) * MI_SIZE) + VP9_INTERP_EXTEND);
  x->mv_row_max = (cm->mi_rows - mi_row) * MI_SIZE + VP9_INTERP_EXTEND;
  x->mv_col_max = (cm->mi_cols - mi_col) * MI_SIZE + VP9_INTERP_EXTEND;

  // Set up distance of MB to edge of frame in 1/8th pel units.
  assert(!(mi_col & (mi_width - 1)) && !(mi_row & (mi_height - 1)));
  set_mi_row_col(xd, tile, mi_row, mi_height, mi_col, mi_width,
                 cm->mi_rows, cm->mi_cols);

  // Set up source buffers.
  vp9_setup_src_planes(x, cpi->Source, mi_row, mi_col);

  // R/D setup.
  x->rddiv = cpi->rd.RDDIV;
  x->rdmult = cpi->rd.RDMULT;

  // Setup segment ID.
  if (seg->enabled) {
    if (cpi->oxcf.aq_mode != VARIANCE_AQ) {
      const uint8_t *const map = seg->update_map ? cpi->segmentation_map
                                                 : cm->last_frame_seg_map;
      mbmi->segment_id = vp9_get_segment_id(cm, map, bsize, mi_row, mi_col);
    }
    vp9_init_plane_quantizers(cpi, x);

    x->encode_breakout = cpi->segment_encode_breakout[mbmi->segment_id];
  } else {
    mbmi->segment_id = 0;
    x->encode_breakout = cpi->encode_breakout;
  }
}

static void duplicate_mode_info_in_sb(VP9_COMMON * const cm,
                                     MACROBLOCKD *const xd,
                                     int mi_row,
                                     int mi_col,
                                     BLOCK_SIZE bsize) {
  const int block_width = num_8x8_blocks_wide_lookup[bsize];
  const int block_height = num_8x8_blocks_high_lookup[bsize];
  int i, j;
  for (j = 0; j < block_height; ++j)
    for (i = 0; i < block_width; ++i) {
      if (mi_row + j < cm->mi_rows && mi_col + i < cm->mi_cols)
        xd->mi[j * xd->mi_stride + i] = xd->mi[0];
    }
}

static void set_block_size(VP9_COMP * const cpi,
                           const TileInfo *const tile,
                           int mi_row, int mi_col,
                           BLOCK_SIZE bsize) {
  if (cpi->common.mi_cols > mi_col && cpi->common.mi_rows > mi_row) {
    MACROBLOCKD *const xd = &cpi->mb.e_mbd;
    set_modeinfo_offsets(&cpi->common, xd, mi_row, mi_col);
    xd->mi[0]->mbmi.sb_type = bsize;
    duplicate_mode_info_in_sb(&cpi->common, xd, mi_row, mi_col, bsize);
  }
}

typedef struct {
  int64_t sum_square_error;
  int64_t sum_error;
  int count;
  int variance;
} var;

typedef struct {
  var none;
  var horz[2];
  var vert[2];
} partition_variance;

typedef struct {
  partition_variance part_variances;
  var split[4];
} v8x8;

typedef struct {
  partition_variance part_variances;
  v8x8 split[4];
} v16x16;

typedef struct {
  partition_variance part_variances;
  v16x16 split[4];
} v32x32;

typedef struct {
  partition_variance part_variances;
  v32x32 split[4];
} v64x64;

typedef struct {
  partition_variance *part_variances;
  var *split[4];
} variance_node;

typedef enum {
  V16X16,
  V32X32,
  V64X64,
} TREE_LEVEL;

static void tree_to_node(void *data, BLOCK_SIZE bsize, variance_node *node) {
  int i;
  switch (bsize) {
    case BLOCK_64X64: {
      v64x64 *vt = (v64x64 *) data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    case BLOCK_32X32: {
      v32x32 *vt = (v32x32 *) data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    case BLOCK_16X16: {
      v16x16 *vt = (v16x16 *) data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i].part_variances.none;
      break;
    }
    case BLOCK_8X8: {
      v8x8 *vt = (v8x8 *) data;
      node->part_variances = &vt->part_variances;
      for (i = 0; i < 4; i++)
        node->split[i] = &vt->split[i];
      break;
    }
    default: {
      assert(0);
    }
  }
}

// Set variance values given sum square error, sum error, count.
static void fill_variance(int64_t s2, int64_t s, int c, var *v) {
  v->sum_square_error = s2;
  v->sum_error = s;
  v->count = c;
  if (c > 0)
    v->variance = (int)(256 *
                        (v->sum_square_error - v->sum_error * v->sum_error /
                         v->count) / v->count);
  else
    v->variance = 0;
}

void sum_2_variances(const var *a, const var *b, var *r) {
  fill_variance(a->sum_square_error + b->sum_square_error,
                a->sum_error + b->sum_error, a->count + b->count, r);
}

static void fill_variance_tree(void *data, BLOCK_SIZE bsize) {
  variance_node node;
  tree_to_node(data, bsize, &node);
  sum_2_variances(node.split[0], node.split[1], &node.part_variances->horz[0]);
  sum_2_variances(node.split[2], node.split[3], &node.part_variances->horz[1]);
  sum_2_variances(node.split[0], node.split[2], &node.part_variances->vert[0]);
  sum_2_variances(node.split[1], node.split[3], &node.part_variances->vert[1]);
  sum_2_variances(&node.part_variances->vert[0], &node.part_variances->vert[1],
                  &node.part_variances->none);
}

static int set_vt_partitioning(VP9_COMP *cpi,
                               void *data,
                               const TileInfo *const tile,
                               BLOCK_SIZE bsize,
                               int mi_row,
                               int mi_col,
                               int mi_size) {
  VP9_COMMON * const cm = &cpi->common;
  variance_node vt;
  const int block_width = num_8x8_blocks_wide_lookup[bsize];
  const int block_height = num_8x8_blocks_high_lookup[bsize];
  // TODO(debargha): Choose this more intelligently.
  const int64_t threshold_multiplier = 25;
  int64_t threshold = threshold_multiplier * cpi->common.base_qindex;
  assert(block_height == block_width);

  tree_to_node(data, bsize, &vt);

  // Split none is available only if we have more than half a block size
  // in width and height inside the visible image.
  if (mi_col + block_width / 2 < cm->mi_cols &&
      mi_row + block_height / 2 < cm->mi_rows &&
      vt.part_variances->none.variance < threshold) {
    set_block_size(cpi, tile, mi_row, mi_col, bsize);
    return 1;
  }

  // Vertical split is available on all but the bottom border.
  if (mi_row + block_height / 2 < cm->mi_rows &&
      vt.part_variances->vert[0].variance < threshold &&
      vt.part_variances->vert[1].variance < threshold) {
    BLOCK_SIZE subsize = get_subsize(bsize, PARTITION_VERT);
    set_block_size(cpi, tile, mi_row, mi_col, subsize);
    set_block_size(cpi, tile, mi_row, mi_col + block_width / 2, subsize);
    return 1;
  }

  // Horizontal split is available on all but the right border.
  if (mi_col + block_width / 2 < cm->mi_cols &&
      vt.part_variances->horz[0].variance < threshold &&
      vt.part_variances->horz[1].variance < threshold) {
    BLOCK_SIZE subsize = get_subsize(bsize, PARTITION_HORZ);
    set_block_size(cpi, tile, mi_row, mi_col, subsize);
    set_block_size(cpi, tile, mi_row + block_height / 2, mi_col, subsize);
    return 1;
  }
  return 0;
}

// TODO(debargha): Fix this function and make it work as expected.
static void choose_partitioning(VP9_COMP *cpi,
                                const TileInfo *const tile,
                                int mi_row, int mi_col) {
  VP9_COMMON * const cm = &cpi->common;
  MACROBLOCK *x = &cpi->mb;
  MACROBLOCKD *xd = &cpi->mb.e_mbd;

  int i, j, k;
  v64x64 vt;
  uint8_t *s;
  const uint8_t *d;
  int sp;
  int dp;
  int pixels_wide = 64, pixels_high = 64;
  int_mv nearest_mv, near_mv;
  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_buffer(cpi, LAST_FRAME);
  const struct scale_factors *const sf = &cm->frame_refs[LAST_FRAME - 1].sf;

  vp9_zero(vt);
  set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);

  if (xd->mb_to_right_edge < 0)
    pixels_wide += (xd->mb_to_right_edge >> 3);
  if (xd->mb_to_bottom_edge < 0)
    pixels_high += (xd->mb_to_bottom_edge >> 3);

  s = x->plane[0].src.buf;
  sp = x->plane[0].src.stride;

  if (cm->frame_type != KEY_FRAME) {
    vp9_setup_pre_planes(xd, 0, yv12, mi_row, mi_col, sf);

    xd->mi[0]->mbmi.ref_frame[0] = LAST_FRAME;
    xd->mi[0]->mbmi.sb_type = BLOCK_64X64;
    vp9_find_best_ref_mvs(xd, cm->allow_high_precision_mv,
                          xd->mi[0]->mbmi.ref_mvs[LAST_FRAME],
                          &nearest_mv, &near_mv);

    xd->mi[0]->mbmi.mv[0] = nearest_mv;
    vp9_build_inter_predictors_sby(xd, mi_row, mi_col, BLOCK_64X64);

    d = xd->plane[0].dst.buf;
    dp = xd->plane[0].dst.stride;
  } else {
    d = VP9_VAR_OFFS;
    dp = 0;
  }

  // Fill in the entire tree of 8x8 variances for splits.
  for (i = 0; i < 4; i++) {
    const int x32_idx = ((i & 1) << 5);
    const int y32_idx = ((i >> 1) << 5);
    for (j = 0; j < 4; j++) {
      const int x16_idx = x32_idx + ((j & 1) << 4);
      const int y16_idx = y32_idx + ((j >> 1) << 4);
      v16x16 *vst = &vt.split[i].split[j];
      for (k = 0; k < 4; k++) {
        int x_idx = x16_idx + ((k & 1) << 3);
        int y_idx = y16_idx + ((k >> 1) << 3);
        unsigned int sse = 0;
        int sum = 0;
        if (x_idx < pixels_wide && y_idx < pixels_high)
          vp9_get_sse_sum_8x8(s + y_idx * sp + x_idx, sp,
                              d + y_idx * dp + x_idx, dp, &sse, &sum);
        fill_variance(sse, sum, 64, &vst->split[k].part_variances.none);
      }
    }
  }
  // Fill the rest of the variance tree by summing split partition values.
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      fill_variance_tree(&vt.split[i].split[j], BLOCK_16X16);
    }
    fill_variance_tree(&vt.split[i], BLOCK_32X32);
  }
  fill_variance_tree(&vt, BLOCK_64X64);

  // Now go through the entire structure,  splitting every block size until
  // we get to one that's got a variance lower than our threshold,  or we
  // hit 8x8.
  if (!set_vt_partitioning(cpi, &vt, tile, BLOCK_64X64,
                           mi_row, mi_col, 8)) {
    for (i = 0; i < 4; ++i) {
      const int x32_idx = ((i & 1) << 2);
      const int y32_idx = ((i >> 1) << 2);
      if (!set_vt_partitioning(cpi, &vt.split[i], tile, BLOCK_32X32,
                               (mi_row + y32_idx), (mi_col + x32_idx), 4)) {
        for (j = 0; j < 4; ++j) {
          const int x16_idx = ((j & 1) << 1);
          const int y16_idx = ((j >> 1) << 1);
          // NOTE: This is a temporary hack to disable 8x8 partitions,
          // since it works really bad - possibly due to a bug
#define DISABLE_8X8_VAR_BASED_PARTITION
#ifdef DISABLE_8X8_VAR_BASED_PARTITION
          if (mi_row + y32_idx + y16_idx + 1 < cm->mi_rows &&
              mi_row + x32_idx + x16_idx + 1 < cm->mi_cols) {
            set_block_size(cpi, tile,
                           (mi_row + y32_idx + y16_idx),
                           (mi_col + x32_idx + x16_idx),
                           BLOCK_16X16);
          } else {
            for (k = 0; k < 4; ++k) {
              const int x8_idx = (k & 1);
              const int y8_idx = (k >> 1);
              set_block_size(cpi, tile,
                             (mi_row + y32_idx + y16_idx + y8_idx),
                             (mi_col + x32_idx + x16_idx + x8_idx),
                             BLOCK_8X8);
            }
          }
#else
          if (!set_vt_partitioning(cpi, &vt.split[i].split[j], tile,
                                   BLOCK_16X16,
                                   (mi_row + y32_idx + y16_idx),
                                   (mi_col + x32_idx + x16_idx), 2)) {
            for (k = 0; k < 4; ++k) {
              const int x8_idx = (k & 1);
              const int y8_idx = (k >> 1);
              set_block_size(cpi, tile,
                             (mi_row + y32_idx + y16_idx + y8_idx),
                             (mi_col + x32_idx + x16_idx + x8_idx),
                             BLOCK_8X8);
            }
          }
#endif
        }
      }
    }
  }
}

// Original activity measure from Tim T's code.
static unsigned int tt_activity_measure(MACROBLOCK *x) {
  unsigned int sse;
  // TODO: This could also be done over smaller areas (8x8), but that would
  // require extensive changes elsewhere, as lambda is assumed to be fixed
  // over an entire MB in most of the code.
  // Another option is to compute four 8x8 variances, and pick a single
  // lambda using a non-linear combination (e.g., the smallest, or second
  // smallest, etc.).
  const unsigned int act = vp9_variance16x16(x->plane[0].src.buf,
                                             x->plane[0].src.stride,
                                             VP9_VAR_OFFS, 0, &sse) << 4;
  // If the region is flat, lower the activity some more.
  return act < (8 << 12) ? MIN(act, 5 << 12) : act;
}

// Stub for alternative experimental activity measures.
static unsigned int alt_activity_measure(MACROBLOCK *x, int use_dc_pred) {
  return vp9_encode_intra(x, use_dc_pred);
}

// Measure the activity of the current macroblock
// What we measure here is TBD so abstracted to this function
#define ALT_ACT_MEASURE 1
static unsigned int mb_activity_measure(MACROBLOCK *x, int mb_row, int mb_col) {
  unsigned int mb_activity;

  if (ALT_ACT_MEASURE) {
    const int use_dc_pred = (mb_col || mb_row) && (!mb_col || !mb_row);

    // Or use and alternative.
    mb_activity = alt_activity_measure(x, use_dc_pred);
  } else {
    // Original activity measure from Tim T's code.
    mb_activity = tt_activity_measure(x);
  }

  return MAX(mb_activity, ACTIVITY_AVG_MIN);
}

// Calculate an "average" mb activity value for the frame
#define ACT_MEDIAN 0
static void calc_av_activity(VP9_COMP *cpi, int64_t activity_sum) {
#if ACT_MEDIAN
  // Find median: Simple n^2 algorithm for experimentation
  {
    unsigned int median;
    unsigned int i, j;
    unsigned int *sortlist;
    unsigned int tmp;

    // Create a list to sort to
    CHECK_MEM_ERROR(&cpi->common, sortlist, vpx_calloc(sizeof(unsigned int),
                    cpi->common.MBs));

    // Copy map to sort list
    vpx_memcpy(sortlist, cpi->mb_activity_map,
        sizeof(unsigned int) * cpi->common.MBs);

    // Ripple each value down to its correct position
    for (i = 1; i < cpi->common.MBs; i ++) {
      for (j = i; j > 0; j --) {
        if (sortlist[j] < sortlist[j - 1]) {
          // Swap values
          tmp = sortlist[j - 1];
          sortlist[j - 1] = sortlist[j];
          sortlist[j] = tmp;
        } else {
          break;
        }
      }
    }

    // Even number MBs so estimate median as mean of two either side.
    median = (1 + sortlist[cpi->common.MBs >> 1] +
        sortlist[(cpi->common.MBs >> 1) + 1]) >> 1;

    cpi->activity_avg = median;

    vpx_free(sortlist);
  }
#else
  // Simple mean for now
  cpi->activity_avg = (unsigned int) (activity_sum / cpi->common.MBs);
#endif  // ACT_MEDIAN

  if (cpi->activity_avg < ACTIVITY_AVG_MIN)
    cpi->activity_avg = ACTIVITY_AVG_MIN;

  // Experimental code: return fixed value normalized for several clips
  if (ALT_ACT_MEASURE)
    cpi->activity_avg = 100000;
}

#define USE_ACT_INDEX   0
#define OUTPUT_NORM_ACT_STATS   0

#if USE_ACT_INDEX
// Calculate an activity index for each mb
static void calc_activity_index(VP9_COMP *cpi, MACROBLOCK *x) {
  VP9_COMMON *const cm = &cpi->common;
  int mb_row, mb_col;

  int64_t act;
  int64_t a;
  int64_t b;

#if OUTPUT_NORM_ACT_STATS
  FILE *f = fopen("norm_act.stt", "a");
  fprintf(f, "\n%12d\n", cpi->activity_avg);
#endif

  // Reset pointers to start of activity map
  x->mb_activity_ptr = cpi->mb_activity_map;

  // Calculate normalized mb activity number.
  for (mb_row = 0; mb_row < cm->mb_rows; mb_row++) {
    // for each macroblock col in image
    for (mb_col = 0; mb_col < cm->mb_cols; mb_col++) {
      // Read activity from the map
      act = *(x->mb_activity_ptr);

      // Calculate a normalized activity number
      a = act + 4 * cpi->activity_avg;
      b = 4 * act + cpi->activity_avg;

      if (b >= a)
      *(x->activity_ptr) = (int)((b + (a >> 1)) / a) - 1;
      else
      *(x->activity_ptr) = 1 - (int)((a + (b >> 1)) / b);

#if OUTPUT_NORM_ACT_STATS
      fprintf(f, " %6d", *(x->mb_activity_ptr));
#endif
      // Increment activity map pointers
      x->mb_activity_ptr++;
    }

#if OUTPUT_NORM_ACT_STATS
    fprintf(f, "\n");
#endif
  }

#if OUTPUT_NORM_ACT_STATS
  fclose(f);
#endif
}
#endif  // USE_ACT_INDEX

// Loop through all MBs. Note activity of each, average activity and
// calculate a normalized activity for each
static void build_activity_map(VP9_COMP *cpi) {
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  VP9_COMMON *const cm = &cpi->common;

#if ALT_ACT_MEASURE
  YV12_BUFFER_CONFIG *new_yv12 = get_frame_new_buffer(cm);
  int recon_yoffset;
  int recon_y_stride = new_yv12->y_stride;
#endif

  int mb_row, mb_col;
  unsigned int mb_activity;
  int64_t activity_sum = 0;

  x->mb_activity_ptr = cpi->mb_activity_map;

  // for each macroblock row in image
  for (mb_row = 0; mb_row < cm->mb_rows; mb_row++) {
#if ALT_ACT_MEASURE
    // reset above block coeffs
    xd->up_available = (mb_row != 0);
    recon_yoffset = (mb_row * recon_y_stride * 16);
#endif
    // for each macroblock col in image
    for (mb_col = 0; mb_col < cm->mb_cols; mb_col++) {
#if ALT_ACT_MEASURE
      xd->plane[0].dst.buf = new_yv12->y_buffer + recon_yoffset;
      xd->left_available = (mb_col != 0);
      recon_yoffset += 16;
#endif

      // measure activity
      mb_activity = mb_activity_measure(x, mb_row, mb_col);

      // Keep frame sum
      activity_sum += mb_activity;

      // Store MB level activity details.
      *x->mb_activity_ptr = mb_activity;

      // Increment activity map pointer
      x->mb_activity_ptr++;

      // adjust to the next column of source macroblocks
      x->plane[0].src.buf += 16;
    }

    // adjust to the next row of mbs
    x->plane[0].src.buf += 16 * x->plane[0].src.stride - 16 * cm->mb_cols;
  }

  // Calculate an "average" MB activity
  calc_av_activity(cpi, activity_sum);

#if USE_ACT_INDEX
  // Calculate an activity index number of each mb
  calc_activity_index(cpi, x);
#endif
}

// Macroblock activity masking
static void activity_masking(VP9_COMP *cpi, MACROBLOCK *x) {
#if USE_ACT_INDEX
  x->rdmult += *(x->mb_activity_ptr) * (x->rdmult >> 2);
  x->errorperbit = x->rdmult * 100 / (110 * x->rddiv);
  x->errorperbit += (x->errorperbit == 0);
#else
  const int64_t act = *(x->mb_activity_ptr);

  // Apply the masking to the RD multiplier.
  const int64_t a = act + (2 * cpi->activity_avg);
  const int64_t b = (2 * act) + cpi->activity_avg;

  x->rdmult = (unsigned int) (((int64_t) x->rdmult * b + (a >> 1)) / a);
  x->errorperbit = x->rdmult * 100 / (110 * x->rddiv);
  x->errorperbit += (x->errorperbit == 0);
#endif

  // Activity based Zbin adjustment
  adjust_act_zbin(cpi, x);
}

static void update_state(VP9_COMP *cpi, PICK_MODE_CONTEXT *ctx,
                         int mi_row, int mi_col, BLOCK_SIZE bsize,
                         int output_enabled) {
  int i, x_idx, y;
  VP9_COMMON *const cm = &cpi->common;
  RD_OPT *const rd_opt = &cpi->rd;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  MODE_INFO *mi = &ctx->mic;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  MODE_INFO *mi_addr = xd->mi[0];
  const struct segmentation *const seg = &cm->seg;

  const int mis = cm->mi_stride;
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];
  int max_plane;

  assert(mi->mbmi.sb_type == bsize);

  *mi_addr = *mi;

  // If segmentation in use
  if (seg->enabled && output_enabled) {
    // For in frame complexity AQ copy the segment id from the segment map.
    if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
      const uint8_t *const map = seg->update_map ? cpi->segmentation_map
                                                 : cm->last_frame_seg_map;
      mi_addr->mbmi.segment_id =
        vp9_get_segment_id(cm, map, bsize, mi_row, mi_col);
    }
    // Else for cyclic refresh mode update the segment map, set the segment id
    // and then update the quantizer.
    else if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
      vp9_cyclic_refresh_update_segment(cpi, &xd->mi[0]->mbmi,
                                        mi_row, mi_col, bsize, 1);
      vp9_init_plane_quantizers(cpi, x);
    }
  }

  max_plane = is_inter_block(mbmi) ? MAX_MB_PLANE : 1;
  for (i = 0; i < max_plane; ++i) {
    p[i].coeff = ctx->coeff_pbuf[i][1];
    p[i].qcoeff = ctx->qcoeff_pbuf[i][1];
    pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][1];
    p[i].eobs = ctx->eobs_pbuf[i][1];
  }

  for (i = max_plane; i < MAX_MB_PLANE; ++i) {
    p[i].coeff = ctx->coeff_pbuf[i][2];
    p[i].qcoeff = ctx->qcoeff_pbuf[i][2];
    pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][2];
    p[i].eobs = ctx->eobs_pbuf[i][2];
  }

  // Restore the coding context of the MB to that that was in place
  // when the mode was picked for it
  for (y = 0; y < mi_height; y++)
    for (x_idx = 0; x_idx < mi_width; x_idx++)
      if ((xd->mb_to_right_edge >> (3 + MI_SIZE_LOG2)) + mi_width > x_idx
        && (xd->mb_to_bottom_edge >> (3 + MI_SIZE_LOG2)) + mi_height > y) {
        xd->mi[x_idx + y * mis] = mi_addr;
      }

  if (cpi->oxcf.aq_mode)
    vp9_init_plane_quantizers(cpi, x);

  // FIXME(rbultje) I'm pretty sure this should go to the end of this block
  // (i.e. after the output_enabled)
  if (bsize < BLOCK_32X32) {
    if (bsize < BLOCK_16X16)
      ctx->tx_rd_diff[ALLOW_16X16] = ctx->tx_rd_diff[ALLOW_8X8];
    ctx->tx_rd_diff[ALLOW_32X32] = ctx->tx_rd_diff[ALLOW_16X16];
  }

  if (is_inter_block(mbmi) && mbmi->sb_type < BLOCK_8X8) {
    mbmi->mv[0].as_int = mi->bmi[3].as_mv[0].as_int;
    mbmi->mv[1].as_int = mi->bmi[3].as_mv[1].as_int;
  }

  x->skip = ctx->skip;
  vpx_memcpy(x->zcoeff_blk[mbmi->tx_size], ctx->zcoeff_blk,
             sizeof(uint8_t) * ctx->num_4x4_blk);

  if (!output_enabled)
    return;

  if (!vp9_segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    for (i = 0; i < TX_MODES; i++)
      rd_opt->tx_select_diff[i] += ctx->tx_rd_diff[i];
  }

#if CONFIG_INTERNAL_STATS
  if (frame_is_intra_only(cm)) {
    static const int kf_mode_index[] = {
      THR_DC        /*DC_PRED*/,
      THR_V_PRED    /*V_PRED*/,
      THR_H_PRED    /*H_PRED*/,
      THR_D45_PRED  /*D45_PRED*/,
      THR_D135_PRED /*D135_PRED*/,
      THR_D117_PRED /*D117_PRED*/,
      THR_D153_PRED /*D153_PRED*/,
      THR_D207_PRED /*D207_PRED*/,
      THR_D63_PRED  /*D63_PRED*/,
      THR_TM        /*TM_PRED*/,
    };
    ++cpi->mode_chosen_counts[kf_mode_index[mbmi->mode]];
  } else {
    // Note how often each mode chosen as best
    ++cpi->mode_chosen_counts[ctx->best_mode_index];
  }
#endif
  if (!frame_is_intra_only(cm)) {
    if (is_inter_block(mbmi)) {
      vp9_update_mv_count(cm, xd);

      if (cm->interp_filter == SWITCHABLE) {
        const int ctx = vp9_get_pred_context_switchable_interp(xd);
        ++cm->counts.switchable_interp[ctx][mbmi->interp_filter];
      }
    }

    rd_opt->comp_pred_diff[SINGLE_REFERENCE] += ctx->single_pred_diff;
    rd_opt->comp_pred_diff[COMPOUND_REFERENCE] += ctx->comp_pred_diff;
    rd_opt->comp_pred_diff[REFERENCE_MODE_SELECT] += ctx->hybrid_pred_diff;

    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
      rd_opt->filter_diff[i] += ctx->best_filter_diff[i];
  }
}

void vp9_setup_src_planes(MACROBLOCK *x, const YV12_BUFFER_CONFIG *src,
                          int mi_row, int mi_col) {
  uint8_t *const buffers[4] = {src->y_buffer, src->u_buffer, src->v_buffer,
                               src->alpha_buffer};
  const int strides[4] = {src->y_stride, src->uv_stride, src->uv_stride,
                          src->alpha_stride};
  int i;

  // Set current frame pointer.
  x->e_mbd.cur_buf = src;

  for (i = 0; i < MAX_MB_PLANE; i++)
    setup_pred_plane(&x->plane[i].src, buffers[i], strides[i], mi_row, mi_col,
                     NULL, x->e_mbd.plane[i].subsampling_x,
                     x->e_mbd.plane[i].subsampling_y);
}

static void rd_pick_sb_modes(VP9_COMP *cpi, const TileInfo *const tile,
                             int mi_row, int mi_col,
                             int *totalrate, int64_t *totaldist,
                             BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                             int64_t best_rd) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const AQ_MODE aq_mode = cpi->oxcf.aq_mode;
  int i, orig_rdmult;
  double rdmult_ratio;

  vp9_clear_system_state();
  rdmult_ratio = 1.0;  // avoid uninitialized warnings

  // Use the lower precision, but faster, 32x32 fdct for mode selection.
  x->use_lp32x32fdct = 1;

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index != 0) {
      *totalrate = 0;
      *totaldist = 0;
      return;
    }
  }

  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  mbmi = &xd->mi[0]->mbmi;
  mbmi->sb_type = bsize;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    p[i].coeff = ctx->coeff_pbuf[i][0];
    p[i].qcoeff = ctx->qcoeff_pbuf[i][0];
    pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][0];
    p[i].eobs = ctx->eobs_pbuf[i][0];
  }
  ctx->is_coded = 0;
  x->skip_recode = 0;

  // Set to zero to make sure we do not use the previous encoded frame stats
  mbmi->skip = 0;

  x->source_variance = get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);

  // Save rdmult before it might be changed, so it can be restored later.
  orig_rdmult = x->rdmult;
  if (cpi->oxcf.tuning == VP8_TUNE_SSIM)
    activity_masking(cpi, x);

  if (aq_mode == VARIANCE_AQ) {
    const int energy = bsize <= BLOCK_16X16 ? x->mb_energy
                                            : vp9_block_energy(cpi, x, bsize);
    if (cm->frame_type == KEY_FRAME ||
        cpi->refresh_alt_ref_frame ||
        (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref)) {
      mbmi->segment_id = vp9_vaq_segment_id(energy);
    } else {
      const uint8_t *const map = cm->seg.update_map ? cpi->segmentation_map
                                                    : cm->last_frame_seg_map;
      mbmi->segment_id = vp9_get_segment_id(cm, map, bsize, mi_row, mi_col);
    }

    rdmult_ratio = vp9_vaq_rdmult_ratio(energy);
    vp9_init_plane_quantizers(cpi, x);
    vp9_clear_system_state();
    x->rdmult = (int)round(x->rdmult * rdmult_ratio);
  } else if (aq_mode == COMPLEXITY_AQ) {
    const int mi_offset = mi_row * cm->mi_cols + mi_col;
    unsigned char complexity = cpi->complexity_map[mi_offset];
    const int is_edge = (mi_row <= 1) || (mi_row >= (cm->mi_rows - 2)) ||
                        (mi_col <= 1) || (mi_col >= (cm->mi_cols - 2));
    if (!is_edge && (complexity > 128))
      x->rdmult += ((x->rdmult * (complexity - 128)) / 256);
  } else if (aq_mode == CYCLIC_REFRESH_AQ) {
    const uint8_t *const map = cm->seg.update_map ? cpi->segmentation_map
                                                  : cm->last_frame_seg_map;
    // If segment 1, use rdmult for that segment.
    if (vp9_get_segment_id(cm, map, bsize, mi_row, mi_col))
      x->rdmult = vp9_cyclic_refresh_get_rdmult(cpi->cyclic_refresh);
  }

  // Find best coding mode & reconstruct the MB so it is available
  // as a predictor for MBs that follow in the SB
  if (frame_is_intra_only(cm)) {
    vp9_rd_pick_intra_mode_sb(cpi, x, totalrate, totaldist, bsize, ctx,
                              best_rd);
  } else {
    if (bsize >= BLOCK_8X8)
      vp9_rd_pick_inter_mode_sb(cpi, x, tile, mi_row, mi_col,
                                totalrate, totaldist, bsize, ctx, best_rd);
    else
      vp9_rd_pick_inter_mode_sub8x8(cpi, x, tile, mi_row, mi_col, totalrate,
                                    totaldist, bsize, ctx, best_rd);
  }

  x->rdmult = orig_rdmult;

  if (aq_mode == VARIANCE_AQ && *totalrate != INT_MAX) {
    vp9_clear_system_state();
    *totalrate = (int)round(*totalrate * rdmult_ratio);
  }
}

static void update_stats(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  const MACROBLOCK *const x = &cpi->mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MODE_INFO *const mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;

  if (!frame_is_intra_only(cm)) {
    const int seg_ref_active = vp9_segfeature_active(&cm->seg, mbmi->segment_id,
                                                     SEG_LVL_REF_FRAME);
    if (!seg_ref_active) {
      FRAME_COUNTS *const counts = &cm->counts;
      const int inter_block = is_inter_block(mbmi);

      counts->intra_inter[vp9_get_intra_inter_context(xd)][inter_block]++;

      // If the segment reference feature is enabled we have only a single
      // reference frame allowed for the segment so exclude it from
      // the reference frame counts used to work out probabilities.
      if (inter_block) {
        const MV_REFERENCE_FRAME ref0 = mbmi->ref_frame[0];

        if (cm->reference_mode == REFERENCE_MODE_SELECT)
          counts->comp_inter[vp9_get_reference_mode_context(cm, xd)]
                            [has_second_ref(mbmi)]++;

        if (has_second_ref(mbmi)) {
          counts->comp_ref[vp9_get_pred_context_comp_ref_p(cm, xd)]
                          [ref0 == GOLDEN_FRAME]++;
        } else {
          counts->single_ref[vp9_get_pred_context_single_ref_p1(xd)][0]
                            [ref0 != LAST_FRAME]++;
          if (ref0 != LAST_FRAME)
            counts->single_ref[vp9_get_pred_context_single_ref_p2(xd)][1]
                              [ref0 != GOLDEN_FRAME]++;
        }
      }
    }
  }
}

static BLOCK_SIZE *get_sb_partitioning(MACROBLOCK *x, BLOCK_SIZE bsize) {
  switch (bsize) {
    case BLOCK_64X64:
      return &x->sb64_partitioning;
    case BLOCK_32X32:
      return &x->sb_partitioning[x->sb_index];
    case BLOCK_16X16:
      return &x->mb_partitioning[x->sb_index][x->mb_index];
    case BLOCK_8X8:
      return &x->b_partitioning[x->sb_index][x->mb_index][x->b_index];
    default:
      assert(0);
      return NULL;
  }
}

static void restore_context(VP9_COMP *cpi, int mi_row, int mi_col,
                            ENTROPY_CONTEXT a[16 * MAX_MB_PLANE],
                            ENTROPY_CONTEXT l[16 * MAX_MB_PLANE],
                            PARTITION_CONTEXT sa[8], PARTITION_CONTEXT sl[8],
                            BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  int mi_width = num_8x8_blocks_wide_lookup[bsize];
  int mi_height = num_8x8_blocks_high_lookup[bsize];
  for (p = 0; p < MAX_MB_PLANE; p++) {
    vpx_memcpy(
        xd->above_context[p] + ((mi_col * 2) >> xd->plane[p].subsampling_x),
        a + num_4x4_blocks_wide * p,
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
        xd->plane[p].subsampling_x);
    vpx_memcpy(
        xd->left_context[p]
            + ((mi_row & MI_MASK) * 2 >> xd->plane[p].subsampling_y),
        l + num_4x4_blocks_high * p,
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
        xd->plane[p].subsampling_y);
  }
  vpx_memcpy(xd->above_seg_context + mi_col, sa,
             sizeof(*xd->above_seg_context) * mi_width);
  vpx_memcpy(xd->left_seg_context + (mi_row & MI_MASK), sl,
             sizeof(xd->left_seg_context[0]) * mi_height);
}
static void save_context(VP9_COMP *cpi, int mi_row, int mi_col,
                         ENTROPY_CONTEXT a[16 * MAX_MB_PLANE],
                         ENTROPY_CONTEXT l[16 * MAX_MB_PLANE],
                         PARTITION_CONTEXT sa[8], PARTITION_CONTEXT sl[8],
                         BLOCK_SIZE bsize) {
  const MACROBLOCK *const x = &cpi->mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide = num_4x4_blocks_wide_lookup[bsize];
  const int num_4x4_blocks_high = num_4x4_blocks_high_lookup[bsize];
  int mi_width = num_8x8_blocks_wide_lookup[bsize];
  int mi_height = num_8x8_blocks_high_lookup[bsize];

  // buffer the above/left context information of the block in search.
  for (p = 0; p < MAX_MB_PLANE; ++p) {
    vpx_memcpy(
        a + num_4x4_blocks_wide * p,
        xd->above_context[p] + (mi_col * 2 >> xd->plane[p].subsampling_x),
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
        xd->plane[p].subsampling_x);
    vpx_memcpy(
        l + num_4x4_blocks_high * p,
        xd->left_context[p]
            + ((mi_row & MI_MASK) * 2 >> xd->plane[p].subsampling_y),
        (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
        xd->plane[p].subsampling_y);
  }
  vpx_memcpy(sa, xd->above_seg_context + mi_col,
             sizeof(*xd->above_seg_context) * mi_width);
  vpx_memcpy(sl, xd->left_seg_context + (mi_row & MI_MASK),
             sizeof(xd->left_seg_context[0]) * mi_height);
}

static void encode_b(VP9_COMP *cpi, const TileInfo *const tile,
                     TOKENEXTRA **tp, int mi_row, int mi_col,
                     int output_enabled, BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &cpi->mb;

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index > 0)
      return;
  }
  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  update_state(cpi, get_block_context(x, bsize), mi_row, mi_col, bsize,
               output_enabled);
  encode_superblock(cpi, tp, output_enabled, mi_row, mi_col, bsize);

  if (output_enabled) {
    update_stats(cpi);

    (*tp)->token = EOSB_TOKEN;
    (*tp)++;
  }
}

static void encode_sb(VP9_COMP *cpi, const TileInfo *const tile,
                      TOKENEXTRA **tp, int mi_row, int mi_col,
                      int output_enabled, BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;

  const int bsl = b_width_log2(bsize), hbs = (1 << bsl) / 4;
  int ctx;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  if (bsize >= BLOCK_8X8) {
    ctx = partition_plane_context(xd, mi_row, mi_col, bsize);
    subsize = *get_sb_partitioning(x, bsize);
  } else {
    ctx = 0;
    subsize = BLOCK_4X4;
  }

  partition = partition_lookup[bsl][subsize];

  switch (partition) {
    case PARTITION_NONE:
      if (output_enabled && bsize >= BLOCK_8X8)
        cm->counts.partition[ctx][PARTITION_NONE]++;
      encode_b(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      break;
    case PARTITION_VERT:
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_VERT]++;
      *get_sb_index(x, subsize) = 0;
      encode_b(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      if (mi_col + hbs < cm->mi_cols) {
        *get_sb_index(x, subsize) = 1;
        encode_b(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled, subsize);
      }
      break;
    case PARTITION_HORZ:
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_HORZ]++;
      *get_sb_index(x, subsize) = 0;
      encode_b(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      if (mi_row + hbs < cm->mi_rows) {
        *get_sb_index(x, subsize) = 1;
        encode_b(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled, subsize);
      }
      break;
    case PARTITION_SPLIT:
      subsize = get_subsize(bsize, PARTITION_SPLIT);
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_SPLIT]++;

      *get_sb_index(x, subsize) = 0;
      encode_sb(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      *get_sb_index(x, subsize) = 1;
      encode_sb(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled, subsize);
      *get_sb_index(x, subsize) = 2;
      encode_sb(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled, subsize);
      *get_sb_index(x, subsize) = 3;
      encode_sb(cpi, tile, tp, mi_row + hbs, mi_col + hbs, output_enabled,
                subsize);
      break;
    default:
      assert("Invalid partition type.");
  }

  if (partition != PARTITION_SPLIT || bsize == BLOCK_8X8)
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
}

// Check to see if the given partition size is allowed for a specified number
// of 8x8 block rows and columns remaining in the image.
// If not then return the largest allowed partition size
static BLOCK_SIZE find_partition_size(BLOCK_SIZE bsize,
                                      int rows_left, int cols_left,
                                      int *bh, int *bw) {
  if (rows_left <= 0 || cols_left <= 0) {
    return MIN(bsize, BLOCK_8X8);
  } else {
    for (; bsize > 0; bsize -= 3) {
      *bh = num_8x8_blocks_high_lookup[bsize];
      *bw = num_8x8_blocks_wide_lookup[bsize];
      if ((*bh <= rows_left) && (*bw <= cols_left)) {
        break;
      }
    }
  }
  return bsize;
}

// This function attempts to set all mode info entries in a given SB64
// to the same block partition size.
// However, at the bottom and right borders of the image the requested size
// may not be allowed in which case this code attempts to choose the largest
// allowable partition.
static void set_fixed_partitioning(VP9_COMP *cpi, const TileInfo *const tile,
                                   MODE_INFO **mi_8x8, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  const int mis = cm->mi_stride;
  int row8x8_remaining = tile->mi_row_end - mi_row;
  int col8x8_remaining = tile->mi_col_end - mi_col;
  int block_row, block_col;
  MODE_INFO *mi_upper_left = cm->mi + mi_row * mis + mi_col;
  int bh = num_8x8_blocks_high_lookup[bsize];
  int bw = num_8x8_blocks_wide_lookup[bsize];

  assert((row8x8_remaining > 0) && (col8x8_remaining > 0));

  // Apply the requested partition size to the SB64 if it is all "in image"
  if ((col8x8_remaining >= MI_BLOCK_SIZE) &&
      (row8x8_remaining >= MI_BLOCK_SIZE)) {
    for (block_row = 0; block_row < MI_BLOCK_SIZE; block_row += bh) {
      for (block_col = 0; block_col < MI_BLOCK_SIZE; block_col += bw) {
        int index = block_row * mis + block_col;
        mi_8x8[index] = mi_upper_left + index;
        mi_8x8[index]->mbmi.sb_type = bsize;
      }
    }
  } else {
    // Else this is a partial SB64.
    for (block_row = 0; block_row < MI_BLOCK_SIZE; block_row += bh) {
      for (block_col = 0; block_col < MI_BLOCK_SIZE; block_col += bw) {
        int index = block_row * mis + block_col;
        // Find a partition size that fits
        bsize = find_partition_size(bsize,
                                    (row8x8_remaining - block_row),
                                    (col8x8_remaining - block_col), &bh, &bw);
        mi_8x8[index] = mi_upper_left + index;
        mi_8x8[index]->mbmi.sb_type = bsize;
      }
    }
  }
}

static void copy_partitioning(VP9_COMMON *cm, MODE_INFO **mi_8x8,
  MODE_INFO **prev_mi_8x8) {
  const int mis = cm->mi_stride;
  int block_row, block_col;

  for (block_row = 0; block_row < 8; ++block_row) {
    for (block_col = 0; block_col < 8; ++block_col) {
      MODE_INFO *const prev_mi = prev_mi_8x8[block_row * mis + block_col];
      const BLOCK_SIZE sb_type = prev_mi ? prev_mi->mbmi.sb_type : 0;

      if (prev_mi) {
        const ptrdiff_t offset = prev_mi - cm->prev_mi;
        mi_8x8[block_row * mis + block_col] = cm->mi + offset;
        mi_8x8[block_row * mis + block_col]->mbmi.sb_type = sb_type;
      }
    }
  }
}

static void constrain_copy_partitioning(VP9_COMP *const cpi,
                                        const TileInfo *const tile,
                                        MODE_INFO **mi_8x8,
                                        MODE_INFO **prev_mi_8x8,
                                        int mi_row, int mi_col,
                                        BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  const int mis = cm->mi_stride;
  const int row8x8_remaining = tile->mi_row_end - mi_row;
  const int col8x8_remaining = tile->mi_col_end - mi_col;
  MODE_INFO *const mi_upper_left = cm->mi + mi_row * mis + mi_col;
  const int bh = num_8x8_blocks_high_lookup[bsize];
  const int bw = num_8x8_blocks_wide_lookup[bsize];
  int block_row, block_col;

  assert((row8x8_remaining > 0) && (col8x8_remaining > 0));

  // If the SB64 if it is all "in image".
  if ((col8x8_remaining >= MI_BLOCK_SIZE) &&
      (row8x8_remaining >= MI_BLOCK_SIZE)) {
    for (block_row = 0; block_row < MI_BLOCK_SIZE; block_row += bh) {
      for (block_col = 0; block_col < MI_BLOCK_SIZE; block_col += bw) {
        const int index = block_row * mis + block_col;
        MODE_INFO *prev_mi = prev_mi_8x8[index];
        const BLOCK_SIZE sb_type = prev_mi ? prev_mi->mbmi.sb_type : 0;
        // Use previous partition if block size is not larger than bsize.
        if (prev_mi && sb_type <= bsize) {
          int block_row2, block_col2;
          for (block_row2 = 0; block_row2 < bh; ++block_row2) {
            for (block_col2 = 0; block_col2 < bw; ++block_col2) {
              const int index2 = (block_row + block_row2) * mis +
                  block_col + block_col2;
              prev_mi = prev_mi_8x8[index2];
              if (prev_mi) {
                const ptrdiff_t offset = prev_mi - cm->prev_mi;
                mi_8x8[index2] = cm->mi + offset;
                mi_8x8[index2]->mbmi.sb_type = prev_mi->mbmi.sb_type;
              }
            }
          }
        } else {
          // Otherwise, use fixed partition of size bsize.
          mi_8x8[index] = mi_upper_left + index;
          mi_8x8[index]->mbmi.sb_type = bsize;
        }
      }
    }
  } else {
    // Else this is a partial SB64, copy previous partition.
    copy_partitioning(cm, mi_8x8, prev_mi_8x8);
  }
}


const struct {
  int row;
  int col;
} coord_lookup[16] = {
    // 32x32 index = 0
    {0, 0}, {0, 2}, {2, 0}, {2, 2},
    // 32x32 index = 1
    {0, 4}, {0, 6}, {2, 4}, {2, 6},
    // 32x32 index = 2
    {4, 0}, {4, 2}, {6, 0}, {6, 2},
    // 32x32 index = 3
    {4, 4}, {4, 6}, {6, 4}, {6, 6},
};

static void set_source_var_based_partition(VP9_COMP *cpi,
                                           const TileInfo *const tile,
                                           MODE_INFO **mi_8x8,
                                           int mi_row, int mi_col) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  const int mis = cm->mi_stride;
  const int row8x8_remaining = tile->mi_row_end - mi_row;
  const int col8x8_remaining = tile->mi_col_end - mi_col;
  MODE_INFO *mi_upper_left = cm->mi + mi_row * mis + mi_col;

  vp9_setup_src_planes(x, cpi->Source, mi_row, mi_col);

  assert((row8x8_remaining > 0) && (col8x8_remaining > 0));

  // In-image SB64
  if ((col8x8_remaining >= MI_BLOCK_SIZE) &&
      (row8x8_remaining >= MI_BLOCK_SIZE)) {
    const int src_stride = x->plane[0].src.stride;
    const int pre_stride = cpi->Last_Source->y_stride;
    const uint8_t *src = x->plane[0].src.buf;
    const int pre_offset = (mi_row * MI_SIZE) * pre_stride +
                           (mi_col * MI_SIZE);
    const uint8_t *pre_src = cpi->Last_Source->y_buffer + pre_offset;
    const unsigned int thr_32x32 = cpi->sf.source_var_thresh;
    const unsigned int thr_64x64 = thr_32x32 << 1;
    int i, j;
    int index;
    diff d32[4];
    int use16x16 = 0;

    for (i = 0; i < 4; i++) {
      diff d16[4];

      for (j = 0; j < 4; j++) {
        int b_mi_row = coord_lookup[i * 4 + j].row;
        int b_mi_col = coord_lookup[i * 4 + j].col;
        int b_offset = b_mi_row * MI_SIZE * src_stride +
                       b_mi_col * MI_SIZE;

        vp9_get_sse_sum_16x16(src + b_offset,
                              src_stride,
                              pre_src + b_offset,
                              pre_stride, &d16[j].sse, &d16[j].sum);

        d16[j].var = d16[j].sse -
            (((uint32_t)d16[j].sum * d16[j].sum) >> 8);

        index = b_mi_row * mis + b_mi_col;
        mi_8x8[index] = mi_upper_left + index;
        mi_8x8[index]->mbmi.sb_type = BLOCK_16X16;

        // TODO(yunqingwang): If d16[j].var is very large, use 8x8 partition
        // size to further improve quality.
      }

      if (d16[0].var < thr_32x32 && d16[1].var < thr_32x32 &&
          d16[2].var < thr_32x32 && d16[3].var < thr_32x32) {
        d32[i].sse = d16[0].sse;
        d32[i].sum = d16[0].sum;

        for (j = 1; j < 4; j++) {
          d32[i].sse += d16[j].sse;
          d32[i].sum += d16[j].sum;
        }

        d32[i].var = d32[i].sse - (((int64_t)d32[i].sum * d32[i].sum) >> 10);

        index = coord_lookup[i*4].row * mis + coord_lookup[i*4].col;
        mi_8x8[index] = mi_upper_left + index;
        mi_8x8[index]->mbmi.sb_type = BLOCK_32X32;

        if (!((cm->current_video_frame - 1) %
            cpi->sf.search_type_check_frequency))
          cpi->use_large_partition_rate += 1;
      } else {
        use16x16 = 1;
      }
    }

    if (!use16x16) {
      if (d32[0].var < thr_64x64 && d32[1].var < thr_64x64 &&
          d32[2].var < thr_64x64 && d32[3].var < thr_64x64)  {
        mi_8x8[0] = mi_upper_left;
        mi_8x8[0]->mbmi.sb_type = BLOCK_64X64;
      }
    }
  } else {   // partial in-image SB64
    BLOCK_SIZE bsize = BLOCK_16X16;
    int bh = num_8x8_blocks_high_lookup[bsize];
    int bw = num_8x8_blocks_wide_lookup[bsize];
    int r, c;
    for (r = 0; r < MI_BLOCK_SIZE; r += bh) {
      for (c = 0; c < MI_BLOCK_SIZE; c += bw) {
        const int index = r * mis + c;
        mi_8x8[index] = mi_upper_left + index;
        mi_8x8[index]->mbmi.sb_type = find_partition_size(bsize,
            row8x8_remaining - r, col8x8_remaining - c, &bh, &bw);
      }
    }
  }
}

static int sb_has_motion(const VP9_COMMON *cm, MODE_INFO **prev_mi_8x8) {
  const int mis = cm->mi_stride;
  int block_row, block_col;

  if (cm->prev_mi) {
    for (block_row = 0; block_row < 8; ++block_row) {
      for (block_col = 0; block_col < 8; ++block_col) {
        const MODE_INFO *prev_mi = prev_mi_8x8[block_row * mis + block_col];
        if (prev_mi) {
          if (abs(prev_mi->mbmi.mv[0].as_mv.row) >= 8 ||
              abs(prev_mi->mbmi.mv[0].as_mv.col) >= 8)
            return 1;
        }
      }
    }
  }
  return 0;
}

static void update_state_rt(VP9_COMP *cpi, PICK_MODE_CONTEXT *ctx,
                            int mi_row, int mi_col, int bsize) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const struct segmentation *const seg = &cm->seg;

  *(xd->mi[0]) = ctx->mic;

  // For in frame adaptive Q, check for reseting the segment_id and updating
  // the cyclic refresh map.
  if ((cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) && seg->enabled) {
    vp9_cyclic_refresh_update_segment(cpi, &xd->mi[0]->mbmi,
                                      mi_row, mi_col, bsize, 1);
    vp9_init_plane_quantizers(cpi, x);
  }

  if (is_inter_block(mbmi)) {
    vp9_update_mv_count(cm, xd);

    if (cm->interp_filter == SWITCHABLE) {
      const int pred_ctx = vp9_get_pred_context_switchable_interp(xd);
      ++cm->counts.switchable_interp[pred_ctx][mbmi->interp_filter];
    }
  }

  x->skip = ctx->skip;
}

static void encode_b_rt(VP9_COMP *cpi, const TileInfo *const tile,
                        TOKENEXTRA **tp, int mi_row, int mi_col,
                        int output_enabled, BLOCK_SIZE bsize) {
  MACROBLOCK *const x = &cpi->mb;

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index > 0)
      return;
  }

  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  update_state_rt(cpi, get_block_context(x, bsize), mi_row, mi_col, bsize);

  encode_superblock(cpi, tp, output_enabled, mi_row, mi_col, bsize);
  update_stats(cpi);

  (*tp)->token = EOSB_TOKEN;
  (*tp)++;
}

static void encode_sb_rt(VP9_COMP *cpi, const TileInfo *const tile,
                         TOKENEXTRA **tp, int mi_row, int mi_col,
                         int output_enabled, BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;

  const int bsl = b_width_log2(bsize), hbs = (1 << bsl) / 4;
  int ctx;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  if (bsize >= BLOCK_8X8) {
    MACROBLOCKD *const xd = &cpi->mb.e_mbd;
    const int idx_str = xd->mi_stride * mi_row + mi_col;
    MODE_INFO ** mi_8x8 = cm->mi_grid_visible + idx_str;
    ctx = partition_plane_context(xd, mi_row, mi_col, bsize);
    subsize = mi_8x8[0]->mbmi.sb_type;
  } else {
    ctx = 0;
    subsize = BLOCK_4X4;
  }

  partition = partition_lookup[bsl][subsize];

  switch (partition) {
    case PARTITION_NONE:
      if (output_enabled && bsize >= BLOCK_8X8)
        cm->counts.partition[ctx][PARTITION_NONE]++;
      encode_b_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      break;
    case PARTITION_VERT:
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_VERT]++;
      *get_sb_index(x, subsize) = 0;
      encode_b_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      if (mi_col + hbs < cm->mi_cols) {
        *get_sb_index(x, subsize) = 1;
        encode_b_rt(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled,
                    subsize);
      }
      break;
    case PARTITION_HORZ:
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_HORZ]++;
      *get_sb_index(x, subsize) = 0;
      encode_b_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      if (mi_row + hbs < cm->mi_rows) {
        *get_sb_index(x, subsize) = 1;
        encode_b_rt(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled,
                    subsize);
      }
      break;
    case PARTITION_SPLIT:
      subsize = get_subsize(bsize, PARTITION_SPLIT);
      if (output_enabled)
        cm->counts.partition[ctx][PARTITION_SPLIT]++;

      *get_sb_index(x, subsize) = 0;
      encode_sb_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, subsize);
      *get_sb_index(x, subsize) = 1;
      encode_sb_rt(cpi, tile, tp, mi_row, mi_col + hbs, output_enabled,
                   subsize);
      *get_sb_index(x, subsize) = 2;
      encode_sb_rt(cpi, tile, tp, mi_row + hbs, mi_col, output_enabled,
                   subsize);
      *get_sb_index(x, subsize) = 3;
      encode_sb_rt(cpi, tile, tp, mi_row + hbs, mi_col + hbs, output_enabled,
                   subsize);
      break;
    default:
      assert("Invalid partition type.");
  }

  if (partition != PARTITION_SPLIT || bsize == BLOCK_8X8)
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
}

static void rd_use_partition(VP9_COMP *cpi,
                             const TileInfo *const tile,
                             MODE_INFO **mi_8x8,
                             TOKENEXTRA **tp, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, int *rate, int64_t *dist,
                             int do_recon) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mis = cm->mi_stride;
  const int bsl = b_width_log2(bsize);
  const int mi_step = num_4x4_blocks_wide_lookup[bsize] / 2;
  const int bss = (1 << bsl) / 4;
  int i, pl;
  PARTITION_TYPE partition = PARTITION_NONE;
  BLOCK_SIZE subsize;
  ENTROPY_CONTEXT l[16 * MAX_MB_PLANE], a[16 * MAX_MB_PLANE];
  PARTITION_CONTEXT sl[8], sa[8];
  int last_part_rate = INT_MAX;
  int64_t last_part_dist = INT64_MAX;
  int64_t last_part_rd = INT64_MAX;
  int none_rate = INT_MAX;
  int64_t none_dist = INT64_MAX;
  int64_t none_rd = INT64_MAX;
  int chosen_rate = INT_MAX;
  int64_t chosen_dist = INT64_MAX;
  int64_t chosen_rd = INT64_MAX;
  BLOCK_SIZE sub_subsize = BLOCK_4X4;
  int splits_below = 0;
  BLOCK_SIZE bs_type = mi_8x8[0]->mbmi.sb_type;
  int do_partition_search = 1;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  assert(num_4x4_blocks_wide_lookup[bsize] ==
         num_4x4_blocks_high_lookup[bsize]);

  partition = partition_lookup[bsl][bs_type];
  subsize = get_subsize(bsize, partition);

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index != 0) {
      *rate = 0;
      *dist = 0;
      return;
    }
  } else {
    *(get_sb_partitioning(x, bsize)) = subsize;
  }
  save_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

  if (bsize == BLOCK_16X16) {
    set_offsets(cpi, tile, mi_row, mi_col, bsize);
    x->mb_energy = vp9_block_energy(cpi, x, bsize);
  } else {
    x->in_active_map = check_active_map(cpi, x, mi_row, mi_col, bsize);
  }

  if (!x->in_active_map) {
    do_partition_search = 0;
    if (mi_row + (mi_step >> 1) < cm->mi_rows &&
        mi_col + (mi_step >> 1) < cm->mi_cols) {
      *(get_sb_partitioning(x, bsize)) = bsize;
      bs_type = mi_8x8[0]->mbmi.sb_type = bsize;
      subsize = bsize;
      partition = PARTITION_NONE;
    }
  }
  if (do_partition_search &&
      cpi->sf.partition_search_type == SEARCH_PARTITION &&
      cpi->sf.adjust_partitioning_from_last_frame) {
    // Check if any of the sub blocks are further split.
    if (partition == PARTITION_SPLIT && subsize > BLOCK_8X8) {
      sub_subsize = get_subsize(subsize, PARTITION_SPLIT);
      splits_below = 1;
      for (i = 0; i < 4; i++) {
        int jj = i >> 1, ii = i & 0x01;
        MODE_INFO * this_mi = mi_8x8[jj * bss * mis + ii * bss];
        if (this_mi && this_mi->mbmi.sb_type >= sub_subsize) {
          splits_below = 0;
        }
      }
    }

    // If partition is not none try none unless each of the 4 splits are split
    // even further..
    if (partition != PARTITION_NONE && !splits_below &&
        mi_row + (mi_step >> 1) < cm->mi_rows &&
        mi_col + (mi_step >> 1) < cm->mi_cols) {
      *(get_sb_partitioning(x, bsize)) = bsize;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &none_rate, &none_dist, bsize,
                       get_block_context(x, bsize), INT64_MAX);

      pl = partition_plane_context(xd, mi_row, mi_col, bsize);

      if (none_rate < INT_MAX) {
        none_rate += x->partition_cost[pl][PARTITION_NONE];
        none_rd = RDCOST(x->rdmult, x->rddiv, none_rate, none_dist);
      }

      restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
      mi_8x8[0]->mbmi.sb_type = bs_type;
      *(get_sb_partitioning(x, bsize)) = subsize;
    }
  }

  switch (partition) {
    case PARTITION_NONE:
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &last_part_rate,
                       &last_part_dist, bsize,
                       get_block_context(x, bsize), INT64_MAX);
      break;
    case PARTITION_HORZ:
      *get_sb_index(x, subsize) = 0;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &last_part_rate,
                       &last_part_dist, subsize,
                       get_block_context(x, subsize), INT64_MAX);
      if (last_part_rate != INT_MAX &&
          bsize >= BLOCK_8X8 && mi_row + (mi_step >> 1) < cm->mi_rows) {
        int rt = 0;
        int64_t dt = 0;
        update_state(cpi, get_block_context(x, subsize), mi_row, mi_col,
                     subsize, 0);
        encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize);
        *get_sb_index(x, subsize) = 1;
        rd_pick_sb_modes(cpi, tile, mi_row + (mi_step >> 1), mi_col, &rt, &dt,
                         subsize, get_block_context(x, subsize), INT64_MAX);
        if (rt == INT_MAX || dt == INT64_MAX) {
          last_part_rate = INT_MAX;
          last_part_dist = INT64_MAX;
          break;
        }

        last_part_rate += rt;
        last_part_dist += dt;
      }
      break;
    case PARTITION_VERT:
      *get_sb_index(x, subsize) = 0;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &last_part_rate,
                       &last_part_dist, subsize,
                       get_block_context(x, subsize), INT64_MAX);
      if (last_part_rate != INT_MAX &&
          bsize >= BLOCK_8X8 && mi_col + (mi_step >> 1) < cm->mi_cols) {
        int rt = 0;
        int64_t dt = 0;
        update_state(cpi, get_block_context(x, subsize), mi_row, mi_col,
                     subsize, 0);
        encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize);
        *get_sb_index(x, subsize) = 1;
        rd_pick_sb_modes(cpi, tile, mi_row, mi_col + (mi_step >> 1), &rt, &dt,
                         subsize, get_block_context(x, subsize), INT64_MAX);
        if (rt == INT_MAX || dt == INT64_MAX) {
          last_part_rate = INT_MAX;
          last_part_dist = INT64_MAX;
          break;
        }
        last_part_rate += rt;
        last_part_dist += dt;
      }
      break;
    case PARTITION_SPLIT:
      // Split partition.
      last_part_rate = 0;
      last_part_dist = 0;
      for (i = 0; i < 4; i++) {
        int x_idx = (i & 1) * (mi_step >> 1);
        int y_idx = (i >> 1) * (mi_step >> 1);
        int jj = i >> 1, ii = i & 0x01;
        int rt;
        int64_t dt;

        if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
          continue;

        *get_sb_index(x, subsize) = i;

        rd_use_partition(cpi, tile, mi_8x8 + jj * bss * mis + ii * bss, tp,
                         mi_row + y_idx, mi_col + x_idx, subsize, &rt, &dt,
                         i != 3);
        if (rt == INT_MAX || dt == INT64_MAX) {
          last_part_rate = INT_MAX;
          last_part_dist = INT64_MAX;
          break;
        }
        last_part_rate += rt;
        last_part_dist += dt;
      }
      break;
    default:
      assert(0);
  }

  pl = partition_plane_context(xd, mi_row, mi_col, bsize);
  if (last_part_rate < INT_MAX) {
    last_part_rate += x->partition_cost[pl][partition];
    last_part_rd = RDCOST(x->rdmult, x->rddiv, last_part_rate, last_part_dist);
  }

  if (do_partition_search
      && cpi->sf.adjust_partitioning_from_last_frame
      && cpi->sf.partition_search_type == SEARCH_PARTITION
      && partition != PARTITION_SPLIT && bsize > BLOCK_8X8
      && (mi_row + mi_step < cm->mi_rows ||
          mi_row + (mi_step >> 1) == cm->mi_rows)
      && (mi_col + mi_step < cm->mi_cols ||
          mi_col + (mi_step >> 1) == cm->mi_cols)) {
    BLOCK_SIZE split_subsize = get_subsize(bsize, PARTITION_SPLIT);
    chosen_rate = 0;
    chosen_dist = 0;
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

    // Split partition.
    for (i = 0; i < 4; i++) {
      int x_idx = (i & 1) * (mi_step >> 1);
      int y_idx = (i >> 1) * (mi_step >> 1);
      int rt = 0;
      int64_t dt = 0;
      ENTROPY_CONTEXT l[16 * MAX_MB_PLANE], a[16 * MAX_MB_PLANE];
      PARTITION_CONTEXT sl[8], sa[8];

      if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
        continue;

      *get_sb_index(x, split_subsize) = i;
      *get_sb_partitioning(x, bsize) = split_subsize;
      *get_sb_partitioning(x, split_subsize) = split_subsize;

      save_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

      rd_pick_sb_modes(cpi, tile, mi_row + y_idx, mi_col + x_idx, &rt, &dt,
                       split_subsize, get_block_context(x, split_subsize),
                       INT64_MAX);

      restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

      if (rt == INT_MAX || dt == INT64_MAX) {
        chosen_rate = INT_MAX;
        chosen_dist = INT64_MAX;
        break;
      }

      chosen_rate += rt;
      chosen_dist += dt;

      if (i != 3)
        encode_sb(cpi, tile, tp,  mi_row + y_idx, mi_col + x_idx, 0,
                  split_subsize);

      pl = partition_plane_context(xd, mi_row + y_idx, mi_col + x_idx,
                                   split_subsize);
      chosen_rate += x->partition_cost[pl][PARTITION_NONE];
    }
    pl = partition_plane_context(xd, mi_row, mi_col, bsize);
    if (chosen_rate < INT_MAX) {
      chosen_rate += x->partition_cost[pl][PARTITION_SPLIT];
      chosen_rd = RDCOST(x->rdmult, x->rddiv, chosen_rate, chosen_dist);
    }
  }

  // If last_part is better set the partitioning to that...
  if (last_part_rd < chosen_rd) {
    mi_8x8[0]->mbmi.sb_type = bsize;
    if (bsize >= BLOCK_8X8)
      *(get_sb_partitioning(x, bsize)) = subsize;
    chosen_rate = last_part_rate;
    chosen_dist = last_part_dist;
    chosen_rd = last_part_rd;
  }
  // If none was better set the partitioning to that...
  if (none_rd < chosen_rd) {
    if (bsize >= BLOCK_8X8)
      *(get_sb_partitioning(x, bsize)) = bsize;
    chosen_rate = none_rate;
    chosen_dist = none_dist;
  }

  restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

  // We must have chosen a partitioning and encoding or we'll fail later on.
  // No other opportunities for success.
  if ( bsize == BLOCK_64X64)
    assert(chosen_rate < INT_MAX && chosen_dist < INT64_MAX);

  if (do_recon) {
    int output_enabled = (bsize == BLOCK_64X64);

    // Check the projected output rate for this SB against it's target
    // and and if necessary apply a Q delta using segmentation to get
    // closer to the target.
    if ((cpi->oxcf.aq_mode == COMPLEXITY_AQ) && cm->seg.update_map) {
      vp9_select_in_frame_q_segment(cpi, mi_row, mi_col,
                                    output_enabled, chosen_rate);
    }

    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
      vp9_cyclic_refresh_set_rate_and_dist_sb(cpi->cyclic_refresh,
                                              chosen_rate, chosen_dist);

    encode_sb(cpi, tile, tp, mi_row, mi_col, output_enabled, bsize);
  }

  *rate = chosen_rate;
  *dist = chosen_dist;
}

static const BLOCK_SIZE min_partition_size[BLOCK_SIZES] = {
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_4X4,
  BLOCK_4X4,   BLOCK_4X4,   BLOCK_4X4,
  BLOCK_8X8,   BLOCK_8X8,   BLOCK_8X8,
  BLOCK_16X16, BLOCK_16X16, BLOCK_16X16,
  BLOCK_16X16
};

static const BLOCK_SIZE max_partition_size[BLOCK_SIZES] = {
  BLOCK_8X8,   BLOCK_16X16, BLOCK_16X16,
  BLOCK_16X16, BLOCK_32X32, BLOCK_32X32,
  BLOCK_32X32, BLOCK_64X64, BLOCK_64X64,
  BLOCK_64X64, BLOCK_64X64, BLOCK_64X64,
  BLOCK_64X64
};

// Look at all the mode_info entries for blocks that are part of this
// partition and find the min and max values for sb_type.
// At the moment this is designed to work on a 64x64 SB but could be
// adjusted to use a size parameter.
//
// The min and max are assumed to have been initialized prior to calling this
// function so repeat calls can accumulate a min and max of more than one sb64.
static void get_sb_partition_size_range(VP9_COMP *cpi, MODE_INFO ** mi_8x8,
                                        BLOCK_SIZE * min_block_size,
                                        BLOCK_SIZE * max_block_size ) {
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  int sb_width_in_blocks = MI_BLOCK_SIZE;
  int sb_height_in_blocks  = MI_BLOCK_SIZE;
  int i, j;
  int index = 0;

  // Check the sb_type for each block that belongs to this region.
  for (i = 0; i < sb_height_in_blocks; ++i) {
    for (j = 0; j < sb_width_in_blocks; ++j) {
      MODE_INFO * mi = mi_8x8[index+j];
      BLOCK_SIZE sb_type = mi ? mi->mbmi.sb_type : 0;
      *min_block_size = MIN(*min_block_size, sb_type);
      *max_block_size = MAX(*max_block_size, sb_type);
    }
    index += xd->mi_stride;
  }
}

// Next square block size less or equal than current block size.
static const BLOCK_SIZE next_square_size[BLOCK_SIZES] = {
  BLOCK_4X4, BLOCK_4X4, BLOCK_4X4,
  BLOCK_8X8, BLOCK_8X8, BLOCK_8X8,
  BLOCK_16X16, BLOCK_16X16, BLOCK_16X16,
  BLOCK_32X32, BLOCK_32X32, BLOCK_32X32,
  BLOCK_64X64
};

// Look at neighboring blocks and set a min and max partition size based on
// what they chose.
static void rd_auto_partition_range(VP9_COMP *cpi, const TileInfo *const tile,
                                    int mi_row, int mi_col,
                                    BLOCK_SIZE *min_block_size,
                                    BLOCK_SIZE *max_block_size) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  MODE_INFO **mi_8x8 = xd->mi;
  const int left_in_image = xd->left_available && mi_8x8[-1];
  const int above_in_image = xd->up_available &&
                             mi_8x8[-xd->mi_stride];
  MODE_INFO **above_sb64_mi_8x8;
  MODE_INFO **left_sb64_mi_8x8;

  int row8x8_remaining = tile->mi_row_end - mi_row;
  int col8x8_remaining = tile->mi_col_end - mi_col;
  int bh, bw;
  BLOCK_SIZE min_size = BLOCK_4X4;
  BLOCK_SIZE max_size = BLOCK_64X64;
  // Trap case where we do not have a prediction.
  if (left_in_image || above_in_image || cm->frame_type != KEY_FRAME) {
    // Default "min to max" and "max to min"
    min_size = BLOCK_64X64;
    max_size = BLOCK_4X4;

    // NOTE: each call to get_sb_partition_size_range() uses the previous
    // passed in values for min and max as a starting point.
    // Find the min and max partition used in previous frame at this location
    if (cm->frame_type != KEY_FRAME) {
      MODE_INFO **const prev_mi =
          &cm->prev_mi_grid_visible[mi_row * xd->mi_stride + mi_col];
      get_sb_partition_size_range(cpi, prev_mi, &min_size, &max_size);
    }
    // Find the min and max partition sizes used in the left SB64
    if (left_in_image) {
      left_sb64_mi_8x8 = &mi_8x8[-MI_BLOCK_SIZE];
      get_sb_partition_size_range(cpi, left_sb64_mi_8x8,
                                  &min_size, &max_size);
    }
    // Find the min and max partition sizes used in the above SB64.
    if (above_in_image) {
      above_sb64_mi_8x8 = &mi_8x8[-xd->mi_stride * MI_BLOCK_SIZE];
      get_sb_partition_size_range(cpi, above_sb64_mi_8x8,
                                  &min_size, &max_size);
    }
    // adjust observed min and max
    if (cpi->sf.auto_min_max_partition_size == RELAXED_NEIGHBORING_MIN_MAX) {
      min_size = min_partition_size[min_size];
      max_size = max_partition_size[max_size];
    }
  }

  // Check border cases where max and min from neighbors may not be legal.
  max_size = find_partition_size(max_size,
                                 row8x8_remaining, col8x8_remaining,
                                 &bh, &bw);
  min_size = MIN(min_size, max_size);

  // When use_square_partition_only is true, make sure at least one square
  // partition is allowed by selecting the next smaller square size as
  // *min_block_size.
  if (cpi->sf.use_square_partition_only &&
      next_square_size[max_size] < min_size) {
     min_size = next_square_size[max_size];
  }
  *min_block_size = min_size;
  *max_block_size = max_size;
}

static INLINE void store_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  vpx_memcpy(ctx->pred_mv, x->pred_mv, sizeof(x->pred_mv));
}

static INLINE void load_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  vpx_memcpy(x->pred_mv, ctx->pred_mv, sizeof(x->pred_mv));
}

// TODO(jingning,jimbankoski,rbultje): properly skip partition types that are
// unlikely to be selected depending on previous rate-distortion optimization
// results, for encoding speed-up.
static void rd_pick_partition(VP9_COMP *cpi, const TileInfo *const tile,
                              TOKENEXTRA **tp, int mi_row,
                              int mi_col, BLOCK_SIZE bsize, int *rate,
                              int64_t *dist, int do_recon, int64_t best_rd) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_step = num_8x8_blocks_wide_lookup[bsize] / 2;
  ENTROPY_CONTEXT l[16 * MAX_MB_PLANE], a[16 * MAX_MB_PLANE];
  PARTITION_CONTEXT sl[8], sa[8];
  TOKENEXTRA *tp_orig = *tp;
  PICK_MODE_CONTEXT *ctx = get_block_context(x, bsize);
  int i, pl;
  BLOCK_SIZE subsize;
  int this_rate, sum_rate = 0, best_rate = INT_MAX;
  int64_t this_dist, sum_dist = 0, best_dist = INT64_MAX;
  int64_t sum_rd = 0;
  int do_split = bsize >= BLOCK_8X8;
  int do_rect = 1;
  // Override skipping rectangular partition operations for edge blocks
  const int force_horz_split = (mi_row + mi_step >= cm->mi_rows);
  const int force_vert_split = (mi_col + mi_step >= cm->mi_cols);
  const int xss = x->e_mbd.plane[1].subsampling_x;
  const int yss = x->e_mbd.plane[1].subsampling_y;

  int partition_none_allowed = !force_horz_split && !force_vert_split;
  int partition_horz_allowed = !force_vert_split && yss <= xss &&
                               bsize >= BLOCK_8X8;
  int partition_vert_allowed = !force_horz_split && xss <= yss &&
                               bsize >= BLOCK_8X8;
  (void) *tp_orig;

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index != 0) {
      *rate = 0;
      *dist = 0;
      return;
    }
  }
  assert(num_8x8_blocks_wide_lookup[bsize] ==
             num_8x8_blocks_high_lookup[bsize]);

  if (bsize == BLOCK_16X16) {
    set_offsets(cpi, tile, mi_row, mi_col, bsize);
    x->mb_energy = vp9_block_energy(cpi, x, bsize);
  } else {
    x->in_active_map = check_active_map(cpi, x, mi_row, mi_col, bsize);
  }

  // Determine partition types in search according to the speed features.
  // The threshold set here has to be of square block size.
  if (cpi->sf.auto_min_max_partition_size) {
    partition_none_allowed &= (bsize <= cpi->sf.max_partition_size &&
                               bsize >= cpi->sf.min_partition_size);
    partition_horz_allowed &= ((bsize <= cpi->sf.max_partition_size &&
                                bsize >  cpi->sf.min_partition_size) ||
                                force_horz_split);
    partition_vert_allowed &= ((bsize <= cpi->sf.max_partition_size &&
                                bsize >  cpi->sf.min_partition_size) ||
                                force_vert_split);
    do_split &= bsize > cpi->sf.min_partition_size;
  }
  if (cpi->sf.use_square_partition_only) {
    partition_horz_allowed &= force_horz_split;
    partition_vert_allowed &= force_vert_split;
  }

  save_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);

  if (cpi->sf.disable_split_var_thresh && partition_none_allowed) {
    unsigned int source_variancey;
    vp9_setup_src_planes(x, cpi->Source, mi_row, mi_col);
    source_variancey = get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
    if (source_variancey < cpi->sf.disable_split_var_thresh) {
      do_split = 0;
      if (source_variancey < cpi->sf.disable_split_var_thresh / 2)
        do_rect = 0;
    }
  }

  if (!x->in_active_map && (partition_horz_allowed || partition_vert_allowed))
    do_split = 0;
  // PARTITION_NONE
  if (partition_none_allowed) {
    rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &this_rate, &this_dist, bsize,
                     ctx, best_rd);
    if (this_rate != INT_MAX) {
      if (bsize >= BLOCK_8X8) {
        pl = partition_plane_context(xd, mi_row, mi_col, bsize);
        this_rate += x->partition_cost[pl][PARTITION_NONE];
      }
      sum_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_dist);
      if (sum_rd < best_rd) {
        int64_t stop_thresh = 4096;
        int64_t stop_thresh_rd;

        best_rate = this_rate;
        best_dist = this_dist;
        best_rd = sum_rd;
        if (bsize >= BLOCK_8X8)
          *(get_sb_partitioning(x, bsize)) = bsize;

        // Adjust threshold according to partition size.
        stop_thresh >>= 8 - (b_width_log2_lookup[bsize] +
            b_height_log2_lookup[bsize]);

        stop_thresh_rd = RDCOST(x->rdmult, x->rddiv, 0, stop_thresh);
        // If obtained distortion is very small, choose current partition
        // and stop splitting.
        if (!x->e_mbd.lossless && best_rd < stop_thresh_rd) {
          do_split = 0;
          do_rect = 0;
        }
      }
    }
    if (!x->in_active_map) {
      do_split = 0;
      do_rect = 0;
    }
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
  }

  // store estimated motion vector
  if (cpi->sf.adaptive_motion_search)
    store_pred_mv(x, ctx);

  // PARTITION_SPLIT
  sum_rd = 0;
  // TODO(jingning): use the motion vectors given by the above search as
  // the starting point of motion search in the following partition type check.
  if (do_split) {
    subsize = get_subsize(bsize, PARTITION_SPLIT);
    for (i = 0; i < 4 && sum_rd < best_rd; ++i) {
      const int x_idx = (i & 1) * mi_step;
      const int y_idx = (i >> 1) * mi_step;

      if (mi_row + y_idx >= cm->mi_rows || mi_col + x_idx >= cm->mi_cols)
        continue;

      *get_sb_index(x, subsize) = i;
      if (cpi->sf.adaptive_motion_search)
        load_pred_mv(x, ctx);
      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        get_block_context(x, subsize)->pred_interp_filter =
            ctx->mic.mbmi.interp_filter;
      rd_pick_partition(cpi, tile, tp, mi_row + y_idx, mi_col + x_idx, subsize,
                        &this_rate, &this_dist, i != 3, best_rd - sum_rd);

      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd && i == 4) {
      pl = partition_plane_context(xd, mi_row, mi_col, bsize);
      sum_rate += x->partition_cost[pl][PARTITION_SPLIT];
      sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      if (sum_rd < best_rd) {
        best_rate = sum_rate;
        best_dist = sum_dist;
        best_rd = sum_rd;
        *(get_sb_partitioning(x, bsize)) = subsize;
      }
    } else {
      // skip rectangular partition test when larger block size
      // gives better rd cost
      if (cpi->sf.less_rectangular_check)
        do_rect &= !partition_none_allowed;
    }
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
  }

  // PARTITION_HORZ
  if (partition_horz_allowed && do_rect) {
    subsize = get_subsize(bsize, PARTITION_HORZ);
    *get_sb_index(x, subsize) = 0;
    if (cpi->sf.adaptive_motion_search)
      load_pred_mv(x, ctx);
    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      get_block_context(x, subsize)->pred_interp_filter =
          ctx->mic.mbmi.interp_filter;
    rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &sum_rate, &sum_dist, subsize,
                     get_block_context(x, subsize), best_rd);
    sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);

    if (sum_rd < best_rd && mi_row + mi_step < cm->mi_rows) {
      update_state(cpi, get_block_context(x, subsize), mi_row, mi_col,
                   subsize, 0);
      encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize);

      *get_sb_index(x, subsize) = 1;
      if (cpi->sf.adaptive_motion_search)
        load_pred_mv(x, ctx);
      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        get_block_context(x, subsize)->pred_interp_filter =
            ctx->mic.mbmi.interp_filter;
      rd_pick_sb_modes(cpi, tile, mi_row + mi_step, mi_col, &this_rate,
                       &this_dist, subsize, get_block_context(x, subsize),
                       best_rd - sum_rd);
      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd) {
      pl = partition_plane_context(xd, mi_row, mi_col, bsize);
      sum_rate += x->partition_cost[pl][PARTITION_HORZ];
      sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      if (sum_rd < best_rd) {
        best_rd = sum_rd;
        best_rate = sum_rate;
        best_dist = sum_dist;
        *(get_sb_partitioning(x, bsize)) = subsize;
      }
    }
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
  }

  // PARTITION_VERT
  if (partition_vert_allowed && do_rect) {
    subsize = get_subsize(bsize, PARTITION_VERT);

    *get_sb_index(x, subsize) = 0;
    if (cpi->sf.adaptive_motion_search)
      load_pred_mv(x, ctx);
    if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
        partition_none_allowed)
      get_block_context(x, subsize)->pred_interp_filter =
          ctx->mic.mbmi.interp_filter;
    rd_pick_sb_modes(cpi, tile, mi_row, mi_col, &sum_rate, &sum_dist, subsize,
                     get_block_context(x, subsize), best_rd);
    sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
    if (sum_rd < best_rd && mi_col + mi_step < cm->mi_cols) {
      update_state(cpi, get_block_context(x, subsize), mi_row, mi_col,
                   subsize, 0);
      encode_superblock(cpi, tp, 0, mi_row, mi_col, subsize);

      *get_sb_index(x, subsize) = 1;
      if (cpi->sf.adaptive_motion_search)
        load_pred_mv(x, ctx);
      if (cpi->sf.adaptive_pred_interp_filter && bsize == BLOCK_8X8 &&
          partition_none_allowed)
        get_block_context(x, subsize)->pred_interp_filter =
            ctx->mic.mbmi.interp_filter;
      rd_pick_sb_modes(cpi, tile, mi_row, mi_col + mi_step, &this_rate,
                       &this_dist, subsize, get_block_context(x, subsize),
                       best_rd - sum_rd);
      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd) {
      pl = partition_plane_context(xd, mi_row, mi_col, bsize);
      sum_rate += x->partition_cost[pl][PARTITION_VERT];
      sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      if (sum_rd < best_rd) {
        best_rate = sum_rate;
        best_dist = sum_dist;
        best_rd = sum_rd;
        *(get_sb_partitioning(x, bsize)) = subsize;
      }
    }
    restore_context(cpi, mi_row, mi_col, a, l, sa, sl, bsize);
  }

  // TODO(jbb): This code added so that we avoid static analysis
  // warning related to the fact that best_rd isn't used after this
  // point.  This code should be refactored so that the duplicate
  // checks occur in some sub function and thus are used...
  (void) best_rd;
  *rate = best_rate;
  *dist = best_dist;

  if (best_rate < INT_MAX && best_dist < INT64_MAX && do_recon) {
    int output_enabled = (bsize == BLOCK_64X64);

    // Check the projected output rate for this SB against it's target
    // and and if necessary apply a Q delta using segmentation to get
    // closer to the target.
    if ((cpi->oxcf.aq_mode == COMPLEXITY_AQ) && cm->seg.update_map) {
      vp9_select_in_frame_q_segment(cpi, mi_row, mi_col, output_enabled,
                                    best_rate);
    }

    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
      vp9_cyclic_refresh_set_rate_and_dist_sb(cpi->cyclic_refresh,
                                              best_rate, best_dist);

    encode_sb(cpi, tile, tp, mi_row, mi_col, output_enabled, bsize);
  }
  if (bsize == BLOCK_64X64) {
    assert(tp_orig < *tp);
    assert(best_rate < INT_MAX);
    assert(best_dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }
}

static void encode_rd_sb_row(VP9_COMP *cpi, const TileInfo *const tile,
                             int mi_row, TOKENEXTRA **tp) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  SPEED_FEATURES *const sf = &cpi->sf;
  int mi_col;

  // Initialize the left context for the new SB row
  vpx_memset(&xd->left_context, 0, sizeof(xd->left_context));
  vpx_memset(xd->left_seg_context, 0, sizeof(xd->left_seg_context));

  // Code each SB in the row
  for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
       mi_col += MI_BLOCK_SIZE) {
    int dummy_rate;
    int64_t dummy_dist;

    BLOCK_SIZE i;
    MACROBLOCK *x = &cpi->mb;

    if (sf->adaptive_pred_interp_filter) {
      for (i = BLOCK_4X4; i < BLOCK_8X8; ++i) {
        const int num_4x4_w = num_4x4_blocks_wide_lookup[i];
        const int num_4x4_h = num_4x4_blocks_high_lookup[i];
        const int num_4x4_blk = MAX(4, num_4x4_w * num_4x4_h);
        for (x->sb_index = 0; x->sb_index < 4; ++x->sb_index)
          for (x->mb_index = 0; x->mb_index < 4; ++x->mb_index)
            for (x->b_index = 0; x->b_index < 16 / num_4x4_blk; ++x->b_index)
              get_block_context(x, i)->pred_interp_filter = SWITCHABLE;
      }
    }

    vp9_zero(cpi->mb.pred_mv);

    if ((sf->partition_search_type == SEARCH_PARTITION &&
         sf->use_lastframe_partitioning) ||
         sf->partition_search_type == FIXED_PARTITION ||
         sf->partition_search_type == VAR_BASED_PARTITION ||
         sf->partition_search_type == VAR_BASED_FIXED_PARTITION) {
      const int idx_str = cm->mi_stride * mi_row + mi_col;
      MODE_INFO **mi_8x8 = cm->mi_grid_visible + idx_str;
      MODE_INFO **prev_mi_8x8 = cm->prev_mi_grid_visible + idx_str;
      cpi->mb.source_variance = UINT_MAX;
      if (sf->partition_search_type == FIXED_PARTITION) {
        set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
        set_fixed_partitioning(cpi, tile, mi_8x8, mi_row, mi_col,
                               sf->always_this_block_size);
        rd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col, BLOCK_64X64,
                         &dummy_rate, &dummy_dist, 1);
      } else if (sf->partition_search_type == VAR_BASED_FIXED_PARTITION) {
        BLOCK_SIZE bsize;
        set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
        bsize = get_rd_var_based_fixed_partition(cpi, mi_row, mi_col);
        set_fixed_partitioning(cpi, tile, mi_8x8, mi_row, mi_col, bsize);
        rd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col, BLOCK_64X64,
                         &dummy_rate, &dummy_dist, 1);
      } else if (sf->partition_search_type == VAR_BASED_PARTITION) {
        choose_partitioning(cpi, tile, mi_row, mi_col);
        rd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col, BLOCK_64X64,
                         &dummy_rate, &dummy_dist, 1);
      } else {
        if ((cm->current_video_frame
            % sf->last_partitioning_redo_frequency) == 0
            || cm->prev_mi == 0
            || cm->show_frame == 0
            || cm->frame_type == KEY_FRAME
            || cpi->rc.is_src_frame_alt_ref
            || ((sf->use_lastframe_partitioning ==
                 LAST_FRAME_PARTITION_LOW_MOTION) &&
                 sb_has_motion(cm, prev_mi_8x8))) {
          // If required set upper and lower partition size limits
          if (sf->auto_min_max_partition_size) {
            set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
            rd_auto_partition_range(cpi, tile, mi_row, mi_col,
                                    &sf->min_partition_size,
                                    &sf->max_partition_size);
          }
          rd_pick_partition(cpi, tile, tp, mi_row, mi_col, BLOCK_64X64,
                            &dummy_rate, &dummy_dist, 1, INT64_MAX);
        } else {
          if (sf->constrain_copy_partition &&
              sb_has_motion(cm, prev_mi_8x8))
            constrain_copy_partitioning(cpi, tile, mi_8x8, prev_mi_8x8,
                                        mi_row, mi_col, BLOCK_16X16);
          else
            copy_partitioning(cm, mi_8x8, prev_mi_8x8);
          rd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col, BLOCK_64X64,
                           &dummy_rate, &dummy_dist, 1);
        }
      }
    } else {
      // If required set upper and lower partition size limits
      if (sf->auto_min_max_partition_size) {
        set_offsets(cpi, tile, mi_row, mi_col, BLOCK_64X64);
        rd_auto_partition_range(cpi, tile, mi_row, mi_col,
                                &sf->min_partition_size,
                                &sf->max_partition_size);
      }
      rd_pick_partition(cpi, tile, tp, mi_row, mi_col, BLOCK_64X64,
                        &dummy_rate, &dummy_dist, 1, INT64_MAX);
    }
  }
}

static void init_encode_frame_mb_context(VP9_COMP *cpi) {
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int aligned_mi_cols = mi_cols_aligned_to_sb(cm->mi_cols);

  x->act_zbin_adj = 0;

  // Copy data over into macro block data structures.
  vp9_setup_src_planes(x, cpi->Source, 0, 0);

  // TODO(jkoleszar): are these initializations required?
  vp9_setup_pre_planes(xd, 0, get_ref_frame_buffer(cpi, LAST_FRAME), 0, 0,
                       NULL);
  vp9_setup_dst_planes(xd, get_frame_new_buffer(cm), 0, 0);

  vp9_setup_block_planes(&x->e_mbd, cm->subsampling_x, cm->subsampling_y);

  xd->mi[0]->mbmi.mode = DC_PRED;
  xd->mi[0]->mbmi.uv_mode = DC_PRED;

  // Note: this memset assumes above_context[0], [1] and [2]
  // are allocated as part of the same buffer.
  vpx_memset(xd->above_context[0], 0,
             sizeof(*xd->above_context[0]) *
             2 * aligned_mi_cols * MAX_MB_PLANE);
  vpx_memset(xd->above_seg_context, 0,
             sizeof(*xd->above_seg_context) * aligned_mi_cols);
}

static void switch_lossless_mode(VP9_COMP *cpi, int lossless) {
  if (lossless) {
    // printf("Switching to lossless\n");
    cpi->mb.fwd_txm4x4 = vp9_fwht4x4;
    cpi->mb.e_mbd.itxm_add = vp9_iwht4x4_add;
    cpi->mb.optimize = 0;
    cpi->common.lf.filter_level = 0;
    cpi->zbin_mode_boost_enabled = 0;
    cpi->common.tx_mode = ONLY_4X4;
  } else {
    // printf("Not lossless\n");
    cpi->mb.fwd_txm4x4 = vp9_fdct4x4;
    cpi->mb.e_mbd.itxm_add = vp9_idct4x4_add;
  }
}

static int check_dual_ref_flags(VP9_COMP *cpi) {
  const int ref_flags = cpi->ref_frame_flags;

  if (vp9_segfeature_active(&cpi->common.seg, 1, SEG_LVL_REF_FRAME)) {
    return 0;
  } else {
    return (!!(ref_flags & VP9_GOLD_FLAG) + !!(ref_flags & VP9_LAST_FLAG)
        + !!(ref_flags & VP9_ALT_FLAG)) >= 2;
  }
}

static void reset_skip_txfm_size(VP9_COMMON *cm, TX_SIZE txfm_max) {
  int mi_row, mi_col;
  const int mis = cm->mi_stride;
  MODE_INFO **mi_ptr = cm->mi_grid_visible;

  for (mi_row = 0; mi_row < cm->mi_rows; ++mi_row, mi_ptr += mis) {
    for (mi_col = 0; mi_col < cm->mi_cols; ++mi_col) {
      if (mi_ptr[mi_col]->mbmi.tx_size > txfm_max)
        mi_ptr[mi_col]->mbmi.tx_size = txfm_max;
    }
  }
}

static MV_REFERENCE_FRAME get_frame_type(const VP9_COMP *cpi) {
  if (frame_is_intra_only(&cpi->common))
    return INTRA_FRAME;
  else if (cpi->rc.is_src_frame_alt_ref && cpi->refresh_golden_frame)
    return ALTREF_FRAME;
  else if (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)
    return LAST_FRAME;
  else
    return GOLDEN_FRAME;
}

static TX_MODE select_tx_mode(const VP9_COMP *cpi) {
  if (cpi->oxcf.lossless) {
    return ONLY_4X4;
  } else if (cpi->common.current_video_frame == 0) {
    return TX_MODE_SELECT;
  } else {
    if (cpi->sf.tx_size_search_method == USE_LARGESTALL) {
      return ALLOW_32X32;
    } else if (cpi->sf.tx_size_search_method == USE_FULL_RD) {
      const RD_OPT *const rd_opt = &cpi->rd;
      const MV_REFERENCE_FRAME frame_type = get_frame_type(cpi);
      return rd_opt->tx_select_threshes[frame_type][ALLOW_32X32] >
                 rd_opt->tx_select_threshes[frame_type][TX_MODE_SELECT] ?
                     ALLOW_32X32 : TX_MODE_SELECT;
    } else {
      unsigned int total = 0;
      int i;
      for (i = 0; i < TX_SIZES; ++i)
        total += cpi->tx_stepdown_count[i];

      if (total) {
        const double fraction = (double)cpi->tx_stepdown_count[0] / total;
        return fraction > 0.90 ? ALLOW_32X32 : TX_MODE_SELECT;
      } else {
        return cpi->common.tx_mode;
      }
    }
  }
}

// Start RTC Exploration
typedef enum {
  BOTH_ZERO = 0,
  ZERO_PLUS_PREDICTED = 1,
  BOTH_PREDICTED = 2,
  NEW_PLUS_NON_INTRA = 3,
  BOTH_NEW = 4,
  INTRA_PLUS_NON_INTRA = 5,
  BOTH_INTRA = 6,
  INVALID_CASE = 9
} motion_vector_context;

static void set_mode_info(MB_MODE_INFO *mbmi, BLOCK_SIZE bsize,
                          MB_PREDICTION_MODE mode) {
  mbmi->mode = mode;
  mbmi->uv_mode = mode;
  mbmi->mv[0].as_int = 0;
  mbmi->mv[1].as_int = 0;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE;
  mbmi->tx_size = max_txsize_lookup[bsize];
  mbmi->skip = 0;
  mbmi->sb_type = bsize;
  mbmi->segment_id = 0;
}

static void nonrd_pick_sb_modes(VP9_COMP *cpi, const TileInfo *const tile,
                                int mi_row, int mi_col,
                                int *rate, int64_t *dist,
                                BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  set_offsets(cpi, tile, mi_row, mi_col, bsize);
  xd->mi[0]->mbmi.sb_type = bsize;

  if (!frame_is_intra_only(cm)) {
    vp9_pick_inter_mode(cpi, x, tile, mi_row, mi_col,
                        rate, dist, bsize);
  } else {
    MB_PREDICTION_MODE intramode = DC_PRED;
    set_mode_info(&xd->mi[0]->mbmi, bsize, intramode);
  }
  duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col, bsize);
}

static void fill_mode_info_sb(VP9_COMMON *cm, MACROBLOCK *x,
                              int mi_row, int mi_col,
                              BLOCK_SIZE bsize, BLOCK_SIZE subsize) {
  MACROBLOCKD *xd = &x->e_mbd;
  int bsl = b_width_log2(bsize), hbs = (1 << bsl) / 4;
  PARTITION_TYPE partition = partition_lookup[bsl][subsize];

  assert(bsize >= BLOCK_8X8);

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  switch (partition) {
    case PARTITION_NONE:
      set_modeinfo_offsets(cm, xd, mi_row, mi_col);
      *(xd->mi[0]) = get_block_context(x, subsize)->mic;
      duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col, bsize);
      break;
    case PARTITION_VERT:
      *get_sb_index(x, subsize) = 0;
      set_modeinfo_offsets(cm, xd, mi_row, mi_col);
      *(xd->mi[0]) = get_block_context(x, subsize)->mic;
      duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col, bsize);

      if (mi_col + hbs < cm->mi_cols) {
        *get_sb_index(x, subsize) = 1;
        set_modeinfo_offsets(cm, xd, mi_row, mi_col + hbs);
        *(xd->mi[0]) = get_block_context(x, subsize)->mic;
        duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col + hbs, bsize);
      }
      break;
    case PARTITION_HORZ:
      *get_sb_index(x, subsize) = 0;
      set_modeinfo_offsets(cm, xd, mi_row, mi_col);
      *(xd->mi[0]) = get_block_context(x, subsize)->mic;
      duplicate_mode_info_in_sb(cm, xd, mi_row, mi_col, bsize);
      if (mi_row + hbs < cm->mi_rows) {
        *get_sb_index(x, subsize) = 1;
        set_modeinfo_offsets(cm, xd, mi_row + hbs, mi_col);
        *(xd->mi[0]) = get_block_context(x, subsize)->mic;
        duplicate_mode_info_in_sb(cm, xd, mi_row + hbs, mi_col, bsize);
      }
      break;
    case PARTITION_SPLIT:
      *get_sb_index(x, subsize) = 0;
      fill_mode_info_sb(cm, x, mi_row, mi_col, subsize,
                        *(get_sb_partitioning(x, subsize)));
      *get_sb_index(x, subsize) = 1;
      fill_mode_info_sb(cm, x, mi_row, mi_col + hbs, subsize,
                        *(get_sb_partitioning(x, subsize)));
      *get_sb_index(x, subsize) = 2;
      fill_mode_info_sb(cm, x, mi_row + hbs, mi_col, subsize,
                        *(get_sb_partitioning(x, subsize)));
      *get_sb_index(x, subsize) = 3;
      fill_mode_info_sb(cm, x, mi_row + hbs, mi_col + hbs, subsize,
                        *(get_sb_partitioning(x, subsize)));
      break;
    default:
      break;
  }
}

static void nonrd_pick_partition(VP9_COMP *cpi, const TileInfo *const tile,
                                 TOKENEXTRA **tp, int mi_row,
                                 int mi_col, BLOCK_SIZE bsize, int *rate,
                                 int64_t *dist, int do_recon, int64_t best_rd) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int ms = num_8x8_blocks_wide_lookup[bsize] / 2;
  TOKENEXTRA *tp_orig = *tp;
  PICK_MODE_CONTEXT *ctx = get_block_context(x, bsize);
  int i;
  BLOCK_SIZE subsize;
  int this_rate, sum_rate = 0, best_rate = INT_MAX;
  int64_t this_dist, sum_dist = 0, best_dist = INT64_MAX;
  int64_t sum_rd = 0;
  int do_split = bsize >= BLOCK_8X8;
  int do_rect = 1;
  // Override skipping rectangular partition operations for edge blocks
  const int force_horz_split = (mi_row + ms >= cm->mi_rows);
  const int force_vert_split = (mi_col + ms >= cm->mi_cols);
  const int xss = x->e_mbd.plane[1].subsampling_x;
  const int yss = x->e_mbd.plane[1].subsampling_y;

  int partition_none_allowed = !force_horz_split && !force_vert_split;
  int partition_horz_allowed = !force_vert_split && yss <= xss &&
                               bsize >= BLOCK_8X8;
  int partition_vert_allowed = !force_horz_split && xss <= yss &&
                               bsize >= BLOCK_8X8;
  (void) *tp_orig;

  if (bsize < BLOCK_8X8) {
    // When ab_index = 0 all sub-blocks are handled, so for ab_index != 0
    // there is nothing to be done.
    if (x->ab_index != 0) {
      *rate = 0;
      *dist = 0;
      return;
    }
  }

  assert(num_8x8_blocks_wide_lookup[bsize] ==
             num_8x8_blocks_high_lookup[bsize]);

  x->in_active_map = check_active_map(cpi, x, mi_row, mi_col, bsize);

  // Determine partition types in search according to the speed features.
  // The threshold set here has to be of square block size.
  if (cpi->sf.auto_min_max_partition_size) {
    partition_none_allowed &= (bsize <= cpi->sf.max_partition_size &&
                               bsize >= cpi->sf.min_partition_size);
    partition_horz_allowed &= ((bsize <= cpi->sf.max_partition_size &&
                                bsize >  cpi->sf.min_partition_size) ||
                                force_horz_split);
    partition_vert_allowed &= ((bsize <= cpi->sf.max_partition_size &&
                                bsize >  cpi->sf.min_partition_size) ||
                                force_vert_split);
    do_split &= bsize > cpi->sf.min_partition_size;
  }
  if (cpi->sf.use_square_partition_only) {
    partition_horz_allowed &= force_horz_split;
    partition_vert_allowed &= force_vert_split;
  }

  if (!x->in_active_map && (partition_horz_allowed || partition_vert_allowed))
    do_split = 0;

  // PARTITION_NONE
  if (partition_none_allowed) {
    nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col,
                        &this_rate, &this_dist, bsize);
    ctx->mic.mbmi = xd->mi[0]->mbmi;

    if (this_rate != INT_MAX) {
      int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
      this_rate += x->partition_cost[pl][PARTITION_NONE];
      sum_rd = RDCOST(x->rdmult, x->rddiv, this_rate, this_dist);
      if (sum_rd < best_rd) {
        int64_t stop_thresh = 4096;
        int64_t stop_thresh_rd;

        best_rate = this_rate;
        best_dist = this_dist;
        best_rd = sum_rd;
        if (bsize >= BLOCK_8X8)
          *(get_sb_partitioning(x, bsize)) = bsize;

        // Adjust threshold according to partition size.
        stop_thresh >>= 8 - (b_width_log2_lookup[bsize] +
            b_height_log2_lookup[bsize]);

        stop_thresh_rd = RDCOST(x->rdmult, x->rddiv, 0, stop_thresh);
        // If obtained distortion is very small, choose current partition
        // and stop splitting.
        if (!x->e_mbd.lossless && best_rd < stop_thresh_rd) {
          do_split = 0;
          do_rect = 0;
        }
      }
    }
    if (!x->in_active_map) {
      do_split = 0;
      do_rect = 0;
    }
  }

  // store estimated motion vector
  store_pred_mv(x, ctx);

  // PARTITION_SPLIT
  sum_rd = 0;
  if (do_split) {
    int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
    sum_rate += x->partition_cost[pl][PARTITION_SPLIT];
    subsize = get_subsize(bsize, PARTITION_SPLIT);
    for (i = 0; i < 4 && sum_rd < best_rd; ++i) {
      const int x_idx = (i & 1) * ms;
      const int y_idx = (i >> 1) * ms;

      if (mi_row + y_idx >= cm->mi_rows || mi_col + x_idx >= cm->mi_cols)
        continue;

      *get_sb_index(x, subsize) = i;
      load_pred_mv(x, ctx);

      nonrd_pick_partition(cpi, tile, tp, mi_row + y_idx, mi_col + x_idx,
                           subsize, &this_rate, &this_dist, 0,
                           best_rd - sum_rd);

      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }

    if (sum_rd < best_rd) {
      best_rate = sum_rate;
      best_dist = sum_dist;
      best_rd = sum_rd;
      *(get_sb_partitioning(x, bsize)) = subsize;
    } else {
      // skip rectangular partition test when larger block size
      // gives better rd cost
      if (cpi->sf.less_rectangular_check)
        do_rect &= !partition_none_allowed;
    }
  }

  // PARTITION_HORZ
  if (partition_horz_allowed && do_rect) {
    subsize = get_subsize(bsize, PARTITION_HORZ);
    *get_sb_index(x, subsize) = 0;
    if (cpi->sf.adaptive_motion_search)
      load_pred_mv(x, ctx);

    nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col,
                        &this_rate, &this_dist, subsize);

    get_block_context(x, subsize)->mic.mbmi = xd->mi[0]->mbmi;

    sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);

    if (sum_rd < best_rd && mi_row + ms < cm->mi_rows) {
      *get_sb_index(x, subsize) = 1;

      load_pred_mv(x, ctx);

      nonrd_pick_sb_modes(cpi, tile, mi_row + ms, mi_col,
                          &this_rate, &this_dist, subsize);

      get_block_context(x, subsize)->mic.mbmi = xd->mi[0]->mbmi;

      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
        this_rate += x->partition_cost[pl][PARTITION_HORZ];
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd) {
      best_rd = sum_rd;
      best_rate = sum_rate;
      best_dist = sum_dist;
      *(get_sb_partitioning(x, bsize)) = subsize;
    }
  }

  // PARTITION_VERT
  if (partition_vert_allowed && do_rect) {
    subsize = get_subsize(bsize, PARTITION_VERT);

    *get_sb_index(x, subsize) = 0;
    if (cpi->sf.adaptive_motion_search)
      load_pred_mv(x, ctx);

    nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col,
                        &this_rate, &this_dist, subsize);
    get_block_context(x, subsize)->mic.mbmi = xd->mi[0]->mbmi;
    sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
    if (sum_rd < best_rd && mi_col + ms < cm->mi_cols) {
      *get_sb_index(x, subsize) = 1;

      load_pred_mv(x, ctx);

      nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col + ms,
                          &this_rate, &this_dist, subsize);

      get_block_context(x, subsize)->mic.mbmi = xd->mi[0]->mbmi;

      if (this_rate == INT_MAX) {
        sum_rd = INT64_MAX;
      } else {
        int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
        this_rate += x->partition_cost[pl][PARTITION_VERT];
        sum_rate += this_rate;
        sum_dist += this_dist;
        sum_rd = RDCOST(x->rdmult, x->rddiv, sum_rate, sum_dist);
      }
    }
    if (sum_rd < best_rd) {
      best_rate = sum_rate;
      best_dist = sum_dist;
      best_rd = sum_rd;
      *(get_sb_partitioning(x, bsize)) = subsize;
    }
  }

  *rate = best_rate;
  *dist = best_dist;

  if (best_rate == INT_MAX)
    return;

  // update mode info array
  fill_mode_info_sb(cm, x, mi_row, mi_col, bsize,
                    *(get_sb_partitioning(x, bsize)));

  if (best_rate < INT_MAX && best_dist < INT64_MAX && do_recon) {
    int output_enabled = (bsize == BLOCK_64X64);

    // Check the projected output rate for this SB against it's target
    // and and if necessary apply a Q delta using segmentation to get
    // closer to the target.
    if ((cpi->oxcf.aq_mode == COMPLEXITY_AQ) && cm->seg.update_map) {
      vp9_select_in_frame_q_segment(cpi, mi_row, mi_col, output_enabled,
                                    best_rate);
    }

    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
      vp9_cyclic_refresh_set_rate_and_dist_sb(cpi->cyclic_refresh,
                                              best_rate, best_dist);

    encode_sb_rt(cpi, tile, tp, mi_row, mi_col, output_enabled, bsize);
  }

  if (bsize == BLOCK_64X64) {
    assert(tp_orig < *tp);
    assert(best_rate < INT_MAX);
    assert(best_dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }
}

static void nonrd_use_partition(VP9_COMP *cpi,
                                const TileInfo *const tile,
                                MODE_INFO **mi_8x8,
                                TOKENEXTRA **tp,
                                int mi_row, int mi_col,
                                BLOCK_SIZE bsize, int output_enabled,
                                int *totrate, int64_t *totdist) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int bsl = b_width_log2(bsize), hbs = (1 << bsl) / 4;
  const int mis = cm->mi_stride;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
  int rate = INT_MAX;
  int64_t dist = INT64_MAX;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  subsize = (bsize >= BLOCK_8X8) ? mi_8x8[0]->mbmi.sb_type : BLOCK_4X4;
  partition = partition_lookup[bsl][subsize];

  switch (partition) {
    case PARTITION_NONE:
      nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col, totrate, totdist, subsize);
      get_block_context(x, subsize)->mic.mbmi = xd->mi[0]->mbmi;
      break;
    case PARTITION_VERT:
      *get_sb_index(x, subsize) = 0;
      nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col, totrate, totdist, subsize);
      get_block_context(x, subsize)->mic.mbmi = xd->mi[0]->mbmi;
      if (mi_col + hbs < cm->mi_cols) {
        *get_sb_index(x, subsize) = 1;
        nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col + hbs,
                            &rate, &dist, subsize);
        get_block_context(x, subsize)->mic.mbmi = xd->mi[0]->mbmi;
        if (rate != INT_MAX && dist != INT64_MAX &&
            *totrate != INT_MAX && *totdist != INT64_MAX) {
          *totrate += rate;
          *totdist += dist;
        }
      }
      break;
    case PARTITION_HORZ:
      *get_sb_index(x, subsize) = 0;
      nonrd_pick_sb_modes(cpi, tile, mi_row, mi_col, totrate, totdist, subsize);
      get_block_context(x, subsize)->mic.mbmi = xd->mi[0]->mbmi;
      if (mi_row + hbs < cm->mi_rows) {
        *get_sb_index(x, subsize) = 1;
        nonrd_pick_sb_modes(cpi, tile, mi_row + hbs, mi_col,
                            &rate, &dist, subsize);
        get_block_context(x, subsize)->mic.mbmi = mi_8x8[0]->mbmi;
        if (rate != INT_MAX && dist != INT64_MAX &&
            *totrate != INT_MAX && *totdist != INT64_MAX) {
          *totrate += rate;
          *totdist += dist;
        }
      }
      break;
    case PARTITION_SPLIT:
      subsize = get_subsize(bsize, PARTITION_SPLIT);
      *get_sb_index(x, subsize) = 0;
      nonrd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col,
                          subsize, output_enabled, totrate, totdist);
      *get_sb_index(x, subsize) = 1;
      nonrd_use_partition(cpi, tile, mi_8x8 + hbs, tp,
                          mi_row, mi_col + hbs, subsize, output_enabled,
                          &rate, &dist);
      if (rate != INT_MAX && dist != INT64_MAX &&
          *totrate != INT_MAX && *totdist != INT64_MAX) {
        *totrate += rate;
        *totdist += dist;
      }
      *get_sb_index(x, subsize) = 2;
      nonrd_use_partition(cpi, tile, mi_8x8 + hbs * mis, tp,
                          mi_row + hbs, mi_col, subsize, output_enabled,
                          &rate, &dist);
      if (rate != INT_MAX && dist != INT64_MAX &&
          *totrate != INT_MAX && *totdist != INT64_MAX) {
        *totrate += rate;
        *totdist += dist;
      }
      *get_sb_index(x, subsize) = 3;
      nonrd_use_partition(cpi, tile, mi_8x8 + hbs * mis + hbs, tp,
                          mi_row + hbs, mi_col + hbs, subsize, output_enabled,
                          &rate, &dist);
      if (rate != INT_MAX && dist != INT64_MAX &&
          *totrate != INT_MAX && *totdist != INT64_MAX) {
        *totrate += rate;
        *totdist += dist;
      }
      break;
    default:
      assert("Invalid partition type.");
  }

  if (bsize == BLOCK_64X64 && output_enabled) {
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ)
      vp9_cyclic_refresh_set_rate_and_dist_sb(cpi->cyclic_refresh,
                                              *totrate, *totdist);
    encode_sb_rt(cpi, tile, tp, mi_row, mi_col, 1, bsize);
  }
}

static void encode_nonrd_sb_row(VP9_COMP *cpi, const TileInfo *const tile,
                                int mi_row, TOKENEXTRA **tp) {
  VP9_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &cpi->mb.e_mbd;
  int mi_col;

  // Initialize the left context for the new SB row
  vpx_memset(&xd->left_context, 0, sizeof(xd->left_context));
  vpx_memset(xd->left_seg_context, 0, sizeof(xd->left_seg_context));

  // Code each SB in the row
  for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
       mi_col += MI_BLOCK_SIZE) {
    int dummy_rate = 0;
    int64_t dummy_dist = 0;
    const int idx_str = cm->mi_stride * mi_row + mi_col;
    MODE_INFO **mi_8x8 = cm->mi_grid_visible + idx_str;
    MODE_INFO **prev_mi_8x8 = cm->prev_mi_grid_visible + idx_str;
    BLOCK_SIZE bsize;

    cpi->mb.source_variance = UINT_MAX;
    vp9_zero(cpi->mb.pred_mv);

    // Set the partition type of the 64X64 block
    switch (cpi->sf.partition_search_type) {
      case VAR_BASED_PARTITION:
        choose_partitioning(cpi, tile, mi_row, mi_col);
        nonrd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col, BLOCK_64X64,
                            1, &dummy_rate, &dummy_dist);
        break;
      case SOURCE_VAR_BASED_PARTITION:
        set_source_var_based_partition(cpi, tile, mi_8x8, mi_row, mi_col);
        nonrd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col, BLOCK_64X64,
                            1, &dummy_rate, &dummy_dist);
        break;
      case VAR_BASED_FIXED_PARTITION:
      case FIXED_PARTITION:
        bsize = cpi->sf.partition_search_type == FIXED_PARTITION ?
                cpi->sf.always_this_block_size :
                get_nonrd_var_based_fixed_partition(cpi, mi_row, mi_col);
        set_fixed_partitioning(cpi, tile, mi_8x8, mi_row, mi_col, bsize);
        nonrd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col, BLOCK_64X64,
                            1, &dummy_rate, &dummy_dist);
        break;
      case REFERENCE_PARTITION:
        if (cpi->sf.partition_check || sb_has_motion(cm, prev_mi_8x8)) {
          nonrd_pick_partition(cpi, tile, tp, mi_row, mi_col, BLOCK_64X64,
                               &dummy_rate, &dummy_dist, 1, INT64_MAX);
        } else {
          copy_partitioning(cm, mi_8x8, prev_mi_8x8);
          nonrd_use_partition(cpi, tile, mi_8x8, tp, mi_row, mi_col,
                              BLOCK_64X64, 1, &dummy_rate, &dummy_dist);
        }
        break;
      default:
        assert(0);
    }
  }
}
// end RTC play code

static void encode_frame_internal(VP9_COMP *cpi) {
  SPEED_FEATURES *const sf = &cpi->sf;
  RD_OPT *const rd_opt = &cpi->rd;
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;

  xd->mi = cm->mi_grid_visible;
  xd->mi[0] = cm->mi;

  vp9_zero(cm->counts);
  vp9_zero(cpi->coef_counts);
  vp9_zero(cpi->tx_stepdown_count);
  vp9_zero(rd_opt->comp_pred_diff);
  vp9_zero(rd_opt->filter_diff);
  vp9_zero(rd_opt->tx_select_diff);
  vp9_zero(rd_opt->tx_select_threshes);

  cm->tx_mode = select_tx_mode(cpi);

  cpi->mb.e_mbd.lossless = cm->base_qindex == 0 &&
                           cm->y_dc_delta_q == 0 &&
                           cm->uv_dc_delta_q == 0 &&
                           cm->uv_ac_delta_q == 0;
  switch_lossless_mode(cpi, cpi->mb.e_mbd.lossless);

  vp9_frame_init_quantizer(cpi);

  vp9_initialize_rd_consts(cpi);
  vp9_initialize_me_consts(cpi, cm->base_qindex);
  init_encode_frame_mb_context(cpi);

  if (cpi->oxcf.tuning == VP8_TUNE_SSIM)
    build_activity_map(cpi);

  set_prev_mi(cm);

  if (sf->use_nonrd_pick_mode) {
    // Initialize internal buffer pointers for rtc coding, where non-RD
    // mode decision is used and hence no buffer pointer swap needed.
    int i;
    struct macroblock_plane *const p = x->plane;
    struct macroblockd_plane *const pd = xd->plane;
    PICK_MODE_CONTEXT *ctx = &cpi->mb.sb64_context;

    for (i = 0; i < MAX_MB_PLANE; ++i) {
      p[i].coeff = ctx->coeff_pbuf[i][0];
      p[i].qcoeff = ctx->qcoeff_pbuf[i][0];
      pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][0];
      p[i].eobs = ctx->eobs_pbuf[i][0];
    }
    vp9_zero(x->zcoeff_blk);

    if (sf->partition_search_type == SOURCE_VAR_BASED_PARTITION &&
        cm->current_video_frame > 0) {
      int check_freq = sf->search_type_check_frequency;

      if ((cm->current_video_frame - 1) % check_freq == 0) {
        cpi->use_large_partition_rate = 0;
      }

      if ((cm->current_video_frame - 1) % check_freq == 1) {
        const int mbs_in_b32x32 = 1 << ((b_width_log2_lookup[BLOCK_32X32] -
                                  b_width_log2_lookup[BLOCK_16X16]) +
                                  (b_height_log2_lookup[BLOCK_32X32] -
                                  b_height_log2_lookup[BLOCK_16X16]));
        cpi->use_large_partition_rate = cpi->use_large_partition_rate * 100 *
                                        mbs_in_b32x32 / cm->MBs;
      }

      if ((cm->current_video_frame - 1) % check_freq >= 1) {
        if (cpi->use_large_partition_rate < 15)
          sf->partition_search_type = FIXED_PARTITION;
      }
    }
  }

  {
    struct vpx_usec_timer emr_timer;
    vpx_usec_timer_start(&emr_timer);

    {
      // Take tiles into account and give start/end MB
      int tile_col, tile_row;
      TOKENEXTRA *tp = cpi->tok;
      const int tile_cols = 1 << cm->log2_tile_cols;
      const int tile_rows = 1 << cm->log2_tile_rows;

      for (tile_row = 0; tile_row < tile_rows; tile_row++) {
        for (tile_col = 0; tile_col < tile_cols; tile_col++) {
          TileInfo tile;
          TOKENEXTRA *tp_old = tp;
          int mi_row;

          // For each row of SBs in the frame
          vp9_tile_init(&tile, cm, tile_row, tile_col);
          for (mi_row = tile.mi_row_start;
               mi_row < tile.mi_row_end; mi_row += MI_BLOCK_SIZE) {
            if (sf->use_nonrd_pick_mode && cm->frame_type != KEY_FRAME)
              encode_nonrd_sb_row(cpi, &tile, mi_row, &tp);
            else
              encode_rd_sb_row(cpi, &tile, mi_row, &tp);
          }
          cpi->tok_count[tile_row][tile_col] = (unsigned int)(tp - tp_old);
          assert(tp - cpi->tok <= get_token_alloc(cm->mb_rows, cm->mb_cols));
        }
      }
    }

    vpx_usec_timer_mark(&emr_timer);
    cpi->time_encode_sb_row += vpx_usec_timer_elapsed(&emr_timer);
  }

  if (sf->skip_encode_sb) {
    int j;
    unsigned int intra_count = 0, inter_count = 0;
    for (j = 0; j < INTRA_INTER_CONTEXTS; ++j) {
      intra_count += cm->counts.intra_inter[j][0];
      inter_count += cm->counts.intra_inter[j][1];
    }
    sf->skip_encode_frame = (intra_count << 2) < inter_count &&
                            cm->frame_type != KEY_FRAME &&
                            cm->show_frame;
  } else {
    sf->skip_encode_frame = 0;
  }

#if 0
  // Keep record of the total distortion this time around for future use
  cpi->last_frame_distortion = cpi->frame_distortion;
#endif
}

void vp9_encode_frame(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  RD_OPT *const rd_opt = &cpi->rd;

  // In the longer term the encoder should be generalized to match the
  // decoder such that we allow compound where one of the 3 buffers has a
  // different sign bias and that buffer is then the fixed ref. However, this
  // requires further work in the rd loop. For now the only supported encoder
  // side behavior is where the ALT ref buffer has opposite sign bias to
  // the other two.
  if (!frame_is_intra_only(cm)) {
    if ((cm->ref_frame_sign_bias[ALTREF_FRAME] ==
             cm->ref_frame_sign_bias[GOLDEN_FRAME]) ||
        (cm->ref_frame_sign_bias[ALTREF_FRAME] ==
             cm->ref_frame_sign_bias[LAST_FRAME])) {
      cm->allow_comp_inter_inter = 0;
    } else {
      cm->allow_comp_inter_inter = 1;
      cm->comp_fixed_ref = ALTREF_FRAME;
      cm->comp_var_ref[0] = LAST_FRAME;
      cm->comp_var_ref[1] = GOLDEN_FRAME;
    }
  }

  if (cpi->sf.frame_parameter_update) {
    int i;

    // This code does a single RD pass over the whole frame assuming
    // either compound, single or hybrid prediction as per whatever has
    // worked best for that type of frame in the past.
    // It also predicts whether another coding mode would have worked
    // better that this coding mode. If that is the case, it remembers
    // that for subsequent frames.
    // It does the same analysis for transform size selection also.
    const MV_REFERENCE_FRAME frame_type = get_frame_type(cpi);
    const int64_t *mode_thresh = rd_opt->prediction_type_threshes[frame_type];
    const int64_t *filter_thresh = rd_opt->filter_threshes[frame_type];

    /* prediction (compound, single or hybrid) mode selection */
    if (frame_type == ALTREF_FRAME || !cm->allow_comp_inter_inter)
      cm->reference_mode = SINGLE_REFERENCE;
    else if (mode_thresh[COMPOUND_REFERENCE] > mode_thresh[SINGLE_REFERENCE] &&
             mode_thresh[COMPOUND_REFERENCE] >
                 mode_thresh[REFERENCE_MODE_SELECT] &&
             check_dual_ref_flags(cpi) &&
             cpi->static_mb_pct == 100)
      cm->reference_mode = COMPOUND_REFERENCE;
    else if (mode_thresh[SINGLE_REFERENCE] > mode_thresh[REFERENCE_MODE_SELECT])
      cm->reference_mode = SINGLE_REFERENCE;
    else
      cm->reference_mode = REFERENCE_MODE_SELECT;

    if (cm->interp_filter == SWITCHABLE) {
      if (frame_type != ALTREF_FRAME &&
          filter_thresh[EIGHTTAP_SMOOTH] > filter_thresh[EIGHTTAP] &&
          filter_thresh[EIGHTTAP_SMOOTH] > filter_thresh[EIGHTTAP_SHARP] &&
          filter_thresh[EIGHTTAP_SMOOTH] > filter_thresh[SWITCHABLE - 1]) {
        cm->interp_filter = EIGHTTAP_SMOOTH;
      } else if (filter_thresh[EIGHTTAP_SHARP] > filter_thresh[EIGHTTAP] &&
          filter_thresh[EIGHTTAP_SHARP] > filter_thresh[SWITCHABLE - 1]) {
        cm->interp_filter = EIGHTTAP_SHARP;
      } else if (filter_thresh[EIGHTTAP] > filter_thresh[SWITCHABLE - 1]) {
        cm->interp_filter = EIGHTTAP;
      }
    }

    encode_frame_internal(cpi);

    for (i = 0; i < REFERENCE_MODES; ++i) {
      const int diff = (int) (rd_opt->comp_pred_diff[i] / cm->MBs);
      rd_opt->prediction_type_threshes[frame_type][i] += diff;
      rd_opt->prediction_type_threshes[frame_type][i] >>= 1;
    }

    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
      const int64_t diff = rd_opt->filter_diff[i] / cm->MBs;
      rd_opt->filter_threshes[frame_type][i] =
          (rd_opt->filter_threshes[frame_type][i] + diff) / 2;
    }

    for (i = 0; i < TX_MODES; ++i) {
      int64_t pd = rd_opt->tx_select_diff[i];
      int diff;
      if (i == TX_MODE_SELECT)
        pd -= RDCOST(cpi->mb.rdmult, cpi->mb.rddiv, 2048 * (TX_SIZES - 1), 0);
      diff = (int) (pd / cm->MBs);
      rd_opt->tx_select_threshes[frame_type][i] += diff;
      rd_opt->tx_select_threshes[frame_type][i] /= 2;
    }

    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      int single_count_zero = 0;
      int comp_count_zero = 0;

      for (i = 0; i < COMP_INTER_CONTEXTS; i++) {
        single_count_zero += cm->counts.comp_inter[i][0];
        comp_count_zero += cm->counts.comp_inter[i][1];
      }

      if (comp_count_zero == 0) {
        cm->reference_mode = SINGLE_REFERENCE;
        vp9_zero(cm->counts.comp_inter);
      } else if (single_count_zero == 0) {
        cm->reference_mode = COMPOUND_REFERENCE;
        vp9_zero(cm->counts.comp_inter);
      }
    }

    if (cm->tx_mode == TX_MODE_SELECT) {
      int count4x4 = 0;
      int count8x8_lp = 0, count8x8_8x8p = 0;
      int count16x16_16x16p = 0, count16x16_lp = 0;
      int count32x32 = 0;

      for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
        count4x4 += cm->counts.tx.p32x32[i][TX_4X4];
        count4x4 += cm->counts.tx.p16x16[i][TX_4X4];
        count4x4 += cm->counts.tx.p8x8[i][TX_4X4];

        count8x8_lp += cm->counts.tx.p32x32[i][TX_8X8];
        count8x8_lp += cm->counts.tx.p16x16[i][TX_8X8];
        count8x8_8x8p += cm->counts.tx.p8x8[i][TX_8X8];

        count16x16_16x16p += cm->counts.tx.p16x16[i][TX_16X16];
        count16x16_lp += cm->counts.tx.p32x32[i][TX_16X16];
        count32x32 += cm->counts.tx.p32x32[i][TX_32X32];
      }

      if (count4x4 == 0 && count16x16_lp == 0 && count16x16_16x16p == 0 &&
          count32x32 == 0) {
        cm->tx_mode = ALLOW_8X8;
        reset_skip_txfm_size(cm, TX_8X8);
      } else if (count8x8_8x8p == 0 && count16x16_16x16p == 0 &&
                 count8x8_lp == 0 && count16x16_lp == 0 && count32x32 == 0) {
        cm->tx_mode = ONLY_4X4;
        reset_skip_txfm_size(cm, TX_4X4);
      } else if (count8x8_lp == 0 && count16x16_lp == 0 && count4x4 == 0) {
        cm->tx_mode = ALLOW_32X32;
      } else if (count32x32 == 0 && count8x8_lp == 0 && count4x4 == 0) {
        cm->tx_mode = ALLOW_16X16;
        reset_skip_txfm_size(cm, TX_16X16);
      }
    }
  } else {
    cm->reference_mode = SINGLE_REFERENCE;
    cm->interp_filter = SWITCHABLE;
    encode_frame_internal(cpi);
  }
}

static void sum_intra_stats(FRAME_COUNTS *counts, const MODE_INFO *mi) {
  const MB_PREDICTION_MODE y_mode = mi->mbmi.mode;
  const MB_PREDICTION_MODE uv_mode = mi->mbmi.uv_mode;
  const BLOCK_SIZE bsize = mi->mbmi.sb_type;

  if (bsize < BLOCK_8X8) {
    int idx, idy;
    const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
    for (idy = 0; idy < 2; idy += num_4x4_h)
      for (idx = 0; idx < 2; idx += num_4x4_w)
        ++counts->y_mode[0][mi->bmi[idy * 2 + idx].as_mode];
  } else {
    ++counts->y_mode[size_group_lookup[bsize]][y_mode];
  }

  ++counts->uv_mode[y_mode][uv_mode];
}

// Experimental stub function to create a per MB zbin adjustment based on
// some previously calculated measure of MB activity.
static void adjust_act_zbin(VP9_COMP *cpi, MACROBLOCK *x) {
#if USE_ACT_INDEX
  x->act_zbin_adj = *(x->mb_activity_ptr);
#else
  // Apply the masking to the RD multiplier.
  const int64_t act = *(x->mb_activity_ptr);
  const int64_t a = act + 4 * cpi->activity_avg;
  const int64_t b = 4 * act + cpi->activity_avg;

  if (act > cpi->activity_avg)
    x->act_zbin_adj = (int) (((int64_t) b + (a >> 1)) / a) - 1;
  else
    x->act_zbin_adj = 1 - (int) (((int64_t) a + (b >> 1)) / b);
#endif
}

static int get_zbin_mode_boost(const MB_MODE_INFO *mbmi, int enabled) {
  if (enabled) {
    if (is_inter_block(mbmi)) {
      if (mbmi->mode == ZEROMV) {
        return mbmi->ref_frame[0] != LAST_FRAME ? GF_ZEROMV_ZBIN_BOOST
                                                : LF_ZEROMV_ZBIN_BOOST;
      } else {
        return mbmi->sb_type < BLOCK_8X8 ? SPLIT_MV_ZBIN_BOOST
                                         : MV_ZBIN_BOOST;
      }
    } else {
      return INTRA_ZBIN_BOOST;
    }
  } else {
    return 0;
  }
}

static void encode_superblock(VP9_COMP *cpi, TOKENEXTRA **t, int output_enabled,
                              int mi_row, int mi_col, BLOCK_SIZE bsize) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &cpi->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MODE_INFO **mi_8x8 = xd->mi;
  MODE_INFO *mi = mi_8x8[0];
  MB_MODE_INFO *mbmi = &mi->mbmi;
  PICK_MODE_CONTEXT *ctx = get_block_context(x, bsize);
  unsigned int segment_id = mbmi->segment_id;
  const int mis = cm->mi_stride;
  const int mi_width = num_8x8_blocks_wide_lookup[bsize];
  const int mi_height = num_8x8_blocks_high_lookup[bsize];

  x->skip_recode = !x->select_txfm_size && mbmi->sb_type >= BLOCK_8X8 &&
                   cpi->oxcf.aq_mode != COMPLEXITY_AQ &&
                   cpi->oxcf.aq_mode != CYCLIC_REFRESH_AQ &&
                   cpi->sf.allow_skip_recode;

  x->skip_optimize = ctx->is_coded;
  ctx->is_coded = 1;
  x->use_lp32x32fdct = cpi->sf.use_lp32x32fdct;
  x->skip_encode = (!output_enabled && cpi->sf.skip_encode_frame &&
                    x->q_index < QIDX_SKIP_THRESH);

  if (x->skip_encode)
    return;

  if (cm->frame_type == KEY_FRAME) {
    if (cpi->oxcf.tuning == VP8_TUNE_SSIM) {
      adjust_act_zbin(cpi, x);
      vp9_update_zbin_extra(cpi, x);
    }
  } else {
    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);

    if (cpi->oxcf.tuning == VP8_TUNE_SSIM) {
      // Adjust the zbin based on this MB rate.
      adjust_act_zbin(cpi, x);
    }

    // Experimental code. Special case for gf and arf zeromv modes.
    // Increase zbin size to suppress noise
    cpi->zbin_mode_boost = get_zbin_mode_boost(mbmi,
                                               cpi->zbin_mode_boost_enabled);
    vp9_update_zbin_extra(cpi, x);
  }

  if (!is_inter_block(mbmi)) {
    int plane;
    mbmi->skip = 1;
    for (plane = 0; plane < MAX_MB_PLANE; ++plane)
      vp9_encode_intra_block_plane(x, MAX(bsize, BLOCK_8X8), plane);
    if (output_enabled)
      sum_intra_stats(&cm->counts, mi);
    vp9_tokenize_sb(cpi, t, !output_enabled, MAX(bsize, BLOCK_8X8));
  } else {
    int ref;
    const int is_compound = has_second_ref(mbmi);
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi,
                                                     mbmi->ref_frame[ref]);
      vp9_setup_pre_planes(xd, ref, cfg, mi_row, mi_col,
                           &xd->block_refs[ref]->sf);
    }
    vp9_build_inter_predictors_sb(xd, mi_row, mi_col, MAX(bsize, BLOCK_8X8));

    if (!x->skip) {
      mbmi->skip = 1;
      vp9_encode_sb(x, MAX(bsize, BLOCK_8X8));
      vp9_tokenize_sb(cpi, t, !output_enabled, MAX(bsize, BLOCK_8X8));
    } else {
      mbmi->skip = 1;
      if (output_enabled)
        cm->counts.skip[vp9_get_skip_context(xd)][1]++;
      reset_skip_context(xd, MAX(bsize, BLOCK_8X8));
    }
  }

  if (output_enabled) {
    if (cm->tx_mode == TX_MODE_SELECT &&
        mbmi->sb_type >= BLOCK_8X8  &&
        !(is_inter_block(mbmi) &&
            (mbmi->skip ||
             vp9_segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)))) {
      ++get_tx_counts(max_txsize_lookup[bsize], vp9_get_tx_size_context(xd),
                      &cm->counts.tx)[mbmi->tx_size];
    } else {
      int x, y;
      TX_SIZE tx_size;
      // The new intra coding scheme requires no change of transform size
      if (is_inter_block(&mi->mbmi)) {
        tx_size = MIN(tx_mode_to_biggest_tx_size[cm->tx_mode],
                      max_txsize_lookup[bsize]);
      } else {
        tx_size = (bsize >= BLOCK_8X8) ? mbmi->tx_size : TX_4X4;
      }

      for (y = 0; y < mi_height; y++)
        for (x = 0; x < mi_width; x++)
          if (mi_col + x < cm->mi_cols && mi_row + y < cm->mi_rows)
            mi_8x8[mis * y + x]->mbmi.tx_size = tx_size;
    }
  }
}
