---
slug: build-and-run
title: Building and running
space: start
summary: The cross-build command, what the launcher sets and why, where the log goes, and how the tests run.
position: 30
tags: [operations]
---

The server is a **32-bit** binary. That is not a preference: it links against
retained x86 assembly taken from the original game, so the whole program has to
be `-m32`.

## Building

The makefile drives three compilers — a C compiler, a C++ compiler and NASM —
and selects the CoDOL target by default:

```make
GAME ?= codol           # or cod4x
TARGETNAME=codol_dedrun
COMMON_FLAGS=-m32 -msse2 -mfpmath=sse -Wall -fno-omit-frame-pointer ... $(CODOL_INC)
```

`CODOL_SERVER` is defined for both the C sources (`COD4X_DEFINES+=CODOL_SERVER`)
and the assembler (`NASMFLAGS+=-dCODOL_SERVER`), which is what switches the
retained listings onto their CoDOL behaviour.

### Cross-building for Windows from WSL

```sh
make -j8 -k OS=Windows_NT \
     CC=i686-w64-mingw32-gcc CPP=i686-w64-mingw32-g++ AR=i686-w64-mingw32-ar \
     OBJ_DIR=obj/codol-win32 bin/codol_dedrun.exe
```

Stop a running `codol_dedrun.exe` first — otherwise the link fails because it
cannot open the output file. A missing post-link `pexports` is a known, harmless
failure in this configuration.

## Running

`bin/START-CODOL-SERVER.cmd` is the launcher. It exists because the native
server has to slot into the place the alpha0022 Python stack already occupies:
the lobby (`dw-capture.py` on udp/3074) hands the client a host and port, and
the native server must answer on exactly that port with exactly that map.

What it does, in order:

1. **Resolves the install and reference paths.** `CODOL_INSTALL` is the CoDOL
   1.1.1.13 directory containing `C1.exe`; `CODOL_REFERENCE` is the alpha0022
   tree with the vendor scripts, the gsc overlay and the session key.
2. **Takes the match from the lobby's handoff.** The lobby stamps the map,
   playlist gametype and zone category it sent to C1 into `startgame.txt`, and
   the launcher consumes that one decision. This is what stops the client
   loading one map while the server publishes another in its connect response,
   serverinfo and gamestate. Setting `CODOL_MAP` yourself pins the match
   instead and deliberately ignores the handoff.
3. **Generates a local-only RCON credential** into `native_server_rcon.txt` if
   it is not there, so the lobby's match-start dispatcher can drive the server
   without a secret being checked into the repository.
4. **Launches** with the settings below.

```
+set dedicated 2                 dedicated server, no local client
+set net_port 17733              the port the lobby hands to C1
+set sv_fps 20                   20 server frames a second
+set developer 1                 diagnostic traces and rawfile dumps
+set logfile 2                   flushed console log
+set fs_basepath <install>       where the game data lives
+set fs_homepath bin/home        where the log and written files go
+set codol_scriptPath .../vendor/iw4
+set codol_scriptOverlay .../gsc_overlay
+set codol_scriptNativeOverlay scripts/codol
+set codol_securityKeyPath .../securitykey.bin
+set codol_profilePath .../resources
+set codol_zoneType mp
+set g_gametype <gametype>
+map <map>
```

The three script paths are a precedence chain: the native overlay in
`scripts/codol` wins over the alpha0022 `gsc_overlay`, which wins over the IW4
vendor scripts. See [The GSC script surface](/wiki/subsystem-script).

`codol_securityKeyPath` points at the 16-byte session key the lobby writes at
ready-up. Without it the client's encrypted handshake is never accepted and the
connection simply times out — see [Transport](/wiki/subsystem-net).

The console log lands at `bin/home/main/qconsole.log`.

## Tests

```sh
make test-codol
```

The C tests name their sources explicitly rather than linking the whole server,
so each one compiles a handful of files and runs in-process:

```make
$(CC) -m32 -O1 -g $(CODOL_INC) tests/test_codol_matchdata_wire.c \
      src/codol/stats/codol_matchdata_wire.c src/codol/net/codol_msg.c \
      -o $(OBJ_DIR)/test_codol_matchdata_wire
```

Several targets assemble a retained listing directly — for example
`nasm -f elf -DCODOL_SERVER src/asmsource/_player_use_mp.asm` — and link it
against a small C harness, which is how the assembly half gets test coverage at
all. The Python tests build their include list from `src/codol/*` at run time.

## Environment overrides

Every launcher setting can be overridden from the environment before calling it:

```bat
set CODOL_MAP=mp_bog_sh & set CODOL_GAMETYPE=war & START-CODOL-SERVER.cmd
set CODOL_DEVELOPER=0 & START-CODOL-SERVER.cmd
set CODOL_EXTRA_ARGS=+set codol_rovAssertUnits 0 & START-CODOL-SERVER.cmd
```
