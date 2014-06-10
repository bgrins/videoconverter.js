/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <math.h>

#include "vp9/common/vp9_seg_common.h"

#include "vp9/encoder/vp9_segmentation.h"

static const double in_frame_q_adj_ratio[MAX_SEGMENTS] =
  {1.0, 2.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

void vp9_setup_in_frame_q_adj(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  struct segmentation *const seg = &cm->seg;

  // Make SURE use of floating point in this function is safe.
  vp9_clear_system_state();

  if (cm->frame_type == KEY_FRAME ||
      cpi->refresh_alt_ref_frame ||
      (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref)) {
    int segment;

    // Clear down the segment map.
    vpx_memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);

    // Clear down the complexity map used for rd.
    vpx_memset(cpi->complexity_map, 0, cm->mi_rows * cm->mi_cols);

    vp9_enable_segmentation(seg);
    vp9_clearall_segfeatures(seg);

    // Select delta coding method.
    seg->abs_delta = SEGMENT_DELTADATA;

    // Segment 0 "Q" feature is disabled so it defaults to the baseline Q.
    vp9_disable_segfeature(seg, 0, SEG_LVL_ALT_Q);

    // Use some of the segments for in frame Q adjustment.
    for (segment = 1; segment < 2; segment++) {
      const int qindex_delta =
          vp9_compute_qdelta_by_rate(&cpi->rc, cm->frame_type, cm->base_qindex,
                                     in_frame_q_adj_ratio[segment]);
      vp9_enable_segfeature(seg, segment, SEG_LVL_ALT_Q);
      vp9_set_segdata(seg, segment, SEG_LVL_ALT_Q, qindex_delta);
    }
  }
}

// Select a segment for the current SB64
void vp9_select_in_frame_q_segment(VP9_COMP *cpi,
                                      int mi_row, int mi_col,
                                      int output_enabled, int projected_rate) {
  VP9_COMMON *const cm = &cpi->common;

  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = num_8x8_blocks_wide_lookup[BLOCK_64X64];
  const int bh = num_8x8_blocks_high_lookup[BLOCK_64X64];
  const int xmis = MIN(cm->mi_cols - mi_col, bw);
  const int ymis = MIN(cm->mi_rows - mi_row, bh);
  int complexity_metric = 64;
  int x, y;

  unsigned char segment;

  if (!output_enabled) {
    segment = 0;
  } else {
    // Rate depends on fraction of a SB64 in frame (xmis * ymis / bw * bh).
    // It is converted to bits * 256 units.
    const int target_rate = (cpi->rc.sb64_target_rate * xmis * ymis * 256) /
                            (bw * bh);

    if (projected_rate < (target_rate / 4)) {
      segment = 1;
    } else {
      segment = 0;
    }

    if (target_rate > 0) {
      complexity_metric =
        clamp((int)((projected_rate * 64) / target_rate), 16, 255);
    }
  }

  // Fill in the entires in the segment map corresponding to this SB64.
  for (y = 0; y < ymis; y++) {
    for (x = 0; x < xmis; x++) {
      cpi->segmentation_map[mi_offset + y * cm->mi_cols + x] = segment;
      cpi->complexity_map[mi_offset + y * cm->mi_cols + x] =
        (unsigned char)complexity_metric;
    }
  }
}
