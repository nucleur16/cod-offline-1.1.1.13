# CoDOL server source layout

Restructured 2026-09-09. Every CoDOL-specific source file now lives under
`src/online/<area>/`; the retained CoD4x engine stays flat in `src/` and its
disassembled functions in `src/asmsource/`. Every area directory is on the include path, so
`#include "x.h"` works from anywhere. The legacy `codol_` / `CODOL_` prefix was dropped from
file names and identifiers on 2026-11-16 (`tools/rename_prefix.py`, table
`tools/rename_prefix_map.json`); a name that would have clashed with a CoD4x or system name
took `ol_` / `OL_` instead (`ol_msg.c`, `ol_msg_t`, `OL_SERVER`).

## Where things are

| path | what lives there |
|---|---|
| `src/*.c`, `src/*.cpp`, `src/*.h` | retained CoD4x engine (server loop, netchan, filesystem, script VM, game logic). CoDOL hooks into it are guarded by `#ifdef OL_SERVER`. |
| `src/asmsource/*.asm` | retained IW3 functions kept as assembly (pmove, mantle, animation, weapons, game entities). calls into `src/online/` are inserted as plain `call`s. |
| `src/online/net/` | client transport: DTLS/handshake, ECDH, association, connect, netchan framing, wire codec, pure check. |
| `src/online/snapshot/` | everything published to the client each frame: native frame builder, snapshot writer and history, delta codec, player/entity/client/actor field tables (`*_fields.inc`), engine-state overlays (`engine_state.c`), events and the event map, config strings, HUD and radar wire state. |
| `src/online/player/` | input and movement: usercmd decode, dive, sprint and recoil state, perks and abilities, stance/animation tables (`animation_*.inc` are also included by `_bg_animation_mp.asm`), spectate defaults, use hints, loop sounds, team fx. |
| `src/online/weapon/` | weapon composition from attachments, weapon registry and loader, weapon state machine, weapon locks and missiles, turret heat, server weapon config strings. |
| `src/online/vehicle/` | vehicle assets, native vehicle simulation, spawner. |
| `src/online/script/` | GSC builtins added for CoDOL scripts (`script_<topic>.c`), the syntax-lowering pass (`script_lower.cpp`), precache, profile/loadout access, menus, startup shims. |
| `src/online/assets/` | fastfile and IWD reading, asset registry and streaming, xmodel/xsurface/xanim/material/sound/effect natives, character composition, string tables, shock rawfiles, map custom data. |
| `src/online/collision/` | BSP collision (brushes, leaves, terrain, static models, materials), world planes, physics, dynamic entities, triggers, map entity string. |
| `src/online/stats/` | secure profile (read only), experience/challenge/unlock rules, skill rating, match data and its wire format, scoreboard, structured data schema. |
| `bin/home/codol_rawfiles/` | THE GSC tree the server loads, in the IW4/T5 layout: `maps/mp/` multiplayer, `maps/_coop*` + `maps/<map>.gsc` co-op, `maps/_zombiemode*` + `maps/zm_*.gsc` zombies, top-level `animscripts/`, `aitype/`, `character/`, `common_scripts/`, `codescripts/` (docs/GSC_TREE_LAYOUT.md). Per-category entry points: `ScriptEntryPoints` in `src/online/script/script_source.c`. |
| `scripts/codol/maps/mp/...` | historical; retired 2026-09-16, never edited or loaded (AGENTS.md "Script tree policy"). |
| `bin/START-SERVER.cmd` | launcher (developer 1, port 17733, script paths); log at `bin/home/main/qconsole.log`. |
| `tests/test_*.c|.cpp|.py` | unit tests, run by `make test` under WSL. |
| `tools/` | offline helpers (`sh1x_archive.c`, `event_map.py`). |
| `docs/*.md` | design notes per subsystem; `IMPLEMENTATION_PASS.md` is the running log, `PLAYER_ACTION_AUDIT.md` the per-action status, `PARITY_PLAN.md` the gap list. |

## Build rules

- The makefile lists the areas in `OL_AREAS`; `C_SOURCES`/`CPP_SOURCES`
  glob each area, objects stay flat in `$(OBJ_DIR)`, and `OL_INC` adds
  `-Isrc` plus every area directory to `COMMON_FLAGS`.
- Adding a file: drop it into the area it belongs to. Nothing else is needed for
  the server build. File names must stay unique across areas (objects are flat).
- Adding an area: create the directory and append its name to `OL_AREAS`.
- Unit tests in `test` name their sources explicitly
  (`src/online/<area>/x.c`); the Python tests build the include list from
  `src/online/*` at run time (`OL_INCLUDE`).
- Cross-build from WSL:
  `make -j8 -k OS=Windows_NT CC=i686-w64-mingw32-gcc CPP=i686-w64-mingw32-g++ AR=i686-w64-mingw32-ar OBJ_DIR=obj/sh1x-win32 bin/sh1x_dedrun.exe`
  (stop `sh1x_dedrun.exe` first or the link cannot open the output file).

## Naming inside an area

- `<topic>.c` holds the engine-facing logic; `<topic>_native.c`
  parses the raw asset or BSP format; `<topic>_wire.c` is the client
  encoding; `sv_<topic>.c` is the server-frame glue; `script_<topic>.c`
  is the GSC builtin surface.
- Developer traces are prefixed `CoDOL <topic>:` in the console log so a paste
  can be grepped by subsystem (`CoDOL pacing`, `CoDOL rov`, `CoDOL mantle`,
  `CoDOL spawn`, `CoDOL field audit`, `CoDOL reliable ->`).
