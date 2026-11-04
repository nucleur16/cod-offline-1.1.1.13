# Production and local test builds

The default build has `CODOL_LOCAL_TESTING=0` and `CODOL_DIAGNOSTICS=0`.
It omits the Kernel33 log writer, parity logger installation, diagnostic pad and
voice commands, synthetic Ready/mouse helpers and command-file transport.
Normal controller input, voice, room identity, sound voice policy and gameplay
features remain enabled. Fatal startup failures still report an error.

`K33_LOCAL_TESTING=1 K33_DIAGNOSTICS=0` builds quiet local QA: the approved
command-file transport is available only in this separately compiled profile.
`K33_LOCAL_TESTING=1 K33_DIAGNOSTICS=1` enables development diagnostics.
Shipping with diagnostics, or values other than zero/one, is rejected.

The room-context migration remains opt-in (`K33_KERNEL_ROOM_CONTEXT=0` by
default). A migration build requires `K33_NO_COPY=1` and cannot use the build
script's installation copy. Its runtime binder rejects a non-system Direct3D
proxy so the legacy and Kernel33 publishers cannot run together. Names and
complete proxy parity, actual Windows exception dispatch and client lifecycle
acceptance remain release gates. Do not turn this switch on in a public release
merely because the standalone policy/binder fixtures pass.

The paired launcher proxy defaults to `PROFILE=shipping` and excludes the
developer ImGui console, developer panel and their input hook. Its normal replay,
name-display and native room integration remain. Development must be selected
explicitly and must use a separate output directory.

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
