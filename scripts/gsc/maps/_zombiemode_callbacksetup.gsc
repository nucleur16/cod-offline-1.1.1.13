// zm asset-category callback setup.
//
// The zm counterpart of maps/mp/gametypes/_callbacksetup.gsc (T5 keeps its
// singleplayer/zombies one at maps/_callbacksetup.gsc).  GScr_LoadGameTypeScript
// binds the six mandatory CodeCallback_* labels from the category's own
// callback module, which for zm is this file (CODOL_ScriptEntryPoints,
// src/codol/script/codol_script_source.c).
//
// It exists because the multiplayer _callbacksetup's SetDefaultCallbacks names
// maps\mp\gametypes\_gamelogic, \_playerlogic, \_damage and \_hostmigration as
// function pointers.  A GSC function reference is a compile-time dependency, so
// merely loading that file compiles the entire multiplayer gametype chain -- the
// chain that precached the killstreak xmodels C1 then reported as "not in any
// mounted zone" under zm, because those assets live in common_gametype_mp and a
// zm match mounts common_gametype_zm instead.  Nothing here names a multiplayer
// module, so nothing multiplayer is compiled.
//
// Shape follows vendor/iw4/maps/mp/gametypes/_callbacksetup.gsc; the two
// callbacks IW4 registers that the engine does not demand
// (CodeCallback_PlayerMigrated, CodeCallback_HostMigration) are omitted because
// this server has no host migration, and CodeCallback_ScriptCommand is omitted
// exactly as IW4 omits it (loaded with errorIfMissing=0, scr_vm_main.c:1312).

/*================
Called by code after the level's main script function has run.
================*/
CodeCallback_StartGameType()
{
	maps\_codol_maintenance_banner::init();
	// If the gametype has not been started, run the startup
	if ( !isDefined( level.gametypestarted ) || !level.gametypestarted )
	{
		[[ level.callbackStartGameType ]]();

		level.gametypestarted = true;
	}
}

/*================
Called when a player begins connecting to the server.
================*/
CodeCallback_PlayerConnect()
{
	self endon( "disconnect" );
	[[ level.callbackPlayerConnect ]]();
}

/*================
Called when a player drops from the server.
================*/
CodeCallback_PlayerDisconnect()
{
	self notify( "disconnect" );
	[[ level.callbackPlayerDisconnect ]]();
}

/*================
Called when a player has taken damage.
================*/
CodeCallback_PlayerDamage( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset )
{
	self endon( "disconnect" );
	[[ level.callbackPlayerDamage ]]( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, timeOffset );
}

/*================
Called when a player has been killed.
================*/
CodeCallback_PlayerKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	self endon( "disconnect" );
	[[ level.callbackPlayerKilled ]]( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration );
}

/*================
Called when a player has been killed but is eligible for last stand.
Zombies calls this the "downed" state; C1 already replaces the scoreboard's
deaths column with MPUI_REVIVES under gametype zm
(docs/CODOL_SCOREBOARD_CONTRACT.md, alpha0022/docs/zombies_mode.md:1088-1148).
================*/
CodeCallback_PlayerLastStand( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	self endon( "disconnect" );
	[[ level.callbackPlayerLastStand ]]( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration );
}

/*================
Called when an actor has been killed.  self is the actor.
T5 MP _callbacksetup.gsc CodeCallback_ActorKilled; the engine hands it the
killing hit's means of death, weapon and HIT LOCATION
(codol_actor_lifecycle.cpp ActorKilledCallback).  Actors are traced through
their hit boxes, so sHitLoc is the struck location ("head", "neck", ...); the
actor "damage" notify carries the struck bone as partName, not sHitLoc.
CodeCallback_ActorDamage is deliberately NOT defined: defining it would gate
every actor's damage on finishActorDamage.  Without it the engine still
scales each hit by its location multiplier once, as T5 G_DamageActor does.
================*/
CodeCallback_ActorKilled( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset )
{
	[[ level.callbackActorKilled ]]( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset );
}

/*================
Called when code is forcibly ending the game.
================*/
CodeCallback_CodeEndGame()
{
	self endon( "disconnect" );
	[[ level.callbackCodeEndGame ]]();
}

/*================
Damage flags used in the playerDamage callback.  Code-defined values; identical
to the multiplayer table because they are the engine's, not the gametype's.
================*/
SetupDamageFlags()
{
	level.iDFLAGS_RADIUS						= 1;
	level.iDFLAGS_NO_ARMOR						= 2;
	level.iDFLAGS_NO_KNOCKBACK					= 4;
	level.iDFLAGS_PENETRATION					= 8;
	level.iDFLAGS_STUN							= 16;
	level.iDFLAGS_SHIELD_EXPLOSIVE_IMPACT		= 32;
	level.iDFLAGS_SHIELD_EXPLOSIVE_IMPACT_HUGE	= 64;
	level.iDFLAGS_SHIELD_EXPLOSIVE_SPLASH		= 128;

	level.iDFLAGS_NO_TEAM_PROTECTION			= 256;
	level.iDFLAGS_NO_PROTECTION					= 512;
	level.iDFLAGS_PASSTHRU						= 1024;
}

SetupCallbacks()
{
	SetDefaultCallbacks();
	SetupDamageFlags();
}

SetDefaultCallbacks()
{
	level.callbackStartGameType		= maps\_zombiemode::Callback_StartGameType;
	level.callbackPlayerConnect		= maps\_zombiemode::Callback_PlayerConnect;
	level.callbackPlayerDisconnect	= maps\_zombiemode::Callback_PlayerDisconnect;
	level.callbackPlayerDamage		= maps\_zombiemode::Callback_PlayerDamage;
	level.callbackPlayerKilled		= maps\_zombiemode::Callback_PlayerKilled;
	level.callbackPlayerLastStand	= maps\_zombiemode::Callback_PlayerLastStand;
	level.callbackCodeEndGame		= maps\_zombiemode::Callback_CodeEndGame;
	level.callbackActorKilled		= maps\_zombiemode_challenges::Callback_ActorKilled;
}

callbackVoid()
{
}

/*================
Called when a gametype is not supported.

REQUIRED, not optional.  maps\_zombiemode_callbacksetup::AbortLevel() is
called from nine sites in the SHARED script tree (_spawnlogic, _teams and
others).  A category _callbacksetup REPLACES the shared one wholesale, so
omitting this makes any shared module that reaches it a fatal COMPILE error --
"unknown function", Sys_Error, dead server -- not a runtime no-op.  MEASURED
2026-09-20: a zombies match died exactly that way at Game Initialization once
the dog aitype preload dragged _spawnlogic in.

Divergence from the multiplayer version, deliberately: it does NOT
setdvar("g_gametype","dm").  Under this category the gametype script is forced
by CODOL_ScriptEntryPoints (src/codol/script/codol_script_source.c), and rewriting the dvar to a
multiplayer gametype here would only make the abort disagree with the mounted
zones and with what the client was told.
================*/
AbortLevel()
{
	{}

	level.callbackStartGameType		= ::callbackVoid;
	level.callbackPlayerConnect		= ::callbackVoid;
	level.callbackPlayerDisconnect	= ::callbackVoid;
	level.callbackPlayerDamage		= ::callbackVoid;
	level.callbackPlayerKilled		= ::callbackVoid;
	level.callbackPlayerLastStand	= ::callbackVoid;
	level.callbackCodeEndGame		= ::callbackVoid;

	exitLevel( false );
}
