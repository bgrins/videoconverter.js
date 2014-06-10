/*
 * Copyright © 2010 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "nestegg/nestegg.h"

#undef DEBUG
#define SEEK_TEST

static int
stdio_read(void * p, size_t length, void * file)
{
  size_t r;
  FILE * fp = file;

  r = fread(p, length, 1, fp);
  if (r == 0 && feof(fp))
    return 0;
  return r == 0 ? -1 : 1;
}

static int
stdio_seek(int64_t offset, int whence, void * file)
{
  FILE * fp = file;
  return fseek(fp, offset, whence);
}

static int64_t
stdio_tell(void * fp)
{
  return ftell(fp);
}

static void
log_callback(nestegg * ctx, unsigned int severity, char const * fmt, ...)
{
  va_list ap;
  char const * sev = NULL;

#if !defined(DEBUG)
  if (severity < NESTEGG_LOG_WARNING)
    return;
#endif

  switch (severity) {
  case NESTEGG_LOG_DEBUG:
    sev = "debug:   ";
    break;
  case NESTEGG_LOG_WARNING:
    sev = "warning: ";
    break;
  case NESTEGG_LOG_CRITICAL:
    sev = "critical:";
    break;
  default:
    sev = "unknown: ";
  }

  fprintf(stderr, "%p %s ", (void *) ctx, sev);

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  fprintf(stderr, "\n");
}

int
main(int argc, char * argv[])
{
  FILE * fp;
  int r, type;
  nestegg * ctx;
  nestegg_audio_params aparams;
  nestegg_packet * pkt;
  nestegg_video_params vparams;
  size_t length, size;
  uint64_t duration, tstamp, pkt_tstamp;
  unsigned char * codec_data, * ptr;
  unsigned int cnt, i, j, track, tracks, pkt_cnt, pkt_track;
  unsigned int data_items = 0;
  nestegg_io io = {
    stdio_read,
    stdio_seek,
    stdio_tell,
    NULL
  };

  if (argc != 2)
    return EXIT_FAILURE;

  fp = fopen(argv[1], "rb");
  if (!fp)
    return EXIT_FAILURE;

  io.userdata = fp;

  ctx = NULL;
  r = nestegg_init(&ctx, io, log_callback, -1);
  if (r != 0)
    return EXIT_FAILURE;

  nestegg_track_count(ctx, &tracks);
  nestegg_duration(ctx, &duration);
#if defined(DEBUG)
  fprintf(stderr, "media has %u tracks and duration %fs\n", tracks, duration / 1e9);
#endif

  for (i = 0; i < tracks; ++i) {
    type = nestegg_track_type(ctx, i);
#if defined(DEBUG)
    fprintf(stderr, "track %u: type: %d codec: %d", i,
            type, nestegg_track_codec_id(ctx, i));
#endif
    nestegg_track_codec_data_count(ctx, i, &data_items);
    for (j = 0; j < data_items; ++j) {
      nestegg_track_codec_data(ctx, i, j, &codec_data, &length);
#if defined(DEBUG)
      fprintf(stderr, " (%p, %u)", codec_data, (unsigned int) length);
#endif
    }
    if (type == NESTEGG_TRACK_VIDEO) {
      nestegg_track_video_params(ctx, i, &vparams);
#if defined(DEBUG)
      fprintf(stderr, " video: %ux%u (d: %ux%u %ux%ux%ux%u)",
              vparams.width, vparams.height,
              vparams.display_width, vparams.display_height,
              vparams.crop_top, vparams.crop_left, vparams.crop_bottom, vparams.crop_right);
#endif
    } else if (type == NESTEGG_TRACK_AUDIO) {
      nestegg_track_audio_params(ctx, i, &aparams);
#if defined(DEBUG)
      fprintf(stderr, " audio: %.2fhz %u bit %u channels",
              aparams.rate, aparams.depth, aparams.channels);
#endif
    }
#if defined(DEBUG)
    fprintf(stderr, "\n");
#endif
  }

#if defined(SEEK_TEST)
#if defined(DEBUG)
  fprintf(stderr, "seek to middle\n");
#endif
  r = nestegg_track_seek(ctx, 0, duration / 2);
  if (r == 0) {
#if defined(DEBUG)
    fprintf(stderr, "middle ");
#endif
    r = nestegg_read_packet(ctx, &pkt);
    if (r == 1) {
      nestegg_packet_track(pkt, &track);
      nestegg_packet_count(pkt, &cnt);
      nestegg_packet_tstamp(pkt, &tstamp);
#if defined(DEBUG)
      fprintf(stderr, "* t %u pts %f frames %u\n", track, tstamp / 1e9, cnt);
#endif
      nestegg_free_packet(pkt);
    } else {
#if defined(DEBUG)
      fprintf(stderr, "middle seek failed\n");
#endif
    }
  }

#if defined(DEBUG)
  fprintf(stderr, "seek to ~end\n");
#endif
  r = nestegg_track_seek(ctx, 0, duration - (duration / 10));
  if (r == 0) {
#if defined(DEBUG)
    fprintf(stderr, "end ");
#endif
    r = nestegg_read_packet(ctx, &pkt);
    if (r == 1) {
      nestegg_packet_track(pkt, &track);
      nestegg_packet_count(pkt, &cnt);
      nestegg_packet_tstamp(pkt, &tstamp);
#if defined(DEBUG)
      fprintf(stderr, "* t %u pts %f frames %u\n", track, tstamp / 1e9, cnt);
#endif
      nestegg_free_packet(pkt);
    } else {
#if defined(DEBUG)
      fprintf(stderr, "end seek failed\n");
#endif
    }
  }

#if defined(DEBUG)
  fprintf(stderr, "seek to ~start\n");
#endif
  r = nestegg_track_seek(ctx, 0, duration / 10);
  if (r == 0) {
#if defined(DEBUG)
    fprintf(stderr, "start ");
#endif
    r = nestegg_read_packet(ctx, &pkt);
    if (r == 1) {
      nestegg_packet_track(pkt, &track);
      nestegg_packet_count(pkt, &cnt);
      nestegg_packet_tstamp(pkt, &tstamp);
#if defined(DEBUG)
      fprintf(stderr, "* t %u pts %f frames %u\n", track, tstamp / 1e9, cnt);
#endif
      nestegg_free_packet(pkt);
    } else {
#if defined(DEBUG)
      fprintf(stderr, "start seek failed\n");
#endif
    }
  }
#endif

  while (nestegg_read_packet(ctx, &pkt) > 0) {
    nestegg_packet_track(pkt, &pkt_track);
    nestegg_packet_count(pkt, &pkt_cnt);
    nestegg_packet_tstamp(pkt, &pkt_tstamp);

#if defined(DEBUG)
    fprintf(stderr, "t %u pts %f frames %u: ", pkt_track, pkt_tstamp / 1e9, pkt_cnt);
#endif

    for (i = 0; i < pkt_cnt; ++i) {
      nestegg_packet_data(pkt, i, &ptr, &size);
#if defined(DEBUG)
      fprintf(stderr, "%u ", (unsigned int) size);
#endif
    }
#if defined(DEBUG)
    fprintf(stderr, "\n");
#endif

    nestegg_free_packet(pkt);
  }

  nestegg_destroy(ctx);
  fclose(fp);

  return EXIT_SUCCESS;
}
