# Native source profiles, 2026-10-23

The primary `src/` and `makefile` are the normal MP/co-op profile. Zombies uses
the exact source delta in `native_profiles/zombies.patch`. Never combine both
sets of definitions in one build or route co-op through the Zombies binary.

## Current upstream integration

The current source integrates Nucleur main
`8458d68306f3d2c3b768f6d50836b55625c2a6fd` into feature parent
`ab21a3699496eb2af6df622c85cada6174b4460d`. Both profiles retain their
previous identity/admission, QuickPlay, theatre, session and gameplay closure.
Both also receive upstream's explicit script error codes, safe console-redirect
cleanup, RCON access while no level runs, and remembered-level recovery with
operator-stop handling. The upstream soak utility's valid-profile and timed
client-command options are retained.

The exact source manifests and Zombies delta have been regenerated. The profile
manifest distinguishes previous tested binary hashes from these changed inputs.
Both updated native binaries built successfully on the pinned GCC 13 builder,
and both focused fixture sets pass, including their expected-failure donor
controls. Parent source review and exact-binary EN/ZH C1 acceptance remain
required before release. No runtime service or package is changed by this source
integration.

| Category | Current built native SHA-256 | Real C1 acceptance |
| --- | --- | --- |
| MP / co-op | `92eb66e3730ced32cf921e2856df44b04a4caabd0e402020a24257819c0e0fda` | Pending |
| Zombies | `4a226c41e6b734a2dce24210bcb51749f152adc41499a66db01c7f68d6bc4d50` | Narrow local start/results/return pass; full qualification pending |

The current qualification is [native-parity-safety-20261023/qualification.json](releases/native-parity-safety-20261023/qualification.json). It binds both exact compiler-input archives, current code manifests, the corrected Zombies unsigned loader and shared animation-reference safety helper. A real 7,265,557-byte unsigned archive fails the old loader and passes the corrected native parser (1,341 assets). Both animation-query implementations pass 24 native cases; two donor controls reproduce the old null-tree crash. The compact portable fixture sources, explicit doubles and proof limits are in that folder. Their newly packaged wrapper has not been executed; the receipts bind the original executed runners.

The external reviewed Zombies caller package `76d3f4dde7a4a6b4af54752f03cc072fa4331772e864fe406a1b85ade90d4a60` removes the stale unused query to the unavailable `walk_and_run_loops` node. This is a separate script-package requirement, not a claim that it equals the preserved rawfiles gitlink. Native safety alone does not prove Zombies gameplay. The parent observed one local four-human EN/ZH start, Round1 HUD, natural defeat/results and persistent room289 return on this exact native/script pair; [the narrow C1 receipt](releases/native-parity-safety-20261023/zombies-narrow-C1.json) separates those observations from exact-pin evidence. Normal C1, repeat-match stability and production qualification remain pending.

The upstream recovery state machine also passes 89 checks in a normal build and
89 checks under ASan/UBSan. It executes the real implementation with explicit
dvar, clock, packet-wait, logging and command-buffer doubles; it does not execute
real RCON or a level loader. Its retained fixture and runner are in
`docs/releases/native-upstream-8458-20261023/fixtures/`. Mount the materialized
source read-only at `/src`, that folder read-only at `/recipe`, and a fresh
writable output at `/out` in the pinned network-none builder, then run
`bash /recipe/run-recovery.sh`. Historical 4d4/e43 build, source-preservation and fixture
receipts remain in that folder; current 92eb/4a22 qualification is linked above.

The rawfiles gitlink is deliberately preserved at
`e37a77d2ed0aa99d4310da5a7cc7c7f8e74bdc6f` until the parent integrates the
incoming rawfiles `3f91e86c49b3a892cf7657351ee72cc7490518b2` into its reviewed
script branch. That commit adds never-spawned podium-player guards and is absent
from raw PR1 `c67753856df1011fb7caada14101bacfdcfacc60`. Selecting upstream's
rawfiles gitlink directly would discard the private script branch. Script
integration and native qualification remain separate, coupled release gates.

## Previous built baseline

| Category | Native profile | Tested native SHA-256 |
| --- | --- | --- |
| MP / co-op | normal | `89bdb64b7a8ad7a8e4a88cae4893f5ecb978c6a5c60143e39a24eaa0ebc792e6` |
| Zombies | zombies | `a7a07b9b9f3e59ae9f6e581d4f97887a1dca9ecc81c2ae48d5b577d117fd54f3` |

This previous built integration was based on private published commit
`885b7d699a9c69ad7f24d64c8be7773ec0c013ab`, merging Nucleur main
`b316bd45c81ebaa8600273e5090e85207573b7fc`. The actual common ancestor is
`b9fba4c87a6e00c01d92846f8871bd95d151a0c1`. Both histories remain reachable.
The private rawfiles gitlink remains `e37a77d2ed0aa99d4310da5a7cc7c7f8e74bdc6f`.
This change does not replace the previously published 29c/0f13 script packages.

Normal preserves the accepted f5eb signed pending-connect identity fix and the
private report/admission/QuickPlay/theatre closure. Zombies separately preserves
the accepted 900 Endless/session, actor dispatch, published FX compound and hand
selector closure, with audited stats-before-drop/reset and UID-owned rejoin
counters. The 149/150 Zombies FX events are not aliases for upstream 148/149.
Both profiles include native 32-slot `useWeaponHideTags` and eight-word masks.
The source-only rawPR1 merge needs that method; it cannot be published as
GSC-only over old a654/f5eb native. Its script/client release composition is a
separate review and acceptance task.

## Reconstruct the exact inputs

Use a new output directory outside this repository. This command only writes
source files; it neither builds nor runs a server:

```text
python -B native_profiles/materialize.py --category mp --profile normal --output <new-normal-directory> --library-dir <pinned-library-directory>
python -B native_profiles/materialize.py --category zm --profile zombies --output <new-zombies-directory> --library-dir <pinned-library-directory>
```

`native_profiles/manifest.json` pins the code manifests, source delta and four
existing native static libraries. The libraries and compiled binaries are not
stored in Git. The frozen compiled-input archives retain those exact libraries.
Absent or mismatched library inputs cannot qualify a new build. The materializer
rejects an existing output, source drift and a wrong category/profile pair.
Per-command Git settings preserve raw source bytes; scoped `.gitattributes`
prevents Windows line-ending conversion from changing inherited compiler inputs.
The materializer also bounds Git discovery to the new output directory and
clears inherited repository/worktree/index overrides for its filesystem patch
operation. Outputs below an unrelated parent checkout therefore receive the
Zombies delta instead of Git silently filtering its paths. Inventory and file
hash checks still run before any library copy or qualification receipt.

`python native_profiles/test_materialize.py` checks nested/plain workspaces,
inherited Git overrides, source preservation, routing, reuse and library rejection
without compiling or launching the game server.

The current resident candidate uses the retained compiler image and immutable
archive pinned by `builder_image` and `builder_image_archive` in the current
profile manifest. Its Ubuntu24/GCC13.3, NASM2.16.01 and Make4.3 versions were
qualified on the private self-hosted runner. Verify the archive hash before
loading it on an isolated build host; source and libraries remain read-only
inputs. Earlier builder/image and binary receipts are historical evidence.

Use each new materialization's `SOURCE-RECEIPT.json` `required_make_variables`
and `required_build_environment` exactly. Profile-specific BUILD_REVISION,
object/output paths and SOURCE_DATE_EPOCH are part of the tested build identity;
do not substitute old values from historical receipts. The current candidate
has two byte-identical independent builds and separately qualified headless
room/recovery tests. See `docs/releases/resident-server-20261029/` for scope.

For resident slot configuration use `resident_contract_profile_sha256`, checked
against the actual compiled profile header. The current code/source-manifest
hash is separate and must not replace that stable contract identity. The exact
native binary, profile source and trusted catalogue identities remain pinned.
No profile metadata authorizes a service launch, selector update or publication.

## Repeat the focused fixtures

```text
python -B native_profiles/prepare_focused_tests.py --source <materialized-source> --output <new-fixture-directory>
```

In the pinned builder, mount the materialized source read-only at `/src`, the
prepared fixture read-only at `/recipe`, and a fresh output at `/out`. Set
`NATIVE_PROFILE` to the selected profile and run `/recipe/run-focused.sh` in a
network-none container. Use a Linux temporary filesystem for `/tmp`; Windows
DrvFS does not implement the session fixture's POSIX mode/umask assertions.
All prepared sources match the previously executed GCC 13 fixture inputs;
the old report donor is equivalent after CRLF-to-LF normalization only.

Recorded normal/sanitizer cases per profile:

- Normal: identity 55, party 83, Quick 40, hide-mask 44, startup 54, endpoint/planner 24.
- Zombies: identity 55, hide-mask 44, endpoint/topology/multi-owner-cuts 42;
  session 789 normal and 309 sanitizer. Both donor controls fail exactly 8 checks.

Engine/VM/asset/I/O/collision boundaries are disclosed test doubles. The session
fixture executes the actual report, scoreboard, GP and async source, with the
exact extracted drop/reset ordering; its reset adapter calls the actual score
stat reset, not the full input subsystem. These tests do not prove a complete
network handshake, game VM/map lifecycle or C1 rendering/gameplay.

Build/function-test receipts and source lineage are under
`docs/releases/native-profiles-20261023/`. Those historical source materializations were verified
against their frozen 1715/1727-file compiler archives. Current 92eb/4a22 inputs
are separately verified across all 1717/1729 members; see the current qualification.

## Remaining release gates

Parent review and real EN/ZH C1 acceptance on these exact native hashes remain
required, including natural end/return/repeat match and Zombies rendered masks,
downed ADS, barrels/teleporter, rejoin counters and dependent client/GSC pairing.
The extra Zombies/co-op resolver intentionally reports missing newer soldier
APIs; the routing gate rejects that unsupported combination. No maintenance,
deployment or live behavior acceptance is implied by this source branch.

`native_profiles/zombies-review-only.patch` is the human review view with CRLF
normalized to LF. It is never applied. The raw `zombies.patch` preserves the
exact tested bytes, including explicit line-ending differences. The merged
`tests/test_native.c` preserves private replay/slow-motion checks and
newer upstream inventory/ammo checks; its actual native fixture passes on
the pinned GCC 13 builder (see `merged-native-fixture.json`).
