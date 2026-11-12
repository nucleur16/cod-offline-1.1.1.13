# Porting rules for offline/ (the C++ Demonware lobby)

Working notes for whoever ports a module of the Python lobby (CODOL-EMU 1.1.1.13 Demonware emulator)
into `offline/`. The design itself is in `docs/OFFLINE_LOBBY.md`.

## What this is

`offline/*.cpp` is a faithful C++ port of the Python lobby, compiled into Kernel33.dll and into
`tests/offline/dwtest.exe` (a loopback test server). "Faithful" means: the bytes the client receives
are the bytes the Python lobby would send for the same request sequence. Log text (`Reply::note`)
is free-form and never compared.

## Layout

| file | role |
|---|---|
| `dw.h` | public core: `Bytes`, `Val`/`Vals` (parsed tagged values), `w_*` writers, `Reply`, `Config`, `Host`, `Conn`, `Server`, file helpers |
| `dw_json.h` | ordered JSON value (`Json`), lenient readers, `dump`/`Parse` |
| `dw_store.h` | the persistent store interface (replaces SQLite `store/lobby_db.py`) |
| `dw_state.h` | `ServerState` and EVERY cross-module function declaration |
| `dw_stubs.cpp` | placeholder definitions per module, under `#ifndef DW_HAVE_<MODULE>` |
| `dw_core.cpp` | framing, auth, dispatch (`BuildTaskReply`), STUN, services 12 / 6 / 71 / 102 |
| `dw_<module>.cpp` | one module each |

A handler has the signature `Reply Fn(Server &s, Conn &c, int task, const Vals &args)` and returns
`Reply()` (err == DECLINED) for a task it does not know, exactly where the Python handler returns
`None, [], ""`. `Reply::results` holds one serialized record per element (Python's list of `bytes`).

## Rules

* C++11, `-fno-exceptions -fno-rtti -fno-threadsafe-statics`. No `throw`, no `try`, no `dynamic_cast`,
  no `std::stoi` / `.at()` / anything that reports errors by throwing. **`std::string::substr(pos)` with
  `pos > size()` aborts the process** under `-fno-exceptions`: bounds-check first. Same for
  `std::vector::at`, `std::map::at`.
* No dependency on C1, Kernel33 or `<windows.h>` outside `dw_util.cpp` / `dw_net.cpp`. Use the helpers
  in `dw.h` (`ReadFile`, `WriteFileAtomic`, `ListDir`, `Inflate`, `Deflate`, `Crc32`, `Fmt`, ...). Need a
  new OS facility? Add it to `dw_util.cpp` and declare it in `dw.h`.
* Single-threaded: the whole server runs on one thread. No locks, no threads, no sleeping. Where
  Python defers work to a writer thread or a timer, do it inline or in the module's tick.
* Environment variables: the Python lobby has hundreds of `CODOL_*` switches. Port the DEFAULT
  behaviour (what `1-START-LOBBY.cmd` + `OFFLINE-DEV.cmd` give: `CODOL_OFFLINE=1`,
  `CODOL_MARKETPLACE_BALANCE=0`, `CODOL_MAP_POLICY=room`, `CODOL_MAP_PREFER=mp_killhouse_sh`,
  `CODOL_MAP_TYPES=mp,coop,zm`, `CODOL_JOIN_FIX=0`), not the switches. A/B kill switches are dropped.
  A genuine tunable becomes a `Config` field.
* Resources (`resources/*.json`, seeds, `.info` files): read them with `Server::Resource(name, out)`,
  which checks the override folder and then the copy embedded in the binary. Never open
  `resources/` paths directly. Parse a JSON table once and cache it in your module state.
* Player-specific data goes through `Store` (`dw_store.h`), never loose files under resources.
* Time: `Server::Now()` (Unix seconds; pinned by `Config::fixedTime` in tests). Never `time()`.
* `Config::differential` is true only in the differential tests. Use it ONLY where the offline
  product must deliberately differ from the Python lobby (e.g. the match handoff) - say why in a comment.
* Cross-module calls: declare in `dw_state.h`, add a placeholder to `dw_stubs.cpp` under the owning
  module's `DW_HAVE_<MODULE>` guard (module name = file name without `dw_` / `.cpp`, upper-cased).
  Other people edit those two files at the same time: make small targeted edits, re-read first.
* Comments: keep the provenance that matters (C1.exe addresses, "MEASURED" facts, why a field has
  the tag it has) in a line or two; do not copy the Python lobby's history essays.
* Only create/modify files of your own module, plus the minimal shared edits above. Do not commit.

## Building and testing

    export DWDIFF_ROOT=/d/CoD/k33work/offl PYLOBBY_SRC=/d/CoD/k33work/dw-emu
    export PYLOBBY_ZONE_DIR='D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13\client_shipRetail\chinese'
    OFFLINE_SKIP="dw_x.cpp dw_y.cpp" sh tests/offline/difftest.sh <slot> /d/CoD/k33work/offl/captures/last.json
    ... /d/CoD/k33work/offl/captures/big.json          # 64 connections, every request kind

`difftest.sh <slot> <session>` builds `dwtest.exe` for that slot (own object folder), starts a fresh
copy of the Python lobby and a fresh dwtest on the slot's private loopback ports, replays the
captured client bytes against both and prints every request whose decrypted reply or pushes differ,
with the first differing plaintext byte. Use YOUR slot only. `-v` prints everything, `--stop` stops
at the first difference; `DWDIFF_KEEP=1` leaves the servers up (`difftest.sh <slot> stop` ends them).
The Python lobby's own log is `$DWDIFF_ROOT/slot<slot>/pystate/dw-capture.log`, dwtest's is
`$DWDIFF_ROOT/slot<slot>/dwtest.log`. `OFFLINE_SKIP` leaves other people's unfinished modules out of
your build. The capture files hold real player ids and names: never copy them, or anything read
from them (names, ids), into the repository or into a report.

Hard limits: never bind or connect to ports 3074, 17733, 17760; loopback only; do not start the game
client; `D:\CoD\k33work\dw-emu`, the real game install and the real lobby folder are read-only; stop
only processes you started; no pushes. Bash heredocs with mixed quoting fail on this machine - write
helper scripts as files (in your scratchpad) and run them. g++ is `/c/msys64/mingw32/bin/g++.exe`
(32-bit); the build scripts put it on PATH.
