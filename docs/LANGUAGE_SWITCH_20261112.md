# Language switch while the game runs (`loc_languageName`), 2026-11-12 / 2026-11-13

`src/locale_switch.c`, `src/locale_switch_policy.h`, `src/patch_locale_policy.h`, the patch-folder code in
`src/kernel33.c`, `zone_packs/common_extras.csv`, `zone_packs/english_locale.csv`.
Client: CoDOL 1.1.1.13 (`C1.exe`, PE timestamp 5314DEF0). Status: **ran in a test client** (offline lobby,
local-testing + diagnostics DLL, 2026-11-13): 48 switches at the front end, 24 in a hosted Killhouse match, 24 at the
front end after that match, 24 in a hosted zombies match on Mech Factory, and a switch before a map load. See
"Measured".

A switch **unloads** the language zone on screen (C1 frees its strings' memory, asset entries and localize pool slots)
and **loads** the chosen language's zone with C1's own zone loader. Nothing of a language that is not on screen is in
memory. This replaces the first implementation (every language resident in DLL tables, eleven lookup sites
redirected), which is removed. That one never ran in a client and could not have: its byte check of
`DB_FindXAssetHeader` compared the packed file's import reference (`8B 1D 50 D3 A1 09`) while the running client has
`8B 1D 64 21 B4 00`, so every start logged `bytes at 0x004CB8E0 differ` and the switch stayed off; with that check
corrected it installed, but its owner map waited for C1's idle event and gave up after 120 s in the one run measured
(`the database was not idle within 120 s of startup; no live switch in this session`; a Chinese start of the same
build did map them). Its cost, measured: +4 entries for an English start; for a Chinese start the 1,019 English-only
names were C1 entries for the whole session (+1,027 entries at the front end, +789 in a Killhouse match, +1,019
localize slots), whether the player ever switched or not. Neither is near the 59,999 limit in the states measured;
the present mechanism costs nothing.

## Contract (what a menu or a script may rely on)

| name | type | meaning |
|---|---|---|
| `loc_languageName` | string, archived | The requested language: a name of `loc_languageList`. Set it (`setdvar` from a movie, the console, `loc_setLanguage`) to change the language. `English`/`Chinese`/any case are accepted and stored as the list's spelling. A name that is not in the list is logged and put back. |
| `loc_languageList` | string, read only | Installed languages, sorted, `;` separated: `en;zh`. `zh` is always there (C1's own text); any other name is there when `client_shipRetail\patch\` holds a zone of that language. Fixed for the session. |
| `loc_languageState` | int, read only | `0` idle: `loc_languageName` is on screen. `1` switching: set in the frame that accepts the request (the unload and the load's queueing happen in that frame); back to 0 when C1's database has linked the new zone (measured 15-550 ms; at most 60 s, then 2). `2` restart needed: the request is saved and applies on the next start; the screen keeps the previous language. Setting `loc_languageName` back to the language on screen returns to `0`. |
| `loc_languageSerial` | int, read only | Starts at 0, +1 each time the language on screen changed. Anything that holds translated text re-creates it when this changes. |
| `loc_setLanguage <name>` | command | `set loc_languageName <name>` after checking the name; without a name prints the list, the language on screen, state and serial. |
| `loc_tableStats [label]`, `loc_cycle <n>`, `loc_forceUnload` | commands, local-testing builds only | log entry / localize / stack use; switch n times; let the next switch unload a zone that is not the top of its stack (measurement only: it corrupts the stack and ends the client, see "Memory"). |

A movie that did `setdvar loc_languageName zh` polls `loc_languageSerial` / `loc_languageState`: serial changed -> rebuild
text; state 2 -> show "applies after restart". Text a movie set from a localize key is a Scaleform copy (the
translator `0x6A5AC0` copies) and stays until the movie sets it again; text C1 draws natively (looked up per draw)
changes at once (measured in a match: `SPECTATING` / `观战中`, `TIE` / `双方持平`, `Left Mouse - Next player`,
zombies `Round 1/9` / `回合 1/9`). What the movies do is unchanged by this work: the pause menu rebuilds on a serial
change; the front end re-sets only some of its texts (measured: after a switch the Multiplayer lobby screen keeps the
old language until it is opened again; dialogs created after the switch are in the new one).

Startup order (`StartupLanguage`) is unchanged: the last valid `+set loc_languageName`; else `+set loc_english` /
`+set loc_patchLang`; else the last `seta loc_languageName` line of `players\config_mp.cfg`; else `en`.

## How it works

1. **Where the zone loads.** The startup language's `<name>_locale` zone is the last zone of the code_post_gfx
   list (`SubmitPatchZones`), alloc flag 1 like every code zone. Other languages' zones are not queued; they load when
   chosen. By then every stock code zone holds its names.
2. **No override entries (the entry limit).** C1's linker (`DB_LinkXAssetEntry 0x4CC390`) keeps both copies of a name
   that two zones with different alloc flags carry (the second as an override entry, `0x4CC648`) and throws the second
   away when the flags are equal (`0x4CC623`-`0x4CC639`). `LR_LinkHook` replaces the flag compare at `0x4CC61B`
   (`cmp ecx,[eax+0x1F0E154]`, 6 bytes -> `call` + `nop`; the hook ends with C1's own compare, so C1 takes its own
   branch). For a localize string of the language zone whose name another zone holds, `LrLink` puts the language's
   string into the holder's pool slot (one pointer store) and remembers the holder's own pointer by slot; C1 then
   discards the new entry (holder flag 1, 8,708 strings for English at the front end) or chains it as its ordinary
   override (holder flag 2/4/8: the name survives that zone's unload; that is the entry C1 uses for that pair
   anyway). Names only the language has are ordinary entries of the language zone (1,464 for English).
3. **The switch** (main thread, the one `K33OnMainTick` slot, when C1's "database idle" event `0x26090BC` is set):
   every pool slot that still carries the language's pointer gets the holder's own pointer back (found through the
   entries that own the slots now, under the database read lock), then the zone is unloaded: its alloc flag (`+0x4C`)
   is 0x10 for the duration of `DB_LoadXAssets({NULL, 0, 0x10}, 1, 0)` (`0x8465768`, virtualized), which unloads that
   group of one inside the call, on the main thread (measured). C1's per-zone unload `0x4CD9A0` is entered through
   `LR_UnloadHook` (6-byte `jmp` over `push ebp; mov ebp,esp; mov eax,[ebp+8]`), which passes `createDefault 0` for
   the language zone only. C1's loader asks for 1, which turns an entry without an override into a copy of the type's
   default asset until the next load, and the localize default `CGAME_UNKNOWN` exists only in `localized_common_mp`
   (in a match): at the front end that is C1's fatal `WIN_LOAD_ASSET_ERROR` (measured: the first prototype's first
   unload ended the client with it). With 0 the entries and slots are freed (`0x4CDAD1`). Then
   `DB_LoadXAssets({<new>_locale, 1, 0}, 1, 0)` queues the new zone; when the event is set again a sweep
   re-applies any shared name not in place yet, serial +1, state 0.
4. **Memory (the crux).** A zone's memory is a block of a PMem stack (`0x2B2C97C + type*0x10C`:
   `{name, count, pos, {name, pos}[32]}`; type 0 for flags 1 and 0x10, 1 otherwise, inlined at `0x4CD6AF`).
   `PMem_Free` is virtualized; measured by reading the stack before and after:
   - freeing the **top** block pops it: count and position return exactly to the block below, the freed pages are
     decommitted (`MEM_RESERVE`), the next zone is allocated there. 120 switches in this state: identical numbers
     after every switch (table below).
   - freeing a block that is **not** the top (`loc_forceUnload` in a match, the English zone block 14 of 19): the list
     entry's name is cleared but the stack position is rewound to the freed block, so the next zone
     (`chinese_locale`) was written at offset `0x1C88000`, over `patch_common_mp` / `common_mp`, and its list
     entry replaced `localized_common_mp`'s. The client crashed while linking it (AV read at `0x4CC435` on the
     database thread, AV at `0x4CB356` on the main thread). A mid-stack free is corruption, not a leak.
   The first match of a session adds `patch_common_mp`, `common_mp`, `patch_localized_common_mp`, `localized_common_mp`
   with flag 1 (`0x4CCD80`, only while common_mp is not loaded; C1 keeps them for the session) to stack 0, above the
   language zone. `LrMapLoadHook` (the map load's `DB_LoadXAssets` call `0x4CD0F0`) sees a flag-1 zone in that list
   while the language zone is the top block, unloads the language zone there (C1 has just unloaded groups 6 from the
   same function, `0x4CCE6C`, synchronously; C1's idle event is not set at that point although nothing loads, so it
   is not tested) and appends it to the list: it loads after common_mp and is the top block for the rest of the
   session (measured at Killhouse and Mech Factory and after leaving the match). Every switch reads the stack first
   (`LrGateDecide`): the language zone not on top -> state 2, nothing touched (measured with the reorder disabled:
   `"zh" saved, applies on restart (the language zone is not the top block of its memory stack)`).
5. **The font movie** (`scaleform/fonts_ingame.gfx`, the Latin glyphs) was in `english_locale`. Loading after
   code_post_gfx it loses to the stock copy (a rawfile, same flags, the first linked wins; measured: the active
   `fonts_ingame.gfx` was `patch_code_post_gfx_mp`'s), and a zone Scaleform's movie came from must not be unloaded.
   It is now part of `common_extras` (`part,english_font,only=rawfile`), ahead of code_pre_gfx in every language
   (measured: active zone `common_extras`). Chinese text keeps C1's CJK glyphs; Latin text uses the English font in both
   languages. `english_locale.ff` shrank from 7,595,839 to 202,238 bytes.

Fallbacks, all decided before anything is touched: a C1 site differs (`LocaleSwitchCheck`, run before anything is
queued) -> nothing is hooked, the startup language's zone loads ahead of code_pre_gfx, the others are skipped (the
behaviour before any language switch existed) and every request ends in state 2. Old-style zones of the startup
language (`english_ui.ff` ...) -> state 2. A switch that finds the stack, the free entries (< 4,096), the free
slots (< 4,096), C1's zone table (33) or the linker's replace mode (`0x19A2F6C`) wrong -> state 2. The database busy
for 60 s -> state 2. The new zone did not load -> C1's own text, state 2, logged.

## Measured (2026-11-13, own test client, read-only process reads; evidence logs in the scratch folder of the run)

Entries are C1's 59,999 asset entries in use (all types); localize = slots of the 24,000-slot pool in use; stack 0 =
the code zones' PMem stack (blocks, position).

| state | build | entries used | localize | stack 0 |
|---|---|---|---|---|
| front end, English | no language switch (the shipped DLL, whose check fails) | 40,342 | 11,211 | 15, 0x24CE000 |
| front end, English | this branch | 40,342 | 11,211 | 15, 0x24CE000, top english_locale |
| front end, English | first implementation, its check corrected (all languages resident) | 40,346 | 11,211 | |
| front end, Chinese | no language switch | 39,319 | 10,192 | |
| front end, Chinese (after switching) | this branch | 39,320 | 10,192 | 15, 0x23FC000, top chinese_locale |
| front end, Chinese start | first implementation, check corrected (English resident) | 40,346 | 11,211 | |
| Killhouse match, English | no language switch | 49,222 | 11,962 | 19 |
| Killhouse match, English | this branch (two runs: started en / switched from zh) | 49,235 / 49,210 | 11,961 / 11,936 | 19, 0x3EAD000, top english_locale |
| Killhouse match, English | first implementation, check corrected | 49,226 | 11,962 | |
| Killhouse match, Chinese | no language switch | 48,451 | 11,181 | |
| Killhouse match, Chinese | this branch (switched at the front end, then the map load) | 48,452 | 11,181 | 19, 0x3DDB000 |
| Killhouse match, Chinese start | first implementation, check corrected | 49,240 | 11,962 | |
| front end after the match, en / zh | this branch | 48,599 / 47,841 | 11,250 / 10,495 | 19 |
| Mech Factory (zombies) match, en / zh | this branch | 38,492 / 37,734 | 11,649 / 10,894 | 19, 0x3EAD000 / 0x3DDB000 |

Cycles (`loc_cycle 24`, numbers read after every switch): front end 2 x 24, Killhouse 24, front end after the
match 24, Mech Factory 24. In each, every English state and every Chinese state had the same entries, localize count,
zone count and stack 0 count/position (e.g. Killhouse: 12 x `10,789 free, 11,936 localize, stack 0 19 blocks
0x3EAD000`, 12 x `11,547 free, 11,181 localize, 19 blocks 0x3DDB000`): no leak, no growth. A switch took 15-30 ms to
Chinese (7.5 KB zone) and 110-550 ms to English.

Who holds pointers into the language zone (scan of all committed writable memory for the exact addresses of the
zone's strings; Killhouse, English, before a switch):

| holder | count | handled |
|---|---|---|
| the localize pool (slots in place and the zone's own slots) | 3,100 | put back / freed by the switch |
| this DLL's slot table and name index (454 KB) | 2,088 | cleared by the switch |
| C1 `.bss` `0x017C6904` (a transient name pointer) | 1 | C1 rewrites it (it pointed into the new zone after the switch) |
| C1 `0x020389E8`, `0x02038A08`, `0x02038A38`: entries of a 16-byte record list of rawfiles read at load time (`{size, size, name, next}`) | 3 | none: free records (size fields 0); C1 leaves the same stale name field in its own free records. They kept pointing at decommitted memory through the later switches and map loads without a fault. Inferred harmless, not proven: the code that walks the list is virtualized |
| about 20 single hits in other heaps / encrypted module data | 1 each | coincidental values (their neighbours are random bytes or packed uint16 pairs) |

After the unload the zone's pages are decommitted, so a pointer C1 kept and read later would fault, not show old text.
None was found besides the above and no fault happened in 168 switches across front end, two maps and a map load;
the scan was one moment of one map, so a holder that exists only during some other event (a kill feed line, an open
scoreboard) is not excluded.

## Addresses

M = measured (bytes compared at run time before use, disassembly of the running image, or read from a live client);
I = inferred. Every patched or called site is byte-checked in `LocaleSwitchCheck`.

| address | what | |
|---|---|---|
| `0x4CC600`-`0x4CC652` | `DB_LinkXAssetEntry`'s flag compare `0x4CC61B` (patched: `call LR_LinkHook; nop`); equal -> discard `0x4CC623` / `0x4CC639 call 0x4CB320`; not equal -> override `0x4CC648` | M |
| `0x4CB320` | free the entry in esi | M |
| `0x4CD9A0` | `DB_UnloadXZone(zone, createDefault)` (patched: `jmp LR_UnloadHook; nop`); createDefault -> `0x4CDB0B` (default copy; fatal `0x4CDB79` without a default), 0 -> `0x4CDAD1` (freed) | M |
| `0x8465768` | `DB_LoadXAssets(info, count, sync)`, virtualized; called; its call sites `0x59864A`, `0x4CCE6C`, `0x5984D2` are checked, `0x4CD0F0` is patched -> `LrMapLoadHook` | M |
| `0x4CCD80` | queues common_mp & co. with flag 1 while common_mp is not loaded | M |
| `0x4CD6AF` | inlined PMem_BeginAlloc: stack 0 for flags 1 and 0x10, stack 1 otherwise; stacks at `0x2B2C97C` | M |
| `0x4CC8D7`, `0x26090BC` | the database idle event | M |
| `0x1C402C8`, `0x18779BC`, `0x18FD9D8` | asset entries, free list, hash | M |
| `0x1F0E108` (+0xC name, +0x4C flags), `0x18FCFBC`, `0x1C402A4`, `0x187490C` | zones, count, load order, loaded flags | M |
| `0xCD5028[27]`, `0xCD4D68[27]`, `0x18FCFC0[27]` | localize pool, size, count | M |
| `0x19A2F6C` | linker replace mode (0 in every state seen) | M |
| `0x624740` | `Dvar_RegisterString` | M |
| after the first map load nothing else loads with flag 1 | measured for MP (Killhouse) and zombies (Mech Factory); other modes not looked at | I (gated at run time) |

## Costs and limits

- Entries and slots: the same as a client without the switch, at every state measured (table above); the language
  not shown costs nothing.
- One extra unload + load of the language zone on the first map load of a session (about 0.1 s, on the loading
  screen; for that moment the shown language's own names resolve to nothing and shared names to C1's text).
- With the match hosted in-process, the switch acts on the client's database only.
- At most `LANG_MAX` (8) languages are listed; one is loaded at a time.

## Not verified

1. A larger MP map than Killhouse (one hosted match per client run on this branch: the lobby refused a second
   `Restart the client before hosting another Local Play match`). The DLL adds no entries, so the map only moves
   C1's own number.
2. Pointer holders at moments other than the scanned one (see above).
3. Survival and Spec Ops (co-op) maps. A mode that loads further flag-1 zones would leave the language zone below them;
   every later switch in that session then answers state 2 (gated, logged), not a fault.
4. `ui_baseline.json` pins `src/kernel33.c` and `src/patch_locale_policy.h`: `tests/test_ui_baseline.py` fails until
   the baseline is re-recorded after review (`python -m zonekit.ui_baseline --record`).

## Manual test (local-testing build, `+set com_cmdFile 1`)

1. Start. `Kernel33.log`: `language: live switch installed (link hook 0x004CC61B, unload hook 0x004CD9A0)`,
   `language: "en" zone "english_locale" loaded: 8708 string(s) in place`, `language: startup: ... top
   "english_locale"`.
2. `loc_setLanguage zh`: `"en" -> "zh" live (serial 1, ...)`; native HUD text and newly opened menus are Chinese.
   `loc_cycle 24`: 24 `live` lines; the `after the switch` numbers are identical per language.
3. Enter a match: `the map load adds code zones; "english_locale" is unloaded and loads again after them`;
   switch in the match; leave; switch at the front end.
4. `set loc_languageName nope`: `is not an installed language`, the dvar goes back.
