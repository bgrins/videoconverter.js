/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_DECODER_VP9_DECODER_H_
#define VP9_DECODER_VP9_DECODER_H_

#include "./vpx_config.h"

#include "vpx/vpx_codec.h"
#include "vpx_scale/yv12config.h"

#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/common/vp9_ppflags.h"

#include "vp9/decoder/vp9_decoder.h"
#include "vp9/decoder/vp9_dthread.h"
#include "vp9/decoder/vp9_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int width;
  int height;
  int version;
  int max_threads;
  int inv_tile_order;
} VP9D_CONFIG;

typedef struct VP9Decoder {
  DECLARE_ALIGNED(16, MACROBLOCKD, mb);

  DECLARE_ALIGNED(16, VP9_COMMON, common);

  VP9D_CONFIG oxcf;

  int64_t last_time_stamp;
  int ready_for_new_data;

  int refresh_frame_flags;

  int decoded_key_frame;

  int do_loopfilter_inline;  // apply loopfilter to available rows immediately
  VP9Worker lf_worker;

  VP9Worker *tile_workers;
  int num_tile_workers;

  VP9LfSync lf_row_sync;
} VP9Decoder;

void vp9_initialize_dec();

int vp9_receive_compressed_data(struct VP9Decoder *pbi,
                                size_t size, const uint8_t **dest,
                                int64_t time_stamp);

int vp9_get_raw_frame(struct VP9Decoder *pbi,
                      YV12_BUFFER_CONFIG *sd,
                      int64_t *time_stamp, int64_t *time_end_stamp,
                      vp9_ppflags_t *flags);

vpx_codec_err_t vp9_copy_reference_dec(struct VP9Decoder *pbi,
                                       VP9_REFFRAME ref_frame_flag,
                                       YV12_BUFFER_CONFIG *sd);

vpx_codec_err_t vp9_set_reference_dec(VP9_COMMON *cm,
                                      VP9_REFFRAME ref_frame_flag,
                                      YV12_BUFFER_CONFIG *sd);

int vp9_get_reference_dec(struct VP9Decoder *pbi,
                          int index, YV12_BUFFER_CONFIG **fb);


struct VP9Decoder *vp9_decoder_create(const VP9D_CONFIG *oxcf);

void vp9_decoder_remove(struct VP9Decoder *pbi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_DECODER_VP9_DECODER_H_
