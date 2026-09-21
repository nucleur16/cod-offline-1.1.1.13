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
}

precache()
{
	bodies = zombie_bodies();
	heads = zombie_heads();
	for ( i = 0; i < bodies.size; i++ )
		precacheModel( bodies[ i ] );
	for ( i = 0; i < heads.size; i++ )
		precacheModel( heads[ i ] );
}
