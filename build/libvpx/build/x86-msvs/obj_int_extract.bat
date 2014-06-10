REM   Copyright (c) 2011 The WebM project authors. All Rights Reserved.
REM
REM   Use of this source code is governed by a BSD-style license
REM   that can be found in the LICENSE file in the root of the source
REM   tree. An additional intellectual property rights grant can be found
REM   in the file PATENTS.  All contributing project authors may
REM   be found in the AUTHORS file in the root of the source tree.
echo on

REM Arguments:
REM   %1 - Relative path to the directory containing the vp8 source directory.
REM   %2 - Path to obj_int_extract.exe.
cl /I "./" /I "%1" /nologo /c "%1/vp8/encoder/vp8_asm_enc_offsets.c"
%2\obj_int_extract.exe rvds "vp8_asm_enc_offsets.obj" > "vp8_asm_enc_offsets.asm"

