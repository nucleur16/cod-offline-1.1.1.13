// zm -- the Zombie Mode rules script.
//
// This is the script name C1 contracts for: mp_playlists.ff's
// playlists_allmaps.info carries `gametype zm` / `script zm` with no
// `teambased` line, mp/gametypescptable.csv lists zm in its non-team half,
// maps/mp/gametypes/_gametypes.txt (out of code_post_gfx_mp.ff) includes zm,
// and mp/basemaps.arena gives every zm_* map `gametype "zm"`.  See
// docs/CODOL_ZOMBIES_STARTUP_20260915.md section 3.1.
//
// Before this file existed, GScr_LoadGameTypeScript could not find
// maps/mp/gametypes/zm, printed one error line, and loaded maps/mp/gametypes/dm
// instead -- so `g_gametype zm` ran IW4 free-for-all deathmatch on a zombies
// map while the serverinfo still said zm.  That fallback is now refused for a
// non-mp asset category (src/scr_vm_main.c, codol_gametype_policy).
//
// SCOPE.  This is the boot chain only: load, publish a gamestate, accept a
// player, do not fatally error, and do not compile or precache anything
// multiplayer.  Rounds, the points economy, barricades, the power switch, the
// mystery box, perk machines, power-ups, revives and the zombie actors
// themselves are the follow-on workstream (section 5/6 of the same document).

main()
{
	maps\mp\gametypes\_callbacksetup::SetupCallbacks();
	maps\mp\gametypes\_zombiemode::init();
}
