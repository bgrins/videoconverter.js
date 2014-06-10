/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

#include "./vp9_rtcd.h"
#include "vp9/common/vp9_blockd.h"
#include "vp9/common/vp9_scan.h"
#include "vpx/vpx_integer.h"

using libvpx_test::ACMRandom;

namespace {
typedef void (*fwd_txfm_t)(const int16_t *in, int16_t *out, int stride);
typedef void (*inv_txfm_t)(const int16_t *in, uint8_t *out, int stride);
typedef std::tr1::tuple<inv_txfm_t,
                        inv_txfm_t,
                        TX_SIZE, int> partial_itxfm_param_t;
const int kMaxNumCoeffs = 1024;
class PartialIDctTest : public ::testing::TestWithParam<partial_itxfm_param_t> {
 public:
  virtual ~PartialIDctTest() {}
  virtual void SetUp() {
    full_itxfm_ = GET_PARAM(0);
    partial_itxfm_ = GET_PARAM(1);
    tx_size_  = GET_PARAM(2);
    last_nonzero_ = GET_PARAM(3);
  }

  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  int last_nonzero_;
  TX_SIZE tx_size_;
  inv_txfm_t full_itxfm_;
  inv_txfm_t partial_itxfm_;
};

TEST_P(PartialIDctTest, ResultsMatch) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  int size;
  switch (tx_size_) {
    case TX_4X4:
      size = 4;
      break;
    case TX_8X8:
      size = 8;
      break;
    case TX_16X16:
      size = 16;
      break;
    case TX_32X32:
      size = 32;
      break;
    default:
      FAIL() << "Wrong Size!";
      break;
  }
  DECLARE_ALIGNED_ARRAY(16, int16_t, test_coef_block1, kMaxNumCoeffs);
  DECLARE_ALIGNED_ARRAY(16, int16_t, test_coef_block2, kMaxNumCoeffs);
  DECLARE_ALIGNED_ARRAY(16, uint8_t, dst1, kMaxNumCoeffs);
  DECLARE_ALIGNED_ARRAY(16, uint8_t, dst2, kMaxNumCoeffs);
  const int count_test_block = 1000;
  const int max_coeff = 32766 / 4;
  const int block_size = size * size;
  int max_error = 0;
  for (int i = 0; i < count_test_block; ++i) {
    // clear out destination buffer
    memset(dst1, 0, sizeof(*dst1) * block_size);
    memset(dst2, 0, sizeof(*dst2) * block_size);
    memset(test_coef_block1, 0, sizeof(*test_coef_block1) * block_size);
    memset(test_coef_block2, 0, sizeof(*test_coef_block2) * block_size);
    int max_energy_leftover = max_coeff * max_coeff;
    for (int j = 0; j < last_nonzero_; ++j) {
      int16_t coef = static_cast<int16_t>(sqrt(1.0 * max_energy_leftover) *
                                          (rnd.Rand16() - 32768) / 65536);
      max_energy_leftover -= coef * coef;
      if (max_energy_leftover < 0) {
        max_energy_leftover = 0;
        coef = 0;
      }
      test_coef_block1[vp9_default_scan_orders[tx_size_].scan[j]] = coef;
    }

    memcpy(test_coef_block2, test_coef_block1,
           sizeof(*test_coef_block2) * block_size);

    REGISTER_STATE_CHECK(full_itxfm_(test_coef_block1, dst1, size));
    REGISTER_STATE_CHECK(partial_itxfm_(test_coef_block2, dst2, size));

    for (int j = 0; j < block_size; ++j) {
      const int diff = dst1[j] - dst2[j];
      const int error = diff * diff;
      if (max_error < error)
        max_error = error;
    }
  }

  EXPECT_EQ(0, max_error)
      << "Error: partial inverse transform produces different results";
}
using std::tr1::make_tuple;

INSTANTIATE_TEST_CASE_P(
    C, PartialIDctTest,
    ::testing::Values(
        make_tuple(&vp9_idct32x32_1024_add_c,
                   &vp9_idct32x32_34_add_c,
                   TX_32X32, 34),
        make_tuple(&vp9_idct32x32_1024_add_c,
                   &vp9_idct32x32_1_add_c,
                   TX_32X32, 1),
        make_tuple(&vp9_idct16x16_256_add_c,
                   &vp9_idct16x16_10_add_c,
                   TX_16X16, 10),
        make_tuple(&vp9_idct16x16_256_add_c,
                   &vp9_idct16x16_1_add_c,
                   TX_16X16, 1),
        make_tuple(&vp9_idct8x8_64_add_c,
                   &vp9_idct8x8_10_add_c,
                   TX_8X8, 10),
        make_tuple(&vp9_idct8x8_64_add_c,
                   &vp9_idct8x8_1_add_c,
                   TX_8X8, 1),
        make_tuple(&vp9_idct4x4_16_add_c,
                   &vp9_idct4x4_1_add_c,
                   TX_4X4, 1)));
#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, PartialIDctTest,
    ::testing::Values(
        make_tuple(&vp9_idct32x32_1024_add_c,
                   &vp9_idct32x32_1_add_neon,
                   TX_32X32, 1),
        make_tuple(&vp9_idct16x16_256_add_c,
                   &vp9_idct16x16_10_add_neon,
                   TX_16X16, 10),
        make_tuple(&vp9_idct16x16_256_add_c,
                   &vp9_idct16x16_1_add_neon,
                   TX_16X16, 1),
        make_tuple(&vp9_idct8x8_64_add_c,
                   &vp9_idct8x8_10_add_neon,
                   TX_8X8, 10),
        make_tuple(&vp9_idct8x8_64_add_c,
                   &vp9_idct8x8_1_add_neon,
                   TX_8X8, 1),
        make_tuple(&vp9_idct4x4_16_add_c,
                   &vp9_idct4x4_1_add_neon,
                   TX_4X4, 1)));
#endif

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, PartialIDctTest,
    ::testing::Values(
        make_tuple(&vp9_idct32x32_1024_add_c,
                   &vp9_idct32x32_34_add_sse2,
                   TX_32X32, 34),
        make_tuple(&vp9_idct32x32_1024_add_c,
                   &vp9_idct32x32_1_add_sse2,
                   TX_32X32, 1),
        make_tuple(&vp9_idct16x16_256_add_c,
                   &vp9_idct16x16_10_add_sse2,
                   TX_16X16, 10),
        make_tuple(&vp9_idct16x16_256_add_c,
                   &vp9_idct16x16_1_add_sse2,
                   TX_16X16, 1),
        make_tuple(&vp9_idct8x8_64_add_c,
                   &vp9_idct8x8_10_add_sse2,
                   TX_8X8, 10),
        make_tuple(&vp9_idct8x8_64_add_c,
                   &vp9_idct8x8_1_add_sse2,
                   TX_8X8, 1),
        make_tuple(&vp9_idct4x4_16_add_c,
                   &vp9_idct4x4_1_add_sse2,
                   TX_4X4, 1)));
#endif
}  // namespace
