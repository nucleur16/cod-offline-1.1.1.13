The match server that Kernel33.dll hosts in-process, prebuilt:

- `libsh1x_host.a`: the server as one 32-bit MinGW (i686-w64-mingw32) static library whose only global symbols are
  its host API (`Sh1xHost_*`). `scripts/build.sh` links it into Kernel33.dll; it links the same with MSYS2's
  compiler on Windows and the cross compiler on Linux.
- `data/waypoints`, `data/mp`: the bot waypoints and bot class table, packed with `scripts/gsc` into
  `main/sh1x_svr_scripts.iwd` by `scripts/stage_server_data.py`.

The server is derived from the CoD4x server and is licensed under the GNU AGPL-3.0 (LICENSE.md).
