#!/bin/sh
# Build the 32-bit Linux sh1x server (sh1x_dedrun) for the Docker package.
#
# Run on Linux or in WSL (Ubuntu 24.04: gcc-multilib g++-multilib nasm make):
#     sh tools/build_linux.sh
#
# Everything lands outside the Windows build's paths, so this never touches
# lib/ (PE archives) or bin/sh1x_dedrun.exe:
#     objects   obj/sh1x-linux/
#     libraries lib-native/  (32-bit ELF mbedTLS/libtomcrypt, made on demand)
#     output    obj/sh1x-linux-bin/sh1x_dedrun
set -eu
cd "$(dirname "$0")/.."

JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}

if [ ! -f lib-native/libtomcrypt.a ] || [ ! -f lib-native/libmbedtls.a ]; then
    # test-external builds 32-bit host copies of both libraries into lib-native/.
    make GAME=sh1x OBJ_DIR=obj/sh1x-linux test-external
fi

mkdir -p obj/sh1x-linux-bin
# EXTERNAL= skips the link rule's order-only mbedtls/tomcrypt sub-builds, which
# write into lib/ -- the Windows build's PE archives on a shared checkout.
make -j"$JOBS" GAME=sh1x OBJ_DIR=obj/sh1x-linux LIB_DIR=lib-native BIN_DIR=obj/sh1x-linux-bin EXTERNAL= \
    obj/sh1x-linux-bin/sh1x_dedrun
file obj/sh1x-linux-bin/sh1x_dedrun 2>/dev/null || ls -l obj/sh1x-linux-bin/sh1x_dedrun
