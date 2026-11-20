---
slug: configuration
title: Configuration
space: operations
summary: The codol_* dvars — what each one selects, which are latched, and which exist to bisect behaviour against a live client.
position: 10
tags: [operations]
---

CoDOL adds its own dvars alongside the retained CoD4x ones. They divide into
three groups: paths that tell the server where the game data is, identity that
the lobby supplies, and switches that exist so behaviour can be bisected against
a live client.

The launcher sets most of them — see [Building and
running](/wiki/build-and-run).

## Paths

| Dvar | Selects |
|---|---|
| `codol_zonePath` | Where fastfiles are looked for. |
| `codol_clientExe` | The client executable the IWD key is read from. |
| `codol_profilePath` | The directory holding the lobby's secure profiles. Read at startup (`Com_StartupVariable`). |
| `codol_securityKeyPath` | The 16-byte session key `securitykey.bin` the lobby writes at ready-up. |
| `codol_scriptPath` | The IW4 vendor scripts. Lowest script precedence. |
| `codol_scriptOverlay` | The alpha0022 `gsc_overlay`. Middle precedence. |
| `codol_scriptNativeOverlay` | This project's adaptations in `scripts/codol`. Highest precedence. |

The three script roots are searched in that order by
`CODOL_ReadScriptSourceWithNativeOverlay` — see [The GSC script
surface](/wiki/subsystem-script).

## Identity

| Dvar | Default | Meaning |
|---|---|---|
| `codol_securityId` | `4600000000000000` | Trusted lobby bdDTLS security ID, 16 hexadecimal digits. |

This is the identity the [handshake](/wiki/connection-lifecycle) validates
against. It is trusted input: it comes from the lobby, never from a client's
connect userinfo.

## Asset selection

| Dvar | Default | Meaning |
|---|---|---|
| `codol_zoneType` | `mp` | CoDOL asset category — `mp`, `coop` or `zm`. **Distinct from `g_gametype`**, and `CVAR_LATCH`: it takes effect at the next map load. |
| `codol_animlistMask` | `0` (range 0–15) | Animation classes loaded at the next full map load: human `1`, dog `2`, civilian `4`, zombie `8`. Requires matching client assets. |

`codol_zoneType` being distinct from `g_gametype` is a common source of
confusion: the gametype selects the script, the zone type selects which asset
category is mounted.

## Behaviour switches

These exist to bisect a behavioural change against a live client: turn one off,
reconnect, and see which half of a change caused the symptom.

| Dvar | Default | Effect |
|---|---|---|
| `codol_publishExtendedPlayerState` | on | Publish the damage-feedback and viewmodel / view-lock / prone player-state fields. |
| `codol_exitToLobby` | on | Return clients to the lobby at match end with `t "EXE_MATCHENDED"`. |
| `codol_turretFriendlyBlockDot` | `0.95` | Dot-product threshold at which a friendly blocks a turret's line of fire. |

## Retained settings that matter here

| Dvar | Launcher value | Why |
|---|---|---|
| `sv_fps` | `20` | One [server frame](/wiki/server-frame) every 50 ms. |
| `net_port` | `17733` | Must match the port the lobby handed the client. |
| `developer` | `1` | Enables the `CoDOL <topic>:` traces and rawfile dumps, **and** every builtin registered with the developer flag — see [GSC builtins](/wiki/gsc-builtins). |
| `logfile` | `2` | Flushed log at `bin/home/main/qconsole.log`. |
| `fs_basepath` / `fs_homepath` | install / `bin/home` | Where game data is read from and where writes go. |
| `rcon_password` | generated | Local-only credential the lobby's match-start dispatcher uses. |

## Finding the rest

Console `dvarDump`, or search the source: every registration is a
`Cvar_Register*` call with its help string as the last argument. The wiki
indexes those functions — [search for
`Cvar_Register`](/reference/symbols?q=Cvar_Register).
