#include maps\mp\_utility;
#include common_scripts\utility;

main()
{
	maps\mp\mp_complex_precache::main();
	maps\mp\mp_complex_fx::main();
	maps\createart\mp_complex_art::main();
	
	// _load initializes the supported whole-entity destructible handler.
	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_complex" );

	ambientPlay( "ambient_mp_complex" );

	// raise up planes to avoid them flying through buildings
	level.airstrikeHeightScale = 2;

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
	
	setdvar( "compassmaxrange", "1500" );
	
}
