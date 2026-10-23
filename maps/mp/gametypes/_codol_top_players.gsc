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
//   Weapons   codol_setCharacterClone also links the player's world guns to
//             the clone at the same hand tags (C1 0x55b5e0 does the same), or
//             the named weapon when the player holds none any more.
//   Camera    the view starts scr_codol_topplayers_dolly units behind
//             top_players_camera and dollies onto it over the walk. Viewers
//             are spectators camera-linked to a moving tag_origin model, so
//             C1 interpolates the camera from the entity's trajectory; an
//             intermission view cannot move smoothly (C1 0x476848 draws
//             pm_type 6 from the snapshot origin and skips the camera link).
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
	// How far behind top_players_camera the shot starts; it dollies in over
	// the walk-up. 0 keeps the camera still.
	if ( getDvar( "scr_codol_topplayers_dolly" ) == "" )
		setDvar( "scr_codol_topplayers_dolly", 64 );

	precacheModel( "tag_origin" );

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

// The scene's look, the same whether or not a final killcam ran before it.
// endGame puts the whole level in the desaturated "mpOutro" vision and
// displayGameEnd draws the outcome (title, reason, team scores) for every
// player. With a final killcam both are gone by the time the scene starts:
// endGame sends reset_outcome before the killcam, which destroys the outcome
// elements, and _damage::doFinalKillcam gives every player the map's own
// vision. Without one (a match ended by time, an objective, a bomb or a
// forfeit; dom, koth, ctf, oneflag and sab never have a final kill) nothing
// cleared them, so the scene was black and white under the outcome. This does
// what the killcam path does, for each player as it joins the shot (a late
// joiner gets mpOutro from the gamestate).
sceneLook()
{
	self notify( "reset_outcome" );
	self VisionSetNakedForPlayer( getDvar( "mapname" ), 0 );
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

// The match's best players overall, regardless of team, ranked at the moment
// the scene starts the way the scoreboard ranks them: score, then fewer
// deaths (_gamescore::getBetterPlayer), then the lower client number so equal
// lines always resolve the same way. Built from level.players here rather
// than read from level.placement, which is only as fresh as its last
// updatePlacement. Spectators never place.
pickTopPlayers( count )
{
	ranked = [];
	foreach ( player in level.players )
	{
		if ( !isDefined( player ) || isDefined( player.connectedPostGame ) )
			continue;
		// A podium source must have completed a real combat spawn.
		if ( !isDefined( player.hasSpawned ) || !player.hasSpawned )
			continue;
		if ( !isDefined( player.pers["team"] ) || ( player.pers["team"] != "allies" && player.pers["team"] != "axis" ) )
			continue;
		ranked[ ranked.size ] = player;
	}

	for ( i = 1; i < ranked.size; i++ )
	{
		player = ranked[i];
		for ( j = i - 1; j >= 0 && rankedAbove( player, ranked[j] ); j-- )
			ranked[ j + 1 ] = ranked[j];
		ranked[ j + 1 ] = player;
	}

	top = [];
	for ( i = 0; i < ranked.size && top.size < count; i++ )
		top[ top.size ] = ranked[i];

	if ( getDvarInt( "scr_codol_topplayers_trace" ) )
	{
		for ( i = 0; i < ranked.size; i++ )
			{}
	}
	return top;
}

rankedAbove( a, b )
{
	if ( a.score != b.score )
		return a.score > b.score;
	if ( a.deaths != b.deaths )
		return a.deaths < b.deaths;
	return a getEntityNumber() < b getEntityNumber();
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
		{}
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

	longest = 0;
	for ( rank = 0; rank < top.size; rank++ )
	{
		length = getAnimLength( level.topPlayersWalk[ rank ] );
		if ( length > longest )
			longest = length;
	}

	// The clones first, while the players still hold their guns: their world
	// guns are what the clones are given.
	for ( rank = 0; rank < top.size; rank++ )
		level.topPlayersModels[ rank ] = top[ rank ] spawnWalker( rank, scene.marks[ rank ] );

	level.topPlayersCamera = spawnCamera( scene.camera, longest );

	// C1 builds a clone by copying its source player's entity DObj (see
	// codol_sendToAll), so the three stay players - out of sight below the
	// camera, sent to everyone - until every client has built its clones,
	// and only then join the others behind the camera.
	for ( rank = 0; rank < top.size; rank++ )
		top[ rank ] thread holdAsSource( level.topPlayersCamera, scene.camera.origin, rank );

	foreach ( player in level.players )
	{
		if ( !isDefined( player.topPlayersRank ) )
			player viewFromCamera( level.topPlayersCamera );
	}

	{}

	// The cards are up for the whole scene (viewFromCamera and holdAsSource
	// each put them up for their player as it joins the shot).
	wait ( longest );
	wait ( getDvarFloat( "scr_codol_topplayers_hold" ) );

	level.topPlayersSceneActive = false;
	level notify( "top_players_scene_end" );

	foreach ( player in level.players )
	{
		player setClientDvar( "cg_topPlayersCards", 0 );
		player cameraUnlink();
		player.topPlayersRank = undefined;
	}

	foreach ( model in level.topPlayersModels )
	{
		if ( isDefined( model ) )
			model removeClone();
	}
	level.topPlayersModels = [];

	if ( isDefined( level.topPlayersCamera ) )
		level.topPlayersCamera delete();
	level.topPlayersCamera = undefined;
}

// The moving camera: a tag_origin model starting behind top_players_camera
// along its view direction, moved onto it over the walk with ease in and out.
// moveTo publishes a trajectory, which C1 evaluates every frame.
spawnCamera( camera, duration )
{
	dolly = getDvarFloat( "scr_codol_topplayers_dolly" );
	start = camera.origin - anglesToForward( camera.angles ) * dolly;

	ent = spawn( "script_model", start );
	ent setModel( "tag_origin" );
	ent.angles = camera.angles;
	ent notSolid();

	if ( dolly != 0 && duration > 0.1 )
		ent moveTo( camera.origin, duration, duration * 0.25, duration * 0.4 );
	return ent;
}

// _playerlogic::spawnIntermission's preparation, then a frozen spectator at
// the camera's position, camera-linked to it (as _remotemissile links the
// view to the missile).
viewFromCamera( camera )
{
	self notify( "spawned" );
	self notify( "end_respawn" );

	self maps\mp\gametypes\_playerlogic::setSpawnVariables();
	self closepopupMenu();
	self closeInGameMenu();
	self clearLowerMessages();
	self freezeControlsWrapper( true );

	self.sessionstate = "spectator";
	self.spectatorclient = -1;
	self.killcamentity = -1;
	self.archivetime = 0;
	self.psoffsettime = 0;
	self ClearKillcamState();
	self.friendlydamage = undefined;

	// No following and no free look: the scene is the only view.
	self allowSpectateTeam( "allies", false );
	self allowSpectateTeam( "axis", false );
	self allowSpectateTeam( "freelook", false );
	self allowSpectateTeam( "none", true );

	self sceneLook();
	self spawn( camera.origin, camera.angles );
	self freezeControlsWrapper( true );
	self cameraLinkTo( camera, "tag_origin" );
	self showCards();
	self setDepthOfField( 0, 128, 512, 4000, 6, 1.8 );
}

// scr_codol_topplayers_trace 2: the server origin every server frame against
// where the clip's own motion has the model at that time.
sampleWalk( walk, length, start, startAngles )
{
	self endon( "death" );
	began = getTime();
	for ( ;; )
	{
		t = ( getTime() - began ) / 1000;
		if ( t > length + 0.1 )
			return;
		fraction = t / length;
		if ( fraction > 1 )
			fraction = 1;
		{}
		wait ( 0.001 );
	}
}

// A top player during the first moments of the scene: still a player, so
// clients hold the entity DObj its clone is copied from, but frozen below the
// camera where the shot cannot see it, sent to every client whatever its PVS,
// and looking through the scene camera like everyone else.
holdAsSource( camera, cameraOrigin, rank )
{
	self endon( "disconnect" );
	level endon( "top_players_scene_end" );

	self.topPlayersRank = rank;

	self notify( "spawned" );
	self notify( "end_respawn" );
	self maps\mp\gametypes\_playerlogic::setSpawnVariables();
	self closepopupMenu();
	self closeInGameMenu();
	self clearLowerMessages();

	self.sessionstate = "playing";
	self.spectatorclient = -1;
	self.killcamentity = -1;
	self.archivetime = 0;
	self.psoffsettime = 0;
	self ClearKillcamState();
	self.friendlydamage = undefined;
	if ( !isDefined( self.maxhealth ) || self.maxhealth <= 0 )
		self.maxhealth = 100;
	self.health = self.maxhealth;

	hidden = cameraOrigin - ( 0, 0, 256 ) + anglesToRight( camera.angles ) * ( ( rank - 1 ) * 40 );
	self sceneLook();
	self spawn( hidden, camera.angles );
	self freezeControlsWrapper( true );
	self codol_sendToAll( 3000 );
	self cameraLinkTo( camera, "tag_origin" );
	self showCards();

	if ( getDvarInt( "scr_codol_topplayers_trace" ) )
		{}

	// Several snapshots at 20 Hz and more: every client has had the source
	// and the clone in the same snapshot by then.
	wait ( 1.5 );

	self codol_sendToAll( 0 );
	self viewFromCamera( camera );
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

	// The player's own world guns, in their hands; a player who holds none
	// any more (dead after the final killcam) gets the last weapon they had.
	model.weapons = model codol_setCharacterClone( self );
	if ( !model.weapons.size )
	{
		weapon = self lastHeldWeapon();
		if ( isDefined( weapon ) )
			model.weapons = model codol_setCharacterClone( self, weapon );
	}

	model setAnimKnobRestart( walk, 1, 0, 1 );

	{}

	model thread walkUp( self, walk, idle, length, start, startAngles );
	return model;
}

lastHeldWeapon()
{
	if ( isDefined( self.lastDroppableWeapon ) && self.lastDroppableWeapon != "none" && self.lastDroppableWeapon != "" )
		return self.lastDroppableWeapon;
	if ( isDefined( self.primaryWeapon ) && self.primaryWeapon != "none" && self.primaryWeapon != "" )
		return self.primaryWeapon;
	return undefined;
}

// The clone and the guns linked to it.
removeClone()
{
	if ( isDefined( self.weapons ) )
	{
		foreach ( weapon in self.weapons )
		{
			if ( isDefined( weapon ) )
				weapon delete();
		}
	}
	self delete();
}

// The clip carries its displacement as root motion, which neither the server
// DObj nor C1 applies to a script model's origin (the delta is not part of
// the bone pose), so the model is carried along the clip's own path: every
// step it moves, on a linear trajectory C1 interpolates, to where the clip's
// accumulated motion puts it at the end of that step. A single moveTo over
// the whole clip is linear, while the walk starts and stops with the steps,
// which slid the feet. The clip was restarted at time 0 on this same frame
// (setAnimKnobRestart in spawnWalker), so path and pose share one clock; the
// idle takes over with a short blend on the frame the walk ends.
walkUp( player, walk, idle, length, start, startAngles )
{
	self endon( "death" );

	player thread dropCloneOnDisconnect( self );

	step = 0.1;
	trace = getDvarInt( "scr_codol_topplayers_trace" );
	if ( trace >= 2 )
		self thread sampleWalk( walk, length, start, startAngles );
	elapsed = 0;
	while ( length - elapsed > 0.001 )
	{
		next = elapsed + step;
		if ( next > length )
			next = length;
		// Aim one step past the next update: each segment is still moving
		// when its replacement arrives, so a snapshot that lands a frame
		// late never shows the model stopped, and consecutive segments
		// average overlapping windows, so the speed changes gradually.
		ahead = next + step;
		if ( ahead > length )
			ahead = length;
		fraction = ahead / length;
		target = start + animDeltaToWorld( getMoveDelta( walk, 0, fraction ), startAngles );
		yaw = startAngles[1] + getAngleDelta( walk, 0, fraction );
		duration = ahead - elapsed;

		self moveTo( target, duration );
		if ( yaw != self.angles[1] )
			self rotateTo( ( 0, yaw, 0 ), duration );

		if ( trace )
			{}

		wait ( next - elapsed );
		elapsed = next;
	}
	if ( trace )
		{}
	self setAnimKnobRestart( idle, 1, 0.2, 1 );
}

// A disconnected client's character record is gone; C1 would fall back to
// the model's single xmodel, so take the clone away instead.
dropCloneOnDisconnect( model )
{
	model endon( "death" );
	level endon( "top_players_scene_end" );

	self waittill( "disconnect" );
	model removeClone();
}
