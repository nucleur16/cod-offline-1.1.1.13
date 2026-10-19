// coop_take_mp_estate_tropical: the scene table and scene moments, IW4's
// estate_anim.gsc / _slowmo_breach.gsc pattern on the coop actors.
//
// The clips are C1's own for this mission (animlists/
// coop_take_mp_estate_tropical.animlist): the zodiac ride, Ghost's breach
// stack, the breach room's reactions (IW4 _slowmo_breach: each
// breach_enemy_spawner's "animation" key, played at the spawner),
// Vazquez's cower, plea, walk-out and surrender, Ghost's walk-out, the
// technical crew's climb-out and the patrols' reactions.  They are looked
// up by name (maps\_coopanim::add_anim -> codol_getAnim), never with %anim:
// this module is compiled in every coop match and the other missions'
// clients do not list them.  The common_human fallbacks are %anim.
//
// Coop category only; called only by _coopmission_estate.gsc on its own
// mission.

#using_animtree ( "generic_human" );

// The table (IW4 estate_anim.gsc main()).
init_anims()
{
	// The zodiac: Ghost at the stern (the third-person rider of the
	// intro; the players are the first-person riders).
	maps\_coopanim::add_anim( "ghost", "boat_ride", "estate_intro_3rd_character_right", undefined );
	// The breach: Ghost stacked on the safe room door (IW4 so_/oilrig
	// breach_flash stack, right side).
	maps\_coopanim::add_anim( "ghost", "breach_stack_enter", "breach_flash_r1_enter", undefined );
	maps\_coopanim::add_anim( "ghost", "breach_stack_idle", "breach_flash_r1_idle", %casual_crouch_idle );
	// The lead out: Ghost and Vazquez walk out of the safe room, then
	// Vazquez knocks Ghost aside and bolts through the door.
	maps\_coopanim::add_anim( "ghost", "walkout", "estate_breach_buddy_walkout", undefined );
	maps\_coopanim::add_anim( "ghost", "walkout_p2", "estate_breach_buddy_walkout_p2", undefined );
	maps\_coopanim::add_anim( "vazquez", "walkout", "estate_breach_hvt_walkout", undefined );
	maps\_coopanim::add_anim( "vazquez", "walkout_p2", "estate_breach_hvt_walkout_p2", undefined );
	// Vazquez in the breach room: the cower on the breach, then cowering.
	maps\_coopanim::add_anim( "vazquez", "cower", "estate_breach_hvt_cower", undefined );
	maps\_coopanim::add_anim( "vazquez", "cower_idle", "estate_breach_hvt_cower_idle", %covercrouch_hide_idle );
	// His plea ("don't kill me, I'll talk").
	maps\_coopanim::add_anim( "vazquez", "plead", "unarmed_cowerstand_idle2point", undefined );
	maps\_coopanim::add_anim( "vazquez", "plead_idle", "unarmed_cowerstand_pointidle", %exposed_idle_reacta );
	// The cliff: the authored surrender struct's animation, its idle and
	// the sniper's shot.
	maps\_coopanim::add_anim( "vazquez", "surrender", "estate_surrender_vasquez", undefined );
	maps\_coopanim::add_anim( "vazquez", "surrender_idle", "estate_surrender_vasquez_02", %exposed_idle_reacta );
	maps\_coopanim::add_anim( "vazquez", "surrender_shot", "estate_surrender_vasquez_02_shot", undefined );
	// Talking while standing (IW4 has facial anims on the dialogue; the
	// body gesture is the casual talk twitch, common_human).
	maps\_coopanim::add_anim( "generic", "talk", "casual_stand_v2_twitch_talk", %casual_stand_v2_twitch_talk );
	// The breach room (IW4 _slowmo_breach scr_anim[ generic ][ <animation> ]):
	// the spawners name breach_react_push_guy1 / _guy2, exposed_idle_reactA
	// and breach_react_knife_charge.
	maps\_coopanim::add_anim( "generic", "breach_react_push_guy1", "breach_react_push_guy1", undefined );
	maps\_coopanim::add_anim( "generic", "breach_react_push_guy2", "breach_react_push_guy2", undefined );
	maps\_coopanim::add_anim( "generic", "breach_react_knife_charge", "breach_react_knife_charge", undefined );
	maps\_coopanim::add_anim( "generic", "breach_react_blowback_v1", "breach_react_blowback_v1", undefined );
	maps\_coopanim::add_anim( "generic", "exposed_idle_reacta", "exposed_idle_reactA", %exposed_idle_reacta );
	// IW4 _stealth_anims: a patrol caught walking stops and reacts
	// (_stealth_behavior_generic3); one standing at his spot starts
	// (_stealth_behavior_spotted_short).
	maps\_coopanim::add_anim( "generic", "stealth_react_walk", "patrol_bored_react_walkstop", %exposed_idle_twitch_v4 );
	maps\_coopanim::add_anim( "generic", "stealth_react_idle", "exposed_idle_twitch_v4", %exposed_idle_twitch_v4 );
	// The technical's crew getting out (IW4 _vehicle_aianim, technical).
	maps\_coopanim::add_anim( "generic", "technical_driver_out", "technical_driver_climb_out", undefined );
	maps\_coopanim::add_anim( "generic", "technical_passenger_out", "technical_passenger_climb_out", undefined );

	if ( getDvarInt( "scr_coop_scene_trace" ) )
	{
		names = strTok( "boat_ride breach_stack_enter breach_stack_idle walkout walkout_p2", " " );
		for ( i = 0; i < names.size; i++ )
			trace_len( "ghost", names[ i ] );
		names = strTok( "walkout walkout_p2 cower cower_idle plead plead_idle surrender surrender_idle surrender_shot", " " );
		for ( i = 0; i < names.size; i++ )
			trace_len( "vazquez", names[ i ] );
	}
}

trace_len( who, scene )
{
	a = maps\_coopanim::get_anim( who, scene );
	if ( isDefined( a ) )
		codolprintln( "CoDOL coop estate scene: " + who + " " + scene + " " + getAnimLength( a ) + " s" );
	else
		codolprintln( "CoDOL coop estate scene: " + who + " " + scene + " not available" );
}

// --------------------------------------------------------------------------
// Small pieces.
// --------------------------------------------------------------------------

// A non-looping idle clip held as a loop (IW4 anim_loop restarts the clip
// each cycle): replayed from its start at each end, until coop_scene_stop.
hold_loop( guy, scene )
{
	guy endon( "death" );
	guy endon( "coop_scene_stop" );
	guy notify( "coop_estate_hold_loop" );
	guy endon( "coop_estate_hold_loop" );
	for ( ;; )
	{
		len = maps\_coopanim::anim_single_solo( guy, scene );
		if ( len <= 0 )
			return;
	}
}

// Ghost talks with his hands while he stands with nothing else to do.
talk_gesture( guy )
{
	if ( !isDefined( guy ) || !isAlive( guy ) )
		return;
	if ( isDefined( guy.coop_cast ) || isDefined( guy.coop_estate_scene ) || isDefined( guy.enemy ) && !guy.ignoreall )
		return;
	v = guy getVelocity();
	if ( length( v ) > 5 )
		return;
	guy endon( "death" );
	guy.coop_estate_scene = "talk";
	saved = guy.animname;
	guy.animname = "generic";
	maps\_coopanim::anim_single_solo( guy, "talk" );
	guy.animname = saved;
	if ( isDefined( guy.coop_estate_scene ) && guy.coop_estate_scene == "talk" )
	{
		guy.coop_estate_scene = undefined;
		maps\_coopanim::release( guy );
	}
}

// The scene start at a node, or undefined when the clip's start is not near
// the actor (a node whose reference the coop addon does not carry): the
// scene is then played where he stands.
near_start( guy, clip, node, limit )
{
	if ( !isDefined( clip ) || !isDefined( node ) )
		return undefined;
	org = maps\_coopanim::node_origin( node, guy );
	ang = maps\_coopanim::node_angles( node, guy );
	start = getStartOrigin( org, ang, clip );
	if ( distance( start, guy.origin ) > limit || abs_( start[ 2 ] - guy.origin[ 2 ] ) > 48 )
	{
		codolprintln( "CoDOL coop estate scene: start " + start + " is " + int( distance( start, guy.origin ) ) + " from " + guy.origin + "; played in place" );
		return undefined;
	}
	return node;
}

abs_( x )
{
	if ( x < 0 )
		return 0 - x;
	return x;
}

// --------------------------------------------------------------------------
// The scenes.
// --------------------------------------------------------------------------

// Aboard the zodiac: Ghost's rider clip, held on its last frame until the
// landing clears his pose.
boat_ride( g )
{
	if ( !isDefined( g ) )
		return;
	g.animname = "ghost";
	maps\_coopanim::anim_single_solo( g, "boat_ride", undefined, "inplace", true );
}

// The breach stack: when Ghost reaches checkpoint_breach_ghost he holds the
// stack idle there until the charge goes (then breach_enter).
breach_stack( g, spot, yaw )
{
	if ( !isDefined( g ) )
		return;
	g endon( "death" );
	level endon( "coop_estate_breached" );
	for ( t = 0; t < 40; t += 0.25 )
	{
		if ( distance( ( g.origin[ 0 ], g.origin[ 1 ], 0 ), ( spot[ 0 ], spot[ 1 ], 0 ) ) < 90 )
			break;
		wait( 0.25 );
	}
	wait( 0.5 );
	g.animname = "ghost";
	g.coop_estate_scene = "stack";
	// IW4 _slowmo_breach: breach_flash_r1_idle is the stack (it starts on
	// the same spot as r1_enter, MEASURED), and r1_enter is the way IN
	// after the blast (115 units, turning -143): breach_stack_done.
	g forceTeleport( g.origin, ( 0, yaw, 0 ) );
	codolprintln( "CoDOL coop estate scene: Ghost stacks on the breach door at " + g.origin );
	maps\_coopanim::anim_loop_solo( g, "breach_stack_idle" );
}

breach_stack_done( g )
{
	level notify( "coop_estate_breached" );
	if ( !isDefined( g ) || !isDefined( g.coop_estate_scene ) || g.coop_estate_scene != "stack" )
		return;
	g.coop_estate_scene = undefined;
	g thread breach_enter();
}

// The blast (1.5 s after the charge is set, _coopmission_estate
// stage_breach), then IW4's r1_enter from the stack with its root motion: he
// runs up to the blown doorway and turns into it; then he fights.  Played in
// place where its path is not clear.
breach_enter()
{
	self endon( "death" );
	wait( 1.6 );
	if ( maps\_coopanim::has_anim( "ghost", "breach_stack_enter" ) )
	{
		self.animname = "ghost";
		maps\_coopanim::anim_single_root( self, "breach_stack_enter" );
	}
	maps\_coopanim::release( self );
}

// IW4 _slowmo_breach: each room guard plays his spawner's reaction at the
// spawner (anim_generic_first_frame, then anim_generic at the blast), and
// fights when it ends.
breach_reactions( ais )
{
	for ( i = 0; i < ais.size; i++ )
	{
		ai = ais[ i ];
		if ( !isDefined( ai ) || !isAlive( ai ) || !isDefined( ai.coop_spawner ) || !isDefined( ai.coop_spawner.animation ) )
			continue;
		ai thread breach_reaction( ai.coop_spawner );
	}
}

breach_reaction( spawner )
{
	self endon( "death" );
	scene = toLower( spawner.animation );
	if ( !maps\_coopanim::has_anim( "generic", scene ) )
	{
		codolprintln( "CoDOL coop estate scene: no reaction " + scene );
		return;
	}
	self.animname = "generic";
	node = spawnStruct();
	node.origin = spawner.origin;
	node.angles = spawner.angles;
	codolprintln( "CoDOL coop estate scene: breach guard ent " + self getEntityNumber() + " reacts " + scene );
	maps\_coopanim::anim_single_solo( self, scene, node, "zonly_physics" );
}

// Vazquez in the breach room: he cowers at the blast and stays down.
vazquez_breach( v )
{
	if ( !isDefined( v ) )
		return;
	v endon( "death" );
	v.animname = "vazquez";
	v.coop_estate_scene = "cower";
	maps\_coopanim::anim_single_solo( v, "cower" );
	if ( isDefined( v.coop_estate_scene ) && v.coop_estate_scene == "cower" )
		level thread hold_loop( v, "cower_idle" );
}

// "Don't kill me, I'll talk": up from the cower, pointing, pleading.
vazquez_plead( v )
{
	if ( !isDefined( v ) )
		return;
	v endon( "death" );
	v notify( "coop_estate_hold_loop" );
	v.animname = "vazquez";
	v.coop_estate_scene = "plead";
	maps\_coopanim::anim_single_solo( v, "plead" );
	if ( isDefined( v.coop_estate_scene ) && v.coop_estate_scene == "plead" )
		maps\_coopanim::anim_loop_solo( v, "plead_idle" );
}

// The lead out: Vazquez and Ghost walk out of the safe room together, Ghost
// picking up the pistol.  The addon carries no reference node for it
// (post_breach_scene_node is the players' checkpoint: the clip's start from
// it lands outside the safe room, MEASURED on the probe), so each plays it
// from where he stands, turned so the clip's own travel heads for
// <toward> (Vazquez's way out).  Returns the scene length (0 when the clips
// are not available).
walkout_start( v, g, toward )
{
	if ( !isDefined( v ) )
		return 0;
	v notify( "coop_estate_hold_loop" );
	v.animname = "vazquez";
	clip = maps\_coopanim::get_anim( "vazquez", "walkout" );
	if ( !isDefined( clip ) )
		return 0;
	mode = "zonly_physics";
	v.coop_estate_scene = "walkout";
	face_travel( v, clip, toward );
	len = maps\_coopanim::anim_single_solo( v, "walkout", undefined, mode, true );
	// Ghost joins it when he is in the room with him (same floor, close).
	if ( isDefined( g ) && isAlive( g ) && distance2D( g.origin, v.origin ) < 300 && abs_( g.origin[ 2 ] - v.origin[ 2 ] ) < 48 )
	{
		g.animname = "ghost";
		gclip = maps\_coopanim::get_anim( "ghost", "walkout" );
		if ( isDefined( gclip ) )
		{
			g.coop_estate_scene = "walkout";
			face_travel( g, gclip, toward );
			maps\_coopanim::anim_single_solo( g, "walkout", undefined, mode, true );
		}
	}
	codolprintln( "CoDOL coop estate scene: the walk-out, " + len + " s, from " + v.origin );
	return len;
}

// Turn guy so <clip>'s whole root travel (getMoveDelta) points at <toward>.
face_travel( guy, clip, toward )
{
	if ( !isDefined( toward ) )
		return;
	d = getMoveDelta( clip, 0, 1 );
	h = vectorToAngles( ( toward[ 0 ] - guy.origin[ 0 ], toward[ 1 ] - guy.origin[ 1 ], 0 ) )[ 1 ];
	if ( d[ 0 ] * d[ 0 ] + d[ 1 ] * d[ 1 ] > 256 )
		h = h - vectorToAngles( ( d[ 0 ], d[ 1 ], 0 ) )[ 1 ];
	guy forceTeleport( guy.origin, ( 0, h, 0 ) );
}

// He knocks Ghost aside and bolts through the door.
walkout_bolt( v, g )
{
	len = 0;
	// The walk-out's own threads end its clips (stopanimscripted) as they
	// run out; let them finish first, or one stops the bolt it overlaps
	// (MEASURED: "stopanimscripted after 0 ms" on the probe).
	settle( v );
	if ( isDefined( g ) && isDefined( g.coop_estate_scene ) && g.coop_estate_scene == "walkout" )
		settle( g );
	wait( 0.1 );
	if ( isDefined( v ) && isAlive( v ) )
	{
		v.animname = "vazquez";
		len = maps\_coopanim::anim_single_solo( v, "walkout_p2", undefined, "zonly_physics", true );
	}
	if ( isDefined( g ) && isAlive( g ) && isDefined( g.coop_estate_scene ) && g.coop_estate_scene == "walkout" )
	{
		g.animname = "ghost";
		glen = maps\_coopanim::anim_single_solo( g, "walkout_p2", undefined, "zonly_physics", true );
		level thread release_after( g, glen, "walkout" );
	}
	if ( len > 0 )
		wait( len );
	if ( isDefined( v ) && isAlive( v ) )
	{
		v.coop_estate_scene = undefined;
		maps\_coopanim::release( v );
	}
}

// Stop the clip the actor is in (its scene threads end with it).
settle( guy )
{
	if ( isDefined( guy ) && isAlive( guy ) )
	{
		guy notify( "coop_scene_stop" );
		guy stopanimscripted();
	}
}

release_after( guy, delay, scene )
{
	guy endon( "death" );
	if ( delay > 0 )
		wait( delay );
	if ( isDefined( guy.coop_estate_scene ) && guy.coop_estate_scene == scene )
	{
		guy.coop_estate_scene = undefined;
		maps\_coopanim::release( guy );
	}
}

// The cliff: at the authored surrender struct (estate_surrender_vasquez),
// hands up, then the surrender idle until the shot.
// The addon's script_struct whose animation key is estate_surrender_vasquez
// (MEASURED: (1696 4080 30.6), yaw 270, by vazquez_run_away_cliff).
surrender_struct()
{
	if ( isDefined( level.struct ) )
	{
		for ( i = 0; i < level.struct.size; i++ )
		{
			s = level.struct[ i ];
			if ( isDefined( s.animation ) && toLower( s.animation ) == "estate_surrender_vasquez" )
				return s;
		}
	}
	s = spawnStruct();
	s.origin = ( 1696, 4080, 30.6 );
	s.angles = ( 0, 270, 0 );
	return s;
}

surrender( v, node )
{
	if ( !isDefined( v ) )
		return;
	v endon( "death" );
	v endon( "coop_estate_shot" );
	v.animname = "vazquez";
	v.coop_estate_scene = "surrender";
	clip = maps\_coopanim::get_anim( "vazquez", "surrender" );
	if ( isDefined( clip ) )
	{
		n = near_start( v, clip, node, 400 );
		maps\_coopanim::anim_single_solo( v, "surrender", n, "zonly_physics" );
	}
	maps\_coopanim::anim_loop_solo( v, "surrender_idle" );
}

// The shot from across the water: the shot clip, then dead where it leaves
// him (IW4 set_deathanim: a.nodeath keeps the death animscript off).
surrender_shot( v )
{
	if ( !isDefined( v ) || !isAlive( v ) )
		return false;
	v notify( "coop_estate_shot" );
	v.animname = "vazquez";
	clip = maps\_coopanim::get_anim( "vazquez", "surrender_shot" );
	if ( !isDefined( clip ) )
		return false;
	maps\_coopanim::release( v );
	// The surrender's thread stops its clip as it ends; let it, before the
	// shot starts (see walkout_bolt).
	wait( 0.1 );
	if ( !isAlive( v ) )
		return true;
	len =maps\_coopanim::anim_single_solo( v, "surrender_shot", undefined, "zonly_physics", true );
	v thread die_after( len );
	return true;
}

die_after( len )
{
	self endon( "death" );
	wait( len - 0.1 );
	if ( !isDefined( self.a ) )
		self.a = spawnStruct();
	self.a.nodeath = true;
	self setCanDamage( true );
	self.coop_cast = undefined;
	self doDamage( self.health + 500, self.origin );
}

// IW4 _stealth: an alerted patrolman reacts before he fights.
stealth_react( walking )
{
	self endon( "death" );
	self.animname = "generic";
	scene = "stealth_react_idle";
	if ( walking )
		scene = "stealth_react_walk";
	maps\_coopanim::anim_single_solo( self, scene, undefined, "normal" );
}

// The technical's crew out of the cab at the end of its path.
climb_out( driver )
{
	self endon( "death" );
	self.animname = "generic";
	scene = "technical_passenger_out";
	if ( driver )
		scene = "technical_driver_out";
	maps\_coopanim::anim_single_solo( self, scene, undefined, "normal" );
}
