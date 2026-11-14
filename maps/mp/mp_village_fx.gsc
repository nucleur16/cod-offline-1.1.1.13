main()
{
	level._effect[ "dust_wind_fast" ] = loadfx( "dust/dust_wind_fast" );
	level._effect[ "leaves_fall_gentlewind" ] = loadfx( "misc/leaves_fall_gentlewind" );
	level._effect[ "mp_village_tower_fire" ] = loadfx( "fire/mp_village_tower_fire" );
	level._effect[ "fx_debris_dust_motes" ] = loadfx( "maps/ber2/fx_debris_dust_motes" );
	level._effect[ "moth_runner" ] = loadfx( "misc/moth_runner" );
	level._effect[ "heli_crash_fire" ] = loadfx( "fire/heli_crash_fire" );
	level._effect[ "fire_falling_runner" ] = loadfx( "fire/fire_falling_runner" );
	level._effect[ "trash_spiral_runner" ] = loadfx( "misc/trash_spiral_runner" );
	level._effect[ "firelp_barrel_pm" ] = loadfx( "fire/firelp_barrel_pm" );
	level._effect[ "firelp_small_cheap_mp" ] = loadfx( "fire/firelp_small_cheap_mp" );
	level._effect[ "smoke_large" ] = loadfx( "smoke/smoke_large" );
	level._effect[ "thick_black_smoke_small" ] = loadfx( "smoke/thick_black_smoke_small" );
	level._effect[ "insects_carcass_flies" ] = loadfx( "misc/insects_carcass_flies" );
	level._effect[ "mp_sub_blow_trash_runner" ] = loadfx( "misc/mp_sub_blow_trash_runner" );
	level._effect[ "fx_grass_blowing_slow" ] = loadfx( "env/foliage/fx_grass_blowing_slow" );
	level._effect[ "dust_wind_medium" ] = loadfx( "dust/dust_wind_medium" );
//	precache_fxanim_props();
//	precache_scripted_fx();
//	precache_createfx_fx();
		
	// calls the createfx server script (i.e., the list of ambient effects and their attributes)
	/#
		if ( common_scripts\utility::shouldRunServerSideEffects() )
			maps\createfx\mp_village_fx::main();
	#/
	
//	maps\createart\mp_village_art::main();
}

// Scripted effects
//precache_scripted_fx()
//{
//	
//}

// Ambient effects
//precache_createfx_fx()
//{
	//level._effect["effect_alias"]	= loadFX("path/to/effect");
	
	//misc,dust,debris,sparks//
//	level._effect["fx_mp_light_dust_motes_md"]                           = loadfx("maps/mp_maps/fx_mp_light_dust_motes_md");
//	level._effect["fx_mp_express_train_blow_dust"] 							         = loadfx("maps/mp_maps/fx_mp_express_train_blow_dust");
//	level._effect["fx_mp_village_grass"] 							                   = loadfx("maps/mp_maps/fx_mp_village_grass");
//	level._effect["fx_mp_village_papers"] 							                 = loadfx("maps/mp_maps/fx_mp_village_papers");
//	level._effect["fx_mp_village_dust_sm"] 							                 = loadfx("maps/mp_maps/fx_mp_village_dust_sm");
//	level._effect["fx_mp_village_dust_ceiling"] 							           = loadfx("maps/mp_maps/fx_mp_village_dust_ceiling");
//	level._effect["fx_insects_swarm_lg_light"]                           = loadfx("bio/insects/fx_insects_swarm_lg_light");
//	level._effect["fx_mp_elec_spark_burst_md_runner"]                    = loadfx("maps/mp_maps/fx_mp_elec_spark_burst_md_runner");
//	level._effect["fx_mp_village_vista_dust"]                            = loadfx("maps/mp_maps/fx_mp_village_vista_dust");
//	
//	//fire//
//	level._effect["fx_mp_village_smoke_fire_lg"]						             = loadfx("maps/mp_maps/fx_mp_village_smoke_fire_lg");
//	level._effect["fx_mp_carrier_smoke_fire_sm"]						             = loadfx("maps/mp_maps/fx_mp_carrier_smoke_fire_sm");
//	level._effect["fx_mp_village_smoke_fire_med"]						             = loadfx("maps/mp_maps/fx_mp_village_smoke_fire_med");
//	level._effect["fx_mp_slums_fire_sm"] 							                   = loadfx("maps/mp_maps/fx_mp_slums_fire_sm");
//	level._effect["fx_mp_village_barrel_fire"] 							             = loadfx("maps/mp_maps/fx_mp_village_barrel_fire");
//
//	//smoke//
//	level._effect["fx_mp_slums_dark_smoke_sm"] 							             = loadfx("maps/mp_maps/fx_mp_slums_dark_smoke_sm");
//	level._effect["fx_mp_village_smoke_tower"]						               = loadfx("maps/mp_maps/fx_mp_village_smoke_tower");
//	level._effect["fx_mp_village_smoke_med"]						                 = loadfx("maps/mp_maps/fx_mp_village_smoke_med");
//	level._effect["fx_mp_village_smoke_xsm"]						                 = loadfx("maps/mp_maps/fx_mp_village_smoke_xsm");
//	level._effect["fx_mp_smoke_vista"]			             	               = loadfx("maps/mp_maps/fx_mp_smoke_vista");
//	level._effect["fx_smk_tin_hat_sm"]                                   = loadfx("smoke/fx_smk_tin_hat_sm");
//	
//	//light,god ray,glares//
//	level._effect["fx_window_god_ray"]			             	               = loadfx("light/fx_window_god_ray");
//	level._effect["fx_window_god_ray_village"]			             	       = loadfx("light/fx_window_god_ray_village");
//	level._effect["fx_window_god_ray_sm"]			             	             = loadfx("light/fx_window_god_ray_sm");
//	level._effect["fx_drone_rectangle_light_03"]			             	     = loadfx("light/fx_drone_rectangle_light_03"); 
//	level._effect["fx_village_tube_light"]			             	           = loadfx("light/fx_village_tube_light");
//	level._effect["fx_village_tube_light_sq"]			             	         = loadfx("light/fx_village_tube_light_sq"); 
//	level._effect["fx_village_rectangle_light_01"]			             	   = loadfx("light/fx_village_rectangle_light_01");
//	level._effect["fx_mp_village_single_glare"]						               = loadfx("maps/mp_maps/fx_mp_village_single_glare");
//	level._effect["fx_mp_village_hole_god_ray"]				                   = loadfx("maps/mp_maps/fx_mp_village_hole_god_ray");
//	
//	//water//
//	level._effect["fx_mp_village_statue_water"]				                   = loadfx("maps/mp_maps/fx_mp_village_statue_water");
//	
//	//sunflare//	
//	level._effect["fx_lf_mp_village_sun1"]			             	         	 = loadfx("lens_flares/fx_lf_mp_village_sun1");
	
//}


// FXanim Props

//precache_fxanim_props()
//{
//	level.scr_anim["fxanim_props"]["sparking_wires_med"] = %fxanim_gp_wirespark_med_anim;
//	level.scr_anim["fxanim_props"]["windsock"] = %fxanim_gp_windsock_anim;
//	level.scr_anim["fxanim_props"]["wire_coil_01"] = %fxanim_gp_wire_coil_01_anim;
//	level.scr_anim["fxanim_props"]["sign_sway_lrg"] = %fxanim_gp_sign_sway_lrg_anim;
//}
