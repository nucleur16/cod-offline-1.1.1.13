// PROVENANCE (2026-10-14): CoDOL mp_ambush_sh (3.3.3.3 map ported to 1.1.1.13). Same role as
// IW4 maps/mp/<map>_precache.gsc. The destructible anim module is the one
// animlists/mp_ambush_sh.animlist lists under =destructibles (me_fanceil1_spin,
// me_fanceil1_spin_stop; the toy_ceiling_fan entity names it in precache_script); the
// animated_models calls are the animated_model entities' precache_script keys.
main()
{
	common_scripts\_destructible_types_anim_me_fanceil1_spin::main();
	maps\animated_models\foliage_tree_river_birch_xl_a::main();
	maps\animated_models\hanging_apron_wind_medium::main();
	maps\animated_models\hanging_longsleeve_wind_medium::main();
	maps\animated_models\hanging_sheet_wind_medium::main();
	maps\animated_models\hanging_shortsleeve_wind_medium::main();
}
