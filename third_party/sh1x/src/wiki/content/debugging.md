---
slug: debugging
title: Debugging and traces
space: operations
summary: Where the log is, how the CoDOL trace prefixes are organised, and the failure diagnostics the frame path exposes.
position: 20
tags: [operations]
---

## The log

`bin/home/main/qconsole.log`, written with `logfile 2` so it is flushed rather
than buffered — which matters, because the interesting case is usually a crash.

## Trace prefixes

Developer traces are prefixed `CoDOL <topic>:` so a pasted log can be filtered
by subsystem:

| Prefix | What it reports |
|---|---|
| `CoDOL pacing` | Snapshot fragment pacing. |
| `CoDOL rov` | Read-origin-velocity handling in the player-state writer. |
| `CoDOL mantle` | Mantle acquisition and frame ownership. |
| `CoDOL spawn` | Spawn selection and entity creation. |
| `CoDOL field audit` | Field-table mismatches. |
| `CoDOL reliable ->` | Reliable commands sent to a client. |

```sh
grep 'CoDOL pacing' bin/home/main/qconsole.log
```

Traces only appear with `developer 1`, which the launcher sets by default.

## Frame failure diagnostics

When a frame fails to build or serialise, three globals name the step that
rejected it rather than making you bisect:

```c
extern const char *codol_frame_failure;     /* last synchronous frame build  */
extern const char *codol_player_failure;    /* which player/HUD overlay step */
extern const char *codol_snapshot_failure;  /* why the write overflowed      */
```

`CODOL_WriteSnapshotFailure()` and `CODOL_HistoryFailure()` return the same kind
of string for the writer and the history.

These are diagnostics, **not wire state** — they are for the log, and nothing
should branch on them.

## Reading a bad snapshot

A malformed snapshot rarely errors. The client decodes it into something
plausible and behaves oddly, so the useful questions are ordered:

1. **Did the frame build?** Check `codol_frame_failure` /
   `codol_player_failure`. A failed rebuild invalidates the previous frame by
   design, so a persistent failure shows up as a client that stops updating.
2. **Was the base right?** The history commits only after transport acceptance,
   and stores quantized client-held values. A base recorded for a frame that was
   never sent desynchronises every later delta.
3. **Was the HUD capture sequence-matched?** Player fields and HUD come from one
   frame; mixing sequences produces an internally inconsistent snapshot.
4. **Is a field mapped at all?** `CoDOL field audit` reports fields with no
   verified mapping. An unmapped value is left out rather than guessed.

See [A server frame](/wiki/server-frame) and
[Snapshot publication](/wiki/subsystem-snapshot).

## Transport problems

`CODOL_AssociationOpenDetailed` returns a failure reason — replay, tag mismatch,
MAC failure — and deliberately contains no key material or plaintext, so it is
safe to paste into a log or an issue.

The usual first-order causes of "the client just times out":

- `securitykey.bin` missing or stale, so the handshake is never accepted.
- The map in `startgame.txt` disagreeing with what the server published.
- A retried socket send that reused a consumed counter instead of re-encrypting.

## Packing bugs

A crash inside a C++ standard-library container, or `CODOL_ScriptLowerSelfTest()`
returning anything other than `3`, means a translation unit built its container
templates under the engine headers' `#pragma pack(1)` and the linker chose those
copies. Include the standard library **before** any project header. See
[the packing hazard](/wiki/source-layout).

## Tests

`make test-codol` runs the unit tests. Each target names its sources explicitly,
so when one fails you already know the handful of files involved. Several
targets assemble a retained listing and link it against a C harness, which is
how assembly behaviour gets checked at all.
