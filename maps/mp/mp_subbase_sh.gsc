// PROVENANCE (2026-10-14): CoDOL mp_subbase_sh is a 3.3.3.3 map (MW2 Sub Base) ported to
// 1.1.1.13 by CODOL-AssetExplorer (port-map). The zone ships maps/mp/mp_subbase_sh_fx.gsc and
// maps/createfx/mp_subbase_sh_fx.gsc (extracted verbatim here) but not its level script, so
// this is IW4 maps/mp/mp_subbase.gsc adapted:
//   * precache/art/fx modules: the mp_subbase_sh ones (precache = IW4's locker anims; art =
//     IW4's fog with the zone's vision/mp_subbase_sh.vision).
//   * compass: compass_map_mp_subbase_sh ships in the zone.
//   * ambient alias and dvars kept as IW4's.
main()
{
	maps\mp\mp_subbase_sh_precache::main();
	maps\createart\mp_subbase_sh_art::main();
	maps\mp\mp_subbase_sh_fx::main();
	// fog off (r_fog 0, sent by _load::codolFogDvar): even thinned, IW4 mp_subbase's snow fog lays a white film over
	// the light-grid-lit models
	level.codolFog = 0;
	maps\mp\_load::main();

	// turn the light grid tweaks off explicitly: another map's level script (e.g. mp_terminal_cls) may have left them on
	setdvar( "r_lightGridEnableTweaks", 0 );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_subbase_sh" );

	ambientPlay( "ambient_mp_snow" );

	game[ "defenders" ] = "axis";
	game[ "attackers" ] = "allies";

	// IW4 mp_subbase's r_specularcolorscale 2.9 and r_lightGridIntensity 2 (light grid tweaks on) are left out: they
	// were tuned for IW4's lighting, and 3.3.3.3's vision/mp_subbase_sh.vision already boosts this map's lighting
	// (primary light tweaks 1.5 / 2, light tint 1.21 1.17 1.06). Stacked, they washed the light-grid-lit models
	// (props, not the lightmapped world) out to white.
	setdvar( "compassmaxrange", "2500" );
}
