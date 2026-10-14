// aitype\ally_codo_tf_mc_lwset_so -- the coop actor for every actor_ally_codo_tf_mc_lwset_so spawner
// (any spelling; the addons vary the case).  A CoDOL classname, in IW4's aitype
// layout.
//
// Loaded and precached by the engine at level load (codol_actor_scripts.cpp,
// AuthoredSpecies), main() run by spawnactor on a spawner of this classname.
// Weapons are the coop role's (aitype\enemy_coop_soldier::role_weapons); health
// and accuracy are the values the coop missions have used since 2026-10-07.
// The CoDO Task Force recruits' mixed set ("mc"): the four Task Force male
// outfits with the hea_* head that names each, plus the set's Radiant model
// and the tan uniform.  coop_spec1_mp_killhouse_sh places only this classname
// (and Price) and ships exactly these bodies and heads.

/*QUAKED actor_ally_codo_tf_mc_lwset_so (0.0 0.25 1.0) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="body_bdurolled_tf_m"
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

	aitype\enemy_coop_soldier::soldier_setup( "pistol" );

	if ( aitype\enemy_coop_soldier::scripted_character() )
		return;

	bodies = [];
	bodies[ 0 ] = "body_bdurolled_tf_m";
	bodies[ 1 ] = "body_codotshirt_tf_m";
	bodies[ 2 ] = "body_uniform_tf_m";
	bodies[ 3 ] = "body_ustshirt_tf_m";
	bodies[ 4 ] = "body_complete_sp_lw";
	bodies[ 5 ] = "body_complete_tf_tan";
	switch ( codescripts\character::get_random_mounted_character( bodies ) )
	{
	case 0:
		character\character_codo_tf_bdurolled::main();
		break;
	case 1:
		character\character_codo_tf_codotshirt::main();
		break;
	case 2:
		character\character_codo_tf_uniform::main();
		break;
	case 3:
		character\character_codo_tf_ustshirt::main();
		break;
	case 4:
		character\character_codo_complete_lw::main();
		break;
	case 5:
		character\character_codo_complete_tf_tan::main();
		break;
	}
}

spawner()
{
	self setspawnerteam( "allies" );
}

precache()
{
	character\character_codo_tf_bdurolled::precache();
	character\character_codo_tf_codotshirt::precache();
	character\character_codo_tf_uniform::precache();
	character\character_codo_tf_ustshirt::precache();
	character\character_codo_complete_lw::precache();
	character\character_codo_complete_tf_tan::precache();
}
