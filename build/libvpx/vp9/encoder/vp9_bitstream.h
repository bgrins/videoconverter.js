/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_BITSTREAM_H_
#define VP9_ENCODER_VP9_BITSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

struct VP9_COMP;

void vp9_entropy_mode_init();

void vp9_pack_bitstream(struct VP9_COMP *cpi, uint8_t *dest, size_t *size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_BITSTREAM_H_
