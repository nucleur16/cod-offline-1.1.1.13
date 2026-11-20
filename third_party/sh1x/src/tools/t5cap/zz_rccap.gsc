// rccap: RC-XD telemetry for CoDOL vs T5 parity (docs/RCXD_T5_PARITY.md).
// The same body runs in real T5 MP (Plutonium t5mp, as scripts/mp/zz_rccap.gsc
// with the T5 platform block) and on a CoDOL probe (regenerated into
// maps/mp/killstreaks/_rcbomb_t5cap.gsc by tools/t5cap/mk_rc.py), so the
// two logs line up field for field. Inactive unless the dvar rccap_scn is set.
// Installed temporarily in the T5 install for a capture; remove afterwards.
//
// Dvars: rccap_scn (scan | drive | blast | timeout), rccap_rides (comma list
// for drive), rccap_px/py/pz/yaw (start spot), rccap_file (output name).
// ---- platform: T5 (Plutonium) ----
#include maps\mp\_utility;
#include common_scripts\utility;

cap_platform()
{
	return "t5";
}

cap_wait_player()
{
	while ( !isdefined( level.players ) || level.players.size == 0 )
		wait 0.05;
	p = level.players[0];
	p thread cap_bot_join();
	while ( !isalive( p ) )
		wait 0.05;
	wait 1;
	return p;
}

cap_platform_init()
{
	setdvar( "scr_rcbomb_notimeout", "0" );
}

cap_quit()
{
	wait 0.5;
	cmdexec( "quit\n" );
}

cap_deploy()
{
	self maps\mp\_rcbomb::useKillstreakRCBomb( "rcbomb_mp" );
}

cap_throttle( v )
{
	cmdexec( "-forward\n" );
	cmdexec( "-back\n" );
	if ( v > 0 )
		cmdexec( "+forward\n" );
	else if ( v < 0 )
		cmdexec( "+back\n" );
}

cap_button( name, on )
{
	c = "+";
	if ( !on )
		c = "-";
	if ( name == "boost" )
		cmdexec( c + "vehicleboost\n" );
	else if ( name == "jump" )
		cmdexec( c + "gostand\n" );
	else if ( name == "ads" )
		cmdexec( c + "speed_throw\n" );
	else if ( name == "sprint" )
		cmdexec( c + "sprint\n" );
	else if ( name == "melee" )
		cmdexec( c + "melee\n" );
	else if ( name == "attack" )
		cmdexec( c + "attack\n" );
	else if ( getsubstr( name, 0, 4 ) == "cmd_" )
		cmdexec( c + getsubstr( name, 4 ) + "\n" );
}

cap_add_bot()
{
	bot = addtestclient();
	if ( !isdefined( bot ) )
		return undefined;
	bot.pers["isBot"] = true;
	bot thread cap_bot_join();
	return bot;
}

cap_bot_join()
{
	self endon( "disconnect" );
	while ( !isdefined( self.pers ) || !isdefined( self.pers["team"] ) )
		wait 0.05;
	if ( level.teambased )
	{
		self notify( "menuresponse", game["menu_team"], "autoassign" );
		wait 0.5;
	}
	classes = getarraykeys( level.classMap );
	ok = [];
	for ( i = 0; i < classes.size; i++ )
	{
		if ( !issubstr( classes[i], "custom" ) && isdefined( level.default_perk[ level.classMap[ classes[i] ] ] ) )
			ok[ ok.size ] = classes[i];
	}
	self notify( "menuresponse", "changeclass", ok[0] );
}

cap_vehicle_speed( car )
{
	return car getspeed();
}

cap_hit( car, att, amount )
{
	car dodamage( amount, car.origin + ( 0, 0, 8 ), att, att, 0, "MOD_RIFLE_BULLET" );
}
// ---- end platform ----

init()
{
	scn = getdvar( "rccap_scn" );
	if ( scn == "" )
		return;
	level thread cap_main( scn );
}

cap_open()
{
	name = getdvar( "rccap_file" );
	if ( name == "" )
		name = "rccap.txt";
	level.cap_fh = fs_fopen( name, "write" );
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

cap_main( scn )
{
	cap_open();
	w( "H scn=" + scn + " plat=" + cap_platform() + " map=" + getdvar( "mapname" ) + " gt=" + getdvar( "g_gametype" ) + " start=" + gettime() );
	p = cap_wait_player();
	level.cap_player = p;
	cap_platform_init();
	level.cap_orig_pd = level.callbackPlayerDamage;
	level.callbackPlayerDamage = ::cap_pd;
	w( "H player " + vstr( p.origin ) + " yaw=" + p.angles[1] + " team=" + p.team );
	wait 1;
	if ( scn == "scan" )
		cap_scan( p );
	else if ( scn == "drive" )
	{
		rides = strtok( getdvar( "rccap_rides" ), "," );
		for ( i = 0; i < rides.size; i++ )
			cap_ride( p, rides[i] );
	}
	else if ( scn == "blast" )
		cap_blast( p );
	else if ( scn == "timeout" )
		cap_ride( p, "timeout" );
	w( "H done t=" + gettime() );
	if ( isdefined( level.cap_fh ) )
		fs_fclose( level.cap_fh );
	level.cap_fh = undefined;
	cap_quit();
}

cap_pd( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime )
{
	an = -1;
	if ( isdefined( eAttacker ) && isplayer( eAttacker ) )
		an = eAttacker getentitynumber();
	line = "D " + gettime() + " victim=" + self getentitynumber() + " att=" + an + " dmg=" + iDamage + " mod=" + sMeansOfDeath + " weap=" + sWeapon + " flags=" + iDFlags + " hp=" + self.health;
	line += " vo=" + vstr( self.origin );
	if ( isdefined( level.cap_blast_origin ) )
		line += " dist=" + int( distance( self.origin, level.cap_blast_origin ) * 10 ) / 10 + " dist3=" + int( distance( self.origin + ( 0, 0, 36 ), level.cap_blast_origin ) * 10 ) / 10;
	w( line );
	self [[ level.cap_orig_pd ]]( eInflictor, eAttacker, iDamage, iDFlags, sMeansOfDeath, sWeapon, vPoint, vDir, sHitLoc, psOffsetTime );
}

cap_start_spot()
{
	s = spawnstruct();
	s.origin = ( getdvarfloat( "rccap_px" ), getdvarfloat( "rccap_py" ), getdvarfloat( "rccap_pz" ) );
	s.yaw = getdvarfloat( "rccap_yaw" );
	return s;
}

cap_place( p )
{
	s = cap_start_spot();
	if ( s.origin[0] != 0 || s.origin[1] != 0 )
	{
		p setorigin( s.origin );
		p setplayerangles( ( 0, s.yaw, 0 ) );
	}
	wait 1;
}

// Deploy the car the way the killstreak does and wait for control.
cap_spawn_car( p, tag )
{
	t0 = gettime();
	w( "R " + t0 + " ride=" + tag + " owner=" + vstr( p.origin ) + " yaw=" + p.angles[1] );
	p thread cap_deploy();
	for ( i = 0; i < 100 && !isdefined( p.rcbomb ); i++ )
		wait 0.05;
	if ( !isdefined( p.rcbomb ) )
	{
		w( "E " + gettime() + " nocar" );
		return undefined;
	}
	car = p.rcbomb;
	w( "E " + gettime() + " spawn o=" + vstr( car.origin ) + " a=" + vstr( car.angles ) + " hp=" + car.health + " dt=" + ( gettime() - t0 ) );
	for ( i = 0; i < 100 && isdefined( car ) && !p isremotecontrolling(); i++ )
		wait 0.05;
	w( "E " + gettime() + " control dt=" + ( gettime() - t0 ) + " rc=" + p isremotecontrolling() );
	level.cap_car = car;
	level thread cap_frames( p, car );
	car thread cap_car_death( p, t0 );
	return car;
}

cap_car_death( p, t0 )
{
	self waittill( "death" );
	w( "E " + gettime() + " cardeath dt=" + ( gettime() - t0 ) );
}

cap_frames( p, car )
{
	car endon( "death" );
	for ( ;; )
	{
		line = "V " + gettime() + " o=" + vstr( car.origin ) + " a=" + vstr( car.angles ) + " sp=" + int( cap_vehicle_speed( car ) * 10 ) / 10;
		line += " cf=" + car getclientflag( level.const_flag_countdown ) + car getclientflag( level.const_flag_timeout );
		line += " po=" + vstr( p.origin ) + " pyaw=" + int( p getplayerangles()[1] * 10 ) / 10 + " php=" + p.health;
		w( line );
		wait 0.05;
	}
}

// Hold the driver's view yaw: mode "abs" (fixed yaw) or "rel" (car yaw + off).
cap_steer( p, car, mode, val )
{
	level notify( "cap_steer" );
	level endon( "cap_steer" );
	car endon( "death" );
	for ( ;; )
	{
		y = val;
		if ( mode == "rel" )
			y = car.angles[1] + val;
		p setplayerangles( ( 0, y, 0 ) );
		wait 0.05;
	}
}

cap_mark( tag )
{
	w( "M " + gettime() + " " + tag );
}

cap_end_ride( p, car )
{
	level notify( "cap_steer" );
	cap_throttle( 0 );
	if ( isdefined( car ) )
	{
		cap_mark( "detonate" );
		car.forceDetonation = true;
		for ( i = 0; i < 100 && isdefined( car ); i++ )
			wait 0.05;
	}
	wait 3;
}

cap_ride( p, ride )
{
	cap_place( p );
	yaw = p.angles[1];
	car = cap_spawn_car( p, ride );
	if ( !isdefined( car ) )
		return;
	// rccap_ownerstep: move the owner's body this far to the car's right once
	// the ride starts, out of a tight circle's way (the body blocks the car).
	step = getdvarfloat( "rccap_ownerstep" );
	if ( step != 0 )
		p setorigin( p.origin + anglestoright( ( 0, car.angles[1], 0 ) ) * step );
	wait 0.5;
	if ( ride == "accel" )
	{
		level thread cap_steer( p, car, "abs", car.angles[1] );
		cap_mark( "throttle 1" );
		cap_throttle( 1 );
		wait 1.6;
		cap_mark( "throttle 0" );
		cap_throttle( 0 );
		wait 3;
		cap_mark( "throttle -1" );
		cap_throttle( -1 );
		wait 2.5;
		cap_mark( "throttle 0" );
		cap_throttle( 0 );
		wait 1;
	}
	else if ( getsubstr( ride, 0, 4 ) == "turn" )
	{
		// turnl/turnr: camera 90 deg left/right of the car; turnh: 15 left;
		// turn<N>: N deg left; turnm<N>: N deg right.
		off = 90;
		if ( ride == "turnr" )
			off = -90;
		else if ( ride == "turnh" )
			off = 15;
		else if ( getsubstr( ride, 0, 5 ) == "turnm" )
			off = 0 - int( getsubstr( ride, 5 ) );
		else if ( ride != "turnl" )
			off = int( getsubstr( ride, 4 ) );
		level thread cap_steer( p, car, "rel", off );
		cap_mark( "throttle 1 steer " + off );
		cap_throttle( 1 );
		wait 6;
		cap_mark( "throttle 0" );
		cap_throttle( 0 );
		wait 1.5;
	}
	else if ( ride == "rev" )
	{
		level thread cap_steer( p, car, "abs", car.angles[1] );
		cap_mark( "throttle -1" );
		cap_throttle( -1 );
		wait 3;
		cap_mark( "throttle 0" );
		cap_throttle( 0 );
		wait 1.5;
	}
	else if ( ride == "brake" )
	{
		level thread cap_steer( p, car, "abs", car.angles[1] );
		cap_mark( "throttle 1" );
		cap_throttle( 1 );
		wait 1.6;
		cap_mark( "throttle -1" );
		cap_throttle( -1 );
		wait 3;
		cap_mark( "throttle 0" );
		cap_throttle( 0 );
		wait 1;
	}
	else if ( ride == "boost" || ride == "jump" || ride == "ads" || ride == "sprint" || ride == "melee" || getsubstr( ride, 0, 4 ) == "cmd_" )
	{
		btn = ride;
		level thread cap_steer( p, car, "abs", car.angles[1] );
		cap_mark( "throttle 1" );
		cap_throttle( 1 );
		wait 1;
		cap_mark( "button " + btn + " 1" );
		cap_button( btn, 1 );
		wait 0.3;
		cap_mark( "button " + btn + " 0" );
		cap_button( btn, 0 );
		wait 2.5;
		cap_mark( "throttle 0" );
		cap_throttle( 0 );
		wait 1;
	}
	else if ( ride == "bump" || ride == "bumpslow" )
	{
		// A frozen test client stands in the car's path; drive into it.
		b = cap_bump_bot_get();
		if ( isdefined( b ) && isalive( b ) )
		{
			pos = car.origin + anglestoforward( ( 0, car.angles[1], 0 ) ) * 260;
			tr = bullettrace( pos + ( 0, 0, 40 ), pos - ( 0, 0, 80 ), false, undefined );
			b setorigin( tr["position"] );
			b freezecontrols( true );
			b.health = 1000;
			b.maxhealth = 1000;
			w( "B " + gettime() + " bot=" + b getentitynumber() + " pos=" + vstr( b.origin ) );
			level thread cap_watch_bot( b, car );
		}
		level thread cap_steer( p, car, "abs", car.angles[1] );
		cap_mark( "throttle 1" );
		if ( ride == "bumpslow" )
		{
			cap_throttle( 1 );
			wait 0.25;
			cap_throttle( 0 );
			wait 0.6;
			cap_throttle( 1 );
			wait 0.15;
			cap_throttle( 0 );
			wait 0.6;
			cap_throttle( 1 );
			wait 0.15;
			cap_throttle( 0 );
			wait 1.5;
		}
		else
		{
			cap_throttle( 1 );
			wait 2.5;
		}
		cap_mark( "throttle 0" );
		cap_throttle( 0 );
		wait 1;
	}
	else if ( ride == "hit" || ride == "hit1" )
	{
		// An enemy player's single bullet-sized hit on the car.
		b = cap_bump_bot_get();
		amount = 20;
		if ( ride == "hit1" )
			amount = 1;
		wait 0.5;
		cap_mark( "hit " + amount );
		if ( isdefined( b ) )
			cap_hit( car, b, amount );
		for ( i = 0; i < 60 && isdefined( car ); i++ )
			wait 0.05;
		wait 1;
		return;
	}
	else if ( ride == "timeout" )
	{
		cap_mark( "idle" );
		for ( i = 0; i < 700 && isdefined( car ); i++ )
			wait 0.05;
		wait 3;
		return;
	}
	cap_end_ride( p, car );
}

cap_bump_bot_get()
{
	if ( !isdefined( level.cap_bump_bot ) )
	{
		level.cap_bump_bot = cap_add_bot();
		for ( i = 0; i < 200 && isdefined( level.cap_bump_bot ) && !isalive( level.cap_bump_bot ); i++ )
			wait 0.05;
	}
	return level.cap_bump_bot;
}

cap_watch_bot( b, car )
{
	car endon( "death" );
	b endon( "disconnect" );
	for ( ;; )
	{
		w( "Q " + gettime() + " bo=" + vstr( b.origin ) + " bhp=" + b.health + " d=" + int( distance( b.origin, car.origin ) ) );
		wait 0.1;
	}
}

// Bots on a ring around where the car will be placed (70 ahead of the
// owner), never on the owner's side, placed before the car spawns so none
// overlaps it; then deploy and detonate at once.
cap_blast( p )
{
	cap_place( p );
	bots = [];
	n = getdvarint( "rccap_bots" );
	if ( n <= 0 )
		n = 10;
	for ( i = 0; i < n; i++ )
	{
		b = cap_add_bot();
		if ( isdefined( b ) )
			bots[bots.size] = b;
		wait 0.3;
	}
	for ( i = 0; i < 160; i++ )
	{
		alive = 0;
		for ( j = 0; j < bots.size; j++ )
			if ( isdefined( bots[j] ) && isalive( bots[j] ) )
				alive++;
		if ( alive == bots.size )
			break;
		wait 0.05;
	}
	yaw = p.angles[1];
	c = p.origin + anglestoforward( ( 0, yaw, 0 ) ) * 70;
	step = getdvarfloat( "rccap_ringstep" );
	if ( step <= 0 )
		step = 24;
	for ( i = 0; i < bots.size; i++ )
	{
		d = 32 + i * step;
		ang = yaw - 135 + i * 30;
		pos = c + ( cos( ang ) * d, sin( ang ) * d, 0 );
		tr = bullettrace( pos + ( 0, 0, 40 ), pos - ( 0, 0, 80 ), false, undefined );
		pos = tr["position"];
		bots[i] setorigin( pos );
		bots[i] setplayerangles( ( 0, ang + 180, 0 ) );
		bots[i] freezecontrols( true );
		bots[i].health = 1000;
		bots[i].maxhealth = 1000;
	}
	wait 0.5;
	car = cap_spawn_car( p, "blast" );
	if ( !isdefined( car ) )
		return;
	wait 0.2;
	level.cap_blast_origin = car.origin + ( 0, 0, 10 );
	o = level.cap_blast_origin;
	for ( i = 0; i < bots.size; i++ )
	{
		bo = bots[i].origin;
		// Closest point of the standing player box (-15,-15,0)..(15,15,70).
		cx = o[0];
		if ( cx < bo[0] - 15 )
			cx = bo[0] - 15;
		if ( cx > bo[0] + 15 )
			cx = bo[0] + 15;
		cy = o[1];
		if ( cy < bo[1] - 15 )
			cy = bo[1] - 15;
		if ( cy > bo[1] + 15 )
			cy = bo[1] + 15;
		cz = o[2];
		if ( cz < bo[2] )
			cz = bo[2];
		if ( cz > bo[2] + 70 )
			cz = bo[2] + 70;
		w( "B " + gettime() + " bot=" + bots[i] getentitynumber() + " pos=" + vstr( bo ) + " dorg=" + int( distance( bo, o ) * 10 ) / 10 + " dbox=" + int( distance( ( cx, cy, cz ), o ) * 10 ) / 10 + " alive=" + isalive( bots[i] ) );
	}
	w( "E " + gettime() + " blast at=" + vstr( o ) + " owner=" + vstr( p.origin ) + " ownerdist=" + int( distance( p.origin, o ) ) );
	cap_end_ride( p, car );
}

// Find long flat straights and open circles near the spawn points.
cap_scan( p )
{
	pts = getentarray( "mp_dm_spawn", "classname" );
	t = getentarray( "mp_tdm_spawn", "classname" );
	for ( i = 0; i < t.size; i++ )
		pts[pts.size] = t[i];
	w( "H scan points=" + pts.size );
	for ( i = 0; i < pts.size; i++ )
	{
		o = pts[i].origin;
		best = 0;
		besty = 0;
		minr = 99999;
		for ( a = 0; a < 360; a += 15 )
		{
			d = cap_clear( o, a );
			if ( d > best )
			{
				best = d;
				besty = a;
			}
			if ( d < minr )
				minr = d;
		}
		w( "S " + i + " o=" + vstr( o ) + " best=" + best + " yaw=" + besty + " minr=" + minr );
		wait 0.05;
	}
}

cap_clear( o, yaw )
{
	f = anglestoforward( ( 0, yaw, 0 ) );
	for ( d = 32; d <= 2400; d += 32 )
	{
		a = o + f * ( d - 32 ) + ( 0, 0, 20 );
		b = o + f * d + ( 0, 0, 20 );
		tr = bullettrace( a, b, false, undefined );
		if ( tr["fraction"] < 1 )
			return d - 32;
		dn = bullettrace( b, b - ( 0, 0, 80 ), false, undefined );
		if ( dn["fraction"] >= 1 || abs( dn["position"][2] - o[2] ) > 12 )
			return d - 32;
	}
	return 2400;
}
