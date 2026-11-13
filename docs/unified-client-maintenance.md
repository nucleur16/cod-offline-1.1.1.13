# Shared client maintenance candidate

The locale selector loads the selected language's matching UI archives. Managed
Quick room updates and the Infected scoreboard use the same native hooks in both
languages. A language choice does not choose an older client implementation.

The ordinary build leaves C1's original single-instance branch and mutex intact.
`K33_LOCAL_TESTING=1` compiles the separate QA profile. That profile still requires
`+set codol_local_test_multi 1` and an exact loopback `dwauth.ini` before replacing
the exact game mutex with a process-local name. Production does not contain that
mutex override.

## Existing launcher upgrade

`K33_LAUNCHER_HOST_SHA256` optionally pins a 64-character lowercase SHA-256 of
the staged `TCLS/LauncherHost.pending.exe`. An unset pin makes the bridge inert.
The bridge runs before game initialization, verifies the staged hash and the
actual parent launcher's path and process creation time, and starts that staged
launcher's after-exit installer. It neither downloads files nor requests
elevation or firewall changes. The old launcher closes normally; the installer
replaces it, keeps a rollback copy, and reopens it. The new launcher owns future
updates. Its installed-hash receipt prevents this one-time bridge from applying
an older staged launcher over a later successful update.

Build without installing with `K33_NO_COPY=1 sh scripts/build.sh`. The controller and
parity entry points use the same checked build profile and dependency closure.

## Validation and remaining release gates

Both production and local profiles compile. The x86 Windows Quick/policy fixture
has 293 passing checks. The updater has nine Windows mechanism tests covering
replacement after exit, hash and process identity failures, preservation,
reopening, the 32-bit bridge, and subsequent self-update. A preserved released R4
launcher downloaded the new full host through its normal update flow; the built
installer replaced and reopened that host after a normal close.

These results do not complete the full game-initiated launcher bridge acceptance
or the lobby acceptance matrix. Release remains coupled to the tested launcher,
matching English/Chinese UI archives, lobby/worker source and native server
profiles. Do not publish the private diagnostic UI or treat this source branch
as deployment approval.
