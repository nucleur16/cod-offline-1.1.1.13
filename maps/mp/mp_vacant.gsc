// PROVENANCE: IW4 mp_vacant.gsc. Since 2026-10-22 the map is CODOL 3.5.6.2's (build 3.5.2.6)
// DLC 255 Vacant, ported to 1.1.1.13 by CODOL-AssetExplorer port-map (its compass
// compass_map_mp_vacant_dlc ships in the zone; vision mp_vacant and ambient_mp_vacant in common_mp).
// 3.5.6.2's Vacant is a breach map: its 28 breach groups put a door (script_brushmodel
// "breach_solid", drawn by "breach_visuals") in the doorways IW4's Vacant leaves open, with frame
// charge spots, construct/breach triggers and "breach_only" props (doors, ladders) for its breach
// mode. 3.5.6.2's breach script is in no client zone and 1.1.1.13 has no breach mode or
// breach_detonator_mp, so every regular mode plays the IW4 layout: open_breaches() removes the
// doors and the breach-mode entities at level start.
main()
{
	maps\mp\mp_vacant_precache::main();
	maps\mp\mp_vacant_fx::main();
	maps\createart\mp_vacant_art::main();
	maps\mp\_load::main();

	open_breaches();
	
	maps\mp\_compass::setupMiniMap( "compass_map_mp_vacant_dlc" );
	
	AmbientPlay( "ambient_mp_vacant" );
	
	game["attackers"] = "axis";
	game["defenders"] = "allies";

	SetDvar( "r_specularcolorscale", "1" );
	SetDvar( "compassmaxrange", "1500" );
		
	setdvar( "r_lightGridEnableTweaks", 1 );
	setdvar( "r_lightGridIntensity", 1.0 );
	setdvar( "r_lightGridContrast", 1 );
}

open_breaches()
{
	names = [];
	names[ names.size ] = "breach_solid";
	names[ names.size ] = "breach_visuals";
	names[ names.size ] = "breach_only";
	names[ names.size ] = "breach_delete";
	names[ names.size ] = "breach_solid_delete";
	names[ names.size ] = "trigger_use_breach";
	foreach ( name in names )
	{
		foreach ( ent in getentarray( name, "targetname" ) )
			ent delete();
	}
	// The frame charge markers and the construct/reinforce triggers carry only a script_noteworthy,
	// which getentarray cannot match here: they are the zone's only weapon_claymore_detonator_objective
	// models and its only trigger_use_touch entities without a targetname.
	foreach ( ent in getentarray( "script_model", "classname" ) )
	{
		if ( ent.model == "weapon_claymore_detonator_objective" )
			ent delete();
	}
	foreach ( ent in getentarray( "trigger_use_touch", "classname" ) )
	{
		if ( !isdefined( ent.targetname ) )
			ent delete();
	}
}
