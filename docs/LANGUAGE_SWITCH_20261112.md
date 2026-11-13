# Language switch while the game runs (`k33_language`), 2026-11-12

`locale_switch.c`, `locale_switch_policy.h`, `patch_locale_policy.h`, the patch-folder code in `kernel33.c`.
Client: CoDOL 1.1.1.13 (`C1.exe`, PE timestamp 5314DEF0). Status: **builds and passes its source tests; not yet
run in a client** (see "Not verified").

## Contract (what a menu or a script may rely on)

| name | type | meaning |
|---|---|---|
| `k33_language` | string, archived | The requested language: a name of `k33_languageList`. Set it (`setdvar` from a movie, the console, `k33_setlanguage`) to change the language. `English`/`Chinese`/any case are accepted and stored as the list's spelling. A name that is not in the list is logged and put back. After startup it always names the language on screen (state 0) or the one saved for the next start (state 2). |
| `k33_languageList` | string, read only | Installed languages, sorted, `;` separated: `en;zh`. `zh` is always there (C1's own text); any other name is there when `client_shipRetail\patch\` holds a zone of that language. Fixed for the session. |
| `k33_languageState` | int, read only | `0` idle: `k33_language` is on screen. `1` switching: only ever set inside the frame that switches, a poll never sees it. `2` restart needed: the request is saved and applies on the next start; the screen keeps the previous language. Setting `k33_language` back to the language on screen returns to `0`. |
| `k33_languageSerial` | int, read only | Starts at 0, +1 each time the language on screen changed. Anything that holds translated text re-creates it when this changes. |
| `k33_setlanguage <name>` | command | `set k33_language <name>` after checking the name; without a name prints the list, the language on screen, state and serial. |

Timing: the four dvars exist from the first frame after C1 registered its own dvars (before the login screen). A
request is read on the main thread once per frame (`K33OnMainTick`, the eighth and last free slot of
`K33_MAX_TICKS` in a local-testing build with every optional tick). The frame that sees a changed `k33_language`
decides (`LsDecide`), and for a live switch the new language answers every lookup from that frame on: serial and
state are final in the same frame. A movie that did `setdvar k33_language zh` can therefore poll
`k33_languageSerial` / `k33_languageState` from its next frame: serial changed -> rebuild text; state 2 -> show
"applies after restart".

What a movie must do on a serial change: every text it set from a localize key is a copy Scaleform holds (the
translator `0x6A5AC0` copies), so it must assign those texts again (`.text = "@KEY"`, `LocalizeUtil.localize`, list
labels, HTML text it built). Text that C1 draws natively (HUD strings looked up per draw) changes by itself. Text
C1 formatted earlier and stored (obituary lines already printed, a chat line, configstrings a server sent) keeps
its old language.

Startup order (`CodolStartupLanguage`): the last valid `+set k33_language <name>`; else `+set k33_english 0|1` /
`+set codol_patchlang en|zh` (unchanged, `CodolPatchLanguageFromArgv`); else the last `seta k33_language "<name>"`
line of a plaintext `players\config_mp.cfg`; else `en`. A language with no zone in the patch folder starts as
`en` when English is installed, else `zh`; `Kernel33.log` says which and why (`language: "zh" (the saved
k33_language)`). At registration the dvar is set to the language the client started in, so a language chosen on
the command line is also what gets saved.

## What it does

A language is which string a localize name (asset type 27) resolves to. Three things are added, all installed
only when every C1 site below has C1's bytes (`LocaleSwitchCheck`; one mismatch -> `language: bytes at 0x...
differ; no live switch`, nothing is patched, other languages' zones are skipped as before, and a request ends in
state 2):

1. **Other languages are loaded as duplicates.** `LoadPatchDir` no longer skips the `codol_<name>_locale.ff` of a
   language that is not the startup one: it queues it after the `code_post_gfx` zones (the existing
   `*_postgfx_mp` path, alloc flag 1). At that point nearly every name in it is already held, by the startup
   language's zone or by a stock code zone, so C1's linker discards the string (`DB_LinkXAssetEntry`
   `0x4CC621`-`0x4CC639`: same alloc flags, the first zone keeps the name). The call that frees the duplicate
   (`0x4CC639`, `call 0x4CB320`, `esi` = duplicate, `ebx` = holder) goes through `LS_DiscardHook`, which copies
   name and value into this DLL's table for that language first. No asset entry and no pool slot remain.
2. **Displaced stock strings are kept the same way**: a stock string discarded because a language's zone holds
   its name goes into one "stock" table. That is the Chinese text an English client otherwise never has.
3. **Lookups consult the tables.** All eleven `DB_FindXAssetHeader(27, name)` call sites (and the translator's
   existence test) call `LsFindHook` / `LsExistsHook`:
   - the shown language's table, when it is not the startup language;
   - else C1's entry. If that entry belongs to a language zone that is not the one shown (found through a map
     built once: localize pool slot -> entry, `LsMapOwners`): the stock table's string; else the first-linked
     entry of a non-language zone chained under it (gametype, map and UI zones have other alloc flags and chain
     as overrides, `0x4CC648`); else NULL, which every site handles (`test eax, eax` is part of the byte check).
   - otherwise C1's entry as it is.

A switch is one interlocked store of the shown language index. There is no load, no unload, no pool write, no
"database idle" precondition, and it works in a match. The result for every name is what a client started in
that language resolves, with one exception listed under "Differences".

## Why the pool is not cleared and reloaded

That was the requested mechanism (unload the locale zone under an alloc flag of its own, load the other one,
promote its entries). It was not built, for measured reasons:

- **The asset entry table is the limit, not the localize pool.** `0x1C402C8` holds 59,999 usable entries for all
  assets of all types (init `0x4CB26B`-`0x4CB294`, free list `0x18779BC`, uint16 links). Measured by reading
  the tables of a running client and of twelve crash dumps (`k33-devtest\DMP*.tmp`, read only):

  | state (English client) | entries used | localize | zones |
  |---|---|---|---|
  | front end, current zones (live client and `CODOL_Builds_quit_popup_20261029.dmp`) | 40,273 | 11,190 | 19 |
  | in a match, mp_killhouse_sh (`DMP5FB9.tmp`) | 47,530 | 10,743 | 21 |
  | front end after a match (`DMP439A.tmp`, `DMPEFB2.tmp`) | 46,980 | 10,057 | 19-20 |

  For Chinese to be reachable from an English start inside C1's pool, the stock strings English displaces must
  keep their entries (up to 7,739). That is about 55,300 of 59,999 in the smallest map, for every English player,
  switched or not. Whether a larger map overflows was not measured; the failure would be C1's fatal "exceeded
  limit" error.
- **Freeing zone memory is not readable.** `PMem_Free` (`0x8373109`) and `DB_LoadXAssets` (`0x8465768`) are
  virtualized. `PMem_BeginAlloc` is inlined at `0x4CD6AF`-`0x4CD6FB`: alloc type 0 for flags 1 or 0x10, else 1;
  a list of 32 `{name, pos}` per type with no bounds check. Measured: type 0 grows up, type 1 down from
  `0x12C00000`, and a locale zone at the head of the pre-gfx list is the second block of its stack, so its
  first unload would be a mid-stack free whose behaviour cannot be read.
- **C1 hands out raw `char *` into zone memory** (`0x5DA3B2` returns `entry->value`). Nothing here is ever freed,
  so no pointer C1 holds can dangle.
- `codol_english_locale` also carries `scaleform/fonts_ingame.gfx`; unloading it changes which font file is active.

What the request wanted (the language changes at once, completely, both ways, as often as wanted) is what this
does; what differs is that the strings of the language not shown stay in memory (about 0.5 MB of heap per
English-sized language) instead of being re-read.

## Addresses

M = measured (capstone over `C1.exe`, bytes compared at run time before use, or read from a live client / dump);
I = inferred.

| address | what | |
|---|---|---|
| `0x4CB8E0` | `DB_FindXAssetHeader(type, name)`; read lock at `0x4CB953`-`0x4CB990` (`++[0x2007A5C]`, wait while `[0x2007A60]`) | M |
| `0x5DA3A6 0x5F4A13 0x5FA7C2 0x5FA965 0x5FAAFB 0x5FACAB 0x5FAED5 0x5FC8A6 0x5FCB1D 0x5FCD87 0x6A5B23` | the eleven `push 1Bh ; call 0x4CB8E0 ; add esp,8 ; test eax,eax` sites: every call of it with type 27 in the image | M |
| `0x6A5AFA` | the translator's `push 1Bh ; call 0x4CB850 ; add esp,8 ; test al,al` | M |
| `0x4CC390` | `DB_LinkXAssetEntry`; `0x4CC621` same flags -> discard, `0x4CC648` other flags -> chain as override (new entry right behind the holder), `0x4CC6A6` replace (zone byte `+0xA4`, only `ffotd_client`) | M |
| `0x4CC639` | `call 0x4CB320`: free the duplicate entry (`esi`); `ebx` = the entry that keeps the name | M |
| `0x4CB350` | asset name hash (not used: entries are found by pool slot) | M |
| `0x1C402C8` | asset entries, 24 bytes: type +0, header +8, zone +0x10, flags +0x11, next +0x12, override +0x14 | M |
| `0x18FD9D8` | 60000 uint16 hash buckets | M |
| `0x1F0E108` | zones, 0xA8 bytes: name +0xC, alloc flags +0x4C, PMem type +0x50; count `0x18FCFBC`, load order `0x1C402A4`; limit 34 at `0x4CC824` | M |
| `0xCD5028[27]`, `0xCD4D68[27]` | localize pool block and size (24000 after `PatchLocalizePool`) | M |
| `0x26090BC` | event: database idle and its results taken (`0x4CC8D7`); `0x2609060` database thread id | M |
| `0x624740` | `Dvar_RegisterString` (prologue checked) | M |
| alloc flags | 1 code zones (never unloaded), 2 UI, 4 gametype/map, 8 load screen; 0x10 and above unused in every state looked at | M |
| language zones are never unloaded, so their entries and pool slots are fixed | no path frees flag 1 except shutdown `0x4CDEB0` | I (read from the callers `0x4CCE3D` mask 6, `0x598624` mask 2, `0x599CB7` mask 0x1E) |
| a discarded duplicate's header is still intact at `0x4CC639` | it was copied into the pool slot at `0x4CC521` and is freed only by the call | M (code), not run |
| all localize text goes through the eleven sites | no other reference to type 27 reads a value (the others are pool setup, enumeration for the pure check, and the linker) | I |

## Costs and limits

- English start: one extra zone load at startup (`codol_chinese_locale`, 426 strings), a few Chinese-only names
  become ordinary entries; two array reads per lookup.
- Chinese start: `codol_english_locale` (7.6 MB file, about 8 MB of zone memory, measured as its PMem block)
  loads at every startup and stays; the English names no zone held yet become entries, at most as many as an
  English client already has.
- At most `LANG_RESIDENT_MAX` (3) other languages are loaded: C1 holds 33 zones. More languages are listed but
  answer state 2.
- A language with only old-style zones (`codol_english_ui.ff` ... without `codol_english_locale.ff`) starts
  normally but cannot be switched to live.
- Each table holds 24,576 strings; more are dropped and the log says so.

## Fonts

Not touched. The `fonts_ingame.gfx` that linked first stays until restart: started in English, the English font
zone's Latin glyphs remain after switching to Chinese (CJK glyphs are the stock ones in both); started in
Chinese, English text is drawn with the stock font until restart.

## Differences from what was asked

1. No pool clear / zone reload (above).
2. `k33_languageState` 1 is never visible: the switch is synchronous.
3. A switch does not wait for an idle database and is allowed in a match.
4. Names that only one language has resolve in that language and to nothing (the key is shown) in every other,
   exactly as after a restart; there is no fallback to the startup language's text.
5. Started with a command-line language, that language is written to `k33_language` and so saved.
6. `test_ui_baseline.py` pins `kernel33.c` and `patch_locale_policy.h` hashes and fails until `ui_baseline.json`
   is re-recorded.

## Not verified

Nothing here ran in a client. The local-testing DLL links (`K33_LOCAL_TESTING=1 K33_NO_COPY=1 sh build.sh`), but
a test client needs a lobby, and no offline one is part of this branch. Open points, in order of weight:

1. The capture hook at `0x4CC639` runs for every duplicate of every type during every zone load (it returns at
   once for other types). Startup and a map load must be unchanged.
2. `LsMapOwners` waits for the event at `0x26090BC`; the log line `language: N string(s) belong to language zones;
   A of other languages and B displaced stock strings kept` must appear within seconds of the first frame, with
   A about 426 (English start) or about 7,700-10,154 (Chinese start) and B about 7,700 (English start).
3. Text after a switch, both directions, front end and in a match.
4. That nothing reads localize text outside the eleven sites (a string that stays in the old language after a
   movie was rebuilt would show it).

Manual test (test client, `+set k33_cmdFile 1`, both locale zones in the patch folder):

1. Start with no language option. Log: `language: "en" (the default)`, `patch folder: patch\codol_chinese_locale.ff
   queued as zone "codol_chinese_locale" after code_post_gfx`, `language: live switch installed (12 lookups ...)`,
   then the `string(s) belong to language zones` line and `language: k33_language "en" of "en;zh"; live switch
   ready`.
2. Console / command file: `k33_setlanguage zh`. Log: `language: "en" -> "zh" live (serial 1; ...)`.
   `k33_languageSerial` 1, `k33_languageState` 0. Reopen a menu: Chinese. `k33_setlanguage en`: serial 2, English.
   Repeat ten times.
3. `set k33_language nope`: log `is not an installed language`, dvar back to the previous value.
4. Enter a match (zombies and MP), switch both ways, open the scoreboard and pause menu, leave the match, switch
   again at the front end.
5. Quit, check `players\config_mp.cfg` has `seta k33_language "zh"`, start without options: `language: "zh" (the
   saved k33_language)`, the game is Chinese, `codol_english_locale` is queued after code_post_gfx; switch to en
   and back.
6. Start with `+set k33_english 1` and with `+set k33_language zh`: the command line wins over the saved line.
7. Remove `codol_chinese_locale.ff`: list is still `en;zh`, `zh` switches live to C1's stock text (the UI movies'
   K33UI keys show as `@K33UI_...`, as in a Chinese start without that zone).
