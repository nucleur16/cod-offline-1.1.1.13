// coop_defense_mp_crash_tropical: the scene table, the way an IW4 level's
// <level>_anim.gsc fills level.scr_anim (maps\_coopanim's
// level.coop_scr_anim[ animname ][ scene ]).
//
// The clips are the ones animlists/coop_defense_mp_crash_tropical.animlist
// lists for generic_human (most are CoDOL's own crash_* scenes; the rest are
// IW4's: estate_ghost_radio from estate_anim, hunted_open_barndoor_* from
// airport / cliffhanger, breach_react_guntoss_v2_guy1 from _slowmo_breach).
// They are looked up by name (codol_getAnim), never with %anim: every coop
// mission module compiles in every coop match, and a %anim of a clip only
// this mission's client lists would shift the actor channel's tree indices
// in every other coop match.  The fallbacks are common_human clips, which
// every coop client loads.
//
// Coop category only; names no multiplayer module.

#using_animtree ( "generic_human" );

init_scenes()
{
	// The intro: the masked man drags the rookie out to the street
	// (crash_body_drag_enemy, played against the rookie's
	// crash_body_drag_player view), then stands over him.
	maps\_coopanim::add_anim( "dragger", "drag", "crash_body_drag_enemy" );
	maps\_coopanim::add_anim( "dragger", "idle", "drone_stand_idle", %casual_stand_idle );

	// The interrogation room: one of the boss's men working the rookie over
	// (village_interrogationb_price: three punches, then he picks up a
	// pistol), the boss's reaction to Ghost's flashbang
	// (crash_flash_react_ortiz).  village_interrogationb_phone is the
	// phone's own clip (=script_model), not the boss's.
	maps\_coopanim::add_anim( "ortiz", "flash_react", "crash_flash_react_ortiz", %exposed_flashbang_v1 );
	maps\_coopanim::add_anim( "puncher", "interrogate", "village_interrogationb_price" );
	// A flashed man sprays blind (IW4 _slowmo_breach's gun toss reaction).
	maps\_coopanim::add_anim( "watcher", "flash_spray", "breach_react_guntoss_v2_guy1" );

	// Ghost: peeks round the door frame and throws the flashbang
	// (crash_flash_price_peek / _throw, flash_release at 39%); pushes the
	// double door open (IW4 hunted_open_barndoor_flathand); touches his
	// radio while Price talks (IW4 estate_ghost_radio).
	maps\_coopanim::add_anim( "ghost", "flash_peek", "crash_flash_price_peek" );
	maps\_coopanim::add_anim( "ghost", "flash_throw", "crash_flash_price_throw" );
	maps\_coopanim::add_anim( "ghost", "door_open", "hunted_open_barndoor_flathand" );
	maps\_coopanim::add_anim( "ghost", "radio", "estate_ghost_radio" );

	// The searchers at the wreck (group_investigate_heli): crouched over the
	// debris or looking it over standing, and the crouched one's getting
	// up when he is disturbed.
	maps\_coopanim::add_anim( "searcher", "crouch_idle", "crash_search_wreckage_crouch_idle", %casual_crouch_idle );
	maps\_coopanim::add_anim( "searcher", "crouch_end", "crash_search_wreckage_crouch_end" );
	maps\_coopanim::add_anim( "searcher", "stand_idle", "crash_search_wreckage_patrol_idle", %patrol_bored_idle );

	// Flashed men (IW4 animscripts\flashed: the five exposed reactions;
	// common_human).
	flashed = [];
	flashed[ 0 ] = %exposed_flashbang_v1;
	flashed[ 1 ] = %exposed_flashbang_v2;
	flashed[ 2 ] = %exposed_flashbang_v3;
	flashed[ 3 ] = %exposed_flashbang_v4;
	flashed[ 4 ] = %exposed_flashbang_v5;
	if ( !isDefined( level.coop_scr_anim[ "generic" ] ) )
		level.coop_scr_anim[ "generic" ] = [];
	level.coop_scr_anim[ "generic" ][ "flashed" ] = flashed;

	n = 0;
	keys = getArrayKeys( level.coop_scr_anim );
	for ( i = 0; i < keys.size; i++ )
		n += getArrayKeys( level.coop_scr_anim[ keys[ i ] ] ).size;
	codolprintln( "CoDOL coop crash: scene table, " + n + " scenes" );
}

// The clip the scene plays, or undefined.
clip( animname, scene )
{
	return maps\_coopanim::get_anim( animname, scene );
}
