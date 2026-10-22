// PROVENANCE (2026-10-22): CoDOL mp_abandon_sh (2.6.9.7 port). Same role as IW4
// maps/mp/mp_abandon_precache.gsc: one call per animated_models script the map's
// entities name in their precache_script keys (the 2.6.9.7 MapEnts: 4 dragon
// windsocks, 3 banners, the ferris wheel and one river birch). The xanims are in the
// zone's animlist and in animtrees/animated_props.atr.
main()
{
	maps\animated_models\mp_abandon_banner_idle::main();
	maps\animated_models\mp_abandon_dragon_windsock_idle::main();
	maps\animated_models\mp_abandon_ferris_wheel::main();
	maps\animated_models\foliage_tree_river_birch_xl_a::main();
}
