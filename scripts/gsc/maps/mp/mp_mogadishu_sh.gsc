// PROVENANCE (2026-11-13): mp_mogadishu_sh is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW5 mp_mogadishu (compiled ScriptFile, mp_mogadishu.ff; rebuilt from its string/const table), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_mogadishu_sh_fx.gsc and maps/createfx/mp_mogadishu_sh_fx.gsc (3.19.26.22) are used as they ship.
// Precache: maps/mp/mp_mogadishu_sh_precache.gsc names the modules the zone's MapEnts give in precache_script keys.
// Adaptations:
//   * ambientPlay( "ambient_mp_mogadishu" ) left out: the zone has no such alias
//   * compass: compass_map_mp_mogadishu_sh (the zone's)
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_mogadishu_sh_precache::main();
	maps\createart\mp_mogadishu_sh_art::main();
	maps\mp\mp_mogadishu_sh_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_mogadishu_sh" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	if ( level.gametype == "oic" || level.gametype == "gun" || level.gametype == "infect" )
	{
		turrets = getEntArray( "misc_turret", "classname" );
		foreach ( turret in turrets )
			turret delete();
	}
}
