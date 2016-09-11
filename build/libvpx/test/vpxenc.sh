#!/bin/sh
##
##  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
##  This file tests vpxenc using hantro_collage_w352h288.yuv as input. To add
##  new tests to this file, do the following:
##    1. Write a shell function (this is your test).
##    2. Add the function to vpxenc_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

YUV_RAW_INPUT="${LIBVPX_TEST_DATA_PATH}/hantro_collage_w352h288.yuv"
YUV_RAW_INPUT_WIDTH=352
YUV_RAW_INPUT_HEIGHT=288
TEST_FRAMES=10

# Environment check: Make sure input is available.
vpxenc_verify_environment() {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    echo "The file ${YUV_RAW_INPUT##*/} must exist in LIBVPX_TEST_DATA_PATH."
    return 1
  fi
}

vpxenc_can_encode_vp8() {
  if [ "$(vpxenc_available)" = "yes" ] && \
     [ "$(vp8_encode_available)" = "yes" ]; then
    echo yes
  fi
}

vpxenc_can_encode_vp9() {
  if [ "$(vpxenc_available)" = "yes" ] && \
     [ "$(vp9_encode_available)" = "yes" ]; then
    echo yes
  fi
}

vpxenc_vp8_ivf() {
  if [ "$(vpxenc_can_encode_vp8)" = "yes" ]; then
    vpxenc vp8 ${YUV_RAW_INPUT_WIDTH} ${YUV_RAW_INPUT_HEIGHT} ${TEST_FRAMES} \
        "${YUV_RAW_INPUT}" vp8.ivf
  fi
}

vpxenc_vp8_ivf_pipe_input() {
  if [ "$(vpxenc_can_encode_vp8)" = "yes" ]; then
    vpxenc vp8 ${YUV_RAW_INPUT_WIDTH} ${YUV_RAW_INPUT_HEIGHT} ${TEST_FRAMES} \
        "${YUV_RAW_INPUT}" vp8.ivf -
  fi
}

vpxenc_vp8_webm() {
  if [ "$(vpxenc_can_encode_vp8)" = "yes" ] &&
     [ "$(webm_io_available)" = "yes" ] ; then
    vpxenc vp8 ${YUV_RAW_INPUT_WIDTH} ${YUV_RAW_INPUT_HEIGHT} ${TEST_FRAMES} \
        "${YUV_RAW_INPUT}" vp8.webm
  fi
}

vpxenc_vp9_ivf() {
  if [ "$(vpxenc_can_encode_vp9)" = "yes" ]; then
    vpxenc vp9 ${YUV_RAW_INPUT_WIDTH} ${YUV_RAW_INPUT_HEIGHT} ${TEST_FRAMES} \
        "${YUV_RAW_INPUT}" vp9.ivf
  fi
}

vpxenc_vp9_webm() {
  if [ "$(vpxenc_can_encode_vp9)" = "yes" ] &&
     [ "$(webm_io_available)" = "yes" ] ; then
    vpxenc vp9 ${YUV_RAW_INPUT_WIDTH} ${YUV_RAW_INPUT_HEIGHT} ${TEST_FRAMES} \
        "${YUV_RAW_INPUT}" vp9.webm
  fi
}

DISABLED_vpxenc_vp9_ivf_lossless() {
  if [ "$(vpxenc_can_encode_vp9)" = "yes" ]; then
    vpxenc vp9 ${YUV_RAW_INPUT_WIDTH} ${YUV_RAW_INPUT_HEIGHT} ${TEST_FRAMES} \
        "${YUV_RAW_INPUT}" vp9_lossless.ivf --lossless
  fi
}

vpxenc_tests="vpxenc_vp8_ivf
              vpxenc_vp8_webm
              vpxenc_vp8_ivf_pipe_input
              vpxenc_vp9_ivf
              vpxenc_vp9_webm
              DISABLED_vpxenc_vp9_ivf_lossless"

run_tests vpxenc_verify_environment "${vpxenc_tests}"
