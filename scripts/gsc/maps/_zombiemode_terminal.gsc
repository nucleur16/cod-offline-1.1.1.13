// C1 terminal presentation; no MP compile closure, touring camera or score cards.
// Jan2014 reference xWsKn1NEBt8: GAME OVER + failure, then native scoreboard.
// Authored top_players_camera/loc marks exist in asylum and prototype zones.
// Character clone transport is the existing MP _codol_top_players mechanism.
#using_animtree( "player_3rd_person" );

init()
{
 precacheString( &"ZOMBIE_GAME_OVER" );
 precacheString( &"ZOMBIE_DID_NOT_SURVIVE" );
 precacheModel( "tag_origin" );
}

result_text()
{
 self.zm_terminal_title = newClientHudElem( self );
 self.zm_terminal_title.alignX = "center";
 self.zm_terminal_title.alignY = "middle";
 self.zm_terminal_title.horzAlign = "center";
 self.zm_terminal_title.vertAlign = "middle";
 self.zm_terminal_title.y = -130;
 self.zm_terminal_title.font = "objective";
 self.zm_terminal_title.fontScale = 2.4;
 self.zm_terminal_title.foreground = true;
 self.zm_terminal_title.hidewhendead = false;
 self.zm_terminal_title setText( &"ZOMBIE_GAME_OVER" );
 // Endless has no extraction objective; keep only its Game Over title.
 if ( isDefined( level.zm_round_mode ) && level.zm_round_mode == 3 ) return;
 self.zm_terminal_reason = newClientHudElem( self );
 self.zm_terminal_reason.alignX = "center";
 self.zm_terminal_reason.alignY = "middle";
 self.zm_terminal_reason.horzAlign = "center";
 self.zm_terminal_reason.vertAlign = "middle";
 self.zm_terminal_reason.y = -90;
 self.zm_terminal_reason.font = "objective";
 self.zm_terminal_reason.fontScale = 1.6;
 self.zm_terminal_reason.foreground = true;
 self.zm_terminal_reason.hidewhendead = false;
 self.zm_terminal_reason setText( &"ZOMBIE_DID_NOT_SURVIVE" );
}

clear_text()
{
 if ( isDefined( self.zm_terminal_title ) ) self.zm_terminal_title destroy();
 if ( isDefined( self.zm_terminal_reason ) ) self.zm_terminal_reason destroy();
 self.zm_terminal_title = undefined;
 self.zm_terminal_reason = undefined;
}

// C1 pm_type6 enters its automatic scoreboard path (43B7E7 ->43B450),
// using final native rows (showscoreboard itself only refreshes those rows).
// Intermission renders ps.origin directly, without normal stance eye height.
// Promote the current eye to the camera origin before entering that path.
defeat()
{
 players = getPlayers();
 foreach ( player in players ) player result_text();
 level.zm_terminal_phase = "game_over";
 wait( 2 );
 foreach ( player in getPlayers() )
 {
  view_origin = player getEye();
  player.sessionstate = "intermission";
  player setOrigin( view_origin );
  player showScoreboard();
 }
 level.zm_terminal_phase = "scoreboard";
 wait( 2 );
 foreach ( player in getPlayers() ) player clear_text();
 fade_black( 1 );
 level.zm_terminal_phase = "black";
 wait( 1 );
}

fade_black( seconds )
{
 // Shipped common_mp black_bw vision: filmEnable1, filmBrightness-1.
 // World fade remains supported even when normal HUD menus are hidden.
 foreach ( player in getPlayers() )
  player VisionSetNakedForPlayer( "black_bw", seconds );
 wait( seconds );
}

podium_marks()
{
 if ( !isDefined( level.struct ) ) return undefined;
 camera = undefined;marks = [];
 foreach ( point in level.struct )
 {
  if ( !isDefined( point.targetname ) ) continue;
  if ( point.targetname == "top_players_camera" ) camera = point;
  if ( point.targetname == "top_players_loc" ) marks[ marks.size ] = point;
 }
 if ( !isDefined( camera ) || !marks.size ) return undefined;
 // Keep authored positions, including the fourth mark which ships with a
 // duplicate index2. Stable index ordering makes index0 the first position.
 for ( i = 1; i < marks.size; i++ )
 {
  value = marks[i];j = i - 1;
  while ( j >= 0 && int( marks[j].script_index ) > int( value.script_index ) )
  { marks[j+1] = marks[j];j--; }
  marks[j+1] = value;
 }
 scene = spawnStruct();scene.camera = camera;scene.marks = marks;
 return scene;
}

// Frame the authored group without assuming a server clone skeleton. C1 can
// compose a visible character from its client record while server DObj stays
// tag_origin. The map's camera-to-mark height is the explicit design scale.
podium_camera( clones, mark )
{
 if ( !clones.size ) return undefined;
 floor_center = ( 0, 0, 0 );
 foreach ( model in clones ) floor_center += model.origin;
 floor_center = floor_center * ( 1.0 / clones.size );
 height = mark.origin[2] - floor_center[2];
 if ( !( height > 0 ) )
 {
  // No invented extent when authored camera is level/below the marks.
  // Fail closed to the unchanged authored static view, not a zero/NaN dolly.
  camera = spawn( "script_model", mark.origin );
  camera setModel( "tag_origin" );camera.angles = mark.angles;camera notSolid();
  return camera;
 }
 target = floor_center + ( 0, 0, height );
 radius = 0;
 foreach ( model in clones )
 {
  root_distance = distance( target, model.origin );
  top_distance = distance( target, model.origin + ( 0, 0, height * 2 ) );
  if ( root_distance > radius ) radius = root_distance;
  if ( top_distance > radius ) radius = top_distance;
 }
 authored_distance = distance( mark.origin, target );
 // A directly overhead authored camera can coincide with the eye-line target.
 // Hold its authored static view instead of normalizing a zero vector.
 if ( !( authored_distance > 0 ) )
 {
  camera = spawn( "script_model", mark.origin );
  camera setModel( "tag_origin" );camera.angles = mark.angles;camera notSolid();
  return camera;
 }
 direction = vectorNormalize( mark.origin - target );
 start_distance = radius * 4.5;
 if ( authored_distance < start_distance ) start_distance = authored_distance;
 end_distance = radius * 3;
 // Never shrink the envelope merely to force a dolly. If the authored view
 // is already too close, retain it rather than manufacture a smaller bound.
 if ( !( start_distance > end_distance ) )
 {
  camera = spawn( "script_model", mark.origin );
  camera setModel( "tag_origin" );camera.angles = mark.angles;camera notSolid();
  return camera;
 }
 start = target + direction * start_distance;
 finish = target + direction * end_distance;
 camera = spawn( "script_model", start );
 camera setModel( "tag_origin" );camera notSolid();
 camera.angles = vectorToAngles( target - start );
 camera moveTo( finish, 15, 15 * 0.25, 15 * 0.4 );
 return camera;
}

podium()
{
 // All end_game cleanup listeners must finish before the new camera is linked.
 waittillframeend;
 scene = podium_marks();
 // Training contains no authored podium. Do not invent a placement there.
 if ( !isDefined( scene ) ) { fade_black( 1 );return; }
 level.zm_terminal_phase = "podium";
 clones = [];players = getPlayers();
 for ( i = 0; i < players.size && i < scene.marks.size; i++ )
 {
  player = players[i];mark = scene.marks[i];
  model = spawn( "script_model", mark.origin );
  model.angles = mark.angles;
  model setModel( "tag_origin" );model notSolid();
  model.zm_weapons = model podium_clone_weapons( player );
  // Native ZM idle, not MP victory walkup or a fabricated player pose.
  model setAnimKnobRestart( %post_game_walkup_cowboy_pistol_idle, 1, 0, 1 );
  clones[clones.size] = model;
 }
 if ( !clones.size ) { fade_black( 1 );return; }
 camera = podium_camera( clones, scene.camera );
 foreach ( player in getPlayers() )
 {
  player setClientDvar( "cg_topPlayersCards", 0 );
  // C1 special s handler468A64 updates cg.drawHud; this is not an alias guess.
  player setClientDvar( "hud_drawHud", 0 );
  player closeInGameMenu();player closepopupMenu();
  player.sessionstate = "spectator";player.spectatorclient = -1;
  player.killcamentity = -1;player.archivetime = 0;player.psoffsettime = 0;
  player cameraLinkTo( camera, "tag_origin" );
  player freezeControls( true );
 }
 wait( 15 );
 fade_black( 1 );
 level.zm_terminal_phase = "black";
 foreach ( player in getPlayers() )
 {
  // Restore archived HUD state only after the world has completed black fade.
  player setClientDvar( "hud_drawHud", 1 );
  player cameraUnlink();
  player.zm_rescue_held_primary = undefined;
 }
 foreach ( model in clones )
 {
  foreach ( weapon in model.zm_weapons ) if ( isDefined( weapon ) ) weapon delete();
  model delete();
 }
 camera delete();
}

// self = podium model. Automatic world guns remain the first choice.
// A freshly released helicopter rider can still be holstered at this frame.
// The named clone argument links the same owned weapon without granting it.
podium_clone_weapons( player )
{
 weapons = self codol_setCharacterClone( player );
 if ( weapons.size || !isDefined( player.zm_extracted ) || !player.zm_extracted ||
      !isDefined( player.zm_rescue_held_primary ) ) return weapons;
 held = player.zm_rescue_held_primary;
 if ( held == "" || held == "none" || !player hasWeapon( held ) ) return weapons;
 return self codol_setCharacterClone( player, held );
}

// Stock Scaleform MatchController reads show once on BACK_FROM_GAME, resets
// it immediately, then opens PAGE_ADV_POPUP_LOBBY_SUMMARY after200/1200ms.
// MainMenuAARController reads victory; rewards remain native report authority.
publish_results()
{
 victory = isDefined( level.zm_extraction_result ) && level.zm_extraction_result == "win";
 foreach ( player in getPlayers() )
 {
  player setClientDvar( "ui_eog_zm_mode", "" + level.zm_round_mode );
  player setClientDvar( "ui_eog_victory", int( victory ) );
  // The shipped consumer deliberately excludes the training map.
  player setClientDvar( "ui_eog_show", int( getDvar( "mapname" ) != "zm_zombie_training" ) );
 }
}
