/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "test/i420_video_source.h"
#include "vpx/svc_context.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"

namespace {

using libvpx_test::CodecFactory;
using libvpx_test::Decoder;
using libvpx_test::VP9CodecFactory;

class SvcTest : public ::testing::Test {
 protected:
  static const uint32_t kWidth = 352;
  static const uint32_t kHeight = 288;

  SvcTest()
      : codec_iface_(0),
        test_file_name_("hantro_collage_w352h288.yuv"),
        stats_file_name_("hantro_collage_w352h288.stat"),
        codec_initialized_(false),
        decoder_(0) {
    memset(&svc_, 0, sizeof(svc_));
    memset(&codec_, 0, sizeof(codec_));
    memset(&codec_enc_, 0, sizeof(codec_enc_));
  }

  virtual ~SvcTest() {}

  virtual void SetUp() {
    svc_.encoding_mode = INTER_LAYER_PREDICTION_IP;
    svc_.log_level = SVC_LOG_DEBUG;
    svc_.log_print = 0;

    codec_iface_ = vpx_codec_vp9_cx();
    const vpx_codec_err_t res =
        vpx_codec_enc_config_default(codec_iface_, &codec_enc_, 0);
    EXPECT_EQ(VPX_CODEC_OK, res);

    codec_enc_.g_w = kWidth;
    codec_enc_.g_h = kHeight;
    codec_enc_.g_timebase.num = 1;
    codec_enc_.g_timebase.den = 60;
    codec_enc_.kf_min_dist = 100;
    codec_enc_.kf_max_dist = 100;

    vpx_codec_dec_cfg_t dec_cfg = {0};
    VP9CodecFactory codec_factory;
    decoder_ = codec_factory.CreateDecoder(dec_cfg, 0);
  }

  virtual void TearDown() {
    vpx_svc_release(&svc_);
    delete(decoder_);
    if (codec_initialized_) vpx_codec_destroy(&codec_);
  }

  SvcContext svc_;
  vpx_codec_ctx_t codec_;
  struct vpx_codec_enc_cfg codec_enc_;
  vpx_codec_iface_t *codec_iface_;
  std::string test_file_name_;
  std::string stats_file_name_;
  bool codec_initialized_;
  Decoder *decoder_;
};

TEST_F(SvcTest, SvcInit) {
  // test missing parameters
  vpx_codec_err_t res = vpx_svc_init(NULL, &codec_, codec_iface_, &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);
  res = vpx_svc_init(&svc_, NULL, codec_iface_, &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);
  res = vpx_svc_init(&svc_, &codec_, NULL, &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_init(&svc_, &codec_, codec_iface_, NULL);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  svc_.spatial_layers = 6;  // too many layers
  res = vpx_svc_init(&svc_, &codec_, codec_iface_, &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  svc_.spatial_layers = 0;  // use default layers
  res = vpx_svc_init(&svc_, &codec_, codec_iface_, &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
  EXPECT_EQ(VPX_SS_DEFAULT_LAYERS, svc_.spatial_layers);
}

TEST_F(SvcTest, InitTwoLayers) {
  svc_.spatial_layers = 2;
  vpx_svc_set_scale_factors(&svc_, "4/16,16*16");  // invalid scale values
  vpx_codec_err_t res = vpx_svc_init(&svc_, &codec_, codec_iface_, &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  vpx_svc_set_scale_factors(&svc_, "4/16,16/16");  // valid scale values
  res = vpx_svc_init(&svc_, &codec_, codec_iface_, &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
}

TEST_F(SvcTest, InvalidOptions) {
  vpx_codec_err_t res = vpx_svc_set_options(&svc_, NULL);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_set_options(&svc_, "not-an-option=1");
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);
}

TEST_F(SvcTest, SetLayersOption) {
  vpx_codec_err_t res = vpx_svc_set_options(&svc_, "layers=3");
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
  EXPECT_EQ(3, svc_.spatial_layers);
}

TEST_F(SvcTest, SetEncodingMode) {
  vpx_codec_err_t res = vpx_svc_set_options(&svc_, "encoding-mode=alt-ip");
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
  EXPECT_EQ(ALT_INTER_LAYER_PREDICTION_IP, svc_.encoding_mode);
}

TEST_F(SvcTest, SetMultipleOptions) {
  vpx_codec_err_t res = vpx_svc_set_options(&svc_, "layers=2 encoding-mode=ip");
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
  EXPECT_EQ(2, svc_.spatial_layers);
  EXPECT_EQ(INTER_LAYER_PREDICTION_IP, svc_.encoding_mode);
}

TEST_F(SvcTest, SetScaleFactorsOption) {
  svc_.spatial_layers = 2;
  vpx_codec_err_t res =
      vpx_svc_set_options(&svc_, "scale-factors=not-scale-factors");
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_set_options(&svc_, "scale-factors=1/3,2/3");
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
}

TEST_F(SvcTest, SetQuantizersOption) {
  svc_.spatial_layers = 2;
  vpx_codec_err_t res = vpx_svc_set_options(&svc_, "quantizers=not-quantizers");
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  vpx_svc_set_options(&svc_, "quantizers=40,45");
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
}

TEST_F(SvcTest, SetKeyFrameQuantizersOption) {
  svc_.spatial_layers = 2;
  vpx_codec_err_t res = vpx_svc_set_options(&svc_,
                                       "quantizers-keyframe=not-quantizers");
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  vpx_svc_set_options(&svc_, "quantizers-keyframe=40,45");
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
}

TEST_F(SvcTest, SetQuantizers) {
  vpx_codec_err_t res = vpx_svc_set_quantizers(NULL, "40,30", 0);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_set_quantizers(&svc_, NULL, 0);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  svc_.spatial_layers = 2;
  res = vpx_svc_set_quantizers(&svc_, "40", 0);
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_set_quantizers(&svc_, "40,30", 0);
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
}

TEST_F(SvcTest, SetKeyFrameQuantizers) {
  vpx_codec_err_t res = vpx_svc_set_quantizers(NULL, "40,31", 1);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_set_quantizers(&svc_, NULL, 1);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_set_quantizers(&svc_, "40,30", 1);
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
}

TEST_F(SvcTest, SetScaleFactors) {
  vpx_codec_err_t res = vpx_svc_set_scale_factors(NULL, "4/16,16/16");
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_set_scale_factors(&svc_, NULL);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  svc_.spatial_layers = 2;
  res = vpx_svc_set_scale_factors(&svc_, "4/16");
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_set_scale_factors(&svc_, "4/16,16/16");
  EXPECT_EQ(VPX_CODEC_OK, res);
  res = vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;
}

// Test that decoder can handle an SVC frame as the first frame in a sequence.
TEST_F(SvcTest, FirstFrameHasLayers) {
  svc_.spatial_layers = 2;
  vpx_svc_set_scale_factors(&svc_, "4/16,16/16");
  vpx_svc_set_quantizers(&svc_, "40,30", 0);

  vpx_codec_err_t res =
      vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;

  libvpx_test::I420VideoSource video(test_file_name_, kWidth, kHeight,
                                     codec_enc_.g_timebase.den,
                                     codec_enc_.g_timebase.num, 0, 30);
  video.Begin();

  res = vpx_svc_encode(&svc_, &codec_, video.img(), video.pts(),
                       video.duration(), VPX_DL_GOOD_QUALITY);
  EXPECT_EQ(VPX_CODEC_OK, res);

  const vpx_codec_err_t res_dec = decoder_->DecodeFrame(
      static_cast<const uint8_t *>(vpx_svc_get_buffer(&svc_)),
      vpx_svc_get_frame_size(&svc_));

  // this test fails with a decoder error
  ASSERT_EQ(VPX_CODEC_OK, res_dec) << decoder_->DecodeError();
}

TEST_F(SvcTest, EncodeThreeFrames) {
  svc_.spatial_layers = 2;
  vpx_svc_set_scale_factors(&svc_, "4/16,16/16");
  vpx_svc_set_quantizers(&svc_, "40,30", 0);

  vpx_codec_err_t res =
      vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  ASSERT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;

  libvpx_test::I420VideoSource video(test_file_name_, kWidth, kHeight,
                                     codec_enc_.g_timebase.den,
                                     codec_enc_.g_timebase.num, 0, 30);
  // FRAME 0
  video.Begin();
  // This frame is a keyframe.
  res = vpx_svc_encode(&svc_, &codec_, video.img(), video.pts(),
                       video.duration(), VPX_DL_GOOD_QUALITY);
  ASSERT_EQ(VPX_CODEC_OK, res);
  EXPECT_EQ(1, vpx_svc_is_keyframe(&svc_));

  vpx_codec_err_t res_dec = decoder_->DecodeFrame(
      static_cast<const uint8_t *>(vpx_svc_get_buffer(&svc_)),
      vpx_svc_get_frame_size(&svc_));
  ASSERT_EQ(VPX_CODEC_OK, res_dec) << decoder_->DecodeError();

  // FRAME 1
  video.Next();
  // This is a P-frame.
  res = vpx_svc_encode(&svc_, &codec_, video.img(), video.pts(),
                       video.duration(), VPX_DL_GOOD_QUALITY);
  ASSERT_EQ(VPX_CODEC_OK, res);
  EXPECT_EQ(0, vpx_svc_is_keyframe(&svc_));

  res_dec = decoder_->DecodeFrame(
      static_cast<const uint8_t *>(vpx_svc_get_buffer(&svc_)),
      vpx_svc_get_frame_size(&svc_));
  ASSERT_EQ(VPX_CODEC_OK, res_dec) << decoder_->DecodeError();

  // FRAME 2
  video.Next();
  // This is a P-frame.
  res = vpx_svc_encode(&svc_, &codec_, video.img(), video.pts(),
                       video.duration(), VPX_DL_GOOD_QUALITY);
  ASSERT_EQ(VPX_CODEC_OK, res);
  EXPECT_EQ(0, vpx_svc_is_keyframe(&svc_));

  res_dec = decoder_->DecodeFrame(
      static_cast<const uint8_t *>(vpx_svc_get_buffer(&svc_)),
      vpx_svc_get_frame_size(&svc_));
  ASSERT_EQ(VPX_CODEC_OK, res_dec) << decoder_->DecodeError();
}

TEST_F(SvcTest, GetLayerResolution) {
  svc_.spatial_layers = 2;
  vpx_svc_set_scale_factors(&svc_, "4/16,8/16");
  vpx_svc_set_quantizers(&svc_, "40,30", 0);

  vpx_codec_err_t res =
      vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  EXPECT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;

  // ensure that requested layer is a valid layer
  uint32_t layer_width, layer_height;
  res = vpx_svc_get_layer_resolution(&svc_, svc_.spatial_layers,
                                     &layer_width, &layer_height);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_get_layer_resolution(NULL, 0, &layer_width, &layer_height);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_get_layer_resolution(&svc_, 0, NULL, &layer_height);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_get_layer_resolution(&svc_, 0, &layer_width, NULL);
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, res);

  res = vpx_svc_get_layer_resolution(&svc_, 0, &layer_width, &layer_height);
  EXPECT_EQ(VPX_CODEC_OK, res);
  EXPECT_EQ(kWidth * 4 / 16, layer_width);
  EXPECT_EQ(kHeight * 4 / 16, layer_height);

  res = vpx_svc_get_layer_resolution(&svc_, 1, &layer_width, &layer_height);
  EXPECT_EQ(VPX_CODEC_OK, res);
  EXPECT_EQ(kWidth * 8 / 16, layer_width);
  EXPECT_EQ(kHeight * 8 / 16, layer_height);
}

TEST_F(SvcTest, FirstPassEncode) {
  svc_.spatial_layers = 2;
  codec_enc_.g_pass = VPX_RC_FIRST_PASS;
  vpx_svc_set_scale_factors(&svc_, "4/16,16/16");
  vpx_svc_set_quantizers(&svc_, "40,30", 0);

  vpx_codec_err_t res =
      vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  ASSERT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;

  libvpx_test::I420VideoSource video(test_file_name_, kWidth, kHeight,
                                     codec_enc_.g_timebase.den,
                                     codec_enc_.g_timebase.num, 0, 30);
  // FRAME 0
  video.Begin();
  res = vpx_svc_encode(&svc_, &codec_, video.img(), video.pts(),
                       video.duration(), VPX_DL_GOOD_QUALITY);
  ASSERT_EQ(VPX_CODEC_OK, res);
  EXPECT_GT(vpx_svc_get_rc_stats_buffer_size(&svc_), 0U);

  // FRAME 1
  video.Next();
  res = vpx_svc_encode(&svc_, &codec_, video.img(), video.pts(),
                       video.duration(), VPX_DL_GOOD_QUALITY);
  ASSERT_EQ(VPX_CODEC_OK, res);
  EXPECT_GT(vpx_svc_get_rc_stats_buffer_size(&svc_), 0U);

  // Flush encoder and test EOS packet
  res = vpx_svc_encode(&svc_, &codec_, NULL, video.pts(),
                       video.duration(), VPX_DL_GOOD_QUALITY);
  ASSERT_EQ(VPX_CODEC_OK, res);
  EXPECT_GT(vpx_svc_get_rc_stats_buffer_size(&svc_), 0U);
}

TEST_F(SvcTest, SecondPassEncode) {
  svc_.spatial_layers = 2;
  codec_enc_.g_pass = VPX_RC_LAST_PASS;

  FILE *const stats_file = libvpx_test::OpenTestDataFile(stats_file_name_);
  ASSERT_TRUE(stats_file != NULL) << "Stats file open failed. Filename: "
      << stats_file;

  struct vpx_fixed_buf stats_buf;
  fseek(stats_file, 0, SEEK_END);
  stats_buf.sz = static_cast<size_t>(ftell(stats_file));
  fseek(stats_file, 0, SEEK_SET);

  stats_buf.buf = malloc(stats_buf.sz);
  ASSERT_TRUE(stats_buf.buf != NULL);
  const size_t bytes_read = fread(stats_buf.buf, 1, stats_buf.sz, stats_file);
  ASSERT_EQ(bytes_read, stats_buf.sz);
  fclose(stats_file);
  codec_enc_.rc_twopass_stats_in = stats_buf;

  vpx_codec_err_t res =
      vpx_svc_init(&svc_, &codec_, vpx_codec_vp9_cx(), &codec_enc_);
  ASSERT_EQ(VPX_CODEC_OK, res);
  codec_initialized_ = true;

  libvpx_test::I420VideoSource video(test_file_name_, kWidth, kHeight,
                                     codec_enc_.g_timebase.den,
                                     codec_enc_.g_timebase.num, 0, 30);
  // FRAME 0
  video.Begin();
  // This frame is a keyframe.
  res = vpx_svc_encode(&svc_, &codec_, video.img(), video.pts(),
                       video.duration(), VPX_DL_GOOD_QUALITY);
  ASSERT_EQ(VPX_CODEC_OK, res);
  EXPECT_EQ(1, vpx_svc_is_keyframe(&svc_));

  vpx_codec_err_t res_dec = decoder_->DecodeFrame(
      static_cast<const uint8_t *>(vpx_svc_get_buffer(&svc_)),
      vpx_svc_get_frame_size(&svc_));
  ASSERT_EQ(VPX_CODEC_OK, res_dec) << decoder_->DecodeError();

  // FRAME 1
  video.Next();
  // This is a P-frame.
  res = vpx_svc_encode(&svc_, &codec_, video.img(), video.pts(),
                       video.duration(), VPX_DL_GOOD_QUALITY);
  ASSERT_EQ(VPX_CODEC_OK, res);
  EXPECT_EQ(0, vpx_svc_is_keyframe(&svc_));

  res_dec = decoder_->DecodeFrame(
      static_cast<const uint8_t *>(vpx_svc_get_buffer(&svc_)),
      vpx_svc_get_frame_size(&svc_));
  ASSERT_EQ(VPX_CODEC_OK, res_dec) << decoder_->DecodeError();

  // FRAME 2
  video.Next();
  // This is a P-frame.
  res = vpx_svc_encode(&svc_, &codec_, video.img(), video.pts(),
                       video.duration(), VPX_DL_GOOD_QUALITY);
  ASSERT_EQ(VPX_CODEC_OK, res);
  EXPECT_EQ(0, vpx_svc_is_keyframe(&svc_));

  res_dec = decoder_->DecodeFrame(
      static_cast<const uint8_t *>(vpx_svc_get_buffer(&svc_)),
      vpx_svc_get_frame_size(&svc_));
  ASSERT_EQ(VPX_CODEC_OK, res_dec) << decoder_->DecodeError();

  free(stats_buf.buf);
}

}  // namespace
