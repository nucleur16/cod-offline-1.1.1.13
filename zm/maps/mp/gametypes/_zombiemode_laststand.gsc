// zm downs, bleedout and revive.
//
// Port of T5's maps\_laststand.gsc (1,028 lines) plus the zombies-specific
// overrides in maps\_zombiemode.gsc:2140 player_laststand, :2025
// player_revive_monitor, :4818 wait_and_revive and :4700 the all-down rule.
//
// TAKEN, by T5 function (maps\_laststand.gsc unless noted):
//   player_is_in_laststand        :45
//   player_num_in_laststand       :51
//   player_all_players_in_laststand :68
//   PlayerLastStand               :80    -- the downs counter, the health-to-1
//                                           clamp, the revive trigger, the
//                                           weapon strip, the pistol, the
//                                           ignoreme flag and the bleedout.
//   laststand_disable_player_weapons :201
//   laststand_enable_player_weapons  :271
//   laststand_give_pistol         :326
//   Laststand_Bleedout            :366
//   revive_trigger_spawn          :431
//   revive_trigger_think          :452
//   revive_give_back_weapons      :529
//   can_revive                    :556   -- alive, not down, touching the
//                                           trigger, facing, sight and bullet
//                                           traces.
//   is_reviving                   :619
//   is_facing                     :627   -- getPlayerAngles, dot > 0.9.
//   revive_do_revive              :643   -- 3 s, halved by Quick Revive; the
//                                           reviver's progress bar and text,
//                                           the revivee's "being revived".
//   revive_success                :833
//   revive_hud_think / fadeReviveMessageOver :907/:961 -- the teammates'
//                                           "player down" notice.
//   _zombiemode.gsc:779            revive_trigger_radius 75.
//   _zombiemode.gsc:2025 player_revive_monitor -- the reviver is paid back
//                                           exactly what the downed player
//                                           lost (score_lost_when_downed).
//   _zombiemode.gsc:2140 player_laststand -- the points penalty, the offhand
//                                           disable, the perk teardown notify.
//   _zombiemode.gsc:4700           everyone down (and no solo life) ends the
//                                           game at once.
//   _zombiemode.gsc:4818 wait_and_revive -- the solo Quick Revive life: a
//                                           10 s self revive.
//
// THE DOWN ITSELF IS THE ENGINE'S.  C1 ability bit 23 (modPistolDeath) is
// published for every zm player (src/codol/player/codol_perk_runtime.c), so a
// lethal hit enters the native last stand: ps.pm_type 7, the 500 ms damage
// grace window, and Callback_PlayerLastStand -> player_laststand() here.
// revive_success calls laststandrevive(), which clears it again
// (src/codol/script/codol_script_laststand.c).
//
// WHY THE PROMPT IS FORCED, NOT A TRIGGER HINT.  T5's revive trigger is a
// trigger_radius whose hint string the T5 engine draws.  The retained cursor
// hint scan here (Player_UpdateCursorHints, src/asmsource/_player_use_mp.asm)
// only offers use triggers and usable entities, never a trigger_radius (the
// same finding _zombiemode_blockers.gsc records for the window repair), so the
// old setHintString on the revive trigger never reached any client: nobody
// near a downed teammate was told they could revive him.  The prompt is now
// pushed to each player who can revive through forceUseHintOn
// (src/codol/script/codol_script_usehint.c), exactly while can_revive holds.
//
// C1 DATA OVER T5 NAMES.  T5's GAME_* revive strings and syrette_sp are not
// in C1's zm data:
//   * prompt   &"SCRIPT_COOP_REVIVE"  "Hold [{+activate}] to revive"
//   * reviver  &"SCRIPT_COOP_REVIVING" "Reviving..."
//   * downed   &"SCRIPT_COOP_BLEEDING_OUT" "Bleeding Out"
//   * others   &"SCRIPT_COOP_BLEEDING_OUT_PARTNER" "Partner Down"
//     (all four in localized_common_gametype_zm, the zm gametype zone)
//   * revivee  &"MPUI_BEING_REVIVED"  (localized_code_post_gfx_mp)
//   * solo     &"ZOMBIE_REVIVING_SOLO" (localized_code_post_gfx_mp)
//   * icon     waypoint_revive (localized_common_mp), bars progress_bar_bg /
//              progress_bar_fill (common_mp): all startup zones.
//   * voice    <character>_whilerevivingteammate (localized_common_gametype_zm;
//              lb / danby / hh / lg, column 1 of mp/cyborgclasstable.csv).
//   syrette_sp ships in common_gametype_zm but is not in the zm weapon window
//   (docs/CODOL_CATEGORY_WEAPON_WINDOW_20260917.md), so the reviver's weapon
//   is lowered with disableWeapons for the hold instead, as C1's own
//   multiplayer revive does (maps/mp/gametypes/_damage.gsc useHoldThink).

#include maps\mp\gametypes\_zombiemode_utility;

REVIVE_BAR_WIDTH = 120;
REVIVE_BAR_HEIGHT = 9;
ROSTER_TABLE = "mp/cyborgclasstable.csv";

init()
{
	set_zombie_var( "zombie_bleedout_time", 45 );
	set_zombie_var( "zombie_solo_revive_time", 10 );
	set_zombie_var( "zombie_revive_time", 3 );
	set_zombie_var( "zombie_revive_trigger_radius", 75 );

	precacheString( &"SCRIPT_COOP_REVIVE" );
	precacheString( &"SCRIPT_COOP_REVIVING" );
	precacheString( &"SCRIPT_COOP_BLEEDING_OUT" );
	precacheString( &"SCRIPT_COOP_BLEEDING_OUT_PARTNER" );
	precacheString( &"MPUI_BEING_REVIVED" );
	precacheString( &"ZOMBIE_REVIVING_SOLO" );
	precacheShader( "waypoint_revive" );
	precacheShader( "progress_bar_bg" );
	precacheShader( "progress_bar_fill" );

	// T5 _zombiemode.gsc:653 level.laststandpistol.  The resolved zm starting
	// weapon is the honest choice here: it is the one pistol this client is
	// recorded as having accepted live.
	if ( !isDefined( level.laststandpistol ) )
		level.laststandpistol = level.zombie_start_weapon;

	level thread revive_hint_think();
}

// T5 :45.
player_is_in_laststand()
{
	return is_true( self.laststand );
}

// T5 :51.
player_num_in_laststand()
{
	players = getPlayers();
	num = 0;
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ] player_is_in_laststand() )
			num++;
	}
	return num;
}

// T5 :68.
player_all_players_in_laststand()
{
	players = getPlayers();
	if ( players.size < 1 )
		return false;

	for ( i = 0; i < players.size; i++ )
	{
		if ( !players[ i ] player_is_in_laststand() && isAlive( players[ i ] ) )
			return false;
	}
	return true;
}

// T5 :80 PlayerLastStand + _zombiemode.gsc:2140 player_laststand.
player_laststand()
{
	if ( self player_is_in_laststand() )
		return;

	if ( !isDefined( self.downs ) )
		self.downs = 0;
	self.downs++;

	self.laststand = true;
	self.health = 1;

	// T5's solo path has no teammate revive prompt.  Without a spare life,
	// a solo down is a wipe; Quick Revive supplies one spare life.
	players = getPlayers();
	solo = players.size == 1;
	spare_life = isDefined( self.pers ) && isDefined( self.pers[ "lives" ] ) && self.pers[ "lives" ] > 0;

	// _zombiemode.gsc:2172 -- the downed player pays penalty_downed and
	// remembers it in score_lost_when_downed for the reviver.
	self maps\mp\gametypes\_zombiemode_score::player_downed_penalty();

	// _zombiemode.gsc:2176.
	self disableOffhandWeapons();

	self allowSprint( false );
	self allowJump( false );
	self allowAds( false );
	self setMoveSpeedScale( 0.15 );
	self setStance( "prone" );

	// T5 :149 -- the AI stops caring about a downed player.
	// self.ignoreme is a plain SCRIPT variable here: no native field reads
	// it, so on its own zombies keep mobbing a downed player. setDontTarget
	// is the registered sentient method (codol_sentient.cpp:845) and
	// dontTarget IS honoured by the native acquisition pass, so the two are
	// paired. T5 gets the same effect through is_player_valid(p, true),
	// which drops last-stand players inside find_flesh.
	self.ignoreme = true;
	self setDontTarget( true );

	self laststand_disable_player_weapons();
	self laststand_give_pistol();

	// Anything that ends this down (revive, bleed-out, death, disconnect)
	// raises "laststand_over"; the HUD and the prompts are torn down there.
	self thread laststand_end_relay( "death" );
	self thread laststand_end_relay( "disconnect" );
	self thread laststand_end_cleanup();

	if ( !solo )
	{
		self revive_trigger_spawn();
		self thread laststand_partner_hud();
	}

	if ( solo && !spare_life )
	{
		self thread laststand_bleedout( 0 );
	}
	else if ( solo )
	{
		// _zombiemode.gsc:4818 wait_and_revive: ZOMBIE_REVIVING_SOLO over
		// the 10 s it takes, then the self revive.
		time = level.zombie_vars[ "zombie_solo_revive_time" ];
		self laststand_self_hud( &"ZOMBIE_REVIVING_SOLO", time, true );
		self thread laststand_bleedout( time );
	}
	else
	{
		time = level.zombie_vars[ "zombie_bleedout_time" ];
		self laststand_self_hud( &"SCRIPT_COOP_BLEEDING_OUT", time, false );
		self thread laststand_bleedout( time );
	}

	codolprintln( "CoDOL zm laststand: " + self.name + " down #" + self.downs + " (players " + players.size +
		", spare lives " + diag_val( self.pers[ "lives" ] ) + ", lost " + diag_val( self.score_lost_when_downed ) + ")" );

	// Every perk_think and every drink is waiting on this.
	self notify( "player_downed" );

	// _zombiemode.gsc:4700: when nobody is left standing the game is over at
	// once -- unless this is the solo player's Quick Revive life.
	if ( !( solo && spare_life ) )
		checkForAllDead();
}

laststand_end_relay( msg )
{
	self endon( "laststand_over" );
	self waittill( msg );
	self notify( "laststand_over" );
}

laststand_end_cleanup()
{
	self waittill( "laststand_over" );

	self laststand_hud_destroy();

	// Prompts aimed at this player go with him.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isDefined( players[ i ].zm_revive_hint_target ) && players[ i ].zm_revive_hint_target == self )
			players[ i ] revive_hint_set( undefined );
	}

	if ( isDefined( self.revivetrigger ) )
	{
		self.revivetrigger delete();
		self.revivetrigger = undefined;
	}
}

// T5 :201.  The downed player keeps nothing but the last-stand pistol.
laststand_disable_player_weapons()
{
	self.laststand_weapons = [];
	self.laststand_weapon_current = self getCurrentWeapon();

	weapons = self getWeaponsListPrimaries();
	for ( i = 0; i < weapons.size; i++ )
	{
		self.laststand_weapons[ self.laststand_weapons.size ] = weapons[ i ];
		self takeWeapon( weapons[ i ] );
	}
}

// T5 :271 / :529 revive_give_back_weapons.
laststand_enable_player_weapons()
{
	if ( isDefined( level.laststandpistol ) && self hasWeapon( level.laststandpistol ) )
	{
		// Only take it back when it was not the player's own gun to begin
		// with; T5 makes the same distinction at :539.
		keep = false;
		if ( isDefined( self.laststand_weapons ) )
		{
			for ( i = 0; i < self.laststand_weapons.size; i++ )
			{
				if ( self.laststand_weapons[ i ] == level.laststandpistol )
					keep = true;
			}
		}

		if ( !keep )
			self takeWeapon( level.laststandpistol );
	}

	if ( isDefined( self.laststand_weapons ) )
	{
		for ( i = 0; i < self.laststand_weapons.size; i++ )
		{
			self giveWeapon( self.laststand_weapons[ i ] );
			self giveMaxAmmo( self.laststand_weapons[ i ] );
		}
	}

	if ( isDefined( self.laststand_weapon_current ) && self.laststand_weapon_current != "none" &&
		self hasWeapon( self.laststand_weapon_current ) )
	{
		self switchToWeapon( self.laststand_weapon_current );
	}

	self.laststand_weapons = undefined;
	self.laststand_weapon_current = undefined;
}

// T5 :326.
laststand_give_pistol()
{
	if ( !isDefined( level.laststandpistol ) )
		return;

	if ( !self hasWeapon( level.laststandpistol ) )
		self giveWeapon( level.laststandpistol );

	self giveMaxAmmo( level.laststandpistol );
	self switchToWeapon( level.laststandpistol );
}

// T5 :366.
laststand_bleedout( delay )
{
	self endon( "player_revived" );
	self endon( "disconnect" );
	// The no-spare-life solo case starts inside the native lethal-hit
	// callback.  Let that callback unwind before forcing the death transition.
	if ( delay <= 0 )
		wait( 0.05 );

	self.bleedout_time = delay;

	while ( self.bleedout_time > 0 )
	{
		self.bleedout_time -= 1;
		wait( 1 );
	}

	// T5 :404 -- do not bleed out mid-revive.
	while ( isDefined( self.revivetrigger ) && is_true( self.revivetrigger.beingRevived ) )
		wait( 0.1 );

	self notify( "bled_out" );

	// T5 hands the bled-out player to level.player_becomes_zombie on a zombies
	// level.  The playable-zombie mode (_zombiemode.gsc:4187 zombify_player)
	// is not ported -- it is animation-first -- so the solo-life rule is used
	// instead: quick revive's extra life self-revives (wait_and_revive),
	// otherwise the player dies and the game-over check runs.
	if ( isDefined( self.pers ) && isDefined( self.pers[ "lives" ] ) && self.pers[ "lives" ] > 0 &&
		getPlayers().size == 1 )
	{
		self.pers[ "lives" ]--;
		codolprintln( "CoDOL zm laststand: " + self.name + " solo self revive, " + self.pers[ "lives" ] + " spare lives left" );
		self revive_success( undefined );
		return;
	}

	self laststand_die();
}

laststand_die()
{
	codolprintln( "CoDOL zm laststand: " + self.name + " bled out" );

	self.laststand = false;
	self laststandrevive();
	self notify( "laststand_over" );

	// The other players are penalised for not reviving.  T5
	// _zombiemode_score.gsc:222, from zombify_player.
	self maps\mp\gametypes\_zombiemode_score::player_died_penalty();

	self.ignoreme = false;
	self setDontTarget( false );
	self suicide();

	level notify( "player_bled_out", self );
}

// T5 :431.  The trigger is the revive volume (can_revive's isTouching, and
// in_revive_trigger for the window repair); its hint is never drawn, see the
// header.
revive_trigger_spawn()
{
	radius = level.zombie_vars[ "zombie_revive_trigger_radius" ];

	self.revivetrigger = spawn( "trigger_radius", self.origin, 0, radius, radius );
	self.revivetrigger enableLinkTo();
	self.revivetrigger linkTo( self );
	self.revivetrigger.beingRevived = 0;
	self.revivetrigger.createtime = getTime();

	self thread revive_trigger_think();
}

// T5 :452.  self is the downed player.
revive_trigger_think()
{
	self endon( "disconnect" );
	self endon( "player_revived" );
	self endon( "bled_out" );
	self endon( "laststand_over" );

	for ( ;; )
	{
		wait( 0.05 );

		if ( !isDefined( self.revivetrigger ) )
			return;

		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			reviver = players[ i ];
			if ( !reviver is_reviving( self ) )
				continue;
			if ( isDefined( reviver.zm_reviving ) )
				continue;

			revived = self revive_attempt( reviver );
			if ( revived )
			{
				self thread revive_success( reviver );
				return;
			}
		}
	}
}

// One hold.  The reviver's half runs in his own thread, so either player
// leaving or going down mid-hold cleans up only what belongs to him.
revive_attempt( reviver )
{
	revive_time = level.zombie_vars[ "zombie_revive_time" ];
	if ( reviver hasPerk( "specialty_quickrevive" ) )
		revive_time = revive_time / 2;

	reviver.zm_reviving = self;
	self.zm_revive_result = false;
	self.revivetrigger.beingRevived = 1;

	codolprintln( "CoDOL zm laststand: " + reviver.name + " reviving " + self.name + " (" + revive_time + " s)" );

	self laststand_being_revived_hud( revive_time );
	reviver thread revive_do_revive( self, revive_time );

	while ( isDefined( reviver ) && isDefined( reviver.zm_reviving ) )
		wait( 0.05 );

	self laststand_being_revived_hud_destroy();
	if ( isDefined( self.revivetrigger ) )
		self.revivetrigger.beingRevived = 0;

	if ( !self.zm_revive_result )
		codolprintln( "CoDOL zm laststand: revive of " + self.name + " cancelled" );

	return self.zm_revive_result;
}

// T5 :643.  self is the reviver.
revive_do_revive( revivee, revive_time )
{
	self endon( "disconnect" );

	self revive_hint_set( undefined );
	self disableWeapons();
	self revive_vox();

	bar = self revive_bar_create( -61 );
	bar revive_bar_fill( 0, 1, revive_time );
	text = self revive_text_create( -75, &"SCRIPT_COOP_REVIVING" );

	timer = 0;
	revived = false;
	for ( ;; )
	{
		wait( 0.05 );
		if ( !isDefined( revivee ) || !revivee player_is_in_laststand() )
			break;
		if ( !self is_reviving( revivee ) )
			break;

		timer += 0.05;
		if ( timer >= revive_time )
		{
			revived = true;
			break;
		}
	}

	bar revive_bar_destroy();
	if ( isDefined( text ) )
		text destroy();

	// T5 revive_give_back_weapons; also when he went down mid-hold, or his
	// last-stand pistol would stay lowered.
	self enableWeapons();

	if ( isDefined( revivee ) )
		revivee.zm_revive_result = revived;
	self.zm_reviving = undefined;
}

// T5 :556.  revivee is the downed player.
can_revive( revivee )
{
	if ( !isDefined( revivee ) || !isDefined( revivee.revivetrigger ) )
		return false;

	if ( self == revivee )
		return false;

	if ( !is_player_valid( self ) )
		return false;

	// The perk bottle is in his hands (T5 makes the same exclusion through
	// the syrette swap, :494).
	if ( is_true( self.is_drinking ) )
		return false;

	if ( !self isTouching( revivee.revivetrigger ) )
		return false;

	if ( !self is_facing( revivee ) )
		return false;

	if ( !sightTracePassed( self.origin + ( 0, 0, 50 ), revivee.origin + ( 0, 0, 30 ), false, undefined ) )
		return false;

	// T5 :608 -- no reviving through a wall.
	if ( !bulletTracePassed( self.origin + ( 0, 0, 50 ), revivee.origin + ( 0, 0, 30 ), false, undefined ) )
		return false;

	return true;
}

// T5 :619.
is_reviving( revivee )
{
	return self can_revive( revivee ) && self useButtonPressed();
}

// T5 :627 -- a ~52-degree cone.
is_facing( facee )
{
	forward = anglesToForward( self getPlayerAngles() );
	to_facee = facee.origin - self.origin;

	forward = vectorNormalize( ( forward[ 0 ], forward[ 1 ], 0 ) );
	to_facee = vectorNormalize( ( to_facee[ 0 ], to_facee[ 1 ], 0 ) );

	return vectorDot( forward, to_facee ) > 0.9;
}

// T5 :833 + _zombiemode.gsc:2025 player_revive_monitor.  reviver is undefined
// for a self-revive (solo quick revive) and for the all-revive power-ups.
revive_success( reviver )
{
	self.laststand = false;
	self.ignoreme = false;
	self setDontTarget( false );

	// See the header: clears the engine's last stand.
	self laststandrevive();

	self.health = self.maxhealth;

	self allowSprint( true );
	self allowJump( true );
	self allowAds( true );
	self setMoveSpeedScale( 1 );
	self setStance( "stand" );
	self enableOffhandWeapons();

	self laststand_enable_player_weapons();

	if ( isDefined( self.revivetrigger ) )
	{
		self.revivetrigger delete();
		self.revivetrigger = undefined;
	}

	if ( isDefined( reviver ) && reviver != self )
	{
		// player_revive_monitor: the reviver gets back what the downed player
		// lost going down.
		points = 0;
		if ( isDefined( self.score_lost_when_downed ) )
			points = self.score_lost_when_downed;
		reviver maps\mp\gametypes\_zombiemode_score::player_add_points( "reviver", points );
		self.score_lost_when_downed = 0;

		if ( !isDefined( reviver.revives ) )
			reviver.revives = 0;
		reviver.revives++;

		codolprintln( "CoDOL zm laststand: " + reviver.name + " revived " + self.name + " (+" + points + ", " + reviver.revives + " revives)" );
	}
	else
	{
		codolprintln( "CoDOL zm laststand: " + self.name + " back up" );
	}

	// The health regen thread ends on "player_downed"; a revived player
	// regenerates again, as in T5.
	self.zmLastDamageTime = getTime();
	self notify( "zm_regen_restart" );
	self thread maps\mp\gametypes\_zombiemode::zm_player_health_regen();

	// "laststand_over" first: the solo self revive runs inside the bleed-out
	// thread, which "player_revived" ends.
	self notify( "laststand_over" );
	self notify( "player_revived", reviver );
}

// T5 _zombiemode.gsc:1567 checkForAllDead, called from the kill callback and
// from player_laststand.
checkForAllDead()
{
	players = getPlayers();
	if ( players.size < 1 )
		return;

	for ( i = 0; i < players.size; i++ )
	{
		if ( isAlive( players[ i ] ) && !players[ i ] player_is_in_laststand() )
			return;
	}

	codolprintln( "CoDOL zm laststand: no player left standing; game over" );
	level notify( "end_game" );
}

// --------------------------------------------------------------------------
// The revive prompt.  One level thread: every player who can revive someone
// gets &"SCRIPT_COOP_REVIVE" forced on, and loses it the moment he cannot.
// --------------------------------------------------------------------------

revive_hint_think()
{
	level endon( "end_game" );

	for ( ;; )
	{
		wait( 0.05 );

		players = getPlayers();
		if ( players.size < 2 )
			continue;

		downed = [];
		for ( i = 0; i < players.size; i++ )
		{
			if ( players[ i ] player_is_in_laststand() && isDefined( players[ i ].revivetrigger ) )
				downed[ downed.size ] = players[ i ];
		}

		for ( i = 0; i < players.size; i++ )
		{
			player = players[ i ];
			target = undefined;
			if ( !isDefined( player.zm_reviving ) )
			{
				for ( j = 0; j < downed.size; j++ )
				{
					if ( player can_revive( downed[ j ] ) )
					{
						target = downed[ j ];
						break;
					}
				}
			}
			player revive_hint_set( target );
		}
	}
}

// self = a player; target is the downed player he is prompted to revive, or
// undefined for no prompt.
revive_hint_set( target )
{
	if ( isDefined( target ) )
	{
		if ( isDefined( self.zm_revive_hint_target ) && self.zm_revive_hint_target == target )
			return;
		self forceUseHintOn( &"SCRIPT_COOP_REVIVE" );
		self.zm_revive_hint_target = target;
		self.zm_revive_hint_on = true;
		return;
	}

	if ( !is_true( self.zm_revive_hint_on ) && !isDefined( self.zm_revive_hint_target ) )
		return;
	self forceUseHintOff();
	self.zm_revive_hint_target = undefined;
	self.zm_revive_hint_on = undefined;
}

// --------------------------------------------------------------------------
// HUD.  C1's own progress bar shape (maps/mp/gametypes/_hud.gsc:105-111,
// _hud_util.gsc createBar): progress_bar_bg behind progress_bar_fill, 120 x 9
// at -61 from centre, its text at -75.  Built directly on newClientHudElem
// because the multiplayer _hud_util chain is kept out of a zm compile.
// --------------------------------------------------------------------------

revive_bar_create( y )
{
	bg = newClientHudElem( self );
	bg.alignX = "center";
	bg.alignY = "middle";
	bg.horzAlign = "center_adjustable";
	bg.vertAlign = "middle";
	bg.x = 0;
	bg.y = y;
	bg.sort = -3;
	bg.color = ( 0, 0, 0 );
	bg.alpha = 0.5;
	bg.foreground = true;
	bg.hidewheninmenu = true;
	bg setShader( "progress_bar_bg", REVIVE_BAR_WIDTH + 4, REVIVE_BAR_HEIGHT + 4 );

	fill = newClientHudElem( self );
	fill.alignX = "left";
	fill.alignY = "middle";
	fill.horzAlign = "center_adjustable";
	fill.vertAlign = "middle";
	fill.x = 0 - int( REVIVE_BAR_WIDTH / 2 );
	fill.y = y;
	fill.sort = -2;
	fill.color = ( 1, 1, 1 );
	fill.alpha = 1;
	fill.foreground = true;
	fill.hidewheninmenu = true;
	bg.bar = fill;

	return bg;
}

// self = the bar.  Fills (or drains) from one fraction to another over time.
revive_bar_fill( from, to, time )
{
	if ( !isDefined( self ) || !isDefined( self.bar ) )
		return;

	start = int( REVIVE_BAR_WIDTH * from + 0.5 );
	if ( start < 1 )
		start = 1;
	end = int( REVIVE_BAR_WIDTH * to + 0.5 );
	if ( end < 1 )
		end = 1;

	self.bar setShader( "progress_bar_fill", start, REVIVE_BAR_HEIGHT );
	if ( time > 0 && start != end )
		self.bar scaleOverTime( time, end, REVIVE_BAR_HEIGHT );
}

revive_bar_destroy()
{
	if ( !isDefined( self ) )
		return;
	if ( isDefined( self.bar ) )
		self.bar destroy();
	self destroy();
}

revive_text_create( y, text )
{
	elem = newClientHudElem( self );
	elem.alignX = "center";
	elem.alignY = "middle";
	elem.horzAlign = "center_adjustable";
	elem.vertAlign = "middle";
	elem.x = 0;
	elem.y = y;
	elem.sort = -1;
	elem.font = "hudbig";
	elem.fontScale = 0.6;
	elem.color = ( 1, 1, 1 );
	elem.alpha = 1;
	elem.foreground = true;
	elem.hidewheninmenu = true;
	elem setText( text );
	return elem;
}

// self = the downed player.  "Bleeding Out" (or the solo "Reviving") and a
// bar that runs over the remaining time.  fill = true counts up (the solo
// self revive), false drains (the bleed-out).
laststand_self_hud( text, time, fill )
{
	self laststand_self_hud_destroy();

	self.zm_laststand_text = self revive_text_create( 56, text );
	self.zm_laststand_text.color = ( 1, 0.25, 0.25 );
	self.zm_laststand_bar = self revive_bar_create( 70 );
	self.zm_laststand_bar.bar.color = ( 1, 0.25, 0.25 );
	if ( fill )
		self.zm_laststand_bar revive_bar_fill( 0, 1, time );
	else
		self.zm_laststand_bar revive_bar_fill( 1, 0, time );
}

laststand_self_hud_destroy()
{
	if ( isDefined( self.zm_laststand_text ) )
		self.zm_laststand_text destroy();
	self.zm_laststand_text = undefined;
	if ( isDefined( self.zm_laststand_bar ) )
		self.zm_laststand_bar revive_bar_destroy();
	self.zm_laststand_bar = undefined;
}

// self = the downed player, while someone holds the revive.
laststand_being_revived_hud( time )
{
	self laststand_being_revived_hud_destroy();

	self.zm_revived_bar = self revive_bar_create( -61 );
	self.zm_revived_bar revive_bar_fill( 0, 1, time );
	self.zm_revived_text = self revive_text_create( -75, &"MPUI_BEING_REVIVED" );
}

laststand_being_revived_hud_destroy()
{
	if ( isDefined( self.zm_revived_text ) )
		self.zm_revived_text destroy();
	self.zm_revived_text = undefined;
	if ( isDefined( self.zm_revived_bar ) )
		self.zm_revived_bar revive_bar_destroy();
	self.zm_revived_bar = undefined;
}

// T5 :907 revive_hud_think + :961 fadeReviveMessageOver, and the revive icon
// over the downed player for everyone else: waypoint_revive, as C1's own
// multiplayer revive draws it (_damage.gsc lastStandTimer), turning from
// white to red as the bleed-out runs.
laststand_partner_hud()
{
	self endon( "laststand_over" );

	self.zm_revive_icons = [];
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		player = players[ i ];
		if ( player == self )
			continue;

		icon = newClientHudElem( player );
		icon setShader( "waypoint_revive", 8, 8 );
		icon setWaypoint( true, true );
		icon setTargetEnt( self );
		icon.color = ( 1, 1, 1 );
		icon.alpha = 1;
		icon.hidewheninmenu = true;
		self.zm_revive_icons[ self.zm_revive_icons.size ] = icon;

		if ( player player_is_in_laststand() )
			continue;

		notice = player revive_text_create( 100, &"SCRIPT_COOP_BLEEDING_OUT_PARTNER" );
		notice fadeOverTime( 3 );
		notice.alpha = 0;
		player thread revive_notice_expire( notice, 3 );
	}

	total = level.zombie_vars[ "zombie_bleedout_time" ];
	for ( ;; )
	{
		wait( 1 );
		if ( !isDefined( self.bleedout_time ) || total <= 0 )
			continue;
		frac = self.bleedout_time / total;
		if ( frac < 0 )
			frac = 0;
		for ( i = 0; i < self.zm_revive_icons.size; i++ )
		{
			if ( isDefined( self.zm_revive_icons[ i ] ) )
				self.zm_revive_icons[ i ].color = ( 1, frac, frac );
		}
	}
}

revive_notice_expire( notice, time )
{
	wait( time );
	if ( isDefined( notice ) )
		notice destroy();
}

laststand_hud_destroy()
{
	self laststand_self_hud_destroy();
	self laststand_being_revived_hud_destroy();

	if ( isDefined( self.zm_revive_icons ) )
	{
		for ( i = 0; i < self.zm_revive_icons.size; i++ )
		{
			if ( isDefined( self.zm_revive_icons[ i ] ) )
				self.zm_revive_icons[ i ] destroy();
		}
	}
	self.zm_revive_icons = undefined;
}

// T5 _zombiemode.gsc:2042 has the reviver speak; C1 ships each character's
// line for exactly that, <code>_whilerevivingteammate.
revive_vox()
{
	code = self revive_vox_code();
	if ( !isDefined( code ) )
		return;
	if ( isDefined( self.zm_revive_vox_time ) && getTime() - self.zm_revive_vox_time < 10000 )
		return;
	self.zm_revive_vox_time = getTime();
	self playSound( code + "_whilerevivingteammate" );
}

// The character code (column 1 of the roster, lb / danby / hh / lg) of this
// player's zm character, which _zombiemode_equipment keys by column 0.
revive_vox_code()
{
	if ( !isDefined( self.zombie_character ) )
		return undefined;

	for ( row = 0; row < 16; row++ )
	{
		key = maps\mp\gametypes\_zombiemode_equipment::trim( tableLookupByRow( ROSTER_TABLE, row, 0 ) );
		if ( key == "" )
			break;
		if ( key != self.zombie_character )
			continue;
		code = maps\mp\gametypes\_zombiemode_equipment::trim( tableLookupByRow( ROSTER_TABLE, row, 1 ) );
		if ( code == "" )
			return undefined;
		return code;
	}
	return undefined;
}
