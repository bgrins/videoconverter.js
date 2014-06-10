/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./vpx_config.h"

#include "vp9/encoder/vp9_variance.h"
#include "vp9/common/vp9_pragmas.h"
#include "vpx_ports/mem.h"

typedef void (*get_var_avx2) (
  const unsigned char *src_ptr,
  int source_stride,
  const unsigned char *ref_ptr,
  int recon_stride,
  unsigned int *SSE,
  int *Sum
);

void vp9_get16x16var_avx2
(
  const unsigned char *src_ptr,
  int source_stride,
  const unsigned char *ref_ptr,
  int recon_stride,
  unsigned int *SSE,
  int *Sum
);

void vp9_get32x32var_avx2
(
  const unsigned char *src_ptr,
  int source_stride,
  const unsigned char *ref_ptr,
  int recon_stride,
  unsigned int *SSE,
  int *Sum
);

unsigned int vp9_sub_pixel_variance32xh_avx2
(
  const uint8_t *src,
  int src_stride,
  int x_offset,
  int y_offset,
  const uint8_t *dst,
  int dst_stride,
  int height,
  unsigned int *sse
);

unsigned int vp9_sub_pixel_avg_variance32xh_avx2
(
  const uint8_t *src,
  int src_stride,
  int x_offset,
  int y_offset,
  const uint8_t *dst,
  int dst_stride,
  const uint8_t *sec,
  int sec_stride,
  int height,
  unsigned int *sseptr
);

static void variance_avx2(const unsigned char *src_ptr, int  source_stride,
                        const unsigned char *ref_ptr, int  recon_stride,
                        int  w, int  h, unsigned int *sse, int *sum,
                        get_var_avx2 var_fn, int block_size) {
  unsigned int sse0;
  int sum0;
  int i, j;

  *sse = 0;
  *sum = 0;

  for (i = 0; i < h; i += 16) {
    for (j = 0; j < w; j += block_size) {
      // processing 16 rows horizontally each call
      var_fn(src_ptr + source_stride * i + j, source_stride,
             ref_ptr + recon_stride * i + j, recon_stride, &sse0, &sum0);
      *sse += sse0;
      *sum += sum0;
    }
  }
}

unsigned int vp9_variance16x16_avx2
(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int var;
  int avg;

  variance_avx2(src_ptr, source_stride, ref_ptr, recon_stride, 16, 16,
                &var, &avg, vp9_get16x16var_avx2, 16);
  *sse = var;
  return (var - (((unsigned int)avg * avg) >> 8));
}

unsigned int vp9_mse16x16_avx2(
  const unsigned char *src_ptr,
  int  source_stride,
  const unsigned char *ref_ptr,
  int  recon_stride,
  unsigned int *sse) {
  unsigned int sse0;
  int sum0;
  vp9_get16x16var_avx2(src_ptr, source_stride, ref_ptr, recon_stride, &sse0,
                       &sum0);
  *sse = sse0;
  return sse0;
}

unsigned int vp9_variance32x32_avx2(const uint8_t *src_ptr,
                                    int  source_stride,
                                    const uint8_t *ref_ptr,
                                    int  recon_stride,
                                    unsigned int *sse) {
  unsigned int var;
  int avg;

  // processing 32 elements vertically in parallel
  variance_avx2(src_ptr, source_stride, ref_ptr, recon_stride, 32, 32,
                &var, &avg, vp9_get32x32var_avx2, 32);
  *sse = var;
  return (var - (((int64_t)avg * avg) >> 10));
}

unsigned int vp9_variance32x16_avx2(const uint8_t *src_ptr,
                                    int  source_stride,
                                    const uint8_t *ref_ptr,
                                    int  recon_stride,
                                    unsigned int *sse) {
  unsigned int var;
  int avg;

  // processing 32 elements vertically in parallel
  variance_avx2(src_ptr, source_stride, ref_ptr, recon_stride, 32, 16,
                &var, &avg, vp9_get32x32var_avx2, 32);
  *sse = var;
  return (var - (((int64_t)avg * avg) >> 9));
}


unsigned int vp9_variance64x64_avx2(const uint8_t *src_ptr,
                                    int  source_stride,
                                    const uint8_t *ref_ptr,
                                    int  recon_stride,
                                    unsigned int *sse) {
  unsigned int var;
  int avg;

  // processing 32 elements vertically in parallel
  variance_avx2(src_ptr, source_stride, ref_ptr, recon_stride, 64, 64,
                &var, &avg, vp9_get32x32var_avx2, 32);
  *sse = var;
  return (var - (((int64_t)avg * avg) >> 12));
}

unsigned int vp9_variance64x32_avx2(const uint8_t *src_ptr,
                                    int  source_stride,
                                    const uint8_t *ref_ptr,
                                    int  recon_stride,
                                    unsigned int *sse) {
  unsigned int var;
  int avg;

  // processing 32 elements vertically in parallel
  variance_avx2(src_ptr, source_stride, ref_ptr, recon_stride, 64, 32,
                &var, &avg, vp9_get32x32var_avx2, 32);

  *sse = var;
  return (var - (((int64_t)avg * avg) >> 11));
}

unsigned int vp9_sub_pixel_variance64x64_avx2(const uint8_t *src,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse_ptr) {
  // processing 32 elements in parallel
  unsigned int sse;
  int se = vp9_sub_pixel_variance32xh_avx2(src, src_stride, x_offset,
                                           y_offset, dst, dst_stride,
                                           64, &sse);
  // processing the next 32 elements in parallel
  unsigned int sse2;
  int se2 = vp9_sub_pixel_variance32xh_avx2(src + 32, src_stride,
                                            x_offset, y_offset,
                                            dst + 32, dst_stride,
                                            64, &sse2);
  se += se2;
  sse += sse2;
  *sse_ptr = sse;
  return sse - (((int64_t)se * se) >> 12);
}

unsigned int vp9_sub_pixel_variance32x32_avx2(const uint8_t *src,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t *dst,
                                              int dst_stride,
                                              unsigned int *sse_ptr) {
  // processing 32 element in parallel
  unsigned int sse;
  int se = vp9_sub_pixel_variance32xh_avx2(src, src_stride, x_offset,
                                           y_offset, dst, dst_stride,
                                           32, &sse);
  *sse_ptr = sse;
  return sse - (((int64_t)se * se) >> 10);
}

unsigned int vp9_sub_pixel_avg_variance64x64_avx2(const uint8_t *src,
                                                  int src_stride,
                                                  int x_offset,
                                                  int y_offset,
                                                  const uint8_t *dst,
                                                  int dst_stride,
                                                  unsigned int *sseptr,
                                                  const uint8_t *sec) {
  // processing 32 elements in parallel
  unsigned int sse;

  int se = vp9_sub_pixel_avg_variance32xh_avx2(src, src_stride, x_offset,
                                               y_offset, dst, dst_stride,
                                               sec, 64, 64, &sse);
  unsigned int sse2;
  // processing the next 32 elements in parallel
  int se2 = vp9_sub_pixel_avg_variance32xh_avx2(src + 32, src_stride, x_offset,
                                                y_offset, dst + 32, dst_stride,
                                                sec + 32, 64, 64, &sse2);
  se += se2;
  sse += sse2;
  *sseptr = sse;

  return sse - (((int64_t)se * se) >> 12);
}

unsigned int vp9_sub_pixel_avg_variance32x32_avx2(const uint8_t *src,
                                                  int src_stride,
                                                  int x_offset,
                                                  int y_offset,
                                                  const uint8_t *dst,
                                                  int dst_stride,
                                                  unsigned int *sseptr,
                                                  const uint8_t *sec) {
  // processing 32 element in parallel
  unsigned int sse;
  int se = vp9_sub_pixel_avg_variance32xh_avx2(src, src_stride, x_offset,
                                                 y_offset, dst, dst_stride,
                                                 sec, 32, 32, &sse);
  *sseptr = sse;
  return sse - (((int64_t)se * se) >> 10);
}


