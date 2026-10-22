// Developer-only actor telemetry (docs/CODOL_ZOMBIE_T5_MEASURED.md).
//
// The same capture runs in real T5 (Plutonium, as scripts/sp/zz_t5cap.gsc
// with a T5 platform block) and here, so the two logs line up field for
// field: every server frame, each zombie's and dog's origin, yaw, health,
// move bucket, run cycle, animscript and goal; events for spawn, the
// melee/tear/traverse/rise/death notetracks, goal, bad_path and damage.
// Inactive unless the dvar t5cap_scn is set.  Output: fs_fopen( t5cap_file ).
// Scenario dvars: t5cap_dur, t5cap_px/py/pz, t5cap_freeze, t5cap_movespeed,
// t5cap_round, t5cap_dogs, t5cap_crawlers, t5cap_path, t5cap_pspeed,
// t5cap_pdelay, t5cap_bot.

// ---- platform: CoDOL ----
#include maps\_zombiemode_utility;
#using_animtree( "generic_human" );

cap_wait_player()
{
	if ( getDvarInt( "t5cap_bot" ) )
	{
		addtestclient();
		level thread cap_god();
	}
	players = getPlayers();
	while ( players.size == 0 || !isAlive( players[0] ) )
	{
		wait 0.5;
		players = getPlayers();
	}
	wait 1;
	return players[0];
}

cap_quit()
{
	codolprintln( "t5cap: done" );
}

cap_health( r )
{
	maps\_zombiemode_spawner::ai_calculate_health( r );
}

cap_emerged()
{
	return isDefined( self.zm_path_phase ) && self.zm_path_phase == "hunt";
}

cap_phase_name()
{
	if ( isDefined( self.zm_path_phase ) )
		return self.zm_path_phase;
	return "-";
}

cap_spawn_dog()
{
	p = level.cap_player;
	maps\_zombiemode_ai_dogs::spawn_dog( maps\_zombiemode_ai_dogs::dog_spawn_location( p ), p );
}

cap_make_crawler()
{
	self.a.gib_ref = "left_leg";
	self maps\_zombiemode_spawner::make_crawler();
}

cap_knockdown_one( z )
{
	z thread maps\_zombiemode_weap_thundergun::thundergun_knockdown_zombie( level.cap_player, false );
}

cap_on_dog_placed( z )
{
	if ( getDvarInt( "codol_tracePace" ) < 0 )
		setDvar( "codol_tracePace", z getEntityNumber() );
}

cap_platform_init()
{
	level.cap_swing_hits = true;
}

cap_as()
{
	s = self.codolanimscript;
	if ( !isDefined( s ) )
		return "-";
	return s;
}

cap_god()
{
	for ( ;; )
	{
		players = getPlayers();
		for ( i = 0; i < players.size; i++ )
		{
			if ( isAlive( players[i] ) && players[i].health < 50000 )
			{
				players[i].maxhealth = 100000;
				players[i].health = 100000;
			}
		}
		wait 0.05;
	}
}
// ---- end platform ----

init()
{
	scn = getdvar( "t5cap_scn" );
	if ( scn == "" )
		return;
	level thread cap_main( scn );
}

cap_open()
{
	name = getdvar( "t5cap_file" );
	if ( name == "" )
		name = "t5cap.txt";
	level.cap_fh = fs_fopen( name, "write" );
}

w( line )
{
	if ( isdefined( level.cap_fh ) )
		fs_writeline( level.cap_fh, line );
}

cap_main( scn )
{
	cap_open();
	w( "H scn=" + scn + " map=" + getdvar( "mapname" ) + " start=" + gettime() );
	p = cap_wait_player();
	level.cap_player = p;
	w( "H player_spawn " + vstr( p.origin ) + " yaw=" + p.angles[1] );

	// player setup: survives, stays where staged
	p.maxhealth = 100000;
	p.health = 100000;
	p thread cap_player_watch();

	px = getdvar( "t5cap_px" );
	if ( px != "" )
	{
		pos = ( getdvarfloat( "t5cap_px" ), getdvarfloat( "t5cap_py" ), getdvarfloat( "t5cap_pz" ) );
		p setorigin( pos );
		w( "H player_moved " + vstr( pos ) );
	}
	if ( getdvarint( "t5cap_freeze" ) )
		p freezecontrols( true );

	// staged round speed / health
	if ( getdvar( "t5cap_movespeed" ) != "" )
		level thread cap_hold_speed( getdvarint( "t5cap_movespeed" ) );
	if ( getdvar( "t5cap_round" ) != "" )
	{
		r = getdvarint( "t5cap_round" );
		level.round_number = r;
		level thread cap_hold_health( r );
	}
	if ( getdvarint( "t5cap_dogs" ) )
		level thread cap_dog_round();
	if ( getdvarint( "t5cap_crawlers" ) )
		level thread cap_crawlers();
	if ( getdvarint( "t5cap_knockdown" ) )
		level thread cap_knockdowns();
	if ( getdvar( "t5cap_path" ) != "" )
		p thread cap_move_player();

	if ( getsubstr( scn, 0, 4 ) == "dump" )
	{
		if ( getdvarint( "t5cap_dogs" ) )
		{
			level thread cap_dog_round();
			wait 15;
		}
		cap_dump_all();
		wait 1;
		fs_fclose( level.cap_fh );
		level.cap_fh = undefined;
		cap_quit();
		return;
	}
	cap_platform_init();
	level.cap_anim_keys = getarraykeys( level.scr_anim["zombie"] );
	level thread cap_track_spawns();
	level thread cap_frames();

	dur = getdvarfloat( "t5cap_dur" );
	if ( dur <= 0 )
		dur = 120;
	wait dur;
	w( "H end " + gettime() );
	fs_fclose( level.cap_fh );
	level.cap_fh = undefined;
	cap_quit();
}

cap_hold_speed( v )
{
	for ( ;; )
	{
		level.zombie_move_speed = v;
		wait 0.05;
	}
}

cap_hold_health( r )
{
	cap_health( r );
	w( "H health_round " + r + " hp=" + level.zombie_health );
	for ( ;; )
	{
		level.round_number = r;
		cap_health( r );
		wait 0.05;
	}
}

vstr( v )
{
	return int( v[0] * 10 ) / 10 + "," + int( v[1] * 10 ) / 10 + "," + int( v[2] * 10 ) / 10;
}

cap_runanim()
{
	if ( !isdefined( self.animname ) || self.animname != "zombie" )
		return "-";
	if ( !isdefined( self.run_combatanim ) )
		return "none";
	keys = level.cap_anim_keys;
	for ( i = 0; i < keys.size; i++ )
	{
		k = keys[i];
		a = level.scr_anim["zombie"][k];
		if ( isdefined( a ) && !isarray( a ) && a == self.run_combatanim )
			return k;
	}
	return "?";
}

cap_track_spawns()
{
	for ( ;; )
	{
		ai = getaispeciesarray( "axis", "all" );
		for ( i = 0; i < ai.size; i++ )
		{
			z = ai[i];
			if ( !isdefined( z.cap_seen ) )
			{
				z.cap_seen = true;
				z.cap_id = z getentitynumber();
				z thread cap_ai_events();
			}
		}
		wait 0.05;
	}
}

cap_frames()
{
	for ( ;; )
	{
		t = gettime();
		p = level.cap_player;
		if ( isdefined( p ) )
			w( "P " + t + " " + vstr( p.origin ) + " yaw=" + int( p.angles[1] ) + " hp=" + p.health );
		ai = getaispeciesarray( "axis", "all" );
		for ( i = 0; i < ai.size; i++ )
		{
			z = ai[i];
			if ( !isalive( z ) )
				continue;
			line = "F " + t + " " + z getentitynumber();
			line += " o=" + vstr( z.origin ) + " y=" + int( z.angles[1] * 10 ) / 10;
			line += " hp=" + z.health;
			line += " ms=" + z.zombie_move_speed;
			line += " ra=" + z cap_runanim();
			sc = z cap_as();
			mv = "-";
			if ( isdefined( z.a ) && isdefined( z.a.movement ) )
				mv = z.a.movement;
			line += " as=" + sc + " mv=" + mv + " ph=" + z cap_phase_name();
			line += " mpr=" + z.moveplaybackrate + " apr=" + z.animplaybackrate;
			legs = 1;
			if ( isdefined( z.has_legs ) && !z.has_legs )
				legs = 0;
			line += " legs=" + legs;
			em = 0;
			if ( z cap_emerged() )
				em = 1;
			line += " em=" + em;
			en = -1;
			ed = -1;
			if ( isdefined( z.enemy ) )
			{
				en = z.enemy getentitynumber();
				ed = int( distance2d( z.origin, z.enemy.origin ) );
			}
			line += " en=" + en + " ed=" + ed;
			if ( isdefined( z.goalpos ) )
				line += " g=" + vstr( z.goalpos );
			if ( isdefined( z.animname ) )
				line += " an=" + z.animname;
			if ( isdefined( z.animname ) && z.animname == "zombie_dog" && isdefined( z.enemy ) )
			{
				cs = 0;
				if ( z cansee( z.enemy ) )
					cs = 1;
				line += " cs=" + cs;
			}
			w( line );
		}
		wait 0.05;
	}
}

cap_ai_events()
{
	self endon( "death" );
	id = self.cap_id;
	ra = "-";
	w( "E " + gettime() + " " + id + " spawn o=" + vstr( self.origin ) + " hp=" + self.health + " an=" + self.animname );
	self thread cap_note( "meleeanim", "melee" );
	self thread cap_note( "tear_anim", "tear" );
	self thread cap_note( "traverseAnim", "trav" );
	self thread cap_note( "rise", "rise" );
	self thread cap_note( "painanim", "pain" );
	self thread cap_note( "deathanim", "death" );
	self thread cap_note( "stepAnim", "step" );
	self thread cap_simple( "zombie_start_traverse" );
	self thread cap_simple( "zombie_end_traverse" );
	self thread cap_simple( "goal" );
	self thread cap_simple( "bad_path" );
	self thread cap_phase();
	self thread cap_simple( "risen" );
	self thread cap_simple( "zombie_acquire_enemy" );
	self thread cap_mpick();
	if ( getdvar( "t5cap_dogpos" ) != "" && isdefined( self.animname ) && self.animname == "zombie_dog" )
		self thread cap_dog_place();
	if ( isdefined( level.cap_swing_hits ) )
		self thread cap_simple( "swing_hit" );
	self thread cap_damage();
	self thread cap_death();
	for ( ;; )
	{
		r = self cap_runanim();
		if ( r != ra )
		{
			w( "E " + gettime() + " " + id + " runanim " + r + " ms=" + self.zombie_move_speed + " mpr=" + self.moveplaybackrate );
			ra = r;
		}
		wait 0.05;
	}
}

cap_note( flag, tag )
{
	self endon( "death" );
	id = self.cap_id;
	for ( ;; )
	{
		self waittill( flag, note );
		extra = "";
		if ( isdefined( self.enemy ) )
			extra = " ed=" + int( distance( self.origin, self.enemy.origin ) ) + " ed2=" + int( distance2d( self.origin, self.enemy.origin ) );
		w( "E " + gettime() + " " + id + " " + tag + " " + note + " o=" + vstr( self.origin ) + " y=" + int( self.angles[1] ) + extra );
	}
}

cap_simple( msg )
{
	self endon( "death" );
	id = self.cap_id;
	last = -10000;
	for ( ;; )
	{
		self waittill( msg );
		t = gettime();
		if ( msg == "goal" && t - last <= 100 )
		{
			last = t;
			continue;
		}
		last = t;
		extra = "";
		if ( isdefined( self.enemy ) )
			extra = " ed=" + int( distance( self.origin, self.enemy.origin ) );
		w( "E " + t + " " + id + " " + msg + " o=" + vstr( self.origin ) + extra );
	}
}

cap_damage()
{
	self endon( "death" );
	id = self.cap_id;
	for ( ;; )
	{
		self waittill( "damage", amount, attacker, dir, point, mod );
		w( "E " + gettime() + " " + id + " damage " + amount + " " + cap_s( mod ) + " hp=" + self.health );
	}
}

cap_death()
{
	id = self.cap_id;
	self waittill( "death" );
	if ( isdefined( level.cap_fh ) )
		w( "E " + gettime() + " " + id + " died o=" + vstr( self.origin ) );
}

cap_player_watch()
{
	for ( ;; )
	{
		self waittill( "damage", amount, attacker, dir, point, mod );
		a = -1;
		ad = -1;
		if ( isdefined( attacker ) )
		{
			a = attacker getentitynumber();
			ad = int( distance( attacker.origin, self.origin ) );
		}
		if ( isdefined( level.cap_fh ) )
			w( "E " + gettime() + " P pdamage " + amount + " " + cap_s( mod ) + " from=" + a + " d=" + ad );
		self.health = 100000;
	}
}

cap_move_player()
{
	// t5cap_path "x,y,z;x,y,z;..." : walk the player around at t5cap_pspeed u/s
	parts = strtok( getdvar( "t5cap_path" ), "|" );
	pts = [];
	for ( i = 0; i < parts.size; i++ )
	{
		c = strtok( parts[i], "," );
		pts[pts.size] = ( int( c[0] ), int( c[1] ), int( c[2] ) );
	}
	spd = getdvarfloat( "t5cap_pspeed" );
	if ( spd <= 0 )
		spd = 150;
	wait getdvarfloat( "t5cap_pdelay" );
	idx = 0;
	pos = pts[0];
	for ( ;; )
	{
		nxt = pts[( idx + 1 ) % pts.size];
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

cap_dog_round()
{
	wait 5;
	w( "H dogs requested" );
	n = getdvarint( "t5cap_dogs" );
	for ( i = 0; i < n; i++ )
	{
		cap_spawn_dog();
		wait 2;
	}
}

cap_crawlers()
{
	// turn every 2nd zombie that has emerged into a crawler via a leg gib
	for ( ;; )
	{
		ai = getaispeciesarray( "axis", "all" );
		for ( i = 0; i < ai.size; i++ )
		{
			z = ai[i];
			if ( isdefined( z.cap_crawl_done ) || !z cap_emerged() )
				continue;
			z.cap_crawl_done = true;
			if ( z.cap_id % 2 && getdvarint( "t5cap_crawlers" ) < 2 )
				continue;
			z cap_make_crawler();
			w( "E " + gettime() + " " + z.cap_id + " cap_crawl_request" );
		}
		wait 0.5;
	}
}

cap_phase()
{
	self endon( "death" );
	ph = "";
	for ( ;; )
	{
		n = self cap_phase_name();
		if ( n != ph )
		{
			ph = n;
			w( "E " + gettime() + " " + self.cap_id + " phase " + ph + " o=" + vstr( self.origin ) );
		}
		wait 0.05;
	}
}

cap_s( v )
{
	if ( !isdefined( v ) )
		return "-";
	return v;
}

cap_dump_all()
{
	sets = [];
	sets["scr_anim"] = level.scr_anim["zombie"];
	sets["melee"] = level._zombie_melee["zombie"];
	sets["walk_melee"] = level._zombie_walk_melee["zombie"];
	sets["run_melee"] = level._zombie_run_melee["zombie"];
	sets["melee_crawl"] = level._zombie_melee_crawl["zombie"];
	sets["stumpy_melee"] = level._zombie_stumpy_melee["zombie"];
	sets["deaths"] = level._zombie_deaths["zombie"];
	sets["board_taunt"] = level._zombie_board_taunt["zombie"];
	sets["taunt"] = level._zombie_taunt["zombie"];
	sets["run_taunt"] = level._zombie_run_taunt["zombie"];
	sets["rise"] = level._zombie_rise_anims["zombie"];
	sets["knockdowns"] = level._zombie_knockdowns["zombie"];
	sets["getups"] = level._zombie_getups["zombie"];
	sets["tesla_death"] = level._zombie_tesla_death["zombie"];
	keys = getarraykeys( sets );
	for ( i = 0; i < keys.size; i++ )
	{
		if ( isdefined( sets[keys[i]] ) )
			cap_dump_set( keys[i], sets[keys[i]] );
	}
	if ( isdefined( level.scr_anim["zombie_dog"] ) )
		cap_dump_set( "dog", level.scr_anim["zombie_dog"] );
	cap_dump_tear();
	ks = getarraykeys( level.scr_anim["zombie"] );
	for ( i = 0; i < ks.size; i++ )
	{
		v = level.scr_anim["zombie"][ks[i]];
		if ( isdefined( v ) && !isarray( v ) )
			cap_dump_profile( "scr_anim." + ks[i], v );
	}
	if ( isdefined( anim.dogAnims ) )
	{
		sets = getarraykeys( anim.dogAnims );
		for ( i = 0; i < sets.size; i++ )
		{
			mv = anim.dogAnims[sets[i]].move;
			if ( !isdefined( mv ) )
				continue;
			mk = getarraykeys( mv );
			for ( j = 0; j < mk.size; j++ )
			{
				if ( !isarray( mv[mk[j]] ) )
				{
					cap_dump_anim( "dogmove." + sets[i] + "." + mk[j], mv[mk[j]] );
					cap_dump_profile( "dogmove." + sets[i] + "." + mk[j], mv[mk[j]] );
				}
			}
		}
	}
}

// Cumulative 2D root travel at 40 even steps of the clip.
cap_dump_profile( name, a )
{
	line = "R " + name + " len=" + getanimlength( a ) + " d=0";
	prev = getmovedelta( a, 0, 0 );
	total = 0;
	for ( i = 1; i <= 40; i++ )
	{
		cur = getmovedelta( a, 0, i / 40 );
		dx = cur[0] - prev[0];
		dy = cur[1] - prev[1];
		total += sqrt( dx * dx + dy * dy );
		prev = cur;
		line += "," + int( total * 100 ) / 100;
	}
	w( line );
}

cap_dump_set( tag, arr )
{
	ks = getarraykeys( arr );
	for ( i = 0; i < ks.size; i++ )
	{
		v = arr[ks[i]];
		if ( !isdefined( v ) )
			continue;
		if ( isarray( v ) )
		{
			cap_dump_set( tag + "." + ks[i], v );
			continue;
		}
		cap_dump_anim( tag + "." + ks[i], v );
	}
}

cap_dump_anim( name, a )
{
	len = getanimlength( a );
	md = getmovedelta( a, 0, 1 );
	ad = getangledelta( a, 0, 1 );
	line = "A " + name + " len=" + len + " md=" + vstr( md ) + " ad=" + ad;
	notes = [];
	notes[0] = "fire";
	notes[1] = "end";
	notes[2] = "board";
	notes[3] = "start_ragdoll";
	notes[4] = "gravity on";
	notes[5] = "stop";
	notes[6] = "blend";
	for ( n = 0; n < notes.size; n++ )
	{
		ts = getnotetracktimes( a, notes[n] );
		if ( !isdefined( ts ) || ts.size == 0 )
			continue;
		line += " " + notes[n] + "=";
		for ( j = 0; j < ts.size; j++ )
		{
			if ( j )
				line += ",";
			line += int( ts[j] * 1000 ) / 1000;
		}
	}
	w( line );
}

cap_mpick()
{
	self endon( "death" );
	id = self.cap_id;
	for ( ;; )
	{
		self waittill( "t5cap_mpick", a );
		w( "E " + gettime() + " " + id + " mpick " + cap_melee_name( a ) );
	}
}

cap_melee_name( a )
{
	if ( !isdefined( a ) )
		return "none";
	sets = [];
	sets["melee"] = level._zombie_melee["zombie"];
	sets["walk_melee"] = level._zombie_walk_melee["zombie"];
	sets["run_melee"] = level._zombie_run_melee["zombie"];
	sets["melee_crawl"] = level._zombie_melee_crawl["zombie"];
	sets["stumpy_melee"] = level._zombie_stumpy_melee["zombie"];
	keys = getarraykeys( sets );
	for ( i = 0; i < keys.size; i++ )
	{
		arr = sets[keys[i]];
		if ( !isdefined( arr ) )
			continue;
		for ( j = 0; j < arr.size; j++ )
		{
			if ( arr[j] == a )
				return keys[i] + "." + j;
		}
	}
	return "?";
}

// Staged hound run: t5cap_dogpos "x,y,z" -- 2.5 s after it spawns (after the
// lightning reveal) the hound is teleported there, facing +x.
cap_dog_place()
{
	self endon( "death" );
	wait 2.5;
	c = strtok( getdvar( "t5cap_dogpos" ), "," );
	pos = ( int( c[0] ), int( c[1] ), int( c[2] ) );
	self forceteleport( pos, ( 0, 0, 0 ) );
	w( "E " + gettime() + " " + self.cap_id + " placed o=" + vstr( self.origin ) );
	cap_on_dog_placed( self );
}

cap_dump_tear()
{
	a = [];
	a["m_1"] = %ai_zombie_boardtear_m_1; a["m_2"] = %ai_zombie_boardtear_m_2; a["m_3"] = %ai_zombie_boardtear_m_3;
	a["m_4"] = %ai_zombie_boardtear_m_4; a["m_5"] = %ai_zombie_boardtear_m_5; a["m_6"] = %ai_zombie_boardtear_m_6;
	a["r_1"] = %ai_zombie_boardtear_r_1; a["r_2"] = %ai_zombie_boardtear_r_2; a["r_3"] = %ai_zombie_boardtear_r_3;
	a["r_4"] = %ai_zombie_boardtear_r_4; a["r_5"] = %ai_zombie_boardtear_r_5; a["r_6"] = %ai_zombie_boardtear_r_6;
	a["l_1"] = %ai_zombie_boardtear_l_1; a["l_2"] = %ai_zombie_boardtear_l_2; a["l_3"] = %ai_zombie_boardtear_l_3;
	a["l_4"] = %ai_zombie_boardtear_l_4; a["l_5"] = %ai_zombie_boardtear_l_5; a["l_6"] = %ai_zombie_boardtear_l_6;
	cap_dump_set( "tear", a );
}

// Knock down every emerged zombie once, 2 s after it gets inside (the
// thundergun's knockdown path), to see the knockdown clips play.
cap_knockdowns()
{
	for ( ;; )
	{
		ai = getaispeciesarray( "axis", "all" );
		for ( i = 0; i < ai.size; i++ )
		{
			z = ai[i];
			if ( isdefined( z.cap_kd_done ) || !z cap_emerged() || !isdefined( z.animname ) || z.animname != "zombie" )
				continue;
			z.cap_kd_done = true;
			w( "E " + gettime() + " " + z.cap_id + " cap_knockdown" );
			cap_knockdown_one( z );
		}
		wait 0.5;
	}
}
