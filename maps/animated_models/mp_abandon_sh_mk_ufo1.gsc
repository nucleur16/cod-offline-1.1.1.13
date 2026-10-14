// PROVENANCE (2026-10-14): CoDOL mp_abandon_sh_mk. The map's animated_model
// entities name this script in their precache_script key ("maps animated_models
// mp_abandon_sh_mk_ufo1"); 3.3.3.3 ships no source for it, so it follows this tree's
// maps/animated_models/*.gsc (IW4 layout, MP name form: PrecacheMpAnim and
// ScriptModelPlayAnim take the xanim name, which the mp_abandon_sh_mk zone ships
// and the animated_props tree lists).
// Both UFOs share abdn_ufo_anim; mp_abandon_sh_mk.gsc pins each one to its own
// loop through the entity's precache_script key.
#include common_scripts\utility;

main()
{
	if( !isdefined ( level.anim_prop_models ) )
		level.anim_prop_models = [];

	model = "abdn_ufo_anim";
	level.anim_prop_models[ model ][ "ufo1" ] = "mp_abandon_mk_ufo1_loop";
}
