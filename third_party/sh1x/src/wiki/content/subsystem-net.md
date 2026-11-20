---
slug: subsystem-net
title: Transport
space: subsystems
summary: src/codol/net — handshake, ECDH, record encryption, fragmentation, the bit-level codec every other area serialises through, and the usercmd queue.
position: 10
tags: [net]
---

`src/codol/net/` is the whole path between a UDP socket and a decoded message.
It is the most self-contained area in the tree: 23 files, no dependency on game
state, and nearly all of it under unit test.

Related: [How a client connects](/wiki/connection-lifecycle) walks the same code
in the order it runs.

## The bit codec

Everything else in the server serialises through `codol_msg_t`. It is a cursor
over a caller-owned buffer with a sticky failure flag:

```c
typedef struct {
    uint8_t *data;
    size_t size, capacity, cursor, bit;
    int failed;
} codol_msg_t;
```

The `failed` flag is the important design choice. A read past the end, or a
write past capacity, sets `failed` and every later call becomes a no-op, so
parsing code can read a whole structure and check once at the end instead of
testing every field. Callers **must** check it: a message that overran returns
zeroes that look like real values.

| Reading | Writing |
|---|---|
| `CODOL_MsgReadInit` | `CODOL_MsgWriteInit` |
| `CODOL_ReadBits`, `ReadByte`, `ReadShort`, `ReadLong`, `ReadData` | `CODOL_WriteBits`, `WriteByte`, `WriteShort`, `WriteLong`, `WriteData`, `WriteString` |

## Crypto

| Function | Role |
|---|---|
| `CODOL_ExportPublicKey` | P-224 public key (100 bytes) from a 28-byte private scalar. |
| `CODOL_SharedSecret` | 28-byte big-endian shared X coordinate. |
| `CODOL_DeriveKey` | 24-byte record key from the shared X and the lobby's 16-byte security key. |
| `CODOL_EncryptRecord` | Seals a record under a counter and tag. |
| `CODOL_DecryptRecord` | Verifies MAC and exact length *before* decrypting; outputs are untouched on any failure. |

`CODOL_DecryptRecord` deliberately does not implement the replay window — it
reports the counter and leaves the window and high-water mark to
`codol_association.c`, so the crypto layer has no per-peer state of its own.

## Association

`codol_association_t` is the per-client record state: the shared X, the derived
key, the receive high-water and window, the next send counter, and the two SCTP
style tags.

```c
typedef struct {
    uint8_t shared_x[28], key[24];
    uint32_t receive_high, receive_window, send_next;
    uint16_t local_tag, peer_tag;
    unsigned active, received, send_exhausted;
} codol_association_t;
```

`send_exhausted` exists because the counter space is finite; when it is set, the
association can no longer seal and must be renegotiated rather than wrap.

## Packets and fragments

```c
size_t CODOL_WritePacket(uint8_t *out, size_t capacity, uint32_t sequence,
                         int qport, const uint8_t *message, size_t length, ...);
int    CODOL_ReadFragment(codol_fragments_t *state, uint8_t *buffer, size_t capacity,
                          uint32_t sequence, const uint8_t *data, ...);
void   CODOL_Xor(uint8_t *data, size_t length, uint8_t key,
                 const uint8_t *command, size_t command_length);
uint32_t CODOL_ExtendCounter(uint16_t wire, uint32_t last);
```

`codol_fragments_t` reassembles one sequence at a time with a four-word received
bitmap. `CODOL_ExtendCounter` widens the 16-bit on-wire counter back to 32 bits
against the last value seen — the standard sequence-extension problem, and the
reason a long stall can look like a replay.

## Client commands

```c
int CODOL_ReadClientCommand(codol_msg_t *message, uint32_t *sequence,
                            char *command, size_t capacity);
```

Reads the payload after `clc_clientCommand`: a little-endian sequence and a
NUL-terminated string. Output and the source cursor are **atomic** — malformed
or overlong input only marks the source failed, it never leaves a half-copied
command behind.

## The usercmd queue

`codol_c1_cmd_queue_t` holds pending commands plus three cursors: the last
accepted, the last executed, and the one executed before that. Storage is
supplied by the owner, deliberately, so queue depth is an integration decision
based on measured C1 traffic rather than a timing assumption baked into this
file.

Acceptance has four failure modes and one success, named in
`codol_c1_cmd_accept_t`:

| Result | Meaning |
|---|---|
| `ACCEPT_INVALID` | Malformed arguments. |
| `ACCEPT_FULL` | Queue full; the accepted cursor is left alone. |
| `ACCEPT_ORDER` | `serverTime` did not advance in signed modular order. |
| `ACCEPT_IGNORED` | Stale or repeated; harmless. |
| `ACCEPTED` | Queued. |

Execution is a two-step so that a command is only consumed if the game actually
ran it:

```c
codol_c1_cmd_exec_t CODOL_C1CmdQueueBeginExecute(const codol_c1_cmd_queue_t *,
                                                 uint32_t server_time,
                                                 uint32_t max_lead_ms,
                                                 codol_usercmd_t *command);
int CODOL_C1CmdQueueCommitExecuted(codol_c1_cmd_queue_t *, uint32_t server_time);
```

`BeginExecute` never mutates cursors and answers `EMPTY`, `READY` or `FUTURE`.
`CommitExecuted` advances the executed cursor **only after native
ClientThink/Pmove has successfully consumed the exact head**.

## Entry points from the rest of the server

```c
void CODOL_NetworkInit(void);
void CODOL_NetworkReset(void);
int  CODOL_NetworkAuthenticated(netadr_t *);
int  CODOL_NetworkReceive(netadr_t *, const void *, int);
int  CODOL_NetworkSend(netadr_t *, const void *, int);
```

Browse every declaration in this area: [net](/reference/areas/net).
