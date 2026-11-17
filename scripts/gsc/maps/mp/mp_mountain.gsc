// PROVENANCE (2026-11-13): mp_mountain is CODOL 3.19.26.22's port of Black Ops (T5) Summit, converted to 1.1.1.13
// by CODOL-AssetExplorer port-map. Level script: T5's maps/mp/mp_mountain.gsc (COD-GSC-Source/T5-STEAM-GSC),
// translated to the 1.1.1.13 (IW4) script set; the gondola sway and the glass exploders are kept.
// Adaptations:
//   * fx: the zone's own maps/mp/mp_mountain_fx.gsc / maps/createfx/mp_mountain_fx.gsc (3.19.26.22).
//   * precache: maps/mp/mp_mountain_precache.gsc (IW4 convention; T5 precaches through its CSC).
//   * art: maps/createart/mp_mountain_art.gsc, T5's setVolFog values as an IW4 setExpFog, plus its light grid dvars.
//   * left out: devgui (/# #/), mp_mountain_amb (T5 _ambientpackage), _teamset_winterspecops,
//     _spawning::level_use_unified_spawning and scr_spawn_enemy_influencer_radius (T5 spawning), the
//     war callsign strings (MPUI_CALLSIGN_MAPNAME_*, unused by this tree's war), and spawncollision() of the
//     collision_* models (not in the zone; no such builtin here).
//   * #"xblive_wagermatch" -> plain dvar name; compass_map_mp_mountain is the zone's.
//   * the gondola swing sounds (amb_gondola_swing[_back]) are not in the zone: the gondola sways silently.
//   * glass_shatter_on_spawn structs: none in the zone's MapEnts, so that loop finds nothing.
#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_mountain_precache::main();
	maps\createart\mp_mountain_art::main();

	//needs to be first for create fx
	maps\mp\mp_mountain_fx::main();

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_mountain" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	level thread gondola_sway();
	level thread glass_exploder_init();

	glasses = getstructarray( "glass_shatter_on_spawn", "targetname" );
	for ( i = 0; i < glasses.size; i++ )
	{
		radiusdamage( glasses[ i ].origin, 64, 101, 100 );
	}
}

//Picks randomly from 2 sway strengths
gondola_sway()
{
	level endon( "gondola_triggered" );
	gondola_cab = getent( "gondola_cab", "targetname" );
	if ( !isDefined( gondola_cab ) )
		return;

	while ( 1 )
	{
		randomSwingAngle = randomfloatrange( 2, 5 );
		randomSwingTime = randomfloatrange( 2, 3 );
		gondola_cab rotateto( ( randomSwingAngle * 0.5, ( randomSwingAngle * 0.6 ) + 90, randomSwingAngle * .8 ), randomSwingTime, randomSwingTime * 0.3, randomSwingTime * 0.3 );
		wait( randomSwingTime );
		gondola_cab rotateto( ( ( randomSwingAngle * 0.5 ) * -1, ( randomSwingAngle * -1 * 0.6 ) + 90, randomSwingAngle * .8 * -1 ), randomSwingTime, randomSwingTime * 0.3, randomSwingTime * 0.3 );
		wait( randomSwingTime );
	}
}

glass_exploder_init()
{
	single_exploders = [];

	if ( !isDefined( level.createFXent ) )
		return;

	for ( i = 0; i < level.createFXent.size; i++ )
	{
		ent = level.createFXent[ i ];

		if ( !isDefined( ent ) )
			continue;

		if ( ent.v[ "type" ] != "exploder" )
			continue;

		if ( ent.v[ "exploder" ] == 201 || ent.v[ "exploder" ] == 202 )
		{
			ent thread glass_group_exploder_think();
		}
		else if ( ent.v[ "exploder" ] >= 101 && ent.v[ "exploder" ] <= 106 )
		{
			single_exploders[ single_exploders.size ] = ent;
		}
		else if ( ent.v[ "exploder" ] == 301 || ent.v[ "exploder" ] == 302 )
		{
			single_exploders[ single_exploders.size ] = ent;
		}
	}

	level thread glass_exploder_think( single_exploders );
}

glass_group_exploder_think()
{
	thresholdSq = 160 * 160;
	count = 0;

	for ( ;; )
	{
		level waittill( "glass_smash", origin );

		if ( distancesquared( self.v[ "origin" ], origin ) < thresholdSq )
		{
			count++;
		}

		if ( count >= 3 )
		{
			exploder( self.v[ "exploder" ] );
			return;
		}
	}
}

glass_exploder_think( exploders )
{
	thresholdSq = 160 * 160;

	if ( exploders.size <= 0 )
	{
		return;
	}

	for ( ;; )
	{
		closest = 999 * 999;
		closest_exploder = undefined;

		level waittill( "glass_smash", origin );

		for ( i = 0; i < exploders.size; i++ )
		{
			if ( !isDefined( exploders[ i ] ) )
				continue;

			if ( isDefined( exploders[ i ].glass_broken ) )
				continue;

			distSq = distancesquared( exploders[ i ].v[ "origin" ], origin );

			if ( distSq > thresholdSq )
				continue;

			if ( distSq < closest )
			{
				closest_exploder = exploders[ i ];
				closest = distSq;
			}
		}

		if ( isDefined( closest_exploder ) )
		{
			closest_exploder.glass_broken = true;
			exploder( closest_exploder.v[ "exploder" ] );
		}
	}
}
