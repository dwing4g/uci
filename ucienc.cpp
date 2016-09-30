#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include "buffer.h"
#include "color1.h"
#include "color3.h"
#include "define.h"

#ifdef _MSC_VER
#pragma intrinsic(memcpy, memset, wcslen, wcscpy)
#pragma warning(disable:4706) // 条件表达式内的赋值
#endif

#ifdef __GNUC__
	#ifndef __forceinline
		#define __forceinline __attribute__((__always_inline__)) inline
	#endif
#endif

#define DEFAULT_OPT L"-b 0 -r 0 -m 11 -t 2 --aq-mode 2 --no-psy --threads 1"
#define DEFAULT_CRF 27

// extern "C" int __sse2_available = 0;

__forceinline void FixY(void* dst, int dstlen)
{
	U8* pdst = (U8*)dst;
	U8* pdstend = pdst + dstlen;
	for(; pdst < pdstend; ++pdst)
		if(*pdst < 255) ++*pdst;
}

__forceinline void RemoveInfo(Buffer& buffer)
{
	if(buffer.size() >= 0x100)
	{
		U8 head[0x48];
		for(int i = 0; i < 5; ++i) head[i] = buffer[i];
		for(int i = 0; i < 0x40; ++i)
		{
			head[i + 5] = buffer[i + 5];
			if(!memcmp(head + i, "\x00\x00\x01\x06\x05\xff", 6))	// 00 00 01 06 05 FF
			{
				U32 len = 6;
				for(int j = i + 5;; ++j)
				{
					U8 n = buffer[j];
					len += n + 1;
					if(n < 255) break;
				}
				if(i == 1)
					buffer.skip(len + 1);
				else
				{
					buffer.skip(len);
					while(--i >= 0) buffer.set(i, head[i]);
				}
				break;
			}
		}
	}
}

// 返回<0表示错误,>=0表示输出长度,输出的*dst_yuv,*dst_alpha要用free释放内存
__forceinline int BMP2YUV(const void* src, int srclen, void** dst_yuv, void** dst_alpha, int* width, int* height, char yuv444)
{
	if(!dst_yuv || !dst_alpha || !width || !height) return -1;
	*dst_yuv = *dst_alpha = 0; *width = *height = 0;
	if(!src || srclen < 0x1d) return -2;
	if(memcmp(src, "BM", 2)) return -3;
	const int srcbase = *(int*)((const U8*)src + 0x0a);
	const int srcw = *width  = *(int*)((const U8*)src + 0x12);
	const int srch = *height = *(int*)((const U8*)src + 0x16); if(srcw <= 0 || srch <= 0) return -4;
	const int srcb = (int)*((const U8*)src + 0x1c);
	const int srcs = srcw * (srcb/8);
	const int srcwb = (srcb == 24 ? srcs + (srcw&3) : srcs);
	const int dstw = srcw + (srcw & !yuv444);
	const int dsth = srch + (srch & !yuv444);
	const int dstlen = (dstw * dsth * 3) << yuv444;
	if(srclen - srcbase < srcwb * srch) return -5;
	if(srcb != 32 && srcb != 24) return -6;

	// 宽高像素补充
	U8* srctemp;
	int srctempwb;
	if(srcw != dstw || srch != dsth)
	{
		srctempwb = dstw * (srcb/8);
		srctemp = (U8*)malloc((srctempwb + 4) * dsth + 4);	// +4: 保证宽度>=srcwb,保证MMX转换时不读出界
		if(!srctemp) return -7;
		if(srch == dsth)
		{
			for(int i = 0; i < srch; ++i)
			{
				memcpy(srctemp + srctempwb * i, (const U8*)src + srcbase + srcwb * i, srcs);
				memcpy(srctemp + srctempwb * i + srcs, (const U8*)src + srcbase + srcwb * i + (srcs - srcb/8), srcb/8);
			}
		}
		else if(srcw == dstw)
		{
			srctempwb = srcwb;
			memcpy(srctemp + srctempwb, (const U8*)src + srcbase, srctempwb * srch);
			memcpy(srctemp, srctemp + srctempwb, srctempwb);
		}
		else
		{
			for(int i = 0; i < srch; ++i)
			{
				memcpy(srctemp + srctempwb * (i+1), (const U8*)src + srcbase + srcwb * i, srcs);
				memcpy(srctemp + srctempwb * (i+1) + srcs, (const U8*)src + srcbase + srcwb * i + (srcs - srcb/8), srcb/8);
			}
			memcpy(srctemp, srctemp + srctempwb, srctempwb);
		}
	}
	else
	{
		srctemp = (U8*)src + srcbase;
		srctempwb = srcwb;
	}

	// 开始转换
	if(!(*dst_yuv = malloc(dstlen)))
	{
		if(srctemp != (U8*)src + srcbase) free(srctemp);
		return -8;
	}
	if(!yuv444)
	{
		if(srcb == 24)
		{
			U8* const dst3[3] = { (U8*)*dst_yuv, (U8*)*dst_yuv + dstw*dsth*2, (U8*)*dst_yuv + dstw*dsth*2 + dstw*dsth/4*2 };
			const int dsts[3] = { dstw, dstw/2, dstw/2 };
			BGR_YUV420_F_10(dst3, dsts, srctemp + srctempwb * (dsth-1), -srctempwb, dstw, dsth);
		}
		else
		{
			if(!(*dst_alpha = malloc(dstw * dsth * 3 / 2)))
			{
				free(*dst_yuv); *dst_yuv = 0;
				if(srctemp != (U8*)src + srcbase) free(srctemp);
				return -9;
			}
			U8* const dst4[4] = { (U8*)*dst_yuv, (U8*)*dst_yuv + dstw*dsth*2, (U8*)*dst_yuv + dstw*dsth*2 + dstw*dsth/4*2, (U8*)*dst_alpha };
			const int dsts[4] = { dstw, dstw/2, dstw/2, dstw };
			BGRA_YUVA420_F_10(dst4, dsts, srctemp + srctempwb * (dsth-1), -srctempwb, dstw, dsth);
			memset((U8*)*dst_alpha + dstw * dsth, 0x80, dstw * dsth / 2);	// 填充alpha的UV通道
		}
	}
	else
	{
		if(srcb == 24)
		{
			U8* const dst3[3] = { (U8*)*dst_yuv, (U8*)*dst_yuv + dstw*dsth*2, (U8*)*dst_yuv + dstw*dsth*2*2 };
			const int dsts[3] = { dstw, dstw, dstw };
			BGR_YUV444_F_10(dst3, dsts, srctemp + srctempwb * (dsth-1), -srctempwb, dstw, dsth);
		}
		else
		{
			if(!(*dst_alpha = malloc(dstw * dsth * 3)))
			{
				free(*dst_yuv); *dst_yuv = 0;
				if(srctemp != (U8*)src + srcbase) free(srctemp);
				return -10;
			}
			U8* const dst4[4] = { (U8*)*dst_yuv, (U8*)*dst_yuv + dstw*dsth*2, (U8*)*dst_yuv + dstw*dsth*2*2, (U8*)*dst_alpha };
			const int dsts[4] = { dstw, dstw, dstw, dstw };
			BGRA_YUVA444_F_10(dst4, dsts, srctemp + srctempwb * (dsth-1), -srctempwb, dstw, dsth);
			memset((U8*)*dst_alpha + dstw * dsth, 0x80, dstw * dsth * 2);	// 填充alpha的UV通道
		}
	}

	if(srctemp != (U8*)src + srcbase) free(srctemp);
	return dstlen;
}

extern "C" int __cdecl wmain(int argc, wchar_t** argv)
{
	int ret = 0, i, n;
	HANDLE hfile = INVALID_HANDLE_VALUE;
	HANDLE hpir  = INVALID_HANDLE_VALUE;
	HANDLE hpiw  = INVALID_HANDLE_VALUE;
	HANDLE hpor  = INVALID_HANDLE_VALUE;
	HANDLE hpow  = INVALID_HANDLE_VALUE;
	SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, TRUE};
	STARTUPINFO si = {sizeof(si), 0};
	PROCESS_INFORMATION pi = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
	Buffer buffer;
	U8* src = 0;
	U8* dst0 = 0;
	U8* dst1 = 0;
	U8* temp = 0;
	int srclen, dstlen;
	int w = 0, h = 0, ww, hh;
	float q0 = DEFAULT_CRF;
	float q1 = -1.0f;
	const wchar_t* opt0 = 0;
	const wchar_t* opt1 = 0;
	const wchar_t* exe = L"x264.exe";
	const wchar_t* format = 0;
	char use_stdin = 0;
	char use_stdout = 0;
	char yuv444 = 0;
	char need_info = 0;
	char is_quiet = 0;
	char yuv_out_10bit = 0;
	char yuv_in_10bit = 0;
	int yuv_out = 0;	// 1:yuv420 2:yuv444
	int yuv_in = 0; 	// 1:yuv420 2:yuv444
	char buf[0x20];
	wchar_t dstname[512];
	wchar_t cmd[2048];
	dstname[0] = 0;
	cmd[2047] = 0;
	setlocale(LC_ALL, "");

	for(i = 2; i < argc; ++i)
	{
		if(!wcscmp(argv[i], L"-o"))
		{
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found filename after '-o'\n");
				ret = -1; goto end_;
			}
			wcsncpy(dstname, argv[i], sizeof(dstname)/sizeof(*dstname) - 1);
		}
		else if(!wcscmp(argv[i], L"-q"))
		{
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found number after '-q'\n");
				ret = -2; goto end_;
			}
			q0 = (float)_wtof(argv[i]);
			if(q0 < 0.0f || q0 > 51.0f)
			{
				fwprintf(stderr, L"ERROR: bad -q number\n");
				ret = -3; goto end_;
			}
		}
		else if(!wcscmp(argv[i], L"-Q"))
		{
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found number after '-Q'\n");
				ret = -4; goto end_;
			}
			q1 = (float)_wtof(argv[i]);
			if(q1 < 0.0f || q1 > 51.0f)
			{
				fwprintf(stderr, L"ERROR: bad -Q number\n");
				ret = -5; goto end_;
			}
		}
		else if(!wcscmp(argv[i], L"-x"))
		{
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found string after '-x'\n");
				ret = -6; goto end_;
			}
			opt0 = argv[i];
		}
		else if(!wcscmp(argv[i], L"-X"))
		{
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found string after '-X'\n");
				ret = -7; goto end_;
			}
			opt1 = argv[i];
		}
		else if(!wcscmp(argv[i], L"-p"))
		{
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found string after '-p'\n");
				ret = -8; goto end_;
			}
			exe = argv[i];
		}
		else if(!wcscmp(argv[i], L"-s"))
		{
			yuv444 = 1;
		}
		else if(!wcscmp(argv[i], L"-o420"))
		{
			yuv_out = 1;
		}
		else if(!wcscmp(argv[i], L"-o444"))
		{
			yuv_out = 2;
		}
		else if(!wcscmp(argv[i], L"-o10b"))
		{
			yuv_out_10bit = 1;
		}
		else if(!wcscmp(argv[i], L"-i420"))
		{
			yuv_in = 1;
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found width after '-i420'\n");
				ret = -9; goto end_;
			}
			w = _wtoi(argv[i]);
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found height after '-i420'\n");
				ret = -10; goto end_;
			}
			h = _wtoi(argv[i]);
			if(w <= 0 || h <= 0)
			{
				fwprintf(stderr, L"ERROR: invalid width or height after '-i420'\n");
				ret = -11; goto end_;
			}
			if((w & 1) || (h & 1))
			{
				fwprintf(stderr, L"ERROR: width and height after '-i420' must be divisible by 2\n");
				ret = -12; goto end_;
			}
		}
		else if(!wcscmp(argv[i], L"-i444"))
		{
			yuv_in = 2;
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found width after '-i444'\n");
				ret = -13; goto end_;
			}
			w = _wtoi(argv[i]);
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found height after '-i444'\n");
				ret = -14; goto end_;
			}
			h = _wtoi(argv[i]);
			if(w <= 0 || h <= 0)
			{
				fwprintf(stderr, L"ERROR: invalid width or height after '-i444'\n");
				ret = -15; goto end_;
			}
		}
		else if(!wcscmp(argv[i], L"-i10b"))
		{
			yuv_in_10bit = 1;
		}
		else if(!wcscmp(argv[i], L"-info"))
		{
			need_info = 1;
		}
		else if(!wcscmp(argv[i], L"-quiet"))
		{
			is_quiet = 1;
		}
		else
		{
			fwprintf(stderr, L"ERROR: unknown option '%ls'\n", argv[i]);
			ret = -16; goto end_;
		}
	}
	if(q1 < 0.0f) q1 = q0;
	if(!opt1) opt1 = opt0;
	if(yuv_in)
	{
		yuv444 = (yuv_in == 2);
		yuv_out = 0;
	}
	else
		yuv_in_10bit = 1;
	if(yuv_out)
		yuv444 = (yuv_out == 2);
	else
		yuv_out_10bit = 0;
	format = (yuv444 ? L"i444" : L"i420");

	if(argc < 2)
	{
		fwprintf(stderr,L"UCI (Ultra Compact Image) Encoder " UCI_VERSION L" [by dwing] " UCI_DATE L"\n"
						L"Usage:   ucienc <src_file.bmp> [options]\n"
						L"Options: -o <filename> set output file name, default: <src_file>.uci\n"
						L"         -q <number>   set quality of RGB/YUV channel compression\n"
						L"                       [0.0 ~ 51.0] (best ~ worst) default: %d\n"
						L"         -Q <number>   set quality of alpha channel compression\n"
						L"                       [0.0 ~ 51.0] (best ~ worst) default: same as -q\n"
						L"         -x \"......\"   use custom detail x264 options for RGB/YUV channel\n"
						L"                       ignore -q/-Q, use carefully\n"
						L"         -X \"......\"   use custom detail x264 options for alpha channel\n"
						L"                       ignore -q/-Q, use carefully, default: same as -x\n"
						L"         -p <progname> set x264 program file name, default: x264.exe\n"
						L"         -s            encode by YUV 4:4:4, default YUV 4:2:0\n"
						L"         -o420         set output format in YUV 4:2:0 (ignore alpha channel)\n"
						L"         -o444         set output format in YUV 4:4:4 (ignore alpha channel)\n"
						L"         -o10b         set output format in YUV 10-bit, default: 8-bit\n"
						L"         -i420 <w> <h> set input format in YUV 4:2:0, ignore -s,-o420,-o444\n"
						L"         -i444 <w> <h> set input format in YUV 4:4:4, ignore -s,-o420,-o444\n"
						L"         -i10b         set input format in YUV 10-bit, default: 8-bit\n"
						L"         -info         reserve x264 mark and parameter info in UCI data\n"
						L"         -quiet        suppress x264 message\n"
						L"Default: x264 option:  --crf %d %ls\n"
						L"Notes:   input file must be a 24 or 32 bit uncompressed BMP format file\n"
						L"         specify - instead of filename to use stdin/stdout\n"
						L"Examples:ucienc input.bmp -q 30 -Q 35\n"
						L"         ucienc - -o output.uci -x \"-q 30\"\n",
						DEFAULT_CRF, DEFAULT_CRF, DEFAULT_OPT);
		return 1;
	}

	use_stdin = (*(int*)argv[1] == *(int*)L"-");
	if(!dstname[0])
	{
		if(use_stdin)
		{
			*(int*)dstname = *(int*)L"-";
		}
		else
		{
			wchar_t* dot;
			wcsncpy(dstname, argv[1], sizeof(dstname)/sizeof(*dstname) - 8);
			if(!yuv_out)
			{
				dot = wcsrchr(dstname, L'.');
				if(dot && wcsicmp(dot + 1, L"uci"))
					wcscpy(dot + 1, L"uci");
				else
					wcscat(dstname, L".uci");
			}
			else
			{
				dot = wcsrchr(dstname, L'.');
				if(dot && wcsicmp(dot + 1, L"yuv"))
					wcscpy(dot + 1, L"yuv");
				else
					wcscat(dstname, L".yuv");
			}
		}
	}
	use_stdout = (*(int*)dstname == *(int*)L"-");

	fwprintf(stderr, is_quiet ? L"%ls => %ls ... " : L"%ls => %ls\n", use_stdin ? L"<stdin>" : argv[1], use_stdout ? L"<stdout>" : dstname);

	if(use_stdin)
	{
		hfile = GetStdHandle(STD_INPUT_HANDLE);
		if(hfile == INVALID_HANDLE_VALUE) { fwprintf(stderr, L"ERROR: can't get stdin handle\n"); ret = -20; goto end_; }
	}
	else
	{
		hfile = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
		if(hfile == INVALID_HANDLE_VALUE) { fwprintf(stderr, L"ERROR: can't open src file\n"); ret = -21; goto end_; }
	}
	if(!yuv_in)
	{
		if(!ReadFile(hfile, buf, 0x20, (DWORD*)&i, 0) || i != 0x20) { fwprintf(stderr, L"ERROR: can't read input\n"); ret = -22; goto end_; }
		if(*(short*)buf != *(short*)"BM") { fwprintf(stderr, L"ERROR: unknown format\n"); ret = -23; goto end_; }
		srclen = *(int*)(buf + 0x0a);
		if((DWORD)srclen > 0x1000) { fwprintf(stderr, L"ERROR: corrupt input data\n"); ret = -24; goto end_; }
		w = *(int*)(buf + 0x12);
		h = *(int*)(buf + 0x16);
		n = buf[0x1c];
		if(w <= 0 || h <= 0) { fwprintf(stderr, L"ERROR: bad width or height for input data\n"); ret = -25; goto end_; }
		if(n != 24 && n != 32) { fwprintf(stderr, L"ERROR: not uncompressed 24/32-bit bmp for input data\n"); ret = -26; goto end_; }
		srclen += (w * (n/8) + (n == 24 ? w & 3 : 0)) * h;
		if(srclen < 0x20) { fwprintf(stderr, L"ERROR: corrupt input data\n"); ret = -27; goto end_; }
		src = (U8*)malloc(srclen);
		if(!src) { fwprintf(stderr, L"ERROR: can't alloc src memory\n"); ret = -28; goto end_; }
		memcpy(src, buf, 0x20);
		if(use_stdin)
		{
			for(int len = 0x20; len < srclen;)
			{
				if(WaitForSingleObject(hfile, INFINITE) != WAIT_OBJECT_0) { fwprintf(stderr, L"ERROR: can't wait input\n"); ret = -29; goto end_; }
				if(!ReadFile(hfile, src + len, srclen - len, (DWORD*)&i, 0)) { fwprintf(stderr, L"ERROR: can't read input\n"); ret = -30; goto end_; }
				len += i;
			}
		}
		else
		{
			if(!ReadFile(hfile, src + 0x20, srclen - 0x20, (DWORD*)&i, 0) || i != srclen - 0x20) { fwprintf(stderr, L"ERROR: can't read input\n"); ret = -31; goto end_; }
		}
	}
	else
	{
		srclen = (yuv_in == 1 ? w * h * 3 / 2 : w * h * 3) << yuv_in_10bit;
		src = (U8*)malloc(srclen);
		if(!src) { fwprintf(stderr, L"ERROR: can't alloc src memory\n"); ret = -32; goto end_; }
		if(use_stdin)
		{
			for(int len = 0; len < srclen;)
			{
				if(WaitForSingleObject(hfile, INFINITE) != WAIT_OBJECT_0) { fwprintf(stderr, L"ERROR: can't wait input\n"); ret = -33; goto end_; }
				if(!ReadFile(hfile, src + len, srclen - len, (DWORD*)&i, 0)) { fwprintf(stderr, L"ERROR: can't read input\n"); ret = -34; goto end_; }
				len += i;
			}
		}
		else
		{
			if(!ReadFile(hfile, src, srclen, (DWORD*)&i, 0) || i != srclen) { fwprintf(stderr, L"ERROR: can't read input\n"); ret = -35; goto end_; }
		}
	}
	if(!use_stdin)
		CloseHandle(hfile);
	hfile = INVALID_HANDLE_VALUE;

	if(!yuv_in)
	{
		if(!memcmp(src, "BM", 2))
		{
			dstlen = BMP2YUV(src, srclen, (void**)&dst0, (void**)&dst1, &w, &h, yuv444);
			if(dstlen < 0)
			{
				switch(dstlen)
				{
				case - 1: fwprintf(stderr, L"ERROR: BMP2YUV: bad param\n"); break;
				case - 3:
				case - 7: fwprintf(stderr, L"ERROR: BMP2YUV: unknown format\n"); break;
				case - 4: fwprintf(stderr, L"ERROR: BMP2YUV: bad width or height of src file\n"); break;
				case - 2:
				case - 5: fwprintf(stderr, L"ERROR: BMP2YUV: corrupt src file (%d)\n", dstlen); break;
				case - 6:
				case - 8:
				case - 9: fwprintf(stderr, L"ERROR: BMP2YUV: can not alloc memory (%d)\n", dstlen); break;
				default:  fwprintf(stderr, L"ERROR: BMP2YUV: unknown error (%d)\n", dstlen); break;
				}
				ret = -100 + dstlen; goto end_;
			}
			ww = w + (yuv444 ? 0 : (w & 1));
			hh = h + (yuv444 ? 0 : (h & 1));
			free(src); src = 0;
		}
		else
		{
			fwprintf(stderr, L"ERROR: unknown src file format\n");
			ret = -100; goto end_;
		}
	}
	else
	{
		ww = w;
		hh = h;
		dstlen = srclen;
		dst0 = src; src = 0;
		dst1 = 0;
	}

	if(use_stdout)
	{
		hfile = GetStdHandle(STD_OUTPUT_HANDLE);
		if(hfile == INVALID_HANDLE_VALUE) { fwprintf(stderr, L"ERROR: can't get stdout handle\n"); ret = -40; goto end_; }
	}
	else
	{
		hfile = CreateFileW(dstname, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, 0, 0);
		if(hfile == INVALID_HANDLE_VALUE) { fwprintf(stderr, L"ERROR: can't create dst file\n"); ret = -41; goto end_; }
	}

	if(yuv_out)
	{
		if(yuv_out_10bit)
		{
			if(!WriteFile(hfile, dst0, dstlen, (DWORD*)&i, 0) || i != dstlen) { fwprintf(stderr, L"ERROR: can't write dst file\n"); ret = -42; goto end_; }
		}
		else
		{
			dstlen /= 2;
			for(i = 0; i < dstlen; ++i)
				dst0[i] = (U8)(((U16*)dst0)[i] >> 2);
			if(!WriteFile(hfile, dst0, dstlen, (DWORD*)&i, 0) || i != dstlen) { fwprintf(stderr, L"ERROR: can't write dst file\n"); ret = -42; goto end_; }
		}
		goto end_;
	}

	if(!yuv444)
	{
		if(!WriteFile(hfile, dst1 ? "UCI\x21" : "UCI\x20", 4, (DWORD*)&i, 0) || i != 4) { fwprintf(stderr, L"ERROR: can't write dst file\n"); ret = -43; goto end_; }
	}
	else
	{
		if(!WriteFile(hfile, dst1 ? "UCI\x41" : "UCI\x40", 4, (DWORD*)&i, 0) || i != 4) { fwprintf(stderr, L"ERROR: can't write dst file\n"); ret = -44; goto end_; }
	}
	if(!WriteFile(hfile, &w, 4, (DWORD*)&i, 0) || i != 4) { fwprintf(stderr, L"ERROR: can't write dst file\n"); ret = -45; goto end_; }
	if(!WriteFile(hfile, &h, 4, (DWORD*)&i, 0) || i != 4) { fwprintf(stderr, L"ERROR: can't write dst file\n"); ret = -46; goto end_; }
	if(!(temp = (U8*)malloc((DWORD)Buffer::BLOCKSIZE))) { fwprintf(stderr, L"ERROR: can't alloc memory\n"); ret = -47; goto end_; }

	if(!CreatePipe(&hpir, &hpiw, &sa, (DWORD)dstlen)) { fwprintf(stderr, L"ERROR: can't create input pipe\n"); ret = -48; goto end_; }
	if(!CreatePipe(&hpor, &hpow, &sa, (DWORD)Buffer::BLOCKSIZE)) { fwprintf(stderr, L"ERROR: can't create output pipe\n"); ret = -49; goto end_; }
	if(!WriteFile(hpiw, dst0, (DWORD)dstlen, (DWORD*)&i, 0) || dstlen != i) { fwprintf(stderr, L"ERROR: can't write pipe\n"); ret = -50; goto end_; }
	CloseHandle(hpiw); hpiw = INVALID_HANDLE_VALUE;

	if(!opt0)
		_snwprintf(cmd, sizeof(cmd) - 1, L"%ls --crf %f %ls -o - --input-csp %ls --output-csp %ls --input-res %ux%u --input-depth %d --input-range pc --range pc -", exe, q0, DEFAULT_OPT, format, format, ww, hh, yuv_in_10bit ? 10 : 8);
	else
		_snwprintf(cmd, sizeof(cmd) - 1, L"%ls %ls -o - --input-csp %ls --output-csp %ls --input-res %ux%u --input-depth %d --input-range pc --range pc -", exe, opt0, format, format, ww, hh, yuv_in_10bit ? 10 : 8);

	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = hpir;
	si.hStdOutput = hpow;
	si.hStdError = (is_quiet ? INVALID_HANDLE_VALUE : GetStdHandle(STD_ERROR_HANDLE));

	if(!CreateProcessW(0, cmd, 0, 0, TRUE, 0, 0, 0, &si, &pi)) { fwprintf(stderr, L"ERROR: can't create x264 process: %ls\n", exe); ret = -52; goto end_; }

	for(;;)
	{
		i = WaitForSingleObject(pi.hProcess, 10);
		if(!PeekNamedPipe(hpor, 0, 0, 0, (DWORD*)&n, 0)) { fwprintf(stderr, L"ERROR: can't peek output pipe\n"); ret = -53; goto end_; }
		if(n)
		{
			if(!ReadFile(hpor, temp, (DWORD)Buffer::BLOCKSIZE, (DWORD*)&n, 0)) { fwprintf(stderr, L"ERROR: can't read output pipe\n"); ret = -54; goto end_; }
			if(!buffer.write(temp, (DWORD)n)) { fwprintf(stderr, L"ERROR: can't alloc memory\n"); ret = -55; goto end_; }
		}
		if(i == WAIT_OBJECT_0) break;
		if(i != WAIT_TIMEOUT) { fwprintf(stderr, L"ERROR: exception while running %ls\n", exe); ret = -56; goto end_; }
	}
	CloseHandle(hpir); hpir = INVALID_HANDLE_VALUE;

	if(!GetExitCodeProcess(pi.hProcess, (DWORD*)&i)) { fwprintf(stderr, L"ERROR: can't get exit code of %ls\n", exe); ret = -57; goto end_; }
	if(i != 0) { fwprintf(stderr, L"ERROR: %ls return code = %d\n", exe, i); ret = -58; goto end_; }

	if(!need_info) RemoveInfo(buffer);
	n = buffer.size();
	if(!WriteFile(hfile, &n, 4, (DWORD*)&i, 0) || i != 4) { fwprintf(stderr, L"ERROR: can't write dst file\n"); ret = -59; goto end_; }
	for(;;)
	{
		n = (buffer.size() < (int)Buffer::BLOCKSIZE ? buffer.size() : (int)Buffer::BLOCKSIZE);
		if(!n) break;
		if(!buffer.read(temp, (DWORD)n)) { fwprintf(stderr, L"ERROR: internal error\n"); ret = -60; goto end_; }
		if(!WriteFile(hfile, temp, (DWORD)n, (DWORD*)&i, 0) || i != n) { fwprintf(stderr, L"ERROR: can't write dst file\n"); ret = -61; goto end_; }
	}

	CloseHandle(pi.hThread ); pi.hThread  = INVALID_HANDLE_VALUE;
	CloseHandle(pi.hProcess); pi.hProcess = INVALID_HANDLE_VALUE;

	if(dst1)
	{
		if((ww & 1) | (hh & 1))
		{
			format = L"i444";
			dstlen = ww * hh * 3;
		}
		else
		{
			format = L"i420";
			dstlen = ww * hh * 3 / 2;
		}

		if(!CreatePipe(&hpir, &hpiw, &sa, (DWORD)dstlen)) { fwprintf(stderr, L"ERROR: can't create input pipe\n"); ret = -88; goto end_; }
		if(!WriteFile(hpiw, dst1, (DWORD)dstlen, (DWORD*)&i, 0) || i != dstlen) { fwprintf(stderr, L"ERROR: can't write pipe\n"); ret = -89; goto end_; }
		CloseHandle(hpiw); hpiw = INVALID_HANDLE_VALUE;

		if(!opt1)
			_snwprintf(cmd, sizeof(cmd) - 1, L"%ls --crf %f %ls -o - --input-csp %ls --output-csp %ls --input-res %ux%u --input-depth 8 --input-range pc --range pc -", exe, q1, DEFAULT_OPT, format, format, ww, hh);
		else
			_snwprintf(cmd, sizeof(cmd) - 1, L"%ls %ls -o - --input-csp %ls --output-csp %ls --input-res %ux%u --input-depth 8 --input-range pc --range pc -", exe, opt1, format, format, ww, hh);

		si.hStdInput = hpir;

		if(!CreateProcessW(0, cmd, 0, 0, TRUE, 0, 0, 0, &si, &pi)) { fwprintf(stderr, L"ERROR: can't create x264 process: %ls\n", exe); ret = -90; goto end_; }

		for(;;)
		{
			i = WaitForSingleObject(pi.hProcess, 10);
			if(!PeekNamedPipe(hpor, 0, 0, 0, (DWORD*)&n, 0)) { fwprintf(stderr, L"ERROR: can't peek output pipe\n"); ret = -91; goto end_; }
			if(n)
			{
				if(!ReadFile(hpor, temp, (DWORD)Buffer::BLOCKSIZE, (DWORD*)&n, 0)) { fwprintf(stderr, L"ERROR: can't read output pipe\n"); ret = -92; goto end_; }
				if(!buffer.write(temp, n)) { fwprintf(stderr, L"ERROR: can't alloc memory\n"); ret = -93; goto end_; }
			}
			if(i == WAIT_OBJECT_0) break;
			if(i != WAIT_TIMEOUT) { fwprintf(stderr, L"ERROR: exception while running %ls\n", exe); ret = -94; goto end_; }
		}
		CloseHandle(hpir); hpir = INVALID_HANDLE_VALUE;

		if(!GetExitCodeProcess(pi.hProcess, (DWORD*)&i)) { fwprintf(stderr, L"ERROR: can't get exit code of %ls\n", exe); ret = -95; goto end_; }
		if(i != 0) { fwprintf(stderr, L"ERROR: %ls return code = %d\n", exe, i); ret = -96; goto end_; }

		if(!need_info) RemoveInfo(buffer);
		n = buffer.size();
		if(!WriteFile(hfile, &n, 4, (DWORD*)&i, 0) || i != 4) { fwprintf(stderr, L"ERROR: can't write dst file\n"); ret = -97; goto end_; }
		for(;;)
		{
			n = buffer.size() < (int)Buffer::BLOCKSIZE ? buffer.size() : (int)Buffer::BLOCKSIZE;
			if(!n) break;
			if(!buffer.read(temp, (DWORD)n)) { fwprintf(stderr, L"ERROR: internal error\n"); ret = -98; goto end_; }
			if(!WriteFile(hfile, temp, (DWORD)n, (DWORD*)&i, 0) || i != n) { fwprintf(stderr, L"ERROR: can't write dst file\n"); ret = -99; goto end_; }
		}

		CloseHandle(pi.hThread ); pi.hThread  = INVALID_HANDLE_VALUE;
		CloseHandle(pi.hProcess); pi.hProcess = INVALID_HANDLE_VALUE;
	}

	if(use_stdout) hfile = INVALID_HANDLE_VALUE;

end_:
	if(temp) free(temp);
	if(dst1) free(dst1);
	if(dst0) free(dst0);
	if(src) free(src);
	if(pi.hThread != INVALID_HANDLE_VALUE) CloseHandle(pi.hThread);
	if(pi.hProcess != INVALID_HANDLE_VALUE) { TerminateProcess(pi.hProcess, 0xffffffff); CloseHandle(pi.hProcess); }
	if(hpow != INVALID_HANDLE_VALUE) CloseHandle(hpow);
	if(hpor != INVALID_HANDLE_VALUE) CloseHandle(hpor);
	if(hpiw != INVALID_HANDLE_VALUE) CloseHandle(hpiw);
	if(hpir != INVALID_HANDLE_VALUE) CloseHandle(hpir);
	if(hfile != INVALID_HANDLE_VALUE) CloseHandle(hfile);
	if(ret)
		fwprintf(stderr, L"ERROR CODE = %d\n", ret);
	else
		fwprintf(stderr, L"OK!\n");

	return ret;
}
