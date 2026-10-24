// zm_prototype_sh level script.
//
// GScr_LoadScripts asks for exactly one level module per match, maps/<mapname>
// for a zm match (CODOL_ScriptEntryPoints, src/codol/script/codol_script_source.c;
// T5 keeps its zombie level scripts in maps/ too).  Without this file the server
// prints "no level script for zm_prototype_sh" and the map runs with no per-map
// setup at all.
//
// mp/basemaps.arena gives zm_prototype_sh gametype "zm", the same as zm_zombie_training,
// so the zm rules script plus this file is the whole per-map contract.
//
// MEASURED: zm_prototype_sh.ff ships sun/zm_prototype_sh.sun and vision/zm_prototype_sh.vision, where
// zm_zombie_training.ff ships neither.  That is why the vision name is passed
// here and omitted there.
//
// DELIBERATELY NOT HERE, and why:
//
//  * maps/mp/_load::main() -- the multiplayer level loader; naming it compiles
//    the whole multiplayer chain (100 scripts) into a zm match.
//  * maps/mp/zm_prototype_sh_fx::main() and maps/createfx/zm_prototype_sh_fx.gsc -- the map's
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
	maps\_zombiemode::levelMain( "zm_prototype_sh" );
	maps\_zombiemode_barrels::main();
	hideNavigationArrows();
	level thread prototype_fog();
	level thread prototype_fog_fx();
}

// --------------------------------------------------------------------------
// Fog.  T5 maps\createart\zombie_cod5_prototype_art.gsc fog_settings sets
// height fog (setVolFog) that, at the ground floor, comes to start 220 /
// halfway 731.5, neutral grey 0.501961.  That walled the map in at ~1000
// units, and the grey read blue, so neither is used any more:
//
// Distance.  C1's halfway is measured past start (zm_sumpf_sh uses a halfway
// shorter than its start).  MEASURED from the zone MapEnts: the bunker spans
// x -270..1110, y -950..1180; the 691 path nodes and the zombie spawners
// reach x -1600..2160, y -2000..2550, i.e. 1500-2500 units from the bunker;
// the zone's own createfx fog banks and skybox props sit 2000-5800 out.
// start 800, halfway 2400, max opacity 0.7: fog is 0 inside the bunker,
// ~20% at the zombie spawners (2000), 35% at 3200 and tops out near 55% at
// the far skybox, so the outskirts and the horizon stay visible.  The
// volumetric look comes from the placed fog effects below instead.
//
// Colour.  vision/zm_prototype_sh.vision (MEASURED, zone rawfile) grades
// after the fog is drawn.  Its film block is written in IW5's order
// (enable 520, contrast 522, brightness 521, desaturation 523 -0.506,
// desaturationDark 524 -0.251, invert 525, then the light 526, medium 527 and
// dark 528 tints), so 527 "0.931 1.1244 1.46827" is the MEDIUM tint -- the
// one a mid-grey fog sits on -- and it multiplies blue by 1.47.  Negative
// desaturation is extra saturation (x1.4 at this luminance) applied before
// the tint.  Modelled per pixel (Rec.709 luminance; tint lerped dark ->
// medium -> light by luminance; saturation lerped desaturationDark ->
// desaturation), T5's 0.502 grey comes out 0.47 0.56 0.74: blue.  The fog
// colour is that model solved backwards for a neutral 0.60 grey after
// grading -- roughly target / tint per channel, with the warm offset shrunk
// by the saturation boost: 0.616 0.539 0.452.  (Under the other reading of
// the tint ids, 527 dark / 528 medium, the same colour grades to a faintly
// warm 0.61 0.58 0.50 rather than blue.)
// zm maps do not run maps\mp\_load, whose codolFogDvar sends C1's r_fog
// (dvar 1464); a previous map may have turned it off, so it is sent here.
// --------------------------------------------------------------------------

prototype_fog()
{
	level thread prototype_fog_dvar();

	setExpFog( 800, 2400, 0.616, 0.539, 0.452, 0.7, 0 );
}

prototype_fog_dvar()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player setClientDvar( "1464", 1 );
	}
}

// --------------------------------------------------------------------------
// Placed volumetric fog.  The thinner global fog above leaves the outskirts
// clear, so rolling ground fog is placed around them instead.
//
// Effects: MEASURED in zm_prototype_sh.ff's FX asset table (real fx assets,
// not just createfx strings; each is a looping effect: msecLoopingLife
// 0x7fffffff, 1-2 looping elements, no one-shot elements), loaded by the
// zone's own maps/mp/zm_prototype_sh_fx.gsc as fog_thick, fog_thick02 and
// fog_thick03.  Being looping, spawnFx + triggerFx keeps them playing (as
// zm_zombie_training.gsc does with its landing-zone lights).
//   ground  maps/zombie/fx_fog_zombie_thick_600x600 -- T5's zombie ground fog
//   light   maps/zombie/codo_fx_fog_zombie_thick_600x600_01 -- the zone's
//           single-element variant, which its createfx puts just south of
//           the bunker
//   bank    maps/zombie/codo_fx_fog_zombie_thick_600x600_03 -- the variant
//           the zone's createfx uses for its distant banks
// The client already draws the zone createfx's own fog (fog_thick03 banks
// 2000-5500 units west / south-west, a few fog_thick01 one-shots near the
// bunker); these placements fill the rest of the ring and do not repeat it.
//
// Placement: MEASURED from the zone MapEnts path nodes (bunker centre about
// ( 420, 115 ); bunker x -270..1110, y -950..1180).  In 30-degree wedges
// around that centre, 250+ units outside the bunker:
//   ground  the outermost path node of each wedge (where zombies approach
//           from), at the node's floor height;
//   light   a node about a third of the way out, in alternate wedges;
//   bank    600 units beyond the outermost node, north / east / south-east
//           only (west and south-west already have the zone's banks).
// Nothing is placed inside the bunker.
// --------------------------------------------------------------------------

prototype_fog_precache()
{
	level._effect[ "proto_fog_ground" ] = loadfx( "maps/zombie/fx_fog_zombie_thick_600x600" );
	level._effect[ "proto_fog_light" ] = loadfx( "maps/zombie/codo_fx_fog_zombie_thick_600x600_01" );
	level._effect[ "proto_fog_bank" ] = loadfx( "maps/zombie/codo_fx_fog_zombie_thick_600x600_03" );
}

prototype_fog_fx()
{
	wait( 0.05 );

	// Outermost path node per wedge (angle from the bunker centre).
	prototype_fog_spot( "proto_fog_ground", ( 2159, 13, 52 ) );		//   0, east field
	prototype_fog_spot( "proto_fog_ground", ( 1799, 563, 36 ) );		//  30
	prototype_fog_spot( "proto_fog_ground", ( 915, 1777, 19 ) );		//  60
	prototype_fog_spot( "proto_fog_ground", ( -96, 2551, 28 ) );		//  90, north spawners
	prototype_fog_spot( "proto_fog_ground", ( -1409, 992, 54 ) );		// 150
	prototype_fog_spot( "proto_fog_ground", ( -1601, -230, 21 ) );		// 180, west spawners
	prototype_fog_spot( "proto_fog_ground", ( -1252, -1212, 33 ) );		// 210
	prototype_fog_spot( "proto_fog_ground", ( -949, -1363, 56 ) );		// 240
	prototype_fog_spot( "proto_fog_ground", ( 733, -2000, 27 ) );		// 270, south spawners
	prototype_fog_spot( "proto_fog_ground", ( 2131, -1767, 49 ) );		// 300, south-east spawners
	prototype_fog_spot( "proto_fog_ground", ( 2091, -967, 33 ) );		// 330

	// Lighter patches between the bunker and the outer ring.
	prototype_fog_spot( "proto_fog_light", ( 1433, 642, 29 ) );		//  30
	prototype_fog_spot( "proto_fog_light", ( 786, 1868, 29 ) );		//  90
	prototype_fog_spot( "proto_fog_light", ( -947, 506, 36 ) );		// 150
	prototype_fog_spot( "proto_fog_light", ( -1008, -448, 25 ) );		// 210
	prototype_fog_spot( "proto_fog_light", ( 507, -1563, 24 ) );		// 270
	prototype_fog_spot( "proto_fog_light", ( 1760, -681, 36 ) );		// 330

	// Banks past the edge of the playable ground.
	prototype_fog_spot( "proto_fog_bank", ( 2758, -22, 52 ) );		//   0
	prototype_fog_spot( "proto_fog_bank", ( 1086, 2352, 19 ) );		//  60
	prototype_fog_spot( "proto_fog_bank", ( -220, 3138, 28 ) );		//  90
	prototype_fog_spot( "proto_fog_bank", ( 2535, -2211, 49 ) );		// 300
	prototype_fog_spot( "proto_fog_bank", ( 2594, -1293, 33 ) );		// 330
}

// The zone createfx places its fog with angles ( 270, 0, 0 ): forward up.
prototype_fog_spot( name, origin )
{
	if ( !isDefined( level._effect[ name ] ) )
		return;
	angles = ( 270, 0, 0 );
	fx = spawnFx( level._effect[ name ], origin, anglesToForward( angles ), anglesToUp( angles ) );
	triggerFx( fx );
}

// The yellow floor arrows.  MEASURED from the zone MapEnts
// (alpha0022 resources/map_spawns.proposed.json, zm_prototype_sh): 14
// script_brushmodels, script_noteworthy "navigation_arrows", under 13
// arrows_to_* targetnames (brush models *12..*25; arrows_to_windows has
// two).  They are the zombie-training tutorial's objective pointers, shared
// with zm_zombie_training's layout; nothing on this map drives them, so they
// are hidden for the whole match (as zm_zombie_training.gsc
// training_setup() does with its own set), not deleted.
hideNavigationArrows()
{
	// getEntArray answers only the engine's own keys (targetname, classname,
	// ...); a script_noteworthy query returns undefined, so the arrows are
	// found by their targetnames.
	names = strTok( "arrows_to_first arrows_to_second arrows_to_third arrows_to_fourth arrows_to_fifth arrows_to_windows arrows_to_stairs arrows_to_power arrows_to_door arrows_to_box arrows_to_speedcola arrows_to_m14 arrows_to_m16", " " );
	hidden = 0;
	for ( i = 0; i < names.size; i++ )
	{
		arrows = getEntArray( names[ i ], "targetname" );
		if ( !isDefined( arrows ) )
			continue;
		for ( j = 0; j < arrows.size; j++ )
		{
			arrows[ j ] notSolid();
			arrows[ j ] hide();
			hidden++;
		}
	}
	codolprintln( "CoDOL zm_prototype_sh: " + hidden + " navigation arrows hidden" );
}
