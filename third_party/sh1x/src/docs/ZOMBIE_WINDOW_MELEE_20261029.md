# Zombies barrier melee strike restoration — 2026-10-29

Window attack clips played without damage because `should_attack_player_thru_boards` invoked the generic notetrack pump without a damage handler. Its `fire` event had no producer. Ordinary native melee uses an eye trace with range 64, and boards intentionally block it; increasing that trace globally would change ordinary combat.

The primary T5 reference is `D:/CoD/COD-GSC-Source/T5-STEAM-GSC/maps/_zombiemode_spawner.gsc`, `should_attack_player_thru_boards` and `window_notetracks` (approximately lines 969 and 1080). It applies window melee at the actual `fire` marker with horizontal/vertical actor reach 90 and window-anchor reach 51. The restored handler retains these bounds, actual claimed node/repair-anchor fallback, original clip selection and one contact per animation. It rechecks live player/laststand state, floor, window and node ownership at contact, and expires context after the clip.

Damage enters the existing real native `Scr_DoDamage` → retained `G_Damage` → Zombies player callback. The MP positional ABI is `player doDamage(damage, source, attacker, inflictor, headshot, MOD)`, unlike the reference SP-shaped call. This keeps native health, invulnerability, attacker cooldown and laststand handling. No direct health edit or timer-driven proxy attack was added.

## Validation

`node tests/zombie_window_melee_test.js` executes the actual production selector and handler bodies. It covers fire-only damage, duplicate/late markers, MP argument order, dead/laststand targets, actor death, changed node ownership, missing damage, movement/range/height, glass and authored anchor fallback. Source/built-EXE registration resolution passed for Prototype, Factory and Theater (1070 builtins, 98 scripts); the raw spawner diff passes whitespace validation.

An isolated real native Prototype server, PID 62896 / UDP 17762, used a native testclient and zombie, an actual authored attack spot/trigger marker and original window clip. The fixture only called the production selector; it did not inject a fire marker or damage call. Actual engine animation notetracks produced:

```text
WINDOWPROBE BEFORE hp 100 alive 1 session playing actor 129 range 33.7009 floor 22.9791 anchor 0
WINDOWPROBE AFTER played 1 hp 40 damage 60 laststand 0 contact 1
WINDOWPROBE OUTOFRANGE played 0 hp 40 damage 0
```

No script compile/runtime failures occurred. The owned process exited and port was released. EXE SHA-256 was identical before and after the run: `448BA91DEBA7623110973ECC54ABB48D2492FDD7EA54ED471EB3115E40E1A98C`; host DLL: `6BF65C76117B0DD1E881057C538E4A4C53CF60CAB19EC82FFBB350D570084C3E`. Exact source hashes and limits are in `releases/zombie-window-melee-20261029.json`; the diagnostic fixture is `tests/fixtures/zombie_window_melee_probe.gsc`. It belongs only in an isolated level overlay, never the production map chain.

This proves native damage on one actual Prototype barrier and shared rejection behavior. It does not claim a live C1 visual test or exhaustive map-barrier coverage. An earlier successful probe PID121300 overlapped a sibling build and is superseded by this exact-binary receipt. No user client/backend input, restart or deployment was performed.
