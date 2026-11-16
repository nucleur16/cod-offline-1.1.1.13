// coop enemy soldier bodies.
//
// CoDOL ships no "character" xasset type (the zm character script says the
// same), so this is an ordinary GSC module that names raw xmodels, the way
// IW4's character\character_sp_* scripts do: setModel the body, attach the
// head.
//
// WHICH MODELS EXIST, MEASURED 2026-10-07 against docs/zone_assets/ALL_xmodel.txt
// (names only; the zones themselves stay out of the repo):
//
//   militia (IW4 "favela" faction)   body_militia_{assault,lmg,smg}_a?_{blk,wht}
//                                    + head_militia_{a_wht,bb_blk_hat,bc_blk,bd_blk}
//       in all seven enemy coop playlist zones (prolog, spec/defense crash,
//       spec/take estate, spec/take fav); in NO killhouse coop zone.
//   shadow company (IW4 "desert_elite")
//                                    body_shadow_co_{assault,lmg,smg,shotgun}
//                                    + head_shadow_co_{b,c}
//       only in coop_prolog_mp_crash_intro, coop_spec_mp_crash_tropical,
//       coop_spec_mp_estate_tropical, coop_take_mp_estate_tropical.
//   juggernaut                       body_complete_sp_juggernaut (one piece)
//       in common_gametype_coop, i.e. every coop match.
//
// A model name in no mounted zone must never be precached: the client is told
// the name over a configstring and has nothing to draw.  So the faction a
// soldier may take is decided by which playlist zone is mounted
// (sv_sessionMap, the playlist spelling the lobby sets), and an unknown map
// falls back to the one body every coop match carries.

session_map()
{
	session = getDvar( "sv_sessionMap" );
	if ( session == "" )
		session = getDvar( "mapname" );
	return session;
}

has_militia()
{
	// Survival mounts its own enemies beside MP, never a coop session zone.
	if ( getDvar( "sv_zoneType" ) == "survival" )
		return false;
	session = session_map();
	if ( isSubStr( session, "killhouse" ) )
		return false;
	// The five enemy base levels; every coop playlist zone built on them ships
	// the militia set.
	if ( isSubStr( session, "crash_tropical" ) || isSubStr( session, "crash_intro" ) ||
	     isSubStr( session, "estate_tropical" ) || isSubStr( session, "fav_tropical" ) )
		return true;
	return false;
}

has_shadow_company()
{
	session = session_map();
	switch ( session )
	{
	case "coop_prolog_mp_crash_intro":
	case "coop_spec_mp_crash_tropical":
	case "coop_spec_mp_estate_tropical":
	case "coop_take_mp_estate_tropical":
		return true;
	}
	return false;
}

// coop_take_mp_estate_tropical ships the other three shadow company bodies
// but not the shotgun one (MEASURED, same index).
has_shadow_shotgun()
{
	return has_shadow_company() && session_map() != "coop_take_mp_estate_tropical";
}

militia_bodies( role )
{
	bodies = [];
	switch ( role )
	{
	case "lmg":
		bodies[ 0 ] = "body_militia_lmg_aa_blk";
		bodies[ 1 ] = "body_militia_lmg_ab_blk";
		bodies[ 2 ] = "body_militia_lmg_ac_blk";
		break;
	case "smg":
	case "shgn":
	case "pistol":
		bodies[ 0 ] = "body_militia_smg_aa_blk";
		bodies[ 1 ] = "body_militia_smg_aa_wht";
		bodies[ 2 ] = "body_militia_smg_ab_blk";
		bodies[ 3 ] = "body_militia_smg_ac_blk";
		break;
	default:
		bodies[ 0 ] = "body_militia_assault_aa_blk";
		bodies[ 1 ] = "body_militia_assault_aa_wht";
		bodies[ 2 ] = "body_militia_assault_ab_blk";
		bodies[ 3 ] = "body_militia_assault_ac_blk";
		break;
	}
	return bodies;
}

// The skin tone of the body decides the head, as IW4's militia character
// scripts pair them.
militia_head( body )
{
	if ( isSubStr( body, "_wht" ) )
		return "head_militia_a_wht";

	heads = [];
	heads[ 0 ] = "head_militia_bb_blk_hat";
	heads[ 1 ] = "head_militia_bc_blk";
	heads[ 2 ] = "head_militia_bd_blk";
	return heads[ randomInt( heads.size ) ];
}

shadow_body( role )
{
	switch ( role )
	{
	case "lmg":
		return "body_shadow_co_lmg";
	case "smg":
	case "pistol":
		return "body_shadow_co_smg";
	case "shgn":
		if ( has_shadow_shotgun() )
			return "body_shadow_co_shotgun";
		return "body_shadow_co_smg";
	}
	return "body_shadow_co_assault";
}

juggernaut_body()
{
	return "body_complete_sp_juggernaut";
}

// faction: "militia", "shadow" or "juggernaut".  Downgrades to what the
// mounted zones actually carry.
resolve_faction( faction )
{
	if ( faction == "shadow" && !has_shadow_company() )
		faction = "militia";
	if ( faction == "militia" && !has_militia() )
		faction = "juggernaut";
	return faction;
}

main( faction, role )
{
	faction = resolve_faction( faction );
	self.coop_faction = faction;

	if ( faction == "juggernaut" )
	{
		self setModel( juggernaut_body() );
		self.headModel = undefined;
		return;
	}

	if ( faction == "shadow" )
	{
		self setModel( shadow_body( role ) );
		heads = [];
		heads[ 0 ] = "head_shadow_co_b";
		heads[ 1 ] = "head_shadow_co_c";
		head = heads[ randomInt( heads.size ) ];
	}
	else
	{
		bodies = militia_bodies( role );
		body = bodies[ randomInt( bodies.size ) ];
		self setModel( body );
		head = militia_head( body );
	}

	// attach() is what puts the head's bones (and tag_eye) into the DObj;
	// see character_zm_zombie.gsc for the measured failure without it.
	self attach( head, "", true );
	self.headModel = head;
}

// A cast member's authored look (_cooptraining.gsc precaches every one).
cast_model( body, head )
{
	self.coop_faction = "cast";
	self setModel( body );
	if ( isDefined( head ) )
	{
		self attach( head, "", true );
		self.headModel = head;
	}
	else
		self.headModel = undefined;
}

precache()
{
	precacheModel( juggernaut_body() );

	if ( has_militia() )
	{
		roles = [];
		roles[ 0 ] = "ar";
		roles[ 1 ] = "lmg";
		roles[ 2 ] = "smg";
		for ( r = 0; r < roles.size; r++ )
		{
			bodies = militia_bodies( roles[ r ] );
			for ( i = 0; i < bodies.size; i++ )
				precacheModel( bodies[ i ] );
		}
		precacheModel( "head_militia_a_wht" );
		precacheModel( "head_militia_bb_blk_hat" );
		precacheModel( "head_militia_bc_blk" );
		precacheModel( "head_militia_bd_blk" );
	}

	if ( has_shadow_company() )
	{
		precacheModel( "body_shadow_co_assault" );
		precacheModel( "body_shadow_co_lmg" );
		precacheModel( "body_shadow_co_smg" );
		if ( has_shadow_shotgun() )
			precacheModel( "body_shadow_co_shotgun" );
		precacheModel( "head_shadow_co_b" );
		precacheModel( "head_shadow_co_c" );
	}
}
