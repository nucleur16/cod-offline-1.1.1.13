// zm -- the Zombie Mode rules script.
//
// This is the script name C1 contracts for: mp_playlists.ff's
// playlists_allmaps.info carries `gametype zm` / `script zm` with no
// `teambased` line, mp/gametypescptable.csv lists zm in its non-team half,
// maps/mp/gametypes/_gametypes.txt (out of code_post_gfx_mp.ff) includes zm,
// and mp/basemaps.arena gives every zm_* map `gametype "zm"`.  See
// docs/CODOL_ZOMBIES_STARTUP_20260915.md section 3.1.
//
// Before this file existed (as maps/mp/gametypes/zm.gsc in the retired zm/
// overlay), GScr_LoadGameTypeScript could not find it, printed one error line,
// and loaded maps/mp/gametypes/dm instead -- so `g_gametype zm` ran IW4
// free-for-all deathmatch on a zombies map while the serverinfo still said zm.
// The zm category now loads exactly this rules module and refuses any
// multiplayer fallback (CODOL_ScriptEntryPoints,
// src/codol/script/codol_script_source.c).
//
// SCOPE.  This is the boot chain only: load, publish a gamestate, accept a
// player, do not fatally error, and do not compile or precache anything
// multiplayer.  Rounds, the points economy, barricades, the power switch, the
// mystery box, perk machines, power-ups, revives and the zombie actors
// themselves are the follow-on workstream (section 5/6 of the same document).

main()
{
	maps\_zombiemode_callbacksetup::SetupCallbacks();
	maps\_zombiemode::init();
}
