// PROVENANCE: written for CODOL 1.1.1.13.  There is no vendor/iw4 equivalent --
// IW4 relays chat entirely inside the engine (G_Say -> SV_GameSendServerCommand),
// so no shipped .gsc ever sees a chat line.  This build cannot do that: the
// relay has to exist somewhere, and the project rule is that everything except
// movement and usercmds is GSC.  So the relay is here.
//
// THE EDGE.  gsc/runtime.py::player_say( clientNum, text, mode ) is this
// server's Scr_PlayerSay: it raises the notify `"say"` on the speaking player
// with ( message, mode ), which is the IW3/IW4/T4/T5 contract every public mod
// tree blocks on with `self waittill( "say", message )`.  Whichever transport
// delivered the text calls it -- on this build that is the Demonware service-102
// channel (`room.say` / `room.say2team` / `party.say`, docs/service102_chat.md),
// because C1.exe has NO `say` reliable client command (MEASURED, 2026-09-02).
//
// THE RELAY.  sayAll()/sayTeam() are real builtins again
// (gsc/builtin_surface.py); they hand the line to the host, which puts it on the
// reliable outbox.  Policy lives HERE so it can be changed without touching
// Python: who may talk, whether the dead may talk, and whether a team message
// stays inside the team.

#include common_scripts\utility;
#include maps\mp\_utility;

init()
{
	// scr_deadchat: retail IW4 default is 0 -- a dead player's messages do not
	// reach the living.  Registered here rather than assumed so a server op can
	// change it, and so getDvarInt() below never reads an unset dvar.
	setDvarIfUninitialized( "scr_deadchat", "0" );
	setDvarIfUninitialized( "scr_allchat", "1" );

	level thread onPlayerConnect();
}


onPlayerConnect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread onPlayerSay();
	}
}


onPlayerSay()
{
	self endon( "disconnect" );

	for ( ;; )
	{
		self waittill( "say", message, mode );

		if ( !isDefined( message ) || message == "" )
			continue;

		if ( isDefined( self.muted ) && self.muted )
			continue;

		// A dead player's line is team-only unless scr_deadchat says otherwise.
		// This is IW4's own rule and the reason the mode is passed through
		// rather than decided in Python.
		teamOnly = ( isDefined( mode ) && mode == "team" );
		if ( !isAlive( self ) && getDvarInt( "scr_deadchat" ) == 0 )
			teamOnly = true;

		if ( teamOnly || getDvarInt( "scr_allchat" ) == 0 )
			self sayTeam( message );
		else
			self sayAll( message );
	}
}
