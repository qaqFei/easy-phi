// Copyright 2016 Adrien Descamps
// Distributed under BSD 3-Clause License

#include "yuv_rgb.h"

#include <emmintrin.h>

#ifdef _MSC_VER
// MSVC does not have __SSE2__ macro
  #if (defined(_M_AMD64) || defined(_M_X64) || (_M_IX86_FP == 2))
    #define _YUVRGB_SSE2_
  #endif
#else
// For everything else than MSVC
  #ifdef __SSE2__
    #define _YUVRGB_SSE2_
  #endif // __SSE2__
#endif // _MSC_VER

uint8_t clamp(int16_t value)
{
	return value<0 ? 0 : (value>255 ? 255 : value);
}

// Definitions
//
// E'R, E'G, E'B, E'Y, E'Cb and E'Cr refer to the analog signals
// E'R, E'G, E'B and E'Y range is [0:1], while E'Cb and E'Cr range is [-0.5:0.5]
// R, G, B, Y, Cb and Cr refer to the digitalized values
// The digitalized values can use their full range ([0:255] for 8bit values),
// or a subrange (typically [16:235] for Y and [16:240] for CbCr).
// We assume here that RGB range is always [0:255], since it is the case for 
// most digitalized images.
// For 8bit values :
// * Y = round((YMax-YMin)*E'Y + YMin)
// * Cb = round((CbRange)*E'Cb + 128)
// * Cr = round((CrRange)*E'Cr + 128)
// Where *Min and *Max are the range of each channel
//
// In the analog domain , the RGB to YCbCr transformation is defined as:
// * E'Y = Rf*E'R + Gf*E'G + Bf*E'B
// Where Rf, Gf and Bf are constants defined in each standard, with 
// Rf + Gf + Bf = 1 (necessary to ensure that E'Y range is [0:1])
// * E'Cb = (E'B - E'Y) / CbNorm
// * E'Cr = (E'R - E'Y) / CrNorm
// Where CbNorm and CrNorm are constants, dependent of Rf, Gf, Bf, computed 
// to normalize to a [-0.5:0.5] range : CbNorm=2*(1-Bf) and CrNorm=2*(1-Rf)
//
// Algorithms
//
// Most operations will be made in a fixed point format for speed, using 
// N bits of precision. In next section the [x] convention is used for 
// a fixed point rounded value, that is (int being the c type conversion)
// * [x] = int(x*(2^N)+0.5)
// N can be different for each factor, we simply use the highest value
// that will not overflow in 16 bits intermediate variables.
//.
// For RGB to YCbCr conversion, we start by generating a pseudo Y value 
// (noted Y') in fixed point format, using the full range for now.
// * Y' = ([Rf]*R + [Gf]*G + [Bf]*B)>>N
// We can then compute Cb and Cr by
// * Cb = ((B - Y')*[CbRange/(255*CbNorm)])>>N + 128
// * Cr = ((R - Y')*[CrRange/(255*CrNorm)])>>N + 128
// And finally, we normalize Y to its digital range
// * Y = (Y'*[(YMax-YMin)/255])>>N + YMin
// 
// For YCbCr to RGB conversion, we first compute the full range Y' value :
// * Y' = ((Y-YMin)*[255/(YMax-YMin)])>>N
// We can then compute B and R values by :
// * B = ((Cb-128)*[(255*CbNorm)/CbRange])>>N + Y'
// * R = ((Cr-128)*[(255*CrNorm)/CrRange])>>N + Y'
// And finally, for G we know that:
// * G = (Y' - (Rf*R + Bf*B)) / Gf
// From above:
// * G = (Y' - Rf * ((Cr-128)*(255*CrNorm)/CrRange + Y') - Bf * ((Cb-128)*(255*CbNorm)/CbRange + Y')) / Gf
// Since 1-Rf-Bf=Gf, we can take Y' out of the division by Gf, and we get:
// * G = Y' - (Cr-128)*Rf/Gf*(255*CrNorm)/CrRange - (Cb-128)*Bf/Gf*(255*CbNorm)/CbRange
// That we can compute, with fixed point arithmetic, by
// * G = Y' - ((Cr-128)*[Rf/Gf*(255*CrNorm)/CrRange] + (Cb-128)*[Bf/Gf*(255*CbNorm)/CbRange])>>N
// 
// Note : in ITU-T T.871(JPEG), Y=Y', so that part could be optimized out


#define FIXED_POINT_VALUE(value, precision) ((int)(((value)*(1<<precision))+0.5))

// see above for description
typedef struct
{
	uint8_t r_factor;    // [Rf]
	uint8_t g_factor;    // [Rg]
	uint8_t b_factor;    // [Rb]
	uint8_t cb_factor;   // [CbRange/(255*CbNorm)]
	uint8_t cr_factor;   // [CrRange/(255*CrNorm)]
	uint8_t y_factor;    // [(YMax-YMin)/255]
	uint8_t y_offset;    // YMin
} RGB2YUVParam;


#define RGB2YUV_PARAM(Rf, Bf, YMin, YMax, CbCrRange) \
{.r_factor=FIXED_POINT_VALUE(Rf, 8), \
.g_factor=256-FIXED_POINT_VALUE(Rf, 8)-FIXED_POINT_VALUE(Bf, 8), \
.b_factor=FIXED_POINT_VALUE(Bf, 8), \
.cb_factor=FIXED_POINT_VALUE((CbCrRange/255.0)/(2.0*(1-Bf)), 8), \
.cr_factor=FIXED_POINT_VALUE((CbCrRange/255.0)/(2.0*(1-Rf)), 8), \
.y_factor=FIXED_POINT_VALUE((YMax-YMin)/255.0, 7), \
.y_offset=(uint8_t)YMin}

static const RGB2YUVParam RGB2YUV[3] = {
	// ITU-T T.871 (JPEG)
	RGB2YUV_PARAM(0.299, 0.114, 0.0, 255.0, 255.0),
	// ITU-R BT.601-7
	RGB2YUV_PARAM(0.299, 0.114, 16.0, 235.0, 224.0),
	// ITU-R BT.709-6
	RGB2YUV_PARAM(0.2126, 0.0722, 16.0, 235.0, 224.0)
};

void rgb32_yuv420_std(
	uint32_t width, uint32_t height, 
	const uint8_t *RGBA, uint32_t RGBA_stride, 
	uint8_t *Y, uint8_t *U, uint8_t *V, uint32_t Y_stride, uint32_t UV_stride, 
	YCbCrType yuv_type)
{
	const RGB2YUVParam *const param = &(RGB2YUV[yuv_type]);
	
	uint32_t x, y;
	for(y=0; y<(height-1); y+=2)
	{
		const uint8_t *rgb_ptr1=RGBA+y*RGBA_stride,
			*rgb_ptr2=RGBA+(y+1)*RGBA_stride;
		
		uint8_t *y_ptr1=Y+y*Y_stride,
			*y_ptr2=Y+(y+1)*Y_stride,
			*u_ptr=U+(y/2)*UV_stride,
			*v_ptr=V+(y/2)*UV_stride;
		
		for(x=0; x<(width-1); x+=2)
		{
			uint8_t y_tmp;
			int16_t u_tmp, v_tmp;
			
			y_tmp = (param->r_factor*rgb_ptr1[0] + param->g_factor*rgb_ptr1[1] + param->b_factor*rgb_ptr1[2])>>8;
			u_tmp = rgb_ptr1[2]-y_tmp;
			v_tmp = rgb_ptr1[0]-y_tmp;
			y_ptr1[0]=((y_tmp*param->y_factor)>>7) + param->y_offset;
			
			y_tmp = (param->r_factor*rgb_ptr1[4] + param->g_factor*rgb_ptr1[5] + param->b_factor*rgb_ptr1[6])>>8;
			u_tmp += rgb_ptr1[6]-y_tmp;
			v_tmp += rgb_ptr1[4]-y_tmp;
			y_ptr1[1]=((y_tmp*param->y_factor)>>7) + param->y_offset;

			y_tmp = (param->r_factor*rgb_ptr2[0] + param->g_factor*rgb_ptr2[1] + param->b_factor*rgb_ptr2[2])>>8;
			u_tmp += rgb_ptr2[2]-y_tmp;
			v_tmp += rgb_ptr2[0]-y_tmp;
			y_ptr2[0]=((y_tmp*param->y_factor)>>7) + param->y_offset;
			
			y_tmp = (param->r_factor*rgb_ptr2[4] + param->g_factor*rgb_ptr2[5] + param->b_factor*rgb_ptr2[6])>>8;
			u_tmp += rgb_ptr2[6]-y_tmp;
			v_tmp += rgb_ptr2[4]-y_tmp;
			y_ptr2[1]=((y_tmp*param->y_factor)>>7) + param->y_offset;

			u_ptr[0] = (((u_tmp>>2)*param->cb_factor)>>8) + 128;
			v_ptr[0] = (((v_tmp>>2)*param->cb_factor)>>8) + 128;
			
			rgb_ptr1 += 8;
			rgb_ptr2 += 8;
			y_ptr1 += 2;
			y_ptr2 += 2;
			u_ptr += 1;
			v_ptr += 1;
		}
	}
    	if (height & 1)
	{
		const uint8_t *rgb_ptr = RGBA + (height - 1) * RGBA_stride;
		uint8_t *y_ptr = Y + (height - 1) * Y_stride;
		uint8_t *u_ptr = U + ((height - 1) / 2) * UV_stride;
		uint8_t *v_ptr = V + ((height - 1) / 2) * UV_stride;
		
		for (x = 0; x < (width - 1); x += 2)
		{
			uint8_t y_tmp;
			int16_t u_tmp, v_tmp;
			
			y_tmp = (param->r_factor * rgb_ptr[0] + param->g_factor * rgb_ptr[1] + param->b_factor * rgb_ptr[2]) >> 8;
			u_tmp = rgb_ptr[2] - y_tmp;
			v_tmp = rgb_ptr[0] - y_tmp;
			y_ptr[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
			
			y_tmp = (param->r_factor * rgb_ptr[4] + param->g_factor * rgb_ptr[5] + param->b_factor * rgb_ptr[6]) >> 8;
			u_tmp += rgb_ptr[6] - y_tmp;
			v_tmp += rgb_ptr[4] - y_tmp;
			y_ptr[1] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
			
			u_ptr[0] = (((u_tmp >> 1) * param->cb_factor) >> 8) + 128;
			v_ptr[0] = (((v_tmp >> 1) * param->cr_factor) >> 8) + 128;
			
			rgb_ptr += 8;
			y_ptr += 2;
			u_ptr += 1;
			v_ptr += 1;
		}
		if (width & 1)
		{
			uint8_t y_tmp = (param->r_factor * rgb_ptr[0] + param->g_factor * rgb_ptr[1] + param->b_factor * rgb_ptr[2]) >> 8;
			int16_t u_tmp = rgb_ptr[2] - y_tmp;
			int16_t v_tmp = rgb_ptr[0] - y_tmp;
			y_ptr[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
			u_ptr[0] = (((u_tmp) * param->cb_factor) >> 8) + 128;
			v_ptr[0] = (((v_tmp) * param->cr_factor) >> 8) + 128;
		}
	}
	
	if (width & 1)
	{
		for (y = 0; y < (height & ~1); y += 2)
		{
			const uint8_t *rgb_ptr1 = RGBA + y * RGBA_stride + (width - 1) * 4;
			const uint8_t *rgb_ptr2 = RGBA + (y + 1) * RGBA_stride + (width - 1) * 4;
			uint8_t *y_ptr1 = Y + y * Y_stride + (width - 1);
			uint8_t *y_ptr2 = Y + (y + 1) * Y_stride + (width - 1);
			uint8_t *u_ptr = U + (y / 2) * UV_stride + (width - 1) / 2;
			uint8_t *v_ptr = V + (y / 2) * UV_stride + (width - 1) / 2;
			
			uint8_t y_tmp = (param->r_factor * rgb_ptr1[0] + param->g_factor * rgb_ptr1[1] + param->b_factor * rgb_ptr1[2]) >> 8;
			int16_t u_tmp = rgb_ptr1[2] - y_tmp;
			int16_t v_tmp = rgb_ptr1[0] - y_tmp;
			y_ptr1[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
			
			y_tmp = (param->r_factor * rgb_ptr2[0] + param->g_factor * rgb_ptr2[1] + param->b_factor * rgb_ptr2[2]) >> 8;
			u_tmp += rgb_ptr2[2] - y_tmp;
			v_tmp += rgb_ptr2[0] - y_tmp;
			y_ptr2[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
			
			u_ptr[0] = (((u_tmp >> 1) * param->cb_factor) >> 8) + 128;
			v_ptr[0] = (((v_tmp >> 1) * param->cr_factor) >> 8) + 128;
		}
	}
}


#ifdef _YUVRGB_SSE2_

// see rgba.txt
#define UNPACK_RGB32_32_STEP(RS1, RS2, RS3, RS4, RS5, RS6, RS7, RS8, RD1, RD2, RD3, RD4, RD5, RD6, RD7, RD8) \
RD1 = _mm_unpacklo_epi8(RS1, RS5); \
RD2 = _mm_unpackhi_epi8(RS1, RS5); \
RD3 = _mm_unpacklo_epi8(RS2, RS6); \
RD4 = _mm_unpackhi_epi8(RS2, RS6); \
RD5 = _mm_unpacklo_epi8(RS3, RS7); \
RD6 = _mm_unpackhi_epi8(RS3, RS7); \
RD7 = _mm_unpacklo_epi8(RS4, RS8); \
RD8 = _mm_unpackhi_epi8(RS4, RS8);


#define RGBA2YUV_32 \
	__m128i r_16, g_16, b_16; \
	__m128i y1_16, y2_16, cb1_16, cb2_16, cr1_16, cr2_16, Y, cb, cr; \
	__m128i tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8; \
	__m128i rgb1 = LOAD_SI128((const __m128i*)(rgb_ptr1)), \
		rgb2 = LOAD_SI128((const __m128i*)(rgb_ptr1+16)), \
		rgb3 = LOAD_SI128((const __m128i*)(rgb_ptr1+32)), \
		rgb4 = LOAD_SI128((const __m128i*)(rgb_ptr1+48)), \
		rgb5 = LOAD_SI128((const __m128i*)(rgb_ptr2)), \
		rgb6 = LOAD_SI128((const __m128i*)(rgb_ptr2+16)), \
		rgb7 = LOAD_SI128((const __m128i*)(rgb_ptr2+32)), \
		rgb8 = LOAD_SI128((const __m128i*)(rgb_ptr2+48)); \
	/* unpack rgb24 data to r, g and b data in separate channels*/ \
	/* see rgb.txt to get an idea of the algorithm, note that we only go to the next to last step*/ \
	/* here, because averaging in horizontal direction is easier like this*/ \
	/* The last step is applied further on the Y channel only*/ \
	UNPACK_RGB32_32_STEP(rgb1, rgb2, rgb3, rgb4, rgb5, rgb6, rgb7, rgb8, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8) \
	UNPACK_RGB32_32_STEP(tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, rgb1, rgb2, rgb3, rgb4, rgb5, rgb6, rgb7, rgb8) \
	UNPACK_RGB32_32_STEP(rgb1, rgb2, rgb3, rgb4, rgb5, rgb6, rgb7, rgb8, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8) \
	UNPACK_RGB32_32_STEP(tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, rgb1, rgb2, rgb3, rgb4, rgb5, rgb6, rgb7, rgb8) \
	/* first compute Y', (B-Y') and (R-Y'), in 16bits values, for the first line */ \
	/* Y is saved for each pixel, while only sums of (B-Y') and (R-Y') for pairs of adjacents pixels are saved*/ \
	r_16 = _mm_unpacklo_epi8(rgb1, _mm_setzero_si128()); \
	g_16 = _mm_unpacklo_epi8(rgb2, _mm_setzero_si128()); \
	b_16 = _mm_unpacklo_epi8(rgb3, _mm_setzero_si128()); \
	y1_16 = _mm_add_epi16(_mm_mullo_epi16(r_16, _mm_set1_epi16(param->r_factor)), \
		_mm_mullo_epi16(g_16, _mm_set1_epi16(param->g_factor))); \
	y1_16 = _mm_add_epi16(y1_16, _mm_mullo_epi16(b_16, _mm_set1_epi16(param->b_factor))); \
	y1_16 = _mm_srli_epi16(y1_16, 8); \
	cb1_16 = _mm_sub_epi16(b_16, y1_16); \
	cr1_16 = _mm_sub_epi16(r_16, y1_16); \
	r_16 = _mm_unpacklo_epi8(rgb5, _mm_setzero_si128()); \
	g_16 = _mm_unpacklo_epi8(rgb6, _mm_setzero_si128()); \
	b_16 = _mm_unpacklo_epi8(rgb7, _mm_setzero_si128()); \
	y2_16 = _mm_add_epi16(_mm_mullo_epi16(r_16, _mm_set1_epi16(param->r_factor)), \
		_mm_mullo_epi16(g_16, _mm_set1_epi16(param->g_factor))); \
	y2_16 = _mm_add_epi16(y2_16, _mm_mullo_epi16(b_16, _mm_set1_epi16(param->b_factor))); \
	y2_16 = _mm_srli_epi16(y2_16, 8); \
	cb1_16 = _mm_add_epi16(cb1_16, _mm_sub_epi16(b_16, y2_16)); \
	cr1_16 = _mm_add_epi16(cr1_16, _mm_sub_epi16(r_16, y2_16)); \
	/* Rescale Y' to Y, pack it to 8bit values and save it */ \
	y1_16 = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(y1_16, _mm_set1_epi16(param->y_factor)), 7), _mm_set1_epi16(param->y_offset)); \
	y2_16 = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(y2_16, _mm_set1_epi16(param->y_factor)), 7), _mm_set1_epi16(param->y_offset)); \
	Y = _mm_packus_epi16(y1_16, y2_16); \
	Y = _mm_unpackhi_epi8(_mm_slli_si128(Y, 8), Y); \
	SAVE_SI128((__m128i*)(y_ptr1), Y); \
	/* same for the second line, compute Y', (B-Y') and (R-Y'), in 16bits values */ \
	/* Y is saved for each pixel, while only sums of (B-Y') and (R-Y') for pairs of adjacents pixels are added to the previous values*/ \
	r_16 = _mm_unpackhi_epi8(rgb1, _mm_setzero_si128()); \
	g_16 = _mm_unpackhi_epi8(rgb2, _mm_setzero_si128()); \
	b_16 = _mm_unpackhi_epi8(rgb3, _mm_setzero_si128()); \
	y1_16 = _mm_add_epi16(_mm_mullo_epi16(r_16, _mm_set1_epi16(param->r_factor)), \
		_mm_mullo_epi16(g_16, _mm_set1_epi16(param->g_factor))); \
	y1_16 = _mm_add_epi16(y1_16, _mm_mullo_epi16(b_16, _mm_set1_epi16(param->b_factor))); \
	y1_16 = _mm_srli_epi16(y1_16, 8); \
	cb1_16 = _mm_add_epi16(cb1_16, _mm_sub_epi16(b_16, y1_16)); \
	cr1_16 = _mm_add_epi16(cr1_16, _mm_sub_epi16(r_16, y1_16)); \
	r_16 = _mm_unpackhi_epi8(rgb5, _mm_setzero_si128()); \
	g_16 = _mm_unpackhi_epi8(rgb6, _mm_setzero_si128()); \
	b_16 = _mm_unpackhi_epi8(rgb7, _mm_setzero_si128()); \
	y2_16 = _mm_add_epi16(_mm_mullo_epi16(r_16, _mm_set1_epi16(param->r_factor)), \
		_mm_mullo_epi16(g_16, _mm_set1_epi16(param->g_factor))); \
	y2_16 = _mm_add_epi16(y2_16, _mm_mullo_epi16(b_16, _mm_set1_epi16(param->b_factor))); \
	y2_16 = _mm_srli_epi16(y2_16, 8); \
	cb1_16 = _mm_add_epi16(cb1_16, _mm_sub_epi16(b_16, y2_16)); \
	cr1_16 = _mm_add_epi16(cr1_16, _mm_sub_epi16(r_16, y2_16)); \
	/* Rescale Y' to Y, pack it to 8bit values and save it */ \
	y1_16 = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(y1_16, _mm_set1_epi16(param->y_factor)), 7), _mm_set1_epi16(param->y_offset)); \
	y2_16 = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(y2_16, _mm_set1_epi16(param->y_factor)), 7), _mm_set1_epi16(param->y_offset)); \
	Y = _mm_packus_epi16(y1_16, y2_16); \
	Y = _mm_unpackhi_epi8(_mm_slli_si128(Y, 8), Y); \
	SAVE_SI128((__m128i*)(y_ptr2), Y); \
	/* Rescale Cb and Cr to their final range */ \
	cb1_16 = _mm_add_epi16(_mm_srai_epi16(_mm_mullo_epi16(_mm_srai_epi16(cb1_16, 2), _mm_set1_epi16(param->cb_factor)), 8), _mm_set1_epi16(128)); \
	cr1_16 = _mm_add_epi16(_mm_srai_epi16(_mm_mullo_epi16(_mm_srai_epi16(cr1_16, 2), _mm_set1_epi16(param->cr_factor)), 8), _mm_set1_epi16(128)); \
	\
	/* do the same again with next data */ \
	rgb1 = LOAD_SI128((const __m128i*)(rgb_ptr1+64)), \
	rgb2 = LOAD_SI128((const __m128i*)(rgb_ptr1+80)), \
	rgb3 = LOAD_SI128((const __m128i*)(rgb_ptr1+96)), \
	rgb4 = LOAD_SI128((const __m128i*)(rgb_ptr1+112)), \
	rgb5 = LOAD_SI128((const __m128i*)(rgb_ptr2+64)), \
	rgb6 = LOAD_SI128((const __m128i*)(rgb_ptr2+80)), \
	rgb7 = LOAD_SI128((const __m128i*)(rgb_ptr2+96)), \
	rgb8 = LOAD_SI128((const __m128i*)(rgb_ptr2+112)); \
	/* unpack rgb24 data to r, g and b data in separate channels*/ \
	/* see rgb.txt to get an idea of the algorithm, note that we only go to the next to last step*/ \
	/* here, because averaging in horizontal direction is easier like this*/ \
	/* The last step is applied further on the Y channel only*/ \
	UNPACK_RGB32_32_STEP(rgb1, rgb2, rgb3, rgb4, rgb5, rgb6, rgb7, rgb8, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8) \
	UNPACK_RGB32_32_STEP(tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, rgb1, rgb2, rgb3, rgb4, rgb5, rgb6, rgb7, rgb8) \
	UNPACK_RGB32_32_STEP(rgb1, rgb2, rgb3, rgb4, rgb5, rgb6, rgb7, rgb8, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8) \
	UNPACK_RGB32_32_STEP(tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, rgb1, rgb2, rgb3, rgb4, rgb5, rgb6, rgb7, rgb8) \
	/* first compute Y', (B-Y') and (R-Y'), in 16bits values, for the first line */ \
	/* Y is saved for each pixel, while only sums of (B-Y') and (R-Y') for pairs of adjacents pixels are saved*/ \
	r_16 = _mm_unpacklo_epi8(rgb1, _mm_setzero_si128()); \
	g_16 = _mm_unpacklo_epi8(rgb2, _mm_setzero_si128()); \
	b_16 = _mm_unpacklo_epi8(rgb3, _mm_setzero_si128()); \
	y1_16 = _mm_add_epi16(_mm_mullo_epi16(r_16, _mm_set1_epi16(param->r_factor)), \
		_mm_mullo_epi16(g_16, _mm_set1_epi16(param->g_factor))); \
	y1_16 = _mm_add_epi16(y1_16, _mm_mullo_epi16(b_16, _mm_set1_epi16(param->b_factor))); \
	y1_16 = _mm_srli_epi16(y1_16, 8); \
	cb2_16 = _mm_sub_epi16(b_16, y1_16); \
	cr2_16 = _mm_sub_epi16(r_16, y1_16); \
	r_16 = _mm_unpacklo_epi8(rgb5, _mm_setzero_si128()); \
	g_16 = _mm_unpacklo_epi8(rgb6, _mm_setzero_si128()); \
	b_16 = _mm_unpacklo_epi8(rgb7, _mm_setzero_si128()); \
	y2_16 = _mm_add_epi16(_mm_mullo_epi16(r_16, _mm_set1_epi16(param->r_factor)), \
		_mm_mullo_epi16(g_16, _mm_set1_epi16(param->g_factor))); \
	y2_16 = _mm_add_epi16(y2_16, _mm_mullo_epi16(b_16, _mm_set1_epi16(param->b_factor))); \
	y2_16 = _mm_srli_epi16(y2_16, 8); \
	cb2_16 = _mm_add_epi16(cb2_16, _mm_sub_epi16(b_16, y2_16)); \
	cr2_16 = _mm_add_epi16(cr2_16, _mm_sub_epi16(r_16, y2_16)); \
	/* Rescale Y' to Y, pack it to 8bit values and save it */ \
	y1_16 = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(y1_16, _mm_set1_epi16(param->y_factor)), 7), _mm_set1_epi16(param->y_offset)); \
	y2_16 = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(y2_16, _mm_set1_epi16(param->y_factor)), 7), _mm_set1_epi16(param->y_offset)); \
	Y = _mm_packus_epi16(y1_16, y2_16); \
	Y = _mm_unpackhi_epi8(_mm_slli_si128(Y, 8), Y); \
	SAVE_SI128((__m128i*)(y_ptr1+16), Y); \
	/* same for the second line, compute Y', (B-Y') and (R-Y'), in 16bits values */ \
	/* Y is saved for each pixel, while only sums of (B-Y') and (R-Y') for pairs of adjacents pixels are added to the previous values*/ \
	r_16 = _mm_unpackhi_epi8(rgb1, _mm_setzero_si128()); \
	g_16 = _mm_unpackhi_epi8(rgb2, _mm_setzero_si128()); \
	b_16 = _mm_unpackhi_epi8(rgb3, _mm_setzero_si128()); \
	y1_16 = _mm_add_epi16(_mm_mullo_epi16(r_16, _mm_set1_epi16(param->r_factor)), \
		_mm_mullo_epi16(g_16, _mm_set1_epi16(param->g_factor))); \
	y1_16 = _mm_add_epi16(y1_16, _mm_mullo_epi16(b_16, _mm_set1_epi16(param->b_factor))); \
	y1_16 = _mm_srli_epi16(y1_16, 8); \
	cb2_16 = _mm_add_epi16(cb2_16, _mm_sub_epi16(b_16, y1_16)); \
	cr2_16 = _mm_add_epi16(cr2_16, _mm_sub_epi16(r_16, y1_16)); \
	r_16 = _mm_unpackhi_epi8(rgb5, _mm_setzero_si128()); \
	g_16 = _mm_unpackhi_epi8(rgb6, _mm_setzero_si128()); \
	b_16 = _mm_unpackhi_epi8(rgb7, _mm_setzero_si128()); \
	y2_16 = _mm_add_epi16(_mm_mullo_epi16(r_16, _mm_set1_epi16(param->r_factor)), \
		_mm_mullo_epi16(g_16, _mm_set1_epi16(param->g_factor))); \
	y2_16 = _mm_add_epi16(y2_16, _mm_mullo_epi16(b_16, _mm_set1_epi16(param->b_factor))); \
	y2_16 = _mm_srli_epi16(y2_16, 8); \
	cb2_16 = _mm_add_epi16(cb2_16, _mm_sub_epi16(b_16, y2_16)); \
	cr2_16 = _mm_add_epi16(cr2_16, _mm_sub_epi16(r_16, y2_16)); \
	/* Rescale Y' to Y, pack it to 8bit values and save it */ \
	y1_16 = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(y1_16, _mm_set1_epi16(param->y_factor)), 7), _mm_set1_epi16(param->y_offset)); \
	y2_16 = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(y2_16, _mm_set1_epi16(param->y_factor)), 7), _mm_set1_epi16(param->y_offset)); \
	Y = _mm_packus_epi16(y1_16, y2_16); \
	Y = _mm_unpackhi_epi8(_mm_slli_si128(Y, 8), Y); \
	SAVE_SI128((__m128i*)(y_ptr2+16), Y); \
	/* Rescale Cb and Cr to their final range */ \
	cb2_16 = _mm_add_epi16(_mm_srai_epi16(_mm_mullo_epi16(_mm_srai_epi16(cb2_16, 2), _mm_set1_epi16(param->cb_factor)), 8), _mm_set1_epi16(128)); \
	cr2_16 = _mm_add_epi16(_mm_srai_epi16(_mm_mullo_epi16(_mm_srai_epi16(cr2_16, 2), _mm_set1_epi16(param->cr_factor)), 8), _mm_set1_epi16(128)); \
	/* Pack and save Cb Cr */ \
	cb = _mm_packus_epi16(cb1_16, cb2_16); \
	cr = _mm_packus_epi16(cr1_16, cr2_16); \
	SAVE_SI128((__m128i*)(u_ptr), cb); \
	SAVE_SI128((__m128i*)(v_ptr), cr);

void rgb32_yuv420_sse(uint32_t width, uint32_t height, 
	const uint8_t *RGBA, uint32_t RGBA_stride, 
	uint8_t *Y, uint8_t *U, uint8_t *V, uint32_t Y_stride, uint32_t UV_stride, 
	YCbCrType yuv_type)
{
	#define LOAD_SI128 _mm_load_si128
	#define SAVE_SI128 _mm_stream_si128
	const RGB2YUVParam *const param = &(RGB2YUV[yuv_type]);
	
	uint32_t x, y;
	for(y=0; y<(height-1); y+=2)
	{
		const uint8_t *rgb_ptr1=RGBA+y*RGBA_stride,
			*rgb_ptr2=RGBA+(y+1)*RGBA_stride;
		
		uint8_t *y_ptr1=Y+y*Y_stride,
			*y_ptr2=Y+(y+1)*Y_stride,
			*u_ptr=U+(y/2)*UV_stride,
			*v_ptr=V+(y/2)*UV_stride;
		
		for(x=0; x<(width-31); x+=32)
		{
			RGBA2YUV_32
			
			rgb_ptr1+=128;
			rgb_ptr2+=128;
			y_ptr1+=32;
			y_ptr2+=32;
			u_ptr+=16; 
			v_ptr+=16;
		}
        for (; x < (width - 1); x += 2)
        {
            const uint8_t *rgb_ptr1 = RGBA + y * RGBA_stride + x * 4;
            const uint8_t *rgb_ptr2 = RGBA + (y + 1) * RGBA_stride + x * 4;
            uint8_t *y_ptr1 = Y + y * Y_stride + x;
            uint8_t *y_ptr2 = Y + (y + 1) * Y_stride + x;
            uint8_t *u_ptr = U + (y / 2) * UV_stride + x / 2;
            uint8_t *v_ptr = V + (y / 2) * UV_stride + x / 2;
            
            uint8_t y_tmp;
            int16_t u_tmp, v_tmp;
            
            y_tmp = (param->r_factor * rgb_ptr1[0] + param->g_factor * rgb_ptr1[1] + param->b_factor * rgb_ptr1[2]) >> 8;
            u_tmp = rgb_ptr1[2] - y_tmp;
            v_tmp = rgb_ptr1[0] - y_tmp;
            y_ptr1[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            y_tmp = (param->r_factor * rgb_ptr1[4] + param->g_factor * rgb_ptr1[5] + param->b_factor * rgb_ptr1[6]) >> 8;
            u_tmp += rgb_ptr1[6] - y_tmp;
            v_tmp += rgb_ptr1[4] - y_tmp;
            y_ptr1[1] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            y_tmp = (param->r_factor * rgb_ptr2[0] + param->g_factor * rgb_ptr2[1] + param->b_factor * rgb_ptr2[2]) >> 8;
            u_tmp += rgb_ptr2[2] - y_tmp;
            v_tmp += rgb_ptr2[0] - y_tmp;
            y_ptr2[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            y_tmp = (param->r_factor * rgb_ptr2[4] + param->g_factor * rgb_ptr2[5] + param->b_factor * rgb_ptr2[6]) >> 8;
            u_tmp += rgb_ptr2[6] - y_tmp;
            v_tmp += rgb_ptr2[4] - y_tmp;
            y_ptr2[1] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            u_ptr[0] = (((u_tmp >> 2) * param->cb_factor) >> 8) + 128;
            v_ptr[0] = (((v_tmp >> 2) * param->cr_factor) >> 8) + 128;
        }
        
        if (width & 1)
        {
            const uint8_t *rgb_ptr1 = RGBA + y * RGBA_stride + (width - 1) * 4;
            const uint8_t *rgb_ptr2 = RGBA + (y + 1) * RGBA_stride + (width - 1) * 4;
            uint8_t *y_ptr1 = Y + y * Y_stride + (width - 1);
            uint8_t *y_ptr2 = Y + (y + 1) * Y_stride + (width - 1);
            uint8_t *u_ptr = U + (y / 2) * UV_stride + (width - 1) / 2;
            uint8_t *v_ptr = V + (y / 2) * UV_stride + (width - 1) / 2;
            
            uint8_t y_tmp = (param->r_factor * rgb_ptr1[0] + param->g_factor * rgb_ptr1[1] + param->b_factor * rgb_ptr1[2]) >> 8;
            int16_t u_tmp = rgb_ptr1[2] - y_tmp;
            int16_t v_tmp = rgb_ptr1[0] - y_tmp;
            y_ptr1[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            y_tmp = (param->r_factor * rgb_ptr2[0] + param->g_factor * rgb_ptr2[1] + param->b_factor * rgb_ptr2[2]) >> 8;
            u_tmp += rgb_ptr2[2] - y_tmp;
            v_tmp += rgb_ptr2[0] - y_tmp;
            y_ptr2[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            u_ptr[0] = (((u_tmp >> 1) * param->cb_factor) >> 8) + 128;
            v_ptr[0] = (((v_tmp >> 1) * param->cr_factor) >> 8) + 128;
        }
    }
	#undef LOAD_SI128
	#undef SAVE_SI128

    if (height & 1)
    {
        const uint8_t *rgb_ptr = RGBA + (height - 1) * RGBA_stride;
        uint8_t *y_ptr = Y + (height - 1) * Y_stride;
        uint8_t *u_ptr = U + ((height - 1) / 2) * UV_stride;
        uint8_t *v_ptr = V + ((height - 1) / 2) * UV_stride;
        
        uint32_t x_tail;
        for (x_tail = 0; x_tail < (width - 1); x_tail += 2)
        {
            uint8_t y_tmp;
            int16_t u_tmp, v_tmp;
            
            y_tmp = (param->r_factor * rgb_ptr[0] + param->g_factor * rgb_ptr[1] + param->b_factor * rgb_ptr[2]) >> 8;
            u_tmp = rgb_ptr[2] - y_tmp;
            v_tmp = rgb_ptr[0] - y_tmp;
            y_ptr[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            y_tmp = (param->r_factor * rgb_ptr[4] + param->g_factor * rgb_ptr[5] + param->b_factor * rgb_ptr[6]) >> 8;
            u_tmp += rgb_ptr[6] - y_tmp;
            v_tmp += rgb_ptr[4] - y_tmp;
            y_ptr[1] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            u_ptr[0] = (((u_tmp >> 1) * param->cb_factor) >> 8) + 128;
            v_ptr[0] = (((v_tmp >> 1) * param->cr_factor) >> 8) + 128;
            
            rgb_ptr += 8;
            y_ptr += 2;
            u_ptr += 1;
            v_ptr += 1;
        }
        if (width & 1)
        {
            uint8_t y_tmp = (param->r_factor * rgb_ptr[0] + param->g_factor * rgb_ptr[1] + param->b_factor * rgb_ptr[2]) >> 8;
            int16_t u_tmp = rgb_ptr[2] - y_tmp;
            int16_t v_tmp = rgb_ptr[0] - y_tmp;
            y_ptr[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            u_ptr[0] = (((u_tmp) * param->cb_factor) >> 8) + 128;
            v_ptr[0] = (((v_tmp) * param->cr_factor) >> 8) + 128;
        }
    }
}

void rgb32_yuv420_sseu(uint32_t width, uint32_t height, 
	const uint8_t *RGBA, uint32_t RGBA_stride, 
	uint8_t *Y, uint8_t *U, uint8_t *V, uint32_t Y_stride, uint32_t UV_stride, 
	YCbCrType yuv_type)
{
	#define LOAD_SI128 _mm_loadu_si128
	#define SAVE_SI128 _mm_storeu_si128
	const RGB2YUVParam *const param = &(RGB2YUV[yuv_type]);
	
	uint32_t x, y;
	for(y=0; y<(height-1); y+=2)
	{
		const uint8_t *rgb_ptr1=RGBA+y*RGBA_stride,
			*rgb_ptr2=RGBA+(y+1)*RGBA_stride;
		
		uint8_t *y_ptr1=Y+y*Y_stride,
			*y_ptr2=Y+(y+1)*Y_stride,
			*u_ptr=U+(y/2)*UV_stride,
			*v_ptr=V+(y/2)*UV_stride;
		
		for(x=0; x<(width-31); x+=32)
		{
			RGBA2YUV_32
			
			rgb_ptr1+=128;
			rgb_ptr2+=128;
			y_ptr1+=32;
			y_ptr2+=32;
			u_ptr+=16; 
			v_ptr+=16;
		}
        for (; x < (width - 1); x += 2)
        {
            const uint8_t *rgb_ptr1 = RGBA + y * RGBA_stride + x * 4;
            const uint8_t *rgb_ptr2 = RGBA + (y + 1) * RGBA_stride + x * 4;
            uint8_t *y_ptr1 = Y + y * Y_stride + x;
            uint8_t *y_ptr2 = Y + (y + 1) * Y_stride + x;
            uint8_t *u_ptr = U + (y / 2) * UV_stride + x / 2;
            uint8_t *v_ptr = V + (y / 2) * UV_stride + x / 2;
            
            uint8_t y_tmp;
            int16_t u_tmp, v_tmp;
            
            y_tmp = (param->r_factor * rgb_ptr1[0] + param->g_factor * rgb_ptr1[1] + param->b_factor * rgb_ptr1[2]) >> 8;
            u_tmp = rgb_ptr1[2] - y_tmp;
            v_tmp = rgb_ptr1[0] - y_tmp;
            y_ptr1[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            y_tmp = (param->r_factor * rgb_ptr1[4] + param->g_factor * rgb_ptr1[5] + param->b_factor * rgb_ptr1[6]) >> 8;
            u_tmp += rgb_ptr1[6] - y_tmp;
            v_tmp += rgb_ptr1[4] - y_tmp;
            y_ptr1[1] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            y_tmp = (param->r_factor * rgb_ptr2[0] + param->g_factor * rgb_ptr2[1] + param->b_factor * rgb_ptr2[2]) >> 8;
            u_tmp += rgb_ptr2[2] - y_tmp;
            v_tmp += rgb_ptr2[0] - y_tmp;
            y_ptr2[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            y_tmp = (param->r_factor * rgb_ptr2[4] + param->g_factor * rgb_ptr2[5] + param->b_factor * rgb_ptr2[6]) >> 8;
            u_tmp += rgb_ptr2[6] - y_tmp;
            v_tmp += rgb_ptr2[4] - y_tmp;
            y_ptr2[1] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            u_ptr[0] = (((u_tmp >> 2) * param->cb_factor) >> 8) + 128;
            v_ptr[0] = (((v_tmp >> 2) * param->cr_factor) >> 8) + 128;
        }
        
        if (width & 1)
        {
            const uint8_t *rgb_ptr1 = RGBA + y * RGBA_stride + (width - 1) * 4;
            const uint8_t *rgb_ptr2 = RGBA + (y + 1) * RGBA_stride + (width - 1) * 4;
            uint8_t *y_ptr1 = Y + y * Y_stride + (width - 1);
            uint8_t *y_ptr2 = Y + (y + 1) * Y_stride + (width - 1);
            uint8_t *u_ptr = U + (y / 2) * UV_stride + (width - 1) / 2;
            uint8_t *v_ptr = V + (y / 2) * UV_stride + (width - 1) / 2;
            
            uint8_t y_tmp = (param->r_factor * rgb_ptr1[0] + param->g_factor * rgb_ptr1[1] + param->b_factor * rgb_ptr1[2]) >> 8;
            int16_t u_tmp = rgb_ptr1[2] - y_tmp;
            int16_t v_tmp = rgb_ptr1[0] - y_tmp;
            y_ptr1[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            y_tmp = (param->r_factor * rgb_ptr2[0] + param->g_factor * rgb_ptr2[1] + param->b_factor * rgb_ptr2[2]) >> 8;
            u_tmp += rgb_ptr2[2] - y_tmp;
            v_tmp += rgb_ptr2[0] - y_tmp;
            y_ptr2[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            u_ptr[0] = (((u_tmp >> 1) * param->cb_factor) >> 8) + 128;
            v_ptr[0] = (((v_tmp >> 1) * param->cr_factor) >> 8) + 128;
        }
    }
	#undef LOAD_SI128
	#undef SAVE_SI128
    
    if (height & 1)
    {
        const uint8_t *rgb_ptr = RGBA + (height - 1) * RGBA_stride;
        uint8_t *y_ptr = Y + (height - 1) * Y_stride;
        uint8_t *u_ptr = U + ((height - 1) / 2) * UV_stride;
        uint8_t *v_ptr = V + ((height - 1) / 2) * UV_stride;
        
        uint32_t x_tail;
        for (x_tail = 0; x_tail < (width - 1); x_tail += 2)
        {
            uint8_t y_tmp;
            int16_t u_tmp, v_tmp;
            
            y_tmp = (param->r_factor * rgb_ptr[0] + param->g_factor * rgb_ptr[1] + param->b_factor * rgb_ptr[2]) >> 8;
            u_tmp = rgb_ptr[2] - y_tmp;
            v_tmp = rgb_ptr[0] - y_tmp;
            y_ptr[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            y_tmp = (param->r_factor * rgb_ptr[4] + param->g_factor * rgb_ptr[5] + param->b_factor * rgb_ptr[6]) >> 8;
            u_tmp += rgb_ptr[6] - y_tmp;
            v_tmp += rgb_ptr[4] - y_tmp;
            y_ptr[1] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            
            u_ptr[0] = (((u_tmp >> 1) * param->cb_factor) >> 8) + 128;
            v_ptr[0] = (((v_tmp >> 1) * param->cr_factor) >> 8) + 128;
            
            rgb_ptr += 8;
            y_ptr += 2;
            u_ptr += 1;
            v_ptr += 1;
        }
        if (width & 1)
        {
            uint8_t y_tmp = (param->r_factor * rgb_ptr[0] + param->g_factor * rgb_ptr[1] + param->b_factor * rgb_ptr[2]) >> 8;
            int16_t u_tmp = rgb_ptr[2] - y_tmp;
            int16_t v_tmp = rgb_ptr[0] - y_tmp;
            y_ptr[0] = ((y_tmp * param->y_factor) >> 7) + param->y_offset;
            u_ptr[0] = (((u_tmp) * param->cb_factor) >> 8) + 128;
            v_ptr[0] = (((v_tmp) * param->cr_factor) >> 8) + 128;
        }
    }
}

#endif
