The match server that Kernel33.dll hosts in-process. It is derived from the CoD4x server and is licensed under the
GNU AGPL-3.0 (LICENSE.md).

- `src/`: the server's source (when present). `scripts/build.sh` builds it with the client's 32-bit MinGW toolchain
  (`make -C third_party/sh1x/src host-lib`, needs make and nasm) into `src/bin/libsh1x_host.a`: the server as one
  static library whose only global symbols are its host API (`Sh1xHost_*`), linked into Kernel33.dll. It includes
  Mbed TLS (Apache-2.0, `src/src/mbedtls/LICENSE`) and LibTomCrypt (public domain, `src/src/tomcrypt`).
- `libsh1x_host.a` (when there is no `src/`): the same library, prebuilt.
- `data/waypoints`, `data/mp`: the bot waypoints and bot class table, packed with `scripts/gsc` into
  `main/sh1x_svr_scripts.iwd` by `scripts/stage_server_data.py`.
