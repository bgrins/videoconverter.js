##
##  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##


# ARM assembly files are written in RVCT-style. We use some make magic to
# filter those files to allow GCC compilation
ifeq ($(ARCH_ARM),yes)
  ASM:=$(if $(filter yes,$(CONFIG_GCC)$(CONFIG_MSVS)),.asm.s,.asm)
else
  ASM:=.asm
endif

#
# Calculate platform- and compiler-specific offsets for hand coded assembly
#
ifeq ($(filter icc gcc,$(TGT_CC)), $(TGT_CC))
OFFSET_PATTERN:='^[a-zA-Z0-9_]* EQU'
define asm_offsets_template
$$(BUILD_PFX)$(1): $$(BUILD_PFX)$(2).S
	@echo "    [CREATE] $$@"
	$$(qexec)LC_ALL=C grep $$(OFFSET_PATTERN) $$< | tr -d '$$$$\#' $$(ADS2GAS) > $$@
$$(BUILD_PFX)$(2).S: $(2)
CLEAN-OBJS += $$(BUILD_PFX)$(1) $(2).S
endef
else
  ifeq ($(filter rvct,$(TGT_CC)), $(TGT_CC))
define asm_offsets_template
$$(BUILD_PFX)$(1): obj_int_extract
$$(BUILD_PFX)$(1): $$(BUILD_PFX)$(2).o
	@echo "    [CREATE] $$@"
	$$(qexec)./obj_int_extract rvds $$< $$(ADS2GAS) > $$@
OBJS-yes += $$(BUILD_PFX)$(2).o
CLEAN-OBJS += $$(BUILD_PFX)$(1)
$$(filter %$$(ASM).o,$$(OBJS-yes)): $$(BUILD_PFX)$(1)
endef
endif # rvct
endif # !gcc

#
# Rule to generate runtime cpu detection files
#
define rtcd_h_template
$$(BUILD_PFX)$(1).h: $$(SRC_PATH_BARE)/$(2)
	@echo "    [CREATE] $$@"
	$$(qexec)$$(SRC_PATH_BARE)/build/make/rtcd.pl --arch=$$(TGT_ISA) \
          --sym=$(1) \
          --config=$$(CONFIG_DIR)$$(target)$$(if $$(FAT_ARCHS),,-$$(TOOLCHAIN)).mk \
          $$(RTCD_OPTIONS) $$^ > $$@
CLEAN-OBJS += $$(BUILD_PFX)$(1).h
RTCD += $$(BUILD_PFX)$(1).h
endef

CODEC_SRCS-yes += CHANGELOG
CODEC_SRCS-yes += libs.mk

# If this is a universal (fat) binary, then all the subarchitectures have
# already been built and our job is to stitch them together. The
# BUILD_LIBVPX variable indicates whether we should be building
# (compiling, linking) the library. The LIPO_LIBVPX variable indicates
# that we're stitching.
$(eval $(if $(filter universal%,$(TOOLCHAIN)),LIPO_LIBVPX,BUILD_LIBVPX):=yes)

include $(SRC_PATH_BARE)/vpx/vpx_codec.mk
CODEC_SRCS-yes += $(addprefix vpx/,$(call enabled,API_SRCS))
CODEC_DOC_SRCS += $(addprefix vpx/,$(call enabled,API_DOC_SRCS))

include $(SRC_PATH_BARE)/vpx_mem/vpx_mem.mk
CODEC_SRCS-yes += $(addprefix vpx_mem/,$(call enabled,MEM_SRCS))

include $(SRC_PATH_BARE)/vpx_scale/vpx_scale.mk
CODEC_SRCS-yes += $(addprefix vpx_scale/,$(call enabled,SCALE_SRCS))

include $(SRC_PATH_BARE)/vpx_ports/vpx_ports.mk
CODEC_SRCS-yes += $(addprefix vpx_ports/,$(call enabled,PORTS_SRCS))

ifneq ($(CONFIG_VP8_ENCODER)$(CONFIG_VP8_DECODER),)
  VP8_PREFIX=vp8/
  include $(SRC_PATH_BARE)/$(VP8_PREFIX)vp8_common.mk
endif

ifeq ($(CONFIG_VP8_ENCODER),yes)
  include $(SRC_PATH_BARE)/$(VP8_PREFIX)vp8cx.mk
  CODEC_SRCS-yes += $(addprefix $(VP8_PREFIX),$(call enabled,VP8_CX_SRCS))
  CODEC_EXPORTS-yes += $(addprefix $(VP8_PREFIX),$(VP8_CX_EXPORTS))
  INSTALL-LIBS-yes += include/vpx/vp8.h include/vpx/vp8cx.h
  INSTALL_MAPS += include/vpx/% $(SRC_PATH_BARE)/$(VP8_PREFIX)/%
  CODEC_DOC_SECTIONS += vp8 vp8_encoder
endif

ifeq ($(CONFIG_VP8_DECODER),yes)
  include $(SRC_PATH_BARE)/$(VP8_PREFIX)vp8dx.mk
  CODEC_SRCS-yes += $(addprefix $(VP8_PREFIX),$(call enabled,VP8_DX_SRCS))
  CODEC_EXPORTS-yes += $(addprefix $(VP8_PREFIX),$(VP8_DX_EXPORTS))
  INSTALL-LIBS-yes += include/vpx/vp8.h include/vpx/vp8dx.h
  INSTALL_MAPS += include/vpx/% $(SRC_PATH_BARE)/$(VP8_PREFIX)/%
  CODEC_DOC_SECTIONS += vp8 vp8_decoder
endif

ifneq ($(CONFIG_VP9_ENCODER)$(CONFIG_VP9_DECODER),)
  VP9_PREFIX=vp9/
  include $(SRC_PATH_BARE)/$(VP9_PREFIX)vp9_common.mk
endif

ifeq ($(CONFIG_VP9_ENCODER),yes)
  VP9_PREFIX=vp9/
  include $(SRC_PATH_BARE)/$(VP9_PREFIX)vp9cx.mk
  CODEC_SRCS-yes += $(addprefix $(VP9_PREFIX),$(call enabled,VP9_CX_SRCS))
  CODEC_EXPORTS-yes += $(addprefix $(VP9_PREFIX),$(VP9_CX_EXPORTS))
  CODEC_SRCS-yes += $(VP9_PREFIX)vp9cx.mk vpx/vp8.h vpx/vp8cx.h
  INSTALL-LIBS-yes += include/vpx/vp8.h include/vpx/vp8cx.h
  INSTALL-LIBS-yes += include/vpx/svc_context.h
  INSTALL_MAPS += include/vpx/% $(SRC_PATH_BARE)/$(VP9_PREFIX)/%
  CODEC_DOC_SRCS += vpx/vp8.h vpx/vp8cx.h
  CODEC_DOC_SECTIONS += vp9 vp9_encoder
endif

ifeq ($(CONFIG_VP9_DECODER),yes)
  VP9_PREFIX=vp9/
  include $(SRC_PATH_BARE)/$(VP9_PREFIX)vp9dx.mk
  CODEC_SRCS-yes += $(addprefix $(VP9_PREFIX),$(call enabled,VP9_DX_SRCS))
  CODEC_EXPORTS-yes += $(addprefix $(VP9_PREFIX),$(VP9_DX_EXPORTS))
  CODEC_SRCS-yes += $(VP9_PREFIX)vp9dx.mk vpx/vp8.h vpx/vp8dx.h
  INSTALL-LIBS-yes += include/vpx/vp8.h include/vpx/vp8dx.h
  INSTALL_MAPS += include/vpx/% $(SRC_PATH_BARE)/$(VP9_PREFIX)/%
  CODEC_DOC_SRCS += vpx/vp8.h vpx/vp8dx.h
  CODEC_DOC_SECTIONS += vp9 vp9_decoder
endif


ifeq ($(CONFIG_ENCODERS),yes)
  CODEC_DOC_SECTIONS += encoder
endif
ifeq ($(CONFIG_DECODERS),yes)
  CODEC_DOC_SECTIONS += decoder
endif


ifeq ($(CONFIG_MSVS),yes)
CODEC_LIB=$(if $(CONFIG_STATIC_MSVCRT),vpxmt,vpxmd)
GTEST_LIB=$(if $(CONFIG_STATIC_MSVCRT),gtestmt,gtestmd)
# This variable uses deferred expansion intentionally, since the results of
# $(wildcard) may change during the course of the Make.
VS_PLATFORMS = $(foreach d,$(wildcard */Release/$(CODEC_LIB).lib),$(word 1,$(subst /, ,$(d))))
endif

# The following pairs define a mapping of locations in the distribution
# tree to locations in the source/build trees.
INSTALL_MAPS += include/vpx/% $(SRC_PATH_BARE)/vpx/%
INSTALL_MAPS += include/vpx/% $(SRC_PATH_BARE)/vpx_ports/%
INSTALL_MAPS += $(LIBSUBDIR)/%     %
INSTALL_MAPS += src/%     $(SRC_PATH_BARE)/%
ifeq ($(CONFIG_MSVS),yes)
INSTALL_MAPS += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/%  $(p)/Release/%)
INSTALL_MAPS += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/%  $(p)/Debug/%)
endif

CODEC_SRCS-$(BUILD_LIBVPX) += build/make/version.sh
CODEC_SRCS-$(BUILD_LIBVPX) += build/make/rtcd.pl
CODEC_SRCS-$(BUILD_LIBVPX) += vpx_ports/emmintrin_compat.h
CODEC_SRCS-$(BUILD_LIBVPX) += vpx_ports/mem_ops.h
CODEC_SRCS-$(BUILD_LIBVPX) += vpx_ports/mem_ops_aligned.h
CODEC_SRCS-$(BUILD_LIBVPX) += vpx_ports/vpx_once.h
CODEC_SRCS-$(BUILD_LIBVPX) += $(BUILD_PFX)vpx_config.c
INSTALL-SRCS-no += $(BUILD_PFX)vpx_config.c
ifeq ($(ARCH_X86)$(ARCH_X86_64),yes)
CODEC_SRCS-$(BUILD_LIBVPX) += third_party/x86inc/x86inc.asm
endif
CODEC_EXPORTS-$(BUILD_LIBVPX) += vpx/exports_com
CODEC_EXPORTS-$(CONFIG_ENCODERS) += vpx/exports_enc
CODEC_EXPORTS-$(CONFIG_DECODERS) += vpx/exports_dec

INSTALL-LIBS-yes += include/vpx/vpx_codec.h
INSTALL-LIBS-yes += include/vpx/vpx_frame_buffer.h
INSTALL-LIBS-yes += include/vpx/vpx_image.h
INSTALL-LIBS-yes += include/vpx/vpx_integer.h
INSTALL-LIBS-$(CONFIG_DECODERS) += include/vpx/vpx_decoder.h
INSTALL-LIBS-$(CONFIG_ENCODERS) += include/vpx/vpx_encoder.h
ifeq ($(CONFIG_EXTERNAL_BUILD),yes)
ifeq ($(CONFIG_MSVS),yes)
INSTALL-LIBS-yes                  += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/$(CODEC_LIB).lib)
INSTALL-LIBS-$(CONFIG_DEBUG_LIBS) += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/$(CODEC_LIB)d.lib)
INSTALL-LIBS-$(CONFIG_SHARED) += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/vpx.dll)
INSTALL-LIBS-$(CONFIG_SHARED) += $(foreach p,$(VS_PLATFORMS),$(LIBSUBDIR)/$(p)/vpx.exp)
endif
else
INSTALL-LIBS-$(CONFIG_STATIC) += $(LIBSUBDIR)/libvpx.a
INSTALL-LIBS-$(CONFIG_DEBUG_LIBS) += $(LIBSUBDIR)/libvpx_g.a
endif

CODEC_SRCS=$(call enabled,CODEC_SRCS)
INSTALL-SRCS-$(CONFIG_CODEC_SRCS) += $(CODEC_SRCS)
INSTALL-SRCS-$(CONFIG_CODEC_SRCS) += $(call enabled,CODEC_EXPORTS)


# Generate a list of all enabled sources, in particular for exporting to gyp
# based build systems.
libvpx_srcs.txt:
	@echo "    [CREATE] $@"
	@echo $(CODEC_SRCS) | xargs -n1 echo | sort -u > $@
CLEAN-OBJS += libvpx_srcs.txt


ifeq ($(CONFIG_EXTERNAL_BUILD),yes)
ifeq ($(CONFIG_MSVS),yes)

obj_int_extract.bat: $(SRC_PATH_BARE)/build/$(MSVS_ARCH_DIR)/obj_int_extract.bat
	@cp $^ $@

obj_int_extract.$(VCPROJ_SFX): obj_int_extract.bat
obj_int_extract.$(VCPROJ_SFX): $(SRC_PATH_BARE)/build/make/obj_int_extract.c
	@echo "    [CREATE] $@"
	$(qexec)$(GEN_VCPROJ) \
    --exe \
    --target=$(TOOLCHAIN) \
    --name=obj_int_extract \
    --ver=$(CONFIG_VS_VERSION) \
    --proj-guid=E1360C65-D375-4335-8057-7ED99CC3F9B2 \
    $(if $(CONFIG_STATIC_MSVCRT),--static-crt) \
    --out=$@ $^ \
    -I. \
    -I"$(SRC_PATH_BARE)" \

PROJECTS-$(BUILD_LIBVPX) += obj_int_extract.$(VCPROJ_SFX)

vpx.def: $(call enabled,CODEC_EXPORTS)
	@echo "    [CREATE] $@"
	$(qexec)$(SRC_PATH_BARE)/build/make/gen_msvs_def.sh\
            --name=vpx\
            --out=$@ $^
CLEAN-OBJS += vpx.def

# Assembly files that are included, but don't define symbols themselves.
# Filtered out to avoid Visual Studio build warnings.
ASM_INCLUDES := \
    third_party/x86inc/x86inc.asm \
    vpx_config.asm \
    vpx_ports/x86_abi_support.asm \

vpx.$(VCPROJ_SFX): $(CODEC_SRCS) vpx.def obj_int_extract.$(VCPROJ_SFX)
	@echo "    [CREATE] $@"
	$(qexec)$(GEN_VCPROJ) \
            $(if $(CONFIG_SHARED),--dll,--lib) \
            --target=$(TOOLCHAIN) \
            $(if $(CONFIG_STATIC_MSVCRT),--static-crt) \
            --name=vpx \
            --proj-guid=DCE19DAF-69AC-46DB-B14A-39F0FAA5DB74 \
            --module-def=vpx.def \
            --ver=$(CONFIG_VS_VERSION) \
            --out=$@ $(CFLAGS) \
            $(filter-out $(addprefix %, $(ASM_INCLUDES)), $^) \
            --src-path-bare="$(SRC_PATH_BARE)" \

PROJECTS-$(BUILD_LIBVPX) += vpx.$(VCPROJ_SFX)

vpx.$(VCPROJ_SFX): vpx_config.asm
vpx.$(VCPROJ_SFX): $(RTCD)

endif
else
LIBVPX_OBJS=$(call objs,$(CODEC_SRCS))
OBJS-$(BUILD_LIBVPX) += $(LIBVPX_OBJS)
LIBS-$(if $(BUILD_LIBVPX),$(CONFIG_STATIC)) += $(BUILD_PFX)libvpx.a $(BUILD_PFX)libvpx_g.a
$(BUILD_PFX)libvpx_g.a: $(LIBVPX_OBJS)


BUILD_LIBVPX_SO         := $(if $(BUILD_LIBVPX),$(CONFIG_SHARED))

ifeq ($(filter darwin%,$(TGT_OS)),$(TGT_OS))
LIBVPX_SO               := libvpx.$(VERSION_MAJOR).dylib
EXPORT_FILE             := libvpx.syms
LIBVPX_SO_SYMLINKS      := $(addprefix $(LIBSUBDIR)/, \
                             libvpx.dylib  )
else
LIBVPX_SO               := libvpx.so.$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)
EXPORT_FILE             := libvpx.ver
SYM_LINK                := libvpx.so
LIBVPX_SO_SYMLINKS      := $(addprefix $(LIBSUBDIR)/, \
                             libvpx.so libvpx.so.$(VERSION_MAJOR) \
                             libvpx.so.$(VERSION_MAJOR).$(VERSION_MINOR))
endif

LIBS-$(BUILD_LIBVPX_SO) += $(BUILD_PFX)$(LIBVPX_SO)\
                           $(notdir $(LIBVPX_SO_SYMLINKS))
$(BUILD_PFX)$(LIBVPX_SO): $(LIBVPX_OBJS) $(EXPORT_FILE)
$(BUILD_PFX)$(LIBVPX_SO): extralibs += -lm
$(BUILD_PFX)$(LIBVPX_SO): SONAME = libvpx.so.$(VERSION_MAJOR)
$(BUILD_PFX)$(LIBVPX_SO): EXPORTS_FILE = $(EXPORT_FILE)

libvpx.ver: $(call enabled,CODEC_EXPORTS)
	@echo "    [CREATE] $@"
	$(qexec)echo "{ global:" > $@
	$(qexec)for f in $?; do awk '{print $$2";"}' < $$f >>$@; done
	$(qexec)echo "local: *; };" >> $@
CLEAN-OBJS += libvpx.ver

libvpx.syms: $(call enabled,CODEC_EXPORTS)
	@echo "    [CREATE] $@"
	$(qexec)awk '{print "_"$$2}' $^ >$@
CLEAN-OBJS += libvpx.syms

define libvpx_symlink_template
$(1): $(2)
	@echo "    [LN]     $(2) $$@"
	$(qexec)mkdir -p $$(dir $$@)
	$(qexec)ln -sf $(2) $$@
endef

$(eval $(call libvpx_symlink_template,\
    $(addprefix $(BUILD_PFX),$(notdir $(LIBVPX_SO_SYMLINKS))),\
    $(BUILD_PFX)$(LIBVPX_SO)))
$(eval $(call libvpx_symlink_template,\
    $(addprefix $(DIST_DIR)/,$(LIBVPX_SO_SYMLINKS)),\
    $(LIBVPX_SO)))


INSTALL-LIBS-$(BUILD_LIBVPX_SO) += $(LIBVPX_SO_SYMLINKS)
INSTALL-LIBS-$(BUILD_LIBVPX_SO) += $(LIBSUBDIR)/$(LIBVPX_SO)


LIBS-$(BUILD_LIBVPX) += vpx.pc
vpx.pc: config.mk libs.mk
	@echo "    [CREATE] $@"
	$(qexec)echo '# pkg-config file from libvpx $(VERSION_STRING)' > $@
	$(qexec)echo 'prefix=$(PREFIX)' >> $@
	$(qexec)echo 'exec_prefix=$${prefix}' >> $@
	$(qexec)echo 'libdir=$${prefix}/$(LIBSUBDIR)' >> $@
	$(qexec)echo 'includedir=$${prefix}/include' >> $@
	$(qexec)echo '' >> $@
	$(qexec)echo 'Name: vpx' >> $@
	$(qexec)echo 'Description: WebM Project VPx codec implementation' >> $@
	$(qexec)echo 'Version: $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)' >> $@
	$(qexec)echo 'Requires:' >> $@
	$(qexec)echo 'Conflicts:' >> $@
	$(qexec)echo 'Libs: -L$${libdir} -lvpx -lm' >> $@
ifeq ($(HAVE_PTHREAD_H),yes)
	$(qexec)echo 'Libs.private: -lm -lpthread' >> $@
else
	$(qexec)echo 'Libs.private: -lm' >> $@
endif
	$(qexec)echo 'Cflags: -I$${includedir}' >> $@
INSTALL-LIBS-yes += $(LIBSUBDIR)/pkgconfig/vpx.pc
INSTALL_MAPS += $(LIBSUBDIR)/pkgconfig/%.pc %.pc
CLEAN-OBJS += vpx.pc
endif

LIBS-$(LIPO_LIBVPX) += libvpx.a
$(eval $(if $(LIPO_LIBVPX),$(call lipo_lib_template,libvpx.a)))

#
# Rule to make assembler configuration file from C configuration file
#
ifeq ($(ARCH_X86)$(ARCH_X86_64),yes)
# YASM
$(BUILD_PFX)vpx_config.asm: $(BUILD_PFX)vpx_config.h
	@echo "    [CREATE] $@"
	@egrep "#define [A-Z0-9_]+ [01]" $< \
	    | awk '{print $$2 " equ " $$3}' > $@
else
ADS2GAS=$(if $(filter yes,$(CONFIG_GCC)),| $(ASM_CONVERSION))
$(BUILD_PFX)vpx_config.asm: $(BUILD_PFX)vpx_config.h
	@echo "    [CREATE] $@"
	@egrep "#define [A-Z0-9_]+ [01]" $< \
	    | awk '{print $$2 " EQU " $$3}' $(ADS2GAS) > $@
	@echo "        END" $(ADS2GAS) >> $@
CLEAN-OBJS += $(BUILD_PFX)vpx_config.asm
endif

#
# Add assembler dependencies for configuration and offsets
#
$(filter %.s.o,$(OBJS-yes)):     $(BUILD_PFX)vpx_config.asm
$(filter %$(ASM).o,$(OBJS-yes)): $(BUILD_PFX)vpx_config.asm


$(shell $(SRC_PATH_BARE)/build/make/version.sh "$(SRC_PATH_BARE)" $(BUILD_PFX)vpx_version.h)
CLEAN-OBJS += $(BUILD_PFX)vpx_version.h


##
## libvpx test directives
##
ifeq ($(CONFIG_UNIT_TESTS),yes)
LIBVPX_TEST_DATA_PATH ?= .

include $(SRC_PATH_BARE)/test/test.mk
LIBVPX_TEST_SRCS=$(addprefix test/,$(call enabled,LIBVPX_TEST_SRCS))
LIBVPX_TEST_BINS=./test_libvpx$(EXE_SFX)
LIBVPX_TEST_DATA=$(addprefix $(LIBVPX_TEST_DATA_PATH)/,\
                     $(call enabled,LIBVPX_TEST_DATA))
libvpx_test_data_url=http://downloads.webmproject.org/test_data/libvpx/$(1)

libvpx_test_srcs.txt:
	@echo "    [CREATE] $@"
	@echo $(LIBVPX_TEST_SRCS) | xargs -n1 echo | sort -u > $@
CLEAN-OBJS += libvpx_test_srcs.txt

$(LIBVPX_TEST_DATA): $(SRC_PATH_BARE)/test/test-data.sha1
	@echo "    [DOWNLOAD] $@"
	$(qexec)trap 'rm -f $@' INT TERM &&\
            curl -L -o $@ $(call libvpx_test_data_url,$(@F))

testdata:: $(LIBVPX_TEST_DATA)
	$(qexec)if [ -x "$$(which sha1sum)" ]; then\
            echo "Checking test data:";\
            if [ -n "$(LIBVPX_TEST_DATA)" ]; then\
                for f in $(call enabled,LIBVPX_TEST_DATA); do\
                    grep $$f $(SRC_PATH_BARE)/test/test-data.sha1 |\
                        (cd $(LIBVPX_TEST_DATA_PATH); sha1sum -c);\
                done; \
            fi; \
        else\
            echo "Skipping test data integrity check, sha1sum not found.";\
        fi

ifeq ($(CONFIG_EXTERNAL_BUILD),yes)
ifeq ($(CONFIG_MSVS),yes)

gtest.$(VCPROJ_SFX): $(SRC_PATH_BARE)/third_party/googletest/src/src/gtest-all.cc
	@echo "    [CREATE] $@"
	$(qexec)$(GEN_VCPROJ) \
            --lib \
            --target=$(TOOLCHAIN) \
            $(if $(CONFIG_STATIC_MSVCRT),--static-crt) \
            --name=gtest \
            --proj-guid=EC00E1EC-AF68-4D92-A255-181690D1C9B1 \
            --ver=$(CONFIG_VS_VERSION) \
            --src-path-bare="$(SRC_PATH_BARE)" \
            -D_VARIADIC_MAX=10 \
            --out=gtest.$(VCPROJ_SFX) $(SRC_PATH_BARE)/third_party/googletest/src/src/gtest-all.cc \
            -I. -I"$(SRC_PATH_BARE)/third_party/googletest/src/include" -I"$(SRC_PATH_BARE)/third_party/googletest/src"

PROJECTS-$(CONFIG_MSVS) += gtest.$(VCPROJ_SFX)

test_libvpx.$(VCPROJ_SFX): $(LIBVPX_TEST_SRCS) vpx.$(VCPROJ_SFX) gtest.$(VCPROJ_SFX)
	@echo "    [CREATE] $@"
	$(qexec)$(GEN_VCPROJ) \
            --exe \
            --target=$(TOOLCHAIN) \
            --name=test_libvpx \
            -D_VARIADIC_MAX=10 \
            --proj-guid=CD837F5F-52D8-4314-A370-895D614166A7 \
            --ver=$(CONFIG_VS_VERSION) \
            $(if $(CONFIG_STATIC_MSVCRT),--static-crt) \
            --out=$@ $(INTERNAL_CFLAGS) $(CFLAGS) \
            -I. -I"$(SRC_PATH_BARE)/third_party/googletest/src/include" \
            -L. -l$(CODEC_LIB) -l$(GTEST_LIB) $^

PROJECTS-$(CONFIG_MSVS) += test_libvpx.$(VCPROJ_SFX)

LIBVPX_TEST_BINS := $(addprefix $(TGT_OS:win64=x64)/Release/,$(notdir $(LIBVPX_TEST_BINS)))
endif
else

include $(SRC_PATH_BARE)/third_party/googletest/gtest.mk
GTEST_SRCS := $(addprefix third_party/googletest/src/,$(call enabled,GTEST_SRCS))
GTEST_OBJS=$(call objs,$(GTEST_SRCS))
ifeq ($(filter win%,$(TGT_OS)),$(TGT_OS))
# Disabling pthreads globally will cause issues on darwin and possibly elsewhere
$(GTEST_OBJS) $(GTEST_OBJS:.o=.d): CXXFLAGS += -DGTEST_HAS_PTHREAD=0
endif
$(GTEST_OBJS) $(GTEST_OBJS:.o=.d): CXXFLAGS += -I$(SRC_PATH_BARE)/third_party/googletest/src
$(GTEST_OBJS) $(GTEST_OBJS:.o=.d): CXXFLAGS += -I$(SRC_PATH_BARE)/third_party/googletest/src/include
OBJS-$(BUILD_LIBVPX) += $(GTEST_OBJS)
LIBS-$(BUILD_LIBVPX) += $(BUILD_PFX)libgtest.a $(BUILD_PFX)libgtest_g.a
$(BUILD_PFX)libgtest_g.a: $(GTEST_OBJS)

LIBVPX_TEST_OBJS=$(sort $(call objs,$(LIBVPX_TEST_SRCS)))
$(LIBVPX_TEST_OBJS) $(LIBVPX_TEST_OBJS:.o=.d): CXXFLAGS += -I$(SRC_PATH_BARE)/third_party/googletest/src
$(LIBVPX_TEST_OBJS) $(LIBVPX_TEST_OBJS:.o=.d): CXXFLAGS += -I$(SRC_PATH_BARE)/third_party/googletest/src/include
OBJS-$(BUILD_LIBVPX) += $(LIBVPX_TEST_OBJS)
BINS-$(BUILD_LIBVPX) += $(LIBVPX_TEST_BINS)

CODEC_LIB=$(if $(CONFIG_DEBUG_LIBS),vpx_g,vpx)
CODEC_LIB_SUF=$(if $(CONFIG_SHARED),.so,.a)
$(foreach bin,$(LIBVPX_TEST_BINS),\
    $(if $(BUILD_LIBVPX),$(eval $(bin): \
        lib$(CODEC_LIB)$(CODEC_LIB_SUF) libgtest.a ))\
    $(if $(BUILD_LIBVPX),$(eval $(call linkerxx_template,$(bin),\
        $(LIBVPX_TEST_OBJS) \
        -L. -lvpx -lgtest $(extralibs) -lm)\
        )))\
    $(if $(LIPO_LIBS),$(eval $(call lipo_bin_template,$(bin))))\

endif

# Install test sources only if codec source is included
INSTALL-SRCS-$(CONFIG_CODEC_SRCS) += $(patsubst $(SRC_PATH_BARE)/%,%,\
    $(shell find $(SRC_PATH_BARE)/third_party/googletest -type f))
INSTALL-SRCS-$(CONFIG_CODEC_SRCS) += $(LIBVPX_TEST_SRCS)

define test_shard_template
test:: test_shard.$(1)
test_shard.$(1): $(LIBVPX_TEST_BINS) testdata
	@set -e; \
	 for t in $(LIBVPX_TEST_BINS); do \
	   export GTEST_SHARD_INDEX=$(1); \
	   export GTEST_TOTAL_SHARDS=$(2); \
	   $$$$t; \
	 done
.PHONY: test_shard.$(1)
endef

NUM_SHARDS := 10
SHARDS := 0 1 2 3 4 5 6 7 8 9
$(foreach s,$(SHARDS),$(eval $(call test_shard_template,$(s),$(NUM_SHARDS))))

endif

##
## documentation directives
##
CLEAN-OBJS += libs.doxy
DOCS-yes += libs.doxy
libs.doxy: $(CODEC_DOC_SRCS)
	@echo "    [CREATE] $@"
	@rm -f $@
	@echo "INPUT += $^" >> $@
	@echo "PREDEFINED = VPX_CODEC_DISABLE_COMPAT" >> $@
	@echo "INCLUDE_PATH += ." >> $@;
	@echo "ENABLED_SECTIONS += $(sort $(CODEC_DOC_SECTIONS))" >> $@

## Generate rtcd.h for all objects
$(OBJS-yes:.o=.d): $(RTCD)

## Update the global src list
SRCS += $(CODEC_SRCS) $(LIBVPX_TEST_SRCS) $(GTEST_SRCS)
