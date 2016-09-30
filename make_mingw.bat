@echo off
setlocal
mode con cols=120 lines=3000
pushd %~dp0

rem mingw-gcc: http://files.1f0.de/mingw/
rem jdk: http://www.oracle.com/technetwork/java/javase/downloads/index.html
rem ffmpeg: git://source.ffmpeg.org/ffmpeg.git
rem ./configure --cpu=i686 --disable-all --enable-swscale-alpha --disable-doc --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages --disable-pthreads --disable-w32threads --disable-os2threads --disable-network --disable-dct --disable-dwt --disable-lsp --disable-lzo --disable-mdct --disable-rdft --disable-fft --disable-everything --enable-avcodec --enable-avutil --enable-swscale --enable-decoder=h264 --enable-lto --disable-debug --disable-dxva2 --disable-iconv
rem config.mak: CFLAGS=... -Ofast -ffat-lto-objects ...
rem ./configure --toolchain=msvc --arch=x86 --enable-yasm --enable-asm --disable-shared --enable-static --cpu=i686 --disable-all --enable-swscale-alpha --disable-doc --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages --disable-pthreads --disable-w32threads --disable-os2threads --disable-network --disable-dct --disable-dwt --disable-lsp --disable-lzo --disable-mdct --disable-rdft --disable-fft --disable-everything --enable-avcodec --enable-avutil --enable-swscale --enable-decoder=h264 --enable-lto --disable-debug --disable-dxva2 --disable-iconv
rem config.mak: -O2 => -O2 -Ob2 -Oi -Ot -Oy -GF -GS- -MT; -flto => -GL / -LTCG

set MINGW_HOME=D:\mingw
set FFMPEG_HOME=D:\ffmpeg-3.1.3
rem set JAVA_HOME=C:\Program Files\Java\jdk1.8.0_102

set GCCEXE=%MINGW_HOME%\bin\gcc.exe
set GPPEXE=%MINGW_HOME%\bin\g++.exe
set DLLEXE=%MINGW_HOME%\bin\dlltool.exe
set LIBPATH=%MINGW_HOME%\i686-w64-mingw32\lib
set AVCODEC_HOME=%FFMPEG_HOME%\libavcodec
set AVUTIL_HOME=%FFMPEG_HOME%\libavutil
set AVSWSCALE_HOME=%FFMPEG_HOME%\libswscale
set JDK_HOME=%JAVA_HOME%
rem set FLTO=
set FLTO=-flto -fwhole-program

set GCC1=%GCCEXE% -m32 -std=gnu99 -pipe -static -shared -march=i686 -Ofast -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -s -w -Wl,--image-base,0x10000000 -Wl,--kill-at -DNDEBUG -DHAVE_AV_CONFIG_H -I. "-I%JDK_HOME%\include" "-I%JDK_HOME%\include\win32" "-I%FFMPEG_HOME%" "-I%AVCODEC_HOME%" "-I%AVUTIL_HOME%" "-I%AVSWSCALE_HOME%"

rem step 1/2: -fprofile-generate
rem step 2/2: -fprofile-use
rem -D__USE_MINGW_ANSI_STDIO=1
set GCC2=%GCCEXE% -pipe -static -march=i686 -Ofast -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -s -Wall -Wextra -Wno-strict-aliasing -Wno-missing-field-initializers -Wl,--kill-at -municode -DNDEBUG -D_UNICODE -DUNICODE
set GPP2=%GPPEXE% -pipe -static -march=i686 -Ofast -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -s -Wall -Wextra -Wno-strict-aliasing -Wno-missing-field-initializers -Wl,--kill-at -municode -DNDEBUG -D_UNICODE -DUNICODE

%GCC1% %FLTO% -o ucidec.dll ucidec_dll.c "%AVCODEC_HOME%\libavcodec.a" "%AVUTIL_HOME%\libavutil.a" "%AVSWSCALE_HOME%\libswscale.a" "%LIBPATH%\libkernel32.a"

%DLLEXE% -d ucidec.def --dllname ucidec.dll --output-lib ucidec.lib --kill-at

%GPP2% -o ucienc.exe ucienc.cpp
%GCC2% -o ucidec.exe ucidec.c ucidec.lib
%GCC2% -o yuv2bmp.exe yuv2bmp.c

pause
