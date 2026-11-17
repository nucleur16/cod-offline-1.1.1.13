// PROVENANCE (2026-10-14): IW4 maps/mp/mp_terminal_precache.gsc for CoDOL mp_terminal_cls
// (3.3.3.3 map ported to 1.1.1.13; IW5 ships mp_terminal_cls but not its precache list).
// The destructible anim modules are exactly the ones animlists/mp_terminal_cls.animlist
// lists under =destructibles (ex_airconditioner_fan, light_fluorescent_*, locker_*,
// wall_fan_*); the two animated_models are the entities' precache_script keys
// (windsock_wind_medium, roofvent_rotate under =animated_props). IW4's
// foliage_cod5_tree_pine05_large and foliage_pacific_fern01 are dropped: mp_terminal_cls
// places neither and its animlist lists neither anim.
main()
{
	common_scripts\_destructible_types_anim_airconditioner::main();
	common_scripts\_destructible_types_anim_light_fluo_on::main();
	common_scripts\_destructible_types_anim_lockers::main();
	common_scripts\_destructible_types_anim_wallfan::main();
	maps\animated_models\accessories_windsock_wind_medium::main();
	maps\animated_models\com_roofvent2::main();
}
