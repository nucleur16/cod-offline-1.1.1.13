// aitype\ally_us_army_wounded_so -- the coop actor for every actor_ally_us_army_wounded_so spawner
// (any spelling; the addons vary the case).  IW4 SP's aitype\ally_us_army_wounded,
// in this server's coop actor contract.
//
// Loaded and precached by the engine at level load (codol_actor_scripts.cpp,
// AuthoredSpecies), main() run by spawnactor on a spawner of this classname.
// Weapons are the coop role's (aitype\enemy_coop_soldier::role_weapons); health
// and accuracy are the values the coop missions have used since 2026-10-07.

/*QUAKED actor_ally_us_army_wounded_so (0.0 0.25 1.0) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="body_us_army_dmg_a_blk"
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

	bodies = [];
	bodies[ 0 ] = "body_us_army_dmg_a_blk";
	bodies[ 1 ] = "body_us_army_dmg_a_wht";
	bodies[ 2 ] = "body_us_army_dmg_b_blk";
	bodies[ 3 ] = "body_us_army_dmg_b_wht";
	bodies[ 4 ] = "body_us_army_dmg_c_blk";
	bodies[ 5 ] = "body_us_army_dmg_c_wht";
	switch ( codescripts\character::get_random_mounted_character( bodies ) )
	{
	case 0:
		character\character_us_army_dmg_a_blk::main();
		break;
	case 1:
		character\character_us_army_dmg_a_wht::main();
		break;
	case 2:
		character\character_us_army_dmg_b_blk::main();
		break;
	case 3:
		character\character_us_army_dmg_b_wht::main();
		break;
	case 4:
		character\character_us_army_dmg_c_blk::main();
		break;
	case 5:
		character\character_us_army_dmg_c_wht::main();
		break;
	}
}

spawner()
{
	self setspawnerteam( "allies" );
}

precache()
{
	character\character_us_army_dmg_a_blk::precache();
	character\character_us_army_dmg_a_wht::precache();
	character\character_us_army_dmg_b_blk::precache();
	character\character_us_army_dmg_b_wht::precache();
	character\character_us_army_dmg_c_blk::precache();
	character\character_us_army_dmg_c_wht::precache();
}
