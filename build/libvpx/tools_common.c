/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./tools_common.h"

#if CONFIG_VP8_ENCODER || CONFIG_VP9_ENCODER
#include "vpx/vp8cx.h"
#endif

#if CONFIG_VP8_DECODER || CONFIG_VP9_DECODER
#include "vpx/vp8dx.h"
#endif

#if defined(_WIN32) || defined(__OS2__)
#include <io.h>
#include <fcntl.h>

#ifdef __OS2__
#define _setmode    setmode
#define _fileno     fileno
#define _O_BINARY   O_BINARY
#endif
#endif

#define LOG_ERROR(label) do {\
  const char *l = label;\
  va_list ap;\
  va_start(ap, fmt);\
  if (l)\
    fprintf(stderr, "%s: ", l);\
  vfprintf(stderr, fmt, ap);\
  fprintf(stderr, "\n");\
  va_end(ap);\
} while (0)


FILE *set_binary_mode(FILE *stream) {
  (void)stream;
#if defined(_WIN32) || defined(__OS2__)
  _setmode(_fileno(stream), _O_BINARY);
#endif
  return stream;
}

void die(const char *fmt, ...) {
  LOG_ERROR(NULL);
  usage_exit();
}

void fatal(const char *fmt, ...) {
  LOG_ERROR("Fatal");
  exit(EXIT_FAILURE);
}

void warn(const char *fmt, ...) {
  LOG_ERROR("Warning");
}

void die_codec(vpx_codec_ctx_t *ctx, const char *s) {
  const char *detail = vpx_codec_error_detail(ctx);

  printf("%s: %s\n", s, vpx_codec_error(ctx));
  if (detail)
    printf("    %s\n", detail);
  exit(EXIT_FAILURE);
}

int read_yuv_frame(struct VpxInputContext *input_ctx, vpx_image_t *yuv_frame) {
  FILE *f = input_ctx->file;
  struct FileTypeDetectionBuffer *detect = &input_ctx->detect;
  int plane = 0;
  int shortread = 0;

  for (plane = 0; plane < 3; ++plane) {
    uint8_t *ptr;
    const int w = (plane ? (1 + yuv_frame->d_w) / 2 : yuv_frame->d_w);
    const int h = (plane ? (1 + yuv_frame->d_h) / 2 : yuv_frame->d_h);
    int r;

    /* Determine the correct plane based on the image format. The for-loop
     * always counts in Y,U,V order, but this may not match the order of
     * the data on disk.
     */
    switch (plane) {
      case 1:
        ptr = yuv_frame->planes[
            yuv_frame->fmt == VPX_IMG_FMT_YV12 ? VPX_PLANE_V : VPX_PLANE_U];
        break;
      case 2:
        ptr = yuv_frame->planes[
            yuv_frame->fmt == VPX_IMG_FMT_YV12 ? VPX_PLANE_U : VPX_PLANE_V];
        break;
      default:
        ptr = yuv_frame->planes[plane];
    }

    for (r = 0; r < h; ++r) {
      size_t needed = w;
      size_t buf_position = 0;
      const size_t left = detect->buf_read - detect->position;
      if (left > 0) {
        const size_t more = (left < needed) ? left : needed;
        memcpy(ptr, detect->buf + detect->position, more);
        buf_position = more;
        needed -= more;
        detect->position += more;
      }
      if (needed > 0) {
        shortread |= (fread(ptr + buf_position, 1, needed, f) < needed);
      }

      ptr += yuv_frame->stride[plane];
    }
  }

  return shortread;
}

static const VpxInterface vpx_encoders[] = {
#if CONFIG_VP8_ENCODER
  {"vp8", VP8_FOURCC, &vpx_codec_vp8_cx},
#endif

#if CONFIG_VP9_ENCODER
  {"vp9", VP9_FOURCC, &vpx_codec_vp9_cx},
#endif
};

int get_vpx_encoder_count() {
  return sizeof(vpx_encoders) / sizeof(vpx_encoders[0]);
}

const VpxInterface *get_vpx_encoder_by_index(int i) {
  return &vpx_encoders[i];
}

const VpxInterface *get_vpx_encoder_by_name(const char *name) {
  int i;

  for (i = 0; i < get_vpx_encoder_count(); ++i) {
    const VpxInterface *encoder = get_vpx_encoder_by_index(i);
    if (strcmp(encoder->name, name) == 0)
      return encoder;
  }

  return NULL;
}

static const VpxInterface vpx_decoders[] = {
#if CONFIG_VP8_DECODER
  {"vp8", VP8_FOURCC, &vpx_codec_vp8_dx},
#endif

#if CONFIG_VP9_DECODER
  {"vp9", VP9_FOURCC, &vpx_codec_vp9_dx},
#endif
};

int get_vpx_decoder_count() {
  return sizeof(vpx_decoders) / sizeof(vpx_decoders[0]);
}

const VpxInterface *get_vpx_decoder_by_index(int i) {
  return &vpx_decoders[i];
}

const VpxInterface *get_vpx_decoder_by_name(const char *name) {
  int i;

  for (i = 0; i < get_vpx_decoder_count(); ++i) {
     const VpxInterface *const decoder = get_vpx_decoder_by_index(i);
     if (strcmp(decoder->name, name) == 0)
       return decoder;
  }

  return NULL;
}

const VpxInterface *get_vpx_decoder_by_fourcc(uint32_t fourcc) {
  int i;

  for (i = 0; i < get_vpx_decoder_count(); ++i) {
    const VpxInterface *const decoder = get_vpx_decoder_by_index(i);
    if (decoder->fourcc == fourcc)
      return decoder;
  }

  return NULL;
}

// TODO(dkovalev): move this function to vpx_image.{c, h}, so it will be part
// of vpx_image_t support
int vpx_img_plane_width(const vpx_image_t *img, int plane) {
  if (plane > 0 && img->x_chroma_shift > 0)
    return (img->d_w + 1) >> img->x_chroma_shift;
  else
    return img->d_w;
}

int vpx_img_plane_height(const vpx_image_t *img, int plane) {
  if (plane > 0 &&  img->y_chroma_shift > 0)
    return (img->d_h + 1) >> img->y_chroma_shift;
  else
    return img->d_h;
}

void vpx_img_write(const vpx_image_t *img, FILE *file) {
  int plane;

  for (plane = 0; plane < 3; ++plane) {
    const unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = vpx_img_plane_width(img, plane);
    const int h = vpx_img_plane_height(img, plane);
    int y;

    for (y = 0; y < h; ++y) {
      fwrite(buf, 1, w, file);
      buf += stride;
    }
  }
}

int vpx_img_read(vpx_image_t *img, FILE *file) {
  int plane;

  for (plane = 0; plane < 3; ++plane) {
    unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = vpx_img_plane_width(img, plane);
    const int h = vpx_img_plane_height(img, plane);
    int y;

    for (y = 0; y < h; ++y) {
      if (fread(buf, 1, w, file) != w)
        return 0;
      buf += stride;
    }
  }

  return 1;
}

// TODO(dkovalev) change sse_to_psnr signature: double -> int64_t
double sse_to_psnr(double samples, double peak, double sse) {
  static const double kMaxPSNR = 100.0;

  if (sse > 0.0) {
    const double psnr = 10.0 * log10(samples * peak * peak / sse);
    return psnr > kMaxPSNR ? kMaxPSNR : psnr;
  } else {
    return kMaxPSNR;
  }
}
