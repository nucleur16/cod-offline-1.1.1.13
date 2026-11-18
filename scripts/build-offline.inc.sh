#!/bin/sh
# Sourced by scripts/build.sh, scripts/build-dev.sh (from src/; every build) and tests/offline/build_dwtest.sh
# (from the repository root, with K33_OFFLINE_SRC=src/offline K33_OFFLINE_INC=scripts/build-offline.inc.sh):
# compiles the Demonware lobby (src/offline/*.cpp, docs/OFFLINE_LOBBY.md) with g++ into an object folder.
# No exceptions, no RTTI: nothing C++ unwinds into C1 or Kernel33's C frames.
#
#   k33_offline_objects <objdir>   -> OFFLINE_OBJS (rebuilt when a source or header is newer)
#
#   OFFLINE_SKIP="dw_rooms.cpp dw_marketplace.cpp"   leave modules out (their dw_stubs.cpp
#                                                    placeholders answer instead); tests only.
. "$(dirname "${K33_OFFLINE_INC:-../scripts/build-offline.inc.sh}")/toolchain.inc.sh"
OFFLINE_CXXFLAGS="-std=gnu++11 -O2 -Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti -fno-threadsafe-statics -ffunction-sections -fdata-sections"
K33_OFFLINE_SRC=${K33_OFFLINE_SRC:-offline}
K33_OFFLINE_INC=${K33_OFFLINE_INC:-../scripts/build-offline.inc.sh}
k33_offline_objects() {
    _dir=$1
    mkdir -p "$_dir"
    OFFLINE_OBJS=""
    _have=""
    _srcs=""
    for _src in "$K33_OFFLINE_SRC"/*.cpp; do
        _base=$(basename "$_src")
        case " ${OFFLINE_SKIP:-} " in *" $_base "*) continue;; esac
        _srcs="$_srcs $_src"
        _name=$(echo "${_base%.cpp}" | sed 's/^dw_//' | tr 'a-z' 'A-Z')
        _have="$_have -DDW_HAVE_$_name"
    done
    # dw_stubs.cpp depends on the module list: rebuild it when that changes.
    if [ "$(cat "$_dir/.have" 2>/dev/null)" != "$_have" ]; then
        rm -f "$_dir/dw_stubs.o"
        echo "$_have" > "$_dir/.have"
    fi
    _newest=$(ls -t "$K33_OFFLINE_SRC"/*.h "$K33_OFFLINE_INC" | head -1)
    for _src in $_srcs; do
        _o="$_dir/$(basename "${_src%.cpp}").o"
        if [ ! -f "$_o" ] || [ "$_src" -nt "$_o" ] || [ "$_newest" -nt "$_o" ]; then
            echo "g++ $_src"
            "$CXX" $OFFLINE_CXXFLAGS $_have -c "$_src" -o "$_o" || exit 1
        fi
        OFFLINE_OBJS="$OFFLINE_OBJS $_o"
    done
}
