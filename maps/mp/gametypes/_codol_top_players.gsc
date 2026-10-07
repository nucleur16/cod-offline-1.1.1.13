// CoDOL end-of-match top players scene.
//
// After the final killcam, every client looks through the map's
// top_players_camera while the match's three best players walk up to the
// map's top_players_loc marks as their own characters, pose, and their
// playercards (1ST / 2ND / 3RD) are shown. Then the normal end of match runs
// (intermission at mp_global_intermission, exitLevel).
//
// Everything below is C1's own data; docs/CODOL_ENDGAME_TOP3.md has the
// measurements and what is still open.
//
//   Map ents  every MP zone authors one script_struct "top_players_camera"
//             (64 units above the marks, facing them) and three
//             "top_players_loc" structs with script_index 0 (centre, 1st),
//             1 (camera-left, 2nd) and 2 (camera-right, 3rd), each facing the
//             camera. zm/coop maps have none.
//   Anims     post_game_walkup_{1st,2nd,3rd} and post_game_walkup_idle_*,
//             in C1's animtrees/player_3rd_person.atr (patch_common_mp) and
//             registered for script models by animlists/common.animlist
//             (=player_3rd_person). They play through the snapshot
//             animation-command channel from time 0 (setAnimKnobRestart),
//             not the random-phase scriptModelPlayAnim path.
//   Models    a script_model only draws one xmodel in C1; codol_setCharacterClone
//             publishes the player at entityState +64, which makes C1 compose
//             that client's whole character on the model (C1 0x55b5e0 /
//             0x43e809 -> 0x4437c0) with the player_3rd_person tree.
//   Cards     the HUD menus playercard_top_players_{1,2,3} (hud_720.txt,
//             patch_common_mp) draw getplayercardinfo(*, 0, 1|2|3) and are
//             visible while dvarbool("cg_topPlayersCards"). Slot N is filled
//             with SetCardDisplaySlot( player, N ) ('L <client> <slot>').
//
// zm and coop never reach this: they end through their own callbacks, their
// maps carry no top_players ents, and the menus test the gametype.

#include maps\mp\_utility;
#include common_scripts\utility;

#using_animtree( "player_3rd_person" );

init()
{
	if ( isDefined( level.is_zombie_level ) )
		return;

	if ( getDvar( "scr_codol_topplayers" ) == "" )
		setDvar( "scr_codol_topplayers", 1 );
	// Seconds the three hold their pose with their cards up once the last one
	// has arrived.
	if ( getDvar( "scr_codol_topplayers_hold" ) == "" )
		setDvar( "scr_codol_topplayers_hold", 6 );

	level.topPlayersWalk = [];
	level.topPlayersWalk[0] = %post_game_walkup_1st;
	level.topPlayersWalk[1] = %post_game_walkup_2nd;
	level.topPlayersWalk[2] = %post_game_walkup_3rd;
	level.topPlayersIdle = [];
	level.topPlayersIdle[0] = %post_game_walkup_idle_1st;
	level.topPlayersIdle[1] = %post_game_walkup_idle_2nd;
	level.topPlayersIdle[2] = %post_game_walkup_idle_3rd;

	level.topPlayersSceneActive = false;
	level thread onPlayerConnect();
}

// The card dvar is created on the client by the first 's' command and lives
// as long as the C1 process does, so a client that saw a scene would carry
// cg_topPlayersCards 1 into its next match.
onPlayerConnect()
{
	for ( ;; )
	{
		level waittill( "connected", player );

		player setClientDvar( "cg_topPlayersCards", 0 );

		if ( level.topPlayersSceneActive )
			player thread lateViewer();
	}
}

lateViewer()
{
	self endon( "disconnect" );

	// Let the connect callback finish first; it spawns the client as a
	// spectator.
	waittillframeend;
	if ( !level.topPlayersSceneActive || !isDefined( level.topPlayersCamera ) )
		return;

	self viewFromCamera( level.topPlayersCamera );
	self showCards();
}

// Script structs by targetname, without common_scripts' getstruct asserts
// (a map that authors two cameras is skipped rather than killed).
findStructs( name )
{
	result = [];
	if ( !isDefined( level.struct ) )
		return result;

	foreach ( struct in level.struct )
	{
		if ( isDefined( struct.targetname ) && struct.targetname == name )
			result[ result.size ] = struct;
	}
	return result;
}

sceneMarks()
{
	cameras = findStructs( "top_players_camera" );
	if ( cameras.size != 1 )
		return undefined;

	locs = findStructs( "top_players_loc" );
	marks = [];
	foreach ( loc in locs )
	{
		if ( !isDefined( loc.script_index ) )
			continue;
		index = int( loc.script_index );
		if ( index < 0 || index > 2 || isDefined( marks[ index ] ) )
			continue;
		marks[ index ] = loc;
	}
	// Ranks are contiguous from 1st: a map missing the 1st mark has no
	// scene, and one missing the 2nd shows the winner alone.
	contiguous = [];
	for ( index = 0; index < 3 && isDefined( marks[ index ] ); index++ )
		contiguous[ index ] = marks[ index ];
	if ( !contiguous.size )
		return undefined;

	scene = spawnStruct();
	scene.camera = cameras[0];
	scene.marks = contiguous;
	return scene;
}

// The match's best players by placement: score, then fewer deaths
// (_gamescore::getBetterPlayer). Spectators never place.
pickTopPlayers( count )
{
	maps\mp\gametypes\_gamescore::updatePlacement();

	top = [];
	if ( !isDefined( level.placement ) || !isDefined( level.placement["all"] ) )
		return top;

	foreach ( player in level.placement["all"] )
	{
		if ( top.size >= count )
			break;
		if ( !isDefined( player ) || !isPlayer( player ) )
			continue;
		top[ top.size ] = player;
	}
	return top;
}

// Called by _gamelogic::endGame once the final killcam is over and before the
// intermission; returns when the scene is done (at once when there is none).
playScene()
{
	if ( isDefined( level.is_zombie_level ) || !isDefined( level.topPlayersWalk ) )
		return;
	if ( !getDvarInt( "scr_codol_topplayers" ) )
		return;

	scene = sceneMarks();
	if ( !isDefined( scene ) )
	{
		print( "CoDOL top players: " + getDvar( "mapname" ) + " has no top_players_camera / top_players_loc, scene skipped\n" );
		return;
	}

	top = pickTopPlayers( scene.marks.size );
	if ( !top.size )
		return;

	level.topPlayersCamera = scene.camera;
	level.topPlayers = top;
	level.topPlayersModels = [];
	level.topPlayersSceneActive = true;
	level notify( "top_players_scene_start" );

	foreach ( player in level.players )
		player viewFromCamera( scene.camera );

	longest = 0;
	for ( rank = 0; rank < top.size; rank++ )
	{
		length = getAnimLength( level.topPlayersWalk[ rank ] );
		if ( length > longest )
			longest = length;
		level.topPlayersModels[ rank ] = top[ rank ] spawnWalker( rank, scene.marks[ rank ] );
	}

	print( "CoDOL top players: " + top.size + " on " + getDvar( "mapname" ) + ", walk " + longest + " s\n" );

	wait ( longest );

	foreach ( player in level.players )
		player showCards();

	wait ( getDvarFloat( "scr_codol_topplayers_hold" ) );

	level.topPlayersSceneActive = false;
	level notify( "top_players_scene_end" );

	foreach ( player in level.players )
		player setClientDvar( "cg_topPlayersCards", 0 );

	foreach ( model in level.topPlayersModels )
	{
		if ( isDefined( model ) )
			model delete();
	}
	level.topPlayersModels = [];
}

// _playerlogic::spawnIntermission's spawn, at the scene camera. An
// intermission view is the spawn origin and angles themselves, which is what
// the camera struct holds.
viewFromCamera( camera )
{
	self notify( "spawned" );
	self notify( "end_respawn" );

	self maps\mp\gametypes\_playerlogic::setSpawnVariables();
	self closepopupMenu();
	self closeInGameMenu();
	self clearLowerMessages();
	self freezeControlsWrapper( true );

	self.sessionstate = "intermission";
	self ClearKillcamState();
	self.friendlydamage = undefined;

	self spawn( camera.origin, camera.angles );
	self setDepthOfField( 0, 128, 512, 4000, 6, 1.8 );
}

showCards()
{
	if ( !isDefined( level.topPlayers ) )
		return;

	for ( rank = 0; rank < level.topPlayers.size; rank++ )
	{
		if ( isDefined( level.topPlayers[ rank ] ) )
			self SetCardDisplaySlot( level.topPlayers[ rank ], rank + 1 );
	}
	self setClientDvar( "cg_topPlayersCards", 1 );
}

// The anim's own displacement, from its start orientation into the world:
// x forward, y left, z up.
animDeltaToWorld( delta, angles )
{
	return anglesToForward( angles ) * delta[0] - anglesToRight( angles ) * delta[1] + anglesToUp( angles ) * delta[2];
}

spawnWalker( rank, mark )
{
	// C1 0x4437c0 reads a clone index of 0x13 or more as an actor slot, so a
	// client past 18 cannot be drawn as itself; a script model alone would be
	// one loose part of the outfit. The card still shows.
	if ( self getEntityNumber() >= 19 )
		return undefined;

	walk = level.topPlayersWalk[ rank ];
	idle = level.topPlayersIdle[ rank ];
	length = getAnimLength( walk );

	// The mark is where the walk ends, facing the camera; start where the
	// clip's own motion would bring the model onto it.
	endAngles = ( 0, mark.angles[1], 0 );
	startAngles = ( 0, mark.angles[1] - getAngleDelta( walk, 0, 1 ), 0 );
	start = mark.origin - animDeltaToWorld( getMoveDelta( walk, 0, 1 ), startAngles );

	model = spawn( "script_model", start );
	model.angles = startAngles;
	// The model's own xmodel is what C1 falls back to when the client's
	// character is gone, and the server builds its animation pose on it.
	if ( isDefined( self.model ) && self.model != "" )
		model setModel( self.model );
	else
		model setModel( "tag_origin" );
	model notSolid();

	model codol_setCharacterClone( self );

	model setAnimKnobRestart( walk, 1, 0, 1 );

	print( "CoDOL top players: rank " + ( rank + 1 ) + " client " + self getEntityNumber() + " '" + self.name + "' ent " + model getEntityNumber() + " from " + start + " yaw " + startAngles[1] + " to " + mark.origin + " yaw " + endAngles[1] + " over " + length + " s\n" );

	if ( distanceSquared( start, mark.origin ) > 1 && length > 0.1 )
		model moveTo( mark.origin, length );
	if ( startAngles[1] != endAngles[1] && length > 0.1 )
		model rotateTo( endAngles, length );

	model thread holdPose( self, idle, length );
	return model;
}

holdPose( player, idle, delay )
{
	self endon( "death" );

	player thread dropCloneOnDisconnect( self );

	wait ( delay );
	self setAnimKnobRestart( idle, 1, 0.2, 1 );
}

// A disconnected client's character record is gone; C1 would fall back to
// the model's single xmodel, so take the clone away instead.
dropCloneOnDisconnect( model )
{
	model endon( "death" );
	level endon( "top_players_scene_end" );

	self waittill( "disconnect" );
	model delete();
}
