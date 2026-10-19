// The killhouse missions' scene table (coop_training_mp_killhouse_sh,
// coop_spec1_mp_killhouse_sh): CoD4 maps\killhouse_anim.gsc and IW4
// maps\trainer_anim.gsc / maps\_humvee.gsc in coop form.
//
// level.coop_scr_anim[ animname ][ scene ] (maps\_coopanim add_anim /
// add_anim_variants), filled by codol_getAnim from the clips this mission's
// animlist puts in the tree; a clip the mission does not ship falls back to a
// common_human one.  None of these clips is named with %anim (see
// maps\_coopanim.gsc: a mission clip named with %anim would shift the actor
// channel's tree indices in every other coop mission).
//
// The cast stands in animscripts\soldier_stop's cast pose (coop_cast set):
// coop_idle_anim is the held clip and coop_pose_anim a clip played from its
// start.  Most of these idles are NOT looping clips (the XAnim loop flag is
// off: killhouse_price_stand_idle, killhouse_sas_*_idle, humvee_idle_*,
// parabolic_leaning_guy_idle_training, roadkill_humvee_map_sequence_quiet_idle
// ...; MEASURED in the zone): IW4's anim_loop restarts them every cycle, so
// idle_loop() below replays the cycle (coop_pose_anim, set back to time 0)
// rather than holding a clip that would freeze on its last frame.
//
// Coop category only; names no multiplayer module.

#using_animtree ( "generic_human" );

// Built once, at the first cast placement (the tree is complete by then).
setup()
{
	if ( isDefined( level.coop_killhouse_scenes ) )
		return;
	level.coop_killhouse_scenes = true;

	// Tiny Man has Gaz's part (CoD4 killhouse_anim: gaz).
	maps\_coopanim::add_anim_variants( "waters", "idle", list1( "killhouse_gaz_idleb" ), %patrol_bored_idle );
	maps\_coopanim::add_anim( "waters", "intro", "killhouse_gaz_intro" );
	maps\_coopanim::add_anim_variants( "waters", "talk",
		list3( "killhouse_gaz_talk_side", "killhouse_gaz_point_side", "killhouse_gaz_idlea" ), %casual_stand_v2_twitch_talk );

	// Newcastle, the second instructor: Gaz's instructor set (no clip of his
	// own ships).
	maps\_coopanim::add_anim_variants( "nwc", "idle", list1( "killhouse_gaz_idlea" ), %patrol_bored_idle );
	maps\_coopanim::add_anim_variants( "nwc", "talk",
		list2( "killhouse_gaz_point_side", "killhouse_gaz_talk_side" ), %casual_stand_v2_twitch_talk );

	// Price on foot (by the jeep, at the end of the course): CoD4's stand
	// idle and stand talk (the recording's arms-crossed Price).
	maps\_coopanim::add_anim_variants( "price_stand", "idle", list1( "killhouse_price_stand_idle" ), %casual_stand_v2_idle );
	maps\_coopanim::add_anim_variants( "price_stand", "talk", list1( "killhouse_price_stand_talk" ), %casual_stand_v2_twitch_talk );

	// The SAS of CoD4's reveal, in their reveal idles.
	maps\_coopanim::add_anim_variants( "price", "idle", list1( "killhouse_sas_price_idle" ), %casual_stand_v2_idle );
	maps\_coopanim::add_anim_variants( "sas1", "idle", list1( "killhouse_sas_1_idle" ), %casual_stand_idle );
	maps\_coopanim::add_anim_variants( "sas2", "idle", list1( "killhouse_sas_2_idle" ), %casual_stand_idle );
	maps\_coopanim::add_anim_variants( "sas3", "idle", list1( "killhouse_sas_3_idle" ), %casual_stand_idle );

	// Pvt. Lootz at the laptop (CoD4 chair_guy_setup: chair_idles).  The
	// plain idle is listed twice so the twitches stay occasional.
	maps\_coopanim::add_anim_variants( "chair_guy", "idle",
		list4( "killhouse_laptop_idle", "killhouse_laptop_idle", "killhouse_laptop_twitch", "killhouse_laptop_lookup" ), %casual_crouch_idle );
	maps\_coopanim::add_anim( "chair_guy", "talk", "killhouse_laptop_lookup" );

	// Price at the wheel (IW4 maps\_humvee: position 0, tag_driver).
	maps\_coopanim::add_anim_variants( "driver_seat", "idle", list1( "humvee_idle_frontl" ), %bh_pilot_idle );
	maps\_coopanim::add_anim( "driver_seat", "getout", "humvee_driver_climb_out" );

	// spec1: the tower Price's arrival.
	maps\_coopanim::add_anim_variants( "spec1_tower", "idle", list1( "killhouse_gaz_idlea" ), %casual_stand_v2_idle );
	maps\_coopanim::add_anim( "spec1_tower", "intro", "killhouse_gaz_intro" );
	maps\_coopanim::add_anim_variants( "spec1_tower", "talk",
		list2( "killhouse_gaz_point_side", "killhouse_gaz_talk_side" ), %casual_stand_v2_twitch_talk );
	maps\_coopanim::add_anim_variants( "spec1_price", "idle", list1( "killhouse_sas_price_idle" ), %casual_stand_v2_idle );
}

list1( a )
{
	l = [];
	l[ 0 ] = a;
	return l;
}

list2( a, b )
{
	l = list1( a );
	l[ 1 ] = b;
	return l;
}

list3( a, b, c )
{
	l = list2( a, b );
	l[ 2 ] = c;
	return l;
}

list4( a, b, c, d )
{
	l = list3( a, b, c );
	l[ 3 ] = d;
	return l;
}

// The animname a cast key plays as (undefined: his authored idle only).
animname_for( key )
{
	switch ( key )
	{
	case "waters":
	case "nwc":
	case "price":
	case "sas1":
	case "sas2":
	case "sas3":
	case "chair_guy":
	case "driver_seat":
	case "spec1_tower":
	case "spec1_price":
		return key;
	case "price2":
	case "driver":
		return "price_stand";
	}
	return undefined;
}

// A placed cast member's idle: his key's scene idle, else his spawner's
// authored animation (the camp's ambient soldiers: civilian_reader_2,
// training_pushups_guy1, cliffhanger_welder_*, ...), else <fallback>.
// Starts the replay loop.
start_idle( key, authored, fallback )
{
	setup();
	self.animname = animname_for( key );
	clips = undefined;
	if ( isDefined( self.animname ) && maps\_coopanim::has_anim( self.animname, "idle" ) )
		clips = level.coop_scr_anim[ self.animname ][ "idle" ];
	else if ( isDefined( authored ) )
	{
		a = maps\_coopanim::scene_anim( authored, fallback );
		if ( isDefined( a ) )
			clips = list1( a );
	}
	if ( !isDefined( clips ) && isDefined( fallback ) )
		clips = list1( fallback );
	if ( !isDefined( clips ) )
		return;
	self.coop_idle_clips = clips;
	self.coop_idle_anim = clips[ 0 ];
	self thread idle_loop();
}

// IW4 anim_loop on the cast pose: one cycle of an idle (a random variant),
// restarted from its first frame when it ends, paused while the actor talks
// or plays a scene.
idle_loop()
{
	self endon( "death" );
	self notify( "coop_idle_loop" );
	self endon( "coop_idle_loop" );
	for ( ;; )
	{
		if ( !isDefined( self.coop_scene_busy ) )
		{
			clips = self.coop_idle_clips;
			c = clips[ randomInt( clips.size ) ];
			self.coop_idle_anim = c;
			self.coop_pose_anim = c;
			self notify( "coop_cast_pose" );
			len = getAnimLength( c );
			if ( len < 0.5 )
				len = 0.5;
			self.coop_idle_until = getTime() + int( len * 1000 );
		}
		self waittill_idle_cycle();
	}
}

waittill_idle_cycle()
{
	self endon( "coop_scene_done" );
	for ( ;; )
	{
		wait( 0.1 );
		if ( !isDefined( self.coop_scene_busy ) && isDefined( self.coop_idle_until ) && getTime() >= self.coop_idle_until )
			return;
	}
}

// Stop the replay (the actor leaves the cast pose for a ride, a scene or the
// soldier animscripts).
stop_idle()
{
	self notify( "coop_idle_loop" );
	self.coop_idle_clips = undefined;
}

// A clip played once over the idle, in place (a talk gesture, an intro); the
// idle resumes after.  <cap> limits how long it is held (a line's length).
play_once( clip, cap )
{
	self endon( "death" );
	if ( !isDefined( clip ) )
		return;
	self notify( "coop_scene_once" );
	self endon( "coop_scene_once" );
	len = getAnimLength( clip );
	if ( isDefined( cap ) && cap < len )
		len = cap;
	if ( len < 0.5 )
		len = 0.5;
	self.coop_scene_busy = true;
	self.coop_pose_anim = clip;
	self notify( "coop_cast_pose" );
	if ( getDvarInt( "scr_coop_scene_trace" ) )
		codolprintln( "CoDOL coop scene: " + self.coop_cast + " plays a " + getAnimLength( clip ) + " s clip for " + len + " s" );
	wait( len );
	self.coop_scene_busy = undefined;
	if ( isDefined( self.coop_idle_clips ) )
		self notify( "coop_scene_done" );
	else
	{
		self.coop_pose_anim = undefined;
		self notify( "coop_cast_pose" );
	}
}

// The talk clip for one line: the key's talk set, or the common talking
// twitch.  A crouched speaker (Mac and the recruits at the line, Lootz in
// his chair has his own) keeps his idle.
talk_clip()
{
	if ( isDefined( self.animname ) && maps\_coopanim::has_anim( self.animname, "talk" ) )
		return maps\_coopanim::get_anim( self.animname, "talk" );
	if ( isDefined( self.coop_crouched ) )
		return undefined;
	return %casual_stand_v2_twitch_talk;
}

// A talking cast member: his talk clip for the line's length.  A scene in
// progress (Tiny Man's intro) is not cut by the next line.
talk( time )
{
	if ( isDefined( self.coop_scene_until ) && getTime() < self.coop_scene_until )
		return;
	clip = talk_clip();
	if ( !isDefined( clip ) )
		return;
	cap = undefined;
	if ( isDefined( time ) )
		cap = time + 0.3;
	self thread play_once( clip, cap );
}

// A scene clip played to its end over the idle (an intro); lines spoken
// meanwhile do not interrupt it.
scene( name )
{
	if ( !isDefined( self.animname ) || !maps\_coopanim::has_anim( self.animname, name ) )
		return 0;
	clip = maps\_coopanim::get_anim( self.animname, name );
	len = getAnimLength( clip );
	self.coop_scene_until = getTime() + int( len * 1000 );
	self thread play_scene( clip );
	return len;
}

// The scene with its root motion (killhouse_gaz_intro steps and turns him
// 127 degrees toward the arriving rookie), or over the idle in place when
// its path is not clear; the idle resumes where it ends.
play_scene( clip )
{
	self endon( "death" );
	if ( !self maps\_coopanim::root_path_clear( clip, self.origin, self.angles[ 1 ] ) )
	{
		self play_once( clip );
		return;
	}
	self notify( "coop_scene_once" );
	self.coop_scene_busy = true;
	self maps\_coopanim::play_root( clip );
	self.coop_scene_busy = undefined;
	if ( isDefined( self.coop_idle_clips ) )
		self notify( "coop_scene_done" );
}
