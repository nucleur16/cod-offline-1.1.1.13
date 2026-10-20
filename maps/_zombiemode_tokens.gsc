// C1 revive token item 8500: the lobby owns inventory and acknowledges one
// authenticated spend. The native handler validates this down and deduplicates
// receipts. Restored from private checkpoint b493943 and adapted to the shared
// laststand hooks; no client-only or free revive path.

init()
{
	precacheMenu( "respawnpopupmenurevive" );
}

begin_down()
{
	self.revive_used_token = false;
	self.zm_token_protection_pending = false;
	self.revive_token_window = false;
	if ( !isDefined( self.revive_token_spent ) )
		self.revive_token_spent = 0;
	if ( self.revive_token_spent >= 3 || !self codolArmReviveToken() )
		return false;

	self.revive_token_window = true;
	self.revive_token_shown_owned = -1;
	self update_inventory();
	self thread token_input();
	self thread token_inventory();
	self thread token_receipt();
	level thread token_match_end( self );
	return true;
}

token_match_end( player )
{
	player endon( "disconnect" );
	player endon( "laststand_over" );
	level waittill( "end_game" );
	if ( isDefined( player ) )
		player end_down();
}

end_down()
{
	was_open = isDefined( self.revive_token_window ) && self.revive_token_window;
	self.revive_token_window = false;
	self codolClearReviveToken();
	// C1 only forwards a named r <slot> to its Scaleform token hide path.
	// A bare closeMenu() has no menu name and leaves this overlay visible.
	if ( was_open )
		self closeMenu( "respawnpopupmenurevive" );
	// Death/bleed-out can remove the last remaining recovery opportunity.
	// Re-run after this frame so the shared revive/death state has settled.
	level thread recovery_finished();
}

recovery_finished()
{
	wait( 0.05 );
	if ( !level.gameEnded )
		common_scripts\_laststand::checkForAllDead();
}

has_recovery_window()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		player = players[ i ];
		if ( isAlive( player ) && isDefined( player.revive_token_window ) &&
			player.revive_token_window && player common_scripts\_laststand::player_is_in_laststand() )
			return true;
	}
	return false;
}

update_inventory()
{
	if ( !self.laststand || !isDefined( self.revive_token_window ) || !self.revive_token_window )
		return;
	owned = self codolGetReviveTokenCount();
	if ( owned < 0 )
		owned = 0;
	if ( owned > 3 - self.revive_token_spent )
		owned = 3 - self.revive_token_spent;
	if ( owned == self.revive_token_shown_owned )
		return;
	was_open = self.revive_token_shown_owned != -1;
	self.revive_token_shown_owned = owned;
	self setClientDvar( "ui_tok_info", self.revive_token_spent + "," + owned );
	if ( was_open )
		self closeMenu( "respawnpopupmenurevive" );
	self openMenu( "respawnpopupmenurevive" );
}

token_input()
{
	self endon( "disconnect" );
	self endon( "laststand_over" );
	self endon( "bled_out" );
	level endon( "end_game" );
	for ( ;; )
	{
		wait( 0.05 );
		if ( !self useButtonPressed() )
			continue;
		elapsed = 0;
		while ( self useButtonPressed() && elapsed < 0.5 )
		{
			wait( 0.05 );
			elapsed += 0.05;
		}
		if ( elapsed >= 0.5 && self.laststand && self codolGetReviveTokenCount() > 0 )
		{
			if ( self codolRequestReviveTokenSpend() )
				return;
		}
	}
}

token_inventory()
{
	self endon( "disconnect" );
	self endon( "laststand_over" );
	self endon( "bled_out" );
	level endon( "end_game" );
	for ( ;; )
	{
		wait( 0.5 );
		if ( !self.laststand || !self.revive_token_window )
			return;
		self update_inventory();
	}
}

token_receipt()
{
	self endon( "disconnect" );
	self endon( "laststand_over" );
	self endon( "bled_out" );
	level endon( "end_game" );
	self waittill( "revive_token_consumed" );
	if ( !self.laststand || self.revive_token_spent >= 3 )
		return;
	self.revive_token_spent++;
	self.revive_used_token = true;
	// Set only after the native handler has accepted the lobby's authenticated
	// receipt. The revive-done hook consumes this once; a normal revive cannot
	// inherit protection from an earlier token or a repeated callback.
	self.zm_token_protection_pending = true;
	// This listener ends on laststand_over. A child owns the full revive so
	// that notify cannot abort it before player_revived/perk cleanup runs.
	self thread common_scripts\_laststand::revive_success( undefined );
}
