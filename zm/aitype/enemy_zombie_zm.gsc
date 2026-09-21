// zm enemy zombie aitype.
//
// Shape copied from the dog's aitype/enemy_dog_mp.gsc, which is the contract
// CODOL_ActorScriptsLoadType expects: main(), precache() and spawner(), loaded
// as aitype/<classname without the actor_ prefix>.
//
// This file is resolved out of the zm CATEGORY root
// (bin/home/codol_rawfiles/zm/), never the shared one: the species table's
// category column is what keeps the multiplayer dog chain out of zm matches
// and must keep this out of mp matches for the same reason.
//
// T5 has no single "zombie" aitype -- maps\_zombiemode_spawner.gsc spawns per
// map from actor_zombie_* spawners authored in the BSP. CoDOL's zm maps author
// none, so, exactly like the dogs killstreak, one aitype is preloaded per
// category and the spawner is built at runtime with spawnactorspawner().

/*QUAKED actor_enemy_zombie_zm (1.0 0.25 0.0) (-16 -16 0) (16 16 72) SPAWNER MAKEROOM UNDELETABLE ENEMYINFO SCRIPT_FORCESPAWN SM_PRIORITY
defaultmdl="c_zom_ger_body1"
"count" -- max AI to ever spawn from this spawner
SPAWNER -- makes this a spawner instead of a guy
MAKEROOM -- will try to delete an AI if spawning fails from too many AI
UNDELETABLE -- this AI (or AI spawned from here) cannot be deleted to make room for MAKEROOM guys
ENEMYINFO -- this AI when spawned will get a snapshot of perfect info about all enemies
SCRIPT_FORCESPAWN -- this AI will spawned even if players can see him spawning.
SM_PRIORITY -- Make the Spawn Manager spawn from this spawner before other spawners.
*/
main()
{
	// animtrees/generic_human.atr, common_mp (mounted for every asset
	// category). It is declared with #using_animtree in
	// maps/mp/animscripts/zombie_init.gsc, which is what actually registers
	// the server tree the native actor path binds.
	self.animTree = "generic_human.atr";
	self.team = "axis";
	self.type = "zombie";
	self.accuracy = 1;

	// T5 maps/_zombiemode.gsc gives a round-1 zombie 150 health and scales it
	// per round from zombie_vars; the round loop is not this task's scope, so
	// the round-1 value stands on its own here.
	self.health = 150;

	self.weapon = "";
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeWeapon = "";
	self.grenadeAmmo = 0;
	self.csvInclude = "";

	self setEngagementMinDist( 0.000000, 0.000000 );
	self setEngagementMaxDist( 64.000000, 128.000000 );

	// Without this a zombie chases but NEVER SWINGS. MEASURED:
	// CODOL_DogCombatGoal (codol_dog_combat.c:104) returns all-zero when
	// attackDistance <= 0, so inAttackRange is never true, so the native
	// frame never switches to the combat animscript and zombie_combat.gsc
	// never runs -- while zombie_combat.gsc:205/:221 reads this very field.
	// Nothing else in the zm tree sets it, so it was 0.
	// 64 is this file's own authored engagement max on the line above, not
	// an invented number; revisit it against the swing animations' reach.
	self.meleeattackdist = 64;

	character\character_zm_zombie::main();
}

spawner()
{
	self setspawnerteam( "axis" );
}

precache()
{
	character\character_zm_zombie::precache();
}
