// CoDOL teammate-direction restoration. The original alive-buddy producer has
// not been recovered. Rendering uses the shipped C1 open-chevron material and
// original waypoint methods (C1 44aeda/44ac0e, IW4 5e1a20/5e1ac0).
// No scripted position/range polling or renderer-dvar changes.

init()
{
	precacheShader( "hud_offscreenobjectivepointer" );
	level.zm_teammate_pointers = [];
	level.zm_teammate_pointers_stopped = false;
}

// Called synchronously before disconnect/free, final death and every new life.
// Native HUD disconnect cleanup covers the departing viewer only; destroy the
// other viewers' references too, before a connection can reuse the target slot.
remove_player( player )
{
	if ( !isDefined( level.zm_teammate_pointers ) )
		return;
	keep = [];
	foreach ( pair in level.zm_teammate_pointers )
	{
		remove = !isDefined( pair.viewer ) || !isDefined( pair.target );
		if ( !remove )
			remove = pair.viewer == player || pair.target == player;
		if ( remove )
		{
			if ( isDefined( pair.hud ) )
				pair.hud Destroy();
		}
		else
			keep[ keep.size ] = pair;
	}
	level.zm_teammate_pointers = keep;
}

on_spawn()
{
	remove_player( self );
	if ( level.zm_teammate_pointers_stopped || level.gameEnded || !eligible( self ) )
		return;
	foreach ( other in level.players )
	{
		if ( !isDefined( other ) || other == self || !eligible( other ) )
			continue;
		create_pair( self, other );
		create_pair( other, self );
	}
}

eligible( player )
{
	if ( !isDefined( player ) || !isDefined( player.connected ) || !player.connected )
		return false;
	if ( !isDefined( player.sessionstate ) || player.sessionstate != "playing" )
		return false;
	// Last stand is not final death: the same living player entity remains a
	// teammate until killed. This module does not invent a replacement revive UI.
	if ( isDefined( player.laststand ) && player.laststand )
		return true;
	return player.health > 0;
}

create_pair( viewer, target )
{
	pair = SpawnStruct();
	pair.viewer = viewer;
	pair.target = target;
	pair.hud = NewClientHudElem( viewer );
	// C1 requires a positive material binding even for offscreen-only waypoints.
	// The client owns waypoint sizing; these are SetShader interface dimensions.
	pair.hud SetShader( "hud_offscreenobjectivepointer", 1, 1 );
	pair.hud SetWaypoint( true, true, false );
	pair.hud SetWaypointEdgeStyle_RotatingIcon();
	pair.hud SetWaypointIconOffscreenOnly();
	pair.hud SetTargetEnt( target );
	pair.hud.hidewheninmenu = true;
	pair.hud.hidewhendead = true;
	level.zm_teammate_pointers[ level.zm_teammate_pointers.size ] = pair;
}

shutdown()
{
	level.zm_teammate_pointers_stopped = true;
	if ( !isDefined( level.zm_teammate_pointers ) )
		return;
	foreach ( pair in level.zm_teammate_pointers )
	{
		if ( isDefined( pair.hud ) )
			pair.hud Destroy();
	}
	level.zm_teammate_pointers = [];
}
