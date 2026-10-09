// coop -- the Spec Ops rules script.
//
// MEASURED: this is the script name C1 contracts for.
//   * mp_playlists.ff -> mp_playlists.info, lines 381-384:
//         gametype coop
//         name english "Spec Ops"
//         name chinese "<special operations>"
//         script coop
//     and there is NO `teambased` line, unlike tdm/dom/ctf/kc/inf.
//   * code_post_gfx_mp.ff -> maps/mp/gametypes/_gametypes.txt lists `coop`
//     among its 19 entries, and maps/mp/gametypes/coop.txt is the one-line
//     localized name "MPUI_COOPERATIVE".
//   * common_gametype_coop.ff -> mp/gametypescptable.csv carries a `coop` row.
//   * code_post_gfx_mp.ff -> mp/basemaps.arena gives every coop_* entry
//     gametype "coop", allieschar "us_army", axischar "opforce_composite".
//   * mp_playlists.info playlist 27 rotates
//         coop_prolog_mp_crash_intro,coop,1
//         coop_training_mp_killhouse_sh,coop,1
//
// Without this file GScr_LoadGameTypeScript cannot find maps/mp/gametypes/coop,
// and because codol_gametype_policy gives the coop category itself as its own
// fallback (src/scr_vm_main.c), the loader raises Com_Error(ERR_DROP) rather
// than silently compiling a multiplayer gametype against coop zones.  A coop
// match therefore does not start at all today; this is the file that starts it.
//
// SCOPE.  Boot chain and per-player basics: load, publish a gamestate, place a
// player at the level's own Spec Ops start, arm him from mp/cocoopclasstable.csv,
// keep him alive, and compile ZERO multiplayer scripts.  The Spec Ops mission
// layer -- objectives, challenge stars, the countdown/challenge HUD, actors,
// vehicles, breaching, downed/bleed-out/revive -- is the follow-on workstream
// and is listed file-by-file in docs/CODOL_CATEGORY_SCRIPT_SETS_20260917.md.

main()
{
	maps\mp\gametypes\_callbacksetup::SetupCallbacks();
	maps\mp\gametypes\_coopmode::init();
}
