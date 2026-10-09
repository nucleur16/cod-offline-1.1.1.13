// coop_spec_mp_fav_tropical -- coop level script (playlist spelling, canonical).
//
// GScr_LoadScripts asks the engine for exactly one level module per match:
// maps/<mapname> for a coop or zm match, maps/mp/<mapname> for multiplayer
// (CODOL_ScriptEntryPoints, src/codol/script/codol_script_source.c) -- the
// IW4 split between singleplayer and multiplayer level scripts
// (docs/CODOL_GSC_TREE_LAYOUT.md).
//
// WHICH SPELLING IS <mapname>.  A coop match has two names for the same level:
// the PLAYLIST name (coop_training_mp_killhouse_sh) and the BASE level name
// (mp_killhouse_sh).  C1 at 0x004CD0FE mounts both, and the client's own
// ClientLoadTime line confirms it live
// (alpha0022/dw-capture.log:2232 -- common_mp | common_gametype_coop |
// coop_training_mp_killhouse_sh | mp_killhouse_sh).  MEASURED evidence from the
// zones themselves says the PLAYLIST spelling is the one the engine's
// per-map naming uses, so it is canonical here:
//
//   * every coop zone ships maps/mp/<PLAYLIST>_fx.gsc and
//     maps/createfx/<PLAYLIST>_fx.gsc -- the same maps/mp/<mapname>_fx.gsc
//     shape that mp zones use for mp_killhouse_sh and zm zones use for
//     zm_zombie_training, where <mapname> is unambiguous;
//   * seven of the nine ship vision/<PLAYLIST>.vision, which is what
//     VisionSetNaked( getDvar("mapname") ) asks for;
//   * every one ships animlists/<PLAYLIST>.animlist;
//   * every one ships its addon entities as maps/mp/<PLAYLIST>.mapents.
//
// The base spellings are carried too, and do the identical work, so this root
// is correct whichever spelling the lobby ends up announcing.  No CoDOL zone
// ships a level script for any map in any category (a sweep of all 90 zones
// finds 55 .gsc rawfiles and every one is an fx pair), so the server owns this
// file the same way it owns maps/mp/mp_killhouse_sh.gsc.
//
// DELIBERATELY NOT HERE, and why:
//
//  * maps\mp\_load::main().  That is the multiplayer level loader, and naming
//    it compiles all 100 multiplayer scripts into a coop match (measured with
//    tools/codol_script_resolve.py).  Its per-map jobs that coop needs -- the
//    vision set and level.isSP -- are done by _coopmode::levelMain() instead.
//
//  * maps\mp\coop_spec_mp_fav_tropical_fx::main().  The effects module does exist, as a rawfile
//    inside the zone.  Whether Scr_ReadFile can reach a CoDOL zone rawfile is
//    unproven -- its fastfile fallback goes through the retained
//    DB_FindXAssetHeader path, not the CoDOL archive registry, and the only
//    rawfile category wired to the registry in Scr_ReadFile is animtrees/.  A
//    file::function call is a compile-time dependency, so being wrong about it
//    is a fatal load error, not a missing effect.
//
//  * maps\mp\_compass::setupMiniMap.  _compass is a multiplayer module.
//
//  * its own vision.  MEASURED: coop_spec_mp_fav_tropical.ff ships NO
//    vision rawfile at all, where the other coop zones ship
//    vision/<zone>.vision.  The base mp_fav_tropical zone is mounted
//    alongside it and ships vision/mp_fav_tropical.vision, so that is the
//    name asked for here.  Asking for a vision that is in no mounted zone
//    is the failure this avoids.

main()
{
	maps\_coopmode::levelMain( "mp_fav_tropical" );
}
