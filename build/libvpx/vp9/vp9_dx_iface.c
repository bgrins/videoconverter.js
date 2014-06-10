/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>

#include "./vpx_version.h"

#include "vpx/internal/vpx_codec_internal.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

#include "vp9/common/vp9_frame_buffers.h"

#include "vp9/decoder/vp9_decoder.h"
#include "vp9/decoder/vp9_read_bit_buffer.h"

#include "vp9/vp9_iface_common.h"

#define VP9_CAP_POSTPROC (CONFIG_VP9_POSTPROC ? VPX_CODEC_CAP_POSTPROC : 0)

typedef vpx_codec_stream_info_t vp9_stream_info_t;

struct vpx_codec_alg_priv {
  vpx_codec_priv_t        base;
  vpx_codec_dec_cfg_t     cfg;
  vp9_stream_info_t       si;
  int                     decoder_init;
  struct VP9Decoder *pbi;
  int                     postproc_cfg_set;
  vp8_postproc_cfg_t      postproc_cfg;
#if CONFIG_POSTPROC_VISUALIZER
  unsigned int            dbg_postproc_flag;
  int                     dbg_color_ref_frame_flag;
  int                     dbg_color_mb_modes_flag;
  int                     dbg_color_b_modes_flag;
  int                     dbg_display_mv_flag;
#endif
  vpx_image_t             img;
  int                     img_setup;
  int                     img_avail;
  int                     invert_tile_order;

  // External frame buffer info to save for VP9 common.
  void *ext_priv;  // Private data associated with the external frame buffers.
  vpx_get_frame_buffer_cb_fn_t get_ext_fb_cb;
  vpx_release_frame_buffer_cb_fn_t release_ext_fb_cb;
};

static vpx_codec_err_t decoder_init(vpx_codec_ctx_t *ctx,
                            vpx_codec_priv_enc_mr_cfg_t *data) {
  // This function only allocates space for the vpx_codec_alg_priv_t
  // structure. More memory may be required at the time the stream
  // information becomes known.
  if (!ctx->priv) {
    vpx_codec_alg_priv_t *alg_priv = vpx_memalign(32, sizeof(*alg_priv));
    if (alg_priv == NULL)
      return VPX_CODEC_MEM_ERROR;

    vp9_zero(*alg_priv);

    ctx->priv = (vpx_codec_priv_t *)alg_priv;
    ctx->priv->sz = sizeof(*ctx->priv);
    ctx->priv->iface = ctx->iface;
    ctx->priv->alg_priv = alg_priv;
    ctx->priv->alg_priv->si.sz = sizeof(ctx->priv->alg_priv->si);
    ctx->priv->init_flags = ctx->init_flags;

    if (ctx->config.dec) {
      // Update the reference to the config structure to an internal copy.
      ctx->priv->alg_priv->cfg = *ctx->config.dec;
      ctx->config.dec = &ctx->priv->alg_priv->cfg;
    }
  }

  return VPX_CODEC_OK;
}

static vpx_codec_err_t decoder_destroy(vpx_codec_alg_priv_t *ctx) {
  if (ctx->pbi) {
    vp9_decoder_remove(ctx->pbi);
    ctx->pbi = NULL;
  }

  vpx_free(ctx);

  return VPX_CODEC_OK;
}

static vpx_codec_err_t decoder_peek_si(const uint8_t *data,
                                       unsigned int data_sz,
                                       vpx_codec_stream_info_t *si) {
  if (data_sz <= 8)
    return VPX_CODEC_UNSUP_BITSTREAM;

  if (data + data_sz <= data)
    return VPX_CODEC_INVALID_PARAM;

  si->is_kf = 0;
  si->w = si->h = 0;

  {
    struct vp9_read_bit_buffer rb = { data, data + data_sz, 0, NULL, NULL };
    const int frame_marker = vp9_rb_read_literal(&rb, 2);
    const int version = vp9_rb_read_bit(&rb);
    (void) vp9_rb_read_bit(&rb);  // unused version bit

    if (frame_marker != VP9_FRAME_MARKER)
      return VPX_CODEC_UNSUP_BITSTREAM;
    if (version > 1) return VPX_CODEC_UNSUP_BITSTREAM;

    if (vp9_rb_read_bit(&rb)) {  // show an existing frame
      return VPX_CODEC_OK;
    }

    si->is_kf = !vp9_rb_read_bit(&rb);
    if (si->is_kf) {
      const int sRGB = 7;
      int colorspace;

      rb.bit_offset += 1;  // show frame
      rb.bit_offset += 1;  // error resilient

      if (vp9_rb_read_literal(&rb, 8) != VP9_SYNC_CODE_0 ||
          vp9_rb_read_literal(&rb, 8) != VP9_SYNC_CODE_1 ||
          vp9_rb_read_literal(&rb, 8) != VP9_SYNC_CODE_2) {
        return VPX_CODEC_UNSUP_BITSTREAM;
      }

      colorspace = vp9_rb_read_literal(&rb, 3);
      if (colorspace != sRGB) {
        rb.bit_offset += 1;  // [16,235] (including xvycc) vs [0,255] range
        if (version == 1) {
          rb.bit_offset += 2;  // subsampling x/y
          rb.bit_offset += 1;  // has extra plane
        }
      } else {
        if (version == 1) {
          rb.bit_offset += 1;  // has extra plane
        } else {
          // RGB is only available in version 1
          return VPX_CODEC_UNSUP_BITSTREAM;
        }
      }

      // TODO(jzern): these are available on non-keyframes in intra only mode.
      si->w = vp9_rb_read_literal(&rb, 16) + 1;
      si->h = vp9_rb_read_literal(&rb, 16) + 1;
    }
  }

  return VPX_CODEC_OK;
}

static vpx_codec_err_t decoder_get_si(vpx_codec_alg_priv_t *ctx,
                                      vpx_codec_stream_info_t *si) {
  const size_t sz = (si->sz >= sizeof(vp9_stream_info_t))
                       ? sizeof(vp9_stream_info_t)
                       : sizeof(vpx_codec_stream_info_t);
  memcpy(si, &ctx->si, sz);
  si->sz = (unsigned int)sz;

  return VPX_CODEC_OK;
}

static vpx_codec_err_t update_error_state(vpx_codec_alg_priv_t *ctx,
                           const struct vpx_internal_error_info *error) {
  if (error->error_code)
    ctx->base.err_detail = error->has_detail ? error->detail : NULL;

  return error->error_code;
}

static void init_buffer_callbacks(vpx_codec_alg_priv_t *ctx) {
  VP9_COMMON *const cm = &ctx->pbi->common;

  cm->new_fb_idx = -1;

  if (ctx->get_ext_fb_cb != NULL && ctx->release_ext_fb_cb != NULL) {
    cm->get_fb_cb = ctx->get_ext_fb_cb;
    cm->release_fb_cb = ctx->release_ext_fb_cb;
    cm->cb_priv = ctx->ext_priv;
  } else {
    cm->get_fb_cb = vp9_get_frame_buffer;
    cm->release_fb_cb = vp9_release_frame_buffer;

    if (vp9_alloc_internal_frame_buffers(&cm->int_frame_buffers))
      vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                         "Failed to initialize internal frame buffers");

    cm->cb_priv = &cm->int_frame_buffers;
  }
}

static void set_default_ppflags(vp8_postproc_cfg_t *cfg) {
  cfg->post_proc_flag = VP8_DEBLOCK | VP8_DEMACROBLOCK;
  cfg->deblocking_level = 4;
  cfg->noise_level = 0;
}

static void set_ppflags(const vpx_codec_alg_priv_t *ctx,
                        vp9_ppflags_t *flags) {
  flags->post_proc_flag =
#if CONFIG_POSTPROC_VISUALIZER
      (ctx->dbg_color_ref_frame_flag ? VP9D_DEBUG_CLR_FRM_REF_BLKS : 0) |
      (ctx->dbg_color_mb_modes_flag ? VP9D_DEBUG_CLR_BLK_MODES : 0) |
      (ctx->dbg_color_b_modes_flag ? VP9D_DEBUG_CLR_BLK_MODES : 0) |
      (ctx->dbg_display_mv_flag ? VP9D_DEBUG_DRAW_MV : 0) |
#endif
      ctx->postproc_cfg.post_proc_flag;

  flags->deblocking_level = ctx->postproc_cfg.deblocking_level;
  flags->noise_level = ctx->postproc_cfg.noise_level;
#if CONFIG_POSTPROC_VISUALIZER
  flags->display_ref_frame_flag = ctx->dbg_color_ref_frame_flag;
  flags->display_mb_modes_flag = ctx->dbg_color_mb_modes_flag;
  flags->display_b_modes_flag = ctx->dbg_color_b_modes_flag;
  flags->display_mv_flag = ctx->dbg_display_mv_flag;
#endif
}

static void init_decoder(vpx_codec_alg_priv_t *ctx) {
  VP9D_CONFIG oxcf;
  oxcf.width = ctx->si.w;
  oxcf.height = ctx->si.h;
  oxcf.version = 9;
  oxcf.max_threads = ctx->cfg.threads;
  oxcf.inv_tile_order = ctx->invert_tile_order;

  ctx->pbi = vp9_decoder_create(&oxcf);
  if (ctx->pbi == NULL)
    return;

  vp9_initialize_dec();

  // If postprocessing was enabled by the application and a
  // configuration has not been provided, default it.
  if (!ctx->postproc_cfg_set &&
      (ctx->base.init_flags & VPX_CODEC_USE_POSTPROC))
    set_default_ppflags(&ctx->postproc_cfg);

  init_buffer_callbacks(ctx);
}

static vpx_codec_err_t decode_one(vpx_codec_alg_priv_t *ctx,
                                  const uint8_t **data, unsigned int data_sz,
                                  void *user_priv, int64_t deadline) {
  YV12_BUFFER_CONFIG sd = { 0 };
  int64_t time_stamp = 0, time_end_stamp = 0;
  vp9_ppflags_t flags = {0};
  VP9_COMMON *cm = NULL;

  ctx->img_avail = 0;

  // Determine the stream parameters. Note that we rely on peek_si to
  // validate that we have a buffer that does not wrap around the top
  // of the heap.
  if (!ctx->si.h) {
    const vpx_codec_err_t res =
        ctx->base.iface->dec.peek_si(*data, data_sz, &ctx->si);
    if (res != VPX_CODEC_OK)
      return res;
  }

  // Initialize the decoder instance on the first frame
  if (!ctx->decoder_init) {
    init_decoder(ctx);
    if (ctx->pbi == NULL)
      return VPX_CODEC_ERROR;

    ctx->decoder_init = 1;
  }

  cm = &ctx->pbi->common;

  if (vp9_receive_compressed_data(ctx->pbi, data_sz, data, deadline))
    return update_error_state(ctx, &cm->error);

  if (ctx->base.init_flags & VPX_CODEC_USE_POSTPROC)
    set_ppflags(ctx, &flags);

  if (vp9_get_raw_frame(ctx->pbi, &sd, &time_stamp, &time_end_stamp, &flags))
    return update_error_state(ctx, &cm->error);

  yuvconfig2image(&ctx->img, &sd, user_priv);
  ctx->img.fb_priv = cm->frame_bufs[cm->new_fb_idx].raw_frame_buffer.priv;
  ctx->img_avail = 1;

  return VPX_CODEC_OK;
}

static void parse_superframe_index(const uint8_t *data, size_t data_sz,
                                   uint32_t sizes[8], int *count) {
  uint8_t marker;

  assert(data_sz);
  marker = data[data_sz - 1];
  *count = 0;

  if ((marker & 0xe0) == 0xc0) {
    const uint32_t frames = (marker & 0x7) + 1;
    const uint32_t mag = ((marker >> 3) & 0x3) + 1;
    const size_t index_sz = 2 + mag * frames;

    if (data_sz >= index_sz && data[data_sz - index_sz] == marker) {
      // found a valid superframe index
      uint32_t i, j;
      const uint8_t *x = &data[data_sz - index_sz + 1];

      for (i = 0; i < frames; i++) {
        uint32_t this_sz = 0;

        for (j = 0; j < mag; j++)
          this_sz |= (*x++) << (j * 8);
        sizes[i] = this_sz;
      }

      *count = frames;
    }
  }
}

static vpx_codec_err_t decoder_decode(vpx_codec_alg_priv_t *ctx,
                                      const uint8_t *data, unsigned int data_sz,
                                      void *user_priv, long deadline) {
  const uint8_t *data_start = data;
  const uint8_t *data_end = data + data_sz;
  vpx_codec_err_t res = VPX_CODEC_OK;
  uint32_t sizes[8];
  int frames_this_pts, frame_count = 0;

  if (data == NULL || data_sz == 0)
    return VPX_CODEC_INVALID_PARAM;

  parse_superframe_index(data, data_sz, sizes, &frames_this_pts);

  do {
    // Skip over the superframe index, if present
    if (data_sz && (*data_start & 0xe0) == 0xc0) {
      const uint8_t marker = *data_start;
      const uint32_t frames = (marker & 0x7) + 1;
      const uint32_t mag = ((marker >> 3) & 0x3) + 1;
      const uint32_t index_sz = 2 + mag * frames;

      if (data_sz >= index_sz && data_start[index_sz - 1] == marker) {
        data_start += index_sz;
        data_sz -= index_sz;
        if (data_start < data_end)
          continue;
        else
          break;
      }
    }

    // Use the correct size for this frame, if an index is present.
    if (frames_this_pts) {
      uint32_t this_sz = sizes[frame_count];

      if (data_sz < this_sz) {
        ctx->base.err_detail = "Invalid frame size in index";
        return VPX_CODEC_CORRUPT_FRAME;
      }

      data_sz = this_sz;
      frame_count++;
    }

    res = decode_one(ctx, &data_start, data_sz, user_priv, deadline);
    assert(data_start >= data);
    assert(data_start <= data_end);

    // Early exit if there was a decode error
    if (res)
      break;

    // Account for suboptimal termination by the encoder.
    while (data_start < data_end && *data_start == 0)
      data_start++;

    data_sz = (unsigned int)(data_end - data_start);
  } while (data_start < data_end);

  return res;
}

static vpx_image_t *decoder_get_frame(vpx_codec_alg_priv_t *ctx,
                                      vpx_codec_iter_t *iter) {
  vpx_image_t *img = NULL;

  if (ctx->img_avail) {
    // iter acts as a flip flop, so an image is only returned on the first
    // call to get_frame.
    if (!(*iter)) {
      img = &ctx->img;
      *iter = img;
    }
  }
  ctx->img_avail = 0;

  return img;
}

static vpx_codec_err_t decoder_set_fb_fn(
    vpx_codec_alg_priv_t *ctx,
    vpx_get_frame_buffer_cb_fn_t cb_get,
    vpx_release_frame_buffer_cb_fn_t cb_release, void *cb_priv) {
  if (cb_get == NULL || cb_release == NULL) {
    return VPX_CODEC_INVALID_PARAM;
  } else if (ctx->pbi == NULL) {
    // If the decoder has already been initialized, do not accept changes to
    // the frame buffer functions.
    ctx->get_ext_fb_cb = cb_get;
    ctx->release_ext_fb_cb = cb_release;
    ctx->ext_priv = cb_priv;
    return VPX_CODEC_OK;
  }

  return VPX_CODEC_ERROR;
}

static vpx_codec_err_t ctrl_set_reference(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id, va_list args) {
  vpx_ref_frame_t *const data = va_arg(args, vpx_ref_frame_t *);

  if (data) {
    vpx_ref_frame_t *const frame = (vpx_ref_frame_t *)data;
    YV12_BUFFER_CONFIG sd;

    image2yuvconfig(&frame->img, &sd);
    return vp9_set_reference_dec(&ctx->pbi->common,
                                 (VP9_REFFRAME)frame->frame_type, &sd);
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t ctrl_copy_reference(vpx_codec_alg_priv_t *ctx,
                                           int ctr_id, va_list args) {
  vpx_ref_frame_t *data = va_arg(args, vpx_ref_frame_t *);

  if (data) {
    vpx_ref_frame_t *frame = (vpx_ref_frame_t *)data;
    YV12_BUFFER_CONFIG sd;

    image2yuvconfig(&frame->img, &sd);

    return vp9_copy_reference_dec(ctx->pbi,
                                  (VP9_REFFRAME)frame->frame_type, &sd);
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t ctrl_get_reference(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id, va_list args) {
  vp9_ref_frame_t *data = va_arg(args, vp9_ref_frame_t *);

  if (data) {
    YV12_BUFFER_CONFIG* fb;

    vp9_get_reference_dec(ctx->pbi, data->idx, &fb);
    yuvconfig2image(&data->img, fb, NULL);
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t ctrl_set_postproc(vpx_codec_alg_priv_t *ctx,
                                         int ctr_id, va_list args) {
#if CONFIG_VP9_POSTPROC
  vp8_postproc_cfg_t *data = va_arg(args, vp8_postproc_cfg_t *);

  if (data) {
    ctx->postproc_cfg_set = 1;
    ctx->postproc_cfg = *((vp8_postproc_cfg_t *)data);
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
#else
  return VPX_CODEC_INCAPABLE;
#endif
}

static vpx_codec_err_t ctrl_set_dbg_options(vpx_codec_alg_priv_t *ctx,
                                            int ctrl_id, va_list args) {
#if CONFIG_POSTPROC_VISUALIZER && CONFIG_POSTPROC
  int data = va_arg(args, int);

#define MAP(id, var) case id: var = data; break;

  switch (ctrl_id) {
      MAP(VP8_SET_DBG_COLOR_REF_FRAME,   ctx->dbg_color_ref_frame_flag);
      MAP(VP8_SET_DBG_COLOR_MB_MODES,    ctx->dbg_color_mb_modes_flag);
      MAP(VP8_SET_DBG_COLOR_B_MODES,     ctx->dbg_color_b_modes_flag);
      MAP(VP8_SET_DBG_DISPLAY_MV,        ctx->dbg_display_mv_flag);
  }

  return VPX_CODEC_OK;
#else
  return VPX_CODEC_INCAPABLE;
#endif
}

static vpx_codec_err_t ctrl_get_last_ref_updates(vpx_codec_alg_priv_t *ctx,
                                                 int ctrl_id, va_list args) {
  int *const update_info = va_arg(args, int *);

  if (update_info) {
    if (ctx->pbi)
      *update_info = ctx->pbi->refresh_frame_flags;
    else
      return VPX_CODEC_ERROR;
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}


static vpx_codec_err_t ctrl_get_frame_corrupted(vpx_codec_alg_priv_t *ctx,
                                                int ctrl_id, va_list args) {
  int *corrupted = va_arg(args, int *);

  if (corrupted) {
    if (ctx->pbi)
      *corrupted = ctx->pbi->common.frame_to_show->corrupted;
    else
      return VPX_CODEC_ERROR;
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t ctrl_get_display_size(vpx_codec_alg_priv_t *ctx,
                                             int ctrl_id, va_list args) {
  int *const display_size = va_arg(args, int *);

  if (display_size) {
    if (ctx->pbi) {
      const VP9_COMMON *const cm = &ctx->pbi->common;
      display_size[0] = cm->display_width;
      display_size[1] = cm->display_height;
    } else {
      return VPX_CODEC_ERROR;
    }
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t ctrl_set_invert_tile_order(vpx_codec_alg_priv_t *ctx,
                                                  int ctr_id, va_list args) {
  ctx->invert_tile_order = va_arg(args, int);
  return VPX_CODEC_OK;
}

static vpx_codec_ctrl_fn_map_t decoder_ctrl_maps[] = {
  {VP8_COPY_REFERENCE,            ctrl_copy_reference},

  // Setters
  {VP8_SET_REFERENCE,             ctrl_set_reference},
  {VP8_SET_POSTPROC,              ctrl_set_postproc},
  {VP8_SET_DBG_COLOR_REF_FRAME,   ctrl_set_dbg_options},
  {VP8_SET_DBG_COLOR_MB_MODES,    ctrl_set_dbg_options},
  {VP8_SET_DBG_COLOR_B_MODES,     ctrl_set_dbg_options},
  {VP8_SET_DBG_DISPLAY_MV,        ctrl_set_dbg_options},
  {VP9_INVERT_TILE_DECODE_ORDER,  ctrl_set_invert_tile_order},

  // Getters
  {VP8D_GET_LAST_REF_UPDATES,     ctrl_get_last_ref_updates},
  {VP8D_GET_FRAME_CORRUPTED,      ctrl_get_frame_corrupted},
  {VP9_GET_REFERENCE,             ctrl_get_reference},
  {VP9D_GET_DISPLAY_SIZE,         ctrl_get_display_size},

  { -1, NULL},
};

#ifndef VERSION_STRING
#define VERSION_STRING
#endif
CODEC_INTERFACE(vpx_codec_vp9_dx) = {
  "WebM Project VP9 Decoder" VERSION_STRING,
  VPX_CODEC_INTERNAL_ABI_VERSION,
  VPX_CODEC_CAP_DECODER | VP9_CAP_POSTPROC |
      VPX_CODEC_CAP_EXTERNAL_FRAME_BUFFER,  // vpx_codec_caps_t
  decoder_init,       // vpx_codec_init_fn_t
  decoder_destroy,    // vpx_codec_destroy_fn_t
  decoder_ctrl_maps,  // vpx_codec_ctrl_fn_map_t
  NOT_IMPLEMENTED,    // vpx_codec_get_mmap_fn_t
  NOT_IMPLEMENTED,    // vpx_codec_set_mmap_fn_t
  { // NOLINT
    decoder_peek_si,    // vpx_codec_peek_si_fn_t
    decoder_get_si,     // vpx_codec_get_si_fn_t
    decoder_decode,     // vpx_codec_decode_fn_t
    decoder_get_frame,  // vpx_codec_frame_get_fn_t
    decoder_set_fb_fn,  // vpx_codec_set_fb_fn_t
  },
  { // NOLINT
    NOT_IMPLEMENTED,
    NOT_IMPLEMENTED,
    NOT_IMPLEMENTED,
    NOT_IMPLEMENTED,
    NOT_IMPLEMENTED,
    NOT_IMPLEMENTED
  }
};
