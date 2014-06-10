/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This is an example demonstrating how to implement a multi-layer
 * VP9 encoding scheme based on spatial scalability for video applications
 * that benefit from a scalable bitstream.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "./args.h"
#include "./tools_common.h"
#include "./video_writer.h"

#include "vpx/svc_context.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"
#include "./vpxstats.h"

static const struct arg_enum_list encoding_mode_enum[] = {
  {"i", INTER_LAYER_PREDICTION_I},
  {"alt-ip", ALT_INTER_LAYER_PREDICTION_IP},
  {"ip", INTER_LAYER_PREDICTION_IP},
  {"gf", USE_GOLDEN_FRAME},
  {NULL, 0}
};

static const arg_def_t encoding_mode_arg = ARG_DEF_ENUM(
    "m", "encoding-mode", 1, "Encoding mode algorithm", encoding_mode_enum);
static const arg_def_t skip_frames_arg =
    ARG_DEF("s", "skip-frames", 1, "input frames to skip");
static const arg_def_t frames_arg =
    ARG_DEF("f", "frames", 1, "number of frames to encode");
static const arg_def_t width_arg = ARG_DEF("w", "width", 1, "source width");
static const arg_def_t height_arg = ARG_DEF("h", "height", 1, "source height");
static const arg_def_t timebase_arg =
    ARG_DEF("t", "timebase", 1, "timebase (num/den)");
static const arg_def_t bitrate_arg = ARG_DEF(
    "b", "target-bitrate", 1, "encoding bitrate, in kilobits per second");
static const arg_def_t layers_arg =
    ARG_DEF("l", "layers", 1, "number of SVC layers");
static const arg_def_t kf_dist_arg =
    ARG_DEF("k", "kf-dist", 1, "number of frames between keyframes");
static const arg_def_t scale_factors_arg =
    ARG_DEF("r", "scale-factors", 1, "scale factors (lowest to highest layer)");
static const arg_def_t quantizers_arg =
    ARG_DEF("q", "quantizers", 1, "quantizers for non key frames, also will "
            "be applied to key frames if -qn is not specified (lowest to "
            "highest layer)");
static const arg_def_t quantizers_keyframe_arg =
    ARG_DEF("qn", "quantizers-keyframe", 1, "quantizers for key frames (lowest "
        "to highest layer)");
static const arg_def_t passes_arg =
    ARG_DEF("p", "passes", 1, "Number of passes (1/2)");
static const arg_def_t pass_arg =
    ARG_DEF(NULL, "pass", 1, "Pass to execute (1/2)");
static const arg_def_t fpf_name_arg =
    ARG_DEF(NULL, "fpf", 1, "First pass statistics file name");
static const arg_def_t min_q_arg =
    ARG_DEF(NULL, "min-q", 1, "Minimum quantizer");
static const arg_def_t max_q_arg =
    ARG_DEF(NULL, "max-q", 1, "Maximum quantizer");
static const arg_def_t min_bitrate_arg =
    ARG_DEF(NULL, "min-bitrate", 1, "Minimum bitrate");
static const arg_def_t max_bitrate_arg =
    ARG_DEF(NULL, "max-bitrate", 1, "Maximum bitrate");

static const arg_def_t *svc_args[] = {
  &encoding_mode_arg, &frames_arg,        &width_arg,       &height_arg,
  &timebase_arg,      &bitrate_arg,       &skip_frames_arg, &layers_arg,
  &kf_dist_arg,       &scale_factors_arg, &quantizers_arg,
  &quantizers_keyframe_arg,               &passes_arg,      &pass_arg,
  &fpf_name_arg,      &min_q_arg,         &max_q_arg,       &min_bitrate_arg,
  &max_bitrate_arg,   NULL
};

static const SVC_ENCODING_MODE default_encoding_mode =
    INTER_LAYER_PREDICTION_IP;
static const uint32_t default_frames_to_skip = 0;
static const uint32_t default_frames_to_code = 60 * 60;
static const uint32_t default_width = 1920;
static const uint32_t default_height = 1080;
static const uint32_t default_timebase_num = 1;
static const uint32_t default_timebase_den = 60;
static const uint32_t default_bitrate = 1000;
static const uint32_t default_spatial_layers = 5;
static const uint32_t default_kf_dist = 100;

typedef struct {
  const char *input_filename;
  const char *output_filename;
  uint32_t frames_to_code;
  uint32_t frames_to_skip;
  struct VpxInputContext input_ctx;
  stats_io_t rc_stats;
  int passes;
  int pass;
} AppInput;

static const char *exec_name;

void usage_exit() {
  fprintf(stderr, "Usage: %s <options> input_filename output_filename\n",
          exec_name);
  fprintf(stderr, "Options:\n");
  arg_show_usage(stderr, svc_args);
  exit(EXIT_FAILURE);
}

static void parse_command_line(int argc, const char **argv_,
                               AppInput *app_input, SvcContext *svc_ctx,
                               vpx_codec_enc_cfg_t *enc_cfg) {
  struct arg arg = {0};
  char **argv = NULL;
  char **argi = NULL;
  char **argj = NULL;
  vpx_codec_err_t res;
  int passes = 0;
  int pass = 0;
  const char *fpf_file_name = NULL;
  unsigned int min_bitrate = 0;
  unsigned int max_bitrate = 0;

  // initialize SvcContext with parameters that will be passed to vpx_svc_init
  svc_ctx->log_level = SVC_LOG_DEBUG;
  svc_ctx->spatial_layers = default_spatial_layers;
  svc_ctx->encoding_mode = default_encoding_mode;

  // start with default encoder configuration
  res = vpx_codec_enc_config_default(vpx_codec_vp9_cx(), enc_cfg, 0);
  if (res) {
    die("Failed to get config: %s\n", vpx_codec_err_to_string(res));
  }
  // update enc_cfg with app default values
  enc_cfg->g_w = default_width;
  enc_cfg->g_h = default_height;
  enc_cfg->g_timebase.num = default_timebase_num;
  enc_cfg->g_timebase.den = default_timebase_den;
  enc_cfg->rc_target_bitrate = default_bitrate;
  enc_cfg->kf_min_dist = default_kf_dist;
  enc_cfg->kf_max_dist = default_kf_dist;

  // initialize AppInput with default values
  app_input->frames_to_code = default_frames_to_code;
  app_input->frames_to_skip = default_frames_to_skip;

  // process command line options
  argv = argv_dup(argc - 1, argv_ + 1);
  for (argi = argj = argv; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;

    if (arg_match(&arg, &encoding_mode_arg, argi)) {
      svc_ctx->encoding_mode = arg_parse_enum_or_int(&arg);
    } else if (arg_match(&arg, &frames_arg, argi)) {
      app_input->frames_to_code = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &width_arg, argi)) {
      enc_cfg->g_w = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &height_arg, argi)) {
      enc_cfg->g_h = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &timebase_arg, argi)) {
      enc_cfg->g_timebase = arg_parse_rational(&arg);
    } else if (arg_match(&arg, &bitrate_arg, argi)) {
      enc_cfg->rc_target_bitrate = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &skip_frames_arg, argi)) {
      app_input->frames_to_skip = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &layers_arg, argi)) {
      svc_ctx->spatial_layers = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &kf_dist_arg, argi)) {
      enc_cfg->kf_min_dist = arg_parse_uint(&arg);
      enc_cfg->kf_max_dist = enc_cfg->kf_min_dist;
    } else if (arg_match(&arg, &scale_factors_arg, argi)) {
      vpx_svc_set_scale_factors(svc_ctx, arg.val);
    } else if (arg_match(&arg, &quantizers_arg, argi)) {
      vpx_svc_set_quantizers(svc_ctx, arg.val, 0);
    } else if (arg_match(&arg, &quantizers_keyframe_arg, argi)) {
      vpx_svc_set_quantizers(svc_ctx, arg.val, 1);
    } else if (arg_match(&arg, &passes_arg, argi)) {
      passes = arg_parse_uint(&arg);
      if (passes < 1 || passes > 2) {
        die("Error: Invalid number of passes (%d)\n", passes);
      }
    } else if (arg_match(&arg, &pass_arg, argi)) {
      pass = arg_parse_uint(&arg);
      if (pass < 1 || pass > 2) {
        die("Error: Invalid pass selected (%d)\n", pass);
      }
    } else if (arg_match(&arg, &fpf_name_arg, argi)) {
      fpf_file_name = arg.val;
    } else if (arg_match(&arg, &min_q_arg, argi)) {
      enc_cfg->rc_min_quantizer = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &max_q_arg, argi)) {
      enc_cfg->rc_max_quantizer = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &min_bitrate_arg, argi)) {
      min_bitrate = arg_parse_uint(&arg);
    } else if (arg_match(&arg, &max_bitrate_arg, argi)) {
      max_bitrate = arg_parse_uint(&arg);
    } else {
      ++argj;
    }
  }

  if (passes == 0 || passes == 1) {
    if (pass) {
      fprintf(stderr, "pass is ignored since there's only one pass\n");
    }
    enc_cfg->g_pass = VPX_RC_ONE_PASS;
  } else {
    if (pass == 0) {
      die("pass must be specified when passes is 2\n");
    }

    if (fpf_file_name == NULL) {
      die("fpf must be specified when passes is 2\n");
    }

    if (pass == 1) {
      enc_cfg->g_pass = VPX_RC_FIRST_PASS;
      if (!stats_open_file(&app_input->rc_stats, fpf_file_name, 0)) {
        fatal("Failed to open statistics store");
      }
    } else {
      enc_cfg->g_pass = VPX_RC_LAST_PASS;
      if (!stats_open_file(&app_input->rc_stats, fpf_file_name, 1)) {
        fatal("Failed to open statistics store");
      }
      enc_cfg->rc_twopass_stats_in = stats_get(&app_input->rc_stats);
    }
    app_input->passes = passes;
    app_input->pass = pass;
  }

  if (enc_cfg->rc_target_bitrate > 0) {
    if (min_bitrate > 0) {
      enc_cfg->rc_2pass_vbr_minsection_pct =
          min_bitrate * 100 / enc_cfg->rc_target_bitrate;
    }
    if (max_bitrate > 0) {
      enc_cfg->rc_2pass_vbr_maxsection_pct =
          max_bitrate * 100 / enc_cfg->rc_target_bitrate;
    }
  }

  // Check for unrecognized options
  for (argi = argv; *argi; ++argi)
    if (argi[0][0] == '-' && strlen(argi[0]) > 1)
      die("Error: Unrecognized option %s\n", *argi);

  if (argv[0] == NULL || argv[1] == 0) {
    usage_exit();
  }
  app_input->input_filename = argv[0];
  app_input->output_filename = argv[1];
  free(argv);

  if (enc_cfg->g_w < 16 || enc_cfg->g_w % 2 || enc_cfg->g_h < 16 ||
      enc_cfg->g_h % 2)
    die("Invalid resolution: %d x %d\n", enc_cfg->g_w, enc_cfg->g_h);

  printf(
      "Codec %s\nframes: %d, skip: %d\n"
      "mode: %d, layers: %d\n"
      "width %d, height: %d,\n"
      "num: %d, den: %d, bitrate: %d,\n"
      "gop size: %d\n",
      vpx_codec_iface_name(vpx_codec_vp9_cx()), app_input->frames_to_code,
      app_input->frames_to_skip, svc_ctx->encoding_mode,
      svc_ctx->spatial_layers, enc_cfg->g_w, enc_cfg->g_h,
      enc_cfg->g_timebase.num, enc_cfg->g_timebase.den,
      enc_cfg->rc_target_bitrate, enc_cfg->kf_max_dist);
}

int main(int argc, const char **argv) {
  AppInput app_input = {0};
  VpxVideoWriter *writer = NULL;
  VpxVideoInfo info = {0};
  vpx_codec_ctx_t codec;
  vpx_codec_enc_cfg_t enc_cfg;
  SvcContext svc_ctx;
  uint32_t i;
  uint32_t frame_cnt = 0;
  vpx_image_t raw;
  vpx_codec_err_t res;
  int pts = 0;            /* PTS starts at 0 */
  int frame_duration = 1; /* 1 timebase tick per frame */
  FILE *infile = NULL;
  int end_of_stream = 0;

  memset(&svc_ctx, 0, sizeof(svc_ctx));
  svc_ctx.log_print = 1;
  exec_name = argv[0];
  parse_command_line(argc, argv, &app_input, &svc_ctx, &enc_cfg);

  // Allocate image buffer
  if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, enc_cfg.g_w, enc_cfg.g_h, 32))
    die("Failed to allocate image %dx%d\n", enc_cfg.g_w, enc_cfg.g_h);

  if (!(infile = fopen(app_input.input_filename, "rb")))
    die("Failed to open %s for reading\n", app_input.input_filename);

  // Initialize codec
  if (vpx_svc_init(&svc_ctx, &codec, vpx_codec_vp9_cx(), &enc_cfg) !=
      VPX_CODEC_OK)
    die("Failed to initialize encoder\n");

  info.codec_fourcc = VP9_FOURCC;
  info.time_base.numerator = enc_cfg.g_timebase.num;
  info.time_base.denominator = enc_cfg.g_timebase.den;
  if (vpx_svc_get_layer_resolution(&svc_ctx, svc_ctx.spatial_layers - 1,
                                   (unsigned int *)&info.frame_width,
                                   (unsigned int *)&info.frame_height) !=
      VPX_CODEC_OK) {
    die("Failed to get output resolution");
  }

  if (!(app_input.passes == 2 && app_input.pass == 1)) {
    // We don't save the bitstream for the 1st pass on two pass rate control
    writer = vpx_video_writer_open(app_input.output_filename, kContainerIVF,
                                   &info);
    if (!writer)
      die("Failed to open %s for writing\n", app_input.output_filename);
  }

  // skip initial frames
  for (i = 0; i < app_input.frames_to_skip; ++i)
    vpx_img_read(&raw, infile);

  // Encode frames
  while (!end_of_stream) {
    if (frame_cnt >= app_input.frames_to_code || !vpx_img_read(&raw, infile)) {
      // We need one extra vpx_svc_encode call at end of stream to flush
      // encoder and get remaining data
      end_of_stream = 1;
    }

    res = vpx_svc_encode(&svc_ctx, &codec, (end_of_stream ? NULL : &raw),
                         pts, frame_duration, VPX_DL_REALTIME);
    printf("%s", vpx_svc_get_message(&svc_ctx));
    if (res != VPX_CODEC_OK) {
      die_codec(&codec, "Failed to encode frame");
    }
    if (!(app_input.passes == 2 && app_input.pass == 1)) {
      if (vpx_svc_get_frame_size(&svc_ctx) > 0) {
        vpx_video_writer_write_frame(writer,
                                     vpx_svc_get_buffer(&svc_ctx),
                                     vpx_svc_get_frame_size(&svc_ctx),
                                     pts);
      }
    }
    if (vpx_svc_get_rc_stats_buffer_size(&svc_ctx) > 0) {
      stats_write(&app_input.rc_stats,
                  vpx_svc_get_rc_stats_buffer(&svc_ctx),
                  vpx_svc_get_rc_stats_buffer_size(&svc_ctx));
    }
    if (!end_of_stream) {
      ++frame_cnt;
      pts += frame_duration;
    }
  }

  printf("Processed %d frames\n", frame_cnt);

  fclose(infile);
  if (vpx_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");

  if (app_input.passes == 2)
    stats_close(&app_input.rc_stats, 1);

  if (writer) {
    vpx_video_writer_close(writer);
  }

  vpx_img_free(&raw);

  // display average size, psnr
  printf("%s", vpx_svc_dump_statistics(&svc_ctx));

  vpx_svc_release(&svc_ctx);

  return EXIT_SUCCESS;
}
