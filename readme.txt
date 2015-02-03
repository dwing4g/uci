UCI (Ultra Compact Image) 0.526 by dwing 2015-02-??


* 简介

UCI是一种基于H.264 intra帧压缩算法和数据流格式的静态图像封装格式.
而且不受图像宽高的一些限制,支持alpha透明通道等特性,
与JPEG,JPEG2000,HD-Photo等静态图像压缩算法相比具有更高的压缩效率.

目前公开的命令行工具暂时只支持24/32位BMP与UCI格式的相互转换,
编码工具只支持x264作为编码内核,
解码内核使用FFmpeg中的libavcodec解码器.

命令行使用方法详见ucienc/ucidec无参数时输出的帮助信息.


* 文件
ucienc.exe  --- 编码工具,支持BMP=>UCI的转换(需要在当前目录能够访问到x264.exe作为编码内核,可在x264.nl网站上下载到最新版本)
ucidec.exe  --- 解码工具,支持UCI=>BMP的转换(需要ucidec.dll内核)
ucidec.dll  --- 解码内核,可用于二次开发
                改名为Xuci.usr并放入XnView的Plugins目录中即可作为XnView读取UCI图像的插件
                改名为ifuci.spi并放入Susie的目录中即可作为Susie读取UCI图像的插件
yuv2bmp.exe --- 转换工具,支持YUV=>BMP的转换,输入图像是YUV420或YUV444格式,宽高必须能被2整除
imgdec.exe  --- 转换工具,支持BMP,TGA,PNG,JPG,GIF,TIF,PCX,PNM,JP2,JPC=>24/32-bitBMP的转换,内嵌CxImage图像库
readme.txt  --- 本文件
UCI.txt     --- UCI格式规范文本
TestUCI.c   --- ucidec.dll的C使用例子
TestUCI.cs  --- ucidec.dll的C#使用例子
TestUCI.lua --- ucidec.dll的LuaJIT使用例子
UCIDec.java --- ucidec.dll的Java使用例子


* 接口

// UCI格式图像解码,目前只支持输出24位BGR和32位的BGRA,返回0表示调用成功,负数表示错误,不支持多线程同时访问
int __stdcall UCIDecode(
        const void* src,    // 输入UCI数据指针(不能传入null,其它指针参数可以传入null表示不需要输出)
        int         srclen, // 输入UCI数据长度
        void**      dst,    // 输出RAW数据的指针(BGR或BGRA格式)
        int*        stride, // 输出RAW数据的行间字节跨度(dst不为null时,stride不能传入null)
        int*        width,  // 输出图像的宽度值
        int*        height, // 输出图像的高度值
        int*        bit);   // 输出图像的bpp值(每像素位数)

// 释放UCIDecode输出的RAW数据指针所指的内存区
void __stdcall UCIFree(void* p);

// 设置FFmpeg的debug输出(stderr)级别,level值是输出级别,最小是-8,最大是48,默认是0,具体含义详见FFmpeg的log.h中AV_LOG_*的定义
void __stdcall UCIDebug(int level);


* 其它说明

+ 所有相关工具和解码包以32位i686级别编译, 即支持Intel Pentium Pro和AMD Duron以上级别的CPU, ucidec.dll仅支持用于32位应用程序
+ 所有相关工具由于使用unicode的系统接口, 因此仅支持Windows2000以上版本的系统
+ 由于当前编解码内核所限,此程序所支持图像的宽高是有上限的,目前已知宽高均在8192之内没有问题,还没做大量测试证明宽高过高的情况
+ 编码输出YUV420格式时宽高会向偶数扩展


* 感谢

+ roozhou,zeas2以及在百度dwing贴吧中交流的朋友


* 版权及责任

+ 此版本是公开测试版本,可以免费用于非赢利用途,dwing不承担任何本软件导致的直接或间接不利后果和风险责任
+ 解码内核版权归FFmpeg所有
+ H.264标准归JVT(ITU-T&MPEG)所有


* 联系

欢迎反馈bug报告或建议:
电子邮箱: dwing@163.com
百度dwing贴吧: http://tieba.baidu.com/f?kw=dwing
旧网站: http://www.geocities.jp/dwingj


* 更新历史(以下版本的UCI格式及解码器均向后兼容)

0.526(2015-02-??) 更新FFmpeg,使用MinGW-GCC 4.9.2编译
0.525(2013-12-20) 更新FFmpeg,使用MinGW-GCC 4.8.2编译,发布五周年开源纪念版
0.524(2013-05-17) 更新FFmpeg,命令行程序使用unicode处理所有字符串
0.523(2013-04-28) 更新FFmpeg,修正上个版本解码的色彩空间转换问题
0.522(2013-04-05) 更新FFmpeg,使用MinGW-GCC 4.8.0编译,修正偶尔YUV420->RGB转换越界访问的bug,防止多线程同时解码,部分改用libswscale转换色彩空间,使画质更好
0.521(2012-07-06) 更新FFmpeg,使用MinGW-GCC 4.7.1编译
0.52 (2012-01-19) 更新FFmpeg,编码时YUV通道改用10bit输入,同时支持8/10bit x264编码及解码,支持指定x264的程序名,调整默认参数,取消支持非全范围YUV的编码及无用的Y通道修正
0.511(2011-12-23) 更新FFmpeg,使用MinGW-GCC 4.6.3pre编译,C++的例子改为C的例子,修正susie接口在MangaMeeya中的崩溃bug
0.51 (2011-09-11) 更新FFmpeg,增加解码的JNI接口和Java的使用例子,一些细节改进
0.5  (2011-07-29) 更新FFmpeg,新增4种格式的编解码,支持全范围YUV映射,直接支持H.264的YUV444编码,取消支持以前的YUV444编码,更新默认subme参数为11
0.494(2011-07-15) 更新FFmpeg,增加UCIDebug接口,改用MinGW-GCC 4.6.1编译
0.493(2011-03-13) 更新FFmpeg,改用VC2010(sp1)编译,编码质量的范围从1~50改为0~51,修正0.492版质量0的alpha通道的解码问题,增加LuaJIT的使用例子
0.492(2011-02-01) 更新FFmpeg,改用VC2010(sp1-beta)编译
0.491(2010-11-13) 更新FFmpeg
0.49 (2010-07-26) 更新FFmpeg,支持新版x264设置图像大小的参数
0.48 (2010-05-22) 更新FFmpeg,取消了作用不大的PGO优化,修正编码器对新版x264输出无用信息的消除,修正yuv2bmp对YUV420处理的严重bug
0.47 (2010-04-03) 更新FFmpeg,改用VC2010编译,启用PGO优化,取消了作用不大的MMX优化
0.46 (2010-03-13) 更新FFmpeg,改用VC2010(rc1)编译
0.45 (2010-01-17) 更新FFmpeg,修正前一版本的几个bug
0.44 (2009-12-12) 更新FFmpeg,改用VC2010(beta2)编译,使用线性插值改进YUV420到RGB的转换,修正访问susie插件接口可能导致崩溃的bug
0.43 (2009-11-28) 更新FFmpeg,改用VC2010(beta1)编译,去掉了一些无用的容错处理(减小体积并可能提升解码速度)
0.42 (2009-07-12) 更新FFmpeg,禁用interlace(减小体积并可能提升解码速度),对新版x264参数的修正,修正上一版本MMX解码优化没有启用的bug
0.41 (2009-06-27) 更新FFmpeg,少部分解码代码使用MMX优化,RGB->YUV转换略微调整,修正例子程序中的错误
0.4  (2009-05-30) 更换解码接口,YUV420相关转换使用MMX优化,更新FFmpeg,增加imgdec工具,增加Susie解码插件的支持
0.31 (2009-04-18) 更新FFmpeg,增加UCI格式描述和C++/C#的使用例子
0.30 (2009-02-27) 更新FFmpeg,为了避免一些兼容性和性能问题程序不再加壳
0.29 (2009-02-15) 支持直接编码YUV420格式,默认去掉x264的一些无用信息,修正一些参数和细节
0.28 (2009-02-12) 更新FFmpeg,增加YUV2BMP程序,--quiet参数改为-quiet,少量细节修正
0.27 (2009-02-03) 修正XnView浏览32位图偏色bug,解码初始化改在载入ucidec.dll时执行
0.26 (2009-02-02) 更新FFmpeg,增加--quiet参数,修正图像宽度为奇数可能对图像解码有误的bug
0.25 (2009-01-31) 修正图像宽度或高度为某些值时无法正常解码的bug
0.24 (2009-01-28) 支持分离YUV编码,编码时增加输出YUV文件的选项,一些细节修正
0.23 (2009-01-23) 提升解码速度,改善UV通道的编码质量,修正某种情况下x264进程阻塞的bug
0.22 (2009-01-18) 支持目前最新版XnView读取UCI的插件,修正可能导致程序阻塞的bug
0.21 (2009-01-17) 编解码器支持stdin/stdout,编码器不再使用临时文件,修正一些细微bugs
0.2  (2009-01-10) 减小编解码器体积,优化解码速度,修正解码接口返回值的2个bugs
0.11 (2008-12-27) 编码器增加Y通道修正,其它一些细节修正及速度略微优化,FFmpeg版本:svn16354
0.1  (2008-12-20) 初始内部测试版本,FFmpeg版本:svn16238
