// PROVENANCE (2026-10-14): CoDOL mp_cha_quad (3.3.3.3 map ported to 1.1.1.13). Same role as
// IW4 maps/mp/<map>_precache.gsc: one call per animated_models script the map's
// animated_model entities name in their precache_script keys. animlists/mp_cha_quad.animlist
// has no =destructibles section and the map places no destructibles, so no
// _destructible_types_anim_* module is called.
main()
{
	maps\animated_models\mp_quad_wind_lamp_brass::main();
	maps\animated_models\mp_quad_windy_cargo_net::main();
	maps\animated_models\mp_quad_windy_olive_tree::main();
	maps\animated_models\mp_quad_windy_pine_tall::main();
	maps\animated_models\mp_quad_windy_rope_bridge::main();
	maps\animated_models\mp_quad_windy_rope_swing::main();
	maps\animated_models\mp_quad_windy_umbrella_tree::main();
}
