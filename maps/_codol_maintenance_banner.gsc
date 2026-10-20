// Website maintenance titles reach this dvar through the lobby's site poll.
// One global HUD pair also covers late joins, respawns and spectators.
init()
{
	if ( isDefined( level.codolMaintenanceBannerStarted ) )
		return;
	level.codolMaintenanceBannerStarted = true;
	precacheShader( "black" );
	level thread watchMaintenanceBanner();
}

watchMaintenanceBanner()
{
	lastText = "";
	background = undefined;
	message = undefined;
	nextScroll = 0;
	for ( ;; )
	{
		text = getDvar( "codol_maintenance_banner" );
		if ( text != lastText )
		{
			if ( text == "" )
			{
				if ( isDefined( message ) )
					message destroy();
				if ( isDefined( background ) )
					background destroy();
				message = undefined;
				background = undefined;
				{}
			}
			else
			{
				if ( !isDefined( background ) )
				{
					background = newHudElem();
					// Fullscreen coordinates bypass the player's HUD safe-area inset.
					background.horzAlign = "fullscreen";
					background.vertAlign = "fullscreen";
					background.alignX = "left";
					background.alignY = "top";
					background.x = 0;
					background.y = 0;
					background.sort = 900;
					background.alpha = 0.78;
					background.archived = false;
					background setShader( "black", 640, 16 );
					message = newHudElem();
					message.horzAlign = "fullscreen";
					message.vertAlign = "fullscreen";
					message.alignX = "left";
					message.alignY = "top";
					message.x = 0;
					message.y = 1;
					message.font = "default";
					message.fontScale = 1.1;
					message.color = ( 1, 0.25, 0.25 );
					message.sort = 901;
					message.archived = false;
				}
				message setText( text );
				nextScroll = 0;
				{}
			}
			lastText = text;
		}
		if ( isDefined( message ) && getTime() >= nextScroll )
		{
			// The sanitized title is at most 180 ASCII characters. A generous
			// runoff distance ensures its tail leaves the screen before wrapping.
			runoff = text.size * 12 + 24;
			travel = ( 640.0 + runoff ) / 50.0;
			message.x = 640;
			message moveOverTime( travel );
			message.x = 0 - runoff;
			nextScroll = getTime() + int( travel * 1000 );
			{}
		}
		wait 0.05;
	}
}
