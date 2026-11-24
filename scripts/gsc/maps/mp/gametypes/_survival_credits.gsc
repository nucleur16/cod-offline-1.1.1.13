// survival credits: "+N" by the crosshair for every award.
//
// The multiplayer score popup's element, place, pulse and window
// (_rank::scorePopup: awards within 0.05 + 1.0 s of the last add up, then it
// fades over 0.75 s and the next award starts over). As on the HUD's credit
// counter (survival_status.menu: "Credits: N") it is a bare number in blue,
// lighter than the counter's to stand out in the middle of the screen.
// In Survival the element shows credits only: _rank::scorePopup returns while
// level.survival_credits_popup is set (a dog's multiplayer XP would share the
// spot).
//
// _survival::add_credits calls award() for each: kills (headshot, knife,
// juggernaut, helicopter, and those of a player's sentry, killstreak or
// vehicle), the wave reward and a teammate's shared credits.

precache()
{
	level.survival_credits_popup = true;
}

// self: the player paid. `why` is add_credits' own.
award( amount, why )
{
	if ( amount <= 0 || !isDefined( self.hud_scorePopup ) )
		return;
	if ( isDefined( self.isbot ) && self.isbot )
		return;
	if ( why == "wave" )
		self thread wave_reward( amount );
	else
		self thread popup( amount );
}

// The wave's reward is its own number, not the tail of the last kill's.
wave_reward( amount )
{
	self endon( "disconnect" );
	self endon( "joined_team" );
	self endon( "joined_spectators" );
	level endon( "survival_over" );
	// "Wave N complete" is above it until then.
	wait( 3.2 );
	self popup( amount );
}

popup( amount )
{
	self endon( "disconnect" );
	self endon( "joined_team" );
	self endon( "joined_spectators" );

	self notify( "survival_credits_popup" );
	self endon( "survival_credits_popup" );

	if ( !isDefined( self.survival_popup_total ) )
		self.survival_popup_total = 0;
	self.survival_popup_total += int( amount );

	wait( 0.05 );

	hud = self.hud_scorePopup;
	hud.label = &"MP_PLUS";
	hud.color = ( 0.35, 0.78, 1 );
	hud.glowAlpha = 0;
	hud setValue( self.survival_popup_total );
	hud.alpha = 0.95;
	hud thread maps\mp\gametypes\_hud::fontPulse( self );

	wait( 1.0 );

	self.survival_popup_total = 0;
	hud fadeOverTime( 0.75 );
	hud.alpha = 0;
}

