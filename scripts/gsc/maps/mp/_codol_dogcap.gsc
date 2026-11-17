// Developer-only MP attack-dog telemetry (docs/CODOL_MP_DOGS_T5_PARITY.md).
//
// The same capture runs in real T5 MP (Plutonium t5mp, as
// scripts/mp/zz_t5mpdogcap.gsc with a T5 platform block) and here, so the two
// logs line up field for field: per frame the target player, each dog's
// origin, yaw, health, enemy and animscript, the dog clip timeline; events
// for spawn, damage, death, deletion, bites (player damage), the owner's
// kills.  Started from maps\mp\_dogs::init; inactive unless the dvar dc_scn
// is set.  Output: fs_fopen( dc_file ).  Scenario dvars: dc_dur, dc_delay,
// dc_px/py/pz, dc_freeze, dc_path, dc_pspeed, dc_hide, dc_kill,
// dc_victims, dc_ownerpos.

// ---- platform: CoDOL ----
#include maps\mp\_utility;
#include common_scripts\utility;

cap_enter( team, combat )
{
	self endon( "disconnect" );
	// the bot class table loadout (no profile upload for a test client)
	self.codolCombatBot = true;
	self.codolBotClass = 0;
	self.pers["codolCombatBot"] = true;
	self.pers["codolBotClass"] = 0;
	wait 1;
	if ( combat )
		self thread cap_combat_bot();
	if ( team == "allies" )
		self [[ level.allies ]]();
	else
		self [[ level.axis ]]();
	wait 0.25;
	self.selectedClass = true;
	self [[ level.class ]]( "class0" );
}

// A victim fights like T5's test clients (which T5 MP hands to its bot AI).
cap_combat_bot()
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "spawned_player" );
		self enableCombatBot( true );
	}
}

cap_host()
{
	p = addTestClient( "DogTarget" );
	p thread cap_enter( "allies", false );
	while ( !isalive( p ) )
		wait 0.05;
	wait 1;
	return p;
}

cap_bot( team, name )
{
	ent = addTestClient( name );
	if ( !isdefined( ent ) )
		return undefined;
	ent thread cap_enter( team, name == "victim" );
	for ( i = 0; i < 200 && !isalive( ent ); i++ )
		wait 0.05;
	return ent;
}

cap_call( owner )
{
	owner thread maps\mp\_dogs::useKillstreakDogs( 0 );
}

cap_hide( p )
{
	p setDontTarget( true );
}

cap_quit()
{
	codolprintln( "dogcap: done" );
}

cap_dogs()
{
	return level.dogs;
}

cap_kills( p )
{
	k = 0;
	if ( isdefined( p.pers ) && isdefined( p.pers["kills"] ) )
		k = p.pers["kills"];
	return k;
}

cap_as()
{
	s = self.codolanimscript;
	if ( !isdefined( s ) )
		return "-";
	return s;
}

// The C1 dog clips (animtrees/dog.atr, the 37 german_shepherd_* xanims).
cap_clip_names()
{
	return strtok( "idle walk run_stop run_pain run_attack run_start_L run_start_R run_start_180_L run_start_180_R "
		+ "death_front run_flashbang attack_player_miss attack_player_miss_turnL attack_player_miss_turnR attack_player "
		+ "attack_player_late player_neck_snap player_neck_miss traverse_up_40 traverse_up_80 traverse_down_40 "
		+ "run_jump_40 run_jump_window_40 run_start run run_lean_L run_lean_R attackidle attackidle_bark attackidle_growl "
		+ "look_down look_up look_left look_right attack_look_down attack_look_up "
		+ "attack_look_left attack_look_right", " " );
}

cap_clip( name )
{
	if ( !isdefined( level.cap_clip ) )
		level.cap_clip = [];
	if ( !isdefined( level.cap_clip[name] ) )
		level.cap_clip[name] = codol_getAnim( "dog", "german_shepherd_" + name );
	return level.cap_clip[name];
}

// "D <time> <ent> clip:time ...": every dog clip whose normalized time moved
// since the last frame (a playing clip), from getanimtime.
cap_dog_clips( t )
{
	if ( !isdefined( level.cap_clipnames ) )
		level.cap_clipnames = cap_clip_names();
	if ( !isdefined( self.cap_ct ) )
		self.cap_ct = [];
	names = level.cap_clipnames;
	line = "";
	for ( i = 0; i < names.size; i++ )
	{
		a = cap_clip( names[i] );
		if ( !isdefined( a ) )
			continue;
		ct = self getanimtime( a );
		if ( isdefined( self.cap_ct[i] ) && self.cap_ct[i] != ct )
			line += " " + names[i] + ":" + int( ct * 1000 ) / 1000;
		self.cap_ct[i] = ct;
	}
	if ( line != "" )
		w( "D " + t + " " + self.cap_id + line );
}

cap_dump_clips()
{
}

// The dog animscripts report their states through level.dogcap (dog_shared
// dogcap) at the points the T5 capture detours.
cap_platform_init()
{
	level.dogcap = ::cap_state;
}

cap_state( s )
{
	w( "S " + gettime() + " " + self getentitynumber() + " " + s );
}
// ---- end platform ----

init()
{
	scn = getdvar( "dc_scn" );
	if ( scn == "" )
		return;
	level thread cap_main( scn );
}

w( line )
{
	if ( isdefined( level.cap_fh ) )
		fs_writeline( level.cap_fh, line );
}

vstr( v )
{
	return int( v[0] * 10 ) / 10 + "," + int( v[1] * 10 ) / 10 + "," + int( v[2] * 10 ) / 10;
}

cap_s( v )
{
	if ( !isdefined( v ) )
		return "-";
	return v;
}

cap_num( e )
{
	if ( !isdefined( e ) )
		return -1;
	return e getentitynumber();
}

cap_main( scn )
{
	name = getdvar( "dc_file" );
	if ( name == "" )
		name = "dogcap.txt";
	level.cap_fh = fs_fopen( name, "write" );
	w( "H scn=" + scn + " map=" + getdvar( "mapname" ) + " gt=" + getdvar( "g_gametype" ) + " start=" + gettime() );
	cap_platform_init();
	p = cap_host();
	level.cap_player = p;
	w( "H player " + cap_num( p ) + " spawn " + vstr( p.origin ) + " team=" + cap_s( p.team ) );
	p.maxhealth = 100000;
	p.health = 100000;
	level thread cap_god( p );
	p thread cap_player_damage( "P" );

	if ( scn == "dump" )
	{
		cap_dump();
		fs_fclose( level.cap_fh );
		level.cap_fh = undefined;
		cap_quit();
		return;
	}

	if ( getdvar( "dc_px" ) != "" )
	{
		pos = ( getdvarfloat( "dc_px" ), getdvarfloat( "dc_py" ), getdvarfloat( "dc_pz" ) );
		p setorigin( pos );
		w( "H player_moved " + vstr( pos ) );
	}
	if ( getdvarint( "dc_freeze" ) )
		p freezecontrols( true );
	if ( getdvarint( "dc_hide" ) )
		cap_hide( p );

	owner = cap_bot( "axis", "owner" );
	level.cap_owner = owner;
	w( "H owner " + cap_num( owner ) + " team=" + cap_s( owner.team ) + " at " + vstr( owner.origin ) );
	level thread cap_god( owner );
	if ( getdvar( "dc_ownerpos" ) != "" )
	{
		c = strtok( getdvar( "dc_ownerpos" ), "," );
		owner setorigin( ( int( c[0] ), int( c[1] ), int( c[2] ) ) );
	}
	owner freezecontrols( true );

	level.cap_victims = [];
	nv = getdvarint( "dc_victims" );
	for ( i = 0; i < nv; i++ )
	{
		v = cap_bot( "allies", "victim" );
		if ( !isdefined( v ) )
			continue;
		level.cap_victims[ level.cap_victims.size ] = v;
		w( "H victim " + cap_num( v ) + " team=" + cap_s( v.team ) + " at " + vstr( v.origin ) );
		v thread cap_player_damage( "V" );
		v thread cap_victim_watch();
	}

	if ( getdvar( "dc_path" ) != "" )
		p thread cap_move_player();

	level thread cap_level_events();
	level thread cap_track_dogs();
	level thread cap_frames();
	level thread cap_kills_watch( owner );

	d = getdvarfloat( "dc_delay" );
	if ( d <= 0 )
		d = 5;
	wait d;
	w( "E " + gettime() + " call owner=" + cap_num( owner ) );
	cap_call( owner );

	dur = getdvarfloat( "dc_dur" );
	if ( dur <= 0 )
		dur = 100;
	wait dur;
	w( "H end " + gettime() );
	fs_fclose( level.cap_fh );
	level.cap_fh = undefined;
	cap_quit();
}

cap_god( p )
{
	for ( ;; )
	{
		if ( !isdefined( p ) )
			return;
		if ( isalive( p ) && p.health < 50000 )
		{
			p.maxhealth = 100000;
			p.health = 100000;
		}
		wait 0.05;
	}
}

cap_dump()
{
	sp = level.spawnpoints;
	if ( isdefined( sp ) )
		for ( i = 0; i < sp.size; i++ )
			w( "S spawn " + vstr( sp[i].origin ) + " " + cap_s( sp[i].classname ) );
	nodes = getallnodes();
	w( "H nodes " + nodes.size );
	sn = getnodearray( "spawn", "script_noteworthy" );
	for ( i = 0; i < sn.size; i++ )
		w( "S dogspawn " + vstr( sn[i].origin ) );
	en = getnodearray( "exit", "script_noteworthy" );
	for ( i = 0; i < en.size; i++ )
		w( "S dogexit " + vstr( en[i].origin ) );
	ds = getentarray( "dog_spawner", "targetname" );
	w( "H dog_spawner " + ds.size );
	for ( i = 0; i < ds.size; i++ )
		w( "S dog_spawner " + cap_s( ds[i].classname ) + " " + vstr( ds[i].origin ) );
	w( "H dvars scr_dog_time=" + getdvar( "scr_dog_time" ) + " scr_dog_count=" + getdvar( "scr_dog_count" ) + " scr_dog_max_at_once=" + getdvar( "scr_dog_max_at_once" ) + " scr_dog_health=" + getdvar( "scr_dog_health" ) + " dog_turn90_angle=" + getdvar( "dog_turn90_angle" ) + " dog_turn180_angle=" + getdvar( "dog_turn180_angle" ) );
	cap_dump_clips();
}

cap_player_damage( tag )
{
	self endon( "disconnect" );
	for ( ;; )
	{
		self waittill( "damage", amount, attacker, dir, point, mod );
		a = cap_num( attacker );
		ad = -1;
		ai = 0;
		if ( isdefined( attacker ) )
		{
			ad = int( distance2d( attacker.origin, self.origin ) );
			if ( isai( attacker ) )
				ai = 1;
		}
		w( "E " + gettime() + " " + tag + " pdamage " + amount + " " + cap_s( mod ) + " from=" + a + " ai=" + ai + " d=" + ad + " ent=" + cap_num( self ) + " hp=" + self.health );
	}
}

cap_victim_watch()
{
	self endon( "disconnect" );
	alive = isalive( self );
	for ( ;; )
	{
		a = isalive( self );
		if ( a != alive )
		{
			if ( a )
				w( "E " + gettime() + " V spawned ent=" + cap_num( self ) + " at " + vstr( self.origin ) );
			else
				w( "E " + gettime() + " V died ent=" + cap_num( self ) + " at " + vstr( self.origin ) );
			alive = a;
		}
		wait 0.05;
	}
}

cap_kills_watch( owner )
{
	k = -1;
	s = -1;
	for ( ;; )
	{
		if ( !isdefined( owner ) )
			return;
		nk = cap_kills( owner );
		ns = owner.score;
		if ( nk != k || ns != s )
		{
			w( "E " + gettime() + " O owner kills=" + nk + " score=" + ns );
			k = nk;
			s = ns;
		}
		wait 0.05;
	}
}

cap_level_events()
{
	level thread cap_level_note( "dogs leaving" );
	level thread cap_level_note( "dogs done" );
	level thread cap_level_note( "dog died" );
	level thread cap_level_note( "all dogs spawned" );
	level thread cap_level_note( "called_in_the_dogs" );
}

cap_level_note( msg )
{
	for ( ;; )
	{
		level waittill( msg );
		w( "E " + gettime() + " L " + msg );
	}
}

cap_track_dogs()
{
	level.cap_seen = [];
	for ( ;; )
	{
		dogs = cap_dogs();
		if ( isdefined( dogs ) )
		{
			for ( i = 0; i < dogs.size; i++ )
			{
				d = dogs[i];
				if ( !isdefined( d ) || isdefined( d.cap_id ) )
					continue;
				d.cap_id = d getentitynumber();
				level.cap_seen[ level.cap_seen.size ] = d;
				d thread cap_dog_events();
			}
		}
		// a seen dog that is no longer defined was deleted
		keep = [];
		for ( i = 0; i < level.cap_seen.size; i++ )
		{
			if ( isdefined( level.cap_seen[i] ) )
				keep[ keep.size ] = level.cap_seen[i];
		}
		if ( keep.size != level.cap_seen.size )
			w( "E " + gettime() + " L gone " + ( level.cap_seen.size - keep.size ) );
		level.cap_seen = keep;
		wait 0.05;
	}
}

cap_near_player( o )
{
	best = -1;
	for ( i = 0; i < level.players.size; i++ )
	{
		pl = level.players[i];
		if ( !isalive( pl ) || pl == level.cap_owner )
			continue;
		dd = distance2d( pl.origin, o );
		if ( best < 0 || dd < best )
			best = dd;
	}
	return int( best );
}

cap_dog_events()
{
	id = self.cap_id;
	ow = -1;
	if ( isdefined( self.script_owner ) )
		ow = cap_num( self.script_owner );
	w( "E " + gettime() + " " + id + " spawn o=" + vstr( self.origin ) + " hp=" + self.health + " team=" + cap_s( self.aiteam ) + " owner=" + ow + " near=" + cap_near_player( self.origin ) + " mad=" + cap_s( self.meleeattackdist ) + " gr=" + cap_s( self.goalradius ) + " apr=" + cap_s( self.animplaybackrate ) );
	self thread cap_note( "goal" );
	self thread cap_note( "bad_path" );
	self thread cap_note( "enemy" );
	self thread cap_note( "leaving" );
	self thread cap_note( "on patrol" );
	self thread cap_note( "killed" );
	self thread cap_dog_damage();
	self thread cap_dog_death();
	k = getdvarfloat( "dc_kill" );
	if ( k > 0 )
		self thread cap_dog_kill( k );
}

cap_note( msg )
{
	self endon( "death" );
	id = self.cap_id;
	last = -10000;
	for ( ;; )
	{
		self waittill( msg, a );
		t = gettime();
		if ( msg == "goal" && t - last < 200 )
		{
			last = t;
			continue;
		}
		last = t;
		extra = "";
		if ( isdefined( self.enemy ) )
			extra = " ed=" + int( distance2d( self.origin, self.enemy.origin ) );
		if ( msg == "killed" || msg == "enemy" )
			extra += " who=" + cap_num( a );
		w( "E " + t + " " + id + " " + msg + " o=" + vstr( self.origin ) + extra );
	}
}

cap_dog_damage()
{
	self endon( "death" );
	id = self.cap_id;
	for ( ;; )
	{
		self waittill( "damage", amount, attacker, dir, point, mod );
		w( "E " + gettime() + " " + id + " damage " + amount + " " + cap_s( mod ) + " from=" + cap_num( attacker ) + " hp=" + self.health );
	}
}

cap_dog_death()
{
	id = self.cap_id;
	self waittill( "death", attacker );
	if ( !isdefined( level.cap_fh ) )
		return;
	o = "-";
	if ( isdefined( self ) )
		o = vstr( self.origin );
	w( "E " + gettime() + " " + id + " died o=" + o + " by=" + cap_num( attacker ) );
	for ( i = 0; i < 80 && isdefined( self ) && isdefined( level.cap_fh ); i++ )
	{
		self cap_dog_clips( gettime() );
		wait 0.05;
	}
}

cap_dog_kill( k )
{
	self endon( "death" );
	wait k;
	if ( !isdefined( level.cap_killdir ) )
		level.cap_killdir = 0;
	yaws = [];
	yaws[0] = 0;
	yaws[1] = 90;
	yaws[2] = 180;
	yaws[3] = -90;
	y = yaws[ level.cap_killdir % 4 ];
	level.cap_killdir++;
	pos = self.origin + ( 0, 0, 20 ) + anglestoforward( ( 0, self.angles[1] + y, 0 ) ) * 100;
	w( "E " + gettime() + " " + self.cap_id + " cap_kill rel=" + y );
	self dodamage( self.health + 50, pos, level.cap_player );
}

cap_frames()
{
	for ( ;; )
	{
		t = gettime();
		p = level.cap_player;
		if ( isdefined( p ) )
			w( "P " + t + " " + vstr( p.origin ) + " yaw=" + int( p.angles[1] ) + " hp=" + p.health );
		dogs = cap_dogs();
		if ( isdefined( dogs ) )
		{
			for ( i = 0; i < dogs.size; i++ )
			{
				z = dogs[i];
				if ( !isdefined( z ) || !isalive( z ) )
					continue;
				line = "F " + t + " " + z getentitynumber();
				line += " o=" + vstr( z.origin ) + " y=" + int( z.angles[1] * 10 ) / 10;
				line += " hp=" + z.health;
				en = -1;
				ed = -1;
				cs = -1;
				if ( isdefined( z.enemy ) )
				{
					en = z.enemy getentitynumber();
					ed = int( distance2d( z.origin, z.enemy.origin ) );
					cs = 0;
					if ( z cansee( z.enemy ) )
						cs = 1;
				}
				line += " en=" + en + " ed=" + ed + " cs=" + cs;
				if ( isdefined( z.goalpos ) )
					line += " g=" + vstr( z.goalpos );
				line += " ig=" + cap_s( z.ignoreall ) + " as=" + z cap_as();
				w( line );
				z cap_dog_clips( t );
			}
		}
		wait 0.05;
	}
}

cap_move_player()
{
	parts = strtok( getdvar( "dc_path" ), "|" );
	pts = [];
	for ( i = 0; i < parts.size; i++ )
	{
		c = strtok( parts[i], "," );
		pts[ pts.size ] = ( int( c[0] ), int( c[1] ), int( c[2] ) );
	}
	spd = getdvarfloat( "dc_pspeed" );
	if ( spd <= 0 )
		spd = 190;
	idx = 0;
	pos = pts[0];
	for ( ;; )
	{
		nxt = pts[ ( idx + 1 ) % pts.size ];
		d = distance( pos, nxt );
		step = spd * 0.05;
		if ( d <= step )
		{
			pos = nxt;
			idx = ( idx + 1 ) % pts.size;
		}
		else
			pos = pos + vectornormalize( nxt - pos ) * step;
		self setorigin( pos );
		wait 0.05;
	}
}
