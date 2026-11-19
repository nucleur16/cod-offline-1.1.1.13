# CoD Online 1.1.1.13 offline (Kernel33.dll)

`Kernel33.dll` runs the 1.1.1.13 client (`codoMP_client_shipRetail.exe`) fully offline. The Demonware lobby and the
match server both run inside the game, so you don't need TenProtect, a launcher, `.ini` files or a separate server.
Run the exe and play: Multiplayer with bots, Zombies, Survival and Spec Ops.

Just want to play? Download the release zip and follow its `INSTALL.txt`. The rest of this page is for building it
yourself.

## Build

Windows or Linux. You need:

- **A 1.1.1.13 install**, read only: the build edits some of its stock zones and patches a copy of
  `codoMP_client_shipRetail.exe`.
- **Python 3.10+** with `pip install pefile pycryptodome pillow`.
- **A 32-bit MinGW compiler:**
  - Windows: [MSYS2](https://www.msys2.org) at `C:\msys64` (or set `MSYS2_ROOT`), then in its shell
    `pacman -S mingw-w64-i686-gcc make nasm`. Python must be the Windows one, on PATH.
  - Linux (Debian/Ubuntu): `sudo apt install gcc-mingw-w64-i686 g++-mingw-w64-i686 make nasm curl`.

Everything else is in the repository (the match server's source, Opus, zlib, the zone key), and SDL2 is downloaded
on the first build. The match server is compiled from `third_party/sh1x/src` as part of the build.

```sh
git clone <this repository>
cd <its folder>
build.bat "D:\path\to\Call of Duty Online 1.1.1.13"        # Windows
sh build.sh "/path/to/Call of Duty Online 1.1.1.13"          # Linux
```

Both build `Kernel33.dll`, the match server's pack, the patch zones and the patched exe into `dist`:

```
dist\
  Kernel33.dll
  codoMP_client_shipRetail.exe        loads Kernel33.dll
  client_shipRetail\patch\*.ff        patch zones (menus, HUD, map lists, locale, ...)
  client_shipRetail\chinese\*.ff      zones that replace or add to stock ones
  main\sh1x_svr_scripts.iwd           the match server's scripts, bot classes and waypoints
  main\iw_image_16.iwd
```

## Install (drag and drop)

1. Open `dist\` and select everything in it (Ctrl+A).
2. Drag it onto your 1.1.1.13 game folder, the one containing `codoMP_client_shipRetail.exe`.
3. When Windows asks, choose **Replace the files in the destination**.
4. Start `codoMP_client_shipRetail.exe`.

From Linux: `cp -r dist/. "/path/to/Call of Duty Online 1.1.1.13/"`.

A finished install only needs:

```
codoMP_client_shipRetail.exe  Kernel33.dll  mss32.dll  binkw32.dll  bink2w32.dll  D3DX9_43.dll
TenProxy.dll  TerSafe.dll  GbSpy.dll  steam_api.dll  localization.txt  splash.bmp  logo.bmp
miles\  client_shipRetail\{chinese,patch,temp}\  main\
```

The game creates `players\` (your profile and stats) and `p2p\offline\` on its own. You can delete `TenSLX*`,
`BugTrace*`, the updater DLLs, `p2p\sh1x_host.dll`, `p2p\survival_maps.txt` and old `.ini` files. Delete an old
`main\sh1x_rawfiles\` too unless you edit scripts: it overrides the scripts in `sh1x_svr_scripts.iwd` file by file.

## Play

- **Menus:** create a room (Multiplayer, Zombies, Survival or Spec Ops) and press Ready.
- **Console:** `devmap <map> [gametype]` loads a map with cheats on, and `map <map> [gametype]` loads it without them.
  The gametype defaults to `war` for `mp_`, `zm` for `zm_` and `coop` for `coop_` maps; `survival` plays Survival on
  an MP map.
- **Name:** `name <new name>` in the console, in the menus (not during a match). It is kept for your profile.
- **Map lists:** they show only maps whose `.ff` is in `client_shipRetail\chinese\` or `usermaps\<map>\`. Set
  `K33_MAPLIST_ALL=1` to list every map.
- **Profile:** `+set dw_username <name>` picks a profile (default `Player`).
- **Logs:** `Kernel33.log` next to the exe, and `p2p\offline\lobby.log`.

## Development

- `sh scripts/build.sh` builds only the DLL (it copies it to `..\Kernel33.dll` unless `K33_NO_COPY=1`).
- `sh scripts/build-dev.sh` builds the diagnostic build, with a developer console and test hooks.
- `python scripts/build_zones.py --help` lists the zone build's options.
- The match server's GSC is `scripts/gsc`. To try a change without rebuilding, put just the changed files under
  `main\sh1x_rawfiles\` in the game folder: the host reads them over the pack's copies.
- For map making, see [sh1xo-radiant, branch codol-1.1.1.13](https://github.com/nucleur16/sh1xo-radiant/tree/codol-1.1.1.13).

## Licences

- The match server linked into `Kernel33.dll` (source in `third_party/sh1x/src`) is derived from the CoD4x server
  and is under the GNU AGPL-3.0: `third_party/sh1x/LICENSE.md`. It includes Mbed TLS (Apache-2.0) and
  LibTomCrypt (public domain) in `third_party/sh1x/src/src/mbedtls` and `src/tomcrypt`.
- Bundled libraries: Opus, zlib (`third_party/mingw32/licenses`); SDL2 (zlib licence, downloaded at build time).
