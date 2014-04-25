##
##  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##


PORTS_SRCS-yes += vpx_ports.mk

PORTS_SRCS-$(BUILD_LIBVPX) += asm_offsets.h
PORTS_SRCS-$(BUILD_LIBVPX) += mem.h
PORTS_SRCS-$(BUILD_LIBVPX) += vpx_timer.h

ifeq ($(ARCH_X86)$(ARCH_X86_64),yes)
PORTS_SRCS-$(BUILD_LIBVPX) += emms.asm
PORTS_SRCS-$(BUILD_LIBVPX) += x86.h
PORTS_SRCS-$(BUILD_LIBVPX) += x86_abi_support.asm
endif

PORTS_SRCS-$(ARCH_ARM) += arm_cpudetect.c
PORTS_SRCS-$(ARCH_ARM) += arm.h
