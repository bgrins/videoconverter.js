/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/* This is a simple program showing how to initialize the decoder in XMA mode */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx_config.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vpx_integer.h"
#if CONFIG_VP9_DECODER
#include "vpx/vp8dx.h"
#endif

static char *exec_name;
static int   verbose = 0;

static const struct {
  const char *name;
  vpx_codec_iface_t *iface;
} ifaces[] = {
#if CONFIG_VP9_DECODER
  {"vp9",  &vpx_codec_vp8_dx_algo},
#endif
};

static void usage_exit(void) {
  int i;

  printf("Usage: %s <options>\n\n"
         "Options:\n"
         "\t--codec <name>\tCodec to use (default=%s)\n"
         "\t-h <height>\tHeight of the simulated video frame, in pixels\n"
         "\t-w <width> \tWidth of the simulated video frame, in pixels\n"
         "\t-v         \tVerbose mode (show individual segment sizes)\n"
         "\t--help     \tShow this message\n"
         "\n"
         "Included decoders:\n"
         "\n",
         exec_name,
         ifaces[0].name);

  for (i = 0; i < sizeof(ifaces) / sizeof(ifaces[0]); i++)
    printf("    %-6s - %s\n",
           ifaces[i].name,
           vpx_codec_iface_name(ifaces[i].iface));

  exit(EXIT_FAILURE);
}

static void usage_error(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  printf("\n");
  usage_exit();
}

void my_mem_dtor(vpx_codec_mmap_t *mmap) {
  if (verbose)
    printf("freeing segment %d\n", mmap->id);

  free(mmap->priv);
}

int main(int argc, char **argv) {
  vpx_codec_ctx_t           decoder;
  vpx_codec_iface_t        *iface = ifaces[0].iface;
  vpx_codec_iter_t          iter;
  vpx_codec_dec_cfg_t       cfg;
  vpx_codec_err_t           res = VPX_CODEC_OK;
  unsigned int            alloc_sz = 0;
  unsigned int            w = 352;
  unsigned int            h = 288;
  int                     i;

  exec_name = argv[0];

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--codec")) {
      if (i + 1 < argc) {
        int j, k = -1;

        i++;

        for (j = 0; j < sizeof(ifaces) / sizeof(ifaces[0]); j++)
          if (!strcmp(ifaces[j].name, argv[i]))
            k = j;

        if (k >= 0)
          iface = ifaces[k].iface;
        else
          usage_error("Error: Unrecognized argument (%s) to --codec\n",
                      argv[i]);
      } else
        usage_error("Error: Option --codec requires argument.\n");
    } else if (!strcmp(argv[i], "-v"))
      verbose = 1;
    else if (!strcmp(argv[i], "-h"))
      if (i + 1 < argc) {
        h = atoi(argv[++i]);
      } else
        usage_error("Error: Option -h requires argument.\n");
    else if (!strcmp(argv[i], "-w"))
      if (i + 1 < argc) {
        w = atoi(argv[++i]);
      } else
        usage_error("Error: Option -w requires argument.\n");
    else if (!strcmp(argv[i], "--help"))
      usage_exit();
    else
      usage_error("Error: Unrecognized option %s\n\n", argv[i]);
  }

  if (argc == 1)
    printf("Using built-in defaults. For options, rerun with --help\n\n");

  /* XMA mode is not supported on all decoders! */
  if (!(vpx_codec_get_caps(iface) & VPX_CODEC_CAP_XMA)) {
    printf("%s does not support XMA mode!\n", vpx_codec_iface_name(iface));
    return EXIT_FAILURE;
  }

  /* The codec knows how much memory to allocate based on the size of the
   * encoded frames. This data can be parsed from the bitstream with
   * vpx_codec_peek_stream_info() if a bitstream is available. Otherwise,
   * a fixed size can be used that will be the upper limit on the frame
   * size the decoder can decode.
   */
  cfg.w = w;
  cfg.h = h;

  /* Initialize the decoder in XMA mode. */
  if (vpx_codec_dec_init(&decoder, iface, &cfg, VPX_CODEC_USE_XMA)) {
    printf("Failed to initialize decoder in XMA mode: %s\n",
           vpx_codec_error(&decoder));
    return EXIT_FAILURE;
  }

  /* Iterate through the list of memory maps, allocating them with the
   * requested alignment.
   */
  iter = NULL;

  do {
    vpx_codec_mmap_t  mmap;
    unsigned int    align;

    res = vpx_codec_get_mem_map(&decoder, &mmap, &iter);
    align = mmap.align ? mmap.align - 1 : 0;

    if (!res) {
      if (verbose)
        printf("Allocating segment %u, size %lu, align %u %s\n",
               mmap.id, mmap.sz, mmap.align,
               mmap.flags & VPX_CODEC_MEM_ZERO ? "(ZEROED)" : "");

      if (mmap.flags & VPX_CODEC_MEM_ZERO)
        mmap.priv = calloc(1, mmap.sz + align);
      else
        mmap.priv = malloc(mmap.sz + align);

      mmap.base = (void *)((((uintptr_t)mmap.priv) + align) &
                  ~(uintptr_t)align);
      mmap.dtor = my_mem_dtor;
      alloc_sz += mmap.sz + align;

      if (vpx_codec_set_mem_map(&decoder, &mmap, 1)) {
        printf("Failed to set mmap: %s\n", vpx_codec_error(&decoder));
        return EXIT_FAILURE;
      }
    } else if (res != VPX_CODEC_LIST_END) {
      printf("Failed to get mmap: %s\n", vpx_codec_error(&decoder));
      return EXIT_FAILURE;
    }
  } while (res != VPX_CODEC_LIST_END);

  printf("%s\n    %d bytes external memory required for %dx%d.\n",
         decoder.name, alloc_sz, cfg.w, cfg.h);
  vpx_codec_destroy(&decoder);
  return EXIT_SUCCESS;

}
