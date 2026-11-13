#!/bin/sh
# Builds tests/offline/dwtest.exe: the offline Demonware core (src/offline/*.cpp) as a standalone loopback
# server. DWTEST_OBJ / DWTEST_OUT pick another object folder / output; OFFLINE_SKIP leaves modules out.
# Same compiler and C++ flags as the DLL's offline objects (scripts/build-offline.inc.sh); the tests see the same
# DW_HAVE_<MODULE> list ($_have), so a module's self-test can compile out when the module is skipped.
set -e
cd "$(dirname "$0")/../.."
K33_OFFLINE_SRC=src/offline
K33_OFFLINE_INC=scripts/build-offline.inc.sh
. ./scripts/build-offline.inc.sh
k33_offline_objects "${DWTEST_OBJ:-obj_offline_test}"
"$BIN/g++.exe" $OFFLINE_CXXFLAGS $_have -o "${DWTEST_OUT:-tests/offline/dwtest.exe}" tests/offline/dwtest_main.cpp tests/offline/dwtest_store.cpp tests/offline/dwtest_rooms.cpp tests/offline/dwtest_market.cpp tests/offline/dwtest_career.cpp $OFFLINE_OBJS \
    -static -static-libgcc -static-libstdc++ -lz -lws2_32
echo "built ${DWTEST_OUT:-tests/offline/dwtest.exe}"
