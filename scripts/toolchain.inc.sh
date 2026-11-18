#!/bin/sh
# Sourced by scripts/build.sh, scripts/build-server.sh, scripts/build-offline.inc.sh and tests/offline/build_dwtest.sh:
# picks the 32-bit MinGW compiler that builds Kernel33.dll and the match server.
#
#   Windows: MSYS2's mingw32 gcc (C:\msys64, or MSYS2_ROOT; K33_MINGW_BIN=/c/msys64/mingw32/bin to name it).
#   Linux:   the i686-w64-mingw32 cross compiler on PATH (Debian/Ubuntu: gcc-mingw-w64-i686 g++-mingw-w64-i686
#            binutils-mingw-w64-i686; Arch: mingw-w64-gcc). K33_MINGW_PREFIX=i686-w64-mingw32- names another prefix.
#
# Sets CC, CXX, WINDRES, K33_HOST_TOOLS (the binutils prefix for the server makefile's HOST_TOOLS) and PYTHON.
# Opus and zlib come from third_party/mingw32 on both, so the two builds link the same libraries.
if [ -z "${K33_TOOLCHAIN_DONE:-}" ]; then
    K33_TOOLCHAIN_DONE=1
    if [ -z "${K33_MINGW_BIN:-}" ] && [ -z "${K33_MINGW_PREFIX:-}" ]; then
        _msys=$(printf '%s' "${MSYS2_ROOT:-C:/msys64}" | sed 's#\\#/#g; s#^\([A-Za-z]\):#/\1#')
        _msys=$(printf '%s' "$_msys" | sed 's#^/\([A-Z]\)/#/\L\1/#')
        [ -x "$_msys/mingw32/bin/gcc.exe" ] && K33_MINGW_BIN=$_msys/mingw32/bin
    fi
    if [ -n "${K33_MINGW_BIN:-}" ]; then
        CC="$K33_MINGW_BIN/gcc.exe"
        CXX="$K33_MINGW_BIN/g++.exe"
        WINDRES="$K33_MINGW_BIN/windres.exe"
        K33_HOST_TOOLS=
        # g++ loads cc1plus and its DLLs from its own folder; make and nasm come from MSYS2's usr/bin
        PATH="$K33_MINGW_BIN:$(dirname "$(dirname "$K33_MINGW_BIN")")/usr/bin:$PATH"
    else
        _p=${K33_MINGW_PREFIX:-i686-w64-mingw32-}
        command -v "${_p}gcc" >/dev/null 2>&1 || {
            echo "no 32-bit MinGW compiler: install MSYS2 (Windows) or ${_p}gcc (Linux: gcc-mingw-w64-i686 g++-mingw-w64-i686)" >&2
            exit 1
        }
        # Debian/Ubuntu: the posix-thread variant if installed (either links; this matches MSYS2's)
        if command -v "${_p}gcc-posix" >/dev/null 2>&1; then CC="${_p}gcc-posix"; CXX="${_p}g++-posix"
        else CC="${_p}gcc"; CXX="${_p}g++"; fi
        WINDRES="${_p}windres"
        K33_HOST_TOOLS=$_p
    fi
    # Windows: "python" (build.bat checks it; python3 there is often the Store stub); Linux: python3
    [ -n "${PYTHON:-}" ] || { [ -n "${K33_MINGW_BIN:-}" ] && PYTHON=python || PYTHON=python3; }
    export PATH CC CXX WINDRES K33_HOST_TOOLS PYTHON K33_TOOLCHAIN_DONE
fi
