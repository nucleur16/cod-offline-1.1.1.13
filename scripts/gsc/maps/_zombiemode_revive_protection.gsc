// Token-revival Painkiller, Zombies only.
//
// Original C1 footage xWsKn1NEBt8 6:42 token popup -> 6:48 pill / 0:10 /
// amber overlay. Those assets and ten seconds are observed. Original Zombie
// damage strength is not recovered: the one-third default INCLUDING claws
// below is an explicit emulation policy, not a measured original multiplier.
// MP CombatHigh excludes melee and depends on a selected deathstreak, so it
// cannot supply this Zombies rule. No MP perk, ability or loadout is changed.
//
// scr_zm_token_painkiller_scale: empty/invalid/negative -> 1/3, positive ->
// min(value,1). Only explicit "0" or "0.0" requests zero combat damage.
// Falls, suicide, trigger hurt and scripted/unknown damage are unaffected.

init()
{
	precacheShader( "specialty_painkiller" );
	precacheShader( "combathigh_overlay" );
}

damage_scale()
{
	raw = getDvar( "scr_zm_token_painkiller_scale" );
	if ( raw == "0" || raw == "0.0" )
		return 0;
	if ( raw == "" )
		return 1.0 / 3.0;
	scale = getDvarFloat( "scr_zm_token_painkiller_scale" );
	if ( !( scale > 0 ) )
		return 1.0 / 3.0;
	return min( scale, 1.0 );
}

start_if_token()
{
	if ( !isDefined( self.zm_token_protection_pending ) || !self.zm_token_protection_pending )
		return false;
	self.zm_token_protection_pending = false;
	if ( getDvar( "g_gametype" ) != "zm" || level.gameEnded ||
		!isAlive( self ) || self.laststand ||
		!isDefined( self.revive_used_token ) || !self.revive_used_token )
		return false;

	self clear();
	self.zm_token_protection_until = getTime() + 10000;
	self.zm_token_protection_scale = damage_scale();
	self create_hud();
	self thread lifetime();
	self thread clear_on_event( "player_downed" );
	self thread clear_on_event( "death" );
	self thread clear_on_event( "disconnect" );
	self thread clear_on_end_game();
	return true;
}

active()
{
	return getDvar( "g_gametype" ) == "zm" && !level.gameEnded &&
		isAlive( self ) && !self.laststand &&
		isDefined( self.zm_token_protection_until ) && getTime() < self.zm_token_protection_until;
}

combat_mod( mod )
{
	switch ( mod )
	{
		case "MOD_PISTOL_BULLET":
		case "MOD_RIFLE_BULLET":
		case "MOD_GRENADE":
		case "MOD_GRENADE_SPLASH":
		case "MOD_PROJECTILE":
		case "MOD_PROJECTILE_SPLASH":
		case "MOD_MELEE":
		case "MOD_EXPLOSIVE":
		case "MOD_EXPLOSIVE_BULLET":
		case "MOD_BURN":
			return true;
	}
	return false;
}

// Called AFTER perk-pop immunity and the existing 75-point explosive cap,
// BEFORE finishPlayerDamage. Keep their zero immunity and integer policy.
damage_override( damage, mod )
{
	if ( damage <= 0 || !self active() || !combat_mod( mod ) )
		return damage;
	if ( self.zm_token_protection_scale == 0 )
		return 0;
	scaled = int( damage * self.zm_token_protection_scale );
	if ( scaled < 1 )
		return 1;
	return scaled;
}

create_hud()
{
	// Native HUD elements use the shipped MP Painkiller layout: center timer
	// y112 (split-screen56), 32-unit pill above its 12-unit font band, and
	// the original full-screen amber material. No duplicate portrait overlay.
	y = 112;
	size = 32;
	if ( isDefined( level.splitscreen ) && level.splitscreen )
	{
		y = 56;
		size = 21;
	}
	self.zm_token_protection_overlay = newClientHudElem( self );
	overlay = self.zm_token_protection_overlay;
	overlay.alignX = "left";
	overlay.alignY = "top";
	overlay.horzAlign = "fullscreen";
	overlay.vertAlign = "fullscreen";
	overlay setShader( "combathigh_overlay", 640, 480 );
	overlay.sort = -10;
	overlay.archived = true;
	overlay.alpha = 0;
	overlay fadeOverTime( 1 );
	overlay.alpha = 1;

	self.zm_token_protection_timer = newClientHudElem( self );
	timer = self.zm_token_protection_timer;
	timer.alignX = "center";
	timer.alignY = "middle";
	timer.horzAlign = "center";
	timer.vertAlign = "middle";
	timer.y = y;
	timer.font = "hudsmall";
	timer.fontScale = 1;
	timer.color = ( 0.8, 0.8, 0 );
	timer.archived = false;
	timer.foreground = true;
	timer setTimer( 10 );

	self.zm_token_protection_icon = newClientHudElem( self );
	icon = self.zm_token_protection_icon;
	icon.alignX = "center";
	icon.alignY = "bottom";
	icon.horzAlign = "center";
	icon.vertAlign = "middle";
	icon.y = y - 6;
	icon setShader( "specialty_painkiller", size, size );
	icon.archived = true;
	icon.foreground = true;
	icon.sort = 1;
	icon.alpha = 0;
	icon fadeOverTime( 1 );
	icon.alpha = 0.85;
}

lifetime()
{
	self endon( "zm_token_protection_cleared" );
	fading = false;
	while ( self active() )
	{
		if ( !fading && self.zm_token_protection_until - getTime() <= 2000 )
		{
			fading = true;
			self.zm_token_protection_overlay fadeOverTime( 2 );
			self.zm_token_protection_overlay.alpha = 0;
			self.zm_token_protection_icon fadeOverTime( 2 );
			self.zm_token_protection_icon.alpha = 0;
			self.zm_token_protection_timer fadeOverTime( 2 );
			self.zm_token_protection_timer.alpha = 0;
		}
		wait( 0.05 );
	}
	self clear();
}

clear_on_event( event )
{
	self endon( "zm_token_protection_cleared" );
	self waittill( event );
	self clear();
}

clear_on_end_game()
{
	self endon( "zm_token_protection_cleared" );
	level waittill( "end_game" );
	self clear();
}

clear()
{
	self.zm_token_protection_until = undefined;
	self.zm_token_protection_scale = undefined;
	if ( isDefined( self.zm_token_protection_overlay ) )
		self.zm_token_protection_overlay destroy();
	if ( isDefined( self.zm_token_protection_timer ) )
		self.zm_token_protection_timer destroy();
	if ( isDefined( self.zm_token_protection_icon ) )
		self.zm_token_protection_icon destroy();
	self.zm_token_protection_overlay = undefined;
	self.zm_token_protection_timer = undefined;
	self.zm_token_protection_icon = undefined;
	// Last: a caller listening for this cancellation must finish destroying
	// its HUD/state before this notify terminates it and its sibling watchers.
	self notify( "zm_token_protection_cleared" );
}
