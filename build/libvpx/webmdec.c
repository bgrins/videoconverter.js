/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./webmdec.h"

#include <stdarg.h>

#include "third_party/nestegg/include/nestegg/nestegg.h"

static int nestegg_read_cb(void *buffer, size_t length, void *userdata) {
  FILE *f = userdata;

  if (fread(buffer, 1, length, f) < length) {
    if (ferror(f))
      return -1;
    if (feof(f))
      return 0;
  }
  return 1;
}

static int nestegg_seek_cb(int64_t offset, int whence, void *userdata) {
  switch (whence) {
    case NESTEGG_SEEK_SET:
      whence = SEEK_SET;
      break;
    case NESTEGG_SEEK_CUR:
      whence = SEEK_CUR;
      break;
    case NESTEGG_SEEK_END:
      whence = SEEK_END;
      break;
  };
  return fseek(userdata, (int32_t)offset, whence) ? -1 : 0;
}

static int64_t nestegg_tell_cb(void *userdata) {
  return ftell(userdata);
}

static void nestegg_log_cb(nestegg *context,
                           unsigned int severity,
                           char const *format, ...) {
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}

int file_is_webm(struct WebmInputContext *webm_ctx,
                 struct VpxInputContext *vpx_ctx) {
  uint32_t i, n;
  int track_type = -1;
  int codec_id;

  nestegg_io io = {nestegg_read_cb, nestegg_seek_cb, nestegg_tell_cb, 0};
  nestegg_video_params params;

  io.userdata = vpx_ctx->file;
  if (nestegg_init(&webm_ctx->nestegg_ctx, io, NULL, -1))
    goto fail;

  if (nestegg_track_count(webm_ctx->nestegg_ctx, &n))
    goto fail;

  for (i = 0; i < n; i++) {
    track_type = nestegg_track_type(webm_ctx->nestegg_ctx, i);

    if (track_type == NESTEGG_TRACK_VIDEO)
      break;
    else if (track_type < 0)
      goto fail;
  }

  codec_id = nestegg_track_codec_id(webm_ctx->nestegg_ctx, i);
  if (codec_id == NESTEGG_CODEC_VP8) {
    vpx_ctx->fourcc = VP8_FOURCC;
  } else if (codec_id == NESTEGG_CODEC_VP9) {
    vpx_ctx->fourcc = VP9_FOURCC;
  } else {
    fatal("Not VPx video, quitting.\n");
  }

  webm_ctx->video_track = i;

  if (nestegg_track_video_params(webm_ctx->nestegg_ctx, i, &params))
    goto fail;

  vpx_ctx->framerate.denominator = 0;
  vpx_ctx->framerate.numerator = 0;
  vpx_ctx->width = params.width;
  vpx_ctx->height = params.height;

  return 1;

 fail:
  webm_ctx->nestegg_ctx = NULL;
  rewind(vpx_ctx->file);

  return 0;
}

int webm_read_frame(struct WebmInputContext *webm_ctx,
                    uint8_t **buffer,
                    size_t *bytes_in_buffer,
                    size_t *buffer_size) {
  if (webm_ctx->chunk >= webm_ctx->chunks) {
    uint32_t track;

    do {
      /* End of this packet, get another. */
      if (webm_ctx->pkt) {
        nestegg_free_packet(webm_ctx->pkt);
        webm_ctx->pkt = NULL;
      }

      if (nestegg_read_packet(webm_ctx->nestegg_ctx, &webm_ctx->pkt) <= 0 ||
          nestegg_packet_track(webm_ctx->pkt, &track)) {
        return 1;
      }
    } while (track != webm_ctx->video_track);

    if (nestegg_packet_count(webm_ctx->pkt, &webm_ctx->chunks))
      return 1;

    webm_ctx->chunk = 0;
  }

  if (nestegg_packet_data(webm_ctx->pkt, webm_ctx->chunk,
                          buffer, bytes_in_buffer)) {
    return 1;
  }

  webm_ctx->chunk++;
  return 0;
}

int webm_guess_framerate(struct WebmInputContext *webm_ctx,
                         struct VpxInputContext *vpx_ctx) {
  uint32_t i;
  uint64_t tstamp = 0;

  /* Check to see if we can seek before we parse any data. */
  if (nestegg_track_seek(webm_ctx->nestegg_ctx, webm_ctx->video_track, 0)) {
    warn("Failed to guess framerate (no Cues), set to 30fps.\n");
    vpx_ctx->framerate.numerator = 30;
    vpx_ctx->framerate.denominator  = 1;
    return 0;
  }

  /* Guess the framerate. Read up to 1 second, or 50 video packets,
   * whichever comes first.
   */
  for (i = 0; tstamp < 1000000000 && i < 50;) {
    nestegg_packet *pkt;
    uint32_t track;

    if (nestegg_read_packet(webm_ctx->nestegg_ctx, &pkt) <= 0)
      break;

    nestegg_packet_track(pkt, &track);
    if (track == webm_ctx->video_track) {
      nestegg_packet_tstamp(pkt, &tstamp);
      ++i;
    }

    nestegg_free_packet(pkt);
  }

  if (nestegg_track_seek(webm_ctx->nestegg_ctx, webm_ctx->video_track, 0))
    goto fail;

  vpx_ctx->framerate.numerator = (i - 1) * 1000000;
  vpx_ctx->framerate.denominator = (int)(tstamp / 1000);
  return 0;

 fail:
  nestegg_destroy(webm_ctx->nestegg_ctx);
  webm_ctx->nestegg_ctx = NULL;
  rewind(vpx_ctx->file);
  return 1;
}

void webm_free(struct WebmInputContext *webm_ctx) {
  if (webm_ctx && webm_ctx->nestegg_ctx) {
    if (webm_ctx->pkt)
      nestegg_free_packet(webm_ctx->pkt);
    nestegg_destroy(webm_ctx->nestegg_ctx);
  }
}
