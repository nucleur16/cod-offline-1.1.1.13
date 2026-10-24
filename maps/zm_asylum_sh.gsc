// zm_asylum_sh level script.
//
// GScr_LoadScripts asks for exactly one level module per match, maps/<mapname>
// for a zm match (CODOL_ScriptEntryPoints, src/codol/script/codol_script_source.c;
// T5 keeps its zombie level scripts in maps/ too).  Without this file the server
// prints "no level script for zm_asylum_sh" and the map runs with no per-map
// setup at all.
//
// mp/basemaps.arena gives zm_asylum_sh gametype "zm", the same as zm_zombie_training,
// so the zm rules script plus this file is the whole per-map contract.
//
// MEASURED: zm_asylum_sh.ff ships sun/zm_asylum_sh.sun and vision/zm_asylum_sh.vision, where
// zm_zombie_training.ff ships neither.  That is why the vision name is passed
// here and omitted there.
//
// DELIBERATELY NOT HERE, and why:
//
//  * maps/mp/_load::main() -- the multiplayer level loader; naming it compiles
//    the whole multiplayer chain (100 scripts) into a zm match.
//  * maps/mp/zm_asylum_sh_fx::main() and maps/createfx/zm_asylum_sh_fx.gsc -- the map's
//    ambient effects (fog banks, fires, light glows, sparks, loop and interval
//    sounds) are drawn by C1 itself: on every map load the client reads both
//    rawfiles out of the map zone with its own parser and plays them locally
//    (C1.exe 0x42df70; see zm_zombie_training.gsc and
//    docs/CODOL_CLIENT_AMBIENT_EMITTERS_20260915.md).  The zone's _fx.gsc runs
//    the createfx module only inside a /# #/ developer block, so the vanilla
//    server never placed them; running it here would draw everything twice.
//  * maps/mp/_compass::setupMiniMap -- _compass is a multiplayer module, and the
//    arena entry for every zm map carries mapimage "blank".

main()
{
	maps\_zombiemode::levelMain( "zm_asylum_sh" );
	level.zm_spawn_point_func = ::asylum_spawn_point;
}

// Asylum's MP start classnames are both in west2_downstairs_zone; the regular
// mp_tdm_spawn is in west_downstairs_zone beside the Juggernog room. Choose
// between the two authored rooms, rather than treating team names as sides.
// Keep a player's assigned room for subsequent spawns. Zone occupancy opens
// its enemy group in _zombiemode_zone_manager::refresh_spawners.
asylum_spawn_point()
{
	if ( isDefined( self.zm_asylum_start_point ) )
		return self.zm_asylum_start_point;

	names = [];
	names[ 0 ] = "mp_tdm_spawn_allies_start";
	names[ 1 ] = "mp_tdm_spawn_axis_start";
	names[ 2 ] = "mp_tdm_spawn";
	names[ 3 ] = "mp_dm_spawn";
	west = [];
	west2 = [];
	for ( i = 0; i < names.size; i++ )
	{
		points = getEntArray( names[ i ], "classname" );
		for ( j = 0; j < points.size; j++ )
		{
			zone = maps\_zombiemode_zone_manager::zone_at_point( points[ j ].origin );
			if ( !isDefined( zone ) )
				continue;
			if ( zone.name == "west_downstairs_zone" )
				west[ west.size ] = points[ j ];
			else if ( zone.name == "west2_downstairs_zone" )
				west2[ west2.size ] = points[ j ];
		}
	}
	sides = [];
	if ( west.size > 0 )
		sides[ sides.size ] = west;
	if ( west2.size > 0 )
		sides[ sides.size ] = west2;
	if ( sides.size < 1 )
		return undefined;

	points = sides[ randomInt( sides.size ) ];
	self.zm_asylum_start_point = points[ randomInt( points.size ) ];
	return self.zm_asylum_start_point;
}
