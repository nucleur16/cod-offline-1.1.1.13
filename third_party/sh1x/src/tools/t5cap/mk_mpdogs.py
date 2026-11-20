"""Regenerate bin/home/codol_rawfiles/maps/mp/_codol_dogcap.gsc from the
Plutonium MP dog capture script (tools/t5cap/zz_t5mpdogcap.gsc): the body is
shared, only the platform block differs.

    python tools/t5cap/mk_mpdogs.py bin/home/codol_rawfiles/maps/mp/_codol_dogcap.gsc
"""
import os
import sys
src = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'zz_t5mpdogcap.gsc')
dst = sys.argv[1]
s = open(src).read()
a = s.index('// ---- platform')
b = s.index('// ---- end platform ----')
plat = r'''// Developer-only MP attack-dog telemetry (docs/MP_DOGS_T5_PARITY.md).
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
'''
out = plat + s[b:]
open(dst, 'w', newline='\n').write(out)
print("wrote", dst)
