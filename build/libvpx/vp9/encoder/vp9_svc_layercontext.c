/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_svc_layercontext.h"

void vp9_init_layer_context(VP9_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  int layer;
  int layer_end;

  svc->spatial_layer_id = 0;
  svc->temporal_layer_id = 0;

  if (svc->number_temporal_layers > 1) {
    layer_end = svc->number_temporal_layers;
  } else {
    layer_end = svc->number_spatial_layers;
  }

  for (layer = 0; layer < layer_end; ++layer) {
    LAYER_CONTEXT *const lc = &svc->layer_context[layer];
    RATE_CONTROL *const lrc = &lc->rc;
    lc->current_video_frame_in_layer = 0;
    lrc->avg_frame_qindex[INTER_FRAME] = oxcf->worst_allowed_q;
    lrc->ni_av_qi = oxcf->worst_allowed_q;
    lrc->total_actual_bits = 0;
    lrc->total_target_vs_actual = 0;
    lrc->ni_tot_qi = 0;
    lrc->tot_q = 0.0;
    lrc->avg_q = 0.0;
    lrc->ni_frames = 0;
    lrc->decimation_count = 0;
    lrc->decimation_factor = 0;
    lrc->rate_correction_factor = 1.0;
    lrc->key_frame_rate_correction_factor = 1.0;

    if (svc->number_temporal_layers > 1) {
      lc->target_bandwidth = oxcf->ts_target_bitrate[layer] * 1000;
      lrc->last_q[INTER_FRAME] = oxcf->worst_allowed_q;
    } else {
      lc->target_bandwidth = oxcf->ss_target_bitrate[layer] * 1000;
      lrc->last_q[0] = oxcf->best_allowed_q;
      lrc->last_q[1] = oxcf->best_allowed_q;
      lrc->last_q[2] = oxcf->best_allowed_q;
    }

    lrc->buffer_level = vp9_rescale((int)(oxcf->starting_buffer_level),
                                    lc->target_bandwidth, 1000);
    lrc->bits_off_target = lrc->buffer_level;
  }
}

// Update the layer context from a change_config() call.
void vp9_update_layer_context_change_config(VP9_COMP *const cpi,
                                            const int target_bandwidth) {
  SVC *const svc = &cpi->svc;
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  const RATE_CONTROL *const rc = &cpi->rc;
  int layer;
  int layer_end;
  float bitrate_alloc = 1.0;

  if (svc->number_temporal_layers > 1) {
    layer_end = svc->number_temporal_layers;
  } else {
    layer_end = svc->number_spatial_layers;
  }

  for (layer = 0; layer < layer_end; ++layer) {
    LAYER_CONTEXT *const lc = &svc->layer_context[layer];
    RATE_CONTROL *const lrc = &lc->rc;

    if (svc->number_temporal_layers > 1) {
      lc->target_bandwidth = oxcf->ts_target_bitrate[layer] * 1000;
    } else {
      lc->target_bandwidth = oxcf->ss_target_bitrate[layer] * 1000;
    }
    bitrate_alloc = (float)lc->target_bandwidth / target_bandwidth;
    // Update buffer-related quantities.
    lc->starting_buffer_level =
        (int64_t)(oxcf->starting_buffer_level * bitrate_alloc);
    lc->optimal_buffer_level =
        (int64_t)(oxcf->optimal_buffer_level * bitrate_alloc);
    lc->maximum_buffer_size =
        (int64_t)(oxcf->maximum_buffer_size * bitrate_alloc);
    lrc->bits_off_target = MIN(lrc->bits_off_target, lc->maximum_buffer_size);
    lrc->buffer_level = MIN(lrc->buffer_level, lc->maximum_buffer_size);
    // Update framerate-related quantities.
    if (svc->number_temporal_layers > 1) {
      lc->framerate = oxcf->framerate / oxcf->ts_rate_decimator[layer];
    } else {
      lc->framerate = oxcf->framerate;
    }
    lrc->av_per_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
    lrc->max_frame_bandwidth = rc->max_frame_bandwidth;
    // Update qp-related quantities.
    lrc->worst_quality = rc->worst_quality;
    lrc->best_quality = rc->best_quality;
  }
}

static LAYER_CONTEXT *get_layer_context(SVC *svc) {
  return svc->number_temporal_layers > 1 ?
         &svc->layer_context[svc->temporal_layer_id] :
         &svc->layer_context[svc->spatial_layer_id];
}

void vp9_update_temporal_layer_framerate(VP9_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(svc);
  RATE_CONTROL *const lrc = &lc->rc;
  const int layer = svc->temporal_layer_id;

  lc->framerate = oxcf->framerate / oxcf->ts_rate_decimator[layer];
  lrc->av_per_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
  lrc->max_frame_bandwidth = cpi->rc.max_frame_bandwidth;
  // Update the average layer frame size (non-cumulative per-frame-bw).
  if (layer == 0) {
    lc->avg_frame_size = lrc->av_per_frame_bandwidth;
  } else {
    const double prev_layer_framerate =
        oxcf->framerate / oxcf->ts_rate_decimator[layer - 1];
    const int prev_layer_target_bandwidth =
        oxcf->ts_target_bitrate[layer - 1] * 1000;
    lc->avg_frame_size =
        (int)((lc->target_bandwidth - prev_layer_target_bandwidth) /
              (lc->framerate - prev_layer_framerate));
  }
}

void vp9_update_spatial_layer_framerate(VP9_COMP *const cpi, double framerate) {
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(&cpi->svc);
  RATE_CONTROL *const lrc = &lc->rc;

  lc->framerate = framerate;
  lrc->av_per_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
  lrc->min_frame_bandwidth = (int)(lrc->av_per_frame_bandwidth *
                                   oxcf->two_pass_vbrmin_section / 100);
  lrc->max_frame_bandwidth = (int)(((int64_t)lrc->av_per_frame_bandwidth *
                                   oxcf->two_pass_vbrmax_section) / 100);
  lrc->max_gf_interval = 16;

  lrc->static_scene_max_gf_interval = cpi->key_frame_frequency >> 1;

  if (oxcf->play_alternate && oxcf->lag_in_frames) {
    if (lrc->max_gf_interval > oxcf->lag_in_frames - 1)
      lrc->max_gf_interval = oxcf->lag_in_frames - 1;

    if (lrc->static_scene_max_gf_interval > oxcf->lag_in_frames - 1)
      lrc->static_scene_max_gf_interval = oxcf->lag_in_frames - 1;
  }

  if (lrc->max_gf_interval > lrc->static_scene_max_gf_interval)
    lrc->max_gf_interval = lrc->static_scene_max_gf_interval;
}

void vp9_restore_layer_context(VP9_COMP *const cpi) {
  LAYER_CONTEXT *const lc = get_layer_context(&cpi->svc);
  const int old_frame_since_key = cpi->rc.frames_since_key;
  const int old_frame_to_key = cpi->rc.frames_to_key;

  cpi->rc = lc->rc;
  cpi->twopass = lc->twopass;
  cpi->oxcf.target_bandwidth = lc->target_bandwidth;
  cpi->oxcf.starting_buffer_level = lc->starting_buffer_level;
  cpi->oxcf.optimal_buffer_level = lc->optimal_buffer_level;
  cpi->oxcf.maximum_buffer_size = lc->maximum_buffer_size;
  // Reset the frames_since_key and frames_to_key counters to their values
  // before the layer restore. Keep these defined for the stream (not layer).
  if (cpi->svc.number_temporal_layers > 1) {
    cpi->rc.frames_since_key = old_frame_since_key;
    cpi->rc.frames_to_key = old_frame_to_key;
  }
}

void vp9_save_layer_context(VP9_COMP *const cpi) {
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = get_layer_context(&cpi->svc);

  lc->rc = cpi->rc;
  lc->twopass = cpi->twopass;
  lc->target_bandwidth = (int)oxcf->target_bandwidth;
  lc->starting_buffer_level = oxcf->starting_buffer_level;
  lc->optimal_buffer_level = oxcf->optimal_buffer_level;
  lc->maximum_buffer_size = oxcf->maximum_buffer_size;
}

void vp9_init_second_pass_spatial_svc(VP9_COMP *cpi) {
  SVC *const svc = &cpi->svc;
  int i;

  for (i = 0; i < svc->number_spatial_layers; ++i) {
    struct twopass_rc *const twopass = &svc->layer_context[i].twopass;

    svc->spatial_layer_id = i;
    vp9_init_second_pass(cpi);

    twopass->total_stats.spatial_layer_id = i;
    twopass->total_left_stats.spatial_layer_id = i;
  }
  svc->spatial_layer_id = 0;
}

void vp9_inc_frame_in_layer(SVC *svc) {
  LAYER_CONTEXT *const lc = (svc->number_temporal_layers > 1)
      ? &svc->layer_context[svc->temporal_layer_id]
      : &svc->layer_context[svc->spatial_layer_id];
  ++lc->current_video_frame_in_layer;
}
