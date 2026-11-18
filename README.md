# CoD Online 1.1.1.13 offline (Kernel33.dll)

`Kernel33.dll` runs the 1.1.1.13 client (`codoMP_client_shipRetail.exe`) fully offline. The Demonware lobby and the
match server both run inside the game, so you don't need C1.exe, TenProtect, a launcher, `.ini` files or a separate
server. Run the exe and play.

## Build

You need:

- **MSYS2** at `C:\msys64` (or set `MSYS2_ROOT`). In its MINGW32 shell:
  `pacman -S make mingw-w64-i686-gcc mingw-w64-i686-opus nasm`
- **Python 3.10+** on PATH: `pip install pefile pycryptodome pillow`
- **A 1.1.1.13 install that includes `C1.exe`**. It's read only: the build takes the zone key and stock zones from it.

The match server's prebuilt crypto libraries (mbedtls, libtomcrypt) are included in `third_party\server_libs`.

Then run:

```bat
git clone https://github.com/nucleur16/cod-offline-1.1.1.13
cd cod-offline-1.1.1.13
build.bat "D:\path\to\Call of Duty Online 1.1.1.13"
```

`build.bat` builds the match server, `Kernel33.dll`, the patch zones and the patched exe, and puts them all in
`dist\`:

```
dist\
  Kernel33.dll
  codoMP_client_shipRetail.exe        loads Kernel33.dll
  client_shipRetail\patch\*.ff        patch zones (menus, HUD, map lists, locale, ...)
  client_shipRetail\chinese\*.ff      zones that replace or add to stock ones
  main\sh1x_rawfiles\                 the match server's scripts
  main\sh1x_svr_scripts.iwd           bot classes and waypoints
  main\iw_image_16.iwd
  p2p\survival_maps.txt               Survival maps the server can host
```

## Install (drag and drop)

1. Open `dist\` and select everything in it (Ctrl+A).
2. Drag it onto your 1.1.1.13 game folder, the one containing `codoMP_client_shipRetail.exe`.
3. When Windows asks, choose **Replace the files in the destination**.
4. Start `codoMP_client_shipRetail.exe`.

A finished install only needs:

```
codoMP_client_shipRetail.exe  Kernel33.dll  mss32.dll  binkw32.dll  bink2w32.dll  D3DX9_43.dll
TenProxy.dll  TerSafe.dll  GbSpy.dll  steam_api.dll  localization.txt  splash.bmp  logo.bmp
miles\  client_shipRetail\{chinese,patch,temp}\  main\
```

The game creates `players\` (your profile and stats) and `p2p\offline\` on its own. You can delete `C1.exe`, `C1.dll`,
`TenSLX*`, `BugTrace*`, the updater DLLs, `p2p\sh1x_host.dll` and old `.ini` files.

## Play

- **Menus:** create a room (Multiplayer, Zombies, Survival or Spec Ops) and press Ready.
- **Console:** `devmap <map> [gametype]` loads a map with cheats on, and `map <map> [gametype]` loads it without them.
  The gametype defaults to `war` for `mp_`, `zm` for `zm_` and `coop` for `coop_` maps.
- **Map lists:** they show only maps whose `.ff` is in `client_shipRetail\chinese\` or `usermaps\<map>\`. Set
  `K33_MAPLIST_ALL=1` to list every map.
- **Profile:** `+set dw_username <name>` picks a profile (default `Player`).
- **Logs:** `Kernel33.log` next to the exe, and `p2p\offline\lobby.log`.

## Development

- `sh scripts/build.sh` builds only the DLL (it copies it to `..\Kernel33.dll` unless `K33_NO_COPY=1`).
- `sh scripts/build-dev.sh` builds the diagnostic build, with a developer console and test hooks.
- `python scripts/build_zones.py --help` lists the zone build's options.
- The server's GSC lives in `scripts/gsc` (a subtree of CODOL-EMU/13-rawfiles); edit it there.
- For map making, see [sh1xo-radiant, branch codol-1.1.1.13](https://github.com/nucleur16/sh1xo-radiant/tree/codol-1.1.1.13).
