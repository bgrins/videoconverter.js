/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_I420_VIDEO_SOURCE_H_
#define TEST_I420_VIDEO_SOURCE_H_
#include <cstdio>
#include <cstdlib>
#include <string>

#include "test/video_source.h"

namespace libvpx_test {

// This class extends VideoSource to allow parsing of raw yv12
// so that we can do actual file encodes.
class I420VideoSource : public VideoSource {
 public:
  I420VideoSource(const std::string &file_name,
                  unsigned int width, unsigned int height,
                  int rate_numerator, int rate_denominator,
                  unsigned int start, int limit)
      : file_name_(file_name),
        input_file_(NULL),
        img_(NULL),
        start_(start),
        limit_(limit),
        frame_(0),
        width_(0),
        height_(0),
        framerate_numerator_(rate_numerator),
        framerate_denominator_(rate_denominator) {
    // This initializes raw_sz_, width_, height_ and allocates an img.
    SetSize(width, height);
  }

  virtual ~I420VideoSource() {
    vpx_img_free(img_);
    if (input_file_)
      fclose(input_file_);
  }

  virtual void Begin() {
    if (input_file_)
      fclose(input_file_);
    input_file_ = OpenTestDataFile(file_name_);
    ASSERT_TRUE(input_file_ != NULL) << "Input file open failed. Filename: "
        << file_name_;
    if (start_) {
      fseek(input_file_, static_cast<unsigned>(raw_sz_) * start_, SEEK_SET);
    }

    frame_ = start_;
    FillFrame();
  }

  virtual void Next() {
    ++frame_;
    FillFrame();
  }

  virtual vpx_image_t *img() const { return (frame_ < limit_) ? img_ : NULL;  }

  // Models a stream where Timebase = 1/FPS, so pts == frame.
  virtual vpx_codec_pts_t pts() const { return frame_; }

  virtual unsigned long duration() const { return 1; }

  virtual vpx_rational_t timebase() const {
    const vpx_rational_t t = { framerate_denominator_, framerate_numerator_ };
    return t;
  }

  virtual unsigned int frame() const { return frame_; }

  virtual unsigned int limit() const { return limit_; }

  void SetSize(unsigned int width, unsigned int height) {
    if (width != width_ || height != height_) {
      vpx_img_free(img_);
      img_ = vpx_img_alloc(NULL, VPX_IMG_FMT_I420, width, height, 1);
      ASSERT_TRUE(img_ != NULL);
      width_ = width;
      height_ = height;
      raw_sz_ = width * height * 3 / 2;
    }
  }

  virtual void FillFrame() {
    ASSERT_TRUE(input_file_ != NULL);
    // Read a frame from input_file.
    if (fread(img_->img_data, raw_sz_, 1, input_file_) == 0) {
      limit_ = frame_;
    }
  }

 protected:
  std::string file_name_;
  FILE *input_file_;
  vpx_image_t *img_;
  size_t raw_sz_;
  unsigned int start_;
  unsigned int limit_;
  unsigned int frame_;
  unsigned int width_;
  unsigned int height_;
  int framerate_numerator_;
  int framerate_denominator_;
};

}  // namespace libvpx_test

#endif  // TEST_I420_VIDEO_SOURCE_H_
