#!/bin/sh
# Build a local DLL candidate with WSL's existing MinGW cross compiler.
# Usage: sh tools/build_local_audit_candidate.sh /absolute/path/to/deps
# This writes only candidate-local/ (in the repository root) and never installs or starts a game client.
set -eu
cd "$(dirname "$0")/../src"
task_dep_root=${1:?pass the local dependency receipt directory}
test -f "$task_dep_root/sdl/lib/libSDL2.a"
test -f "$task_dep_root/msys/lib/libopus.a"
test -f "$task_dep_root/msys/lib/libz.a"
test -f unpacked.z
mkdir -p ../candidate-local
i686-w64-mingw32-windres -O coff -i kernel33.rc -o ../candidate-local/kernel33_res.o
i686-w64-mingw32-gcc -DCODOL_LOCAL_TESTING=1 -DCODOL_DIAGNOSTICS=1 \
    -O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type \
    -Wno-missing-field-initializers -shared -s -static -static-libgcc \
    -I"$task_dep_root/sdl/include" -I"$task_dep_root/msys/include" \
    -o ../candidate-local/Kernel33.dll kernel33.c ../candidate-local/kernel33_res.o kernel33.def \
    -L"$task_dep_root/sdl/lib" -L"$task_dep_root/msys/lib" \
    -lSDL2 -lsetupapi -lcfgmgr32 -lversion -loleaut32 -luuid \
    -lopus -lz -limm32 -lgdi32 -lshlwapi -lole32 -lwinmm \
    -lws2_32 -liphlpapi -lpsapi -ladvapi32 -lshell32 \
    -Wl,--enable-stdcall-fixup -Wl,--dynamicbase -Wl,--nxcompat
sha256sum ../candidate-local/Kernel33.dll
file ../candidate-local/Kernel33.dll
