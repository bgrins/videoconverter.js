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
void vp9_idct4x4_16_add_c(const int16_t *input, uint8_t *output, int pitch);
}

using libvpx_test::ACMRandom;

namespace {
const int kNumCoeffs = 16;
typedef void (*fdct_t)(const int16_t *in, int16_t *out, int stride);
typedef void (*idct_t)(const int16_t *in, uint8_t *out, int stride);
typedef void (*fht_t) (const int16_t *in, int16_t *out, int stride,
                       int tx_type);
typedef void (*iht_t) (const int16_t *in, uint8_t *out, int stride,
                       int tx_type);

typedef std::tr1::tuple<fdct_t, idct_t, int> dct_4x4_param_t;
typedef std::tr1::tuple<fht_t, iht_t, int> ht_4x4_param_t;

void fdct4x4_ref(const int16_t *in, int16_t *out, int stride, int tx_type) {
  vp9_fdct4x4_c(in, out, stride);
}

void fht4x4_ref(const int16_t *in, int16_t *out, int stride, int tx_type) {
  vp9_fht4x4_c(in, out, stride, tx_type);
}

class Trans4x4TestBase {
 public:
  virtual ~Trans4x4TestBase() {}

 protected:
  virtual void RunFwdTxfm(const int16_t *in, int16_t *out, int stride) = 0;

  virtual void RunInvTxfm(const int16_t *out, uint8_t *dst, int stride) = 0;

  void RunAccuracyCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    uint32_t max_error = 0;
    int64_t total_error = 0;
    const int count_test_block = 10000;
    for (int i = 0; i < count_test_block; ++i) {
      DECLARE_ALIGNED_ARRAY(16, int16_t, test_input_block, kNumCoeffs);
      DECLARE_ALIGNED_ARRAY(16, int16_t, test_temp_block, kNumCoeffs);
      DECLARE_ALIGNED_ARRAY(16, uint8_t, dst, kNumCoeffs);
      DECLARE_ALIGNED_ARRAY(16, uint8_t, src, kNumCoeffs);

      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < kNumCoeffs; ++j) {
        src[j] = rnd.Rand8();
        dst[j] = rnd.Rand8();
        test_input_block[j] = src[j] - dst[j];
      }

      REGISTER_STATE_CHECK(RunFwdTxfm(test_input_block,
                                      test_temp_block, pitch_));
      REGISTER_STATE_CHECK(RunInvTxfm(test_temp_block, dst, pitch_));

      for (int j = 0; j < kNumCoeffs; ++j) {
        const uint32_t diff = dst[j] - src[j];
        const uint32_t error = diff * diff;
        if (max_error < error)
          max_error = error;
        total_error += error;
      }
    }

    EXPECT_GE(1u, max_error)
        << "Error: 4x4 FHT/IHT has an individual round trip error > 1";

    EXPECT_GE(count_test_block , total_error)
        << "Error: 4x4 FHT/IHT has average round trip error > 1 per block";
  }

  void RunCoeffCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    DECLARE_ALIGNED_ARRAY(16, int16_t, input_block, kNumCoeffs);
    DECLARE_ALIGNED_ARRAY(16, int16_t, output_ref_block, kNumCoeffs);
    DECLARE_ALIGNED_ARRAY(16, int16_t, output_block, kNumCoeffs);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < kNumCoeffs; ++j)
        input_block[j] = rnd.Rand8() - rnd.Rand8();

      fwd_txfm_ref(input_block, output_ref_block, pitch_, tx_type_);
      REGISTER_STATE_CHECK(RunFwdTxfm(input_block, output_block, pitch_));

      // The minimum quant value is 4.
      for (int j = 0; j < kNumCoeffs; ++j)
        EXPECT_EQ(output_block[j], output_ref_block[j]);
    }
  }

  void RunMemCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    DECLARE_ALIGNED_ARRAY(16, int16_t, input_block, kNumCoeffs);
    DECLARE_ALIGNED_ARRAY(16, int16_t, input_extreme_block, kNumCoeffs);
    DECLARE_ALIGNED_ARRAY(16, int16_t, output_ref_block, kNumCoeffs);
    DECLARE_ALIGNED_ARRAY(16, int16_t, output_block, kNumCoeffs);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < kNumCoeffs; ++j) {
        input_block[j] = rnd.Rand8() - rnd.Rand8();
        input_extreme_block[j] = rnd.Rand8() % 2 ? 255 : -255;
      }
      if (i == 0)
        for (int j = 0; j < kNumCoeffs; ++j)
          input_extreme_block[j] = 255;
      if (i == 1)
        for (int j = 0; j < kNumCoeffs; ++j)
          input_extreme_block[j] = -255;

      fwd_txfm_ref(input_extreme_block, output_ref_block, pitch_, tx_type_);
      REGISTER_STATE_CHECK(RunFwdTxfm(input_extreme_block,
                                      output_block, pitch_));

      // The minimum quant value is 4.
      for (int j = 0; j < kNumCoeffs; ++j) {
        EXPECT_EQ(output_block[j], output_ref_block[j]);
        EXPECT_GE(4 * DCT_MAX_VALUE, abs(output_block[j]))
            << "Error: 16x16 FDCT has coefficient larger than 4*DCT_MAX_VALUE";
      }
    }
  }

  void RunInvAccuracyCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 1000;
    DECLARE_ALIGNED_ARRAY(16, int16_t, in, kNumCoeffs);
    DECLARE_ALIGNED_ARRAY(16, int16_t, coeff, kNumCoeffs);
    DECLARE_ALIGNED_ARRAY(16, uint8_t, dst, kNumCoeffs);
    DECLARE_ALIGNED_ARRAY(16, uint8_t, src, kNumCoeffs);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < kNumCoeffs; ++j) {
        src[j] = rnd.Rand8();
        dst[j] = rnd.Rand8();
        in[j] = src[j] - dst[j];
      }

      fwd_txfm_ref(in, coeff, pitch_, tx_type_);

      REGISTER_STATE_CHECK(RunInvTxfm(coeff, dst, pitch_));

      for (int j = 0; j < kNumCoeffs; ++j) {
        const uint32_t diff = dst[j] - src[j];
        const uint32_t error = diff * diff;
        EXPECT_GE(1u, error)
            << "Error: 16x16 IDCT has error " << error
            << " at index " << j;
      }
    }
  }

  int pitch_;
  int tx_type_;
  fht_t fwd_txfm_ref;
};

class Trans4x4DCT
    : public Trans4x4TestBase,
      public ::testing::TestWithParam<dct_4x4_param_t> {
 public:
  virtual ~Trans4x4DCT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_  = GET_PARAM(2);
    pitch_    = 4;
    fwd_txfm_ref = fdct4x4_ref;
  }
  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, int16_t *out, int stride) {
    fwd_txfm_(in, out, stride);
  }
  void RunInvTxfm(const int16_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }

  fdct_t fwd_txfm_;
  idct_t inv_txfm_;
};

TEST_P(Trans4x4DCT, AccuracyCheck) {
  RunAccuracyCheck();
}

TEST_P(Trans4x4DCT, CoeffCheck) {
  RunCoeffCheck();
}

TEST_P(Trans4x4DCT, MemCheck) {
  RunMemCheck();
}

TEST_P(Trans4x4DCT, InvAccuracyCheck) {
  RunInvAccuracyCheck();
}

class Trans4x4HT
    : public Trans4x4TestBase,
      public ::testing::TestWithParam<ht_4x4_param_t> {
 public:
  virtual ~Trans4x4HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_  = GET_PARAM(2);
    pitch_    = 4;
    fwd_txfm_ref = fht4x4_ref;
  }
  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, int16_t *out, int stride) {
    fwd_txfm_(in, out, stride, tx_type_);
  }

  void RunInvTxfm(const int16_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride, tx_type_);
  }

  fht_t fwd_txfm_;
  iht_t inv_txfm_;
};

TEST_P(Trans4x4HT, AccuracyCheck) {
  RunAccuracyCheck();
}

TEST_P(Trans4x4HT, CoeffCheck) {
  RunCoeffCheck();
}

TEST_P(Trans4x4HT, MemCheck) {
  RunMemCheck();
}

TEST_P(Trans4x4HT, InvAccuracyCheck) {
  RunInvAccuracyCheck();
}

using std::tr1::make_tuple;

INSTANTIATE_TEST_CASE_P(
    C, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct4x4_c, &vp9_idct4x4_16_add_c, 0)));
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4HT,
    ::testing::Values(
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 0),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 1),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 2),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 3)));

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct4x4_c,
                   &vp9_idct4x4_16_add_neon, 0)));
INSTANTIATE_TEST_CASE_P(
    DISABLED_NEON, Trans4x4HT,
    ::testing::Values(
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_neon, 0),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_neon, 1),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_neon, 2),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_neon, 3)));
#endif

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct4x4_sse2,
                   &vp9_idct4x4_16_add_sse2, 0)));
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4HT,
    ::testing::Values(
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 0),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 1),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 2),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 3)));
#endif

}  // namespace
