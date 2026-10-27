// aitype\enemy_so_demolitionist -- the coop actor for every actor_enemy_so_demolitionist spawner.
// Ported from Infinity Ward's Modern Warfare 3 Spec Ops Survival aitype
// enemy_so_demolitionist (CoDOL: AT4), in this server's coop actor contract (aitype\enemy_coop_soldier::
// soldier_setup: the coop weapon of the role, the engagement bands).  The
// survival layer (maps\_coopsurvival_ai::spawn_type) sets health, accuracy and
// speed from the AI type table after the spawn.  Look:
// character\character_so_survival, CoDOL's own survival model for it.

/*QUAKED actor_enemy_so_demolitionist (1.0 0.25 0.0) (-16 -16 0) (16 16 72) SPAWNER FORCESPAWN UNDELETABLE PERFECTENEMYINFO DONTSHAREENEMYINFO
defaultmdl="body_composite_smg"
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
	self.accuracy = 0.2;
	self.health = 150;
	self.secondaryweapon = "";
	self.sidearm = "";
	self.grenadeWeapon = "";
	self.grenadeAmmo = 0;

	aitype\enemy_coop_soldier::soldier_setup( "ar" );

	if ( aitype\enemy_coop_soldier::scripted_character() )
		return;

	character\character_so_survival::main( "demolitionist" );
}

spawner()
{
	self setspawnerteam( "axis" );
}

precache()
{
	character\character_so_survival::precache( "demolitionist" );
}
