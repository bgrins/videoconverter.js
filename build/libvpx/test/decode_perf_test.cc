/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "test/ivf_video_source.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "test/webm_video_source.h"
#include "vpx_ports/vpx_timer.h"
#include "./vpx_version.h"

using std::tr1::make_tuple;

namespace {

#define VIDEO_NAME 0
#define THREADS 1

const double kUsecsInSec = 1000000.0;

/*
 DecodePerfTest takes a tuple of filename + number of threads to decode with
 */
typedef std::tr1::tuple<const char *, unsigned> decode_perf_param_t;

const decode_perf_param_t kVP9DecodePerfVectors[] = {
  make_tuple("vp90-2-bbb_426x240_tile_1x1_180kbps.webm", 1),
  make_tuple("vp90-2-bbb_640x360_tile_1x2_337kbps.webm", 2),
  make_tuple("vp90-2-bbb_854x480_tile_1x2_651kbps.webm", 2),
  make_tuple("vp90-2-bbb_1280x720_tile_1x4_1310kbps.webm", 4),
  make_tuple("vp90-2-bbb_1920x1080_tile_1x1_2581kbps.webm", 1),
  make_tuple("vp90-2-bbb_1920x1080_tile_1x4_2586kbps.webm", 4),
  make_tuple("vp90-2-bbb_1920x1080_tile_1x4_fpm_2304kbps.webm", 4),
  make_tuple("vp90-2-sintel_426x182_tile_1x1_171kbps.webm", 1),
  make_tuple("vp90-2-sintel_640x272_tile_1x2_318kbps.webm", 2),
  make_tuple("vp90-2-sintel_854x364_tile_1x2_621kbps.webm", 2),
  make_tuple("vp90-2-sintel_1280x546_tile_1x4_1257kbps.webm", 4),
  make_tuple("vp90-2-sintel_1920x818_tile_1x4_fpm_2279kbps.webm", 4),
  make_tuple("vp90-2-tos_426x178_tile_1x1_181kbps.webm", 1),
  make_tuple("vp90-2-tos_640x266_tile_1x2_336kbps.webm", 2),
  make_tuple("vp90-2-tos_854x356_tile_1x2_656kbps.webm", 2),
  make_tuple("vp90-2-tos_1280x534_tile_1x4_1306kbps.webm", 4),
  make_tuple("vp90-2-tos_1920x800_tile_1x4_fpm_2335kbps.webm", 4),
};

/*
 In order to reflect real world performance as much as possible, Perf tests
 *DO NOT* do any correctness checks. Please run them alongside correctness
 tests to ensure proper codec integrity. Furthermore, in this test we
 deliberately limit the amount of system calls we make to avoid OS
 preemption.

 TODO(joshualitt) create a more detailed perf measurement test to collect
   power/temp/min max frame decode times/etc
 */

class DecodePerfTest : public ::testing::TestWithParam<decode_perf_param_t> {
};

TEST_P(DecodePerfTest, PerfTest) {
  const char *const video_name = GET_PARAM(VIDEO_NAME);
  const unsigned threads = GET_PARAM(THREADS);

  libvpx_test::WebMVideoSource video(video_name);
  video.Init();

  vpx_codec_dec_cfg_t cfg = {0};
  cfg.threads = threads;
  libvpx_test::VP9Decoder decoder(cfg, 0);

  vpx_usec_timer t;
  vpx_usec_timer_start(&t);

  for (video.Begin(); video.cxdata() != NULL; video.Next()) {
    decoder.DecodeFrame(video.cxdata(), video.frame_size());
  }

  vpx_usec_timer_mark(&t);
  const double elapsed_secs = double(vpx_usec_timer_elapsed(&t))
                              / kUsecsInSec;
  const unsigned frames = video.frame_number();
  const double fps = double(frames) / elapsed_secs;

  printf("{\n");
  printf("\t\"version\" : \"%s\",\n", VERSION_STRING_NOSP);
  printf("\t\"videoName\" : \"%s\",\n", video_name);
  printf("\t\"threadCount\" : %u,\n", threads);
  printf("\t\"decodeTimeSecs\" : %f,\n", elapsed_secs);
  printf("\t\"totalFrames\" : %u,\n", frames);
  printf("\t\"framesPerSecond\" : %f\n", fps);
  printf("}\n");
}

INSTANTIATE_TEST_CASE_P(VP9, DecodePerfTest,
                        ::testing::ValuesIn(kVP9DecodePerfVectors));

}  // namespace
