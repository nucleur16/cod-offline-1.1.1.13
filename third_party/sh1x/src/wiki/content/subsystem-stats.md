---
slug: subsystem-stats
title: Profiles, progression and match data
space: subsystems
summary: src/codol/stats — the read-only secure profile, structured data, experience and unlocks, skill rating, the scoreboard and the match-data upload.
position: 90
tags: [stats]
---

Everything a CoDOL player carries between matches lives behind this area: the
secure profile the lobby stores, the structured records inside it, and the match
data the server reports back. It is written defensively, because almost every
function here is a read of somebody else's authenticated data.

## Structured data

A profile is not a struct. It is a **structured record** described by a schema
that lives in a mounted archive:

```c
int CODOL_ProfileRecord(const uint8_t *profile, size_t size,
                        const codol_structured_schema_t *,
                        const codol_structured_definition_t **definition,
                        const uint8_t **record, size_t *size_out);
int CODOL_ClientProfileScalar(unsigned client, const codol_structured_schema_t *,
                             const codol_structured_path_t *, size_t count, uint32_t *out);
```

Reads select the **unique definition matching the upload's version and format
checksum**; the uploaded CRC sits in four bytes before the record. The header is
explicit about what this does *not* do: it does not establish CRC authenticity,
and it does not translate IW4 fields to CoDOL fields.

## Identity is somebody else's job

The rule stated repeatedly in `codol_profile.h`:

> `trusted_uid` must come from a trusted session owner, **never directly from
> connect userinfo**.

```c
int CODOL_ReadSecureProfile(const char *directory, uint64_t trusted_uid, int per_uid,
                            const char *seed_basename, uint8_t *out, size_t capacity);
int CODOL_AttachClientSecureProfile(unsigned client, uint64_t trusted_uid,
                                    const uint8_t *, size_t,
                                    const codol_structured_schema_t *);
```

Selection tries `mpdata_s.<decimal uid>` when per-uid storage is enabled, then
`mpdata_s`, then an optional seed basename. An existing file that is unreadable
or invalid **fails without falling back** — silently substituting a different
player's profile would be far worse than refusing.

`CODOL_DecodeStoredProfile` decodes the lobby's zlib stream and requires exactly
32768 decoded bytes and a valid CRC32 over bytes 4 onward.

## Three views of one profile

This trio exists because scripts can edit a profile in-session and different
callers need different answers:

| Accessor | Returns |
|---|---|
| `CODOL_ClientSecureProfile` | The shared view: the session copy if a script edited it, otherwise the attached bytes. |
| `CODOL_ClientSecureProfileAttached` | The attached bytes themselves, ignoring session edits — for callers that must see what was actually attached. |
| `CODOL_ClientSecureProfileWritable` | A copy-on-write session view, dropped on detach and re-attach. **Never uploaded, stored or re-authenticated.** |

## Progression reads

Each of these is a read-only adapter over an explicit profile plus a mounted
table. None of them writes a profile.

```c
int CODOL_ProfileExperience(..., const codol_engine_table_t *, int *rank, int *experience);
int CODOL_RankForExperience(const codol_engine_table_t *, int experience, int *rank);
int CODOL_ProfileChallengeState(..., const char *challenge, int *out);
int CODOL_ProfileItemUnlocked(..., const codol_engine_table_t *unlocks,
                              const codol_engine_table_t *ranks, ...);
int CODOL_ProfileCharacter(..., const char *faction, codol_profile_character_t *out);
```

Details that are easy to get wrong and are pinned down here:

- **Rank lookup uses numeric rank keys**, not physical CSV row indices. Missing,
  duplicate or non-contiguous thresholds fail rather than producing a plausible
  rank. Above-max XP returns maxrank.
- **Experience conversion includes rankVersion mismatch clamping**, and returns
  the zero-based stored rank and total XP atomically.
- **Challenges** resolve a name to a column-4 ID, read `challenges[ID]` as float
  progress and return IW4's one-based *next* tier.
- **Unlocks** follow IW4 exactly, including that an unknown item row returns
  *unlocked*. An unknown title or icon override may permit `true` when ordinary
  requirements prove access, but can never establish a locked result.
- **Characters** read the C1 faction selector plus all twenty
  `characterClasses[index].slots` values. Faction is `tf`/`sc`/`zm`.

## Skill rating

```c
int CODOL_UpdateProfileSkills(unsigned winner, unsigned loser, const char *mode, float weight);
void CODOL_Scr_UpdateSkill(void);
```

Applies the IW4 skill reducer to two actual public playerdata records, and
returns `0` **before any rating write** if either schema, path or record is
invalid — so a partial update cannot leave one player rated and the other not.
It does not imply upload or persistent storage.

## Local progress

```c
int CODOL_LoadClientProgress(unsigned client, uint64_t uid);
int CODOL_SaveClientProgress(unsigned client, uint64_t uid);
```

Server-local session storage at `<fs_homepath>/codol_progress/<uid>.txt`. A
missing file is success with nothing loaded; a store with no entries **writes
nothing and removes no existing file**, so an empty session cannot erase what
was there.

## Match data

Two records, with different lifetimes and different destinations.

**Per-client** (`codol_client_matchdata_t`): selected by name, idempotent for the
same ASCII-case-insensitive name, reset at `SV_ClearServer` before another map
starts. Requests are tracked per client so the server can send when due:

```c
void CODOL_RequestClientMatchData(unsigned client);
int  CODOL_ClientMatchDataDue(unsigned client, uint32_t time);
void CODOL_ClientMatchDataSent(unsigned client, uint32_t time);
int  CODOL_WriteClientMatchDataSection(codol_msg_t *, const char *name,
                                       const uint8_t *record, size_t record_size);
```

The writer appends opcode 4, the name, a NUL and the full record **before the
enclosing message EOF**, preserving any pending bit pool. Invalid input sets
`message.failed` without changing the size, bit cursor or payload bytes.

**Per-server** (`codol_server_matchdata_t`): selecting a definition writes its
little-endian version and checksum into the first eight record bytes and leaves
the rest untouched, as C1 does. The status enum names every outcome —
`LOADED`, `UNCHANGED`, `ALREADY_SET`, `NAME_TOO_LONG`, `UNAVAILABLE`,
`TOO_LARGE` — so callers do not have to infer why.

```c
int CODOL_PrepareServerMatchDataSubmission(codol_server_matchdata_submission_t *out);
int CODOL_DeliverServerMatchData(const codol_server_matchdata_submission_t *);
```

Preparation mirrors C1's queued sender: re-resolve the definition and hand
`{record, definition size}` to the upload path. `-1` means the definition is
unusable — C1 itself warns `Unknown def file %s` and submits a zero length.

The schema can be released independently (`CODOL_ReleaseServerMatchDataSchema`)
because it borrows names and the offset map from mounted archives; the selected
name and record survive, and the next access reloads and re-checks the stored
version/checksum header.

## Scoreboard

`codol_scoreboard_stats.c`, `codol_scoreboard_wire.c` and
`codol_scoreboard_sender.c` collect per-player scoreboard values, encode them,
and schedule the send. See `docs/CODOL_SCOREBOARD_CONTRACT.md` for the wire
contract this implements.

Browse every declaration in this area: [stats](/reference/areas/stats).
