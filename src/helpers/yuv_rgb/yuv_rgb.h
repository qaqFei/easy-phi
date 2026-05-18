// Copyright 2016 Adrien Descamps
// Distributed under BSD 3-Clause License

// Provide optimized functions to convert images from 8bits yuv420 to rgb24 format

// There are a few slightly different variations of the YCbCr color space with different parameters that 
// change the conversion matrix.
// The three most common YCbCr color space, defined by BT.601, BT.709 and JPEG standard are implemented here.
// See the respective standards for details
// The matrix values used are derived from http://www.equasys.de/colorconversion.html

// YUV420 is stored as three separate channels, with U and V (Cb and Cr) subsampled by a 2 factor
// For conversion from yuv to rgb, no interpolation is done, and the same UV value are used for 4 rgb pixels. This 
// is suboptimal for image quality, but by far the fastest method.

// For all methods, width and height should be even, if not, the last row/column of the result image won't be affected.
// For sse methods, if the width if not divisable by 32, the last (width%32) pixels of each line won't be affected.

#include <stdint.h>

typedef enum
{
	YCBCR_JPEG,
	YCBCR_601,
	YCBCR_709
} YCbCrType;

#ifdef __cplusplus
extern "C" {
#endif
// rgba to yuv, standard c implementation
// alpha channel is ignored
void rgb32_yuv420_std(
	uint32_t width, uint32_t height, 
	const uint8_t *rgba, uint32_t rgba_stride, 
	uint8_t *y, uint8_t *u, uint8_t *v, uint32_t y_stride, uint32_t uv_stride, 
	YCbCrType yuv_type);

// rgba to yuv, sse implementation
// pointers must be 16 byte aligned, and strides must be divisible by 16
// alpha channel is ignored
void rgb32_yuv420_sse(
	uint32_t width, uint32_t height, 
	const uint8_t *rgba, uint32_t rgba_stride, 
	uint8_t *y, uint8_t *u, uint8_t *v, uint32_t y_stride, uint32_t uv_stride, 
	YCbCrType yuv_type);

// rgba to yuv, sse implementation
// pointers do not need to be 16 byte aligned
// alpha channel is ignored
void rgb32_yuv420_sseu(
	uint32_t width, uint32_t height, 
	const uint8_t *rgba, uint32_t rgba_stride, 
	uint8_t *y, uint8_t *u, uint8_t *v, uint32_t y_stride, uint32_t uv_stride, 
	YCbCrType yuv_type);

#ifdef __cplusplus
}
#endif
