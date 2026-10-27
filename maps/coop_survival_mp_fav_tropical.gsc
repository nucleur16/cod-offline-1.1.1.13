// coop_survival_mp_fav_tropical -- coop level script (playlist spelling).
//
// Spec Ops Survival on the favela (docs/CODOL_SURVIVAL_PLAN.md).  The engine
// runs the BASE spelling's level script, maps/mp_fav_tropical.gsc, for this
// session; this file mirrors it for a lobby that announces the playlist
// spelling, as the other coop playlists do (see coop_defense_mp_crash_tropical
// for why both exist).  The mode itself is maps\_coopsurvival, selected by
// codol_sessionMap.  The session zone (ported from CoDOL 2.6.9.7) ships no
// vision rawfile, so the base level's is named.

main()
{
	maps\mp\mp_fav_tropical_precache::main();
	maps\_coopmode::levelMain( "mp_fav_tropical" );
}
