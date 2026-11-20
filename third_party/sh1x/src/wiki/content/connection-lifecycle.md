---
slug: connection-lifecycle
title: How a client connects
space: architecture
summary: From the lobby's session key to a spawned player — handshake, association, connect, gamestate, and the pure check.
position: 20
tags: [net, architecture]
---

A C1 client does not arrive at the server cold. By the time it sends its first
packet, an out-of-band lobby has already told it where to connect and has issued
a session identity that the server must recognise. The connection then proceeds
through a DTLS-shaped handshake before a single byte of game protocol is spoken.

## 0. Before the client arrives

The alpha0022 lobby (`dw-capture.py` on udp/3074) does two things the server
depends on:

- it writes the live **16-byte session key** to `securitykey.bin`, which the
  server reads through `codol_securityKeyPath`;
- it stamps the **map, gametype and zone type** it told the client to load into
  `startgame.txt`, which the launcher consumes.

If the key is missing the handshake is never accepted and the client simply
times out. If the map disagrees, the client loads one world while the server
publishes another in its connect response, serverinfo and gamestate.

## 1. Handshake

Four messages, held in `codol_handshake_t`:

```c
typedef struct {
    codol_association_t data;
    uint8_t init[16], init_ack[38];
    uint8_t private_key[28], public_key[100];
    uint8_t echo[189], cookie_ack[114];
    unsigned pending, established;
} codol_handshake_t;
```

**INIT → INIT_ACK.** `CODOL_HandshakeInit` binds the state to a UDP endpoint and
takes the trusted session ID, a fresh private scalar, a local tag and an RNG. It
returns `1` for a new handshake, `2` for a retransmission and `0` for a
rejection. That distinction is load-bearing: a **duplicate INIT reuses the
original cookie**, and the caller clears the old game client *only* on a
successful new INIT. Treating a retransmission as new would drop a connected
player every time a packet was duplicated.

**COOKIE_ECHO → COOKIE_ACK.** `CODOL_HandshakeCookie` takes the security key
belonging to that session ID and performs ECDH on P-224
(`CODOL_ExportPublicKey`, `CODOL_SharedSecret` — 28-byte scalars, 100-byte
public keys, a 28-byte shared X coordinate). A successful COOKIE_ECHO **enables
DATA MAC verification, not player identity**. An exact retransmission returns
the cached ACK without resetting the data association.

## 2. Association

Once the handshake is validated the owner starts a record association:

```c
int CODOL_AssociationStart(codol_association_t *, const uint8_t shared_x[28],
                           const uint8_t security_key[16],
                           uint16_t local_tag, uint16_t peer_tag);
```

`CODOL_DeriveKey` turns the shared X coordinate and the security key into the
24-byte record key. From then on every packet is sealed and opened:

```c
size_t CODOL_AssociationSeal(codol_association_t *, uint8_t *, size_t, const uint8_t *, size_t);
int    CODOL_AssociationOpen(codol_association_t *, uint8_t *, size_t, size_t *,
                             const uint8_t *, size_t, const uint8_t candidate[16]);
```

Three rules in that code are easy to get wrong and are commented in the header:

- **Sealing consumes a counter** before the socket send. Retrying a failed send
  must encrypt again with a fresh counter, never resend the same record.
- **Replay, tag and MAC failures leave both the output and the association
  unchanged.** A forged packet cannot advance the receive window.
- The optional `candidate` key lets the trusted lobby hand over a new session
  key mid-flight; it is adopted **only if the record authenticates under it**.

`CODOL_AssociationOpenDetailed` gives a failure reason for the log and contains
no key material or plaintext.

## 3. Connect

Now the ordinary game handshake runs, in plaintext out-of-band packets:

```c
size_t CODOL_WriteChallengeResponse(uint8_t *, size_t, int challenge);
size_t CODOL_WriteConnectResponse(uint8_t *, size_t, const char *map,
                                  const char *gametype, unsigned max_clients);
int    CODOL_ReadConnectUserinfo(char *, size_t, const char *xuid, const char *info);
```

`CODOL_ReadConnectUserinfo` validates `connect <16-hex xuid> "<userinfo>"` and
requires the single xuid field to match the envelope. The header is explicit
that this is **structural consistency, not identity authentication** — identity
comes from the association owner, which got it from the lobby.

The connect response's values deliberately differ from C1's protocol and
shortversion dvar defaults.

## 4. Pure check

```c
int CODOL_VerifyPureChecksums(int argc, const char *const *argv,
                              const char *loaded_checksums, uint32_t feed);
```

The client sends `cp @ <referenced signed checksums> <feed XOR count XOR
checksums>`. Only **archive** checksums are validated — this is not fastfile
verification, and the wiki should not be read as claiming it is.

## 5. Gamestate and entering the world

The server publishes config strings and the baseline entity set, the client
loads the map, and then the usercmd queue is seeded from authoritative C1 state:

```c
int CODOL_C1CmdQueueSeed(codol_c1_cmd_queue_t *queue, const codol_usercmd_t *baseline);
```

The baseline is a **cursor only** and is never scheduled for execution. From
here the [frame loop](/wiki/server-frame) takes over.

## Diagnostic mode

`CODOL_NetworkReceive` returns `1` when it consumed an encrypted packet and `0`
when the server is in plaintext diagnostic mode; `CODOL_NetworkSend` returns
`-1` in that mode. Raw handshakes bypass `NET_SendPacket` entirely. This is how
the transport can be exercised without a real client.
