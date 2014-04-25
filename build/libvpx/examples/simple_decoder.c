/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


// Simple Decoder
// ==============
//
// This is an example of a simple decoder loop. It takes an input file
// containing the compressed data (in IVF format), passes it through the
// decoder, and writes the decompressed frames to disk. Other decoder
// examples build upon this one.
//
// The details of the IVF format have been elided from this example for
// simplicity of presentation, as IVF files will not generally be used by
// your application. In general, an IVF file consists of a file header,
// followed by a variable number of frames. Each frame consists of a frame
// header followed by a variable length payload. The length of the payload
// is specified in the first four bytes of the frame header. The payload is
// the raw compressed data.
//
// Standard Includes
// -----------------
// For decoders, you only have to include `vpx_decoder.h` and then any
// header files for the specific codecs you use. In this case, we're using
// vp8. The `VPX_CODEC_DISABLE_COMPAT` macro can be defined to ensure
// strict compliance with the latest SDK by disabling some backwards
// compatibility features. Defining this macro is encouraged.
//
// Initializing The Codec
// ----------------------
// The decoder is initialized by the following code. This is an example for
// the VP8 decoder, but the code is analogous for all algorithms. Replace
// `vpx_codec_vp8_dx()` with a pointer to the interface exposed by the
// algorithm you want to use. The `cfg` argument is left as NULL in this
// example, because we want the algorithm to determine the stream
// configuration (width/height) and allocate memory automatically. This
// parameter is generally only used if you need to preallocate memory,
// particularly in External Memory Allocation mode.
//
// Decoding A Frame
// ----------------
// Once the frame has been read into memory, it is decoded using the
// `vpx_codec_decode` function. The call takes a pointer to the data
// (`frame`) and the length of the data (`frame_sz`). No application data
// is associated with the frame in this example, so the `user_priv`
// parameter is NULL. The `deadline` parameter is left at zero for this
// example. This parameter is generally only used when doing adaptive
// postprocessing.
//
// Codecs may produce a variable number of output frames for every call to
// `vpx_codec_decode`. These frames are retrieved by the
// `vpx_codec_get_frame` iterator function. The iterator variable `iter` is
// initialized to NULL each time `vpx_codec_decode` is called.
// `vpx_codec_get_frame` is called in a loop, returning a pointer to a
// decoded image or NULL to indicate the end of list.
//
// Processing The Decoded Data
// ---------------------------
// In this example, we simply write the encoded data to disk. It is
// important to honor the image's `stride` values.
//
// Cleanup
// -------
// The `vpx_codec_destroy` call frees any memory allocated by the codec.
//
// Error Handling
// --------------
// This example does not special case any error return codes. If there was
// an error, a descriptive message is printed and the program exits. With
// few exeptions, vpx_codec functions return an enumerated error status,
// with the value `0` indicating success.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VPX_CODEC_DISABLE_COMPAT 1

#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

#include "./tools_common.h"
#include "./video_reader.h"
#include "./vpx_config.h"

static const char *exec_name;

void usage_exit() {
  fprintf(stderr, "Usage: %s <infile> <outfile>\n", exec_name);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  int frame_cnt = 0;
  FILE *outfile = NULL;
  vpx_codec_ctx_t codec;
  VpxVideoReader *reader = NULL;
  const VpxInterface *decoder = NULL;
  const VpxVideoInfo *info = NULL;

  exec_name = argv[0];

  if (argc != 3)
    die("Invalid number of arguments.");

  reader = vpx_video_reader_open(argv[1]);
  if (!reader)
    die("Failed to open %s for reading.", argv[1]);

  if (!(outfile = fopen(argv[2], "wb")))
    die("Failed to open %s for writing.", argv[2]);

  info = vpx_video_reader_get_info(reader);

  decoder = get_vpx_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder)
    die("Unknown input codec.");

  printf("Using %s\n", vpx_codec_iface_name(decoder->interface()));

  if (vpx_codec_dec_init(&codec, decoder->interface(), NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");

  while (vpx_video_reader_read_frame(reader)) {
    vpx_codec_iter_t iter = NULL;
    vpx_image_t *img = NULL;
    size_t frame_size = 0;
    const unsigned char *frame = vpx_video_reader_get_frame(reader,
                                                            &frame_size);
    if (vpx_codec_decode(&codec, frame, (unsigned int)frame_size, NULL, 0))
      die_codec(&codec, "Failed to decode frame.");

    while ((img = vpx_codec_get_frame(&codec, &iter)) != NULL) {
      vpx_img_write(img, outfile);
      ++frame_cnt;
    }
  }

  printf("Processed %d frames.\n", frame_cnt);
  if (vpx_codec_destroy(&codec))
    die_codec(&codec, "Failed to destroy codec");

  printf("Play: ffplay -f rawvideo -pix_fmt yuv420p -s %dx%d %s\n",
         info->frame_width, info->frame_height, argv[2]);

  vpx_video_reader_close(reader);

  fclose(outfile);

  return EXIT_SUCCESS;
}
