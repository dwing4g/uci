#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <locale.h>
#include "define.h"

// UCI格式图像解码,目前只支持输出24位BGR和32位的BGRA,返回0表示调用成功,负数表示错误,不支持多线程同时访问
int __stdcall UCIDecode(
	const void* src,	// 输入UCI数据指针(不能传入null,其它指针参数可以传入null表示不需要输出)
	int 		srclen, // 输入UCI数据长度
	void**		dst,	// 输出RAW数据的指针(BGR或BGRA格式)
	int*		stride, // 输出RAW数据的行间字节跨度(dst不为null时,stride不能传入null)
	int*		width,	// 输出图像的宽度值
	int*		height, // 输出图像的高度值
	int*		bit);	// 输出图像的bpp值(每像素位数)

// 释放UCIDecode输出的RAW数据指针所指的内存区
void __stdcall UCIFree(void* p);

// 设置ffmpeg的debug输出级别
void __stdcall UCIDebug(int level);

int __cdecl wmain(int argc, wchar_t** argv)
{
	int ret = 0, i;
	FILE* fp = 0;
	unsigned char* src = 0;
	unsigned char* dst = 0;
	int srclen, stride;
	int w, h, b;
	char use_stdin;
	char use_stdout;
	char buf[0x10];
	wchar_t dstname[512];
	static unsigned char bmp[0x36]=
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
		else if(!wcscmp(argv[i], L"-d"))
		{
			UCIDebug(0x7fffffff);
		}
		else
		{
			fwprintf(stderr, L"ERROR: unknown option '%ls'\n", argv[i]);
			ret = -2; goto end_;
		}
	}

	if(argc < 2)
	{
		fwprintf(stderr,	L"UCI (Ultra Compact Image) Decoder " UCI_VERSION_W L" [by dwing] " UCI_DATE_W L"\n"
							L"Usage:   ucidec <src_file.uci> [options]\n"
							L"Options: -o <filename> set output file name, default: <src_file>.bmp\n"
							L"         -d            enable debug info output\n"
							L"Notes:   output file is a 24 or 32 bit uncompressed BMP format file\n"
							L"         specify - instead of filename to use stdin/stdout\n"
							L"Examples:ucidec input.uci -o output.bmp\n"
							L"         ucidec input.uci -o -\n");
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

	if(use_stdin)
	{
		_setmode(_fileno(stdin), _O_BINARY);
		if(fread(buf, 1, 0x10, stdin) != 0x10) { fwprintf(stderr, L"ERROR: can't read stdin\n"); ret = -10; goto end_; }
		if((*(unsigned*)buf & 0xffffff) != *(unsigned*)"UCI") { fwprintf(stderr, L"ERROR: unknown format\n"); ret = -11; goto end_; }
		srclen = 16 + *(int*)(buf + 0x0c);
		if(srclen < 16) { fwprintf(stderr, L"ERROR: corrupt stdin data\n"); ret = -12; goto end_; }
		src = (unsigned char*)malloc(buf[3] != '4' ? srclen : srclen * 2);
		if(!src) { fwprintf(stderr, L"ERROR: can't alloc src memory\n"); ret = -13; goto end_; }
		memcpy(src, buf, 0x10);
		if(buf[3] != '4')
		{
			for(i = 0; i < srclen - 16; )
			{
				int ii = (int)fread(src + 16 + i, 1, srclen - 16 - i, stdin);
				if(ii <= 0)
				{
					fwprintf(stderr, L"ERROR: can't read stdin\n");
					ret = -14; goto end_;
				}
				i += ii;
			}
		}
		else
		{
			for(i = 0; i < srclen - 16 + 4;)
			{
				int ii = (int)fread(src + 16 + i, 1, srclen - 16 + 4 - i, stdin);
				if(ii <= 0)
				{
					fwprintf(stderr, L"ERROR: can't read stdin\n");
					ret = -15; goto end_;
				}
				i += ii;
			}
			i = *(int*)(src + srclen);
			if(i < 0) { fwprintf(stderr, L"ERROR: corrupt stdin data\n"); ret = -16; goto end_; }
			if(4 + i > srclen)
			{
				src = (unsigned char*)realloc(src, srclen + 4 + i);
				if(!src) { fwprintf(stderr, L"ERROR: can't realloc src memory\n"); ret = -17; goto end_; }
			}
			i += srclen + 4;
			for(srclen += 4; srclen < i;)
			{
				int ii = (int)fread(src + srclen, 1, i - srclen, stdin);
				if(ii <= 0)
				{
					fwprintf(stderr, L"ERROR: can't read stdin\n");
					ret = -18; goto end_;
				}
				srclen += ii;
			}
		}
	}
	else
	{
		fp = _wfopen(argv[1], L"rb");
		if(!fp) { fwprintf(stderr, L"ERROR: can't open src file\n"); ret = -20; goto end_; }
		if(fseek(fp, 0, SEEK_END)) { fwprintf(stderr, L"ERROR: can't seek src file\n"); ret = -21; goto end_; }
		if((srclen = ftell(fp)) < 0) { fwprintf(stderr, L"ERROR: can't get length of src file\n"); ret = -22; goto end_; }
		if(fseek(fp, 0, SEEK_SET)) { fwprintf(stderr, L"ERROR: can't seek src file\n"); ret = -23; goto end_; }
		src = (unsigned char*)malloc(srclen + 8); memset(src + srclen, 0, 8);
		if(!src) { fwprintf(stderr, L"ERROR: can't alloc src memory\n"); ret = -24; goto end_; }
		if((int)fread(src, 1, srclen, fp) != srclen) { fwprintf(stderr, L"ERROR: can't read src file\n"); ret = -25; goto end_; }
		if(fclose(fp)) { fwprintf(stderr, L"ERROR: can't close src file\n"); ret = -26; goto end_; }
		fp = 0;
	}

	i = UCIDecode(src, srclen, (void**)&dst, &stride, &w, &h, &b);
	if(i < 0) { fwprintf(stderr, L"ERROR: UCIDecode: return = %d\n", i); ret = -100 + i; goto end_; }
	*(int *)(bmp + 0x02) = 0x36 + (w * (b/8) + (b == 24 ? w & 3 : 0)) * h;
	*(int *)(bmp + 0x12) = w;
	*(int *)(bmp + 0x16) = h;
	*(char*)(bmp + 0x1c) = (char)b;

	if(use_stdout)
	{
		_setmode(_fileno(stdout), _O_BINARY);
		fp = stdout;
	}
	else
	{
		fp = _wfopen(dstname, L"wb");
		if(!fp) { fwprintf(stderr, L"ERROR: can't create '%ls'\n", dstname); ret = -30; goto end_; }
	}
	if((int)fwrite(bmp, 1, 0x36, fp) != 0x36) { fwprintf(stderr, L"ERROR: can't write '%ls'\n", dstname); ret = -31; goto end_; }
	if(b == 24)
	{
		for(i = h - 1; i >= 0; --i)
		{
			if((int)fwrite(dst + i * stride, 1, w * 3, fp) != (w * 3)) { fwprintf(stderr, L"ERROR: can't write '%ls'\n", dstname); ret = -32; goto end_; }
			if((int)fwrite("\0\0\0" 	   , 1, w & 3, fp) != (w & 3)) { fwprintf(stderr, L"ERROR: can't write '%ls'\n", dstname); ret = -33; goto end_; }
		}
	}
	else
	{
		for(i = h - 1; i >= 0; --i)
		{
			if((int)fwrite(dst + i * stride, 1, w * 4, fp) != (w * 4)) { fwprintf(stderr, L"ERROR: can't write '%ls'\n", dstname); ret = -34; goto end_; }
		}
	}
	if(!use_stdout) fclose(fp);
	fp = 0;

end_:
	if(dst) UCIFree(dst);
	if(src) free(src);
	if(fp && fp != stdout) fclose(fp);
	if(ret)
		fwprintf(stderr, L"ERROR CODE = %d\n", ret);
	else
		fwprintf(stderr, L"OK!\n");

	return ret;
}
