# Zombies authored door USE regression (2026-10-29)

## Cause and correction

Native base `789e5779`, rawfiles base `f05ed0c`.
`ab21a3699` made `PlayerUseAllowed` reject `SVF_NOCLIENT` while
protecting owner-only equipment. This confused snapshot suppression with
interaction suppression. Retained `_g_mover.asm::trigger_use_shared` always
sets `r.svFlags = 1` (`SVF_NOCLIENT`) on invisible authored use brushes;
it also sets usable contents `0x200000` and hint `1`. The added rejection
therefore removed ordinary Zombies doors/debris from the candidate list.

The native correction removes only that flag check. The real retained
`ScrCmd_Hide`, `ScrCmd_Show`, and `ScrCmd_ShowToPlayer` manage `clientMask`
at entity offsets `+0xf8/+0xfc`; those masks remain enforced, together with
per-client `disablePlayerUse` and all lifecycle resets. No map whitelist,
script-trigger proxy, automatic unlock, free purchase, or geometry deletion
was introduced. Door price, power gating, target animation/movement,
paired-trigger removal, and authored zone flags remain the existing GSC path.

Actual read-only Factory capture (owned PID113404, old binary) observed
live triggers 518/520/498 with correct bounds, contents `0x200000`, hint1,
string8/8/6, `svFlags=1`, but player cursor hint0/entity2047. Script gates
were use1/alive1/valid1/revive0. A separate synthetic-notify diagnostic
charged/moved these same blockers; that isolated dispatch but is not counted
as real purchase proof.

## Actual production-path proof

`tests/fixtures/zombies_door_purchase_probe.json` retains sanitized exact
before/after owned-server observations. No user client/server was changed.
Before correction, actual USE produced zero charge on all attempted
Factory/Sumpf/Theater triggers. After correction:

| Map | Actual purchased triggers | Charged |
|---|---|---|
| Factory | debris518, debris520, door498 | 1000 each |
| Sumpf | stairs debris115, door116 | 1000 each |
| Theater | door74, door76 | 1250, 750 |

Buying Sumpf115 also removes its other-side trigger122; buying Theater74
removes its paired trigger75. Later test attempts on those deleted triggers
correctly find no live target. The stock purchase code deducted once,
removed the authored blockers/triggers and continued running without script
errors. This is a bot holding the real USE usercmd through native selection
and `Player_UpdateActivate`, not a script notification. No human C1 purchase
acceptance has been claimed yet. The reusable tool repeated all seven purchases
on fresh owned PIDs8176/122360/5416 with the same EXE hash, no runtime errors,
and clean exits; results are retained in the fixture.

Release build completed for both targets:

- EXE SHA256 `448BA91DEBA7623110973ECC54ABB48D2492FDD7EA54ED471EB3115E40E1A98C`
- host DLL SHA256 `6BF65C76117B0DD1E881057C538E4A4C53CF60CAB19EC82FFBB350D570084C3E`

Build: `make -j8 LOCAL_TEST=0 OS=Windows_NT CC=i686-w64-mingw32-gcc CPP=i686-w64-mingw32-g++ AR=i686-w64-mingw32-ar OBJ_DIR=obj/codol-win32 bin/sh1x_dedrun.exe host-dll`
inside WSL, log `obj/zm-door-20261029-build.log`.

## Gates and reproducibility

- Actual `script_playeruse.c` fixture compiled with 32-bit GCC,
  ASAN+UBSAN (alignment excluded for retained native layouts): PASS.
  Covers invisible usable brushes; owner-only slots0/31/32/63; future joins;
  hide masks; disable/enable; target reuse; client/all resets; invalid receivers;
  existing MP128/Zombies72 use radii. Existing fixture normalized to LF.
- Built-EXE and source resolvers: PASS for Zombies Factory/Sumpf/Theater,
  MP war/Killhouse, Coop Favela + `coop_spec_mp_fav_tropical`, and Survival Favela.
- Raw developer harness adds only a timed gate diagnostic; production door
  scripts are unchanged. Raw `_zombiemode_spawner.gsc` belongs to the separate
  barrier-melee fix and is not part of this door correction.
- `git diff --check` and rawfiles equivalent: PASS.

Re-run owned real purchases with a fresh output path:

```powershell
python tools/zm_door_probe.py --game 'D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13' --out obj/zm-door-new-probes
```

The tool refuses an occupied isolated UDP17761 or pre-existing result directory,
uses fresh credentials/home/profile, records binary identity and errors, and
quits only the child it created. It never addresses the user server.

Suggested central implementation tracking entry (parent-owned): shared native
use selection now distinguishes invisible authored use brushes from per-client
hidden equipment; release built and real USE purchases verified on all three
reported Zombies maps; human C1 acceptance pending deployment.
