/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBMDEC_H_
#define WEBMDEC_H_

#include "./tools_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct nestegg;
struct nestegg_packet;
struct VpxInputContext;

struct WebmInputContext {
  uint32_t chunk;
  uint32_t chunks;
  uint32_t video_track;
  struct nestegg *nestegg_ctx;
  struct nestegg_packet *pkt;
};

int file_is_webm(struct WebmInputContext *webm_ctx,
                 struct VpxInputContext *vpx_ctx);

int webm_read_frame(struct WebmInputContext *webm_ctx,
                    uint8_t **buffer,
                    size_t *bytes_in_buffer,
                    size_t *buffer_size);

int webm_guess_framerate(struct WebmInputContext *webm_ctx,
                         struct VpxInputContext *vpx_ctx);

void webm_free(struct WebmInputContext *webm_ctx);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WEBMDEC_H_
