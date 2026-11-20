# Native reliability qualification, 2026-10-25

Both profiles from source commit `f64b93630466c0d0228396377832c184e66553b5`
build successfully in the originally pinned GCC13 builder with all four originally
pinned static libraries. The focused functions below pass. Native release,
Windows qualification and current-build C1 gameplay acceptance remain held.
No game or client executable was launched for these checks.

| Profile | Code inputs | Linked Linux native SHA-256 |
| --- | ---: | --- |
| Normal | 1715 | `1af13fe608611bb7d100c84a554d02a4c4538243ea957753bcf2a6184c485297` |
| Zombies | 1727 | `f6ea071a7b16b3d5a6dc3d3b02c9ff121e3f1a6342758181e4a9b6daab6dc897` |

The four library files are additional compiler inputs. Their hashes, build
variables, source-date epoch and builder image remain those in
`native_profiles/manifest.json`. The builder is
`sha256:bb151123b61a808c03cd82d011aedeba1a54c7c6b55c521a5540daef0c76b6d7`.
It reports GCC `13.3.0-6ubuntu2~24.04.1`. Source hashes are checked before and
after compilation; neither input tree changed.

The earlier default-tree Linux link used different locally rebuilt libraries.
It remains separate evidence and is not substituted for these profile builds.

## Focused native functions

Normal: pending connection identity, native Quick party/admission helpers,
legacy Quick selection, Training helper and DirectConnect seat allocation,
startup archive selection, weapon hide masks, and route endpoint planning.
Each runs normally and with AddressSanitizer/UndefinedBehaviorSanitizer.
The retained fixture recipe excludes alignment instrumentation for packed engine
layouts; this does not qualify arbitrary pointer alignment or full engine safety.

Zombies: pending identity, hide masks and route endpoint planning run in both
variants. Session reporting, reconnect totals, drop/reset order, and asynchronous
capture handling pass in candidate and sanitizer fixtures. Each of the two
historical controls fails precisely the expected eight reconnect assertions;
neither fails through a crash or sanitizer error. Positive logs have no failed
assertions or sanitizer diagnostics.

Additional fixtures compile against each exact materialized profile: per-port
session relocation and home loss, future send-counter reservations, DATA fast
path, atomic file replacement, peer-slot selection, path-node ownership/lookup,
and the i386 packed entity-reference return boundary. All seven pass normally
and with sanitizers, for 28 positive executions across the two profiles.

The first Zombies path-node fixture used the normal profile's ten-field table
and stopped on its field-count assertion. The Zombies profile intentionally has
nine fields without normal's derived `node.type`. Preserve that failed run; a
separate follow-up adapts only the test's field count, absent type checks and
first invalid index. The Zombies source is unchanged. Only the unfinished
path-node and ABI fixtures are rerun. Do not report the original run as passing.

The combined runs contain 50 positive fixture executions and two expected-failure
controls. Fixtures use explicit engine, VM, network, clock, filesystem or asset
boundaries. The ABI fixture models the retained scalar/EAX entry; it does not
execute the whole VM. These checks do not establish full runtime admission,
client event consumption, audible effects, damage in the reported session, or
successful repeated C1 matches.

## Evidence and release boundary

Durable local incident records are under
`re/aws-server-efficiency-20261024/`: `profile-compile-20261025-r4`,
`profile-function-qualification.json`, and
`profile-regression-qualification.json`. They retain exact container identities,
frozen recipes, source/fixture hashes, toolchain output, terminal exit states,
logs and the initial profile-expectation failure. Compiler/test containers use
no network and read-only source mounts; no production game service is involved.

Do not populate complete-package acceptance fields from function tests alone.
The application native pin remains held. The worker's per-port cache option
requires this native implementation, so release those components together.
The user has authorized production deployment after required testing; the
continuous-empty gate and advance announced lobby maintenance still apply.
