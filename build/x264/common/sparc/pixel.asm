/*****************************************************************************
 * pixel.asm: sparc pixel metrics
 *****************************************************************************
 * Copyright (C) 2005-2014 x264 project
 *
 * Authors: Phil Jensen <philj@csufresno.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/

! VIS optimized SAD for UltraSPARC

.text
.global x264_pixel_sad_8x8_vis
x264_pixel_sad_8x8_vis:
	save %sp, -120, %sp

	fzero %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	std %f12, [%fp-24]
	ld [%fp-20], %i0

	ret
	restore

.global x264_pixel_sad_8x16_vis
x264_pixel_sad_8x16_vis:
	save %sp, -120, %sp

	fzero %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
	add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	std %f12, [%fp-24]
	ld [%fp-20], %i0

	ret
	restore

.global x264_pixel_sad_16x8_vis
x264_pixel_sad_16x8_vis:
	save %sp, -120, %sp

	fzero %f12			! zero out the accumulator used for pdist

	sub %i1, 8, %i1			! reduce stride by 8, since we are moving forward 8 each block
	sub %i3, 8, %i3			! same here, reduce stride by 8

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	std %f12, [%fp-24]
	ld [%fp-20], %i0

	ret
	restore

.global x264_pixel_sad_16x16_vis
x264_pixel_sad_16x16_vis:
	save %sp, -120, %sp

	fzero %f12			! zero out the accumulator used for pdist

	sub %i1, 8, %i1			! reduce stride by 8, since we are moving forward 8 each block
	sub %i3, 8, %i3			! same here, reduce stride by 8

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, 8, %i0
	add %i2, 8, %i2
	pdist %f4, %f10, %f12

	alignaddr %i0, %g0, %l0	
	ldd [%l0], %f0
	ldd [%l0+8], %f2
	faligndata %f0, %f2, %f4

	alignaddr %i2, %g0, %l2
	ldd [%l2], %f6
	ldd [%l2+8], %f8
	faligndata %f6, %f8, %f10

	add %i0, %i1, %i0
        add %i2, %i3, %i2
	pdist %f4, %f10, %f12

	std %f12, [%fp-24]
	ld [%fp-20], %i0

	ret
	restore
