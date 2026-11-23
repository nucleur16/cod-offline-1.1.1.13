#include maps\mp\_utility;
#include common_scripts\utility;

// survival intro: IW5 Spec Ops Survival's slam zoom in place of the MP
// prematch countdown.
//
// IW5 maps\_so_survival::setup_players threads do_slamzoom() on every player
// (D:\CoD\COD-GSC-Source\IW5-GSC\scripts\1571.gsc): the player is frozen with
// his weapons down, lifted 16000 units, linked to a tag_origin looking 89
// degrees down, and the tag drops back onto his start in 1.75 s, all of it
// decelerating (moveTo( start, 1.75, 0, 1.75 )).  0.55 s before it lands the
// view flashes (vision coup_sunblind over 0.25 s, back over 1 s) and the tag
// turns back to the player's own angles in 0.5 s (0.3 s in, 0.2 s out); 0.5 s
// later he is unlinked and given his controls and weapons.  The survival
// waves start a few seconds after (IW5 waittill_survival_start, here
// _survival::main's wait).
//
// On the multiplayer chain the start is _gamelogic::prematchPeriod: "waiting
// for players" and then "match begins in", players frozen until it ends.
// Survival has no opposing team to wait for, so that period was the whole
// playerwaittime + matchstarttime.  init() turns it off for survival (the
// skip branch, matchStartTimerSkip, only sets the map's vision), and each
// player's first spawn of the map gets his own descent instead.  A player
// spawns once he has picked his class (C1's class picker answers the
// changeclass menu, _menus::menuClass spawns him), so the descent follows
// the pick, as IW5's follows the loadout: everyone at the start, and a late
// joiner on his first spawn.  A respawn at an intermission gets none.  The
// other MP gametypes keep their countdown: only survival.gsc calls this.
//
// Differences from IW5: the view holds black for 0.25 s first (the spawn's
// weapon raise is put away under it), the flash is a white overlay (C1 has
// no coup_sunblind vision), the drop ends on the floor under a start that
// floats above it, and partners do not see the falling body (hide()).
//
// scr_survival_intro 0 keeps the MP countdown; scr_survival_intro_height and
// scr_survival_intro_time change the drop (IW5's 16000 and 1.75).

init()
{
	if ( getDvar( "scr_survival_intro" ) != "" && !getDvarInt( "scr_survival_intro" ) )
		return;

	// _gamelogic::startGame runs after onStartGameType: with no period it
	// takes matchStartTimerSkip and sets prematch_done at once, so nobody is
	// frozen by the MP layer and no countdown is drawn.
	level.prematchPeriod = 0;
	level.prematchPeriodEnd = 0;

	level.survival_intro_height = intro_dvar( "scr_survival_intro_height", 16000 );
	level.survival_intro_time = intro_dvar( "scr_survival_intro_time", 1.75 );
	if ( level.survival_intro_time < 0.8 )
		level.survival_intro_time = 0.8;

	precacheModel( "tag_origin" );
	precacheShader( "white" );
	precacheShader( "black" );

	level thread on_connect();
}

intro_dvar( name, value )
{
	if ( getDvar( name ) == "" )
		return value;
	return getDvarFloat( name );
}

on_connect()
{
	for ( ;; )
	{
		level waittill( "connected", player );
		player thread first_spawn();
	}
}

// The first spawn of this map only. Script fields do not survive a map
// change or map_restart, so a devmap or a restart gives every player the
// descent again.
first_spawn()
{
	self endon( "disconnect" );
	if ( isDefined( self.survival_intro_done ) )
		return;
	self waittill( "spawned_player" );
	self.survival_intro_done = true;
	if ( isDefined( level.survival_over ) && level.survival_over )
		return;
	if ( game["state"] != "playing" )
		return;
	self slamzoom();
}

// IW5 do_slamzoom, on the multiplayer player.
slamzoom()
{
	self endon( "disconnect" );
	self endon( "death" );
	self endon( "survival_intro_landed" );

	time = level.survival_intro_time;
	height = level.survival_intro_height;
	// Some survival starts float above the floor (Seatown's by 84 units): land
	// on the floor under the start, not in the air above it.
	land = playerPhysicsTrace( self.origin + ( 0, 0, 4 ), self.origin - ( 0, 0, 256 ) );
	if ( land[ 2 ] <= self.origin[ 2 ] - 255 )
		land = self.origin;
	angles = self getPlayerAngles();
	angles = ( angles[ 0 ], angles[ 1 ], 0 );
	down = ( 89, angles[ 1 ], 0 );

	self.survival_in_intro = true;
	self freezeControlsWrapper( true );
	self _disableWeapon();
	self _disableOffhandWeapons();
	self setClientDvar( "hud_drawHud", 0 );
	// Out of reach of the waves (a late joiner drops in while one is on);
	// _survival::ai_hit honours it.
	self.survival_invul_until = getTime() + int( ( time + 1.75 ) * 1000 );
	// Partners do not see a body falling out of the sky.
	self hide();

	rig = spawn( "script_model", land + ( 0, 0, height ) );
	rig setModel( "tag_origin" );
	rig.angles = down;
	self.survival_intro_rig = rig;
	level thread delete_rig_on_disconnect( self, rig );
	self thread abort_on_end();
	self thread abort_on_game_end();

	self setOrigin( land + ( 0, 0, height ) );
	self setPlayerAngles( down );
	// View follows the tag fully (fraction 1) with no free look (arcs 0).
	self playerLinkTo( rig, "tag_origin", 1, 0, 0, 0, 0 );

	// IW5's challenge fade-in: the view comes out of black high above the
	// map. The black holds while the spawn's weapon raise is put down, then
	// the drop starts as it fades.
	self.survival_intro_fade = self overlay( "black", 1 );
	self.survival_intro_flash = self overlay( "white", 0 );

	self thread hide_survival_hud();

	wait( 0.25 );
	self.survival_intro_fade fadeOverTime( 0.5 );
	self.survival_intro_fade.alpha = 0;
	rig moveTo( land, time, 0, time );
	codolprintln( "CoDOL survival: " + self.name + " intro from " + rig.origin + " to " + land + " in " + time + " s" );
	wait( 0.05 );
	wait( time - 0.55 );

	// The sun-blind flash as the view turns level (IW5: coup_sunblind in
	// 0.25 s, the map's vision back in 1 s).
	self.survival_intro_flash fadeOverTime( 0.25 );
	self.survival_intro_flash.alpha = 0.55;
	rig rotateTo( angles, 0.5, 0.3, 0.2 );
	wait( 0.25 );
	self.survival_intro_flash fadeOverTime( 1.0 );
	self.survival_intro_flash.alpha = 0;
	wait( 0.45 );

	self finish();
}

// Landed: the player stands on his start with his own angles and gets his
// controls, weapons and HUD back.
finish()
{
	if ( !isDefined( self.survival_in_intro ) || !self.survival_in_intro )
		return;
	self.survival_in_intro = false;
	self unlink();
	self show();
	self _enableWeapon();
	self _enableOffhandWeapons();
	self setClientDvar( "hud_drawHud", 1 );
	if ( !isDefined( level.survival_over ) || !level.survival_over )
		self freezeControlsWrapper( false );
	if ( isDefined( self.survival_intro_rig ) )
		self.survival_intro_rig delete();
	self.survival_intro_rig = undefined;
	self show_survival_hud();
	self thread remove_overlays();
	codolprintln( "CoDOL survival: " + self.name + " landed (intro)" );
	// Last: it ends the drop's own threads, possibly the one running this.
	self notify( "survival_intro_landed" );
}

remove_overlays()
{
	self endon( "disconnect" );
	fade = self.survival_intro_fade;
	flash = self.survival_intro_flash;
	self.survival_intro_fade = undefined;
	self.survival_intro_flash = undefined;
	if ( isDefined( flash ) && flash.alpha > 0 )
	{
		flash fadeOverTime( 0.5 );
		flash.alpha = 0;
	}
	wait( 1.0 );
	if ( isDefined( fade ) )
		fade destroy();
	if ( isDefined( flash ) )
		flash destroy();
}

// Death or a team change mid-drop: unlink and give the state back, the way
// landing does.
abort_on_end()
{
	self endon( "disconnect" );
	self endon( "survival_intro_landed" );
	self waittill_any( "death", "joined_spectators", "joined_team", "survival_intro_abort" );
	self finish();
}

abort_on_game_end()
{
	self endon( "disconnect" );
	self endon( "survival_intro_landed" );
	level waittill( "game_ended" );
	self notify( "survival_intro_abort" );
}

// A player who disconnects mid-drop leaves the tag behind.
delete_rig_on_disconnect( player, rig )
{
	rig endon( "death" );
	player endon( "survival_intro_landed" );
	player waittill( "disconnect" );
	if ( isDefined( rig ) )
		rig delete();
}

overlay( shader, alpha )
{
	h = newClientHudElem( self );
	h.x = 0;
	h.y = 0;
	h.horzAlign = "fullscreen";
	h.vertAlign = "fullscreen";
	h.alignX = "left";
	h.alignY = "top";
	h.sort = 1000;
	h.foreground = true;
	h.hidewheninmenu = false;
	h setShader( shader, 640, 480 );
	h.alpha = alpha;
	return h;
}

// _survival::player_hud makes the wave and armor lines on the same
// spawned_player; they appear when the player lands, as IW5's survival HUD
// does after its slam zoom.
hide_survival_hud()
{
	self endon( "disconnect" );
	waittillframeend;
	if ( !self.survival_in_intro )
		return;
	if ( isDefined( self.survival_hud_wave ) )
		self.survival_hud_wave.alpha = 0;
	if ( isDefined( self.survival_hud_armor ) )
		self.survival_hud_armor.alpha = 0;
}

show_survival_hud()
{
	if ( isDefined( self.survival_hud_wave ) )
	{
		self.survival_hud_wave fadeOverTime( 0.5 );
		self.survival_hud_wave.alpha = 1;
	}
	if ( isDefined( self.survival_hud_armor ) )
	{
		self.survival_hud_armor fadeOverTime( 0.5 );
		self.survival_hud_armor.alpha = 1;
	}
}
