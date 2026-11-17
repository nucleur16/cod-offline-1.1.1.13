// Local Endless policy. Reuse the T5-derived common laststand revive_success
// (native laststandrevive, restored weapons/movement and normal notifications).
// Quick Revive must be obtained through the map's normal perk system. No
// spawn/revive grant is made. Its normal teammate hold is 3 / 2 seconds;
// a solo player who owns it may recover at most three times per match.
// No token inventory, request, receipt, protection or wallet function is used.

enabled()
{
	return isDefined( level.zm_round_mode ) && level.zm_round_mode == 3;
}

init()
{
	if ( enabled() && !isDefined( level.zm_endless_revives_used ) )
		level.zm_endless_revives_used = [];
}

// getGuid is native SV_GetGuid: authenticated CoDOL connect identity in hex.
// Reject its slot-based fallback/test-client keys: they cannot prove that a
// reconnect is the same player. getXuid is unsuitable: C1 playerid is0.
// The level ledger survives disconnect/entity-slot reuse until this map ends.
player_key()
{
	key = self getGuid();
	if ( key.size != 16 || key == "0000000000000000" )
		return undefined;
	for ( i = 0; i < 16; i++ )
	{
		if ( !isSubStr( "0123456789abcdef", getSubStr( key, i, i + 1 ) ) )
			return undefined;
	}
	return "uid_" + key;
}

on_connect()
{
	if ( !enabled() )
		return;
	init();
	key = self player_key();
	if ( isDefined( key ) && !isDefined( level.zm_endless_revives_used[ key ] ) )
		level.zm_endless_revives_used[ key ] = 0;
	self.revive_token_window = false;
	self.revive_used_token = false;
	self.zm_token_protection_pending = false;
	self end_down();
}

has_quick_revive_machine()
{
	if ( !isDefined( level.zombie_perk_machines ) )
		return false;
	for ( i = 0; i < level.zombie_perk_machines.size; i++ )
	{
		if ( level.zombie_perk_machines[ i ] == "specialty_quickrevive" )
			return true;
	}
	return false;
}

on_spawn()
{
	if ( !enabled() )
		return;
	self end_down();
}

uses_remaining( key )
{
	if ( !enabled() || !isDefined( key ) || !isDefined( level.zm_endless_revives_used ) ||
		!isDefined( level.zm_endless_revives_used[ key ] ) )
		return false;
	return level.zm_endless_revives_used[ key ] < 3;
}

// Count the ordinary connected roster, including downed/dead/spectating
// teammates. Losing standing targets must not turn a co-op down into solo.
is_current_solo()
{
	players = getPlayers();
	return players.size == 1 && players[ 0 ] == self;
}

prepare_down()
{
	if ( !enabled() || isDefined( self.laststand ) && self.laststand )
		return;
	self.zm_endless_down_key = self player_key();
	self.zm_endless_solo_at_down = self is_current_solo();
	// Capture ownership before the ordinary down notification removes perks.
	self.zm_endless_auto_pending = self.zm_endless_solo_at_down &&
		uses_remaining( self.zm_endless_down_key ) && has_quick_revive_machine() &&
		self hasPerk( "specialty_quickrevive" );
}

can_recover()
{
	return enabled() && !level.gameEnded && isAlive( self ) && self.laststand &&
		isDefined( self.zm_endless_auto_pending ) && self.zm_endless_auto_pending &&
		isDefined( self.zm_endless_solo_at_down ) && self.zm_endless_solo_at_down &&
		self is_current_solo() && self player_key() == self.zm_endless_down_key &&
		uses_remaining( self.zm_endless_down_key );
}

pending_auto_recovery()
{
	if ( !enabled() )
		return false;
	players = getPlayers();
	return players.size == 1 && players[ 0 ] can_recover();
}

// This is an ordinary down/revive window, with no token popup/hold/receipt.
// Keep the shared 45-second bleed-out and the existing all-down end countdown.
begin_down()
{
	if ( !enabled() )
		return false;
	if ( !isDefined( self.zm_endless_recovery_watching ) || !self.zm_endless_recovery_watching )
	{
		self.zm_endless_recovery_watching = true;
		self thread recovery_watch();
	}
	return true;
}

end_down()
{
	self.zm_endless_auto_pending = false;
	self.zm_endless_solo_at_down = false;
	self.zm_endless_down_key = undefined;
	self.zm_endless_recovery_watching = false;
}

recovery_watch()
{
	self endon( "disconnect" );
	self endon( "laststand_over" );
	level endon( "end_game" );
	// Native lethal-damage/laststand callbacks must unwind before get-up.
	wait( 0.05 );
	if ( self can_recover() )
	{
		// No wait between ownership check, consumption and clearing pending:
		// duplicate callbacks cannot spend twice or restore a fourth recovery.
		level.zm_endless_revives_used[ self.zm_endless_down_key ]++;
		self.zm_endless_auto_pending = false;
		// Own the complete revive in a child, as the existing token listener
		// does: laststand_over ends this watcher before player_revived is sent.
		self thread common_scripts\_laststand::revive_success( undefined );
		return;
	}
	// Roster changes can cancel this down's solo opportunity, never create one.
	self.zm_endless_auto_pending = false;
	last_count = -1;
	while ( !level.gameEnded && isAlive( self ) && self.laststand )
	{
		players = getPlayers();
		if ( players.size > 1 && !isDefined( self.revivetrigger ) )
		{
			self common_scripts\_laststand::revive_trigger_spawn();
			self thread common_scripts\_laststand::laststand_partner_hud();
		}
		if ( last_count != players.size )
		{
			last_count = players.size;
			common_scripts\_laststand::checkForAllDead();
		}
		wait( 0.1 );
	}
}
