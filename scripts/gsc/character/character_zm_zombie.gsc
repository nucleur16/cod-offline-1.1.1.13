// zm enemy zombie body.
//
// CoDOL ships NO "character" xasset type (docs/zone_assets/COUNTS.tsv has no
// character/aitype/mptype/xmodelalias column), so a character script here is
// an ordinary GSC module that names raw xmodels, exactly as the dog's
// character/character_mp_german_shepherd.gsc does.
//
// MEASURED 2026-09-20, docs/zone_assets/ALL_xmodel.txt: the enemy zombie
// bodies are five c_zom_* families, each shipped identically in all three zm
// MAP zones (zm_asylum_sh, zm_prototype_sh, zm_zombie_training) and in none of
// the common_* zones:
//
//     c_zom_ger_body1            + c_zom_ger_head            + 17 gib models
//     c_zom_garbageman_body      + c_zom_garbageman_head     + 17 gib models
//     c_zom_worker_body          + c_zom_worker_head         + 17 gib models
//     c_zom_secunityguard_body   + c_zom_secunityguard_head  + 17 gib models
//     c_zom_juggernaut_body1     + c_zom_juggernaut_head     (no gib set)
//
// They are CoDOL-original assets that follow Treyarch's naming scheme: of the
// 1,112 model names quoted by T5/BO1 character scripts, only four exist in
// CoDOL and none of them is a humanoid zombie body. The gib suffixes differ
// from T5's too (_behead / _larmoff_1, not T5's _g_behead / _g_rarmspawn).
//
// The fja_zm* / han_zm* / hea_zm* models in common_gametype_zm are PLAYER
// customization parts (slot_outfit_faction_gender_variant, with _vmchar
// viewmodel variants) and are never an enemy body.
//
// c_zom_juggernaut_body1 is deliberately not in the rotation: it has no gib
// set, which is how the art marks a boss rather than a round zombie.

// array() is not a registered builtin on this server (the script gate rejects
// it), so the lists are built element by element.
zombie_bodies()
{
	bodies = [];
	bodies[ 0 ] = "c_zom_ger_body1";
	bodies[ 1 ] = "c_zom_garbageman_body";
	bodies[ 2 ] = "c_zom_worker_body";
	bodies[ 3 ] = "c_zom_secunityguard_body";
	return bodies;
}

zombie_heads()
{
	heads = [];
	heads[ 0 ] = "c_zom_ger_head";
	heads[ 1 ] = "c_zom_garbageman_head";
	heads[ 2 ] = "c_zom_worker_head";
	heads[ 3 ] = "c_zom_secunityguard_head";
	return heads;
}

// The dismemberment set, T5's character-script fields (torsoDmg1-5,
// legDmg1-4, gibSpawn1-4 + gibSpawnTag1-4, e.g. T5
// character/c_zom_barechest.gsc), mapped onto CoDOL's own suffixes.
//
// MEASURED 2026-10-05 with codol_checkmodel on a zm_prototype_sh probe (the
// c_zom_ger_body_* set; the other three families ship the same 17 suffixes,
// docs/zone_assets/ALL_xmodel.txt):
//   _upclean, _rarmoff_1/2, _larmoff_1/2, _torso_1  73 bones -- the full
//       skeleton, i.e. the UPPER body a T5 torsoDmg model is (setModel)
//   _behead       6 bones, tag_origin..j_neck -- T5's torsoDmg5 neck stump
//                 (attached in place of the head)
//   _lowclean    12 bones, hips/knees/ankles -- T5 legDmg1 (attached)
//   _rlegoff_1/2, _llegoff_1/2  16 bones -- T5 legDmg2/legDmg3
//   _legsoff_1   10 bones, hips/knees only -- T5 legDmg4
//   _rarmspawn, _larmspawn, _rlegspawn, _llegspawn  2 bones, tag_origin and
//       tag_fx -- the flying limb pieces (T5 gibSpawn1-4)
// T5's spawn tags (J_Elbow_RI, J_Elbow_LE, J_Knee_RI, J_Knee_LE) are bones of
// every c_zom_*_body (lower-case here, as the bone dump prints them).
//
// CoDOL ships two cuts of each single-limb model (_1 and _2) where T5 ships
// one; nothing in the shipped data says which one a given wound uses, so
// either is valid and a zombie picks one at spawn for variety.
zombie_gib_prefixes()
{
	prefixes = [];
	prefixes[ 0 ] = "c_zom_ger_body_";
	prefixes[ 1 ] = "c_zom_garbageman_body_";
	prefixes[ 2 ] = "c_zom_worker_body_";
	prefixes[ 3 ] = "c_zom_secunityguard_body_";
	return prefixes;
}

zombie_gib_suffixes()
{
	suffixes = [];
	suffixes[ suffixes.size ] = "upclean";
	suffixes[ suffixes.size ] = "rarmoff_1";
	suffixes[ suffixes.size ] = "rarmoff_2";
	suffixes[ suffixes.size ] = "larmoff_1";
	suffixes[ suffixes.size ] = "larmoff_2";
	suffixes[ suffixes.size ] = "torso_1";
	suffixes[ suffixes.size ] = "behead";
	suffixes[ suffixes.size ] = "lowclean";
	suffixes[ suffixes.size ] = "rlegoff_1";
	suffixes[ suffixes.size ] = "rlegoff_2";
	suffixes[ suffixes.size ] = "llegoff_1";
	suffixes[ suffixes.size ] = "llegoff_2";
	suffixes[ suffixes.size ] = "legsoff_1";
	suffixes[ suffixes.size ] = "rarmspawn";
	suffixes[ suffixes.size ] = "larmspawn";
	suffixes[ suffixes.size ] = "rlegspawn";
	suffixes[ suffixes.size ] = "llegspawn";
	return suffixes;
}

// T5 field names, so animscripts\zombie_death::get_limb_data reads them
// unchanged.
set_gib_models( prefix )
{
	cut = "_" + randomIntRange( 1, 3 );

	self.torsoDmg1 = prefix + "upclean";
	self.torsoDmg2 = prefix + "rarmoff" + cut;
	self.torsoDmg3 = prefix + "larmoff" + cut;
	self.torsoDmg4 = prefix + "torso_1";
	self.torsoDmg5 = prefix + "behead";
	self.legDmg1 = prefix + "lowclean";
	self.legDmg2 = prefix + "rlegoff" + cut;
	self.legDmg3 = prefix + "llegoff" + cut;
	self.legDmg4 = prefix + "legsoff_1";
	self.gibSpawn1 = prefix + "rarmspawn";
	self.gibSpawnTag1 = "j_elbow_ri";
	self.gibSpawn2 = prefix + "larmspawn";
	self.gibSpawnTag2 = "j_elbow_le";
	self.gibSpawn3 = prefix + "rlegspawn";
	self.gibSpawnTag3 = "j_knee_ri";
	self.gibSpawn4 = prefix + "llegspawn";
	self.gibSpawnTag4 = "j_knee_le";
}

main()
{
	bodies = zombie_bodies();
	index = randomint( bodies.size );

	self setModel( bodies[ index ] );
	heads = zombie_heads();

	// MEASURED, and the root cause of the live
	// "tag 'tag_eye' does not exist on entity with model 'c_zom_ger_body1'"
	// error: a bone dump of the shipped zm fastfiles shows every c_zom_*_body
	// model carries 73 bones with NO tag_eye, while every c_zom_*_head carries
	// 29 bones that DO include tag_eye (and j_eyeball_le/ri).  Setting
	// .headModel alone is inert bookkeeping -- the engine never reads that
	// field (zero hits for "headmodel" across src/**).  attach() is what puts
	// the head's bones into the entity's DObj, and it is registered
	// (src/scr_vm_main.c:739).  This mirrors the shipped stock helper
	// codescripts/character.gsc:32-33.  Without it the zombies were being
	// spawned with no head model at all.
	self attach( heads[ index ], "", true );
	self.headModel = heads[ index ];
	self.voice = "zombie";
	self.skeleton = "base";

	prefixes = zombie_gib_prefixes();
	self set_gib_models( prefixes[ index ] );
}

precache()
{
	bodies = zombie_bodies();
	heads = zombie_heads();
	for ( i = 0; i < bodies.size; i++ )
		precacheModel( bodies[ i ] );
	for ( i = 0; i < heads.size; i++ )
		precacheModel( heads[ i ] );

	prefixes = zombie_gib_prefixes();
	suffixes = zombie_gib_suffixes();
	for ( i = 0; i < prefixes.size; i++ )
	{
		for ( j = 0; j < suffixes.size; j++ )
			precacheModel( prefixes[ i ] + suffixes[ j ] );
	}
}
