// sh1x: the SP arm's percent-sign animation reference is a compile-time animtree
// reference. The compiler registers it whether or not the branch runs, so
// the animtree load then demands an xanim that only the SP zones carry and
// the map dies (mp_storm_spring / foliage_tree_oak_1_sway). MP registers the
// animation by name, which PrecacheMpAnim/ScriptModelPlayAnim resolve inside
// the zone actually mounted, so the name form is the only one kept here.
#include common_scripts\utility;

main()
{
	if( !isdefined ( level.anim_prop_models ) )
		level.anim_prop_models = [];

	model = "hanging_short_sleeve";
	level.anim_prop_models[ model ][ "wind_medium" ] = "hanging_clothes_short_sleeve_wind_medium";
}
