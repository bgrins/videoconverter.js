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
void vp9_idct16x16_256_add_c(const int16_t *input, uint8_t *output, int pitch);
}

using libvpx_test::ACMRandom;

namespace {

#ifdef _MSC_VER
static int round(double x) {
  if (x < 0)
    return static_cast<int>(ceil(x - 0.5));
  else
    return static_cast<int>(floor(x + 0.5));
}
#endif

const int kNumCoeffs = 256;
const double PI = 3.1415926535898;
void reference2_16x16_idct_2d(double *input, double *output) {
  double x;
  for (int l = 0; l < 16; ++l) {
    for (int k = 0; k < 16; ++k) {
      double s = 0;
      for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
          x = cos(PI * j * (l + 0.5) / 16.0) *
              cos(PI * i * (k + 0.5) / 16.0) *
              input[i * 16 + j] / 256;
          if (i != 0)
            x *= sqrt(2.0);
          if (j != 0)
            x *= sqrt(2.0);
          s += x;
        }
      }
      output[k*16+l] = s;
    }
  }
}


const double C1 = 0.995184726672197;
const double C2 = 0.98078528040323;
const double C3 = 0.956940335732209;
const double C4 = 0.923879532511287;
const double C5 = 0.881921264348355;
const double C6 = 0.831469612302545;
const double C7 = 0.773010453362737;
const double C8 = 0.707106781186548;
const double C9 = 0.634393284163646;
const double C10 = 0.555570233019602;
const double C11 = 0.471396736825998;
const double C12 = 0.38268343236509;
const double C13 = 0.290284677254462;
const double C14 = 0.195090322016128;
const double C15 = 0.098017140329561;

void butterfly_16x16_dct_1d(double input[16], double output[16]) {
  double step[16];
  double intermediate[16];
  double temp1, temp2;

  // step 1
  step[ 0] = input[0] + input[15];
  step[ 1] = input[1] + input[14];
  step[ 2] = input[2] + input[13];
  step[ 3] = input[3] + input[12];
  step[ 4] = input[4] + input[11];
  step[ 5] = input[5] + input[10];
  step[ 6] = input[6] + input[ 9];
  step[ 7] = input[7] + input[ 8];
  step[ 8] = input[7] - input[ 8];
  step[ 9] = input[6] - input[ 9];
  step[10] = input[5] - input[10];
  step[11] = input[4] - input[11];
  step[12] = input[3] - input[12];
  step[13] = input[2] - input[13];
  step[14] = input[1] - input[14];
  step[15] = input[0] - input[15];

  // step 2
  output[0] = step[0] + step[7];
  output[1] = step[1] + step[6];
  output[2] = step[2] + step[5];
  output[3] = step[3] + step[4];
  output[4] = step[3] - step[4];
  output[5] = step[2] - step[5];
  output[6] = step[1] - step[6];
  output[7] = step[0] - step[7];

  temp1 = step[ 8] * C7;
  temp2 = step[15] * C9;
  output[ 8] = temp1 + temp2;

  temp1 = step[ 9] * C11;
  temp2 = step[14] * C5;
  output[ 9] = temp1 - temp2;

  temp1 = step[10] * C3;
  temp2 = step[13] * C13;
  output[10] = temp1 + temp2;

  temp1 = step[11] * C15;
  temp2 = step[12] * C1;
  output[11] = temp1 - temp2;

  temp1 = step[11] * C1;
  temp2 = step[12] * C15;
  output[12] = temp2 + temp1;

  temp1 = step[10] * C13;
  temp2 = step[13] * C3;
  output[13] = temp2 - temp1;

  temp1 = step[ 9] * C5;
  temp2 = step[14] * C11;
  output[14] = temp2 + temp1;

  temp1 = step[ 8] * C9;
  temp2 = step[15] * C7;
  output[15] = temp2 - temp1;

  // step 3
  step[ 0] = output[0] + output[3];
  step[ 1] = output[1] + output[2];
  step[ 2] = output[1] - output[2];
  step[ 3] = output[0] - output[3];

  temp1 = output[4] * C14;
  temp2 = output[7] * C2;
  step[ 4] = temp1 + temp2;

  temp1 = output[5] * C10;
  temp2 = output[6] * C6;
  step[ 5] = temp1 + temp2;

  temp1 = output[5] * C6;
  temp2 = output[6] * C10;
  step[ 6] = temp2 - temp1;

  temp1 = output[4] * C2;
  temp2 = output[7] * C14;
  step[ 7] = temp2 - temp1;

  step[ 8] = output[ 8] + output[11];
  step[ 9] = output[ 9] + output[10];
  step[10] = output[ 9] - output[10];
  step[11] = output[ 8] - output[11];

  step[12] = output[12] + output[15];
  step[13] = output[13] + output[14];
  step[14] = output[13] - output[14];
  step[15] = output[12] - output[15];

  // step 4
  output[ 0] = (step[ 0] + step[ 1]);
  output[ 8] = (step[ 0] - step[ 1]);

  temp1 = step[2] * C12;
  temp2 = step[3] * C4;
  temp1 = temp1 + temp2;
  output[ 4] = 2*(temp1 * C8);

  temp1 = step[2] * C4;
  temp2 = step[3] * C12;
  temp1 = temp2 - temp1;
  output[12] = 2 * (temp1 * C8);

  output[ 2] = 2 * ((step[4] + step[ 5]) * C8);
  output[14] = 2 * ((step[7] - step[ 6]) * C8);

  temp1 = step[4] - step[5];
  temp2 = step[6] + step[7];
  output[ 6] = (temp1 + temp2);
  output[10] = (temp1 - temp2);

  intermediate[8] = step[8] + step[14];
  intermediate[9] = step[9] + step[15];

  temp1 = intermediate[8] * C12;
  temp2 = intermediate[9] * C4;
  temp1 = temp1 - temp2;
  output[3] = 2 * (temp1 * C8);

  temp1 = intermediate[8] * C4;
  temp2 = intermediate[9] * C12;
  temp1 = temp2 + temp1;
  output[13] = 2 * (temp1 * C8);

  output[ 9] = 2 * ((step[10] + step[11]) * C8);

  intermediate[11] = step[10] - step[11];
  intermediate[12] = step[12] + step[13];
  intermediate[13] = step[12] - step[13];
  intermediate[14] = step[ 8] - step[14];
  intermediate[15] = step[ 9] - step[15];

  output[15] = (intermediate[11] + intermediate[12]);
  output[ 1] = -(intermediate[11] - intermediate[12]);

  output[ 7] = 2 * (intermediate[13] * C8);

  temp1 = intermediate[14] * C12;
  temp2 = intermediate[15] * C4;
  temp1 = temp1 - temp2;
  output[11] = -2 * (temp1 * C8);

  temp1 = intermediate[14] * C4;
  temp2 = intermediate[15] * C12;
  temp1 = temp2 + temp1;
  output[ 5] = 2 * (temp1 * C8);
}

void reference_16x16_dct_2d(int16_t input[256], double output[256]) {
  // First transform columns
  for (int i = 0; i < 16; ++i) {
    double temp_in[16], temp_out[16];
    for (int j = 0; j < 16; ++j)
      temp_in[j] = input[j * 16 + i];
    butterfly_16x16_dct_1d(temp_in, temp_out);
    for (int j = 0; j < 16; ++j)
      output[j * 16 + i] = temp_out[j];
  }
  // Then transform rows
  for (int i = 0; i < 16; ++i) {
    double temp_in[16], temp_out[16];
    for (int j = 0; j < 16; ++j)
      temp_in[j] = output[j + i * 16];
    butterfly_16x16_dct_1d(temp_in, temp_out);
    // Scale by some magic number
    for (int j = 0; j < 16; ++j)
      output[j + i * 16] = temp_out[j]/2;
  }
}

typedef void (*fdct_t)(const int16_t *in, int16_t *out, int stride);
typedef void (*idct_t)(const int16_t *in, uint8_t *out, int stride);
typedef void (*fht_t) (const int16_t *in, int16_t *out, int stride,
                       int tx_type);
typedef void (*iht_t) (const int16_t *in, uint8_t *out, int stride,
                       int tx_type);

typedef std::tr1::tuple<fdct_t, idct_t, int> dct_16x16_param_t;
typedef std::tr1::tuple<fht_t, iht_t, int> ht_16x16_param_t;

void fdct16x16_ref(const int16_t *in, int16_t *out, int stride, int tx_type) {
  vp9_fdct16x16_c(in, out, stride);
}

void fht16x16_ref(const int16_t *in, int16_t *out, int stride, int tx_type) {
  vp9_fht16x16_c(in, out, stride, tx_type);
}

class Trans16x16TestBase {
 public:
  virtual ~Trans16x16TestBase() {}

 protected:
  virtual void RunFwdTxfm(int16_t *in, int16_t *out, int stride) = 0;

  virtual void RunInvTxfm(int16_t *out, uint8_t *dst, int stride) = 0;

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
        << "Error: 16x16 FHT/IHT has an individual round trip error > 1";

    EXPECT_GE(count_test_block , total_error)
        << "Error: 16x16 FHT/IHT has average round trip error > 1 per block";
  }

  void RunCoeffCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 1000;
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
    const int count_test_block = 1000;
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
      double out_r[kNumCoeffs];

      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < kNumCoeffs; ++j) {
        src[j] = rnd.Rand8();
        dst[j] = rnd.Rand8();
        in[j] = src[j] - dst[j];
      }

      reference_16x16_dct_2d(in, out_r);
      for (int j = 0; j < kNumCoeffs; ++j)
        coeff[j] = round(out_r[j]);

      REGISTER_STATE_CHECK(RunInvTxfm(coeff, dst, 16));

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

class Trans16x16DCT
    : public Trans16x16TestBase,
      public ::testing::TestWithParam<dct_16x16_param_t> {
 public:
  virtual ~Trans16x16DCT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_  = GET_PARAM(2);
    pitch_    = 16;
    fwd_txfm_ref = fdct16x16_ref;
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

TEST_P(Trans16x16DCT, AccuracyCheck) {
  RunAccuracyCheck();
}

TEST_P(Trans16x16DCT, CoeffCheck) {
  RunCoeffCheck();
}

TEST_P(Trans16x16DCT, MemCheck) {
  RunMemCheck();
}

TEST_P(Trans16x16DCT, InvAccuracyCheck) {
  RunInvAccuracyCheck();
}

class Trans16x16HT
    : public Trans16x16TestBase,
      public ::testing::TestWithParam<ht_16x16_param_t> {
 public:
  virtual ~Trans16x16HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_  = GET_PARAM(2);
    pitch_    = 16;
    fwd_txfm_ref = fht16x16_ref;
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

TEST_P(Trans16x16HT, AccuracyCheck) {
  RunAccuracyCheck();
}

TEST_P(Trans16x16HT, CoeffCheck) {
  RunCoeffCheck();
}

TEST_P(Trans16x16HT, MemCheck) {
  RunMemCheck();
}

using std::tr1::make_tuple;

INSTANTIATE_TEST_CASE_P(
    C, Trans16x16DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct16x16_c, &vp9_idct16x16_256_add_c, 0)));
INSTANTIATE_TEST_CASE_P(
    C, Trans16x16HT,
    ::testing::Values(
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 0),
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 1),
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 2),
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 3)));

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, Trans16x16DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct16x16_c,
                   &vp9_idct16x16_256_add_neon, 0)));
#endif

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans16x16DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct16x16_sse2,
                   &vp9_idct16x16_256_add_sse2, 0)));
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans16x16HT,
    ::testing::Values(
        make_tuple(&vp9_fht16x16_sse2, &vp9_iht16x16_256_add_sse2, 0),
        make_tuple(&vp9_fht16x16_sse2, &vp9_iht16x16_256_add_sse2, 1),
        make_tuple(&vp9_fht16x16_sse2, &vp9_iht16x16_256_add_sse2, 2),
        make_tuple(&vp9_fht16x16_sse2, &vp9_iht16x16_256_add_sse2, 3)));
#endif
}  // namespace
