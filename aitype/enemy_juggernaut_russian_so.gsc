// aitype\enemy_juggernaut_russian_so -- the coop actor for every actor_enemy_juggernaut_russian_so spawner
// (any spelling; the addons vary the case).  IW4 SP's aitype\enemy_juggernaut_russian,
// in this server's coop actor contract.
//
// Loaded and precached by the engine at level load (codol_actor_scripts.cpp,
// AuthoredSpecies), main() run by spawnactor on a spawner of this classname.
// Weapons are the coop role's (aitype\enemy_coop_soldier::role_weapons); health
// and accuracy are the values the coop missions have used since 2026-10-07.
// IW4 maps\_juggernaut gives the juggernaut 3600 health behind armour that
// soaks most of it; one number stands in for both here.

/*QUAKED actor_enemy_juggernaut_russian_so (1.0 0.25 0.0) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="body_complete_sp_juggernaut"
"count" -- max AI to ever spawn from this spawner
SPAWNER -- makes this a spawner instead of a guy
*/
main()
{
	self.animTree = "generic_human.atr";
	self.additionalAssets = "";
	self.team = "axis";
	self.type = "soldier";
	self.subclass = "juggernaut";
	self.accuracy = 0.25;
	self.health = 1500;
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeWeapon = "";
	self.grenadeAmmo = 0;

	aitype\enemy_coop_soldier::soldier_setup( "lmg" );

	if ( aitype\enemy_coop_soldier::scripted_character() )
		return;

	character\character_sp_juggernaut_russian::main();
}

spawner()
{
	self setspawnerteam( "axis" );
}

precache()
{
	character\character_sp_juggernaut_russian::precache();
}
