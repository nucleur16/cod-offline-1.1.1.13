// zm dismemberment and zombie blood.
//
// Port of T5's gib path:
//   maps\_zombiemode_spawner.gsc   zombie_gib_on_damage :2819,
//                                  zombie_should_gib :3047,
//                                  head_should_gib :2697,
//                                  zombie_head_gib :2605,
//                                  damage_over_time :2671,
//                                  derive_damage_refs :3137 / init_gib_tags
//   animscripts\zombie_death.gsc   do_gib :1508, get_limb_data :1594,
//                                  throw_gib :1700
// with C1's own models and effects in place of T5's.
//
// ASSETS (all MEASURED, none guessed):
//   * The gib models are the 17 per-family c_zom_*_body_* xmodels; which
//     T5 field each one fills, and why, is in character_zm_zombie.gsc
//     set_gib_models (bone dump of a zm_prototype_sh probe, 2026-10-05).
//   * Blue blood ships in common_gametype_zm (docs/zone_assets/ALL_fx.txt),
//     and the strings next to each effect in the inflated zone name its
//     textures:
//       misc/fx_zombie_bloodspurt_codo     fxt_bio_bloodgush_blue +
//                                          fxt_bio_blooddrops_blue
//       weapon/bullet/fx_flesh_gib_fatal_01_blue  fxt_bio_blood_cloud --
//                                          the blue twin of T5's
//                                          animscript_gib_fx
//                                          (weapon/bullet/fx_flesh_gib_fatal_01)
//   * zmb_headshot and zmb_death_gibs are common_gametype_zm sound aliases
//     (ALL_sound.txt); T5's own refs are zombie_head_gib / zombie_death_gibs.
//
// C1-vs-T5 DIFFERENCES, C1 WINS:
//   * T5 throws the limb with the native self gib( type, pieces ), which has
//     the CLIENT spawn and simulate the gibSpawn model.  C1 has no such
//     method (no "gib" builtin is registered and C1.exe names none), so the
//     piece is a server script_model with the gibSpawn model, launched with
//     physicsLaunchServer -- the server-simulated body C1 itself publishes as
//     an interpolated pose (src/codol/script/codol_physics_launch.c) -- and
//     deleted a few seconds later.  It carries no weapon or item index, is
//     not solid, and is capped in number.
//   * T5's bullet impacts on a zombie are the map's flesh impact table.  MEASURED
//     (impact table decode of the three zm zones, 2026-10-05): zm_asylum_sh's
//     flesh entries are its own blue impacts/flesh_hit_*_zm effects, but
//     zm_prototype_sh and zm_zombie_training reference the red common_mp
//     impacts/flesh_hit_* set.  So every bullet hit on a zombie also plays
//     the blue gush at the hit point, oriented back at the shooter.
//   * damageLocation is not a field here.  The "damage" notify's 8th
//     argument is the struck BONE (see _zombiemode_score.gsc): a bullet is
//     traced through the zombie's hit boxes (codol_actor_spawn.cpp, svFlags
//     bit 2), so partName is the bone the round met, and the hit location is
//     classified from that bone name; T5 hitLocation names are accepted as
//     well.  Only a hit that names no bone falls back to the shot line.
//   * T5 refuses to gib heads AND limbs with the starting pistol.  Here the
//     pistol may still pop a head on a killing headshot -- the round-1 weapon
//     otherwise never shows the feature -- but limbs keep T5's rule.

#include maps\_zombiemode_utility;

init()
{
	level._effect[ "zombie_blood_impact" ] = loadfx( "misc/fx_zombie_bloodspurt_codo" );
	level._effect[ "zombie_gib" ] = loadfx( "weapon/bullet/fx_flesh_gib_fatal_01_blue" );

	add_sound( "zombie_head_gib", "zmb_headshot" );

	// T5 :2614 level.max_head_gibs_per_frame.
	level.max_head_gibs_per_frame = 4;

	// Live thrown limbs.  Each is an entity until it expires.
	level.zm_gib_pieces = [];
	level.zm_gib_piece_max = 8;
	level.zm_gib_piece_life = 6;

	// Developer-only verbs (scr_codol_gibtest); returns at once on a
	// production server.

}

// ---------------------------------------------------------------------------
// The damage watcher.  T5 zombie_gib_on_damage :2819, plus the blue impact.
// Threaded per zombie from _zombiemode_spawner::spawn_zombie.
//
// NO endon( "death" ), for the reason zombie_damage_watch gives: on an actor
// the killing hit's "damage" notify arrives after "death", and the killing
// hit is exactly the one that pops a head or takes a leg.
// ---------------------------------------------------------------------------
zombie_gib_on_damage()
{
	for ( ;; )
	{
		self waittill( "damage", amount, attacker, direction, point, mod, modelName, tagName, partName, flags, weapon );

		if ( !isDefined( self ) )
			return;

		self zombie_blood_impact( point, direction, mod );

		// The bleed-out ticks of a beheaded zombie (damage_over_time) are
		// not wounds of their own.
		if ( isDefined( self.zm_gib_dot_time ) && self.zm_gib_dot_time == getTime() )
			continue;

		if ( !isDefined( weapon ) && isDefined( attacker ) && isPlayer( attacker ) )
			weapon = attacker getCurrentWeapon();

		if ( !self zombie_should_gib( amount, mod ) )
			continue;

		// The struck bone first; the shot line only for a hit that names none.
		location = gib_location( partName );
		source = "bone";
		if ( location == "none" && !is_explosive_mod( mod ) )
		{
			location = self gib_location_from_ray( point, direction );
			source = "ray";
		}

		// scr_codol_gibtest (developer only) sets this.
		if ( is_true( level.zm_gib_diag ) )
		{
			diag_weapon = "<undefined>";
			if ( isDefined( weapon ) )
				diag_weapon = weapon;
			{}
		}

		if ( self head_should_gib( attacker, mod, point, location, weapon ) )
		{
			self zombie_head_gib( attacker, mod, weapon );
			continue;
		}

		if ( is_true( self.gibbed ) )
			continue;

		// T5 :2869: a head hit that did not pop the head does not gib a limb.
		if ( location == "head" )
			continue;

		if ( !self limb_weapon_may_gib( attacker, mod, weapon ) )
			continue;

		refs = self gib_refs( location, mod, point );
		if ( refs.size < 1 )
			continue;

		if ( !isDefined( self.a ) )
			continue;

		self.a.gib_ref = refs[ randomInt( refs.size ) ];
		gib_ref = self.a.gib_ref;

		// The direction the pieces fly (T5 self.damagedir, :1207).
		if ( isDefined( direction ) && lengthSquared( direction ) > 0.0001 )
			self.zm_gib_damage_dir = vectorNormalize( direction );

		// T5 forces the gib only on a live zombie and leaves a dead one to
		// zombie_death.gsc; do_gib is idempotent (self.gibbed), so it is run
		// here either way and the death script's call is the backstop.
		self do_gib();

		// T5 :2909: a zombie that keeps living without a leg crawls.
		if ( self.health > 0 && ( gib_ref == "no_legs" || gib_ref == "right_leg" || gib_ref == "left_leg" ) )
			self maps\_zombiemode_spawner::make_crawler();
	}
}

// Blue blood at the bullet's hit point.  One per zombie per server frame
// (a shotgun's pellets land together) and a level cap per frame, because
// each is a temp entity in every client's snapshot.
zombie_blood_impact( point, direction, mod )
{
	if ( !isDefined( point ) || !isDefined( mod ) )
		return;

	if ( !isDefined( level._effect ) || !isDefined( level._effect[ "zombie_blood_impact" ] ) )
		return;

	if ( !is_bullet_mod( mod ) && mod != "MOD_MELEE" )
		return;

	if ( isDefined( self.zm_blood_time ) && self.zm_blood_time == getTime() )
		return;

	if ( !frame_budget( "blood", 6 ) )
		return;

	self.zm_blood_time = getTime();

	forward = ( 0, 0, 1 );
	if ( isDefined( direction ) && lengthSquared( direction ) > 0.0001 )
		forward = vectorNormalize( direction * -1 );

	playFx( level._effect[ "zombie_blood_impact" ], point, forward );
}

// ---------------------------------------------------------------------------
// Gates.  T5 zombie_should_gib :3047 and head_should_gib :2697.
// ---------------------------------------------------------------------------
zombie_should_gib( amount, mod )
{
	if ( !isDefined( mod ) || !isDefined( amount ) )
		return false;

	if ( is_true( self.no_gib ) )
		return false;

	switch ( mod )
	{
	case "MOD_UNKNOWN":
	case "MOD_CRUSH":
	case "MOD_TELEFRAG":
	case "MOD_FALLING":
	case "MOD_SUICIDE":
	case "MOD_TRIGGER_HURT":
	case "MOD_BURNED":
	case "MOD_MELEE":
		return false;
	}

	// T5 :3120: a hit worth less than a tenth of what the zombie had left
	// does not tear anything off.
	prev_health = amount + self.health;
	if ( prev_health <= 0 )
		prev_health = 1;

	damage_percent = ( amount / prev_health ) * 100;
	if ( damage_percent < 10 )
		return false;

	return true;
}

head_should_gib( attacker, mod, point, location, weapon )
{
	if ( is_true( self.head_gibbed ) )
		return false;

	if ( !isDefined( attacker ) || !isPlayer( attacker ) )
		return false;

	// T5 :2724: only a zombie that is nearly (or already) dead loses its head.
	if ( !isDefined( self.maxhealth ) || self.maxhealth <= 0 )
		return false;

	low_health_percent = ( self.health / self.maxhealth ) * 100;
	if ( low_health_percent > 10 )
		return false;

	if ( !is_bullet_mod( mod ) )
	{
		if ( mod == "MOD_GRENADE" || mod == "MOD_GRENADE_SPLASH" )
			return isDefined( point ) && distance( point, self getTagOrigin( "j_head" ) ) <= 55;

		if ( mod == "MOD_PROJECTILE" )
			return isDefined( point ) && distance( point, self getTagOrigin( "j_head" ) ) <= 10;

		// Shotguns: T5 :2766.
		if ( !isDefined( weapon ) || weapon == "none" || weaponClass( weapon ) != "spread" )
			return false;
	}

	if ( location != "head" )
		return false;

	if ( !isDefined( weapon ) || weapon == "none" )
		return false;

	return true;
}

// T5 zombie_should_gib :3093 for bullets: the round-1 pistol never takes a
// limb off.
limb_weapon_may_gib( attacker, mod, weapon )
{
	if ( !is_bullet_mod( mod ) )
		return true;

	if ( !isDefined( attacker ) || !isPlayer( attacker ) )
		return false;

	if ( !isDefined( weapon ) || weapon == "none" )
		return false;

	if ( isDefined( attacker.zombie_start_weapon ) && weapon == attacker.zombie_start_weapon )
		return false;

	if ( isDefined( level.zombie_start_weapon ) && weapon == level.zombie_start_weapon )
		return false;

	return true;
}

is_bullet_mod( mod )
{
	if ( !isDefined( mod ) )
		return false;

	switch ( mod )
	{
	case "MOD_PISTOL_BULLET":
	case "MOD_RIFLE_BULLET":
	case "MOD_HEAD_SHOT":
	case "MOD_EXPLOSIVE_BULLET":
		return true;
	}

	return false;
}

is_explosive_mod( mod )
{
	if ( !isDefined( mod ) )
		return false;

	switch ( mod )
	{
	case "MOD_GRENADE":
	case "MOD_GRENADE_SPLASH":
	case "MOD_PROJECTILE":
	case "MOD_PROJECTILE_SPLASH":
	case "MOD_EXPLOSIVE":
		return true;
	}

	return false;
}

// ---------------------------------------------------------------------------
// Hit location from the struck bone.  T5 reads self.damageLocation; here the
// notify names the bone of the body (73 bones) or of the attached head
// (29 bones: j_brow_*, j_cheek*, j_eye*, j_jaw, j_lip_*, j_mouth_*, ...).
// Returns "head", "right_arm", "left_arm", "right_leg", "left_leg", "torso",
// "other" (a bone that is none of those) or "none".
// ---------------------------------------------------------------------------
gib_location( part )
{
	if ( !isDefined( part ) )
		return "none";

	part = toLower( part );
	if ( part == "" || part == "none" )
		return "none";

	// T5 hitLocation names.
	switch ( part )
	{
	case "head":
	case "helmet":
	case "neck":
		return "head";
	case "torso_upper":
	case "torso_lower":
		return "torso";
	case "right_arm_upper":
	case "right_arm_lower":
	case "right_hand":
		return "right_arm";
	case "left_arm_upper":
	case "left_arm_lower":
	case "left_hand":
		return "left_arm";
	case "right_leg_upper":
	case "right_leg_lower":
	case "right_foot":
		return "right_leg";
	case "left_leg_upper":
	case "left_leg_lower":
	case "left_foot":
		return "left_leg";
	}

	head_words = [];
	head_words[ 0 ] = "head";
	head_words[ 1 ] = "helmet";
	head_words[ 2 ] = "neck";
	head_words[ 3 ] = "eye";
	head_words[ 4 ] = "brow";
	head_words[ 5 ] = "cheek";
	head_words[ 6 ] = "jaw";
	head_words[ 7 ] = "lip";
	head_words[ 8 ] = "mouth";
	head_words[ 9 ] = "chin";
	head_words[ 10 ] = "levator";
	if ( has_any_word( part, head_words ) )
		return "head";

	side = undefined;
	if ( isSubStr( part, "_ri" ) || isSubStr( part, "right" ) )
		side = "right";
	else if ( isSubStr( part, "_le" ) || isSubStr( part, "left" ) )
		side = "left";

	if ( isDefined( side ) )
	{
		arm_words = [];
		arm_words[ 0 ] = "shoulder";
		arm_words[ 1 ] = "elbow";
		arm_words[ 2 ] = "wrist";
		arm_words[ 3 ] = "index";
		arm_words[ 4 ] = "mid_";
		arm_words[ 5 ] = "pinky";
		arm_words[ 6 ] = "ring";
		arm_words[ 7 ] = "thumb";
		arm_words[ 8 ] = "weapon";
		if ( has_any_word( part, arm_words ) )
			return side + "_arm";

		leg_words = [];
		leg_words[ 0 ] = "hip";
		leg_words[ 1 ] = "knee";
		leg_words[ 2 ] = "ankle";
		leg_words[ 3 ] = "ball";
		if ( has_any_word( part, leg_words ) )
			return side + "_leg";
	}

	if ( isSubStr( part, "spine" ) || isSubStr( part, "mainroot" ) || isSubStr( part, "clavicle" ) )
		return "torso";

	return "other";
}

// FALLBACK: the location of a hit whose notify names no bone.  Until
// 2026-10-05 every bullet on a zombie arrived that way (the actor was traced
// as a capsule, so modelName, tagName and partName were all empty); actors are
// now traced through their hit boxes and a bullet names the struck bone.  What
// still arrives without one -- a doDamage with a point and no trace, or a
// zombie whose DObj the trace could not pose -- is placed here: the struck
// part is the skeleton bone the shot's line passes closest to (T5
// derive_damage_refs does the same with the blast point), counting only bones
// at or past the entry point.  A line that passes no bone within 16 units hit
// nothing that comes off.
gib_location_from_ray( point, direction )
{
	if ( !isDefined( point ) || !isDefined( direction ) || lengthSquared( direction ) < 0.0001 )
		return "none";

	if ( !isDefined( level.zm_gib_ray_bones ) )
	{
		bones = [];
		bones[ bones.size ] = "j_head";
		bones[ bones.size ] = "j_neck";
		bones[ bones.size ] = "j_spine4";
		bones[ bones.size ] = "j_spineupper";
		bones[ bones.size ] = "j_spinelower";
		bones[ bones.size ] = "j_mainroot";
		bones[ bones.size ] = "j_shoulder_ri";
		bones[ bones.size ] = "j_elbow_ri";
		bones[ bones.size ] = "j_wrist_ri";
		bones[ bones.size ] = "j_shoulder_le";
		bones[ bones.size ] = "j_elbow_le";
		bones[ bones.size ] = "j_wrist_le";
		bones[ bones.size ] = "j_hip_ri";
		bones[ bones.size ] = "j_knee_ri";
		bones[ bones.size ] = "j_ankle_ri";
		bones[ bones.size ] = "j_hip_le";
		bones[ bones.size ] = "j_knee_le";
		bones[ bones.size ] = "j_ankle_le";
		level.zm_gib_ray_bones = bones;
	}

	dir = vectorNormalize( direction );
	best = undefined;
	best_d2 = 16 * 16;
	for ( i = 0; i < level.zm_gib_ray_bones.size; i++ )
	{
		v = self getTagOrigin( level.zm_gib_ray_bones[ i ] ) - point;
		t = vectorDot( v, dir );
		if ( t < -8 )
			continue;
		perp = v - dir * t;
		d2 = lengthSquared( perp );
		if ( d2 < best_d2 )
		{
			best = level.zm_gib_ray_bones[ i ];
			best_d2 = d2;
		}
	}

	if ( !isDefined( best ) )
		return "none";

	return gib_location( best );
}

has_any_word( text, words )
{
	for ( i = 0; i < words.size; i++ )
	{
		if ( isSubStr( text, words[ i ] ) )
			return true;
	}
	return false;
}

// T5 :2873-:2972 (the switch on damageLocation) and derive_damage_refs :3137.
gib_refs( location, mod, point )
{
	refs = [];

	switch ( location )
	{
	case "torso":
		// T5: "the torso that gets swapped for guts also removes the left
		// arm, so we need to sometimes do another ref".
		refs[ refs.size ] = "guts";
		refs[ refs.size ] = "right_arm";
		break;

	case "right_arm":
		refs[ refs.size ] = "right_arm";
		break;

	case "left_arm":
		refs[ refs.size ] = "left_arm";
		break;

	case "right_leg":
		// T5 :2894: a leg only comes off a zombie the hit kills, three times
		// as often alone as with the other one.
		if ( self.health <= 0 )
		{
			refs[ refs.size ] = "right_leg";
			refs[ refs.size ] = "right_leg";
			refs[ refs.size ] = "right_leg";
			refs[ refs.size ] = "no_legs";
		}
		break;

	case "left_leg":
		if ( self.health <= 0 )
		{
			refs[ refs.size ] = "left_leg";
			refs[ refs.size ] = "left_leg";
			refs[ refs.size ] = "left_leg";
			refs[ refs.size ] = "no_legs";
		}
		break;

	case "none":
		// T5 :2917: a grenade or a projectile reports no location; derive
		// one from the nearest gib tag to the blast.
		if ( is_explosive_mod( mod ) && isDefined( point ) )
			refs = self derive_damage_refs( point );
		break;

	default:
		refs[ refs.size ] = "guts";
		refs[ refs.size ] = "right_arm";
		refs[ refs.size ] = "left_arm";
		refs[ refs.size ] = "right_leg";
		refs[ refs.size ] = "left_leg";
		refs[ refs.size ] = "no_legs";
		break;
	}

	return refs;
}

// T5 derive_damage_refs :3137 / init_gib_tags.  T5's J_* tag names, lower
// case as the bone dump prints them; every one is a bone of the 73-bone
// c_zom_*_body skeleton the torso models keep.
derive_damage_refs( point )
{
	if ( !isDefined( level.zm_gib_tags ) )
	{
		tags = [];
		tags[ tags.size ] = "j_spinelower";
		tags[ tags.size ] = "j_spineupper";
		tags[ tags.size ] = "j_spine4";
		tags[ tags.size ] = "j_shoulder_le";
		tags[ tags.size ] = "j_elbow_le";
		tags[ tags.size ] = "j_wrist_le";
		tags[ tags.size ] = "j_shoulder_ri";
		tags[ tags.size ] = "j_elbow_ri";
		tags[ tags.size ] = "j_wrist_ri";
		tags[ tags.size ] = "j_hip_le";
		tags[ tags.size ] = "j_knee_le";
		tags[ tags.size ] = "j_ankle_le";
		tags[ tags.size ] = "j_hip_ri";
		tags[ tags.size ] = "j_knee_ri";
		tags[ tags.size ] = "j_ankle_ri";
		level.zm_gib_tags = tags;
	}

	closest = undefined;
	closest_dist = undefined;
	for ( i = 0; i < level.zm_gib_tags.size; i++ )
	{
		d = distanceSquared( point, self getTagOrigin( level.zm_gib_tags[ i ] ) );
		if ( !isDefined( closest_dist ) || d < closest_dist )
		{
			closest = level.zm_gib_tags[ i ];
			closest_dist = d;
		}
	}

	refs = [];
	if ( !isDefined( closest ) )
		return refs;

	switch ( closest )
	{
	case "j_spinelower":
	case "j_spineupper":
	case "j_spine4":
		refs[ refs.size ] = "guts";
		refs[ refs.size ] = "right_arm";
		break;
	case "j_shoulder_le":
	case "j_elbow_le":
	case "j_wrist_le":
		refs[ refs.size ] = "left_arm";
		break;
	case "j_shoulder_ri":
	case "j_elbow_ri":
	case "j_wrist_ri":
		refs[ refs.size ] = "right_arm";
		break;
	case "j_hip_le":
	case "j_knee_le":
	case "j_ankle_le":
		refs[ refs.size ] = "left_leg";
		refs[ refs.size ] = "no_legs";
		break;
	case "j_hip_ri":
	case "j_knee_ri":
	case "j_ankle_ri":
		refs[ refs.size ] = "right_leg";
		refs[ refs.size ] = "no_legs";
		break;
	}

	return refs;
}

// ---------------------------------------------------------------------------
// The head.  T5 zombie_head_gib :2605.
// ---------------------------------------------------------------------------
zombie_head_gib( attacker, mod, weapon )
{
	if ( is_true( self.head_gibbed ) )
		return false;

	// T5 :2626 caps head gibs per frame.
	if ( !frame_budget( "head_gib", level.max_head_gibs_per_frame ) )
		return false;

	head = undefined;
	size = self getAttachSize();
	for ( i = 0; i < size; i++ )
	{
		model = self getAttachModelName( i );
		if ( isSubStr( model, "head" ) )
		{
			head = model;
			break;
		}
	}

	if ( !isDefined( head ) )
		return false;

	self.head_gibbed = true;

	// Read before the head's bones leave the DObj.
	pop_origin = self getTagOrigin( "j_head" );
	neck_origin = self getTagOrigin( "j_neck" );

	// The glow lives on the head's eye bones; stopFxOnTag on a bone that is
	// gone is a script error, so it is stopped first (T5 :2643 does the same).
	self maps\_zombiemode_spawner::zombie_eye_glow_stop();
	self maps\_zombiemode_cortex::cortex_overlay_off();

	self detach( head, "" );
	if ( isDefined( self.torsoDmg5 ) )
		self attach( self.torsoDmg5, "", true );

	if ( frame_budget( "gib_fx", 6 ) )
	{
		playFx( level._effect[ "zombie_gib" ], pop_origin );
		playFx( level._effect[ "zombie_blood_impact" ], neck_origin, ( 0, 0, 1 ) );
	}

	// The shipped zmb_headshot primary recording is a Thundergun shot.
	// Do not dispatch that unrelated sound for a head gib.

	// T5 :2668: a headless zombie that is still standing bleeds out.
	if ( self.health > 0 )
		self thread damage_over_time( self.health * 0.2, 1, attacker, mod, weapon );

	return true;
}

// T5 :2671.  Each tick is flagged so zombie_gib_on_damage does not read it as
// a new wound, and _zombiemode_spawner::zombie_damage_watch does not pay
// damage points for it.
damage_over_time( dmg, delay, attacker, mod, weapon )
{
	self endon( "death" );

	if ( !isAlive( self ) )
		return;

	if ( dmg < 1 )
		dmg = 1;

	if ( !isDefined( attacker ) || !isPlayer( attacker ) )
		attacker = undefined;

	// Credit a bullet-caused beheading only when its own bleed-out kills.
	// A later body hit still supplies its own killing location and weapon.
	headshot = maps\_zombiemode_challenges::zm_is_bullet( mod );
	if ( !isDefined( weapon ) || weapon == "" )
		weapon = "none";

	// doDamage resolves the name against the retained (IW3) means-of-death
	// table; C1's MOD_EXPLOSIVE_BULLET is past it, and anything but a bullet
	// is not what bled the zombie out.
	if ( !isDefined( mod ) || mod == "MOD_EXPLOSIVE_BULLET" )
		mod = "MOD_RIFLE_BULLET";
	else if ( mod != "MOD_PISTOL_BULLET" && mod != "MOD_RIFLE_BULLET" && mod != "MOD_HEAD_SHOT" )
		mod = "MOD_UNKNOWN";

	for ( ;; )
	{
		wait( delay );

		self.zm_gib_dot_time = getTime();

		if ( isDefined( attacker ) )
			self doDamage( dmg, self.origin, attacker, attacker, headshot, mod, 0, weapon );
		else
			self doDamage( dmg, self.origin );
	}
}

// ---------------------------------------------------------------------------
// Limbs.  T5 animscripts\zombie_death.gsc do_gib :1508 / get_limb_data.
// ---------------------------------------------------------------------------
do_gib()
{
	if ( !isDefined( self.a ) || !isDefined( self.a.gib_ref ) )
		return;

	if ( is_true( self.is_on_fire ) )
		return;

	// T5 :1532: once per zombie.
	if ( is_true( self.gibbed ) )
		return;

	gib_ref = self.a.gib_ref;

	if ( gib_ref == "head" )
	{
		self zombie_head_gib();
		return;
	}

	limb = get_limb_data( gib_ref );
	if ( !isDefined( limb ) )
		return;

	if ( !frame_budget( "limb_gib", 4 ) )
		return;

	self.gibbed = true;

	// The flying pieces leave from where the limb is NOW, before the body
	// model changes under them.
	self throw_gib( limb[ "pieces" ] );

	self setModel( limb[ "body_model" ] );
	self attach( limb[ "legs_model" ], "", true );

	self play_sound_on_ent( "zombie_death_gibs" );
}

// T5 :1594 get_limb_data, with the piece list as gibSpawn numbers (1 right
// arm, 2 left arm, 3 right leg, 4 left leg) instead of T5's client piece
// indices.  A family without a gib set (no torsoDmg fields) never gibs.
get_limb_data( gib_ref )
{
	limb = [];
	pieces = [];

	switch ( gib_ref )
	{
	case "right_arm":
		if ( !isDefined( self.torsoDmg2 ) || !isDefined( self.legDmg1 ) )
			return undefined;
		limb[ "body_model" ] = self.torsoDmg2;
		limb[ "legs_model" ] = self.legDmg1;
		pieces[ 0 ] = 1;
		break;

	case "left_arm":
		if ( !isDefined( self.torsoDmg3 ) || !isDefined( self.legDmg1 ) )
			return undefined;
		limb[ "body_model" ] = self.torsoDmg3;
		limb[ "legs_model" ] = self.legDmg1;
		pieces[ 0 ] = 2;
		break;

	case "right_leg":
		if ( !isDefined( self.torsoDmg1 ) || !isDefined( self.legDmg2 ) )
			return undefined;
		limb[ "body_model" ] = self.torsoDmg1;
		limb[ "legs_model" ] = self.legDmg2;
		pieces[ 0 ] = 3;
		break;

	case "left_leg":
		if ( !isDefined( self.torsoDmg1 ) || !isDefined( self.legDmg3 ) )
			return undefined;
		limb[ "body_model" ] = self.torsoDmg1;
		limb[ "legs_model" ] = self.legDmg3;
		pieces[ 0 ] = 4;
		break;

	case "no_legs":
		if ( !isDefined( self.torsoDmg1 ) || !isDefined( self.legDmg4 ) )
			return undefined;
		limb[ "body_model" ] = self.torsoDmg1;
		limb[ "legs_model" ] = self.legDmg4;
		pieces[ 0 ] = 3;
		pieces[ 1 ] = 4;
		break;

	case "guts":
		if ( !isDefined( self.torsoDmg4 ) || !isDefined( self.legDmg1 ) )
			return undefined;
		limb[ "body_model" ] = self.torsoDmg4;
		limb[ "legs_model" ] = self.legDmg1;
		// T5 :1665: "our guts torso is missing left arm".
		pieces[ 0 ] = 2;
		break;

	default:
		return undefined;
	}

	limb[ "pieces" ] = pieces;
	return limb;
}

gib_piece_model( piece )
{
	switch ( piece )
	{
	case 1: return self.gibSpawn1;
	case 2: return self.gibSpawn2;
	case 3: return self.gibSpawn3;
	case 4: return self.gibSpawn4;
	}
	return undefined;
}

gib_piece_tag( piece )
{
	switch ( piece )
	{
	case 1: return self.gibSpawnTag1;
	case 2: return self.gibSpawnTag2;
	case 3: return self.gibSpawnTag3;
	case 4: return self.gibSpawnTag4;
	}
	return undefined;
}

// T5 :1700 throw_gib -> the native gib(); see the header for why this is a
// server-simulated script_model here.
throw_gib( pieces )
{
	if ( !isDefined( pieces ) )
		return;

	// T5 :1207 gib_vel: along the shot, and up.
	push = ( 0, 0, 0 );
	if ( isDefined( self.zm_gib_damage_dir ) )
		push = self.zm_gib_damage_dir;
	else if ( isDefined( self.attacker ) )
		push = vectorNormalize( self.origin - self.attacker.origin );

	for ( i = 0; i < pieces.size; i++ )
	{
		model = self gib_piece_model( pieces[ i ] );
		tag = self gib_piece_tag( pieces[ i ] );
		if ( !isDefined( model ) || !isDefined( tag ) )
			continue;

		origin = self getTagOrigin( tag );
		angles = self getTagAngles( tag );

		if ( frame_budget( "gib_fx", 6 ) )
			playFx( level._effect[ "zombie_gib" ], origin );

		launch_gib_piece( model, origin, angles, push );
	}
}

launch_gib_piece( model, origin, angles, push )
{
	// Oldest piece goes first when the cap is reached.
	live = [];
	for ( i = 0; i < level.zm_gib_pieces.size; i++ )
	{
		if ( isDefined( level.zm_gib_pieces[ i ] ) )
			live[ live.size ] = level.zm_gib_pieces[ i ];
	}
	while ( live.size >= level.zm_gib_piece_max )
	{
		live[ 0 ] delete();
		rest = [];
		for ( i = 1; i < live.size; i++ )
			rest[ rest.size ] = live[ i ];
		live = rest;
	}

	piece = spawn( "script_model", origin );
	piece setModel( model );
	piece.angles = angles;
	piece notSolid();
	piece.targetname = "zombie_gib_piece";

	live[ live.size ] = piece;
	level.zm_gib_pieces = live;

	// physicsLaunchServer takes an impulse; a gib model carries no
	// PhysPreset (codol_checkmodel: physics reference 0), so the body has
	// the server's default mass of 10 and the impulse is velocity * 10.
	speed = randomFloatRange( 120, 220 );
	velocity = ( push[ 0 ] * speed, push[ 1 ] * speed, randomFloatRange( 140, 240 ) );
	contact = origin + ( randomFloatRange( -2, 2 ), randomFloatRange( -2, 2 ), randomFloatRange( -2, 2 ) );
	piece physicsLaunchServer( contact, velocity * 10 );

	piece thread gib_piece_expire( level.zm_gib_piece_life );
}

gib_piece_expire( life )
{
	self endon( "death" );

	wait( life );

	if ( isDefined( self ) )
		self delete();
}

// Per-frame caps on presentation work, keyed by kind.
frame_budget( key, max )
{
	now = getTime();
	if ( !isDefined( level.zm_fx_budget_time ) || level.zm_fx_budget_time != now )
	{
		level.zm_fx_budget_time = now;
		level.zm_fx_budget = [];
	}

	if ( !isDefined( level.zm_fx_budget[ key ] ) )
		level.zm_fx_budget[ key ] = 0;

	if ( level.zm_fx_budget[ key ] >= max )
		return false;

	level.zm_fx_budget[ key ]++;
	return true;
}
