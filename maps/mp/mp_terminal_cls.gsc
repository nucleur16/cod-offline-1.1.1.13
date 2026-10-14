// PROVENANCE (2026-10-14): CoDOL mp_terminal_cls is a 3.3.3.3 map (MW3 Terminal) ported to
// 1.1.1.13 by CODOL-AssetExplorer (port-map). The zone ships maps/mp/mp_terminal_cls_fx.gsc
// and maps/createfx/mp_terminal_cls_fx.gsc (extracted verbatim here) but not its level
// script, so this is IW5 maps/mp/mp_terminal_cls.gsc (COD-GSC-Source IW5-GSC) adapted:
//   * precache module: maps/mp/mp_terminal_cls_precache.gsc (IW4 mp_terminal's list cut to
//     what this map's animlist and entities name).
//   * art: maps/createart/mp_terminal_cls_art.gsc carries IW5's terminal_cls fog as setExpFog.
//   * ambient: 3.3.3.3's ambient_mp_terminal_cls (common_mp, channel 28, which the porter carries
//     into the map zone). The zone's own ambient_mp_terminal_cls_ext is an emitter loop on channel 3,
//     which C1 refuses as an ambient (message 19, 0x739550). IW5 used ambient_mp_airport_dlc.
//   * IW5's audio_settings() (maps\mp\_audio::add_reverb) is dropped: this tree has no
//     maps/mp/_audio.gsc.
//   * compass: compass_map_mp_terminal_cls ships in the zone.
main()
{
	maps\mp\mp_terminal_cls_precache::main();
	maps\createart\mp_terminal_cls_art::main();
	maps\mp\mp_terminal_cls_fx::main();
	maps\mp\_explosive_barrels::main();
	maps\mp\_load::main();

	ambientPlay( "ambient_mp_terminal_cls" );

	maps\mp\_compass::setupMiniMap( "compass_map_mp_terminal_cls" );

	// IW4 maps/mp/mp_terminal.gsc's light grid tuning for this layout (IW5's 1.33 with no contrast lit the models too
	// brightly on the 3.3.3.3 map).
	setDvar( "r_lightGridEnableTweaks", 1 );
	setDvar( "r_lightGridIntensity", 1.22 );
	setDvar( "r_lightGridContrast", .6 );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
}
