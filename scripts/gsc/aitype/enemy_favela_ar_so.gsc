// aitype\enemy_favela_ar_so -- the coop actor for every actor_enemy_favela_ar_so spawner
// (any spelling; the addons vary the case).  IW4 SP's aitype\enemy_favela_ar,
// in this server's coop actor contract.
//
// Loaded and precached by the engine at level load (codol_actor_scripts.cpp,
// AuthoredSpecies), main() run by spawnactor on a spawner of this classname.
// Weapons are the coop role's (aitype\enemy_coop_soldier::role_weapons); health
// and accuracy are the values the coop missions have used since 2026-10-07.

/*QUAKED actor_enemy_favela_ar_so (1.0 0.25 0.0) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="body_militia_assault_aa_blk"
"count" -- max AI to ever spawn from this spawner
SPAWNER -- makes this a spawner instead of a guy
*/
main()
{
	self.animTree = "generic_human.atr";
	self.additionalAssets = "";
	self.team = "axis";
	self.type = "soldier";
	self.subclass = "militia";
	self.accuracy = 0.3;
	self.health = 150;
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeWeapon = "";
	self.grenadeAmmo = 0;

	aitype\enemy_coop_soldier::soldier_setup( "ar" );

	if ( aitype\enemy_coop_soldier::scripted_character() )
		return;

	bodies = [];
	bodies[ 0 ] = "body_militia_assault_aa_blk";
	bodies[ 1 ] = "body_militia_assault_aa_wht";
	bodies[ 2 ] = "body_militia_assault_ab_blk";
	bodies[ 3 ] = "body_militia_assault_ac_blk";
	bodies[ 4 ] = "body_militia_lmg_aa_blk";
	bodies[ 5 ] = "body_militia_lmg_ab_blk";
	bodies[ 6 ] = "body_militia_lmg_ac_blk";
	bodies[ 7 ] = "body_militia_smg_aa_blk";
	bodies[ 8 ] = "body_militia_smg_aa_wht";
	bodies[ 9 ] = "body_militia_smg_ab_blk";
	bodies[ 10 ] = "body_militia_smg_ac_blk";
	switch ( codescripts\character::get_random_mounted_character( bodies ) )
	{
	case 0:
		character\character_opf_militia_assault_aa_blk::main();
		break;
	case 1:
		character\character_opf_militia_assault_aa_wht::main();
		break;
	case 2:
		character\character_opf_militia_assault_ab_blk::main();
		break;
	case 3:
		character\character_opf_militia_assault_ac_blk::main();
		break;
	case 4:
		character\character_opf_militia_lmg_aa_blk::main();
		break;
	case 5:
		character\character_opf_militia_lmg_ab_blk::main();
		break;
	case 6:
		character\character_opf_militia_lmg_ac_blk::main();
		break;
	case 7:
		character\character_opf_militia_smg_aa_blk::main();
		break;
	case 8:
		character\character_opf_militia_smg_aa_wht::main();
		break;
	case 9:
		character\character_opf_militia_smg_ab_blk::main();
		break;
	case 10:
		character\character_opf_militia_smg_ac_blk::main();
		break;
	}
}

spawner()
{
	self setspawnerteam( "axis" );
}

precache()
{
	character\character_opf_militia_assault_aa_blk::precache();
	character\character_opf_militia_assault_aa_wht::precache();
	character\character_opf_militia_assault_ab_blk::precache();
	character\character_opf_militia_assault_ac_blk::precache();
	character\character_opf_militia_lmg_aa_blk::precache();
	character\character_opf_militia_lmg_ab_blk::precache();
	character\character_opf_militia_lmg_ac_blk::precache();
	character\character_opf_militia_smg_aa_blk::precache();
	character\character_opf_militia_smg_aa_wht::precache();
	character\character_opf_militia_smg_ab_blk::precache();
	character\character_opf_militia_smg_ac_blk::precache();
}
