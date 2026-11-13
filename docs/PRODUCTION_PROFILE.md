# Production and local test builds

The default build has `CODOL_LOCAL_TESTING=0` and `CODOL_DIAGNOSTICS=0`.
It omits the Kernel33 log writer, parity logger installation, diagnostic pad and
voice commands, synthetic Ready/mouse helpers and command-file transport.
Normal controller input, voice, room identity, sound voice policy and gameplay
features remain enabled. Fatal startup failures still report an error.

`K33_LOCAL_TESTING=1 K33_DIAGNOSTICS=0` builds quiet local QA: the approved
command-file transport is available only in this separately compiled profile.
`K33_LOCAL_TESTING=1 K33_DIAGNOSTICS=1` enables development diagnostics.
That profile alone compiles `src/console/k33shot.c` (with the developer console it rides on): the `k33_screenshot` and
`k33_framestats` commands and, on a test client only (`+set k33_shotFile 1` or `+set k33_cmdFile 1`), the
`k33_shot.req` / `k33_stats.req` request files. Shipping and quiet-QA DLLs contain none of it: no command, no
request-file polling, no Direct3D hook.
Shipping with diagnostics, or values other than zero/one, is rejected.

The room-context migration remains opt-in (`K33_KERNEL_ROOM_CONTEXT=0` by
default). A migration build requires `K33_NO_COPY=1` and cannot use the build
script's installation copy. Its runtime binder rejects a non-system Direct3D
proxy so the legacy and Kernel33 publishers cannot run together. Names and
complete proxy parity, actual Windows exception dispatch and client lifecycle
acceptance remain release gates. Do not turn this switch on in a public release
merely because the standalone policy/binder fixtures pass.

The paired launcher proxy (a separate project, not in this repository) defaults
to `PROFILE=shipping` and excludes its own developer console overlay, developer
panel and their input hook. Kernel33's in-game console is `src/console/console.c`: it draws
itself with GDI and Direct3D 9 and uses no GUI library. Its normal replay,
name-display and native room integration remain. Development must be selected
explicitly and must use a separate output directory.

The full proxy-free prototype additionally selects `K33_KERNEL_NAME_DISPLAY=1`
with `K33_KERNEL_ROOM_CONTEXT=1 K33_NO_COPY=1`. It explicitly loads Windows D3D
and moves the retained name hooks into Kernel33; see
`docs/KERNEL_NAME_MIGRATION.md`. Both flags remain disabled for shipping until
the exact combined candidate passes real-client qualification.

The shared frontend hides Quick Match from the playlist menu for this release.
Its native catalogue and room/training paths retain their existing geometry.
The reviewed MatchController predicate and quiet profile pins are recorded in
`ui_baseline.json` after real-client room creation, Ready, connection, results
and repeat checks. Only four reviewed file pins and the MatchController compiled
source digest advanced. The overall contract remains a release candidate;
controllable gameplay and complete release qualification are separate gates.

`tests/test_production_profile_windows.py` compiles and executes the actual
profile-sensitive function bodies in owned hidden Windows fixtures. It does
not launch a game or provide gameplay acceptance.

`tests/test_qa_pointer_windows.py` executes the exact QA cursor/Escape bodies in
a never-visible owned Windows window. Left/right click and ordinary Escape are
available only in the separately built, launch-guarded QA profile; shipping and
QA launches without that guard deliver no input. The test checks the foreground
window remains unchanged. It does not certify the game's menu response.

## Offline mode

`K33_OFFLINE=1` (`CODOL_OFFLINE_LOBBY=1`) is a separate opt-in build flag: it compiles the in-client lobby (`src/offline/`, `src/offline_lobby.c`, `docs/OFFLINE_LOBBY.md`). It enables no diagnostics and changes nothing unless the player also switches offline mode on at run time (`+set k33_offline 1` or `k33_offline.ini`). Without the flag none of that code is built or linked.
