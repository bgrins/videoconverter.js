sub vp9_common_forward_decls() {
print <<EOF
/*
 * VP9
 */

#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_enums.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct vp9_variance_vtable;

struct mv;
union int_mv;
struct yv12_buffer_config;
EOF
}
forward_decls qw/vp9_common_forward_decls/;

# x86inc.asm doesn't work if pic is enabled on 32 bit platforms so no assembly.
if (vpx_config("CONFIG_USE_X86INC") eq "yes") {
  $mmx_x86inc = 'mmx';
  $sse_x86inc = 'sse';
  $sse2_x86inc = 'sse2';
  $ssse3_x86inc = 'ssse3';
  $avx_x86inc = 'avx';
  $avx2_x86inc = 'avx2';
} else {
  $mmx_x86inc = $sse_x86inc = $sse2_x86inc = $ssse3_x86inc =
  $avx_x86inc = $avx2_x86inc = '';
}

# this variable is for functions that are 64 bit only.
if ($opts{arch} eq "x86_64") {
  $mmx_x86_64 = 'mmx';
  $sse2_x86_64 = 'sse2';
  $ssse3_x86_64 = 'ssse3';
  $avx_x86_64 = 'avx';
  $avx2_x86_64 = 'avx2';
} else {
  $mmx_x86_64 = $sse2_x86_64 = $ssse3_x86_64 =
  $avx_x86_64 = $avx2_x86_64 = '';
}

#
# RECON
#
add_proto qw/void vp9_d207_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d207_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void vp9_d45_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d45_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void vp9_d63_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d63_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void vp9_h_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_h_predictor_4x4 neon dspr2/, "$ssse3_x86inc";

add_proto qw/void vp9_d117_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d117_predictor_4x4/;

add_proto qw/void vp9_d135_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d135_predictor_4x4/;

add_proto qw/void vp9_d153_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d153_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void vp9_v_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_v_predictor_4x4 neon/, "$sse_x86inc";

add_proto qw/void vp9_tm_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_tm_predictor_4x4 neon dspr2/, "$sse_x86inc";

add_proto qw/void vp9_dc_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_predictor_4x4 dspr2/, "$sse_x86inc";

add_proto qw/void vp9_dc_top_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_top_predictor_4x4/;

add_proto qw/void vp9_dc_left_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_left_predictor_4x4/;

add_proto qw/void vp9_dc_128_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_128_predictor_4x4/;

add_proto qw/void vp9_d207_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d207_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void vp9_d45_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d45_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void vp9_d63_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d63_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void vp9_h_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_h_predictor_8x8 neon dspr2/, "$ssse3_x86inc";

add_proto qw/void vp9_d117_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d117_predictor_8x8/;

add_proto qw/void vp9_d135_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d135_predictor_8x8/;

add_proto qw/void vp9_d153_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d153_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void vp9_v_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_v_predictor_8x8 neon/, "$sse_x86inc";

add_proto qw/void vp9_tm_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_tm_predictor_8x8 neon dspr2/, "$sse2_x86inc";

add_proto qw/void vp9_dc_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_predictor_8x8 dspr2/, "$sse_x86inc";

add_proto qw/void vp9_dc_top_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_top_predictor_8x8/;

add_proto qw/void vp9_dc_left_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_left_predictor_8x8/;

add_proto qw/void vp9_dc_128_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_128_predictor_8x8/;

add_proto qw/void vp9_d207_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d207_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void vp9_d45_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d45_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void vp9_d63_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d63_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void vp9_h_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_h_predictor_16x16 neon dspr2/, "$ssse3_x86inc";

add_proto qw/void vp9_d117_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d117_predictor_16x16/;

add_proto qw/void vp9_d135_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d135_predictor_16x16/;

add_proto qw/void vp9_d153_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d153_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void vp9_v_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_v_predictor_16x16 neon/, "$sse2_x86inc";

add_proto qw/void vp9_tm_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_tm_predictor_16x16 neon/, "$sse2_x86inc";

add_proto qw/void vp9_dc_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_predictor_16x16 dspr2/, "$sse2_x86inc";

add_proto qw/void vp9_dc_top_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_top_predictor_16x16/;

add_proto qw/void vp9_dc_left_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_left_predictor_16x16/;

add_proto qw/void vp9_dc_128_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_128_predictor_16x16/;

add_proto qw/void vp9_d207_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d207_predictor_32x32/, "$ssse3_x86inc";

add_proto qw/void vp9_d45_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d45_predictor_32x32/, "$ssse3_x86inc";

add_proto qw/void vp9_d63_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d63_predictor_32x32/, "$ssse3_x86inc";

add_proto qw/void vp9_h_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_h_predictor_32x32 neon/, "$ssse3_x86inc";

add_proto qw/void vp9_d117_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d117_predictor_32x32/;

add_proto qw/void vp9_d135_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d135_predictor_32x32/;

add_proto qw/void vp9_d153_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d153_predictor_32x32/;

add_proto qw/void vp9_v_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_v_predictor_32x32 neon/, "$sse2_x86inc";

add_proto qw/void vp9_tm_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_tm_predictor_32x32 neon/, "$sse2_x86_64";

add_proto qw/void vp9_dc_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_predictor_32x32/, "$sse2_x86inc";

add_proto qw/void vp9_dc_top_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_top_predictor_32x32/;

add_proto qw/void vp9_dc_left_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_left_predictor_32x32/;

add_proto qw/void vp9_dc_128_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_128_predictor_32x32/;

#
# Loopfilter
#
add_proto qw/void vp9_lpf_vertical_16/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/vp9_lpf_vertical_16 sse2 neon dspr2/;

add_proto qw/void vp9_lpf_vertical_16_dual/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/vp9_lpf_vertical_16_dual sse2 neon dspr2/;

add_proto qw/void vp9_lpf_vertical_8/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/vp9_lpf_vertical_8 sse2 neon dspr2/;

add_proto qw/void vp9_lpf_vertical_8_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/vp9_lpf_vertical_8_dual sse2 neon dspr2/;

add_proto qw/void vp9_lpf_vertical_4/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/vp9_lpf_vertical_4 mmx neon dspr2/;

add_proto qw/void vp9_lpf_vertical_4_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/vp9_lpf_vertical_4_dual sse2 neon dspr2/;

add_proto qw/void vp9_lpf_horizontal_16/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/vp9_lpf_horizontal_16 sse2 avx2 neon dspr2/;

add_proto qw/void vp9_lpf_horizontal_8/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/vp9_lpf_horizontal_8 sse2 neon dspr2/;

add_proto qw/void vp9_lpf_horizontal_8_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/vp9_lpf_horizontal_8_dual sse2 neon dspr2/;

add_proto qw/void vp9_lpf_horizontal_4/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/vp9_lpf_horizontal_4 mmx neon dspr2/;

add_proto qw/void vp9_lpf_horizontal_4_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/vp9_lpf_horizontal_4_dual sse2 neon dspr2/;

#
# post proc
#
if (vpx_config("CONFIG_VP9_POSTPROC") eq "yes") {
add_proto qw/void vp9_mbpost_proc_down/, "uint8_t *dst, int pitch, int rows, int cols, int flimit";
specialize qw/vp9_mbpost_proc_down mmx sse2/;
$vp9_mbpost_proc_down_sse2=vp9_mbpost_proc_down_xmm;

add_proto qw/void vp9_mbpost_proc_across_ip/, "uint8_t *src, int pitch, int rows, int cols, int flimit";
specialize qw/vp9_mbpost_proc_across_ip sse2/;
$vp9_mbpost_proc_across_ip_sse2=vp9_mbpost_proc_across_ip_xmm;

add_proto qw/void vp9_post_proc_down_and_across/, "const uint8_t *src_ptr, uint8_t *dst_ptr, int src_pixels_per_line, int dst_pixels_per_line, int rows, int cols, int flimit";
specialize qw/vp9_post_proc_down_and_across mmx sse2/;
$vp9_post_proc_down_and_across_sse2=vp9_post_proc_down_and_across_xmm;

add_proto qw/void vp9_plane_add_noise/, "uint8_t *Start, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int Width, unsigned int Height, int Pitch";
specialize qw/vp9_plane_add_noise mmx sse2/;
$vp9_plane_add_noise_sse2=vp9_plane_add_noise_wmt;
}

add_proto qw/void vp9_blend_mb_inner/, "uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride";
specialize qw/vp9_blend_mb_inner/;

add_proto qw/void vp9_blend_mb_outer/, "uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride";
specialize qw/vp9_blend_mb_outer/;

add_proto qw/void vp9_blend_b/, "uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride";
specialize qw/vp9_blend_b/;

#
# Sub Pixel Filters
#
add_proto qw/void vp9_convolve_copy/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve_copy neon dspr2/, "$sse2_x86inc";

add_proto qw/void vp9_convolve_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve_avg neon dspr2/, "$sse2_x86inc";

add_proto qw/void vp9_convolve8/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8 sse2 ssse3 avx2 neon dspr2/;

add_proto qw/void vp9_convolve8_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8_horiz sse2 ssse3 avx2 neon dspr2/;

add_proto qw/void vp9_convolve8_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8_vert sse2 ssse3 avx2 neon dspr2/;

add_proto qw/void vp9_convolve8_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8_avg sse2 ssse3 neon dspr2/;

add_proto qw/void vp9_convolve8_avg_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8_avg_horiz sse2 ssse3 neon dspr2/;

add_proto qw/void vp9_convolve8_avg_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8_avg_vert sse2 ssse3 neon dspr2/;

#
# dct
#
add_proto qw/void vp9_idct4x4_1_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct4x4_1_add sse2 neon dspr2/;

add_proto qw/void vp9_idct4x4_16_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct4x4_16_add sse2 neon dspr2/;

add_proto qw/void vp9_idct8x8_1_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct8x8_1_add sse2 neon dspr2/;

add_proto qw/void vp9_idct8x8_64_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct8x8_64_add sse2 neon dspr2/;

add_proto qw/void vp9_idct8x8_10_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct8x8_10_add sse2 neon dspr2/;

add_proto qw/void vp9_idct16x16_1_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct16x16_1_add sse2 neon dspr2/;

add_proto qw/void vp9_idct16x16_256_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct16x16_256_add sse2 neon dspr2/;

add_proto qw/void vp9_idct16x16_10_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct16x16_10_add sse2 neon dspr2/;

add_proto qw/void vp9_idct32x32_1024_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct32x32_1024_add sse2 neon dspr2/;

add_proto qw/void vp9_idct32x32_34_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct32x32_34_add sse2 neon dspr2/;
$vp9_idct32x32_34_add_neon=vp9_idct32x32_1024_add_neon;

add_proto qw/void vp9_idct32x32_1_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_idct32x32_1_add sse2 neon dspr2/;

add_proto qw/void vp9_iht4x4_16_add/, "const int16_t *input, uint8_t *dest, int dest_stride, int tx_type";
specialize qw/vp9_iht4x4_16_add sse2 neon dspr2/;

add_proto qw/void vp9_iht8x8_64_add/, "const int16_t *input, uint8_t *dest, int dest_stride, int tx_type";
specialize qw/vp9_iht8x8_64_add sse2 neon dspr2/;

add_proto qw/void vp9_iht16x16_256_add/, "const int16_t *input, uint8_t *output, int pitch, int tx_type";
specialize qw/vp9_iht16x16_256_add sse2 dspr2/;

# dct and add

add_proto qw/void vp9_iwht4x4_1_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_iwht4x4_1_add/;

add_proto qw/void vp9_iwht4x4_16_add/, "const int16_t *input, uint8_t *dest, int dest_stride";
specialize qw/vp9_iwht4x4_16_add/;

#
# Encoder functions below this point.
#
if (vpx_config("CONFIG_VP9_ENCODER") eq "yes") {


# variance
add_proto qw/unsigned int vp9_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance32x16/, "$sse2_x86inc", "$avx2_x86inc";

add_proto qw/unsigned int vp9_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance16x32/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance64x32/, "$sse2_x86inc", "$avx2_x86inc";

add_proto qw/unsigned int vp9_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance32x64/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance32x32/, "$sse2_x86inc", "$avx2_x86inc";

add_proto qw/unsigned int vp9_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance64x64/, "$sse2_x86inc", "$avx2_x86inc";

add_proto qw/unsigned int vp9_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance16x16 mmx/, "$sse2_x86inc", "$avx2_x86inc";

add_proto qw/void vp9_get_sse_sum_16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
specialize qw/vp9_get_sse_sum_16x16 sse2/;
$vp9_get_sse_sum_16x16_sse2=vp9_get16x16var_sse2;

add_proto qw/unsigned int vp9_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance16x8 mmx/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance8x16 mmx/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance8x8 mmx/, "$sse2_x86inc";

add_proto qw/void vp9_get_sse_sum_8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
specialize qw/vp9_get_sse_sum_8x8 sse2/;
$vp9_get_sse_sum_8x8_sse2=vp9_get8x8var_sse2;

add_proto qw/unsigned int vp9_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance8x4/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance4x8/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance4x4 mmx/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance64x64 avx2/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance64x64 avx2/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance32x64/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance32x64/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance64x32/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance64x32/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance32x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance32x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance16x32/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance16x32/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance32x32 avx2/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance32x32 avx2/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance16x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance16x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance8x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance8x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance16x8/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance16x8/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance8x8/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance8x8/, "$sse2_x86inc", "$ssse3_x86inc";

# TODO(jingning): need to convert 8x4/4x8 functions into mmx/sse form
add_proto qw/unsigned int vp9_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance8x4/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance8x4/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance4x8/, "$sse_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance4x8/, "$sse_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance4x4/, "$sse_x86inc", "$ssse3_x86inc";
#vp9_sub_pixel_variance4x4_sse2=vp9_sub_pixel_variance4x4_wmt

add_proto qw/unsigned int vp9_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance4x4/, "$sse_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sad64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad";
specialize qw/vp9_sad64x64/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int max_sad";
specialize qw/vp9_sad32x64/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int max_sad";
specialize qw/vp9_sad64x32/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int max_sad";
specialize qw/vp9_sad32x16/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int max_sad";
specialize qw/vp9_sad16x32/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad";
specialize qw/vp9_sad32x32/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad";
specialize qw/vp9_sad16x16 mmx/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad";
specialize qw/vp9_sad16x8 mmx/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad";
specialize qw/vp9_sad8x16 mmx/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad";
specialize qw/vp9_sad8x8 mmx/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int max_sad";
specialize qw/vp9_sad8x4/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int max_sad";
specialize qw/vp9_sad4x8/, "$sse_x86inc";

add_proto qw/unsigned int vp9_sad4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int max_sad";
specialize qw/vp9_sad4x4 mmx/, "$sse_x86inc";

add_proto qw/unsigned int vp9_sad64x64_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad64x64_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad32x64_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad32x64_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad64x32_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad64x32_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad32x16_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad32x16_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad16x32_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad16x32_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad32x32_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad32x32_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad16x16_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad16x16_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad16x8_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad16x8_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad8x16_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad8x16_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad8x8_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad8x8_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad8x4_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad8x4_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_sad4x8_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad4x8_avg/, "$sse_x86inc";

add_proto qw/unsigned int vp9_sad4x4_avg/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred, unsigned int max_sad";
specialize qw/vp9_sad4x4_avg/, "$sse_x86inc";

add_proto qw/unsigned int vp9_variance_halfpixvar16x16_h/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance_halfpixvar16x16_h/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_variance_halfpixvar16x16_v/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance_halfpixvar16x16_v/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_variance_halfpixvar16x16_hv/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance_halfpixvar16x16_hv/, "$sse2_x86inc";

add_proto qw/unsigned int vp9_variance_halfpixvar64x64_h/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance_halfpixvar64x64_h/;

add_proto qw/unsigned int vp9_variance_halfpixvar64x64_v/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance_halfpixvar64x64_v/;

add_proto qw/unsigned int vp9_variance_halfpixvar64x64_hv/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance_halfpixvar64x64_hv/;

add_proto qw/unsigned int vp9_variance_halfpixvar32x32_h/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance_halfpixvar32x32_h/;

add_proto qw/unsigned int vp9_variance_halfpixvar32x32_v/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance_halfpixvar32x32_v/;

add_proto qw/unsigned int vp9_variance_halfpixvar32x32_hv/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_variance_halfpixvar32x32_hv/;

add_proto qw/void vp9_sad64x64x3/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad64x64x3/;

add_proto qw/void vp9_sad32x32x3/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad32x32x3/;

add_proto qw/void vp9_sad16x16x3/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad16x16x3 sse3 ssse3/;

add_proto qw/void vp9_sad16x8x3/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad16x8x3 sse3 ssse3/;

add_proto qw/void vp9_sad8x16x3/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad8x16x3 sse3/;

add_proto qw/void vp9_sad8x8x3/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad8x8x3 sse3/;

add_proto qw/void vp9_sad4x4x3/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad4x4x3 sse3/;

add_proto qw/void vp9_sad64x64x8/, "const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array";
specialize qw/vp9_sad64x64x8/;

add_proto qw/void vp9_sad32x32x8/, "const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array";
specialize qw/vp9_sad32x32x8/;

add_proto qw/void vp9_sad16x16x8/, "const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array";
specialize qw/vp9_sad16x16x8 sse4/;

add_proto qw/void vp9_sad16x8x8/, "const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array";
specialize qw/vp9_sad16x8x8 sse4/;

add_proto qw/void vp9_sad8x16x8/, "const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array";
specialize qw/vp9_sad8x16x8 sse4/;

add_proto qw/void vp9_sad8x8x8/, "const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array";
specialize qw/vp9_sad8x8x8 sse4/;

add_proto qw/void vp9_sad8x4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vp9_sad8x4x8/;

add_proto qw/void vp9_sad4x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vp9_sad4x8x8/;

add_proto qw/void vp9_sad4x4x8/, "const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array";
specialize qw/vp9_sad4x4x8 sse4/;

add_proto qw/void vp9_sad64x64x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad64x64x4d sse2 avx2/;

add_proto qw/void vp9_sad32x64x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad32x64x4d sse2/;

add_proto qw/void vp9_sad64x32x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad64x32x4d sse2/;

add_proto qw/void vp9_sad32x16x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad32x16x4d sse2/;

add_proto qw/void vp9_sad16x32x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad16x32x4d sse2/;

add_proto qw/void vp9_sad32x32x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad32x32x4d sse2 avx2/;

add_proto qw/void vp9_sad16x16x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad16x16x4d sse2/;

add_proto qw/void vp9_sad16x8x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad16x8x4d sse2/;

add_proto qw/void vp9_sad8x16x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad8x16x4d sse2/;

add_proto qw/void vp9_sad8x8x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad8x8x4d sse2/;

# TODO(jingning): need to convert these 4x8/8x4 functions into sse2 form
add_proto qw/void vp9_sad8x4x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad8x4x4d sse2/;

add_proto qw/void vp9_sad4x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad4x8x4d sse/;

add_proto qw/void vp9_sad4x4x4d/, "const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array";
specialize qw/vp9_sad4x4x4d sse/;

#add_proto qw/unsigned int vp9_sub_pixel_mse16x16/, "const uint8_t *src_ptr, int  src_pixels_per_line, int  xoffset, int  yoffset, const uint8_t *dst_ptr, int dst_pixels_per_line, unsigned int *sse";
#specialize qw/vp9_sub_pixel_mse16x16 sse2 mmx/;

add_proto qw/unsigned int vp9_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
specialize qw/vp9_mse16x16 mmx/, "$sse2_x86inc", "$avx2_x86inc";

add_proto qw/unsigned int vp9_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
specialize qw/vp9_mse8x16/;

add_proto qw/unsigned int vp9_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
specialize qw/vp9_mse16x8/;

add_proto qw/unsigned int vp9_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
specialize qw/vp9_mse8x8/;

add_proto qw/unsigned int vp9_sub_pixel_mse64x64/, "const uint8_t *src_ptr, int  source_stride, int  xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_mse64x64/;

add_proto qw/unsigned int vp9_sub_pixel_mse32x32/, "const uint8_t *src_ptr, int  source_stride, int  xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_mse32x32/;

add_proto qw/unsigned int vp9_get_mb_ss/, "const int16_t *";
specialize qw/vp9_get_mb_ss mmx sse2/;
# ENCODEMB INVOKE

add_proto qw/int64_t vp9_block_error/, "const int16_t *coeff, const int16_t *dqcoeff, intptr_t block_size, int64_t *ssz";
specialize qw/vp9_block_error/, "$sse2_x86inc";

add_proto qw/void vp9_subtract_block/, "int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride";
specialize qw/vp9_subtract_block/, "$sse2_x86inc";

add_proto qw/void vp9_quantize_b/, "const int16_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr, const int16_t *dequant_ptr, int zbin_oq_value, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
specialize qw/vp9_quantize_b/, "$ssse3_x86_64";

add_proto qw/void vp9_quantize_b_32x32/, "const int16_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr, const int16_t *dequant_ptr, int zbin_oq_value, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
specialize qw/vp9_quantize_b_32x32/, "$ssse3_x86_64";

#
# Structured Similarity (SSIM)
#
if (vpx_config("CONFIG_INTERNAL_STATS") eq "yes") {
    add_proto qw/void vp9_ssim_parms_8x8/, "uint8_t *s, int sp, uint8_t *r, int rp, unsigned long *sum_s, unsigned long *sum_r, unsigned long *sum_sq_s, unsigned long *sum_sq_r, unsigned long *sum_sxr";
    specialize qw/vp9_ssim_parms_8x8/, "$sse2_x86_64";

    add_proto qw/void vp9_ssim_parms_16x16/, "uint8_t *s, int sp, uint8_t *r, int rp, unsigned long *sum_s, unsigned long *sum_r, unsigned long *sum_sq_s, unsigned long *sum_sq_r, unsigned long *sum_sxr";
    specialize qw/vp9_ssim_parms_16x16/, "$sse2_x86_64";
}

# fdct functions
add_proto qw/void vp9_fht4x4/, "const int16_t *input, int16_t *output, int stride, int tx_type";
specialize qw/vp9_fht4x4 sse2 avx2/;

add_proto qw/void vp9_fht8x8/, "const int16_t *input, int16_t *output, int stride, int tx_type";
specialize qw/vp9_fht8x8 sse2 avx2/;

add_proto qw/void vp9_fht16x16/, "const int16_t *input, int16_t *output, int stride, int tx_type";
specialize qw/vp9_fht16x16 sse2 avx2/;

add_proto qw/void vp9_fwht4x4/, "const int16_t *input, int16_t *output, int stride";
specialize qw/vp9_fwht4x4/;

add_proto qw/void vp9_fdct4x4/, "const int16_t *input, int16_t *output, int stride";
specialize qw/vp9_fdct4x4 sse2 avx2/;

add_proto qw/void vp9_fdct8x8/, "const int16_t *input, int16_t *output, int stride";
specialize qw/vp9_fdct8x8 sse2 avx2/;

add_proto qw/void vp9_fdct16x16/, "const int16_t *input, int16_t *output, int stride";
specialize qw/vp9_fdct16x16 sse2 avx2/;

add_proto qw/void vp9_fdct32x32/, "const int16_t *input, int16_t *output, int stride";
specialize qw/vp9_fdct32x32 sse2 avx2/;

add_proto qw/void vp9_fdct32x32_rd/, "const int16_t *input, int16_t *output, int stride";
specialize qw/vp9_fdct32x32_rd sse2 avx2/;

#
# Motion search
#
add_proto qw/int vp9_full_search_sad/, "const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv";
specialize qw/vp9_full_search_sad sse3 sse4_1/;
$vp9_full_search_sad_sse3=vp9_full_search_sadx3;
$vp9_full_search_sad_sse4_1=vp9_full_search_sadx8;

add_proto qw/int vp9_refining_search_sad/, "const struct macroblock *x, struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv";
specialize qw/vp9_refining_search_sad sse3/;
$vp9_refining_search_sad_sse3=vp9_refining_search_sadx4;

add_proto qw/int vp9_diamond_search_sad/, "const struct macroblock *x, struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv";
specialize qw/vp9_diamond_search_sad sse3/;
$vp9_diamond_search_sad_sse3=vp9_diamond_search_sadx4;

add_proto qw/int vp9_full_range_search/, "const struct macroblock *x, struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv";
specialize qw/vp9_full_range_search/;

add_proto qw/void vp9_temporal_filter_apply/, "uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_size, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count";
specialize qw/vp9_temporal_filter_apply sse2/;

}
# end encoder functions
1;
