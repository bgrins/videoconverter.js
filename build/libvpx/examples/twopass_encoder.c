/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Two Pass Encoder
// ================
//
// This is an example of a two pass encoder loop. It takes an input file in
// YV12 format, passes it through the encoder twice, and writes the compressed
// frames to disk in IVF format. It builds upon the simple_encoder example.
//
// Twopass Variables
// -----------------
// Twopass mode needs to track the current pass number and the buffer of
// statistics packets.
//
// Updating The Configuration
// ---------------------------------
// In two pass mode, the configuration has to be updated on each pass. The
// statistics buffer is passed on the last pass.
//
// Encoding A Frame
// ----------------
// Encoding a frame in two pass mode is identical to the simple encoder
// example, except the deadline is set to VPX_DL_BEST_QUALITY to get the
// best quality possible. VPX_DL_GOOD_QUALITY could also be used.
//
//
// Processing Statistics Packets
// -----------------------------
// Each packet of type `VPX_CODEC_CX_FRAME_PKT` contains the encoded data
// for this frame. We write a IVF frame header, followed by the raw data.
//
//
// Pass Progress Reporting
// -----------------------------
// It's sometimes helpful to see when each pass completes.
//
//
// Clean-up
// -----------------------------
// Destruction of the encoder instance must be done on each pass. The
// raw image should be destroyed at the end as usual.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_encoder.h"

#include "./tools_common.h"
#include "./video_writer.h"

static const char *exec_name;

void usage_exit() {
  fprintf(stderr, "Usage: %s <codec> <width> <height> <infile> <outfile>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static void get_frame_stats(vpx_codec_ctx_t *ctx,
                            const vpx_image_t *img,
                            vpx_codec_pts_t pts,
                            unsigned int duration,
                            vpx_enc_frame_flags_t flags,
                            unsigned int deadline,
                            vpx_fixed_buf_t *stats) {
  vpx_codec_iter_t iter = NULL;
  const vpx_codec_cx_pkt_t *pkt = NULL;
  const vpx_codec_err_t res = vpx_codec_encode(ctx, img, pts, duration, flags,
                                               deadline);
  if (res != VPX_CODEC_OK)
    die_codec(ctx, "Failed to get frame stats.");

  while ((pkt = vpx_codec_get_cx_data(ctx, &iter)) != NULL) {
    if (pkt->kind == VPX_CODEC_STATS_PKT) {
      const uint8_t *const pkt_buf = pkt->data.twopass_stats.buf;
      const size_t pkt_size = pkt->data.twopass_stats.sz;
      stats->buf = realloc(stats->buf, stats->sz + pkt_size);
      memcpy((uint8_t *)stats->buf + stats->sz, pkt_buf, pkt_size);
      stats->sz += pkt_size;
    }
  }
}

static void encode_frame(vpx_codec_ctx_t *ctx,
                         const vpx_image_t *img,
                         vpx_codec_pts_t pts,
                         unsigned int duration,
                         vpx_enc_frame_flags_t flags,
                         unsigned int deadline,
                         VpxVideoWriter *writer) {
  vpx_codec_iter_t iter = NULL;
  const vpx_codec_cx_pkt_t *pkt = NULL;
  const vpx_codec_err_t res = vpx_codec_encode(ctx, img, pts, duration, flags,
                                               deadline);
  if (res != VPX_CODEC_OK)
    die_codec(ctx, "Failed to encode frame.");

  while ((pkt = vpx_codec_get_cx_data(ctx, &iter)) != NULL) {
    if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
      const int keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;

      if (!vpx_video_writer_write_frame(writer, pkt->data.frame.buf,
                                                pkt->data.frame.sz,
                                                pkt->data.frame.pts))
        die_codec(ctx, "Failed to write compressed frame.");
      printf(keyframe ? "K" : ".");
      fflush(stdout);
    }
  }
}

int main(int argc, char **argv) {
  FILE *infile = NULL;
  VpxVideoWriter *writer = NULL;
  vpx_codec_ctx_t codec;
  vpx_codec_enc_cfg_t cfg;
  vpx_image_t raw;
  vpx_codec_err_t res;
  vpx_fixed_buf_t stats = {0};
  VpxVideoInfo info = {0};
  const VpxInterface *encoder = NULL;
  int pass;
  const int fps = 30;        // TODO(dkovalev) add command line argument
  const int bitrate = 200;   // kbit/s TODO(dkovalev) add command line argument
  const char *const codec_arg = argv[1];
  const char *const width_arg = argv[2];
  const char *const height_arg = argv[3];
  const char *const infile_arg = argv[4];
  const char *const outfile_arg = argv[5];
  exec_name = argv[0];

  if (argc != 6)
    die("Invalid number of arguments.");

  encoder = get_vpx_encoder_by_name(codec_arg);
  if (!encoder)
    die("Unsupported codec.");

  info.codec_fourcc = encoder->fourcc;
  info.time_base.numerator = 1;
  info.time_base.denominator = fps;
  info.frame_width = strtol(width_arg, NULL, 0);
  info.frame_height = strtol(height_arg, NULL, 0);

  if (info.frame_width <= 0 ||
      info.frame_height <= 0 ||
      (info.frame_width % 2) != 0 ||
      (info.frame_height % 2) != 0) {
    die("Invalid frame size: %dx%d", info.frame_width, info.frame_height);
  }

  if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, info.frame_width,
                                             info.frame_height, 1)) {
    die("Failed to allocate image", info.frame_width, info.frame_height);
  }

  writer = vpx_video_writer_open(outfile_arg, kContainerIVF, &info);
  if (!writer)
    die("Failed to open %s for writing", outfile_arg);

  printf("Using %s\n", vpx_codec_iface_name(encoder->interface()));

  res = vpx_codec_enc_config_default(encoder->interface(), &cfg, 0);
  if (res)
    die_codec(&codec, "Failed to get default codec config.");

  cfg.g_w = info.frame_width;
  cfg.g_h = info.frame_height;
  cfg.g_timebase.num = info.time_base.numerator;
  cfg.g_timebase.den = info.time_base.denominator;
  cfg.rc_target_bitrate = bitrate;

  for (pass = 0; pass < 2; ++pass) {
    int frame_count = 0;

    if (pass == 0) {
      cfg.g_pass = VPX_RC_FIRST_PASS;
    } else {
      cfg.g_pass = VPX_RC_LAST_PASS;
      cfg.rc_twopass_stats_in = stats;
    }

    if (!(infile = fopen(infile_arg, "rb")))
      die("Failed to open %s for reading", infile_arg);

    if (vpx_codec_enc_init(&codec, encoder->interface(), &cfg, 0))
      die_codec(&codec, "Failed to initialize encoder");

    while (vpx_img_read(&raw, infile)) {
      ++frame_count;

      if (pass == 0) {
        get_frame_stats(&codec, &raw, frame_count, 1, 0, VPX_DL_BEST_QUALITY,
                        &stats);
      } else {
        encode_frame(&codec, &raw, frame_count, 1, 0, VPX_DL_BEST_QUALITY,
                     writer);
      }
    }

    if (pass == 0) {
      get_frame_stats(&codec, NULL, frame_count, 1, 0, VPX_DL_BEST_QUALITY,
                      &stats);
    } else {
      printf("\n");
    }

    fclose(infile);
    printf("Pass %d complete. Processed %d frames.\n", pass + 1, frame_count);
    if (vpx_codec_destroy(&codec))
      die_codec(&codec, "Failed to destroy codec.");
  }

  vpx_img_free(&raw);
  free(stats.buf);

  vpx_video_writer_close(writer);

  return EXIT_SUCCESS;
}
