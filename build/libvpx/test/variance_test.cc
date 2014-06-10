/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdlib.h>
#include <new>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "test/clear_system_state.h"
#include "test/register_state_check.h"

#include "vpx/vpx_integer.h"
#include "./vpx_config.h"
#include "vpx_mem/vpx_mem.h"
#if CONFIG_VP8_ENCODER
# include "./vp8_rtcd.h"
# include "vp8/common/variance.h"
#endif
#if CONFIG_VP9_ENCODER
# include "./vp9_rtcd.h"
# include "vp9/encoder/vp9_variance.h"
#endif
#include "test/acm_random.h"

namespace {

using ::std::tr1::get;
using ::std::tr1::make_tuple;
using ::std::tr1::tuple;
using libvpx_test::ACMRandom;

static unsigned int variance_ref(const uint8_t *ref, const uint8_t *src,
                                 int l2w, int l2h, unsigned int *sse_ptr) {
  int se = 0;
  unsigned int sse = 0;
  const int w = 1 << l2w, h = 1 << l2h;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int diff = ref[w * y + x] - src[w * y + x];
      se += diff;
      sse += diff * diff;
    }
  }
  *sse_ptr = sse;
  return sse - (((int64_t) se * se) >> (l2w + l2h));
}

static unsigned int subpel_variance_ref(const uint8_t *ref, const uint8_t *src,
                                        int l2w, int l2h, int xoff, int yoff,
                                        unsigned int *sse_ptr) {
  int se = 0;
  unsigned int sse = 0;
  const int w = 1 << l2w, h = 1 << l2h;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      // bilinear interpolation at a 16th pel step
      const int a1 = ref[(w + 1) * (y + 0) + x + 0];
      const int a2 = ref[(w + 1) * (y + 0) + x + 1];
      const int b1 = ref[(w + 1) * (y + 1) + x + 0];
      const int b2 = ref[(w + 1) * (y + 1) + x + 1];
      const int a = a1 + (((a2 - a1) * xoff + 8) >> 4);
      const int b = b1 + (((b2 - b1) * xoff + 8) >> 4);
      const int r = a + (((b - a) * yoff + 8) >> 4);
      int diff = r - src[w * y + x];
      se += diff;
      sse += diff * diff;
    }
  }
  *sse_ptr = sse;
  return sse - (((int64_t) se * se) >> (l2w + l2h));
}

template<typename VarianceFunctionType>
class VarianceTest
    : public ::testing::TestWithParam<tuple<int, int, VarianceFunctionType> > {
 public:
  virtual void SetUp() {
    const tuple<int, int, VarianceFunctionType>& params = this->GetParam();
    log2width_  = get<0>(params);
    width_ = 1 << log2width_;
    log2height_ = get<1>(params);
    height_ = 1 << log2height_;
    variance_ = get<2>(params);

    rnd(ACMRandom::DeterministicSeed());
    block_size_ = width_ * height_;
    src_ = new uint8_t[block_size_];
    ref_ = new uint8_t[block_size_];
    ASSERT_TRUE(src_ != NULL);
    ASSERT_TRUE(ref_ != NULL);
  }

  virtual void TearDown() {
    delete[] src_;
    delete[] ref_;
    libvpx_test::ClearSystemState();
  }

 protected:
  void ZeroTest();
  void RefTest();
  void OneQuarterTest();

  ACMRandom rnd;
  uint8_t* src_;
  uint8_t* ref_;
  int width_, log2width_;
  int height_, log2height_;
  int block_size_;
  VarianceFunctionType variance_;
};

template<typename VarianceFunctionType>
void VarianceTest<VarianceFunctionType>::ZeroTest() {
  for (int i = 0; i <= 255; ++i) {
    memset(src_, i, block_size_);
    for (int j = 0; j <= 255; ++j) {
      memset(ref_, j, block_size_);
      unsigned int sse;
      unsigned int var;
      REGISTER_STATE_CHECK(var = variance_(src_, width_, ref_, width_, &sse));
      EXPECT_EQ(0u, var) << "src values: " << i << "ref values: " << j;
    }
  }
}

template<typename VarianceFunctionType>
void VarianceTest<VarianceFunctionType>::RefTest() {
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < block_size_; j++) {
      src_[j] = rnd.Rand8();
      ref_[j] = rnd.Rand8();
    }
    unsigned int sse1, sse2;
    unsigned int var1;
    REGISTER_STATE_CHECK(var1 = variance_(src_, width_, ref_, width_, &sse1));
    const unsigned int var2 = variance_ref(src_, ref_, log2width_,
                                           log2height_, &sse2);
    EXPECT_EQ(sse1, sse2);
    EXPECT_EQ(var1, var2);
  }
}

template<typename VarianceFunctionType>
void VarianceTest<VarianceFunctionType>::OneQuarterTest() {
  memset(src_, 255, block_size_);
  const int half = block_size_ / 2;
  memset(ref_, 255, half);
  memset(ref_ + half, 0, half);
  unsigned int sse;
  unsigned int var;
  REGISTER_STATE_CHECK(var = variance_(src_, width_, ref_, width_, &sse));
  const unsigned int expected = block_size_ * 255 * 255 / 4;
  EXPECT_EQ(expected, var);
}

#if CONFIG_VP9_ENCODER

unsigned int subpel_avg_variance_ref(const uint8_t *ref,
                                     const uint8_t *src,
                                     const uint8_t *second_pred,
                                     int l2w, int l2h,
                                     int xoff, int yoff,
                                     unsigned int *sse_ptr) {
  int se = 0;
  unsigned int sse = 0;
  const int w = 1 << l2w, h = 1 << l2h;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      // bilinear interpolation at a 16th pel step
      const int a1 = ref[(w + 1) * (y + 0) + x + 0];
      const int a2 = ref[(w + 1) * (y + 0) + x + 1];
      const int b1 = ref[(w + 1) * (y + 1) + x + 0];
      const int b2 = ref[(w + 1) * (y + 1) + x + 1];
      const int a = a1 + (((a2 - a1) * xoff + 8) >> 4);
      const int b = b1 + (((b2 - b1) * xoff + 8) >> 4);
      const int r = a + (((b - a) * yoff + 8) >> 4);
      int diff = ((r + second_pred[w * y + x] + 1) >> 1) - src[w * y + x];
      se += diff;
      sse += diff * diff;
    }
  }
  *sse_ptr = sse;
  return sse - (((int64_t) se * se) >> (l2w + l2h));
}

template<typename SubpelVarianceFunctionType>
class SubpelVarianceTest
    : public ::testing::TestWithParam<tuple<int, int,
                                            SubpelVarianceFunctionType> > {
 public:
  virtual void SetUp() {
    const tuple<int, int, SubpelVarianceFunctionType>& params =
        this->GetParam();
    log2width_  = get<0>(params);
    width_ = 1 << log2width_;
    log2height_ = get<1>(params);
    height_ = 1 << log2height_;
    subpel_variance_ = get<2>(params);

    rnd(ACMRandom::DeterministicSeed());
    block_size_ = width_ * height_;
    src_ = reinterpret_cast<uint8_t *>(vpx_memalign(16, block_size_));
    sec_ = reinterpret_cast<uint8_t *>(vpx_memalign(16, block_size_));
    ref_ = new uint8_t[block_size_ + width_ + height_ + 1];
    ASSERT_TRUE(src_ != NULL);
    ASSERT_TRUE(sec_ != NULL);
    ASSERT_TRUE(ref_ != NULL);
  }

  virtual void TearDown() {
    vpx_free(src_);
    delete[] ref_;
    vpx_free(sec_);
    libvpx_test::ClearSystemState();
  }

 protected:
  void RefTest();

  ACMRandom rnd;
  uint8_t *src_;
  uint8_t *ref_;
  uint8_t *sec_;
  int width_, log2width_;
  int height_, log2height_;
  int block_size_;
  SubpelVarianceFunctionType subpel_variance_;
};

template<typename SubpelVarianceFunctionType>
void SubpelVarianceTest<SubpelVarianceFunctionType>::RefTest() {
  for (int x = 0; x < 16; ++x) {
    for (int y = 0; y < 16; ++y) {
      for (int j = 0; j < block_size_; j++) {
        src_[j] = rnd.Rand8();
      }
      for (int j = 0; j < block_size_ + width_ + height_ + 1; j++) {
        ref_[j] = rnd.Rand8();
      }
      unsigned int sse1, sse2;
      unsigned int var1;
      REGISTER_STATE_CHECK(var1 = subpel_variance_(ref_, width_ + 1, x, y,
                                                   src_, width_, &sse1));
      const unsigned int var2 = subpel_variance_ref(ref_, src_, log2width_,
                                                    log2height_, x, y, &sse2);
      EXPECT_EQ(sse1, sse2) << "at position " << x << ", " << y;
      EXPECT_EQ(var1, var2) << "at position " << x << ", " << y;
    }
  }
}

template<>
void SubpelVarianceTest<vp9_subp_avg_variance_fn_t>::RefTest() {
  for (int x = 0; x < 16; ++x) {
    for (int y = 0; y < 16; ++y) {
      for (int j = 0; j < block_size_; j++) {
        src_[j] = rnd.Rand8();
        sec_[j] = rnd.Rand8();
      }
      for (int j = 0; j < block_size_ + width_ + height_ + 1; j++) {
        ref_[j] = rnd.Rand8();
      }
      unsigned int sse1, sse2;
      unsigned int var1;
      REGISTER_STATE_CHECK(var1 = subpel_variance_(ref_, width_ + 1, x, y,
                                                   src_, width_, &sse1, sec_));
      const unsigned int var2 = subpel_avg_variance_ref(ref_, src_, sec_,
                                                        log2width_, log2height_,
                                                        x, y, &sse2);
      EXPECT_EQ(sse1, sse2) << "at position " << x << ", " << y;
      EXPECT_EQ(var1, var2) << "at position " << x << ", " << y;
    }
  }
}

#endif  // CONFIG_VP9_ENCODER

// -----------------------------------------------------------------------------
// VP8 test cases.

namespace vp8 {

#if CONFIG_VP8_ENCODER
typedef VarianceTest<vp8_variance_fn_t> VP8VarianceTest;

TEST_P(VP8VarianceTest, Zero) { ZeroTest(); }
TEST_P(VP8VarianceTest, Ref) { RefTest(); }
TEST_P(VP8VarianceTest, OneQuarter) { OneQuarterTest(); }

const vp8_variance_fn_t variance4x4_c = vp8_variance4x4_c;
const vp8_variance_fn_t variance8x8_c = vp8_variance8x8_c;
const vp8_variance_fn_t variance8x16_c = vp8_variance8x16_c;
const vp8_variance_fn_t variance16x8_c = vp8_variance16x8_c;
const vp8_variance_fn_t variance16x16_c = vp8_variance16x16_c;
INSTANTIATE_TEST_CASE_P(
    C, VP8VarianceTest,
    ::testing::Values(make_tuple(2, 2, variance4x4_c),
                      make_tuple(3, 3, variance8x8_c),
                      make_tuple(3, 4, variance8x16_c),
                      make_tuple(4, 3, variance16x8_c),
                      make_tuple(4, 4, variance16x16_c)));

#if HAVE_NEON
const vp8_variance_fn_t variance8x8_neon = vp8_variance8x8_neon;
const vp8_variance_fn_t variance8x16_neon = vp8_variance8x16_neon;
const vp8_variance_fn_t variance16x8_neon = vp8_variance16x8_neon;
const vp8_variance_fn_t variance16x16_neon = vp8_variance16x16_neon;
INSTANTIATE_TEST_CASE_P(
    NEON, VP8VarianceTest,
    ::testing::Values(make_tuple(3, 3, variance8x8_neon),
                      make_tuple(3, 4, variance8x16_neon),
                      make_tuple(4, 3, variance16x8_neon),
                      make_tuple(4, 4, variance16x16_neon)));
#endif

#if HAVE_MMX
const vp8_variance_fn_t variance4x4_mmx = vp8_variance4x4_mmx;
const vp8_variance_fn_t variance8x8_mmx = vp8_variance8x8_mmx;
const vp8_variance_fn_t variance8x16_mmx = vp8_variance8x16_mmx;
const vp8_variance_fn_t variance16x8_mmx = vp8_variance16x8_mmx;
const vp8_variance_fn_t variance16x16_mmx = vp8_variance16x16_mmx;
INSTANTIATE_TEST_CASE_P(
    MMX, VP8VarianceTest,
    ::testing::Values(make_tuple(2, 2, variance4x4_mmx),
                      make_tuple(3, 3, variance8x8_mmx),
                      make_tuple(3, 4, variance8x16_mmx),
                      make_tuple(4, 3, variance16x8_mmx),
                      make_tuple(4, 4, variance16x16_mmx)));
#endif

#if HAVE_SSE2
const vp8_variance_fn_t variance4x4_wmt = vp8_variance4x4_wmt;
const vp8_variance_fn_t variance8x8_wmt = vp8_variance8x8_wmt;
const vp8_variance_fn_t variance8x16_wmt = vp8_variance8x16_wmt;
const vp8_variance_fn_t variance16x8_wmt = vp8_variance16x8_wmt;
const vp8_variance_fn_t variance16x16_wmt = vp8_variance16x16_wmt;
INSTANTIATE_TEST_CASE_P(
    SSE2, VP8VarianceTest,
    ::testing::Values(make_tuple(2, 2, variance4x4_wmt),
                      make_tuple(3, 3, variance8x8_wmt),
                      make_tuple(3, 4, variance8x16_wmt),
                      make_tuple(4, 3, variance16x8_wmt),
                      make_tuple(4, 4, variance16x16_wmt)));
#endif
#endif  // CONFIG_VP8_ENCODER

}  // namespace vp8

// -----------------------------------------------------------------------------
// VP9 test cases.

namespace vp9 {

#if CONFIG_VP9_ENCODER
typedef VarianceTest<vp9_variance_fn_t> VP9VarianceTest;
typedef SubpelVarianceTest<vp9_subpixvariance_fn_t> VP9SubpelVarianceTest;
typedef SubpelVarianceTest<vp9_subp_avg_variance_fn_t> VP9SubpelAvgVarianceTest;

TEST_P(VP9VarianceTest, Zero) { ZeroTest(); }
TEST_P(VP9VarianceTest, Ref) { RefTest(); }
TEST_P(VP9SubpelVarianceTest, Ref) { RefTest(); }
TEST_P(VP9SubpelAvgVarianceTest, Ref) { RefTest(); }
TEST_P(VP9VarianceTest, OneQuarter) { OneQuarterTest(); }

const vp9_variance_fn_t variance4x4_c = vp9_variance4x4_c;
const vp9_variance_fn_t variance4x8_c = vp9_variance4x8_c;
const vp9_variance_fn_t variance8x4_c = vp9_variance8x4_c;
const vp9_variance_fn_t variance8x8_c = vp9_variance8x8_c;
const vp9_variance_fn_t variance8x16_c = vp9_variance8x16_c;
const vp9_variance_fn_t variance16x8_c = vp9_variance16x8_c;
const vp9_variance_fn_t variance16x16_c = vp9_variance16x16_c;
const vp9_variance_fn_t variance16x32_c = vp9_variance16x32_c;
const vp9_variance_fn_t variance32x16_c = vp9_variance32x16_c;
const vp9_variance_fn_t variance32x32_c = vp9_variance32x32_c;
const vp9_variance_fn_t variance32x64_c = vp9_variance32x64_c;
const vp9_variance_fn_t variance64x32_c = vp9_variance64x32_c;
const vp9_variance_fn_t variance64x64_c = vp9_variance64x64_c;
INSTANTIATE_TEST_CASE_P(
    C, VP9VarianceTest,
    ::testing::Values(make_tuple(2, 2, variance4x4_c),
                      make_tuple(2, 3, variance4x8_c),
                      make_tuple(3, 2, variance8x4_c),
                      make_tuple(3, 3, variance8x8_c),
                      make_tuple(3, 4, variance8x16_c),
                      make_tuple(4, 3, variance16x8_c),
                      make_tuple(4, 4, variance16x16_c),
                      make_tuple(4, 5, variance16x32_c),
                      make_tuple(5, 4, variance32x16_c),
                      make_tuple(5, 5, variance32x32_c),
                      make_tuple(5, 6, variance32x64_c),
                      make_tuple(6, 5, variance64x32_c),
                      make_tuple(6, 6, variance64x64_c)));

const vp9_subpixvariance_fn_t subpel_variance4x4_c =
    vp9_sub_pixel_variance4x4_c;
const vp9_subpixvariance_fn_t subpel_variance4x8_c =
    vp9_sub_pixel_variance4x8_c;
const vp9_subpixvariance_fn_t subpel_variance8x4_c =
    vp9_sub_pixel_variance8x4_c;
const vp9_subpixvariance_fn_t subpel_variance8x8_c =
    vp9_sub_pixel_variance8x8_c;
const vp9_subpixvariance_fn_t subpel_variance8x16_c =
    vp9_sub_pixel_variance8x16_c;
const vp9_subpixvariance_fn_t subpel_variance16x8_c =
    vp9_sub_pixel_variance16x8_c;
const vp9_subpixvariance_fn_t subpel_variance16x16_c =
    vp9_sub_pixel_variance16x16_c;
const vp9_subpixvariance_fn_t subpel_variance16x32_c =
    vp9_sub_pixel_variance16x32_c;
const vp9_subpixvariance_fn_t subpel_variance32x16_c =
    vp9_sub_pixel_variance32x16_c;
const vp9_subpixvariance_fn_t subpel_variance32x32_c =
    vp9_sub_pixel_variance32x32_c;
const vp9_subpixvariance_fn_t subpel_variance32x64_c =
    vp9_sub_pixel_variance32x64_c;
const vp9_subpixvariance_fn_t subpel_variance64x32_c =
    vp9_sub_pixel_variance64x32_c;
const vp9_subpixvariance_fn_t subpel_variance64x64_c =
    vp9_sub_pixel_variance64x64_c;
INSTANTIATE_TEST_CASE_P(
    C, VP9SubpelVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_variance4x4_c),
                      make_tuple(2, 3, subpel_variance4x8_c),
                      make_tuple(3, 2, subpel_variance8x4_c),
                      make_tuple(3, 3, subpel_variance8x8_c),
                      make_tuple(3, 4, subpel_variance8x16_c),
                      make_tuple(4, 3, subpel_variance16x8_c),
                      make_tuple(4, 4, subpel_variance16x16_c),
                      make_tuple(4, 5, subpel_variance16x32_c),
                      make_tuple(5, 4, subpel_variance32x16_c),
                      make_tuple(5, 5, subpel_variance32x32_c),
                      make_tuple(5, 6, subpel_variance32x64_c),
                      make_tuple(6, 5, subpel_variance64x32_c),
                      make_tuple(6, 6, subpel_variance64x64_c)));

const vp9_subp_avg_variance_fn_t subpel_avg_variance4x4_c =
    vp9_sub_pixel_avg_variance4x4_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance4x8_c =
    vp9_sub_pixel_avg_variance4x8_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x4_c =
    vp9_sub_pixel_avg_variance8x4_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x8_c =
    vp9_sub_pixel_avg_variance8x8_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x16_c =
    vp9_sub_pixel_avg_variance8x16_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x8_c =
    vp9_sub_pixel_avg_variance16x8_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x16_c =
    vp9_sub_pixel_avg_variance16x16_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x32_c =
    vp9_sub_pixel_avg_variance16x32_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x16_c =
    vp9_sub_pixel_avg_variance32x16_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x32_c =
    vp9_sub_pixel_avg_variance32x32_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x64_c =
    vp9_sub_pixel_avg_variance32x64_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x32_c =
    vp9_sub_pixel_avg_variance64x32_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x64_c =
    vp9_sub_pixel_avg_variance64x64_c;
INSTANTIATE_TEST_CASE_P(
    C, VP9SubpelAvgVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_avg_variance4x4_c),
                      make_tuple(2, 3, subpel_avg_variance4x8_c),
                      make_tuple(3, 2, subpel_avg_variance8x4_c),
                      make_tuple(3, 3, subpel_avg_variance8x8_c),
                      make_tuple(3, 4, subpel_avg_variance8x16_c),
                      make_tuple(4, 3, subpel_avg_variance16x8_c),
                      make_tuple(4, 4, subpel_avg_variance16x16_c),
                      make_tuple(4, 5, subpel_avg_variance16x32_c),
                      make_tuple(5, 4, subpel_avg_variance32x16_c),
                      make_tuple(5, 5, subpel_avg_variance32x32_c),
                      make_tuple(5, 6, subpel_avg_variance32x64_c),
                      make_tuple(6, 5, subpel_avg_variance64x32_c),
                      make_tuple(6, 6, subpel_avg_variance64x64_c)));

#if HAVE_MMX
const vp9_variance_fn_t variance4x4_mmx = vp9_variance4x4_mmx;
const vp9_variance_fn_t variance8x8_mmx = vp9_variance8x8_mmx;
const vp9_variance_fn_t variance8x16_mmx = vp9_variance8x16_mmx;
const vp9_variance_fn_t variance16x8_mmx = vp9_variance16x8_mmx;
const vp9_variance_fn_t variance16x16_mmx = vp9_variance16x16_mmx;
INSTANTIATE_TEST_CASE_P(
    MMX, VP9VarianceTest,
    ::testing::Values(make_tuple(2, 2, variance4x4_mmx),
                      make_tuple(3, 3, variance8x8_mmx),
                      make_tuple(3, 4, variance8x16_mmx),
                      make_tuple(4, 3, variance16x8_mmx),
                      make_tuple(4, 4, variance16x16_mmx)));
#endif

#if HAVE_SSE2
#if CONFIG_USE_X86INC
const vp9_variance_fn_t variance4x4_sse2 = vp9_variance4x4_sse2;
const vp9_variance_fn_t variance4x8_sse2 = vp9_variance4x8_sse2;
const vp9_variance_fn_t variance8x4_sse2 = vp9_variance8x4_sse2;
const vp9_variance_fn_t variance8x8_sse2 = vp9_variance8x8_sse2;
const vp9_variance_fn_t variance8x16_sse2 = vp9_variance8x16_sse2;
const vp9_variance_fn_t variance16x8_sse2 = vp9_variance16x8_sse2;
const vp9_variance_fn_t variance16x16_sse2 = vp9_variance16x16_sse2;
const vp9_variance_fn_t variance16x32_sse2 = vp9_variance16x32_sse2;
const vp9_variance_fn_t variance32x16_sse2 = vp9_variance32x16_sse2;
const vp9_variance_fn_t variance32x32_sse2 = vp9_variance32x32_sse2;
const vp9_variance_fn_t variance32x64_sse2 = vp9_variance32x64_sse2;
const vp9_variance_fn_t variance64x32_sse2 = vp9_variance64x32_sse2;
const vp9_variance_fn_t variance64x64_sse2 = vp9_variance64x64_sse2;
INSTANTIATE_TEST_CASE_P(
    SSE2, VP9VarianceTest,
    ::testing::Values(make_tuple(2, 2, variance4x4_sse2),
                      make_tuple(2, 3, variance4x8_sse2),
                      make_tuple(3, 2, variance8x4_sse2),
                      make_tuple(3, 3, variance8x8_sse2),
                      make_tuple(3, 4, variance8x16_sse2),
                      make_tuple(4, 3, variance16x8_sse2),
                      make_tuple(4, 4, variance16x16_sse2),
                      make_tuple(4, 5, variance16x32_sse2),
                      make_tuple(5, 4, variance32x16_sse2),
                      make_tuple(5, 5, variance32x32_sse2),
                      make_tuple(5, 6, variance32x64_sse2),
                      make_tuple(6, 5, variance64x32_sse2),
                      make_tuple(6, 6, variance64x64_sse2)));

const vp9_subpixvariance_fn_t subpel_variance4x4_sse =
    vp9_sub_pixel_variance4x4_sse;
const vp9_subpixvariance_fn_t subpel_variance4x8_sse =
    vp9_sub_pixel_variance4x8_sse;
const vp9_subpixvariance_fn_t subpel_variance8x4_sse2 =
    vp9_sub_pixel_variance8x4_sse2;
const vp9_subpixvariance_fn_t subpel_variance8x8_sse2 =
    vp9_sub_pixel_variance8x8_sse2;
const vp9_subpixvariance_fn_t subpel_variance8x16_sse2 =
    vp9_sub_pixel_variance8x16_sse2;
const vp9_subpixvariance_fn_t subpel_variance16x8_sse2 =
    vp9_sub_pixel_variance16x8_sse2;
const vp9_subpixvariance_fn_t subpel_variance16x16_sse2 =
    vp9_sub_pixel_variance16x16_sse2;
const vp9_subpixvariance_fn_t subpel_variance16x32_sse2 =
    vp9_sub_pixel_variance16x32_sse2;
const vp9_subpixvariance_fn_t subpel_variance32x16_sse2 =
    vp9_sub_pixel_variance32x16_sse2;
const vp9_subpixvariance_fn_t subpel_variance32x32_sse2 =
    vp9_sub_pixel_variance32x32_sse2;
const vp9_subpixvariance_fn_t subpel_variance32x64_sse2 =
    vp9_sub_pixel_variance32x64_sse2;
const vp9_subpixvariance_fn_t subpel_variance64x32_sse2 =
    vp9_sub_pixel_variance64x32_sse2;
const vp9_subpixvariance_fn_t subpel_variance64x64_sse2 =
    vp9_sub_pixel_variance64x64_sse2;
INSTANTIATE_TEST_CASE_P(
    SSE2, VP9SubpelVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_variance4x4_sse),
                      make_tuple(2, 3, subpel_variance4x8_sse),
                      make_tuple(3, 2, subpel_variance8x4_sse2),
                      make_tuple(3, 3, subpel_variance8x8_sse2),
                      make_tuple(3, 4, subpel_variance8x16_sse2),
                      make_tuple(4, 3, subpel_variance16x8_sse2),
                      make_tuple(4, 4, subpel_variance16x16_sse2),
                      make_tuple(4, 5, subpel_variance16x32_sse2),
                      make_tuple(5, 4, subpel_variance32x16_sse2),
                      make_tuple(5, 5, subpel_variance32x32_sse2),
                      make_tuple(5, 6, subpel_variance32x64_sse2),
                      make_tuple(6, 5, subpel_variance64x32_sse2),
                      make_tuple(6, 6, subpel_variance64x64_sse2)));

const vp9_subp_avg_variance_fn_t subpel_avg_variance4x4_sse =
    vp9_sub_pixel_avg_variance4x4_sse;
const vp9_subp_avg_variance_fn_t subpel_avg_variance4x8_sse =
    vp9_sub_pixel_avg_variance4x8_sse;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x4_sse2 =
    vp9_sub_pixel_avg_variance8x4_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x8_sse2 =
    vp9_sub_pixel_avg_variance8x8_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x16_sse2 =
    vp9_sub_pixel_avg_variance8x16_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x8_sse2 =
    vp9_sub_pixel_avg_variance16x8_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x16_sse2 =
    vp9_sub_pixel_avg_variance16x16_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x32_sse2 =
    vp9_sub_pixel_avg_variance16x32_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x16_sse2 =
    vp9_sub_pixel_avg_variance32x16_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x32_sse2 =
    vp9_sub_pixel_avg_variance32x32_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x64_sse2 =
    vp9_sub_pixel_avg_variance32x64_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x32_sse2 =
    vp9_sub_pixel_avg_variance64x32_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x64_sse2 =
    vp9_sub_pixel_avg_variance64x64_sse2;
INSTANTIATE_TEST_CASE_P(
    SSE2, VP9SubpelAvgVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_avg_variance4x4_sse),
                      make_tuple(2, 3, subpel_avg_variance4x8_sse),
                      make_tuple(3, 2, subpel_avg_variance8x4_sse2),
                      make_tuple(3, 3, subpel_avg_variance8x8_sse2),
                      make_tuple(3, 4, subpel_avg_variance8x16_sse2),
                      make_tuple(4, 3, subpel_avg_variance16x8_sse2),
                      make_tuple(4, 4, subpel_avg_variance16x16_sse2),
                      make_tuple(4, 5, subpel_avg_variance16x32_sse2),
                      make_tuple(5, 4, subpel_avg_variance32x16_sse2),
                      make_tuple(5, 5, subpel_avg_variance32x32_sse2),
                      make_tuple(5, 6, subpel_avg_variance32x64_sse2),
                      make_tuple(6, 5, subpel_avg_variance64x32_sse2),
                      make_tuple(6, 6, subpel_avg_variance64x64_sse2)));
#endif
#endif

#if HAVE_SSSE3
#if CONFIG_USE_X86INC

const vp9_subpixvariance_fn_t subpel_variance4x4_ssse3 =
    vp9_sub_pixel_variance4x4_ssse3;
const vp9_subpixvariance_fn_t subpel_variance4x8_ssse3 =
    vp9_sub_pixel_variance4x8_ssse3;
const vp9_subpixvariance_fn_t subpel_variance8x4_ssse3 =
    vp9_sub_pixel_variance8x4_ssse3;
const vp9_subpixvariance_fn_t subpel_variance8x8_ssse3 =
    vp9_sub_pixel_variance8x8_ssse3;
const vp9_subpixvariance_fn_t subpel_variance8x16_ssse3 =
    vp9_sub_pixel_variance8x16_ssse3;
const vp9_subpixvariance_fn_t subpel_variance16x8_ssse3 =
    vp9_sub_pixel_variance16x8_ssse3;
const vp9_subpixvariance_fn_t subpel_variance16x16_ssse3 =
    vp9_sub_pixel_variance16x16_ssse3;
const vp9_subpixvariance_fn_t subpel_variance16x32_ssse3 =
    vp9_sub_pixel_variance16x32_ssse3;
const vp9_subpixvariance_fn_t subpel_variance32x16_ssse3 =
    vp9_sub_pixel_variance32x16_ssse3;
const vp9_subpixvariance_fn_t subpel_variance32x32_ssse3 =
    vp9_sub_pixel_variance32x32_ssse3;
const vp9_subpixvariance_fn_t subpel_variance32x64_ssse3 =
    vp9_sub_pixel_variance32x64_ssse3;
const vp9_subpixvariance_fn_t subpel_variance64x32_ssse3 =
    vp9_sub_pixel_variance64x32_ssse3;
const vp9_subpixvariance_fn_t subpel_variance64x64_ssse3 =
    vp9_sub_pixel_variance64x64_ssse3;
INSTANTIATE_TEST_CASE_P(
    SSSE3, VP9SubpelVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_variance4x4_ssse3),
                      make_tuple(2, 3, subpel_variance4x8_ssse3),
                      make_tuple(3, 2, subpel_variance8x4_ssse3),
                      make_tuple(3, 3, subpel_variance8x8_ssse3),
                      make_tuple(3, 4, subpel_variance8x16_ssse3),
                      make_tuple(4, 3, subpel_variance16x8_ssse3),
                      make_tuple(4, 4, subpel_variance16x16_ssse3),
                      make_tuple(4, 5, subpel_variance16x32_ssse3),
                      make_tuple(5, 4, subpel_variance32x16_ssse3),
                      make_tuple(5, 5, subpel_variance32x32_ssse3),
                      make_tuple(5, 6, subpel_variance32x64_ssse3),
                      make_tuple(6, 5, subpel_variance64x32_ssse3),
                      make_tuple(6, 6, subpel_variance64x64_ssse3)));

const vp9_subp_avg_variance_fn_t subpel_avg_variance4x4_ssse3 =
    vp9_sub_pixel_avg_variance4x4_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance4x8_ssse3 =
    vp9_sub_pixel_avg_variance4x8_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x4_ssse3 =
    vp9_sub_pixel_avg_variance8x4_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x8_ssse3 =
    vp9_sub_pixel_avg_variance8x8_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x16_ssse3 =
    vp9_sub_pixel_avg_variance8x16_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x8_ssse3 =
    vp9_sub_pixel_avg_variance16x8_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x16_ssse3 =
    vp9_sub_pixel_avg_variance16x16_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x32_ssse3 =
    vp9_sub_pixel_avg_variance16x32_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x16_ssse3 =
    vp9_sub_pixel_avg_variance32x16_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x32_ssse3 =
    vp9_sub_pixel_avg_variance32x32_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x64_ssse3 =
    vp9_sub_pixel_avg_variance32x64_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x32_ssse3 =
    vp9_sub_pixel_avg_variance64x32_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x64_ssse3 =
    vp9_sub_pixel_avg_variance64x64_ssse3;
INSTANTIATE_TEST_CASE_P(
    SSSE3, VP9SubpelAvgVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_avg_variance4x4_ssse3),
                      make_tuple(2, 3, subpel_avg_variance4x8_ssse3),
                      make_tuple(3, 2, subpel_avg_variance8x4_ssse3),
                      make_tuple(3, 3, subpel_avg_variance8x8_ssse3),
                      make_tuple(3, 4, subpel_avg_variance8x16_ssse3),
                      make_tuple(4, 3, subpel_avg_variance16x8_ssse3),
                      make_tuple(4, 4, subpel_avg_variance16x16_ssse3),
                      make_tuple(4, 5, subpel_avg_variance16x32_ssse3),
                      make_tuple(5, 4, subpel_avg_variance32x16_ssse3),
                      make_tuple(5, 5, subpel_avg_variance32x32_ssse3),
                      make_tuple(5, 6, subpel_avg_variance32x64_ssse3),
                      make_tuple(6, 5, subpel_avg_variance64x32_ssse3),
                      make_tuple(6, 6, subpel_avg_variance64x64_ssse3)));
#endif
#endif
#endif  // CONFIG_VP9_ENCODER

}  // namespace vp9

}  // namespace
