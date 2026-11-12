#!/bin/sh
# Sourced by build.sh, build-dev.sh and tests/offline/build_dwtest.sh: compiles the offline Demonware
# lobby (offline/*.cpp, docs/OFFLINE_LOBBY.md) with g++ into an object folder, like build-dev.sh does
# for the vision editor. No exceptions, no RTTI: nothing C++ unwinds into C1 or Kernel33's C frames.
#
#   k33_offline_objects <objdir>   -> OFFLINE_OBJS (rebuilt when a source or header is newer)
BIN=${BIN:-/c/msys64/mingw32/bin}
PATH="$BIN:$PATH"   # g++ loads cc1plus and its DLLs from its own folder
export PATH
OFFLINE_CXXFLAGS="-std=gnu++11 -O2 -Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti -fno-threadsafe-statics -ffunction-sections -fdata-sections"
k33_offline_objects() {
    _dir=$1
    mkdir -p "$_dir"
    OFFLINE_OBJS=""
    _newest=$(ls -t offline/*.h build-offline.inc.sh | head -1)
    for _src in offline/*.cpp; do
        _o="$_dir/$(basename "${_src%.cpp}").o"
        if [ ! -f "$_o" ] || [ "$_src" -nt "$_o" ] || [ "$_newest" -nt "$_o" ]; then
            echo "g++ $_src"
            "$BIN/g++.exe" $OFFLINE_CXXFLAGS -c "$_src" -o "$_o" || exit 1
        fi
        OFFLINE_OBJS="$OFFLINE_OBJS $_o"
    done
}
