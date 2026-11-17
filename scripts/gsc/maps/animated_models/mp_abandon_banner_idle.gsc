// PROVENANCE (2026-10-14): CoDOL mp_abandon_sh_mk. The map's animated_model
// entities name this script in their precache_script key ("maps animated_models
// mp_abandon_banner_idle"); 3.3.3.3 ships no source for it, so it follows this tree's
// maps/animated_models/*.gsc (IW4 layout, MP name form: PrecacheMpAnim and
// ScriptModelPlayAnim take the xanim name, which the mp_abandon_sh_mk zone ships
// and the animated_props tree lists).
#include common_scripts\utility;

main()
{
	if( !isdefined ( level.anim_prop_models ) )
		level.anim_prop_models = [];

	model = "abdn_new_year_banner_01";
	level.anim_prop_models[ model ][ "idle" ] = "mp_abandon_banner_idle";
}
