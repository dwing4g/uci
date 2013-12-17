#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

typedef int (__stdcall* F_UCIDecode)(const void *src, int srclen, void** dst, int* stride, int* w, int* h, int* b);
typedef int (__stdcall* F_UCIFree)(void* p);
typedef int (__stdcall* F_UCIDebug)(int level);

int main()
{
	unsigned char *src, *dst;
	int w, h, b, r, srclen, stride, i;
	FILE* fp;

	HMODULE hucimod = LoadLibrary("ucidec.dll");
	F_UCIDecode UCIDecode;
	F_UCIFree UCIFree;
	F_UCIDebug UCIDebug;
	if(!hucimod)
	{
		printf("ERROR: can not load ucidec.dll\n");
		return -1;
	}

	UCIDecode = (F_UCIDecode)GetProcAddress(hucimod, "UCIDecode");
	UCIFree = (F_UCIFree)GetProcAddress(hucimod, "UCIFree");
	UCIDebug = (F_UCIDebug)GetProcAddress(hucimod, "UCIDebug");
	if(!UCIDecode || !UCIFree || !UCIDebug)
	{
		printf("ERROR: can not load UCIDecode or UCIFree or UCIDebug from ucidec.dll\n");
		return -2;
	}

	fp = fopen("test.uci", "rb");
	if(!fp)
	{
		printf("ERROR: can not open test.uci\n");
		return -3;
	}

	fseek(fp, 0, SEEK_END);
	srclen = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	src = (unsigned char*)malloc(srclen);
	fread(src, 1, srclen, fp);
	fclose(fp);

	UCIDebug(0x7fffffff);
	r = UCIDecode(src, srclen, (void**)&dst, &stride, &w, &h, &b);
	free(src);
	if(r < 0)
	{
		printf("ERROR: UCIDecode failed (return %d)\n", r);
		return -4;
	}
	printf("INFO: width x height x bit : %d x %d x %d\n", w, h, b);

	fp = fopen("test.rgb", "wb");
	if(!fp)
	{
		UCIFree(dst);
		printf("ERROR: can not create test.rgb\n");
		return -5;
	}
	for(i = 0; i < h; ++i)
		fwrite(dst + i * stride, 1, w * (b/8), fp);
	fclose(fp);

	UCIFree(dst);

	return 0;
}
