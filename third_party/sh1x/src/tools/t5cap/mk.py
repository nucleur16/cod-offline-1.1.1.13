"""Regenerate bin/home/codol_rawfiles/maps/_zombiemode_t5cap.gsc from the
Plutonium capture script (tools/t5cap/zz_t5cap.gsc): the body is shared, only
the platform block differs.

    python tools/t5cap/mk.py bin/home/codol_rawfiles/maps/_zombiemode_t5cap.gsc
"""
import os
import sys
src = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'zz_t5cap.gsc')
dst = sys.argv[1]
s = open(src).read()
a = s.index('// ---- platform')
b = s.index('// ---- end platform ----')
plat = r'''// Developer-only actor telemetry (docs/ZOMBIE_T5_MEASURED.md).
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
	if ( getDvarInt( "ai_tracePace" ) < 0 )
		setDvar( "ai_tracePace", z getEntityNumber() );
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

cap_dog_kill( z, pos )
{
	z doDamage( z.health + 50, pos, level.cap_player );
}

// A script_model takes its tree from the first %anim it plays here.
cap_pose_prep( m )
{
}

// The hound clips by name (codol_getAnim adds no node to the server tree).
cap_dog_clip( name )
{
	return codol_getAnim( "zombie_dog", name );
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
'''
out = plat + s[b:]
open(dst, 'w', newline='\n').write(out)
print("wrote", dst)
