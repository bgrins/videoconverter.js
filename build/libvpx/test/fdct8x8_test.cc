/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
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
#include "vp9/common/vp9_entropy.h"
#include "vpx/vpx_integer.h"

extern "C" {
void vp9_idct8x8_64_add_c(const int16_t *input, uint8_t *output, int pitch);
}

using libvpx_test::ACMRandom;

namespace {
typedef void (*fdct_t)(const int16_t *in, int16_t *out, int stride);
typedef void (*idct_t)(const int16_t *in, uint8_t *out, int stride);
typedef void (*fht_t) (const int16_t *in, int16_t *out, int stride,
                       int tx_type);
typedef void (*iht_t) (const int16_t *in, uint8_t *out, int stride,
                       int tx_type);

typedef std::tr1::tuple<fdct_t, idct_t, int> dct_8x8_param_t;
typedef std::tr1::tuple<fht_t, iht_t, int> ht_8x8_param_t;

void fdct8x8_ref(const int16_t *in, int16_t *out, int stride, int tx_type) {
  vp9_fdct8x8_c(in, out, stride);
}

void fht8x8_ref(const int16_t *in, int16_t *out, int stride, int tx_type) {
  vp9_fht8x8_c(in, out, stride, tx_type);
}

class FwdTrans8x8TestBase {
 public:
  virtual ~FwdTrans8x8TestBase() {}

 protected:
  virtual void RunFwdTxfm(int16_t *in, int16_t *out, int stride) = 0;
  virtual void RunInvTxfm(int16_t *out, uint8_t *dst, int stride) = 0;

  void RunSignBiasCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    DECLARE_ALIGNED_ARRAY(16, int16_t, test_input_block, 64);
    DECLARE_ALIGNED_ARRAY(16, int16_t, test_output_block, 64);
    int count_sign_block[64][2];
    const int count_test_block = 100000;

    memset(count_sign_block, 0, sizeof(count_sign_block));

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < 64; ++j)
        test_input_block[j] = rnd.Rand8() - rnd.Rand8();
      REGISTER_STATE_CHECK(
          RunFwdTxfm(test_input_block, test_output_block, pitch_));

      for (int j = 0; j < 64; ++j) {
        if (test_output_block[j] < 0)
          ++count_sign_block[j][0];
        else if (test_output_block[j] > 0)
          ++count_sign_block[j][1];
      }
    }

    for (int j = 0; j < 64; ++j) {
      const int diff = abs(count_sign_block[j][0] - count_sign_block[j][1]);
      const int max_diff = 1125;
      EXPECT_LT(diff, max_diff)
          << "Error: 8x8 FDCT/FHT has a sign bias > "
          << 1. * max_diff / count_test_block * 100 << "%"
          << " for input range [-255, 255] at index " << j
          << " count0: " << count_sign_block[j][0]
          << " count1: " << count_sign_block[j][1]
          << " diff: " << diff;
    }

    memset(count_sign_block, 0, sizeof(count_sign_block));

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-15, 15].
      for (int j = 0; j < 64; ++j)
        test_input_block[j] = (rnd.Rand8() >> 4) - (rnd.Rand8() >> 4);
      REGISTER_STATE_CHECK(
          RunFwdTxfm(test_input_block, test_output_block, pitch_));

      for (int j = 0; j < 64; ++j) {
        if (test_output_block[j] < 0)
          ++count_sign_block[j][0];
        else if (test_output_block[j] > 0)
          ++count_sign_block[j][1];
      }
    }

    for (int j = 0; j < 64; ++j) {
      const int diff = abs(count_sign_block[j][0] - count_sign_block[j][1]);
      const int max_diff = 10000;
      EXPECT_LT(diff, max_diff)
          << "Error: 4x4 FDCT/FHT has a sign bias > "
          << 1. * max_diff / count_test_block * 100 << "%"
          << " for input range [-15, 15] at index " << j
          << " count0: " << count_sign_block[j][0]
          << " count1: " << count_sign_block[j][1]
          << " diff: " << diff;
    }
  }

  void RunRoundTripErrorCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    int max_error = 0;
    int total_error = 0;
    const int count_test_block = 100000;
    DECLARE_ALIGNED_ARRAY(16, int16_t, test_input_block, 64);
    DECLARE_ALIGNED_ARRAY(16, int16_t, test_temp_block, 64);
    DECLARE_ALIGNED_ARRAY(16, uint8_t, dst, 64);
    DECLARE_ALIGNED_ARRAY(16, uint8_t, src, 64);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < 64; ++j) {
        src[j] = rnd.Rand8();
        dst[j] = rnd.Rand8();
        test_input_block[j] = src[j] - dst[j];
      }

      REGISTER_STATE_CHECK(
          RunFwdTxfm(test_input_block, test_temp_block, pitch_));
      for (int j = 0; j < 64; ++j) {
          if (test_temp_block[j] > 0) {
            test_temp_block[j] += 2;
            test_temp_block[j] /= 4;
            test_temp_block[j] *= 4;
          } else {
            test_temp_block[j] -= 2;
            test_temp_block[j] /= 4;
            test_temp_block[j] *= 4;
          }
      }
      REGISTER_STATE_CHECK(
          RunInvTxfm(test_temp_block, dst, pitch_));

      for (int j = 0; j < 64; ++j) {
        const int diff = dst[j] - src[j];
        const int error = diff * diff;
        if (max_error < error)
          max_error = error;
        total_error += error;
      }
    }

    EXPECT_GE(1, max_error)
      << "Error: 8x8 FDCT/IDCT or FHT/IHT has an individual"
      << " roundtrip error > 1";

    EXPECT_GE(count_test_block/5, total_error)
      << "Error: 8x8 FDCT/IDCT or FHT/IHT has average roundtrip "
      << "error > 1/5 per block";
  }

  void RunExtremalCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    int max_error = 0;
    int total_error = 0;
    const int count_test_block = 100000;
    DECLARE_ALIGNED_ARRAY(16, int16_t, test_input_block, 64);
    DECLARE_ALIGNED_ARRAY(16, int16_t, test_temp_block, 64);
    DECLARE_ALIGNED_ARRAY(16, uint8_t, dst, 64);
    DECLARE_ALIGNED_ARRAY(16, uint8_t, src, 64);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < 64; ++j) {
        src[j] = rnd.Rand8() % 2 ? 255 : 0;
        dst[j] = src[j] > 0 ? 0 : 255;
        test_input_block[j] = src[j] - dst[j];
      }

      REGISTER_STATE_CHECK(
          RunFwdTxfm(test_input_block, test_temp_block, pitch_));
      REGISTER_STATE_CHECK(
          RunInvTxfm(test_temp_block, dst, pitch_));

      for (int j = 0; j < 64; ++j) {
        const int diff = dst[j] - src[j];
        const int error = diff * diff;
        if (max_error < error)
          max_error = error;
        total_error += error;
      }

      EXPECT_GE(1, max_error)
          << "Error: Extremal 8x8 FDCT/IDCT or FHT/IHT has"
          << "an individual roundtrip error > 1";

      EXPECT_GE(count_test_block/5, total_error)
          << "Error: Extremal 8x8 FDCT/IDCT or FHT/IHT has average"
          << " roundtrip error > 1/5 per block";
    }
  }

  int pitch_;
  int tx_type_;
  fht_t fwd_txfm_ref;
};

class FwdTrans8x8DCT
    : public FwdTrans8x8TestBase,
      public ::testing::TestWithParam<dct_8x8_param_t> {
 public:
  virtual ~FwdTrans8x8DCT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_  = GET_PARAM(2);
    pitch_    = 8;
    fwd_txfm_ref = fdct8x8_ref;
  }

  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(int16_t *in, int16_t *out, int stride) {
    fwd_txfm_(in, out, stride);
  }
  void RunInvTxfm(int16_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }

  fdct_t fwd_txfm_;
  idct_t inv_txfm_;
};

TEST_P(FwdTrans8x8DCT, SignBiasCheck) {
  RunSignBiasCheck();
}

TEST_P(FwdTrans8x8DCT, RoundTripErrorCheck) {
  RunRoundTripErrorCheck();
}

TEST_P(FwdTrans8x8DCT, ExtremalCheck) {
  RunExtremalCheck();
}

class FwdTrans8x8HT
    : public FwdTrans8x8TestBase,
      public ::testing::TestWithParam<ht_8x8_param_t> {
 public:
  virtual ~FwdTrans8x8HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_  = GET_PARAM(2);
    pitch_    = 8;
    fwd_txfm_ref = fht8x8_ref;
  }

  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(int16_t *in, int16_t *out, int stride) {
    fwd_txfm_(in, out, stride, tx_type_);
  }
  void RunInvTxfm(int16_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride, tx_type_);
  }

  fht_t fwd_txfm_;
  iht_t inv_txfm_;
};

TEST_P(FwdTrans8x8HT, SignBiasCheck) {
  RunSignBiasCheck();
}

TEST_P(FwdTrans8x8HT, RoundTripErrorCheck) {
  RunRoundTripErrorCheck();
}

TEST_P(FwdTrans8x8HT, ExtremalCheck) {
  RunExtremalCheck();
}

using std::tr1::make_tuple;

INSTANTIATE_TEST_CASE_P(
    C, FwdTrans8x8DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct8x8_c, &vp9_idct8x8_64_add_c, 0)));
INSTANTIATE_TEST_CASE_P(
    C, FwdTrans8x8HT,
    ::testing::Values(
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 0),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 1),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 2),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 3)));

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, FwdTrans8x8DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct8x8_c, &vp9_idct8x8_64_add_neon, 0)));
INSTANTIATE_TEST_CASE_P(
    DISABLED_NEON, FwdTrans8x8HT,
    ::testing::Values(
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_neon, 0),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_neon, 1),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_neon, 2),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_neon, 3)));
#endif

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, FwdTrans8x8DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct8x8_sse2, &vp9_idct8x8_64_add_sse2, 0)));
INSTANTIATE_TEST_CASE_P(
    SSE2, FwdTrans8x8HT,
    ::testing::Values(
        make_tuple(&vp9_fht8x8_sse2, &vp9_iht8x8_64_add_sse2, 0),
        make_tuple(&vp9_fht8x8_sse2, &vp9_iht8x8_64_add_sse2, 1),
        make_tuple(&vp9_fht8x8_sse2, &vp9_iht8x8_64_add_sse2, 2),
        make_tuple(&vp9_fht8x8_sse2, &vp9_iht8x8_64_add_sse2, 3)));
#endif
}  // namespace
