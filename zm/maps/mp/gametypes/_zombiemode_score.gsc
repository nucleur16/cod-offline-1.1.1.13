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
// C1's live Zombies HUD displays the purse and point changes beside the player
// name.  T5's floating delta hudelem is intentionally omitted here.

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

	// DIAGNOSTIC 2026-09-20.  A running ledger of what the economy actually
	// pays out, so "is the score tracked?" is answerable from one live log
	// line instead of inferred from hudelem error stacks.  The first award of
	// each event type prints, then every 25th.
	if ( !isDefined( level.zm_award_count ) )
		level.zm_award_count = [];

	if ( !isDefined( level.zm_award_count[ event ] ) )
		level.zm_award_count[ event ] = 0;

	level.zm_award_count[ event ]++;

	if ( level.zm_award_count[ event ] == 1 || level.zm_award_count[ event ] % 25 == 0 )
	{
		println( "CoDOL zm DIAG award: event=" + event +
			" points=" + player_points +
			" count=" + level.zm_award_count[ event ] +
			" purse=" + self.score );
	}

	self add_to_player_score( player_points );
}

get_points_multiplier()
{
	multiplier = level.zombie_vars[ "zombie_point_scalar" ];
	if ( !isDefined( multiplier ) )
		multiplier = 1;
	if ( is_true( level.zombie_vars[ "zombie_powerup_point_doubler_on" ] ) )
		multiplier = multiplier * 2;
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

	// CORRECTED 2026-09-20.  T5 gets a hitLocation_t name ("head", "neck",
	// "torso_upper") because its Callback_ActorDamage is handed sHitLoc.
	// This server has no actor damage callback; the award is driven off the
	// "damage" notify, whose 8th argument is partName -- the BONE const
	// string from the bullet trace, not the hit location
	// (MEASURED: src/asmsource/_g_combat_mp.asm pushes
	// Scr_AddConstString([ebp+0x34]) for partName, and src/asmsource/_bullet.asm
	// fills that slot from the trace's bone index; hitLoc itself is never
	// pushed).  So the cases that actually fire are the zombie skeleton's
	// bone names -- MEASURED from a bone dump of the shipped zm fastfiles:
	// every c_zom_*_body carries j_head, j_neck, j_spine4, j_spineupper,
	// j_spinelower, j_helmet among its 73 bones.
	//
	// The T5 hitLocation names are kept as accepted aliases so that a caller
	// which does supply one still scores; they simply never arrive today.
	switch( hit_location )
	{
	case "j_head":
	case "j_head_end":
	case "j_helmet":
	case "head":
	case "helmet":
		score = level.zombie_vars[ "zombie_score_bonus_head" ];
		break;

	case "j_neck":
	case "neck":
		score = level.zombie_vars[ "zombie_score_bonus_neck" ];
		break;

	case "j_spine4":
	case "j_spineupper":
	case "j_spinelower":
	case "j_clavicle_le":
	case "j_clavicle_ri":
	case "j_mainroot":
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

	// Every caller is a purchase; the discount perk pop refunds from this.
	self notify( "zm_spent_points", points );
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
// C1 draws the purse and point changes in its own Zombies HUD.  Keep this
// callable for score paths, but do not create a second floating number.
// --------------------------------------------------------------------------

set_player_score_hud( init )
{
	self.old_score = self.score;
}
