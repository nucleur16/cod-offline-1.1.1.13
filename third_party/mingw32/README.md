32-bit MinGW static libraries Kernel33.dll links, with their headers, so the DLL builds the same with MSYS2 on
Windows and with the i686-w64-mingw32 cross compiler on Linux (scripts/toolchain.inc.sh):

- libopus.a, include/opus/: Opus 1.5.2 (MSYS2 mingw-w64-i686-opus 1.5.2-1), BSD licence: licenses/opus-COPYING
- libz.a, include/zlib.h, zconf.h: zlib 1.3.1 (MSYS2 mingw-w64-i686-zlib 1.3.1-1), zlib licence: licenses/zlib-LICENSE
