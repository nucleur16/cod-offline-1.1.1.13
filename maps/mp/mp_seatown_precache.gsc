// PROVENANCE (2026-10-14): CoDOL mp_seatown (3.3.3.3 map ported to 1.1.1.13). Same role as
// IW4 maps/mp/<map>_precache.gsc. The destructible anim modules are exactly the ones
// animlists/mp_seatown.animlist lists under =destructibles (me_fanceil1_spin*,
// chicken_cage_*, ex_airconditioner_fan) and the destructible_toy entities name in
// precache_script; the animated_models calls are the animated_model entities'
// precache_script keys.
main()
{
	common_scripts\_destructible_types_anim_airconditioner::main();
	common_scripts\_destructible_types_anim_chicken::main();
	common_scripts\_destructible_types_anim_me_fanceil1_spin::main();
	maps\animated_models\hanging_apron_wind_medium::main();
	maps\animated_models\hanging_longsleeve_wind_medium::main();
	maps\animated_models\hanging_sheet_wind_medium::main();
	maps\animated_models\hanging_shortsleeve_wind_medium::main();
	maps\animated_models\seatown_lrg_wiregrp_sway::main();
	maps\animated_models\seatown_mid01_wiregrp_sway::main();
	maps\animated_models\seatown_wire_flags1_sway::main();
	maps\animated_models\seatown_wire_flags2_sway::main();
}
