// PROVENANCE (2026-10-22): CoDOL 3.5.6.2 (mp_dome, ported to 1.1.1.13). The map's animated_model
// entities name this script in their precache_script key ("maps animated_models windmill_spin_med");
// 3.5.6.2 ships no source for it, so it follows this tree's maps/animated_models/*.gsc (IW4/IW5
// layout, MP name form: PrecacheMpAnim and ScriptModelPlayAnim take the xanim name, which the
// map zone ships and animtrees/animated_props.atr lists in its 3.5.6.2 block).
#include common_scripts\utility;

main()
{
	if( !isdefined ( level.anim_prop_models ) )
		level.anim_prop_models = [];

	model = "machinery_windmill";
	level.anim_prop_models[ model ][ "spin" ] = "windmill_spin_med";
}
