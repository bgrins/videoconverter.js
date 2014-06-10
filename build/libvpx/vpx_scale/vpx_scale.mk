SCALE_SRCS-yes += vpx_scale.mk
SCALE_SRCS-yes += yv12config.h
SCALE_SRCS-yes += vpx_scale.h
SCALE_SRCS-yes += generic/vpx_scale.c
SCALE_SRCS-yes += generic/yv12config.c
SCALE_SRCS-yes += generic/yv12extend.c
SCALE_SRCS-$(CONFIG_SPATIAL_RESAMPLING) += generic/gen_scalers.c
SCALE_SRCS-yes += vpx_scale_asm_offsets.c
SCALE_SRCS-yes += vpx_scale_rtcd.c
SCALE_SRCS-yes += vpx_scale_rtcd.pl

#neon
SCALE_SRCS-$(HAVE_NEON)  += arm/neon/vp8_vpxyv12_copyframe_func_neon$(ASM)
SCALE_SRCS-$(HAVE_NEON)  += arm/neon/vp8_vpxyv12_copysrcframe_func_neon$(ASM)
SCALE_SRCS-$(HAVE_NEON)  += arm/neon/vp8_vpxyv12_extendframeborders_neon$(ASM)
SCALE_SRCS-$(HAVE_NEON)  += arm/neon/yv12extend_arm.c

#mips(dspr2)
SCALE_SRCS-$(HAVE_DSPR2)  += mips/dspr2/yv12extend_dspr2.c

SCALE_SRCS-no += $(SCALE_SRCS_REMOVE-yes)

$(eval $(call asm_offsets_template,\
	         vpx_scale_asm_offsets.asm, vpx_scale/vpx_scale_asm_offsets.c))

$(eval $(call rtcd_h_template,vpx_scale_rtcd,vpx_scale/vpx_scale_rtcd.pl))
