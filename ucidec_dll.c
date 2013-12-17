#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jni.h>
#include "libavcodec/avcodec.h"
#include "libavcodec/h264.h"
#include "libswscale/swscale.h"
#include "libswscale/swscale_internal.h"
#include "color1.h"
#include "define.h"

#ifdef _MSC_VER
#pragma intrinsic(memcpy, memset, strlen, strcpy)
#pragma warning(disable:4100)	// unreferenced formal parameter
#endif

#undef malloc
#undef free
#undef strncpy

#define GFP_READ	1
#define GFP_WRITE	2
#define GFP_RGB 	0
#define GFP_BGR 	1
#define GFP_GREY	5

CRITICAL_SECTION	g_cs;
AVFrame*			g_frame			= 0;
AVCodecContext*		g_context		= 0;
SwsContext*			g_swsctx		= 0;
const int*			g_cs_table		= 0;
//const int			g_cs_table[4]	= { 104611, 132130, 25748, 53290 }; // custom for uci
AVPacket			g_packet;

// UCI格式图像解码,目前只支持输出24位BGR和32位的BGRA,返回0表示调用成功,负数表示错误,不支持多线程同时访问
__declspec(dllexport) int __stdcall UCIDecode(
	const void* src,	// 输入UCI数据指针(不能传入null,其它指针参数可以传入null表示不需要输出)
	int 		srclen, // 输入UCI数据长度
	void**		dst,	// 输出RAW数据的指针(BGR或BGRA格式)
	int*		stride, // 输出RAW数据的行间字节跨度(dst不为null时,stride不能传入null)
	int*		width,	// 输出图像的宽度值
	int*		height, // 输出图像的高度值
	int*		bit)	// 输出图像的bpp值(每像素位数)
{
	extern AVCodec ff_h264_decoder;
	int ret = 0;
	U8* frame0 = 0;
	U8* frame1 = 0;
	U8* frame_data[4];
	int frame_size[4];
	int w, h, b, m;
	int ww, hh;
	int size, hasimg, bit10;
	U8* psrc, *pdst;
	const U8* srcend = (const U8*)src + srclen;

	if(dst	 ) *dst    = 0;
	if(stride) *stride = 0;
	if(width ) *width  = 0;
	if(height) *height = 0;
	if(bit	 ) *bit    = 0;
	if(!src || srclen < 0 || dst && !stride) return -1;
	if(srclen < 12) return -2;
	if((*(U32*)src & 0xffffff) != *(U32*)"UCI") return -3;
	switch(*((U8*)src + 3))
	{
	case '3':	b = 24; m = 0; break; // part range, YUV420
	case '4':	b = 32; m = 0; break; // part range, YUV420+A
	case 'T':	b = 24; m = 1; break; // part range, Y+U+V(420)
	case 'Q':	b = 32; m = 1; break; // part range, Y+U+V(420)+A
	case 0x20:	b = 24; m = 2; break; // full range, YUV420
	case 0x21:	b = 32; m = 2; break; // full range, YUV420+A
	case 0x40:	b = 24; m = 3; break; // full range, YUV444
	case 0x41:	b = 32; m = 3; break; // full range, YUV444+A
	default: return -4;
	}
	w = *(int*)((U8*)src + 4);
	h = *(int*)((U8*)src + 8);

	if(width ) *width  = w;
	if(height) *height = h;
	if(bit	 ) *bit    = b;
	if(stride) *stride = (m == 1 ? w : (w+7)&0xfffffff8) * (b/8);
	if(!dst) return 0;
	if(srclen < 12 + 4) return -5;
	ww = w + (w & ((m & 1) ^ 1));
	hh = h + (h & ((m & 1) ^ 1));

	frame_data[0] = (U8*)src + 12 + 4;
	frame_size[0] = *(int*)((U8*)src + 12);
	if(m != 1)
	{
		if(b == 24)
		{
			if(frame_size[0] < 0 || frame_data[0] + frame_size[0] > srcend)		return -10;
		}
		else
		{
			if(frame_size[0] < 0 || frame_data[0] + frame_size[0] + 4 > srcend) return -11;
			frame_data[3] = frame_data[0] + frame_size[0] + 4;
			frame_size[3] = *(int*)(frame_data[0] + frame_size[0]);
			if(frame_size[3] < 0 || frame_data[3] + frame_size[3] > srcend)		return -12;
		}
	}
	else
	{
		if(frame_size[0] < 0 || frame_data[0] + frame_size[0] + 4 > srcend)		return -13;
		frame_data[1] = frame_data[0] + frame_size[0] + 4;
		frame_size[1] = *(int*)(frame_data[0] + frame_size[0]);
		if(frame_size[1] < 0 || frame_data[1] + frame_size[1] + 4 > srcend)		return -14;
		frame_data[2] = frame_data[1] + frame_size[1] + 4;
		frame_size[2] = *(int*)(frame_data[1] + frame_size[1]);
		if(b == 24)
		{
			if(frame_size[2] < 0 || frame_data[2] + frame_size[2] > srcend)		return -15;
		}
		else
		{
			if(frame_size[2] < 0 || frame_data[2] + frame_size[2] + 4 > srcend) return -16;
			frame_data[3] = frame_data[2] + frame_size[2] + 4;
			frame_size[3] = *(int*)(frame_data[2] + frame_size[2]);
			if(frame_size[3] < 0 || frame_data[3] + frame_size[3] > srcend)		return -17;
		}
	}

	EnterCriticalSection(&g_cs);
	if(!g_frame && !(g_frame = avcodec_alloc_frame()))							{ ret = -20; goto end_; }
	if(!g_context && !(g_context = avcodec_alloc_context3(&ff_h264_decoder)))	{ ret = -21; goto end_; }
	g_context->flags &= ~CODEC_FLAG_EMU_EDGE;
	if(av_log_get_level() >= AV_LOG_DEBUG) g_context->debug = -1;
	if(avcodec_open2(g_context, &ff_h264_decoder, 0) < 0)						{ ret = -22; goto end_; }
	g_context->flags &= ~CODEC_FLAG_EMU_EDGE;
	g_packet.data = frame_data[0];
	g_packet.size = frame_size[0];
	size = avcodec_decode_video2(g_context, g_frame, &hasimg, &g_packet);
	if(size <= 0)																{ ret = -100 + size; goto end_; }
	if(!hasimg) 																{ ret = -23; goto end_; }
	if(g_context->width < ww || g_context->height < hh)							{ ret = -24; goto end_; }
	if(!g_frame->data[0] || m != 1 && (!g_frame->data[1] || !g_frame->data[2]))	{ ret = -25; goto end_; }
	if(g_frame->linesize[1] != g_frame->linesize[2])							{ ret = -26; goto end_; }
	bit10 = ((H264Context*)g_context->priv_data)->sps.bit_depth_luma;
	if(bit10 == 8) bit10 = 0; else if(bit10 != 10)								{ ret = -27; goto end_; }

	if(m != 1)
	{
		enum AVPixelFormat pfsrc, pfdst = (b == 24 ? AV_PIX_FMT_BGR24 : AV_PIX_FMT_BGRA);
		if(m != 3)	pfsrc = (bit10 ? AV_PIX_FMT_YUV420P10LE : AV_PIX_FMT_YUV420P);
		else		pfsrc = (bit10 ? AV_PIX_FMT_YUV444P10LE : AV_PIX_FMT_YUV444P);
		if(!(*dst = malloc(*stride * hh)))						{ ret = -28; goto end_; }
		g_swsctx = sws_getCachedContext(g_swsctx, ww, hh, pfsrc, ww, hh, pfdst, SWS_LANCZOS, 0, 0, 0);
		if(!g_swsctx)											{ ret = -29; goto end_; }
		sws_setColorspaceDetails(g_swsctx, g_cs_table, m != 0, g_cs_table, m != 0, 0, 1 << 16, 1 << 16);
		if(sws_scale(g_swsctx, g_frame->data, g_frame->linesize, 0, hh, dst, stride) != hh) { ret = -30; goto end_; }
	}
	else
	{
		if(!(frame0 = (U8*)malloc(g_frame->linesize[0] * h)))	{ ret = -31; goto end_; }
		memcpy(frame0, g_frame->data[0], g_frame->linesize[0] * h);
		frame_data[0] = frame0;
		frame_size[0] = g_frame->linesize[0];
		g_packet.data = frame_data[1];
		g_packet.size = frame_size[1];
		size = avcodec_decode_video2(g_context, g_frame, &hasimg, &g_packet);
		if(size <= 0)											{ ret = -200 + size; goto end_; }
		if(!hasimg) 											{ ret = -32; goto end_; }
		if(g_context->width < ww || g_context->height < hh)		{ ret = -33; goto end_; }
		if(!g_frame->data[0])									{ ret = -34; goto end_; }
		if(!(frame1 = (U8*)malloc(g_frame->linesize[0] * h)))	{ ret = -35; goto end_; }
		memcpy(frame1, g_frame->data[0], g_frame->linesize[0] * h);
		frame_data[1] = frame1;
		frame_size[1] = g_frame->linesize[0];
		g_packet.data = frame_data[2];
		g_packet.size = frame_size[2];
		size = avcodec_decode_video2(g_context, g_frame, &hasimg, &g_packet);
		if(size <= 0)											{ ret = -300 + size; goto end_; }
		if(!hasimg) 											{ ret = -36; goto end_; }
		if(g_context->width < ww || g_context->height < hh)		{ ret = -37; goto end_; }
		if(!g_frame->data[0])									{ ret = -38; goto end_; }
		frame_data[2] = g_frame->data[0];
		frame_size[2] = g_frame->linesize[0];
		if(!(*dst = malloc(*stride * h)))						{ ret = -39; goto end_; }
		if(b == 24) YUV444_BGR (*dst, w * 3, frame_data, frame_size, w, h);
		else		YUV444_BGRA(*dst, w * 4, frame_data, frame_size, w, h);
	}

	if(b == 32)
	{
		avcodec_close(g_context);
		if(!(g_context = avcodec_alloc_context3(&ff_h264_decoder)))	{ ret = -40; goto end_; }
		if(av_log_get_level() >= AV_LOG_DEBUG) g_context->debug = -1;
		if(avcodec_open2(g_context, &ff_h264_decoder, 0) < 0)	{ ret = -41; goto end_; }
		g_packet.data = frame_data[3];
		g_packet.size = frame_size[3];
		size = avcodec_decode_video2(g_context, g_frame, &hasimg, &g_packet);
		if(size <= 0)											{ ret = -400 + size; goto end_; }
		if(!hasimg) 											{ ret = -42; goto end_; }
		if(g_context->width < ww || g_context->height < hh)		{ ret = -43; goto end_; }
		if(!g_frame->data[0])									{ ret = -44; goto end_; }
		psrc = g_frame->data[0];
		pdst = (U8*)*dst + 3;
		if(!bit10)
			for(; h; --h)
			{
				for(m = 0; m < w; ++m)
					pdst[m * 4] = psrc[m];
				psrc += g_frame->linesize[0];
				pdst += *stride;
			}
		else
			for(; h; --h)
			{
				for(m = 0; m < w; ++m)
					pdst[m * 4] = ((U16*)psrc)[m] >> 2;
				psrc += g_frame->linesize[0];
				pdst += *stride;
			}
	}

end_:
	if(g_context) { avcodec_close(g_context); g_context = 0; }
	LeaveCriticalSection(&g_cs);
	if(ret < 0 && *dst) { free(*dst); *dst = 0; }
	if(frame1) free(frame1);
	if(frame0) free(frame0);
	return ret;
}

// 释放UCIDecode输出的RAW数据指针所指的内存区
__declspec(dllexport) void __stdcall UCIFree(void* p)
{
	if(p) free(p);
}

// 设置ffmpeg的debug输出级别
__declspec(dllexport) void __stdcall UCIDebug(int level)
{
	av_log_set_level(level);
}

BOOL __stdcall DllMain(HMODULE hm, DWORD reason, LPVOID dummy)
{
	dummy;
	if(reason == DLL_PROCESS_ATTACH)
	{
		extern AVCodec ff_h264_decoder;
		DisableThreadLibraryCalls(hm);
		InitializeCriticalSection(&g_cs);
		av_log_set_level(AV_LOG_PANIC);
		g_cs_table = sws_getCoefficients(SWS_CS_ITU601);
		memset(&g_packet, 0, sizeof(g_packet));
		g_packet.pts = AV_NOPTS_VALUE;
		g_packet.dts = AV_NOPTS_VALUE;
		g_packet.pos = -1;
		avcodec_register(&ff_h264_decoder);
	}
	else if(reason == DLL_PROCESS_DETACH)
	{
		if(g_swsctx)	{ sws_freeContext(g_swsctx);	g_swsctx = 0; }
		if(g_context)	{ av_free(g_context);			g_context = 0; }
		if(g_frame)		{ av_free(g_frame);				g_frame	= 0; }
		DeleteCriticalSection(&g_cs);
	}
	return TRUE;
}

// for XnView

typedef struct
{
	U8 red[256];
	U8 green[256];
	U8 blue[256];
}SColorMap;

typedef struct
{
	U8* data;
	U32 size;
	int width;
	int height;
	int bpp;
	int yuv444;
	int linesize;
	int linepos;
	U8* image;
}SImageParam;

// UCI格式图像解码,目前只支持输出24位BGR和32位的BGRA,返回0表示调用成功,负数表示错误,不支持多线程同时访问
static int __stdcall UCIDecode4XnView(
	const void* src,	// 输入UCI数据指针(不能传入null,其它指针参数可以传入null表示不需要输出)
	int 		srclen, // 输入UCI数据长度
	void**		dst,	// 输出RAW数据的指针(BGR或BGRA格式)
	int*		stride, // 输出RAW数据的行间字节跨度(dst不为null时,stride不能传入null)
	int*		width,	// 输出图像的宽度值
	int*		height, // 输出图像的高度值
	int*		bit)	// 输出图像的bpp值(每像素位数)
{
	extern AVCodec ff_h264_decoder;
	int ret = 0;
	U8* frame0 = 0;
	U8* frame1 = 0;
	U8* frame_data[4];
	int frame_size[4];
	int w, h, b, m;
	int ww, hh;
	int size, hasimg, bit10;
	U8* psrc, *pdst;
	const U8* srcend = (const U8*)src + srclen;

	if(dst	 ) *dst    = 0;
	if(stride) *stride = 0;
	if(width ) *width  = 0;
	if(height) *height = 0;
	if(bit	 ) *bit    = 0;
	if(!src || srclen < 0 || dst && !stride) return -1;
	if(srclen < 12) return -2;
	if((*(U32*)src & 0xffffff) != *(U32*)"UCI") return -3;
	switch(*((U8*)src + 3))
	{
	case '3':	b = 24; m = 0; break; // part range, YUV420
	case '4':	b = 32; m = 0; break; // part range, YUV420+A
	case 'T':	b = 24; m = 1; break; // part range, Y+U+V(420)
	case 'Q':	b = 32; m = 1; break; // part range, Y+U+V(420)+A
	case 0x20:	b = 24; m = 2; break; // full range, YUV420
	case 0x21:	b = 32; m = 2; break; // full range, YUV420+A
	case 0x40:	b = 24; m = 3; break; // full range, YUV444
	case 0x41:	b = 32; m = 3; break; // full range, YUV444+A
	default: return -4;
	}
	w = *(int*)((U8*)src + 4);
	h = *(int*)((U8*)src + 8);

	if(width ) *width  = w;
	if(height) *height = h;
	if(bit	 ) *bit    = b;
	if(stride) *stride = (m == 1 ? w : (w+7)&0xfffffff8) * (b/8);
	if(!dst) return 0;
	if(srclen < 12 + 4) return -5;
	ww = w + (w & ((m & 1) ^ 1));
	hh = h + (h & ((m & 1) ^ 1));

	frame_data[0] = (U8*)src + 12 + 4;
	frame_size[0] = *(int*)((U8*)src + 12);
	if(m != 1)
	{
		if(b == 24)
		{
			if(frame_size[0] < 0 || frame_data[0] + frame_size[0] > srcend) 	return -10;
		}
		else
		{
			if(frame_size[0] < 0 || frame_data[0] + frame_size[0] + 4 > srcend) return -11;
			frame_data[3] = frame_data[0] + frame_size[0] + 4;
			frame_size[3] = *(int*)(frame_data[0] + frame_size[0]);
			if(frame_size[3] < 0 || frame_data[3] + frame_size[3] > srcend) 	return -12;
		}
	}
	else
	{
		if(frame_size[0] < 0 || frame_data[0] + frame_size[0] + 4 > srcend) 	return -13;
		frame_data[1] = frame_data[0] + frame_size[0] + 4;
		frame_size[1] = *(int*)(frame_data[0] + frame_size[0]);
		if(frame_size[1] < 0 || frame_data[1] + frame_size[1] + 4 > srcend) 	return -14;
		frame_data[2] = frame_data[1] + frame_size[1] + 4;
		frame_size[2] = *(int*)(frame_data[1] + frame_size[1]);
		if(b == 24)
		{
			if(frame_size[2] < 0 || frame_data[2] + frame_size[2] > srcend) 	return -15;
		}
		else
		{
			if(frame_size[2] < 0 || frame_data[2] + frame_size[2] + 4 > srcend) return -16;
			frame_data[3] = frame_data[2] + frame_size[2] + 4;
			frame_size[3] = *(int*)(frame_data[2] + frame_size[2]);
			if(frame_size[3] < 0 || frame_data[3] + frame_size[3] > srcend) 	return -17;
		}
	}

	EnterCriticalSection(&g_cs);
	if(!g_frame && !(g_frame = avcodec_alloc_frame()))							{ ret = -20; goto end_; }
	if(!g_context && !(g_context = avcodec_alloc_context3(&ff_h264_decoder)))	{ ret = -21; goto end_; }
	if(av_log_get_level() >= AV_LOG_DEBUG) g_context->debug = -1;
	if(avcodec_open2(g_context, &ff_h264_decoder, 0) < 0)						{ ret = -22; goto end_; }
	g_packet.data = frame_data[0];
	g_packet.size = frame_size[0];
	size = avcodec_decode_video2(g_context, g_frame, &hasimg, &g_packet);
	if(size <= 0)																{ ret = -100 + size; goto end_; }
	if(!hasimg) 																{ ret = -23; goto end_; }
	if(g_context->width < ww || g_context->height < hh)							{ ret = -24; goto end_; }
	if(!g_frame->data[0] || m != 1 && (!g_frame->data[1] || !g_frame->data[2]))	{ ret = -25; goto end_; }
	if(g_frame->linesize[1] != g_frame->linesize[2])							{ ret = -26; goto end_; }
	bit10 = ((H264Context*)g_context->priv_data)->sps.bit_depth_luma;
	if(bit10 == 8) bit10 = 0; else if(bit10 != 10)								{ ret = -27; goto end_; }

	if(m != 1)
	{
		enum AVPixelFormat pfsrc, pfdst = (b == 24 ? AV_PIX_FMT_BGR24 : AV_PIX_FMT_BGRA); U8* dst1[1];
		if(m != 3)	pfsrc = (bit10 ? AV_PIX_FMT_YUV420P10LE : AV_PIX_FMT_YUV420P);
		else		pfsrc = (bit10 ? AV_PIX_FMT_YUV444P10LE : AV_PIX_FMT_YUV444P);
		if(!(*dst = malloc(*stride * hh + 4)))					{ ret = -28; goto end_; }
		g_swsctx = sws_getCachedContext(g_swsctx, ww, hh, pfsrc, ww, hh, pfdst, SWS_LANCZOS, 0, 0, 0);
		if(!g_swsctx)											{ ret = -29; goto end_; }
		sws_setColorspaceDetails(g_swsctx, g_cs_table, m != 0, g_cs_table, m != 0, 0, 1 << 16, 1 << 16);
		dst1[0] = (U8*)*dst + 1;
		if(sws_scale(g_swsctx, g_frame->data, g_frame->linesize, 0, hh, (b == 24 ? dst : dst1), stride) != hh) { ret = -30; goto end_; }
	}
	else
	{
		if(!(frame0 = (U8*)malloc(g_frame->linesize[0] * h)))	{ ret = -31; goto end_; }
		memcpy(frame0, g_frame->data[0], g_frame->linesize[0] * h);
		frame_data[0] = frame0;
		frame_size[0] = g_frame->linesize[0];
		g_packet.data = frame_data[1];
		g_packet.size = frame_size[1];
		size = avcodec_decode_video2(g_context, g_frame, &hasimg, &g_packet);
		if(size <= 0)											{ ret = -200 + size; goto end_; }
		if(!hasimg) 											{ ret = -32; goto end_; }
		if(g_context->width < ww || g_context->height < hh)		{ ret = -33; goto end_; }
		if(!g_frame->data[0])									{ ret = -34; goto end_; }
		if(!(frame1 = (U8*)malloc(g_frame->linesize[0] * h)))	{ ret = -35; goto end_; }
		memcpy(frame1, g_frame->data[0], g_frame->linesize[0] * h);
		frame_data[1] = frame1;
		frame_size[1] = g_frame->linesize[0];
		g_packet.data = frame_data[2];
		g_packet.size = frame_size[2];
		size = avcodec_decode_video2(g_context, g_frame, &hasimg, &g_packet);
		if(size <= 0)											{ ret = -300 + size; goto end_; }
		if(!hasimg) 											{ ret = -36; goto end_; }
		if(g_context->width < ww || g_context->height < hh)		{ ret = -37; goto end_; }
		if(!g_frame->data[0])									{ ret = -38; goto end_; }
		frame_data[2] = g_frame->data[0];
		frame_size[2] = g_frame->linesize[0];
		if(!(*dst = malloc(*stride * h)))						{ ret = -39; goto end_; }
		if(b == 24) YUV444_BGR (*dst	   , w * 3, frame_data, frame_size, w, h);
		else		YUV444_BGRA((U8*)*dst+1, w * 4, frame_data, frame_size, w, h);
	}

	if(b == 32)
	{
		avcodec_close(g_context);
		if(!(g_context = avcodec_alloc_context3(&ff_h264_decoder)))	{ ret = -40; goto end_; }
		if(av_log_get_level() >= AV_LOG_DEBUG) g_context->debug = -1;
		if(avcodec_open2(g_context, &ff_h264_decoder, 0) < 0)	{ ret = -41; goto end_; }
		g_packet.data = frame_data[3];
		g_packet.size = frame_size[3];
		size = avcodec_decode_video2(g_context, g_frame, &hasimg, &g_packet);
		if(size <= 0)											{ ret = -400 + size; goto end_; }
		if(!hasimg) 											{ ret = -42; goto end_; }
		if(g_context->width < ww || g_context->height < hh)		{ ret = -43; goto end_; }
		if(!g_frame->data[0])									{ ret = -44; goto end_; }
		psrc = g_frame->data[0];
		pdst = (U8*)*dst;
		if(!bit10)
			for(; h; --h)
			{
				for(m = 0; m < w; ++m)
					pdst[m * 4] = psrc[m];
				psrc += g_frame->linesize[0];
				pdst += *stride;
			}
		else
			for(; h; --h)
			{
				for(m = 0; m < w; ++m)
					pdst[m * 4] = ((U16*)psrc)[m] >> 2;
				psrc += g_frame->linesize[0];
				pdst += *stride;
			}
	}

end_:
	if(g_context) { avcodec_close(g_context); g_context = 0; }
	LeaveCriticalSection(&g_cs);
	if(ret < 0 && *dst) { free(*dst); *dst = 0; }
	if(frame1) free(frame1);
	if(frame0) free(frame0);
	return ret;
}

__declspec(dllexport) BOOL __stdcall gfpGetPluginInfo(DWORD version, char* label, int label_max_size, char* extension, int extension_max_size, int* support)
{
	if(version != 0x0002)
		return FALSE;

	strncpy(label, "Ultra Compact Image", label_max_size);
	strncpy(extension, "uci", extension_max_size);

	*support = GFP_READ;

	return TRUE;
}

__declspec(dllexport) void* __stdcall gfpLoadPictureInit(const char* filename)
{
	SImageParam* ip = 0;
	FILE* fp = 0;
	U32 t;

	if(!(ip = (SImageParam*)malloc(sizeof(SImageParam)))) goto err_;
	memset(ip, 0, sizeof(SImageParam));
	if(!(fp = fopen(filename, "rb"))) goto err_;
	if(fread(&t, 4, 1, fp) != 1) goto err_;
	if((t & 0xffffff) != *(U32*)"UCI") goto err_;
	if(fseek(fp, 12, SEEK_SET)) goto err_;
	ip->size = 12;
	switch(t >>= 24)
	{
	case '3':	ip->yuv444 = 0; goto sec1_;
	case '4':	ip->yuv444 = 0; goto sec2_;
	case 'T':	ip->yuv444 = 1; goto sec3_;
	case 'Q':	ip->yuv444 = 1; goto sec4_;
	case 0x20:	ip->yuv444 = 0; goto sec1_;
	case 0x21:	ip->yuv444 = 0; goto sec2_;
	case 0x40:	ip->yuv444 = 1; goto sec1_;
	case 0x41:	ip->yuv444 = 1; goto sec2_;
	default: goto err_;
	}

sec4_:
	if(fread(&t, 4, 1, fp) != 1) goto err_;
	if(t >= 0x7ffffffc) goto err_;
	ip->size += 4 + t;
	if(ip->size >= 0x80000000) goto err_;
	if(fseek(fp, ip->size, SEEK_SET)) goto err_;

sec3_:
	if(fread(&t, 4, 1, fp) != 1) goto err_;
	if(t >= 0x7ffffffc) goto err_;
	ip->size += 4 + t;
	if(ip->size >= 0x80000000) goto err_;
	if(fseek(fp, ip->size, SEEK_SET)) goto err_;

sec2_:
	if(fread(&t, 4, 1, fp) != 1) goto err_;
	if(t >= 0x7ffffffc) goto err_;
	ip->size += 4 + t;
	if(ip->size >= 0x80000000) goto err_;
	if(fseek(fp, ip->size, SEEK_SET)) goto err_;

sec1_:
	if(fread(&t, 4, 1, fp) != 1) goto err_;
	if(t >= 0x7ffffffc) goto err_;
	ip->size += 4 + t;
	if(ip->size >= 0x80000000) goto err_;
	if(fseek(fp, 0, SEEK_SET)) goto err_;

	ip->data = (U8*)malloc(ip->size);
	if(!ip->data) goto err_;
	if(fread(ip->data, 1, ip->size, fp) != ip->size) goto err_;
	fclose(fp);

	if(UCIDecode4XnView(ip->data, ip->size, 0, &ip->linesize, &ip->width, &ip->height, &ip->bpp) < 0) goto err_;
	return ip;

err_:
	if(fp) fclose(fp);
	if(ip)
	{
		if(ip->data) { free(ip->data); ip->data = 0; }
		free(ip);
	}
	return 0;
}

__declspec(dllexport) BOOL __stdcall gfpLoadPictureGetInfo(void* p, int* pictype, int* width, int* height, int* dpi, int* bits_per_pixel, int* bytes_per_line, BOOL* has_colormap, char* label, int label_max_size)
{
	SImageParam *ip = (SImageParam*)p;
	if(!ip) return FALSE;

	*pictype = GFP_BGR;
	*width = ip->width;
	*height = ip->height;
	*dpi = 72;
	*bits_per_pixel = ip->bpp;
	*bytes_per_line = ip->linesize;
	*has_colormap = FALSE;
	_snprintf(label, label_max_size, "Ultra Compact Image (YUV 4:%s)", ip->yuv444 ? "4:4" : "2:0");

	return TRUE;
}

__declspec(dllexport) BOOL __stdcall gfpLoadPictureGetLine(void* p, int line, U8* buf)
{
	SImageParam* ip = (SImageParam*)p;
	line;
	if(!ip || ip->linepos >= ip->height) return FALSE;

	if(!ip->image)
	{
		if(!ip->data) return FALSE;
		if(UCIDecode4XnView(ip->data, ip->size, &ip->image, &ip->linesize, 0, 0, 0) < 0)
		{
			free(ip->data); ip->data = 0;
			return FALSE;
		}
		free(ip->data); ip->data = 0;
	}

	memcpy(buf, ip->image + ip->linesize * ip->linepos++, ip->linesize);
	return TRUE;
}

__declspec(dllexport) BOOL __stdcall gfpLoadPictureGetColormap(void* p, SColorMap* cmap)
{
	p; cmap;
	return FALSE;
}

__declspec(dllexport) void __stdcall gfpLoadPictureExit(void* p)
{
	SImageParam *ip = (SImageParam*)p;
	if(ip)
	{
		if(ip->image) { free(ip->image); ip->image = 0; }
		if(ip->data) { free(ip->data); ip->data = 0; }
		free(ip);
	}
}

/*
// bits_per_pixel can be 1 to 8, 24, 32
BOOL __stdcall gfpSavePictureIsSupported( INT width, INT height, INT bits_per_pixel, BOOL has_colormap )
{
	if ( has_colormap
		|| bits_per_pixel < 8 )
		return FALSE;

	return TRUE;
}

void * __stdcall gfpSavePictureInit( LPCSTR filename, INT width, INT height, INT bits_per_pixel, INT dpi, INT * picture_type, LPSTR label, INT label_max_size )
{
	MYDATA * data;

	data = calloc( 1, sizeof(MYDATA) );

	data->fp = fopen( filename, "wb" );
	if ( data->fp == NULL )
	{
		free(data);
		data = NULL;
	}

	*picture_type = GFP_RGB;

	fwrite( &width, sizeof(width), 1, data->fp );
	fwrite( &height, sizeof(height), 1, data->fp );
	fwrite( &bits_per_pixel, sizeof(bits_per_pixel), 1, data->fp );

	strncpy( label, "Test format", label_max_size );

	data->width = width;
	data->component_per_pixel = bits_per_pixel / 8;
	return data;
}

BOOL __stdcall gfpSavePicturePutLine( void * ptr, INT line, const U8 * buffer )
{
	MYDATA * data = (MYDATA *)ptr;

	fwrite( buffer, data->width, data->component_per_pixel, data->fp );
	return TRUE;
}

void __stdcall gfpSavePictureExit( void * ptr )
{
	MYDATA * data = (MYDATA *)ptr;

	fclose(data->fp);
	free(data);
}
*/

// for susie

#pragma pack(push)
#pragma pack(1)
typedef struct PictureInfo
{
	int left, top;
	int width, height;
	unsigned short x_density, y_density;
	short bit;
	void* hInfo;
}PictureInfo;
#pragma pack(pop)

__declspec(dllexport) int __stdcall GetPluginInfo(int id, char* dst, int dstlen)
{
	switch(id)
	{
	case 0: strcpy(dst, "00IN"); return 4;
	case 1: { static const char* s = "UCI Susie Plugin " UCI_VERSION " by dwing"; strcpy(dst, s); return strlen(s); }
	case 2:
	case 3: strcpy(dst, "*.uci"); return 5;
	}
	return 0;
}

__declspec(dllexport) int __stdcall IsSupported(const char* filename, int dw)
{
	if(!(dw & 0xffff0000))
	{
		DWORD t;
		if(!ReadFile((HANDLE)dw, &dw, 4, &t, 0))
			return 0;
	}
	else
		dw = *(int*)dw;
	return (dw & 0xffffff) == *(int*)"UCI";
}

__declspec(dllexport) int __stdcall GetPictureInfo(const char* filename, int len, unsigned flag, PictureInfo* pi)
{
	FILE* fp = fopen(filename, "rb");
	if(fp)
	{
		int bit;
		unsigned char buf[12] = {0};
		fread(buf, 1, 12, fp);
		fclose(fp);
		memset(pi, 0, sizeof(PictureInfo));
		if(UCIDecode(buf, 12, 0, 0, &pi->width, &pi->height, &bit) < 0)
			return 2;
		pi->bit = (short)bit;
	}
	return 1;
}

__declspec(dllexport) int __stdcall GetPicture(const char* filename, int len, unsigned flag, HLOCAL* hbi, HLOCAL* hbm, void* lpPrgressCallback, int lData)
{
	int ret = 1;
	FILE* fp = 0;
	unsigned char* src = 0, *dst = 0;
	unsigned char* pdst, *pbmp;
	BITMAPINFOHEADER* bih;
	int wide, dst_stride, bmp_stride;
	*hbi = 0;
	*hbm = 0;

	if((flag & 7) == 0) // disk file
	{
		fp = fopen(filename, "rb");
		if(!fp) goto err_;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		src = (unsigned char*)malloc(len);
		if(!src) goto err_;
		if((int)fread(src, 1, len, fp) != len) goto err_;
		fclose(fp); fp = 0;
	}
	else if((flag & 7) == 1) // image on memory
		src = (unsigned char*)filename;
	else goto err_;

	*hbi = LocalAlloc(LMEM_MOVEABLE, sizeof(BITMAPINFOHEADER));
	if(!*hbi) goto err_;
	bih = (BITMAPINFOHEADER*)LocalLock(*hbi);
	if(!bih) goto err_;
	if(UCIDecode(src, len, &dst, &dst_stride, (int*)&bih->biWidth, (int*)&bih->biHeight, (int*)&wide) < 0)
		goto err_;
	bih->biSize			= sizeof(BITMAPINFOHEADER);
	bih->biPlanes		= 1;
	bih->biBitCount		= (WORD)wide;
	bih->biCompression	= BI_RGB;
	bih->biSizeImage	= 0;
	bih->biXPelsPerMeter= 0;
	bih->biYPelsPerMeter= 0;
	bih->biClrUsed		= 0;
	bih->biClrImportant	= 0;
	if((char*)src != filename) { free(src); src = 0; }

	wide = wide / 8 * bih->biWidth;
	bmp_stride = (wide + 3) & 0xfffffffc;
	*hbm = LocalAlloc(LMEM_MOVEABLE, bmp_stride * bih->biHeight);
	if(!*hbm) goto err_;
	pbmp = (unsigned char*)LocalLock(*hbm);
	if(!pbmp) goto err_;
	pdst = dst + dst_stride * (bih->biHeight - 1);
	while(pdst >= dst)
	{
		memcpy(pbmp, pdst, wide);
		pbmp += bmp_stride;
		pdst -= dst_stride;
	}

	ret = 0;
err_:
	if(ret != 0)
	{
		if(*hbm) { LocalFree(*hbm); *hbm = 0; }
		if(*hbi) { LocalFree(*hbi); *hbi = 0; }
	}
	else
	{
		LocalUnlock(*hbm);
		LocalUnlock(*hbi);
	}
	if(dst) UCIFree(dst);
	if(src && (char*)src != filename) free(src);
	if(fp) fclose(fp);
	return ret;
}

__declspec(dllexport) int __stdcall GetPreview(const char* filename, int len, unsigned flag, HLOCAL* hbi, HLOCAL* hbm, void* lpPrgressCallback, int lData)
{
	return GetPicture(filename, len, flag, hbi, hbm, lpPrgressCallback, lData);
}

JNIEXPORT jint JNICALL Java_UCIDec_UCIDecode(JNIEnv* jenv, jclass jcls, jbyteArray j_src, jint j_srclen, jobjectArray j_dsts, jintArray j_stride, jintArray j_w, jintArray j_h, jintArray j_b)
{
	jcls;
	const void* const src = (*jenv)->GetByteArrayElements(jenv, j_src, 0);
	const void* dst = 0;
	int stride, w, h, b;
	const int r = UCIDecode(src, (int)j_srclen, (j_dsts && (*jenv)->GetArrayLength(jenv, j_dsts) > 0) ? &dst : 0, &stride, &w, &h, &b);
	(*jenv)->ReleaseByteArrayElements(jenv, j_src, src, JNI_ABORT);
	if(r >= 0)
	{
		if(dst)
		{
			const int dstlen = stride * h;
			const jbyteArray j_dst = (*jenv)->NewByteArray(jenv, dstlen);
			(*jenv)->SetByteArrayRegion(jenv, j_dst, 0, dstlen, dst);
			UCIFree(dst);
			(*jenv)->SetObjectArrayElement(jenv, j_dsts, 0, j_dst);
		}
		if(j_stride && (*jenv)->GetArrayLength(jenv, j_stride) > 0) (*jenv)->SetIntArrayRegion(jenv, j_stride, 0, 1, &stride);
		if(j_w		&& (*jenv)->GetArrayLength(jenv, j_w	 ) > 0) (*jenv)->SetIntArrayRegion(jenv, j_w	 , 0, 1, &w		);
		if(j_h		&& (*jenv)->GetArrayLength(jenv, j_h	 ) > 0) (*jenv)->SetIntArrayRegion(jenv, j_h	 , 0, 1, &h		);
		if(j_b		&& (*jenv)->GetArrayLength(jenv, j_b	 ) > 0) (*jenv)->SetIntArrayRegion(jenv, j_b	 , 0, 1, &b		);
	}
	return (jint)r;
}

JNIEXPORT void JNICALL Java_UCIDec_UCIDebug(JNIEnv* jenv, jclass jcls, jint level)
{
	jenv; jcls;
	UCIDebug((int)level);
}
