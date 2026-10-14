// PROVENANCE (2026-10-14): CoDOL mp_mideast (3.3.3.3 map ported to 1.1.1.13). The map's
// animated_model entities (qad_lamp_wall_01) name this script in their precache_script key
// ("maps animated_models qad_lamp_wall_01"); 3.3.3.3 ships no source for it, so it follows
// this tree's maps/animated_models/*.gsc (IW4 layout, MP name form: PrecacheMpAnim and
// ScriptModelPlayAnim take the xanim name). mp_mid_lamp_wall_idle ships in the mp_mideast zone and
// its animlists/mp_mideast.animlist lists it under =animated_props.
#include common_scripts\utility;

main()
{
	if( !isdefined ( level.anim_prop_models ) )
		level.anim_prop_models = [];

	model = "qad_lamp_wall_01";
	level.anim_prop_models[ model ][ "idle" ] = "mp_mid_lamp_wall_idle";
}
