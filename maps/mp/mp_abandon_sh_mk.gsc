// PROVENANCE (2026-10-14): mp_abandon_sh_mk is a 3.3.3.3 map (the amusement-park Abandon
// variant) ported to 1.1.1.13 by CODOL-AssetExplorer (port-map). The zone ships
// maps/mp/mp_abandon_sh_mk_fx.gsc and maps/createfx/mp_abandon_sh_mk_fx.gsc (extracted verbatim
// here) but not its level script, so this is mp_abandon.gsc adapted:
//   * fx/art modules: the mp_abandon_sh_mk ones.
//   * precache module: maps/mp/mp_abandon_sh_mk_precache.gsc registers the animated props
//     (maps/animated_models/mp_abandon_*.gsc, named by the entities' precache_script keys) and
//     the firecracker destructible anims; the toy types themselves are in
//     common_scripts/_destructible_types.gsc (IW5 arcade/pinball/clown-trashcan equivalents).
//   * the two UFOs share one model, so codol_pinUfoAnims() gives each its own loop.
//   * compass: compass_map_mp_abandon_sh_mk ships in the zone.
//   * ambient alias kept as mp_abandon's (host-recorded).
main()
{
	maps\mp\mp_abandon_sh_mk_precache::main();
	maps\createart\mp_abandon_sh_mk_art::main();
	maps\mp\mp_abandon_sh_mk_fx::main();

	codol_pinUfoAnims();

	maps\mp\_load::main();

	// turn the light grid tweaks off explicitly: another map's level script (e.g. mp_terminal_cls) may have left them on
	setdvar( "r_lightGridEnableTweaks", 0 );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_abandon_sh_mk" );

	ambientPlay( "ambient_mp_abandon" );

	// No r_lightGrid*/r_specularcolorscale overrides: IW4 mp_abandon's (lightGridIntensity 1.452, contrast 0,
	// specular 2.5) were tuned for IW4's own lightgrid bake and made player models far too bright on the 3.3.3.3
	// remake, whose lightgrid is baked brighter; the map's own baked lighting is used as it is.

	setdvar( "compassmaxrange", "1800" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}

// Both UFO props are abdn_ufo_anim, and _animatedmodels picks a random key of
// level.anim_prop_models[ model ] per entity, so both could fly the same loop.
// In 3.3.3.3 each entity names its own script (precache_script
// "maps animated_models mp_abandon_sh_mk_ufo1" / "..._ufo2"); _animatedmodels
// plays self.animation when it is set, so pin each UFO to its loop here. If the
// key is not readable at runtime, fall back to the placement: the ufo1 entity is
// the one with the smaller x (777 vs 1024).
codol_pinUfoAnims()
{
	ufos = [];
	animated_models = getentarray( "animated_model", "targetname" );
	foreach ( ent in animated_models )
	{
		if ( isdefined( ent.model ) && ent.model == "abdn_ufo_anim" )
			ufos[ ufos.size ] = ent;
	}

	foreach ( ufo in ufos )
	{
		key = undefined;
		if ( isdefined( ufo.precache_script ) )
		{
			if ( issubstr( ufo.precache_script, "ufo1" ) )
				key = "ufo1";
			else if ( issubstr( ufo.precache_script, "ufo2" ) )
				key = "ufo2";
		}
		if ( !isdefined( key ) )
		{
			key = "ufo1";
			foreach ( other in ufos )
			{
				if ( other != ufo && other.origin[ 0 ] < ufo.origin[ 0 ] )
					key = "ufo2";
			}
		}
		ufo.animation = level.anim_prop_models[ "abdn_ufo_anim" ][ key ];
		{}
	}
}
