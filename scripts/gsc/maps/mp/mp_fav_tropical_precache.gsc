// CoDOL adaptation of IW4 mp_favela_precache. Preserve vendor attribution.
// The shipped tropical map has no foliage_tree_palm_bushy_3 / bushy3 XAnims.
main()
{
	common_scripts\_destructible_types_anim_airconditioner::main();
	common_scripts\_destructible_types_anim_light_fluo_single::main();
	common_scripts\_destructible_types_anim_me_fanceil1_spin::main();
	if ( !isDefined( level.anim_prop_models ) )
		level.anim_prop_models = [];
	level.anim_prop_models["foliage_codo_short_palm_animated"]["sway"] = "foliage_codo_short_palm_1_sway";
	// IW4x registers this second authored palm; C1 ships its sway under the
	// tropical-estate name. Its three animation bones match the actual model.
	level.anim_prop_models["foliage_palm_tree_1_animated"]["sway"] = "foliage_tropical_estate_sway";
	maps\animated_models\hanging_apron_wind_medium::main();
	maps\animated_models\hanging_longsleeve_wind_medium::main();
	maps\animated_models\hanging_sheet_wind_medium::main();
	maps\animated_models\hanging_shortsleeve_wind_medium::main();
}
