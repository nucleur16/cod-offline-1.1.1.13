// aitype\enemy_desert_elite_shgn_so -- the coop actor for every actor_enemy_desert_elite_shgn_so spawner
// (any spelling; the addons vary the case).  IW4 SP's aitype\enemy_desert_elite_shgn,
// in this server's coop actor contract.
//
// Loaded and precached by the engine at level load (codol_actor_scripts.cpp,
// AuthoredSpecies), main() run by spawnactor on a spawner of this classname.
// Weapons are the coop role's (aitype\enemy_coop_soldier::role_weapons); health
// and accuracy are the values the coop missions have used since 2026-10-07.
// body_shadow_co_shotgun is not in every shadow company zone; the smg
// body stands in where it is absent, as the coop soldier always did.

/*QUAKED actor_enemy_desert_elite_shgn_so (1.0 0.25 0.0) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="body_shadow_co_shotgun"
"count" -- max AI to ever spawn from this spawner
SPAWNER -- makes this a spawner instead of a guy
*/
main()
{
	self.animTree = "generic_human.atr";
	self.additionalAssets = "";
	self.team = "axis";
	self.type = "soldier";
	self.subclass = "regular";
	self.accuracy = 0.35;
	self.health = 200;
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeWeapon = "";
	self.grenadeAmmo = 0;

	aitype\enemy_coop_soldier::soldier_setup( "shgn" );

	if ( aitype\enemy_coop_soldier::scripted_character() )
		return;

	if ( codescripts\character::modelMounted( "body_shadow_co_shotgun" ) )
		character\character_shadow_co_shotgun::main();
	else
		character\character_shadow_co_smg::main();
}

spawner()
{
	self setspawnerteam( "axis" );
}

precache()
{
	character\character_shadow_co_shotgun::precache();
	character\character_shadow_co_smg::precache();
}
