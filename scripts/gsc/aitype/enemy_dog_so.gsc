// aitype\enemy_dog_so -- the Spec Ops dog for every actor_enemy_dog_so spawner
// (CoDOL survival addons: targetname dog_spawner, model german_sheperd_dog).
//
// Ported from Infinity Ward's Modern Warfare 3 aitype\enemy_dog (IW5:
// animtree dog.atr, type "dog", health 200, weapon dog_bite) into the coop
// dog species ("coop_dog", src/codol/player/codol_actor_scripts.cpp), whose
// animscripts are animscripts/coop_dog_*.  dog_bite, german_sheperd_dog,
// animlists/common_dog.animlist and the dog clips ship in common_gametype_coop;
// animtrees/dog.atr in common_mp.  maps\_coopsurvival_ai sets health from the
// survival AI type table (dog_reg / dog_splode) after the spawn.

/*QUAKED actor_enemy_dog_so (1.0 0.25 0.0) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="german_sheperd_dog"
"count" -- max AI to ever spawn from this spawner
SPAWNER -- makes this a spawner instead of a guy
*/
main()
{
	self.animTree = "dog.atr";
	self.team = "axis";
	self.type = "dog";
	self.accuracy = 1;
	self.health = 200;
	self.weapon = character\character_so_dog::bite_weapon();
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeWeapon = "";
	self.grenadeAmmo = 0;
	self.csvInclude = "";

	self setEngagementMinDist( 0.000000, 0.000000 );
	self setEngagementMaxDist( 100.000000, 300.000000 );

	character\character_so_dog::main();
}

spawner()
{
	self setspawnerteam( "axis" );
}

precache()
{
	character\character_so_dog::precache();
}
