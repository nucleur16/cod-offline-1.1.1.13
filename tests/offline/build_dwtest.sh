#!/bin/sh
# Builds tests/offline/dwtest.exe: the offline Demonware core (offline/*.cpp) as a standalone loopback
# server. DWTEST_OBJ / DWTEST_OUT pick another object folder / output; OFFLINE_SKIP leaves modules out.
# Same compiler and C++ flags as the DLL's offline objects (build-offline.inc.sh).
set -e
cd "$(dirname "$0")/../.."
. ./build-offline.inc.sh
k33_offline_objects "${DWTEST_OBJ:-obj_offline_test}"
"$BIN/g++.exe" $OFFLINE_CXXFLAGS -o "${DWTEST_OUT:-tests/offline/dwtest.exe}" tests/offline/dwtest_main.cpp $OFFLINE_OBJS \
    -static -static-libgcc -static-libstdc++ -lz -lws2_32
echo "built ${DWTEST_OUT:-tests/offline/dwtest.exe}"
