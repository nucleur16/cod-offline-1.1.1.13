// aitype\enemy_vazquez_so -- the coop actor for every actor_enemy_vazquez_so spawner
// (any spelling; the addons vary the case).  A CoDOL classname, in IW4's aitype
// layout.
//
// Loaded and precached by the engine at level load (codol_actor_scripts.cpp,
// AuthoredSpecies), main() run by spawnactor on a spawner of this classname.
// Weapons are the coop role's (aitype\enemy_coop_soldier::role_weapons); health
// and accuracy are the values the coop missions have used since 2026-10-07.

/*QUAKED actor_enemy_vazquez_so (1.0 0.25 0.0) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="body_militia_smg_aa_wht"
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
	self.accuracy = 0.3;
	self.health = 150;
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeWeapon = "";
	self.grenadeAmmo = 0;

	aitype\enemy_coop_soldier::soldier_setup( "pistol" );

	if ( aitype\enemy_coop_soldier::scripted_character() )
		return;

	character\character_codo_vil_vazquez::main();
}

spawner()
{
	self setspawnerteam( "axis" );
}

precache()
{
	character\character_codo_vil_vazquez::precache();
}
