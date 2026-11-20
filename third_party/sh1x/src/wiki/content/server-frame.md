---
slug: server-frame
title: A server frame
space: architecture
summary: What happens twenty times a second — from received commands, through the game frame, to one snapshot per client.
position: 10
tags: [snapshot, architecture]
---

The server runs at `sv_fps 20`: one frame every 50 ms. A frame does four things
in order, and the order matters because each stage reads state the previous one
produced.

```
  received packets        →  usercmds queued per client
  game frame (retained)   →  entities, playerState, events move
  frame capture (CoDOL)   →  semantic snapshot state per recipient
  snapshot write          →  one delta-encoded message per client
```

## 1. Input

Packets arrive on the transport described in [Transport](/wiki/subsystem-net),
are decrypted, defragmented, and yield reliable commands and user commands.

User commands are not executed as they arrive. They go into a per-client queue
(`codol_c1_cmd_queue_t` in `src/codol/net/codol_usercmd_queue.h`) that keeps
three distinguished commands — the last **accepted**, the last **executed**, and
the one **executed before that**. Movement prediction on both sides depends on
that triple, which is why the queue exposes it rather than just a ring:

```c
codol_c1_cmd_accept_t CODOL_C1CmdQueueAccept(codol_c1_cmd_queue_t *, const codol_usercmd_t *);
codol_c1_cmd_exec_t   CODOL_C1CmdQueueBeginExecute(const codol_c1_cmd_queue_t *,
                                                   uint32_t server_time, uint32_t max_lag);
int                   CODOL_C1CmdQueueCommitExecuted(codol_c1_cmd_queue_t *, uint32_t server_time);
```

`BeginExecute` decides whether a command is due yet; `CommitExecuted` is what
advances the triple, and only after the command has actually run.

## 2. The game frame

This is retained engine code: `SV_RunFrame` in `src/sv_main.c`, the pmove and
game logic under `src/asmsource/`, and the GSC threads the scripts started. It
moves entities, runs movement and weapons, fires events, and leaves the result
in the engine's own structures — `playerState_t`, `entityState_t`,
`clientState_t`, the HUD element array.

Those structures are IW3's. Nothing the client sees uses them directly.

## 3. Building the frame

The bridge from engine state to wire state is the **frame builder**
(`src/codol/snapshot/codol_frame_builder.h`). For each recipient it assembles a
`codol_native_frame_t` from:

- the player-state seed and its array updates,
- resolvers that convert engine entities and clients into CoDOL states,
- actors, animation commands, and the sequence-matched HUD capture.

```c
int CODOL_BuildClientNativeFrame(unsigned recipient, uint32_t sequence,
                                 const clientSnapshot_t *,
                                 const codol_native_frame_bindings_t *);
```

Two properties of this step are worth internalising because they explain a lot
of the code around it:

**It is a transaction.** The previously owned frame is replaced *only* after
every section succeeds. A failed rebuild invalidates the previous frame rather
than leaving it in place — including when a retry reuses the same outgoing
sequence. That is deliberate: a half-updated frame would serialise into a
snapshot the client would decode as coherent.

**The HUD has to be captured before native identity is lost.** `CoDOL` keeps one
current HUD capture per recipient (`CODOL_CaptureClientSnapshotHud`), not an ACK
history, and the player overlay composes the exact frame's player fields with
its sequence-matched HUD (`CODOL_OverlayClientSnapshotPlayer`). Mismatching
those two produces a snapshot that is internally inconsistent in a way that is
very hard to see from the client side.

When a build fails, three diagnostics say where:
`codol_frame_failure`, `codol_player_failure`, `codol_snapshot_failure`.

## 4. Writing the snapshot

Serialisation is delta-encoded against a base the client has acknowledged:

```c
int CODOL_WriteSnapshot(codol_msg_t *msg,
                        const codol_snapshot_state_t *to,
                        const codol_snapshot_state_t *base,
                        const codol_player_array_updates_t *updates,
                        const codol_animcmd_t *commands, size_t command_count,
                        unsigned flags, int read_origin_velocity,
                        codol_delta_context_t *context);
```

The caller owns the states, selects the acknowledged base, and — the part that
is easy to get wrong — **must record the quantized client-held state only after
a successful send**. If the send fails and the server has already recorded what
it *would* have sent, every later delta is computed against a baseline the
client never received.

`CODOL_WriteSnapshotDeltaNumber` exists because the wire `deltaNum` and the
effective serialisation base are not always the same value: C1 treats
`deltaNum 0` with `flags 0` as its pinned baseline.

The send path is staged for the same reason as the frame build:

```c
int  CODOL_PrepareClientSnapshot(unsigned n, msg_t *msg, ...);
int  CODOL_ClientSnapshotPrepared(unsigned n, const msg_t *msg, uint32_t sequence);
int  CODOL_FinishClientSnapshot(unsigned n, const msg_t *msg, uint32_t sequence, int accepted);
void CODOL_DiscardClientSnapshot(unsigned client_number);
```

Prepare builds the message and holds it pending; finish commits it to the
history only when the transport accepted it; discard throws it away.

## Where to read further

- [Snapshot and frame publication](/wiki/subsystem-snapshot) — the field tables,
  the delta codec, events and config strings.
- [Transport](/wiki/subsystem-net) — how the bytes get there.
- [Player input and movement](/wiki/subsystem-player) — what happens between the
  usercmd and the playerState.
