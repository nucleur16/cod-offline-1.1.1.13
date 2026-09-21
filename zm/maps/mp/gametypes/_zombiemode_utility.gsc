// zm shared helpers.
//
// Port of the parts of T5's maps\_zombiemode_utility.gsc (3,941 lines) that the
// gameplay loop actually calls, plus the handful of helpers the loop takes from
// T5's maps\_utility / common_scripts\utility.  Provenance is named per
// function; line numbers are into
// D:\CoD\COD-GSC-Source\T5-STEAM-GSC\maps\_zombiemode_utility.gsc unless
// another file is given.
//
// WHY THIS IS NOT common_scripts\utility.  The shared tree already ships
// flag_*, getstruct and array_thread in common_scripts/utility.gsc, and its
// compile closure is only four files, all of them common_scripts -- MEASURED
// 2026-09-20 with tools/codol_script_resolve.py --only-seeds.  It is still not
// used here: that file is load-bearing for the user's live multiplayer matches
// and this workstream owns nothing outside zm/.  Everything below is local, so
// a zm change can never reach an mp match.
//
// NO ANIMATION.  Nothing in this file or any file that includes it calls
// setanim, setanimknob, animscripted, useanimtree or a %anim token.  Those
// builtins are not registered by this server (MEASURED: absent from the 929
// names tools/codol_script_resolve.py extracts from src/, and absent from
// bin/codol_dedrun_next.exe), and naming one is a COMPILE error that kills the
// match at map load.

// --------------------------------------------------------------------------
// Tuning table.  T5 _zombiemode_utility.gsc:1331 set_zombie_var().
//
// mp/zombiemode.csv ships in common_gametype_zm.ff, 5 columns x 106 rows,
// NAME | EASY | MEDIUM | HARD | VETERAN.  T5 reads it as
// TableLookUp( table, 0, name, difficulty + 1 ); difficulty 1 => column 2 in
// T5's indexing, which is column 1 counting the name column as 0 -- the same
// cell this server's tableLookup( table, 0, name, 1 ) returns.
//
// tableLookup answers "" for a missing table or row and never errors
// (src/codol/script/codol_script_table.c:77-98), so an absent
// mp/zombiemode.csv degrades to the supplied defaults.
// --------------------------------------------------------------------------

zombie_table()
{
	return "mp/zombiemode.csv";
}

set_zombie_var( var, value, is_float, column )
{
	if ( !isDefined( is_float ) )
		is_float = false;
	if ( !isDefined( column ) )
		column = 1;

	if ( !isDefined( level.zombie_vars ) )
		level.zombie_vars = [];

	table_value = tableLookup( zombie_table(), 0, var, column );
	if ( isDefined( table_value ) && table_value != "" )
	{
		if ( is_float )
			value = float( table_value );
		else
			value = int( table_value );
	}

	level.zombie_vars[ var ] = value;
	return value;
}

get_zombie_var( var )
{
	return level.zombie_vars[ var ];
}

// --------------------------------------------------------------------------
// Flags.  common_scripts\utility::flag_init/flag_set/flag_clear/flag/flag_wait,
// reimplemented locally (see the header note).  Same contract: a flag must be
// init'ed before it is read, setting a set flag is a no-op, and flag_wait
// returns immediately when the flag is already set.
// --------------------------------------------------------------------------

flag_init( message )
{
	if ( !isDefined( level.flag ) )
	{
		level.flag = [];
		level.flags_lock = [];
	}

	if ( !isDefined( level.flag[ message ] ) )
		level.flag[ message ] = false;
}

flag_exist( message )
{
	if ( !isDefined( level.flag ) )
		return false;
	return isDefined( level.flag[ message ] );
}

flag( message )
{
	if ( !isDefined( level.flag ) || !isDefined( level.flag[ message ] ) )
		return false;
	return level.flag[ message ];
}

flag_set( message )
{
	flag_init( message );

	if ( level.flag[ message ] )
		return;

	level.flag[ message ] = true;
	level notify( message );
}

flag_clear( message )
{
	flag_init( message );

	if ( !level.flag[ message ] )
		return;

	level.flag[ message ] = false;
	level notify( message );
}

flag_wait( message )
{
	flag_init( message );

	for ( ;; )
	{
		if ( level.flag[ message ] )
			return;
		level waittill( message );
	}
}

flag_waitopen( message )
{
	flag_init( message );

	for ( ;; )
	{
		if ( !level.flag[ message ] )
			return;
		level waittill( message );
	}
}

// --------------------------------------------------------------------------
// Structs.  common_scripts\utility::getstruct / getstructarray, reimplemented
// against level.struct directly instead of the level.struct_class_names index
// the shared tree builds in struct_class_init() -- building that index here
// would mean owning its lifetime too, and the zm maps carry 185 script_structs
// (docs/CODOL_ZOMBIES_STARTUP_20260915.md section 3.6), so a linear scan at
// init time is not worth an index.
//
// level.struct is the ENGINE's array: G_LoadStructs (src/asmsource/
// _g_spawn_mp.asm:540-600) walks the map entity string, and for every entity
// whose classname is script_struct calls codescripts\struct::createstruct and
// copies the entity's spawn vars onto the returned struct.  The two labels are
// bound at script load with errorIfMissing=1 (src/scr_vm_main.c:1433-1434), so
// this data is present for every asset category, zm included.  INFERRED from
// that assembly, not measured on a zm map.
// --------------------------------------------------------------------------

get_struct_array( name, type )
{
	array = [];

	if ( !isDefined( level.struct ) )
		return array;

	for ( i = 0; i < level.struct.size; i++ )
	{
		s = level.struct[ i ];
		if ( !isDefined( s ) )
			continue;

		value = undefined;
		if ( type == "targetname" )
			value = s.targetname;
		else if ( type == "target" )
			value = s.target;
		else if ( type == "script_noteworthy" )
			value = s.script_noteworthy;
		else if ( type == "script_linkname" )
			value = s.script_linkname;
		else if ( type == "script_string" )
			value = s.script_string;

		if ( isDefined( value ) && value == name )
			array[ array.size ] = s;
	}

	return array;
}

get_struct( name, type )
{
	array = get_struct_array( name, type );
	if ( array.size < 1 )
		return undefined;
	return array[ 0 ];
}

// --------------------------------------------------------------------------
// Arrays and misc.  T5 maps\_utility / common_scripts\utility.
// --------------------------------------------------------------------------

is_true( value )
{
	if ( !isDefined( value ) )
		return false;
	if ( value )
		return true;
	return false;
}

array_thread_1( entities, process )
{
	for ( i = 0; i < entities.size; i++ )
		entities[ i ] thread [[ process ]]();
}

array_randomize( array )
{
	for ( i = 0; i < array.size; i++ )
	{
		j = randomInt( array.size );
		temp = array[ i ];
		array[ i ] = array[ j ];
		array[ j ] = temp;
	}
	return array;
}

// common_scripts\utility::vector_scale.  Not a builtin on this server
// (MEASURED: "vectorscale" is absent from the registered table).
vector_scale( vec, scale )
{
	return ( vec[ 0 ] * scale, vec[ 1 ] * scale, vec[ 2 ] * scale );
}

// common_scripts\utility::getClosest.  Not a builtin here either.
get_closest( org, array )
{
	if ( array.size < 1 )
		return undefined;

	best = array[ 0 ];
	best_dist = distanceSquared( org, best.origin );

	for ( i = 1; i < array.size; i++ )
	{
		d = distanceSquared( org, array[ i ].origin );
		if ( d < best_dist )
		{
			best_dist = d;
			best = array[ i ];
		}
	}

	return best;
}

// T5 maps\_utility::groundpos, minus the water variant (no depthinwater()
// builtin here, and no zm map ships water volumes).
groundpos( origin )
{
	trace = bulletTrace( origin + ( 0, 0, 16 ), origin - ( 0, 0, 20000 ), false, undefined );
	return trace[ "position" ];
}

// T5 calls wait_network_frame() to let one server frame's worth of entity
// state reach the client before the next change.  It is an engine builtin
// there and is not registered here; this server ticks at 30-60 Hz, so one
// 0.05s wait is at least one frame at either rate.
wait_network_frame()
{
	wait( 0.05 );
}

// common_scripts\utility::waittill_either.
waittill_either( msg1, msg2 )
{
	self endon( msg1 );
	self waittill( msg2 );
}

// T5 _zombiemode_utility.gsc:227,237.
round_up_to_ten( score )
{
	score = int( score );
	new_score = score - score % 10;
	if ( new_score < score )
		new_score += 10;
	return new_score;
}

round_up_score( score, value )
{
	score = int( score );
	new_score = score - score % value;
	if ( new_score < score )
		new_score += value;
	return new_score;
}

// --------------------------------------------------------------------------
// Player predicates.  T5 _zombiemode_utility.gsc:1050 is_player_valid().
//
// Three of T5's seven tests are dropped because the builtin behind them is
// not registered here: isnotarget(), maps\_laststand::player_is_in_laststand()
// (replaced by the script-side self.laststand this port maintains) and
// player.is_zombie (the zombify-the-player mode is not ported).
// --------------------------------------------------------------------------

is_player_valid( player )
{
	if ( !isDefined( player ) )
		return false;
	if ( !isPlayer( player ) )
		return false;
	if ( !isAlive( player ) )
		return false;
	if ( is_true( player.laststand ) )
		return false;
	if ( isDefined( player.sessionstate ) && player.sessionstate == "spectator" )
		return false;
	if ( isDefined( player.sessionstate ) && player.sessionstate == "intermission" )
		return false;
	return true;
}

get_number_of_valid_players()
{
	players = getPlayers();
	count = 0;
	for ( i = 0; i < players.size; i++ )
	{
		if ( is_player_valid( players[ i ] ) )
			count++;
	}
	return count;
}

// T5 _zombiemode_utility.gsc:1120.  self is the entity being tested.
in_revive_trigger()
{
	players = getPlayers();
	for ( i = 0; i < players.size; i++ )
	{
		if ( !isDefined( players[ i ] ) || !isAlive( players[ i ] ) )
			continue;
		if ( !isDefined( players[ i ].revivetrigger ) )
			continue;
		if ( self isTouching( players[ i ].revivetrigger ) )
			return true;
	}
	return false;
}

// T5 reads the use button through use_button_held(); useButtonPressed() is the
// registered equivalent here.
use_button_held()
{
	return self useButtonPressed();
}

// --------------------------------------------------------------------------
// Enemy accounting.  T5 _zombiemode_utility.gsc:51 get_enemy_count() calls
// GetAiSpeciesArray( "axis", "all" ), which this server does not register
// (MEASURED: no getaiarray / getaispeciesarray / getaiteamarray).  The spawner
// therefore keeps its own live list, and this counts it.
// --------------------------------------------------------------------------

get_enemy_count()
{
	if ( !isDefined( level.zombies ) )
		return 0;

	count = 0;
	for ( i = 0; i < level.zombies.size; i++ )
	{
		z = level.zombies[ i ];
		if ( !isDefined( z ) )
			continue;
		if ( is_true( z.ignore_enemy_count ) )
			continue;
		count++;
	}
	return count;
}

// Drops the dead and the deleted out of level.zombies.  Called by the spawner
// on every death notify; also called by the round loop as a backstop.
zombie_list_compact()
{
	if ( !isDefined( level.zombies ) )
	{
		level.zombies = [];
		return;
	}

	alive = [];
	for ( i = 0; i < level.zombies.size; i++ )
	{
		if ( !isDefined( level.zombies[ i ] ) )
			continue;
		alive[ alive.size ] = level.zombies[ i ];
	}
	level.zombies = alive;
}

// --------------------------------------------------------------------------
// HUD.  T5 _zombiemode_utility.gsc:111 create_simple_hud / :131 destroy_hud /
// :3208 create_counter_hud.
// --------------------------------------------------------------------------

create_simple_hud( client )
{
	if ( isDefined( client ) )
		hud = newClientHudElem( client );
	else
		hud = newHudElem();

	if ( !isDefined( level.hudelem_count ) )
		level.hudelem_count = 0;
	level.hudelem_count++;

	hud.foreground = true;
	hud.sort = 1;
	hud.hidewheninmenu = false;

	return hud;
}

destroy_hud()
{
	level.hudelem_count--;
	self destroy();
}

create_counter_hud( x )
{
	if ( !isDefined( x ) )
		x = 0;

	hud = create_simple_hud();
	hud.alignX = "left";
	hud.alignY = "top";
	hud.horzAlign = "user_left";
	hud.vertAlign = "user_top";
	hud.color = ( 1, 1, 1 );
	/* C1 scale; see _zombiemode_score.gsc. T5's 32 is its largest
	 * announcement size, so this takes the largest the mp tree uses. */
	hud.fontscale = 3;
	hud.x = x;
	hud.alpha = 0;

	return hud;
}

// --------------------------------------------------------------------------
// Sounds.  T5 _zombiemode_utility.gsc:2499-2560.
//
// Aliases are looked up through level.zombie_sounds exactly as T5 does, and an
// unregistered ref is a silent return rather than T5's AssertMsg: a zm zone
// that does not carry the alias is the expected case here, not a content bug.
// --------------------------------------------------------------------------

add_sound( ref, alias )
{
	if ( !isDefined( level.zombie_sounds ) )
		level.zombie_sounds = [];
	level.zombie_sounds[ ref ] = alias;
}

play_sound_at_pos( ref, pos )
{
	if ( !isDefined( ref ) || ref == "none" )
		return;
	if ( !isDefined( level.zombie_sounds ) || !isDefined( level.zombie_sounds[ ref ] ) )
		return;
	playSoundAtPosition( level.zombie_sounds[ ref ], pos );
}

play_sound_on_ent( ref )
{
	if ( isDefined( self.script_soundalias ) )
	{
		self playSound( self.script_soundalias );
		return;
	}

	if ( isDefined( self.script_sound ) )
		ref = self.script_sound;

	if ( !isDefined( ref ) || ref == "none" )
		return;
	if ( !isDefined( level.zombie_sounds ) || !isDefined( level.zombie_sounds[ ref ] ) )
		return;

	self playSound( level.zombie_sounds[ ref ] );
}

// --------------------------------------------------------------------------
// Hint strings.  T5 _zombiemode_utility.gsc:2483 set_hint_string() resolves a
// hint REF through a localized-string table; C1 has no such table for zm and
// setHintString takes a plain string here, so the refs are formatted directly.
// self is the trigger, ent is the entity whose script_hint may override.
// --------------------------------------------------------------------------

set_hint_string( ent, default_string )
{
	if ( isDefined( ent ) && isDefined( ent.script_hint ) )
		self setHintString( ent.script_hint );
	else
		self setHintString( default_string );
}

// --------------------------------------------------------------------------
// Triggers.  T5 _zombiemode_utility.gsc:1772,1781 -- a trigger is disabled by
// dropping it 10,000 units, because there is no TriggerEnable in T5 either.
// The same trick is used here: "triggerenable" is not registered on this
// server (MEASURED).
// --------------------------------------------------------------------------

disable_trigger()
{
	if ( !isDefined( self.disabled ) || !self.disabled )
	{
		self.disabled = true;
		self.origin = self.origin - ( 0, 0, 10000 );
	}
}

enable_trigger()
{
	if ( !isDefined( self.disabled ) || !self.disabled )
		return;

	self.disabled = false;
	self.origin = self.origin + ( 0, 0, 10000 );
}

// --------------------------------------------------------------------------
// Barrier chunk state.  T5 _zombiemode_utility.gsc:139,154,1906 plus
// _zombiemode_blockers.gsc:2920 update_states().
//
// T5 keeps the state in a per-chunk array indexed by a state name so the
// animation layer can ask "mid_tear"; with no animation layer the three states
// that matter are repaired / mid_tear / destroyed.
// --------------------------------------------------------------------------

update_states( state )
{
	self.chunk_state = state;
}

get_chunk_state()
{
	if ( !isDefined( self.chunk_state ) )
		return "repaired";
	return self.chunk_state;
}

all_chunks_intact( barrier_chunks )
{
	for ( i = 0; i < barrier_chunks.size; i++ )
	{
		if ( barrier_chunks[ i ] get_chunk_state() != "repaired" )
			return false;
	}
	return true;
}

no_valid_repairable_boards( barrier_chunks )
{
	for ( i = 0; i < barrier_chunks.size; i++ )
	{
		if ( barrier_chunks[ i ] get_chunk_state() == "destroyed" )
			return false;
	}
	return true;
}

all_chunks_destroyed( barrier_chunks )
{
	for ( i = 0; i < barrier_chunks.size; i++ )
	{
		if ( barrier_chunks[ i ] get_chunk_state() != "destroyed" )
			return false;
	}
	return true;
}

get_destroyed_chunks( barrier_chunks )
{
	array = [];
	for ( i = 0; i < barrier_chunks.size; i++ )
	{
		if ( barrier_chunks[ i ] get_chunk_state() == "destroyed" )
			array[ array.size ] = barrier_chunks[ i ];
	}
	return array;
}

get_random_destroyed_chunk( barrier_chunks )
{
	chunks = get_destroyed_chunks( barrier_chunks );
	if ( chunks.size < 1 )
		return undefined;
	return chunks[ randomInt( chunks.size ) ];
}
