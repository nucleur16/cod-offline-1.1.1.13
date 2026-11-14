main()
{

	level._effect["dust_wind_spiral"]					= loadfx ("dust/dust_spiral_runner");	
	level._effect["dust_wind_fast"]				= loadfx ("dust/dust_wind_fast");
	level._effect["dust_wind_slow"]				= loadfx ("dust/dust_wind_slow_yel");
	level._effect["hawk"]				= loadfx ("weather/hawk");
	level._effect["hallway_smoke_light"]				= loadfx ("smoke/hallway_smoke_light");
	
/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_showdown_fx::main();
#/		
}
