/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>
#include <cstdlib>
#include <string>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "test/ivf_video_source.h"
#include "test/md5_helper.h"
#include "test/test_vectors.h"
#include "test/util.h"
#include "test/webm_video_source.h"
#include "vpx_mem/vpx_mem.h"

namespace {

class TestVectorTest : public ::libvpx_test::DecoderTest,
    public ::libvpx_test::CodecTestWithParam<const char*> {
 protected:
  TestVectorTest() : DecoderTest(GET_PARAM(0)), md5_file_(NULL) {}

  virtual ~TestVectorTest() {
    if (md5_file_)
      fclose(md5_file_);
  }

  void OpenMD5File(const std::string& md5_file_name_) {
    md5_file_ = libvpx_test::OpenTestDataFile(md5_file_name_);
    ASSERT_TRUE(md5_file_ != NULL) << "Md5 file open failed. Filename: "
        << md5_file_name_;
  }

  virtual void DecompressedFrameHook(const vpx_image_t& img,
                                     const unsigned int frame_number) {
    ASSERT_TRUE(md5_file_ != NULL);
    char expected_md5[33];
    char junk[128];

    // Read correct md5 checksums.
    const int res = fscanf(md5_file_, "%s  %s", expected_md5, junk);
    ASSERT_NE(res, EOF) << "Read md5 data failed";
    expected_md5[32] = '\0';

    ::libvpx_test::MD5 md5_res;
    md5_res.Add(&img);
    const char *actual_md5 = md5_res.Get();

    // Check md5 match.
    ASSERT_STREQ(expected_md5, actual_md5)
        << "Md5 checksums don't match: frame number = " << frame_number;
  }

 private:
  FILE *md5_file_;
};

// This test runs through the whole set of test vectors, and decodes them.
// The md5 checksums are computed for each frame in the video file. If md5
// checksums match the correct md5 data, then the test is passed. Otherwise,
// the test failed.
TEST_P(TestVectorTest, MD5Match) {
  const std::string filename = GET_PARAM(1);
  libvpx_test::CompressedVideoSource *video = NULL;

  // Open compressed video file.
  if (filename.substr(filename.length() - 3, 3) == "ivf") {
    video = new libvpx_test::IVFVideoSource(filename);
  } else if (filename.substr(filename.length() - 4, 4) == "webm") {
    video = new libvpx_test::WebMVideoSource(filename);
  }
  video->Init();

  // Construct md5 file name.
  const std::string md5_filename = filename + ".md5";
  OpenMD5File(md5_filename);

  // Decode frame, and check the md5 matching.
  ASSERT_NO_FATAL_FAILURE(RunLoop(video));
  delete video;
}

VP8_INSTANTIATE_TEST_CASE(TestVectorTest,
                          ::testing::ValuesIn(libvpx_test::kVP8TestVectors,
                                              libvpx_test::kVP8TestVectors +
                                              libvpx_test::kNumVP8TestVectors));
VP9_INSTANTIATE_TEST_CASE(TestVectorTest,
                          ::testing::ValuesIn(libvpx_test::kVP9TestVectors,
                                              libvpx_test::kVP9TestVectors +
                                              libvpx_test::kNumVP9TestVectors));

}  // namespace
