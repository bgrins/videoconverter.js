##
##  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##

#
# This file is to be used for compiling libvpx for Android using the NDK.
# In an Android project place a libvpx checkout in the jni directory.
# Run the configure script from the jni directory.  Base libvpx
# encoder/decoder configuration will look similar to:
# ./libvpx/configure --target=armv7-android-gcc --disable-examples \
#                    --sdk-path=/opt/android-ndk-r6b/
#
# When targeting Android, realtime-only is enabled by default.  This can
# be overridden by adding the command line flag:
#  --disable-realtime-only
#
# This will create .mk files that contain variables that contain the
# source files to compile.
#
# Place an Android.mk file in the jni directory that references the
# Android.mk file in the libvpx directory:
# LOCAL_PATH := $(call my-dir)
# include $(CLEAR_VARS)
# include jni/libvpx/build/make/Android.mk
#
# There are currently two TARGET_ARCH_ABI targets for ARM.
# armeabi and armeabi-v7a.  armeabi-v7a is selected by creating an
# Application.mk in the jni directory that contains:
# APP_ABI := armeabi-v7a
#
# By default libvpx will detect at runtime the existance of NEON extension.
# For this we import the 'cpufeatures' module from the NDK sources.
# libvpx can also be configured without this runtime detection method.
# Configuring with --disable-runtime-cpu-detect will assume presence of NEON.
# Configuring with --disable-runtime-cpu-detect --disable-neon will remove any
# NEON dependency.

# To change to building armeabi, run ./libvpx/configure again, but with
# --target=arm5te-android-gcc and modify the Application.mk file to
# set APP_ABI := armeabi
#
# Running ndk-build will build libvpx and include it in your project.
#

CONFIG_DIR := $(LOCAL_PATH)/
LIBVPX_PATH := $(LOCAL_PATH)/libvpx
ASM_CNV_PATH_LOCAL := $(TARGET_ARCH_ABI)/ads2gas
ASM_CNV_PATH := $(LOCAL_PATH)/$(ASM_CNV_PATH_LOCAL)

# Makefiles created by the libvpx configure process
# This will need to be fixed to handle x86.
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
  include $(CONFIG_DIR)libs-armv7-android-gcc.mk
else
  include $(CONFIG_DIR)libs-armv5te-android-gcc.mk
endif

# Rule that is normally in Makefile created by libvpx
# configure.  Used to filter out source files based on configuration.
enabled=$(filter-out $($(1)-no),$($(1)-yes))

# Override the relative path that is defined by the libvpx
# configure process
SRC_PATH_BARE := $(LIBVPX_PATH)

# Include the list of files to be built
include $(LIBVPX_PATH)/libs.mk

# Want arm, not thumb, optimized
LOCAL_ARM_MODE := arm
LOCAL_CFLAGS := -O3

# -----------------------------------------------------------------------------
# Template  : asm_offsets_template
# Arguments : 1: assembly offsets file to be created
#             2: c file to base assembly offsets on
# Returns   : None
# Usage     : $(eval $(call asm_offsets_template,<asmfile>, <srcfile>
# Rationale : Create offsets at compile time using for structures that are
#             defined in c, but used in assembly functions.
# -----------------------------------------------------------------------------
define asm_offsets_template

_SRC:=$(2)
_OBJ:=$(ASM_CNV_PATH)/$$(notdir $(2)).S

_FLAGS = $$($$(my)CFLAGS) \
          $$(call get-src-file-target-cflags,$(2)) \
          $$(call host-c-includes,$$(LOCAL_C_INCLUDES) $$(CONFIG_DIR)) \
          $$(LOCAL_CFLAGS) \
          $$(NDK_APP_CFLAGS) \
          $$(call host-c-includes,$$($(my)C_INCLUDES)) \
          -DINLINE_ASM \
          -S \

_TEXT = "Compile $$(call get-src-file-text,$(2))"
_CC   = $$(TARGET_CC)

$$(eval $$(call ev-build-file))

$(1) : $$(_OBJ) $(2)
	@mkdir -p $$(dir $$@)
	@grep $(OFFSET_PATTERN) $$< | tr -d '\#' | $(CONFIG_DIR)$(ASM_CONVERSION) > $$@
endef

# Use ads2gas script to convert from RVCT format to GAS format.  This passes
#  puts the processed file under $(ASM_CNV_PATH).  Local clean rule
#  to handle removing these
ifeq ($(CONFIG_VP8_ENCODER), yes)
  ASM_CNV_OFFSETS_DEPEND += $(ASM_CNV_PATH)/vp8_asm_enc_offsets.asm
endif
ifeq ($(HAVE_NEON), yes)
  ASM_CNV_OFFSETS_DEPEND += $(ASM_CNV_PATH)/vpx_scale_asm_offsets.asm
endif

.PRECIOUS: %.asm.s
$(ASM_CNV_PATH)/libvpx/%.asm.s: $(LIBVPX_PATH)/%.asm $(ASM_CNV_OFFSETS_DEPEND)
	@mkdir -p $(dir $@)
	@$(CONFIG_DIR)$(ASM_CONVERSION) <$< > $@

# For building *_rtcd.h, which have rules in libs.mk
TGT_ISA:=$(word 1, $(subst -, ,$(TOOLCHAIN)))
target := libs

LOCAL_SRC_FILES += vpx_config.c

# Remove duplicate entries
CODEC_SRCS_UNIQUE = $(sort $(CODEC_SRCS))

# Pull out C files.  vpx_config.c is in the immediate directory and
# so it does not need libvpx/ prefixed like the rest of the source files.
# The neon files with intrinsics need to have .neon appended so the proper
# flags are applied.
CODEC_SRCS_C = $(filter %.c, $(CODEC_SRCS_UNIQUE))
LOCAL_NEON_SRCS_C = $(filter %_neon.c, $(CODEC_SRCS_C))
LOCAL_CODEC_SRCS_C = $(filter-out vpx_config.c %_neon.c, $(CODEC_SRCS_C))

LOCAL_SRC_FILES += $(foreach file, $(LOCAL_CODEC_SRCS_C), libvpx/$(file))
LOCAL_SRC_FILES += $(foreach file, $(LOCAL_NEON_SRCS_C), libvpx/$(file).neon)

# Pull out assembly files, splitting NEON from the rest.  This is
# done to specify that the NEON assembly files use NEON assembler flags.
CODEC_SRCS_ASM_ALL = $(filter %.asm.s, $(CODEC_SRCS_UNIQUE))
CODEC_SRCS_ASM = $(foreach v, \
                 $(CODEC_SRCS_ASM_ALL), \
                 $(if $(findstring neon,$(v)),,$(v)))
CODEC_SRCS_ASM_ADS2GAS = $(patsubst %.s, \
                         $(ASM_CNV_PATH_LOCAL)/libvpx/%.s, \
                         $(CODEC_SRCS_ASM))
LOCAL_SRC_FILES += $(CODEC_SRCS_ASM_ADS2GAS)

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
  CODEC_SRCS_ASM_NEON = $(foreach v, \
                        $(CODEC_SRCS_ASM_ALL),\
                        $(if $(findstring neon,$(v)),$(v),))
  CODEC_SRCS_ASM_NEON_ADS2GAS = $(patsubst %.s, \
                                $(ASM_CNV_PATH_LOCAL)/libvpx/%.s, \
                                $(CODEC_SRCS_ASM_NEON))
  LOCAL_SRC_FILES += $(patsubst %.s, \
                     %.s.neon, \
                     $(CODEC_SRCS_ASM_NEON_ADS2GAS))
endif

LOCAL_CFLAGS += \
    -DHAVE_CONFIG_H=vpx_config.h \
    -I$(LIBVPX_PATH) \
    -I$(ASM_CNV_PATH)

LOCAL_MODULE := libvpx

LOCAL_LDLIBS := -llog

ifeq ($(CONFIG_RUNTIME_CPU_DETECT),yes)
  LOCAL_STATIC_LIBRARIES := cpufeatures
endif

# Add a dependency to force generation of the RTCD files.
ifeq ($(CONFIG_VP8), yes)
$(foreach file, $(LOCAL_SRC_FILES), $(LOCAL_PATH)/$(file)): vp8_rtcd.h
endif
ifeq ($(CONFIG_VP9), yes)
$(foreach file, $(LOCAL_SRC_FILES), $(LOCAL_PATH)/$(file)): vp9_rtcd.h
endif
$(foreach file, $(LOCAL_SRC_FILES), $(LOCAL_PATH)/$(file)): vpx_scale_rtcd.h

.PHONY: clean
clean:
	@echo "Clean: ads2gas files [$(TARGET_ARCH_ABI)]"
	@$(RM) $(CODEC_SRCS_ASM_ADS2GAS) $(CODEC_SRCS_ASM_NEON_ADS2GAS)
	@$(RM) $(patsubst %.asm, %.*, $(ASM_CNV_OFFSETS_DEPEND))
	@$(RM) -r $(ASM_CNV_PATH)
	@$(RM) $(CLEAN-OBJS)

include $(BUILD_SHARED_LIBRARY)

ifeq ($(HAVE_NEON), yes)
  $(eval $(call asm_offsets_template,\
    $(ASM_CNV_PATH)/vpx_scale_asm_offsets.asm, \
    $(LIBVPX_PATH)/vpx_scale/vpx_scale_asm_offsets.c))
endif

ifeq ($(CONFIG_VP8_ENCODER), yes)
  $(eval $(call asm_offsets_template,\
    $(ASM_CNV_PATH)/vp8_asm_enc_offsets.asm, \
    $(LIBVPX_PATH)/vp8/encoder/vp8_asm_enc_offsets.c))
endif

ifeq ($(CONFIG_RUNTIME_CPU_DETECT),yes)
$(call import-module,cpufeatures)
endif
