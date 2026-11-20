The match server that Kernel33.dll hosts in-process. It is derived from the CoD4x server and is licensed under the
GNU AGPL-3.0 (LICENSE.md).

- `src/`: the server's full source, copied from the server repository (codol_1.1.1.13_alpha_server; commit in
  `src/VENDORED.txt`) by `python tools/vendor_server.py <server checkout>`. Left out: its submodules, the CoD4
  executables it disassembles (`tools/cod4_dasm`), recorded sessions, binaries and Mbed TLS's test certificates
  (`mbedtls/certs.c`). `scripts/build.sh` builds it with the client's 32-bit MinGW toolchain
  (`make -C third_party/sh1x/src host-lib`, needs make and nasm) into `src/bin/libsh1x_host.a`: the server as one
  static library whose only global symbols are its host API (`Sh1xHost_*`), linked into Kernel33.dll. It includes
  Mbed TLS (Apache-2.0, `src/src/mbedtls/LICENSE`) and LibTomCrypt (public domain, `src/src/tomcrypt`).
- `data/waypoints`, `data/mp`: the bot waypoints and bot class table, packed with `scripts/gsc` into
  `main/sh1x_svr_scripts.iwd` by `scripts/stage_server_data.py`.
