
// Owned17762 only: real native player/actor, actual authored barrier and fire notes.
owned_window_probe()
{
 wait( 2 );
 p = addTestClient( "Window probe" );
 if ( !isDefined( p ) ) { codolprintln( "WINDOWPROBE BOTFAIL" ); return; }
 for ( waitSpawn = 0; waitSpawn < 30; waitSpawn++ )
 {
  if ( isAlive( p ) && p.sessionstate == "playing" ) break;
  wait( 0.5 );
 }
 maps\_zombiemode_spawner::stop_spawning();
 p freezeControls( true );
 p codol_botPressButtons( 0, 60 );
 wait( 5 );
 node = undefined;
 for ( i = 0; i < level.exterior_goals.size; i++ )
 {
  candidate = level.exterior_goals[ i ];
  if ( !isDefined( candidate.attack_spots ) || candidate.attack_spots.size < 1 ) continue;
  anchor = candidate;
  if ( isDefined( candidate.trigger_location ) ) anchor = candidate.trigger_location;
  if ( distance2d( candidate.attack_spots[ 0 ], anchor.origin ) < 80 && abs( candidate.attack_spots[ 0 ][ 2 ] - anchor.origin[ 2 ] ) < 45 )
  { node = candidate; break; }
 }
 if ( !isDefined( node ) ) { codolprintln( "WINDOWPROBE NODEFAIL" ); return; }
 anchor = node;
 if ( isDefined( node.trigger_location ) ) anchor = node.trigger_location;
 ai = level.zombie_spawner spawnActor();
 if ( !isDefined( ai ) ) { codolprintln( "WINDOWPROBE ACTORFAIL" ); return; }
 ai.ignoreall = true;
 ai.ignoreme = true;
 ai.has_legs = true;
 ai.is_zombie = true;
 ai.meleeDamage = 60;
 ai.animplaybackrate = 1;
 ai.attacking_node = node;
 ai forceTeleport( node.attack_spots[ 0 ], node.angles );
 p setOrigin( anchor.origin );
 wait( 0.5 );
 codolprintln( "WINDOWPROBE BEFORE hp " + p.health + " alive " + isAlive( p ) + " session " + p.sessionstate + " actor " + ai getEntityNumber() + " range " + distance2d( ai.origin, p.origin ) + " floor " + abs( ai.origin[ 2 ] - p.origin[ 2 ] ) + " anchor " + distance2d( anchor.origin, p.origin ) );
 initial = p.health;
 played = ai maps\_zombiemode_spawner::should_attack_player_thru_boards();
 wait( 0.1 );
 codolprintln( "WINDOWPROBE AFTER played " + played + " hp " + p.health + " damage " + ( initial - p.health ) + " laststand " + p.laststand + " contact " + ai.window_melee_contact );
 p setOrigin( anchor.origin + ( 200, 0, 0 ) );
 wait( 0.1 );
 beforeMiss = p.health;
 playedMiss = ai maps\_zombiemode_spawner::should_attack_player_thru_boards();
 codolprintln( "WINDOWPROBE OUTOFRANGE played " + playedMiss + " hp " + p.health + " damage " + ( beforeMiss - p.health ) );
}
