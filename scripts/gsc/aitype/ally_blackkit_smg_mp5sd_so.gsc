// aitype\ally_blackkit_smg_mp5sd_so -- the coop actor for every actor_ally_blackkit_smg_mp5sd_so spawner
// (any spelling; the addons vary the case).  A CoDOL classname, in IW4's aitype
// layout.
//
// Loaded and precached by the engine at level load (codol_actor_scripts.cpp,
// AuthoredSpecies), main() run by spawnactor on a spawner of this classname.
// Weapons are the coop role's (aitype\enemy_coop_soldier::role_weapons); health
// and accuracy are the values the coop missions have used since 2026-10-07.

/*QUAKED actor_ally_blackkit_smg_mp5sd_so (0.0 0.25 1.0) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="body_complete_sp_sas_ct_benjamin"
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

	aitype\enemy_coop_soldier::soldier_setup( "smg" );

	if ( aitype\enemy_coop_soldier::scripted_character() )
		return;

	bodies = [];
	bodies[ 0 ] = "body_complete_sp_sas_ct_benjamin";
	bodies[ 1 ] = "body_complete_sp_sas_ct_charles";
	bodies[ 2 ] = "body_complete_sp_sas_ct_mitchel";
	bodies[ 3 ] = "body_complete_sp_sas_ct_neal";
	bodies[ 4 ] = "body_complete_sp_sas_ct_william";
	switch ( codescripts\character::get_random_mounted_character( bodies ) )
	{
	case 0:
		character\character_sas_ct_benjamin::main();
		break;
	case 1:
		character\character_sas_ct_charles::main();
		break;
	case 2:
		character\character_sas_ct_mitchel::main();
		break;
	case 3:
		character\character_sas_ct_neal::main();
		break;
	case 4:
		character\character_sas_ct_william::main();
		break;
	}
}

spawner()
{
	self setspawnerteam( "allies" );
}

precache()
{
	character\character_sas_ct_benjamin::precache();
	character\character_sas_ct_charles::precache();
	character\character_sas_ct_mitchel::precache();
	character\character_sas_ct_neal::precache();
	character\character_sas_ct_william::precache();
}
