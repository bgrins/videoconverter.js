/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/register_state_check.h"
#include "test/video_source.h"

namespace libvpx_test {

vpx_codec_err_t Decoder::DecodeFrame(const uint8_t *cxdata, size_t size) {
  vpx_codec_err_t res_dec;
  InitOnce();
  REGISTER_STATE_CHECK(
      res_dec = vpx_codec_decode(&decoder_,
                                 cxdata, static_cast<unsigned int>(size),
                                 NULL, 0));
  return res_dec;
}

void DecoderTest::RunLoop(CompressedVideoSource *video) {
  vpx_codec_dec_cfg_t dec_cfg = {0};
  Decoder* const decoder = codec_->CreateDecoder(dec_cfg, 0);
  ASSERT_TRUE(decoder != NULL);

  // Decode frames.
  for (video->Begin(); video->cxdata(); video->Next()) {
    PreDecodeFrameHook(*video, decoder);
    vpx_codec_err_t res_dec = decoder->DecodeFrame(video->cxdata(),
                                                   video->frame_size());
    ASSERT_EQ(VPX_CODEC_OK, res_dec) << decoder->DecodeError();

    DxDataIterator dec_iter = decoder->GetDxData();
    const vpx_image_t *img = NULL;

    // Get decompressed data
    while ((img = dec_iter.Next()))
      DecompressedFrameHook(*img, video->frame_number());
  }

  delete decoder;
}
}  // namespace libvpx_test
