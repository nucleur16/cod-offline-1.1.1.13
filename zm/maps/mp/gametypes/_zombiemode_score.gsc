// zm points economy.
//
// Port of T5's maps\_zombiemode_score.gsc (728 lines).  Every scoring rule,
// every zombie_vars row it reads and the round_up_score(points, 5) rounding
// are the T5 ones; the differences are listed at the bottom of this comment.
//
// TAKEN, by T5 function:
//   player_add_points              :10   (the "death"/"damage"/"damage_ads"/
//                                        "damage_light"/"rebuild_board"/
//                                        "bonus_points_powerup"/"nuke_powerup"
//                                        /"reviver" events)
//   get_points_multiplier          :137
//   get_zombie_death_player_points :148
//   get_zombie_death_team_points   :172
//   player_add_points_kill_bonus   :222
//   player_reduce_points           :256
//   add_to_player_score            :302
//   minus_to_player_score          :331
//   player_died_penalty            :412
//   player_downed_penalty          :430
//   set_player_score_hud           :447
//   create_highlight_hud           :497
//   score_highlight                :540
//
// NOT TAKEN, and why:
//   team_score_init / add_to_team_score / minus_to_team_score / set_team_score
//   _hud / create_team_hud -- T5 itself disabled the team pot on 2010-03-10
//   ("MM (3/10/10) Disable team points"): add_to_team_score's body is entirely
//   commented out and set_team_score_hud zeroes the pool.  Porting a dead pot
//   would mean porting level.team_pool and player.team_num purely so that the
//   door and debris purchase paths could read a value that is always zero.
//   Those paths are ported solo-buy-only instead, which is the branch T5 takes
//   whenever get_players().size == 1 and the only branch that ever moves
//   points in T5 as shipped.
//   play_killstreak_vo             -- commented out in T5.
//   "ballistic_knife_death" / "thundergun_fling" / "hacker_transfer" /
//   "carpenter_powerup" events -- those weapons and power-ups are not ported.
//
// The scoring HUD is a server hudelem, not a client dvar.  MEASURED: C1's only
// zombies HUD element is ZOMBIE_ROUND (dvar ui_hud_round, renderer
// 0x00461A71); an exhaustive sweep of all 23,124 C1 strings found no points
// widget at all (alpha0022/docs/zombies_mode.md:1088-1148).  So the score
// readout and T5's floating "+50" both have to be drawn from here.

#include maps\mp\gametypes\_zombiemode_utility;

// --------------------------------------------------------------------------
// T5 _zombiemode_score.gsc:10.  self is the scoring player.
// --------------------------------------------------------------------------

player_add_points( event, mod, hit_location )
{
	if ( is_true( level.intermission ) )
		return;

	if ( !is_player_valid( self ) )
		return;

	player_points = 0;
	multiplier = get_points_multiplier();

	switch( event )
	{
	case "death":
		player_points = get_zombie_death_player_points();
		points = player_add_points_kill_bonus( mod, hit_location );

		if ( is_true( level.zombie_vars[ "zombie_powerup_insta_kill_on" ] ) && mod == "MOD_UNKNOWN" )
			points = points * 2;

		player_points = player_points + points;

		if ( isDefined( self.kill_tracker ) )
			self.kill_tracker++;
		else
			self.kill_tracker = 1;
		break;

	case "damage_light":
		player_points = level.zombie_vars[ "zombie_score_damage_light" ];
		break;

	case "damage":
		player_points = level.zombie_vars[ "zombie_score_damage_normal" ];
		break;

	case "damage_ads":
		player_points = int( level.zombie_vars[ "zombie_score_damage_normal" ] * 1.25 );
		break;

	case "rebuild_board":
	case "bonus_points_powerup":
	case "nuke_powerup":
	case "reviver":
		player_points = mod;
		break;

	default:
		// T5 assertex( 0, "Unknown point event" ).  An assert here would end
		// the match on a typo in a caller; an unknown event is worth nothing
		// and is reported once instead.
		println( "CoDOL zm: unknown point event " + event );
		return;
	}

	player_points = multiplier * round_up_score( player_points, 5 );

	self add_to_player_score( player_points );
}

get_points_multiplier()
{
	multiplier = level.zombie_vars[ "zombie_point_scalar" ];
	if ( !isDefined( multiplier ) )
		multiplier = 1;
	return multiplier;
}

// T5 :148 / :172.  Adjust the per-kill award by player count.
get_zombie_death_player_points()
{
	players = getPlayers();
	if ( players.size == 1 )
		return level.zombie_vars[ "zombie_score_kill_1player" ];
	if ( players.size == 2 )
		return level.zombie_vars[ "zombie_score_kill_2player" ];
	if ( players.size == 3 )
		return level.zombie_vars[ "zombie_score_kill_3player" ];
	return level.zombie_vars[ "zombie_score_kill_4player" ];
}

// T5 :222.
player_add_points_kill_bonus( mod, hit_location )
{
	if ( isDefined( mod ) && mod == "MOD_MELEE" )
		return level.zombie_vars[ "zombie_score_bonus_melee" ];

	if ( isDefined( mod ) && mod == "MOD_BURNED" )
		return level.zombie_vars[ "zombie_score_bonus_burn" ];

	score = 0;

	if ( !isDefined( hit_location ) )
		return score;

	switch( hit_location )
	{
	case "head":
	case "helmet":
		score = level.zombie_vars[ "zombie_score_bonus_head" ];
		break;

	case "neck":
		score = level.zombie_vars[ "zombie_score_bonus_neck" ];
		break;

	case "torso_upper":
	case "torso_lower":
		score = level.zombie_vars[ "zombie_score_bonus_torso" ];
		break;
	}

	return score;
}

// T5 :256.  The three penalty events are percentages of the CURRENT purse.
player_reduce_points( event )
{
	if ( is_true( level.intermission ) )
		return;

	points = 0;

	switch( event )
	{
	case "no_revive_penalty":
		points = self.score * level.zombie_vars[ "penalty_no_revive" ];
		break;

	case "died":
		points = self.score * level.zombie_vars[ "penalty_died" ];
		break;

	case "downed":
		points = self.score * level.zombie_vars[ "penalty_downed" ];
		self notify( "I_am_down" );
		self.score_lost_when_downed = round_up_to_ten( int( points ) );
		break;

	default:
		println( "CoDOL zm: unknown point penalty " + event );
		return;
	}

	points = self.score - round_up_to_ten( int( points ) );
	if ( points < 0 )
		points = 0;

	self.score = points;
	self set_player_score_hud();
}

// T5 :302.  add_to_total false is the magic-box refund, which returns the
// spend without crediting the lifetime total the scoreboard reads.
add_to_player_score( points, add_to_total )
{
	if ( !isDefined( add_to_total ) )
		add_to_total = true;

	if ( !isDefined( points ) || is_true( level.intermission ) )
		return;

	self.score += points;

	if ( add_to_total )
		self.score_total += points;

	self set_player_score_hud();
}

// T5 :331.
minus_to_player_score( points )
{
	if ( !isDefined( points ) || is_true( level.intermission ) )
		return;

	self.score -= points;
	if ( self.score < 0 )
		self.score = 0;

	self set_player_score_hud();
}

// T5 :412.  Everyone who did not revive the dead player pays.
player_died_penalty()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] == self )
			continue;
		players[ i ] player_reduce_points( "no_revive_penalty" );
	}
}

// T5 :430.
player_downed_penalty()
{
	self player_reduce_points( "downed" );
}

// --------------------------------------------------------------------------
// HUD.  T5 :447 set_player_score_hud / :497 create_highlight_hud /
// :540 score_highlight.
//
// T5 draws every player's purse in every viewport (a lobby of four shares one
// screen in splitscreen).  This server has no splitscreen and C1's zm HUD has
// no points widget, so each player gets their own persistent readout plus the
// floating delta, both as client hudelems owned by that player.
// --------------------------------------------------------------------------

score_hud_init()
{
	self.score_hud = create_simple_hud( self );
	self.score_hud.alignX = "right";
	self.score_hud.alignY = "middle";
	self.score_hud.horzAlign = "user_right";
	self.score_hud.vertAlign = "user_bottom";
	self.score_hud.x = -103;
	self.score_hud.y = -71;
	self.score_hud.fontScale = 12;
	self.score_hud.color = ( 0.9, 0.9, 0.0 );
	self.score_hud.alpha = 1;
	self.score_hud.sort = 10;
	self.score_hud setValue( self.score );

	self.old_score = self.score;
}

set_player_score_hud( init )
{
	if ( !isDefined( self.old_score ) )
		self.old_score = self.score;

	score_diff = self.score - self.old_score;

	if ( isDefined( self.score_hud ) )
		self.score_hud setValue( self.score );

	if ( score_diff != 0 && !is_true( init ) )
		self thread score_highlight( score_diff );

	self.old_score = self.score;
}

// T5 :497.  Yellow "+50" for a gain, dark red for a loss.
create_highlight_hud( x, y, value )
{
	hud = create_simple_hud( self );

	hud.foreground = true;
	hud.sort = 0;
	hud.x = x;
	hud.y = y;
	hud.fontScale = 8;
	hud.alignX = "right";
	hud.alignY = "middle";
	hud.horzAlign = "user_right";
	hud.vertAlign = "user_bottom";
	hud.hidewheninmenu = false;

	if ( value < 1 )
		hud.color = ( 0.21, 0, 0 );
	else
		hud.color = ( 0.9, 0.9, 0.0 );

	hud setValue( value );

	return hud;
}

// T5 :540, minus the splitscreen row arithmetic.
score_highlight( value )
{
	self endon( "disconnect" );

	// T5 lets the hudelem budget run free; this server shares one budget with
	// everything else the match draws, so a burst of hits does not get to
	// starve the round counter.
	if ( isDefined( level.hudelem_count ) && level.hudelem_count > 24 )
		return;

	time = 0.5;
	half_time = time * 0.5;

	hud = self create_highlight_hud( -103, -100, value );

	hud moveOverTime( time );
	hud.x -= 20 + randomInt( 40 );
	hud.y -= ( -15 + randomInt( 30 ) );

	wait( half_time );

	hud fadeOverTime( half_time );
	hud.alpha = 0;

	wait( half_time );

	hud destroy_hud();
}
