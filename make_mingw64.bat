@echo off
setlocal
mode con cols=120 lines=3000
pushd %~dp0

rem mingw-gcc: http://files.1f0.de/mingw/
rem jdk: https://adoptopenjdk.net/
rem ffmpeg: https://git.ffmpeg.org/ffmpeg.git
rem ./configure --arch=x86_64 --disable-all --enable-asm --enable-x86asm --enable-swscale-alpha --disable-doc --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages --disable-pthreads --disable-w32threads --disable-os2threads --disable-network --disable-dct --disable-dwt --disable-lsp --disable-lzo --disable-mdct --disable-rdft --disable-fft --disable-everything --enable-avcodec --enable-avutil --enable-swscale --enable-decoder=h264 --enable-decoder=hevc --disable-debug --disable-dxva2 --disable-iconv --cross-prefix=x86_64-w64-mingw32- --target-os=win64
rem ffbuild/config.mak: -O3 => -Ofast
rem config.h: #define HAVE_BCRYPT 1 => 0
rem ./configure --toolchain=msvc --arch=x86_64 --enable-asm --enable-x86asm --disable-shared --enable-static --disable-all --enable-swscale-alpha --disable-doc --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages --disable-pthreads --disable-w32threads --disable-os2threads --disable-network --disable-dct --disable-dwt --disable-lsp --disable-lzo --disable-mdct --disable-rdft --disable-fft --disable-everything --enable-avcodec --enable-avutil --enable-swscale --enable-decoder=h264 --enable-decoder=hevc --enable-lto --disable-debug --disable-dxva2 --disable-iconv
rem ffbuild/config.mak: -O2 => -O2 -Ob2 -Oi -Ot -Oy -GF -GS- -MT; -flto => -GL / -LTCG
rem config.h: #define HAVE_BCRYPT 1 => 0

set MINGW_HOME=C:\mingw
set FFMPEG_HOME=C:\ffmpeg
rem set JAVA_HOME=C:\jdk-11.0.9

set GCCEXE=%MINGW_HOME%\bin\x86_64-w64-mingw32-gcc.exe
set GPPEXE=%MINGW_HOME%\bin\x86_64-w64-mingw32-g++.exe
set DLLEXE=%MINGW_HOME%\bin\x86_64-w64-mingw32-dlltool.exe
set LIBPATH=%MINGW_HOME%\x86_64-w64-mingw32\lib
set AVCODEC_HOME=%FFMPEG_HOME%\libavcodec
set AVUTIL_HOME=%FFMPEG_HOME%\libavutil
set AVSWSCALE_HOME=%FFMPEG_HOME%\libswscale
set JDK_HOME=%JAVA_HOME%
rem set FLTO=
set FLTO=-flto -fwhole-program

set GCC1=%GCCEXE% -m64 -std=gnu99 -pipe -static -shared -Ofast -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -s -w -Wl,--image-base,0x10000000 -Wl,--kill-at -DNDEBUG -DHAVE_AV_CONFIG_H -I. "-I%JDK_HOME%\include" "-I%JDK_HOME%\include\win32" "-I%FFMPEG_HOME%" "-I%AVCODEC_HOME%" "-I%AVUTIL_HOME%" "-I%AVSWSCALE_HOME%"

rem step 1/2: -fprofile-generate
rem step 2/2: -fprofile-use
rem -D__USE_MINGW_ANSI_STDIO=1
set GCC2=%GCCEXE% -pipe -static -m64 -Ofast -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -s -Wall -Wextra -Wno-strict-aliasing -Wno-missing-field-initializers -Wl,--kill-at -municode -DNDEBUG -D_UNICODE -DUNICODE
set GPP2=%GPPEXE% -pipe -static -m64 -Ofast -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -s -Wall -Wextra -Wno-strict-aliasing -Wno-missing-field-initializers -Wl,--kill-at -municode -DNDEBUG -D_UNICODE -DUNICODE

%GCC1% %FLTO% -o ucidec64.dll ucidec_dll.c "%AVCODEC_HOME%\libavcodec.a" "%AVUTIL_HOME%\libavutil.a" "%AVSWSCALE_HOME%\libswscale.a" "%LIBPATH%\libkernel32.a"

%DLLEXE% -d ucidec.def --dllname ucidec64.dll --output-lib ucidec64.lib --kill-at

%GPP2% -o ucienc64.exe ucienc.cpp
%GCC2% -o ucidec64.exe ucidec.c ucidec64.lib
%GCC2% -o yuv2bmp64.exe yuv2bmp.c

pause
