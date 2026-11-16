// PROVENANCE (2026-11-13): mp_liberation_sh is CODOL 3.19.26.22's port, converted to 1.1.1.13 by CODOL-AssetExplorer port-map.
// Level script: IW5 mp_park (COD-GSC-Source/IW5-GSC/scripts/maps/mp/mp_park.gsc), translated to the 1.1.1.13 script set.
// The zone's own maps/mp/mp_liberation_sh_fx.gsc and maps/createfx/mp_liberation_sh_fx.gsc (3.19.26.22) are used as they ship.
// Adaptations:
//   * ambientPlay( "ambient_mp_park" ) left out: the zone has no such alias
//   * audio_settings() (maps\mp\_audio::add_reverb "mountains") left out: this tree has no maps\mp\_audio
//   * compass: compass_map_mp_liberation_sh (the zone's)
//   * IW5's decompiled turret loop deletes only the last turret (a decompiler artefact: foreach(var_2 in var_0) {} var_2 delete();); every misc_turret is deleted here, which is what the mode check intends
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\createart\mp_liberation_sh_art::main();
	maps\mp\mp_liberation_sh_fx::main();
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_liberation_sh" );
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.33 );
	setdvar( "sm_sunShadowScale", "0.8" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	if ( level.gametype == "oic" || level.gametype == "gun" || level.gametype == "infect" )
	{
		turrets = getEntArray( "misc_turret", "classname" );
		foreach ( turret in turrets )
			turret delete();
	}

	thread killTrigger( ( 1750, -302, 122 ), 40, 15 );
	thread killTrigger( ( 1858, -350, 174 ), 60, 15 );
	thread killTrigger( ( 1806, -326, 160 ), 60, 15 );
	thread killTrigger( ( 1738, -358, 122 ), 40, 15 );
}
