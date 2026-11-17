// PROVENANCE (2026-10-14): IW4 maps/mp/mp_rust_long_precache.gsc (COD-GSC-Source IW4-GSC),
// unchanged in content. The CoDOL mp_rust_long entities (highrise_fencetarp_04_dlc_2 /
// _07_dlc_2) name these two scripts in precache_script and carry their animation in an
// "animation" key (mp_storm_fencetarp_04_windB / _07_windC), which _animatedmodels plays;
// the scripts PrecacheMpAnim those names, and animlists/mp_rust_long.animlist lists both
// under =animated_props.
main()
{
	maps\animated_models\highrise_fencetarp_07b_wind_c::main();
	maps\animated_models\highrise_fencetarp_04b_wind_b::main();
}
