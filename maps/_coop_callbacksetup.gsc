// coop asset-category callback setup.
//
// The coop counterpart of maps/mp/gametypes/_callbacksetup.gsc (T5 keeps its
// singleplayer one at maps/_callbacksetup.gsc).  GScr_LoadGameTypeScript binds
// the six mandatory CodeCallback_* labels from the category's own callback
// module, which for coop is this file (CODOL_ScriptEntryPoints,
// src/codol/script/codol_script_source.c).
//
// It exists for the same reason the zm copy does.  The multiplayer
// _callbacksetup's SetDefaultCallbacks names maps\mp\gametypes\_gamelogic,
// \_playerlogic, \_damage and \_hostmigration as function pointers, and a GSC
// `file::function` reference is a COMPILE-TIME dependency: merely loading that
// file compiles the whole multiplayer gametype chain.  Measured with
// tools/codol_script_resolve.py, that chain is 100 scripts -- every killstreak,
// every perk, _class, _rank, _missions, _persistence, _spawnlogic -- all of
// which precache multiplayer assets out of common_gametype_mp, which a coop
// match does not mount (it mounts common_gametype_coop).  Nothing in this file
// names a multiplayer module, so nothing multiplayer is compiled.
//
// Shape follows the IW4 maps/mp/gametypes/_callbacksetup.gsc that the shared
// tree carries.  CodeCallback_PlayerMigrated / CodeCallback_HostMigration are
// omitted because this server has no host migration, and
// CodeCallback_ScriptCommand is omitted exactly as IW4 omits it (the engine
// loads that one with errorIfMissing = 0).

/*================
Called by code after the level's main script function has run.
================*/
CodeCallback_StartGameType()
{
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
Called when a player has been killed but is eligible for last stand.  Spec Ops
calls this state "downed"; IW4's maps/_coop.gsc drives bleed-out and revive from
the same moment (its own trigger is EnableDeathShield, which this server has no
builtin for -- see _coopmode::Callback_PlayerLastStand).
================*/
CodeCallback_PlayerLastStand( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration )
{
	self endon( "disconnect" );
	[[ level.callbackPlayerLastStand ]]( eInflictor, eAttacker, iDamage, sMeansOfDeath, sWeapon, vDir, sHitLoc, timeOffset, deathAnimDuration );
}

/*================
Called when an actor has been killed.  self is the actor.
The coop mission's enemy soldiers are native actors (species "soldier",
src/codol/player/codol_actor_scripts.cpp); the engine resolves this label
optionally (CODOL_ActorDamage_LoadCallbacks), exactly as the zm copy relies on.
CodeCallback_ActorDamage is deliberately NOT defined, as in zm: defining it
would gate every actor's damage on finishActorDamage.
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
	level.callbackStartGameType		= maps\_coopmode::Callback_StartGameType;
	level.callbackPlayerConnect		= maps\_coopmode::Callback_PlayerConnect;
	level.callbackPlayerDisconnect	= maps\_coopmode::Callback_PlayerDisconnect;
	level.callbackPlayerDamage		= maps\_coopmode::Callback_PlayerDamage;
	level.callbackPlayerKilled		= maps\_coopmode::Callback_PlayerKilled;
	level.callbackPlayerLastStand	= maps\_coopmode::Callback_PlayerLastStand;
	level.callbackCodeEndGame		= maps\_coopmode::Callback_CodeEndGame;
	level.callbackActorKilled		= maps\_coopmode::Callback_ActorKilled;
}

callbackVoid()
{
}

/*================
Called when a gametype is not supported.

REQUIRED, not optional.  maps\_coop_callbacksetup::AbortLevel() is
called from nine sites in the SHARED script tree (_spawnlogic, _teams and
others).  A category _callbacksetup REPLACES the shared one wholesale, so
omitting this makes any shared module that reaches it a fatal COMPILE error --
"unknown function", Sys_Error, dead server -- not a runtime no-op.  MEASURED
2026-09-20: a co-op match died exactly that way at Game Initialization once
the dog aitype preload dragged _spawnlogic in.

Divergence from the multiplayer version, deliberately: it does NOT
setdvar("g_gametype","dm").  Under this category the gametype script is forced
by CODOL_ScriptEntryPoints (src/codol/script/codol_script_source.c), and rewriting the dvar to a
multiplayer gametype here would only make the abort disagree with the mounted
zones and with what the client was told.
================*/
AbortLevel()
{
	println( "Aborting level - gametype is not supported (coop)" );

	level.callbackStartGameType		= ::callbackVoid;
	level.callbackPlayerConnect		= ::callbackVoid;
	level.callbackPlayerDisconnect	= ::callbackVoid;
	level.callbackPlayerDamage		= ::callbackVoid;
	level.callbackPlayerKilled		= ::callbackVoid;
	level.callbackPlayerLastStand	= ::callbackVoid;
	level.callbackCodeEndGame		= ::callbackVoid;

	exitLevel( false );
}
