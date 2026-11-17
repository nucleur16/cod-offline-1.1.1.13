// aitype\ally_hero_ghost_socom_so -- the coop actor for every actor_ally_hero_ghost_socom_so spawner
// (any spelling; the addons vary the case).  IW4 SP's aitype\ally_hero_ghost_socom,
// in this server's coop actor contract.
//
// Loaded and precached by the engine at level load (codol_actor_scripts.cpp,
// AuthoredSpecies), main() run by spawnactor on a spawner of this classname.
// Weapons are the coop role's (aitype\enemy_coop_soldier::role_weapons); health
// and accuracy are the values the coop missions have used since 2026-10-07.

/*QUAKED actor_ally_hero_ghost_socom_so (0.0 0.25 1.0) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="body_hero_ghost_socom"
"count" -- max AI to ever spawn from this spawner
SPAWNER -- makes this a spawner instead of a guy
*/
main()
{
	self.animTree = "generic_human.atr";
	self.additionalAssets = "";
	self.team = "allies";
	self.type = "soldier";
	self.subclass = "regular";
	self.accuracy = 0.2;
	self.health = 100;
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeWeapon = "";
	self.grenadeAmmo = 0;

	aitype\enemy_coop_soldier::soldier_setup( "ar" );

	if ( aitype\enemy_coop_soldier::scripted_character() )
		return;

	character\character_hero_seal_soccom_ghost::main();
}

spawner()
{
	self setspawnerteam( "allies" );
}

precache()
{
	character\character_hero_seal_soccom_ghost::precache();
}
