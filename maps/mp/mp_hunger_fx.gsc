// PROVENANCE (2026-10-22): mp_hunger_fx.gsc as CODOL 3.5.6.2 (build 3.5.2.6) ships it in
// ff_dlc23_mp_hunger, except that the createfx call is a developer block as in the other
// usermaps: 3.5.6.2 gates it with getdvarint( "1106" ) (C1's clientSideEffects id), which the
// server reads as 0, and the client runs the zone's own createfx copy anyway.
main()
{
	level._effect[ "test_effect" ]										 = loadfx( "misc/moth_runner" );
/#
	if ( common_scripts\utility::shouldRunServerSideEffects() )
		maps\createfx\mp_hunger_fx::main();
#/
}
