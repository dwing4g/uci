#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <locale.h>
#include "color1.h"
#include "color2.h"
#include "color3.h"
#include "define.h"

#ifdef _MSC_VER
#pragma warning(disable:4204) // warning C4204: nonstandard extension used : non-constant aggregate initializer
#pragma intrinsic(memcpy, memset, wcslen, wcscpy)
#endif

typedef unsigned char	U8;
typedef unsigned short	U16;

// extern "C" int __sse2_available = 0;

int __cdecl wmain(int argc, wchar_t** argv)
{
	int ret = 0, i;
	FILE* fp = 0;
	U8* src = 0;
	U8* dst = 0;
	int srclen, dstlen;
	int w = 0, h = 0;
	char use_stdin;
	char use_stdout;
	char yuv444 = 0;
	char i10b = 0;
	char range = 0;
	wchar_t dstname[512];
	static U8 bmp[0x36]=
	{	'B','M',0,0,	0,0,0,0,	0,0,0x36,0, 	0,0,0x28,0,
		0,0,0x80,0, 	0,0,0x80,0, 0,0,1,0,		0x18		};
	dstname[0] = 0;
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
		else if(!wcscmp(argv[i], L"-w"))
		{
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found number after '-w'\n");
				ret = -2; goto end_;
			}
			w = _wtoi(argv[i]);
			if(w < 0)
			{
				fwprintf(stderr, L"ERROR: bad width\n");
				ret = -3; goto end_;
			}
		}
		else if(!wcscmp(argv[i], L"-h"))
		{
			if(++i >= argc)
			{
				fwprintf(stderr, L"ERROR: not found number after '-h'\n");
				ret = -4; goto end_;
			}
			h = _wtoi(argv[i]);
			if(h < 0)
			{
				fwprintf(stderr, L"ERROR: bad height\n");
				ret = -5; goto end_;
			}
		}
		else if(!wcscmp(argv[i], L"-i444"))
		{
			yuv444 = 1;
		}
		else if(!wcscmp(argv[i], L"-i10b"))
		{
			i10b = 1;
		}
		else if(!wcscmp(argv[i], L"-r"))
		{
			range = 1;
		}
		else
		{
			fwprintf(stderr, L"ERROR: unknown option '%ls'\n", argv[i]);
			ret = -6; goto end_;
		}
	}
	if(w <= 0) w = 256;
	if(h <= 0) h = 256;
	if(!yuv444 && ((w&7)|(h&1)))
	{
		fwprintf(stderr, L"ERROR: width and height must be divisible by 8 and 2 in YUV 4:2:0\n");
		ret = -7; goto end_;
	}
	if(range) i10b = 0;

	if(argc < 2)
	{
		fwprintf(stderr,	L"YUV2BMP Converter " UCI_VERSION_W L" [by dwing] " UCI_DATE_W L"\n"
							L"Usage:   yuv2bmp <src_file.yuv> [options]\n"
							L"Options: -o <filename> set output file name, default: <src_file>.bmp\n"
							L"         -w <width>    set input width , must be 2x for i420, default: 256\n"
							L"         -h <height>   set input height, must be 2x for i420, default: 256\n"
							L"         -i444         set input format YUV 4:4:4, default: YUV 4:2:0\n"
							L"         -i10b         set input bit to 10, only for full range, default: 8\n"
							L"         -r            set YUV range to [16, 255], default: [0, 255]\n"
							L"Notes:   output file is a 24 or 32 bit uncompressed BMP format file\n"
							L"         specify - instead of filename to use stdin/stdout\n"
							L"Examples:yuv2bmp - -o - -w 1024 -h 768\n"
							L"         yuv2bmp input.yuv -i444\n");
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
			dot = wcsrchr(dstname, L'.');
			if(dot && _wcsicmp(dot + 1, L"bmp"))
				wcscpy(dot + 1, L"bmp");
			else
				wcscat(dstname, L".bmp");
		}
	}
	use_stdout = (*(int*)dstname == *(int*)L"-");

	fwprintf(stderr, L"%ls => %ls ... ", use_stdin ? L"<stdin>" : argv[1], use_stdout ? L"<stdout>" : dstname);

	dstlen = w * h * 3;
	srclen = dstlen << i10b;
	if(!yuv444) srclen /= 2;
	src = (U8*)malloc(srclen);
	if(!src) { fwprintf(stderr, L"ERROR: can't alloc src memory\n"); ret = -10; goto end_; }
	dst = (U8*)malloc(dstlen);
	if(!dst) { fwprintf(stderr, L"ERROR: can't alloc dst memory\n"); ret = -11; goto end_; }

	*(int*)(bmp + 0x02) = 0x36 + dstlen;
	*(int*)(bmp + 0x12) = w;
	*(int*)(bmp + 0x16) = h;

	if(use_stdin)
	{
		_setmode(_fileno(stdin), _O_BINARY);
		for(i = 0; i < srclen; )
		{
			int ii = fread(src + i, 1, srclen - i, stdin);
			if(ii <= 0) { fwprintf(stderr, L"ERROR: can't read stdin\n"); ret = -12; goto end_; }
			i += ii;
		}
	}
	else
	{
		fp = _wfopen(argv[1], L"rb");
		if(!fp) { fwprintf(stderr, L"ERROR: can't open src file\n"); ret = -13; goto end_; }
		if((int)fread(src, 1, srclen, fp) != srclen) { fwprintf(stderr, L"ERROR: can't read src file\n"); ret = -14; goto end_; }
		if(fclose(fp)) { fwprintf(stderr, L"ERROR: can't close src file\n"); ret = -15; goto end_; }
		fp = 0;
	}

	if(!yuv444)
	{
		if(!i10b)
		{
			U8* src2 = CreateYUV420Frame(src, w, h);
			const U8* frame_data[3] = {src2, src2 + w*h+w/2+3, src2 + w*h+(w/2+2)*(h/2+2)+w/2+3};
			const int frame_size[3] = {w, w/2+2, w/2+2};
			if(range) YUV420_BGR(dst, w * 3, frame_data, frame_size, w, h);
			else	  YUV420_BGR_F(dst, w * 3, frame_data, frame_size, w, h);
			free(src2);
		}
		else
		{
			U16* src2 = CreateYUV420Frame10((const U16*)src, w, h);
			const U8* frame_data[3] = {(const U8*)(src2), (const U8*)(src2 + w*h+w/2+3), (const U8*)(src2 + w*h+(w/2+2)*(h/2+2)+w/2+3)};
			const int frame_size[3] = {w, w/2+2, w/2+2};
			YUV420_BGR_F_10(dst, w * 3, frame_data, frame_size, w, h);
			free(src2);
		}
	}
	else
	{
		if(!i10b)
		{
			const U8* frame_data[3] = {src, src + w*h, src + w*h*2};
			const int frame_size[3] = {w, w, w};
			if(range) YUV444_BGR(dst, w * 3, frame_data, frame_size, w, h);
			else	  YUV444_BGR_F(dst, w * 3, frame_data, frame_size, w, h);
		}
		else
		{
			const U8* frame_data[3] = {src, src + w*h*2, src + w*h*2*2};
			const int frame_size[3] = {w, w, w};
			YUV444_BGR_F_10(dst, w * 3, frame_data, frame_size, w, h);
		}
	}

	if(use_stdout)
	{
		_setmode(_fileno(stdout), _O_BINARY);
		fp = stdout;
	}
	else
	{
		fp = _wfopen(dstname, L"wb");
		if(!fp) { fwprintf(stderr, L"ERROR: can't create '%ls'\n", dstname); ret = -20; goto end_; }
	}
	if((int)fwrite(bmp, 1, 0x36, fp) != 0x36) { fwprintf(stderr, L"ERROR: can't write '%ls'\n", dstname); ret = -21; goto end_; }
	for(i = h - 1; i >= 0; --i)
	{
		if((int)fwrite(dst + i * w * 3, 1, w * 3, fp) != (w * 3)) { fwprintf(stderr, L"ERROR: can't write '%ls'\n", dstname); ret = -22; goto end_; }
		if((int)fwrite("\0\0\0" 	  , 1, w & 3, fp) != (w & 3)) { fwprintf(stderr, L"ERROR: can't write '%ls'\n", dstname); ret = -23; goto end_; }
	}
	if(!use_stdout) fclose(fp);
	fp = 0;

end_:
	if(dst) free(dst);
	if(src) free(src);
	if(fp && fp != stdout) fclose(fp);
	if(ret)
		fwprintf(stderr, L"ERROR CODE = %d\n", ret);
	else
		fwprintf(stderr, L"OK!\n");

	return ret;
}
