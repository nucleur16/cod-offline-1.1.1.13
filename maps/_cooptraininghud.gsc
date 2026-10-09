// coop_training_mp_killhouse_sh: the training mission's HUD, as the retail
// recording draws it (docs/CODOL_CAMPAIGN_MODE.md, the training passes).
//
// Every text here is the mission zone's own localize entry (the CoD4
// killhouse strings C1 ships in coop_training_mp_killhouse_sh.ff, read from
// the zone: KILLHOUSE_INTROSCREEN_LINE_*, the objective lines, the
// KILLHOUSE_HINT_* key hints with their [{+attack}] style key tokens, the
// course countdown), so C1 draws them in its own language.  The recording
// shows four kinds of element:
//
//   * the title card: three lines typed in at the left (the camp's name, the
//     date with a running clock, the classified location) over a black
//     screen that fades into the jeep ride;
//   * the objective line: light blue, top middle, typed in, held a few
//     seconds, then gone;
//   * the key hint: white, a third of the way down, on a dark band;
//   * the objective's 3D marker: a point with the distance under it, on the
//     current goal (squares on each window still to be hit).
//
// Coop module (maps/, IW4 singleplayer namespace); names no multiplayer module.

precache()
{
	precacheShader( "black" );
	precacheShader( "white" );
	precacheShader( "objpoint_default" );
	precacheShader( "waypoint_target" );

	precacheString( &"KILLHOUSE_INTROSCREEN_LINE_1" );
	precacheString( &"KILLHOUSE_INTROSCREEN_LINE_2" );
	precacheString( &"KILLHOUSE_INTROSCREEN_LINE_3" );

	precacheString( &"KILLHOUSE_USE_YOUR_OBJECTIVE_INDICATOR" );
	precacheString( &"KILLHOUSE_PICK_UP_A_RIFLE_FROM" );
	precacheString( &"KILLHOUSE_ENTER_STATION_NUMBER" );
	precacheString( &"KILLHOUSE_SHOOT_EACH_TARGET_WHILE1" );
	precacheString( &"KILLHOUSE_SHOOT_EACH_TARGET_WHILE" );
	precacheString( &"KILLHOUSE_SHOOT_EACH_TARGET_AS" );
	precacheString( &"KILLHOUSE_SWITCH_TO_YOUR_PISTOL" );
	precacheString( &"KILLHOUSE_SHOOT_EACH_TARGET_PISTOL" );
	precacheString( &"KILLHOUSE_MELEE_THE_TARGET" );
	precacheString( &"KILLHOUSE_GO_OUTSIDE_AND_REPORT" );
	precacheString( &"KILLHOUSE_ENTER_THE_SAFETY_PIT" );
	precacheString( &"KILLHOUSE_THROW_A_GRENADE_INTO" );
	precacheString( &"KILLHOUSE_HINT_EQUIP_LAUNCHER" );
	precacheString( &"KILLHOUSE_FIRE_AT_THE_WALL_WITH" );
	precacheString( &"KILLHOUSE_FIRE_YOUR_GRENADE_LAUNCHER" );
	precacheString( &"KILLHOUSE_RUN_THE_OBSTACLE_COURSE" );
	precacheString( &"KILLHOUSE_DEBRIEF_WITH_CPT_PRICE" );

	precacheString( &"KILLHOUSE_LOOK_UP" );
	precacheString( &"KILLHOUSE_LOOK_DOWN" );
	precacheString( &"KILLHOUSE_HINT_MOVE_AROUND" );
	precacheString( &"KILLHOUSE_HINT_HIP_ATTACK_PC" );
	precacheString( &"KILLHOUSE_HINT_CROSSHAIR_CHANGES" );
	precacheString( &"KILLHOUSE_HINT_ADS_THROW" );
	precacheString( &"KILLHOUSE_HINT_ADS_ACCURACY" );
	precacheString( &"KILLHOUSE_HINT_RELOAD" );
	precacheString( &"KILLHOUSE_HINT_SIDEARM_SWAP" );
	precacheString( &"KILLHOUSE_HINT_PRIMARY_SWAP" );
	precacheString( &"KILLHOUSE_HINT_MELEE_TARGET" );
	precacheString( &"KILLHOUSE_HINT_FRAG" );
	precacheString( &"KILLHOUSE_HINT_GRENADE_TOO_LOW" );
	precacheString( &"KILLHOUSE_HINT_FIREMODE" );
	precacheString( &"KILLHOUSE_HINT_ATTACK_PC" );
	precacheString( &"KILLHOUSE_HINT_GL_TOO_LOW" );
	precacheString( &"KILLHOUSE_HINT_SPRINT_PC" );
	precacheString( &"KILLHOUSE_HINT_HOLDING_SPRINT" );
	precacheString( &"KILLHOUSE_OBSTACLE_COUNTDOWN_3" );
	precacheString( &"KILLHOUSE_OBSTACLE_COUNTDOWN_2" );
	precacheString( &"KILLHOUSE_OBSTACLE_COUNTDOWN_1" );
}

// --------------------------------------------------------------------------
// The title card (recording 1:19-1:26): black, then the three lines typed in
// at the left, light blue; the black fades into the ride while the card
// stays; the card goes a few seconds into the ride.  Per player, from the
// moment he is seated (_coopintro board), so a late joiner gets none.
// --------------------------------------------------------------------------

title_card_player()
{
	self endon( "disconnect" );
	if ( isDefined( self.coop_title_shown ) )
		return;
	self.coop_title_shown = true;

	black = newClientHudElem( self );
	black.x = 0;
	black.y = 0;
	black.horzAlign = "fullscreen";
	black.vertAlign = "fullscreen";
	black.foreground = false;
	black.sort = -5;
	black.alpha = 1;
	black setShader( "black", 640, 480 );

	lines = [];
	lines[ 0 ] = &"KILLHOUSE_INTROSCREEN_LINE_1";
	lines[ 1 ] = &"KILLHOUSE_INTROSCREEN_LINE_2";
	lines[ 2 ] = &"KILLHOUSE_INTROSCREEN_LINE_3";
	huds = [];
	for ( i = 0; i < lines.size; i++ )
	{
		h = newClientHudElem( self );
		h.alignX = "left";
		h.alignY = "top";
		h.horzAlign = "left";
		h.vertAlign = "top";
		h.x = 64;
		h.y = 246 + i * 18;
		h.font = "default";
		h.fontScale = 1.4;
		h.color = ( 0.62, 0.86, 1 );
		h.foreground = true;
		h.sort = 2;
		h.hidewheninmenu = true;
		h setText( lines[ i ] );
		// The typewriter: 40 ms a letter, held, then a one-second fade.
		h setPulseFX( 40, 9000 - i * 600, 1000 );
		huds[ i ] = h;
		wait( 0.6 );
	}
	codolprintln( "CoDOL coop training: title card for " + self.name );

	// The black lifts as the jeep sets off (or after a few seconds).
	waited = 0;
	while ( waited < 4 && !isDefined( level.coop_intro_riding ) )
	{
		wait( 0.25 );
		waited += 0.25;
	}
	black fadeOverTime( 2 );
	black.alpha = 0;
	wait( 8 );
	black destroy();
	for ( i = 0; i < huds.size; i++ )
	{
		if ( isDefined( huds[ i ] ) )
			huds[ i ] destroy();
	}
}

// --------------------------------------------------------------------------
// The objective line (top middle, light blue, typed in, gone after a few
// seconds) -- the recording's reading of every new task.
// --------------------------------------------------------------------------

objective_text( text, time )
{
	if ( !isDefined( time ) )
		time = 5;
	if ( isDefined( level.coop_training_objtext ) )
		level.coop_training_objtext destroy();
	h = newHudElem();
	h.alignX = "center";
	h.alignY = "top";
	h.horzAlign = "center";
	h.vertAlign = "top";
	h.x = 0;
	h.y = 84;
	h.font = "objective";
	h.fontScale = 1.6;
	h.color = ( 0.45, 0.78, 1 );
	h.foreground = true;
	h.hidewheninmenu = true;
	h setText( text );
	h setPulseFX( 45, int( time * 1000 ), 800 );
	level.coop_training_objtext = h;
	h thread expire( time + 1 );
}

// --------------------------------------------------------------------------
// The key hint (white, a third of the way down, on a dark band).
// --------------------------------------------------------------------------

hint( text, time )
{
	hint_clear();
	band = newHudElem();
	band.alignX = "center";
	band.alignY = "middle";
	band.horzAlign = "center";
	band.vertAlign = "top";
	band.x = 0;
	band.y = 150;
	band.foreground = false;
	band.hidewheninmenu = true;
	band.alpha = 0.45;
	band setShader( "black", 400, 18 );

	h = newHudElem();
	h.alignX = "center";
	h.alignY = "middle";
	h.horzAlign = "center";
	h.vertAlign = "top";
	h.x = 0;
	h.y = 150;
	h.font = "default";
	h.fontScale = 1.2;
	h.foreground = true;
	h.hidewheninmenu = true;
	h setText( text );
	level.coop_training_hint = h;
	level.coop_training_hint_band = band;
	if ( !isDefined( time ) )
		time = 5;
	level notify( "coop_training_hint" );
	level thread hint_expire( time );
}

hint_expire( time )
{
	level endon( "coop_training_hint" );
	wait( time );
	hint_clear();
}

hint_clear()
{
	if ( isDefined( level.coop_training_hint ) )
		level.coop_training_hint destroy();
	if ( isDefined( level.coop_training_hint_band ) )
		level.coop_training_hint_band destroy();
	level.coop_training_hint = undefined;
	level.coop_training_hint_band = undefined;
}

// Hints one after another (each `time` seconds), threaded.
hint_sequence( texts, time )
{
	level notify( "coop_training_hint_sequence" );
	level endon( "coop_training_hint_sequence" );
	level endon( "coop_mission_over" );
	for ( i = 0; i < texts.size; i++ )
	{
		hint( texts[ i ], time );
		wait( time );
	}
}

expire( time )
{
	self endon( "death" );
	wait( time );
	if ( isDefined( self ) )
		self destroy();
}

// --------------------------------------------------------------------------
// 3D markers: the current goal's point with its distance (`marker`), and the
// squares on each window still to be hit (`target_marker`, by key).
// --------------------------------------------------------------------------

marker( origin, ent, height )
{
	marker_clear();
	if ( !isDefined( origin ) && !isDefined( ent ) )
		return;
	level.coop_training_marker = new_marker( origin, ent, height, "objpoint_default", 8 );
}

marker_clear()
{
	if ( isDefined( level.coop_training_marker ) )
		level.coop_training_marker destroy();
	level.coop_training_marker = undefined;
}

new_marker( origin, ent, height, shader, size )
{
	if ( !isDefined( height ) )
		height = 0;
	wp = newHudElem();
	if ( isDefined( ent ) )
	{
		wp.x = ent.origin[ 0 ];
		wp.y = ent.origin[ 1 ];
		wp.z = ent.origin[ 2 ] + height;
		wp setTargetEnt( ent );
	}
	else
	{
		wp.x = origin[ 0 ];
		wp.y = origin[ 1 ];
		wp.z = origin[ 2 ] + height;
	}
	wp.alpha = 0.9;
	wp.hidewheninmenu = true;
	wp setShader( shader, size, size );
	wp setWaypoint( true, true );
	return wp;
}

target_marker( key, origin )
{
	if ( !isDefined( level.coop_training_targets ) )
		level.coop_training_targets = [];
	target_marker_clear( key );
	level.coop_training_targets[ key ] = new_marker( origin, undefined, 0, "waypoint_target", 10 );
}

target_marker_clear( key )
{
	if ( !isDefined( level.coop_training_targets ) || !isDefined( level.coop_training_targets[ key ] ) )
		return;
	level.coop_training_targets[ key ] destroy();
	level.coop_training_targets[ key ] = undefined;
}

// --------------------------------------------------------------------------
// The course countdown (recording 4:47-4:49: "3...", "2...", "1..." in the
// hint place, then Mac's GO).
// --------------------------------------------------------------------------

countdown()
{
	texts = [];
	texts[ 0 ] = &"KILLHOUSE_OBSTACLE_COUNTDOWN_3";
	texts[ 1 ] = &"KILLHOUSE_OBSTACLE_COUNTDOWN_2";
	texts[ 2 ] = &"KILLHOUSE_OBSTACLE_COUNTDOWN_1";
	for ( i = 0; i < texts.size; i++ )
	{
		hint( texts[ i ], 1 );
		wait( 1 );
	}
	hint_clear();
}

// --------------------------------------------------------------------------
// The end (recording 5:41-5:50): after Price's last line the view blurs and
// darkens, then the lobby.
// --------------------------------------------------------------------------

end_fade()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		p = players[ i ];
		p setBlurForPlayer( 6, 2 );
		b = newClientHudElem( p );
		b.x = 0;
		b.y = 0;
		b.horzAlign = "fullscreen";
		b.vertAlign = "fullscreen";
		b.foreground = false;
		b.sort = -5;
		b.alpha = 0;
		b setShader( "black", 640, 480 );
		b fadeOverTime( 3 );
		b.alpha = 0.85;
	}
	codolprintln( "CoDOL coop training: end fade" );
}
