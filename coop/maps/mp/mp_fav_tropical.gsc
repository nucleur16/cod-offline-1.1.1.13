// mp_fav_tropical -- coop level script (base spelling).
//
// GScr_LoadScripts asks the engine for exactly one module, maps/mp/<mapname>,
// for every asset category (src/scr_vm_main.c), and the per-category source
// root is what makes that file coop's rather than multiplayer's
// (src/codol/script/codol_script_source.c).
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
//  * maps\mp\mp_fav_tropical_fx::main().  The effects module does exist, as a rawfile
//    inside the zone.  Whether Scr_ReadFile can reach a CoDOL zone rawfile is
//    unproven -- its fastfile fallback goes through the retained
//    DB_FindXAssetHeader path, not the CoDOL archive registry, and the only
//    rawfile category wired to the registry in Scr_ReadFile is animtrees/.  A
//    file::function call is a compile-time dependency, so being wrong about it
//    is a fatal load error, not a missing effect.
//
//  * maps\mp\_compass::setupMiniMap.  _compass is a multiplayer module.
//
// This is the BASE spelling.  It shadows the multiplayer level script of the
// same name for coop matches ONLY -- the shared maps/mp/mp_fav_tropical.gsc is
// untouched and is what an mp match still compiles.  It cannot know which of
// the coop playlists (coop_spec_mp_fav_tropical, coop_take_mp_fav_tropical) is running, so it names its own
// zone's vision, which the base zone always ships.

main()
{
	maps\mp\gametypes\_coopmode::levelMain( "mp_fav_tropical" );
}
