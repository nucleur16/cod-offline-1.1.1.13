#include maps\mp\_utility;
#include common_scripts\utility;

init()
{
	precacheShellShock( "frag_grenade_mp" );
	precacheShellShock( "damage_mp" );
	precacheRumble( "artillery_rumble" );
	precacheRumble( "grenade_rumble" );
	
	// CoDOL: the dirt_effect_* menus are drawn as HUD elements (dirtEffect).
	precacheShader( "fullscreen_dirt_left" );
	precacheShader( "fullscreen_dirt_right" );
	precacheShader( "fullscreen_dirt_bottom" );
	precacheShader( "fullscreen_dirt_bottom_b" );
}

shellshockOnDamage( cause, damage )
{
	if ( self maps\mp\_flashgrenades::isFlashbanged() )
		return; // don't interrupt flashbang shellshock

	if ( self _hasPerk( "specialty_blastshield" ) )
		return;
	
	if ( cause == "MOD_EXPLOSIVE" ||
	     cause == "MOD_GRENADE" ||
	     cause == "MOD_GRENADE_SPLASH" ||
	     cause == "MOD_PROJECTILE" ||
	     cause == "MOD_PROJECTILE_SPLASH" )
	{
		time = 0;
		
		if(damage >= 90)
			time = 4;
		else if(damage >= 50)
			time = 3;
		else if(damage >= 25)
			time = 2;
		else if(damage > 10)
			time = 2;
		
		if ( time )
		{
			self shellshock("frag_grenade_mp", 0.5);
		}
	}
}

endOnDeath()
{
	self waittill( "death" );
	waittillframeend;
	self notify ( "end_explode" );
}

grenade_earthQuake()
{
	self thread endOnDeath();
	self endon( "end_explode" );
	self waittill( "explode", position );
	PlayRumbleOnPosition( "grenade_rumble", position );
	earthquake( 0.5, 0.75, position, 800 );
	
	foreach ( player in level.players )
	{
		if ( player isUsingRemote() )
			continue;
			
		if ( distance( position, player.origin ) > 600 )
			continue;
			
		if ( player DamageConeTrace( position ) )
			player thread dirtEffect( position );
	}
}


dirtEffect( position )
{
	self endon ( "disconnect" );
	
	forwardVec = vectorNormalize( anglesToForward( self.angles ) );
	rightVec = vectorNormalize( anglesToRight( self.angles ) );
	grenadeVec = vectorNormalize( position - self.origin );
	
	fDot = vectorDot( grenadeVec, forwardVec );
	rDot = vectorDot( grenadeVec, rightVec );
	
	{}
	{}
	
	effectMenus = [];
	if ( fDot > 0 && fDot > 0.5 && !codolIsWeaponBase( self getCurrentWeapon(), "riotshield_mp" ) )
		effectMenus[effectMenus.size] = "dirt_effect_center";

	if ( abs(fDot) < 0.866 )
	{
		if ( rDot > 0 )
			effectMenus[effectMenus.size] = "dirt_effect_right";
		else
			effectMenus[effectMenus.size] = "dirt_effect_left";
	}

	if ( !effectMenus.size )
		return;

	// CoDOL: the dirt_effect_* scriptmenus are drawn as client HUD elements.
	// Any legacy menu C1 opens sets the UI key-catcher (Menus_OpenByName
	// 0x005F4FB0, write at 0x005F509B), and UI_Refresh draws the mouse cursor
	// over the game while it is set, so opening them showed a cursor for the
	// whole effect.  A new explosion replaces the running overlays.  See
	// dirtOverlayCreate for the decoded menus.
	self notify( "dirt_effect_new" );
	self endon( "dirt_effect_new" );
	self dirtOverlayClear();

	foreach ( menu in effectMenus )
		self dirtOverlayCreate( menu );

	if ( isAlive( self ) )
		self waittill_notify_or_timeout( "death", 2.0 );

	if ( !isAlive( self ) )
	{
		self dirtOverlayClear();
		return;
	}

	// The menus' onClose: "lerp" "alpha" "from" 1 "to" 0 "over" "1.0".
	foreach ( overlay in self.dirtOverlays )
	{
		overlay fadeOverTime( 1.0 );
		overlay.alpha = 0;
	}
	self waittill_notify_or_timeout( "death", 1.0 );
	self dirtOverlayClear();
}

/*
	CoDOL: dirt_effect_left/right/center as decoded from common_mp.ff (menufile
	assets 1593-1595, back-references resolved through the native zone replay).
	Every item is a WINDOW_STYLE_SHADER window, forecolour 1 1 1 1, rect
	-682.667 -341.333 1365.333 682.667 with horizontal/vertical align 2/2, the
	same enumeration as hud horzAlign "center" / vertAlign "middle": a 1365x683
	virtual-unit image centred on the screen.  The menus are 0 0 640 480.
	IW4 SP draws the same art the same way (maps/_gameskill.gsc
	grenade_dirt_on_screen: center/middle, 2048*0.66 x 1024*0.66, the same
	slides, 0.2 s flash then bottom_b, 2 s then a 1 s fade, and
	fullscreen_dirt_right for the right side).

	  left    fullscreen_dirt_left.  onOpen: alpha 0->1 over 0.05, scale 0->1
	          over 0.05, x -1000->0 over 0.05.
	  right   fullscreen_dirt_left with width and height negated (the image
	          turned 180 degrees); drawn here with fullscreen_dirt_right, since
	          setShader rejects negative sizes.  onOpen: alpha 0->1 over 0.15,
	          scale 0->1 over 0.1, x 300->0 over 0.1.
	  center  onOpen: alpha 0->1 over 0.02, scale 0.3->1 over 0.1, y 100->0 over
	          0.08, and setLocalVarInt dirt_update_time = milliseconds() + 200.
	          Item 1 fullscreen_dirt_bottom: forecolour alpha =
	            min( ( localvarint( "dirt_update_time" ) - milliseconds() ) / 200, 1 )
	          i.e. full at open, gone 200 ms later.  Item 2
	          fullscreen_dirt_bottom_b: visible when
	            milliseconds() > localvarint( "dirt_update_time" )
	          i.e. from 200 ms on.
	  all     onClose: alpha 1->0 over 1.0.
*/
dirtOverlayCreate( menu )
{
	switch ( menu )
	{
		case "dirt_effect_left":
			self dirtOverlayElem( "fullscreen_dirt_left", 0.05, -1000, 0, 0.05, 0, 0.05 );
			break;

		case "dirt_effect_right":
			self dirtOverlayElem( "fullscreen_dirt_right", 0.15, 300, 0, 0.1, 0, 0.1 );
			break;

		case "dirt_effect_center":
			// The menu's own 0.02 s fade-in is shorter than a server frame;
			// item 1 starts at full alpha and fades out over the 200 ms.
			first = self dirtOverlayElem( "fullscreen_dirt_bottom", 0, 0, 100, 0.08, 0.3, 0.1 );
			second = self dirtOverlayElem( "fullscreen_dirt_bottom_b", -1, 0, 100, 0.08, 0.3, 0.1 );
			self thread dirtOverlayCenterSwap( first, second );
			break;
	}
}

// fadeTime: the menu alpha lerp 0->1 (0 = start at 1, -1 = start hidden).
// ( fromX, fromY ) over moveTime: the menu x/y lerp to the rest position.
// fromScale over scaleTime: the menu scale lerp to 1.
// setShader clears any running fade/move/scale, so it goes first.
dirtOverlayElem( material, fadeTime, fromX, fromY, moveTime, fromScale, scaleTime )
{
	overlay = newClientHudElem( self );
	overlay.archived = false;
	overlay.foreground = true;	// over the HUD, as the menu was; IW4 SP _hud_util create_client_overlay_custom_size
	overlay.hideWhenInMenu = true;
	overlay.sort = 1 + self.dirtOverlays.size;
	overlay.horzAlign = "center";
	overlay.vertAlign = "middle";
	overlay.alignX = "center";
	overlay.alignY = "middle";
	overlay.color = ( 1, 1, 1 );
	overlay setShader( material, int( 1365 * fromScale ), int( 683 * fromScale ) );
	overlay scaleOverTime( scaleTime, 1365, 683 );

	overlay.x = fromX;
	overlay.y = fromY;
	overlay moveOverTime( moveTime );
	overlay.x = 0;
	overlay.y = 0;

	if ( fadeTime > 0 )
	{
		overlay.alpha = 0;
		overlay fadeOverTime( fadeTime );
		overlay.alpha = 1;
	}
	else if ( fadeTime == 0 )
		overlay.alpha = 1;
	else
		overlay.alpha = 0;

	self.dirtOverlays[self.dirtOverlays.size] = overlay;
	return overlay;
}

dirtOverlayCenterSwap( first, second )
{
	self endon( "disconnect" );
	self endon( "dirt_effect_new" );
	self endon( "death" );

	first fadeOverTime( 0.2 );
	first.alpha = 0;
	wait ( 0.2 );
	if ( isDefined( second ) )
		second.alpha = 1;
}

dirtOverlayClear()
{
	if ( isDefined( self.dirtOverlays ) )
	{
		foreach ( overlay in self.dirtOverlays )
		{
			if ( isDefined( overlay ) )
				overlay destroy();
		}
	}
	self.dirtOverlays = [];
}

c4_earthQuake()
{
	self thread endOnDeath();
	self endon( "end_explode" );
	self waittill( "explode", position );
	PlayRumbleOnPosition( "grenade_rumble", position );
	earthquake( 0.4, 0.75, position, 512 );
}

barrel_earthQuake()
{
	PlayRumbleOnPosition( "grenade_rumble", self.origin );
	earthquake( 0.4, 0.5, self.origin, 512 );
}


artillery_earthQuake()
{
	PlayRumbleOnPosition( "artillery_rumble", self.origin );
	earthquake( 0.7, 0.5, self.origin, 800 );
}