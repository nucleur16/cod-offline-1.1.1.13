// common_scripts/_laststand.gsc -- downs, bleed-out and revive, shared by
// every CoDOL mode that has them (zm today; coop when it opts in).
//
// WHY A SHARED MODULE.  T5 keeps this machine in ONE file, maps\_laststand.gsc,
// that both its campaign co-op (maps\_load) and Zombies (maps\_zombiemode)
// run; the mode only overrides a few hooks (_zombiemode.gsc player_laststand,
// player_revive_monitor, wait_and_revive).  CoDOL ships the evidence that it
// did the same: localized_common_gametype_coop.ff and
// localized_common_gametype_zm.ff carry the IDENTICAL seven revive strings,
// SCRIPT_COOP_REVIVE / _REVIVING / _REVIVING_PARTNER / _BLEEDING_OUT /
// _BLEEDING_OUT_PARTNER / _PICKUP / _DROP (docs/zone_assets), which are IW4
// maps\_coop.gsc's revive strings.  So one revive layer, two modes.
//
// PROVENANCE.  This is zm/maps/mp/gametypes/_zombiemode_laststand.gsc (the
// port of T5 maps\_laststand.gsc, 1,028 lines, plus the zombies overrides in
// T5 maps\_zombiemode.gsc:2025/2140/4700/4818) with every zombies-only call
// moved behind a hook.  Line references are into
// D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\_laststand.gsc unless named.
// Behaviour with zm's hooks installed is the zm module's, statement for
// statement; see _zombiemode_laststand.gsc for the zm hooks.
//
// THE DOWN ITSELF IS THE ENGINE'S.  A lethal hit on a player who carries C1
// ability 8 "modPistolDeath" (ps+0x4d0 bit 23) enters the native last stand:
// ps.pm_type 7, the 500 ms damage grace and CodeCallback_PlayerLastStand
// (src/codol/script/codol_script_laststand.c CODOL_LastStand_CanEnter).  zm
// grants that ability to every player natively (codol_perk_runtime.c
// apply_category_ability_floor); any other mode grants it per player with
// enable_player() below, which is the registered setAbilityModifier.
// revive_success() calls laststandrevive(), which clears the native state.
//
// NAMES MODES READ.  self.laststand, self.revivetrigger, self.downs,
// self.bleedout_time, self.laststand_weapons and the notifies "player_downed",
// "player_revived" (reviver), "bled_out", "laststand_over" and level
// "player_bled_out" are T5's and are kept, because mode scripts wait on them.
//
// NO DEPENDENCIES.  No #include and no mode module named: a mode configures
// this file, never the reverse, so any category can compile it alone.
//
// CONFIGURATION (set before init(); every key has a default):
//   level.laststand_vars[ "bleedout_time" ]      45   (T5 zombie_bleedout_time)
//   level.laststand_vars[ "solo_revive_time" ]   10   (T5 zombie_solo_revive_time)
//   level.laststand_vars[ "revive_time" ]         3   (T5 zombie_revive_time)
//   level.laststand_vars[ "trigger_radius" ]     75   (T5 _zombiemode.gsc:779)
//   level.laststand_vars[ "log" ]       "CoDOL laststand"  log line prefix
//   level.laststandpistol                        the pistol a downed player holds
// HOOKS (level.laststand_funcs[ key ], all optional):
//   "downed"       self = the player going down, before his weapons are taken
//   "downed_post"  self = the downed player, after "player_downed"
//   "bled_out"     self = the player who bled out, before he dies
//   "revived"      self = the revivee, ( reviver ): a teammate brought him back
//   "revive_done"  self = the player back up (any cause)
//   "can_revive"   self = the reviver, ( revivee ) -> false vetoes the revive
//   "revive_time"  self = the reviver, ( base ) -> seconds the hold takes
//   "vox_code"     self = the reviver -> alias prefix for <code>_whilerevivingteammate
//   "spare_life"   self = the downed player -> true when a solo down self-revives
//   "use_spare"    self = the solo player self-reviving: spend the spare life
//   "pistol"       self = the player going down -> the weapon he holds while down
//   "all_down"     level: nobody is left standing

REVIVE_BAR_WIDTH = 120;
REVIVE_BAR_HEIGHT = 9;

init()
{
	set_default( "bleedout_time", 45 );
	set_default( "solo_revive_time", 10 );
	set_default( "revive_time", 3 );
	set_default( "trigger_radius", 75 );
	set_default( "log", "CoDOL laststand" );
	if ( !isDefined( level.laststand_funcs ) )
		level.laststand_funcs = [];

	precacheString( &"SCRIPT_COOP_REVIVE" );
	precacheString( &"SCRIPT_COOP_REVIVING" );
	precacheString( &"SCRIPT_COOP_BLEEDING_OUT" );
	precacheString( &"SCRIPT_COOP_BLEEDING_OUT_PARTNER" );
	precacheString( &"MPUI_BEING_REVIVED" );
	precacheString( &"ZOMBIE_REVIVING_SOLO" );
	precacheShader( "waypoint_revive" );
	precacheShader( "progress_bar_bg" );
	precacheShader( "progress_bar_fill" );

	level thread revive_hint_think();
}

set_default( key, value )
{
	if ( !isDefined( level.laststand_vars ) )
		level.laststand_vars = [];
	if ( !isDefined( level.laststand_vars[ key ] ) )
		level.laststand_vars[ key ] = value;
}

get_var( key )
{
	return level.laststand_vars[ key ];
}

has_hook( key )
{
	return isDefined( level.laststand_funcs ) && isDefined( level.laststand_funcs[ key ] );
}

ls_log( text )
{
	prefix = "CoDOL laststand";
	if ( isDefined( level.laststand_vars ) && isDefined( level.laststand_vars[ "log" ] ) )
		prefix = level.laststand_vars[ "log" ];
	codolprintln( prefix + ": " + text );
}

is_true( value )
{
	return isDefined( value ) && value;
}

diag_val( v )
{
	if ( !isDefined( v ) )
		return "undefined";
	return "" + v;
}

// A mode without the native category floor (anything but zm) grants the down
// per player: C1 ability 8, the bit CODOL_LastStand_CanEnter tests.  Call it
// on every spawn; disable_player() takes it away again.
enable_player()
{
	if ( !( self hasAbilityModifier( "modPistolDeath" ) ) )
		self setAbilityModifier( "modPistolDeath" );
}

disable_player()
{
	if ( self hasAbilityModifier( "modPistolDeath" ) )
		self unsetAbilityModifier( "modPistolDeath" );
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

// T5 _zombiemode_utility.gsc:1050 is_player_valid, less the three tests
// whose builtins are not registered here (see zm _zombiemode_utility.gsc).
is_player_valid( player )
{
	if ( !isDefined( player ) )
		return false;
	if ( !isPlayer( player ) )
		return false;
	if ( !isAlive( player ) )
		return false;
	if ( is_true( player.laststand ) )
		return false;
	if ( isDefined( player.sessionstate ) && player.sessionstate == "spectator" )
		return false;
	if ( isDefined( player.sessionstate ) && player.sessionstate == "intermission" )
		return false;
	return true;
}

player_has_spare_life()
{
	if ( has_hook( "spare_life" ) )
		return self [[ level.laststand_funcs[ "spare_life" ] ]]();
	return false;
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
	// a solo down is a wipe.
	players = getPlayers();
	solo = players.size == 1;
	spare_life = self player_has_spare_life();

	// The mode's own bookkeeping, before the weapons are stored (zm: the
	// points penalty and the weapon power-up's release, T5 :2141/:2172).
	if ( has_hook( "downed" ) )
		self [[ level.laststand_funcs[ "downed" ] ]]();

	// _zombiemode.gsc:2176.
	self disableOffhandWeapons();

	self allowSprint( false );
	self allowJump( false );
	// C1 BG_PlayerCanAds permits pm_type7; WEAPFLAG0x20 still blocks ADS.
	// Zombies keeps sighted pistol aim; preserve the co-op restriction.
	self allowAds( getDvar( "g_gametype" ) == "zm" );
	self setMoveSpeedScale( 0.15 );
	self setStance( "prone" );

	// T5 :149 -- the AI stops caring about a downed player.  self.ignoreme is
	// a plain script variable here; setDontTarget is the registered sentient
	// method the native acquisition pass honours, so the two are paired.
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
		// _zombiemode.gsc:4818 wait_and_revive: the self revive over the
		// solo revive time.
		time = get_var( "solo_revive_time" );
		self laststand_self_hud( &"ZOMBIE_REVIVING_SOLO", time, true );
		self thread laststand_bleedout( time );
	}
	else
	{
		time = get_var( "bleedout_time" );
		self laststand_self_hud( &"SCRIPT_COOP_BLEEDING_OUT", time, false );
		self thread laststand_bleedout( time );
	}

	lives = undefined;
	if ( isDefined( self.pers ) )
		lives = self.pers[ "lives" ];
	ls_log( self.name + " down #" + self.downs + " (players " + players.size +
		", spare lives " + diag_val( lives ) + ", lost " + diag_val( self.score_lost_when_downed ) + ")" );

	// Every perk_think and every drink is waiting on this.
	self notify( "player_downed" );
	if ( has_hook( "downed_post" ) )
		self [[ level.laststand_funcs[ "downed_post" ] ]]();

	// _zombiemode.gsc:4700: when nobody is left standing the game is over at
	// once -- unless this is the solo player's spare life.
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

	// Disconnect can resume this wait after the native player was freed.
	// ClientDisconnect already removes client HUDs and the mode trigger.
	if ( !isDefined( self ) )
		return;

	self laststand_hud_destroy();

	// Prompts aimed at this player go with him.
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( isDefined( players[ i ].ls_revive_hint_target ) && players[ i ].ls_revive_hint_target == self )
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
	pistol = level.laststandpistol;
	if ( isDefined( self.ls_pistol ) )
		pistol = self.ls_pistol;
	if ( isDefined( pistol ) && self hasWeapon( pistol ) )
	{
		// Only take it back when it was not the player's own gun to begin
		// with; T5 makes the same distinction at :539.
		keep = false;
		if ( isDefined( self.laststand_weapons ) )
		{
			for ( i = 0; i < self.laststand_weapons.size; i++ )
			{
				if ( self.laststand_weapons[ i ] == pistol )
					keep = true;
			}
		}

		if ( !keep )
			self takeWeapon( pistol );
	}
	self.ls_pistol = undefined;

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

// T5 :326.  The mode may choose the pistol per player (IW4 Spec Ops
// maps\_coop.gsc:995 player_coop_force_switch_to_pistol: the player's own
// pistol when he carries one, else a forced one); otherwise it is
// level.laststandpistol, T5's rule.
laststand_give_pistol()
{
	pistol = level.laststandpistol;
	if ( has_hook( "pistol" ) )
		pistol = self [[ level.laststand_funcs[ "pistol" ] ]]();
	self.ls_pistol = pistol;
	if ( !isDefined( pistol ) )
		return;

	if ( !self hasWeapon( pistol ) )
		self giveWeapon( pistol );

	self giveMaxAmmo( pistol );
	self switchToWeapon( pistol );
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

	// T5 hands a bled-out zombies player to level.player_becomes_zombie; the
	// playable-zombie mode is not ported, so the spare-life rule is used: a
	// solo player's spare life self-revives (wait_and_revive), otherwise he
	// dies and the all-down check runs.
	if ( getPlayers().size == 1 && self player_has_spare_life() )
	{
		if ( has_hook( "use_spare" ) )
			self [[ level.laststand_funcs[ "use_spare" ] ]]();
		ls_log( self.name + " solo self revive" );
		self revive_success( undefined );
		return;
	}

	self laststand_die();
}

laststand_die()
{
	ls_log( self.name + " bled out" );

	self.laststand = false;
	self laststandrevive();
	self notify( "laststand_over" );

	// zm: the other players are penalised for not reviving (T5
	// _zombiemode_score.gsc:222, from zombify_player).
	if ( has_hook( "bled_out" ) )
		self [[ level.laststand_funcs[ "bled_out" ] ]]();

	self.ignoreme = false;
	self setDontTarget( false );
	self suicide();

	level notify( "player_bled_out", self );
}

// T5 :431.  The trigger is the revive volume (can_revive's isTouching); its
// hint is never drawn by this engine for a trigger_radius, so the prompt is
// forced instead (revive_hint_think).
revive_trigger_spawn()
{
	radius = get_var( "trigger_radius" );

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
			if ( isDefined( reviver.ls_reviving ) )
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

// The hold time for this reviver.  The mode decides what shortens it (zm:
// Quick Revive halves it).  Without a hook the reviver's selected perks do:
// the coPerkStatsTable modifier modReviveRate (specialty_coop_quickrevive 2,
// _pro 3) divides the base time.
revive_hold_time( base )
{
	if ( has_hook( "revive_time" ) )
		return self [[ level.laststand_funcs[ "revive_time" ] ]]( base );
	rate = self getSelectedPerkModifierValue( "modReviveRate" );
	if ( isDefined( rate ) && rate > 1 )
		return base / rate;
	return base;
}

// One hold.  The reviver's half runs in his own thread, so either player
// leaving or going down mid-hold cleans up only what belongs to him.
revive_attempt( reviver )
{
	revive_time = reviver revive_hold_time( get_var( "revive_time" ) );

	reviver.ls_reviving = self;
	self.ls_revive_result = false;
	self.revivetrigger.beingRevived = 1;

	ls_log( reviver.name + " reviving " + self.name + " (" + revive_time + " s)" );

	self laststand_being_revived_hud( revive_time );
	reviver thread revive_do_revive( self, revive_time );

	while ( isDefined( reviver ) && isDefined( reviver.ls_reviving ) )
		wait( 0.05 );

	self laststand_being_revived_hud_destroy();
	if ( isDefined( self.revivetrigger ) )
		self.revivetrigger.beingRevived = 0;

	if ( !self.ls_revive_result )
		ls_log( "revive of " + self.name + " cancelled" );

	return self.ls_revive_result;
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
		revivee.ls_revive_result = revived;
	self.ls_reviving = undefined;
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

	// The mode's own exclusions (zm: a power-up weapon or a perk bottle in
	// hand, T5 _zombiemode.gsc:4176 level.can_revive).
	if ( has_hook( "can_revive" ) && !( self [[ level.laststand_funcs[ "can_revive" ] ]]( revivee ) ) )
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
// for a self-revive and for scripted revives (zm's all-revive power-ups).
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
		if ( !isDefined( reviver.revives ) )
			reviver.revives = 0;
		reviver.revives++;

		// zm: the reviver gets back what the downed player lost going down.
		if ( has_hook( "revived" ) )
			self [[ level.laststand_funcs[ "revived" ] ]]( reviver );

		ls_log( reviver.name + " revived " + self.name + " (" + reviver.revives + " revives)" );
	}
	else
	{
		ls_log( self.name + " back up" );
	}

	// zm: the health regen thread ended on "player_downed"; restart it.
	if ( has_hook( "revive_done" ) )
		self [[ level.laststand_funcs[ "revive_done" ] ]]();

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

	ls_log( "no player left standing" );
	if ( has_hook( "all_down" ) )
		[[ level.laststand_funcs[ "all_down" ] ]]();
	else
		level notify( "laststand_all_down" );
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
			if ( !isDefined( player.ls_reviving ) )
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
		if ( isDefined( self.ls_revive_hint_target ) && self.ls_revive_hint_target == target )
			return;
		self forceUseHintOn( &"SCRIPT_COOP_REVIVE" );
		self.ls_revive_hint_target = target;
		self.ls_revive_hint_on = true;
		return;
	}

	if ( !is_true( self.ls_revive_hint_on ) && !isDefined( self.ls_revive_hint_target ) )
		return;
	self forceUseHintOff();
	self.ls_revive_hint_target = undefined;
	self.ls_revive_hint_on = undefined;
}

// --------------------------------------------------------------------------
// HUD.  C1's own progress bar shape (maps/mp/gametypes/_hud.gsc:105-111,
// _hud_util.gsc createBar): progress_bar_bg behind progress_bar_fill, 120 x 9
// at -61 from centre, its text at -75.
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

	self.ls_text = self revive_text_create( 56, text );
	self.ls_text.color = ( 1, 0.25, 0.25 );
	self.ls_bar = self revive_bar_create( 70 );
	self.ls_bar.bar.color = ( 1, 0.25, 0.25 );
	if ( fill )
		self.ls_bar revive_bar_fill( 0, 1, time );
	else
		self.ls_bar revive_bar_fill( 1, 0, time );
}

laststand_self_hud_destroy()
{
	if ( isDefined( self.ls_text ) )
		self.ls_text destroy();
	self.ls_text = undefined;
	if ( isDefined( self.ls_bar ) )
		self.ls_bar revive_bar_destroy();
	self.ls_bar = undefined;
}

// self = the downed player, while someone holds the revive.
laststand_being_revived_hud( time )
{
	self laststand_being_revived_hud_destroy();

	self.ls_revived_bar = self revive_bar_create( -61 );
	self.ls_revived_bar revive_bar_fill( 0, 1, time );
	self.ls_revived_text = self revive_text_create( -75, &"MPUI_BEING_REVIVED" );
}

laststand_being_revived_hud_destroy()
{
	if ( isDefined( self.ls_revived_text ) )
		self.ls_revived_text destroy();
	self.ls_revived_text = undefined;
	if ( isDefined( self.ls_revived_bar ) )
		self.ls_revived_bar revive_bar_destroy();
	self.ls_revived_bar = undefined;
}

// T5 :907 revive_hud_think + :961 fadeReviveMessageOver, and the revive icon
// over the downed player for everyone else: waypoint_revive, as C1's own
// multiplayer revive draws it (_damage.gsc lastStandTimer), turning from
// white to red as the bleed-out runs.
laststand_partner_hud()
{
	self endon( "laststand_over" );

	self.ls_revive_icons = [];
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
		self.ls_revive_icons[ self.ls_revive_icons.size ] = icon;

		if ( player player_is_in_laststand() )
			continue;

		notice = player revive_text_create( 100, &"SCRIPT_COOP_BLEEDING_OUT_PARTNER" );
		notice fadeOverTime( 3 );
		notice.alpha = 0;
		player thread revive_notice_expire( notice, 3 );
	}

	total = get_var( "bleedout_time" );
	for ( ;; )
	{
		wait( 1 );
		if ( !isDefined( self.bleedout_time ) || total <= 0 )
			continue;
		frac = self.bleedout_time / total;
		if ( frac < 0 )
			frac = 0;
		for ( i = 0; i < self.ls_revive_icons.size; i++ )
		{
			if ( isDefined( self.ls_revive_icons[ i ] ) )
				self.ls_revive_icons[ i ].color = ( 1, frac, frac );
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

	if ( isDefined( self.ls_revive_icons ) )
	{
		for ( i = 0; i < self.ls_revive_icons.size; i++ )
		{
			if ( isDefined( self.ls_revive_icons[ i ] ) )
				self.ls_revive_icons[ i ] destroy();
		}
	}
	self.ls_revive_icons = undefined;
}

// T5 _zombiemode.gsc:2042 has the reviver speak; C1 ships each zm character's
// line for exactly that, <code>_whilerevivingteammate.  The mode names the
// code (zm: the roster's column 1).
revive_vox()
{
	if ( !has_hook( "vox_code" ) )
		return;
	code = self [[ level.laststand_funcs[ "vox_code" ] ]]();
	if ( !isDefined( code ) )
		return;
	if ( isDefined( self.ls_revive_vox_time ) && getTime() - self.ls_revive_vox_time < 10000 )
		return;
	self.ls_revive_vox_time = getTime();
	self playSound( code + "_whilerevivingteammate" );
}
