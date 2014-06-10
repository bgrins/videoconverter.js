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
##  This file tests vpxdec. To add new tests to this file, do the following:
##    1. Write a shell function (this is your test).
##    2. Add the function to vpxdec_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

VP8_IVF_FILE="${LIBVPX_TEST_DATA_PATH}/vp80-00-comprehensive-001.ivf"
VP9_WEBM_FILE="${LIBVPX_TEST_DATA_PATH}/vp90-2-00-quantizer-00.webm"

# Environment check: Make sure input is available.
vpxdec_verify_environment() {
  if [ ! -e "${VP8_IVF_FILE}" ] || [ ! -e "${VP9_WEBM_FILE}" ]; then
    echo "Libvpx test data must exist in LIBVPX_TEST_DATA_PATH."
    return 1
  fi
}

vpxdec_can_decode_vp8() {
  if [ "$(vpxdec_available)" = "yes" ] && \
     [ "$(vp8_decode_available)" = "yes" ]; then
    echo yes
  fi
}

vpxdec_can_decode_vp9() {
  if [ "$(vpxdec_available)" = "yes" ] && \
     [ "$(vp9_decode_available)" = "yes" ]; then
    echo yes
  fi
}

vpxdec_vp8_ivf() {
  if [ "$(vpxdec_can_decode_vp8)" = "yes" ]; then
    vpxdec "${VP8_IVF_FILE}"
  fi
}

vpxdec_vp8_ivf_pipe_input() {
  if [ "$(vpxdec_can_decode_vp8)" = "yes" ]; then
    vpxdec "${VP8_IVF_FILE}" -
  fi
}

vpxdec_vp9_webm() {
  if [ "$(vpxdec_can_decode_vp9)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    vpxdec "${VP9_WEBM_FILE}"
  fi
}

vpxdec_tests="vpxdec_vp8_ivf
              vpxdec_vp8_ivf_pipe_input
              vpxdec_vp9_webm"

run_tests vpxdec_verify_environment "${vpxdec_tests}"
