// aitype\civilian_slum_male_so -- the coop actor for every actor_civilian_slum_male_so spawner
// (any spelling; the addons vary the case).  IW4 SP's aitype\civilian_slum_male,
// in this server's coop actor contract.
//
// Loaded and precached by the engine at level load (codol_actor_scripts.cpp,
// AuthoredSpecies), main() run by spawnactor on a spawner of this classname.
// Weapons are the coop role's (aitype\enemy_coop_soldier::role_weapons); health
// and accuracy are the values the coop missions have used since 2026-10-07.

/*QUAKED actor_civilian_slum_male_so (0.2 0.8 0.2) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="body_slum_civ_male_aa"
"count" -- max AI to ever spawn from this spawner
SPAWNER -- makes this a spawner instead of a guy
*/
main()
{
	self.animTree = "generic_human.atr";
	self.additionalAssets = "";
	self.team = "neutral";
	self.type = "soldier";
	self.subclass = "regular";
	self.accuracy = 0.2;
	self.health = 30;
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeWeapon = "";
	self.grenadeAmmo = 0;

	aitype\enemy_coop_soldier::soldier_setup( "pistol" );

	if ( aitype\enemy_coop_soldier::scripted_character() )
		return;

	bodies = [];
	bodies[ 0 ] = "body_slum_civ_male_aa";
	bodies[ 1 ] = "body_slum_civ_male_aa_wht";
	bodies[ 2 ] = "body_slum_civ_male_ab";
	bodies[ 3 ] = "body_slum_civ_male_ab_wht";
	bodies[ 4 ] = "body_slum_civ_male_ba";
	bodies[ 5 ] = "body_slum_civ_male_ba_wht";
	bodies[ 6 ] = "body_slum_civ_male_bb";
	bodies[ 7 ] = "body_slum_civ_male_bb_wht";
	switch ( codescripts\character::get_random_mounted_character( bodies ) )
	{
	case 0:
		character\character_civilian_slum_male_aa::main();
		break;
	case 1:
		character\character_civilian_slum_male_aa_wht::main();
		break;
	case 2:
		character\character_civilian_slum_male_ab::main();
		break;
	case 3:
		character\character_civilian_slum_male_ab_wht::main();
		break;
	case 4:
		character\character_civilian_slum_male_ba::main();
		break;
	case 5:
		character\character_civilian_slum_male_ba_wht::main();
		break;
	case 6:
		character\character_civilian_slum_male_bb::main();
		break;
	case 7:
		character\character_civilian_slum_male_bb_wht::main();
		break;
	}
}

spawner()
{
	self setspawnerteam( "neutral" );
}

precache()
{
	character\character_civilian_slum_male_aa::precache();
	character\character_civilian_slum_male_aa_wht::precache();
	character\character_civilian_slum_male_ab::precache();
	character\character_civilian_slum_male_ab_wht::precache();
	character\character_civilian_slum_male_ba::precache();
	character\character_civilian_slum_male_ba_wht::precache();
	character\character_civilian_slum_male_bb::precache();
	character\character_civilian_slum_male_bb_wht::precache();
}
