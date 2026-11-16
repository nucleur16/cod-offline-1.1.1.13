// PROVENANCE (2026-10-14): CoDOL mp_abandon_sh_mk. Same shape as IW4's
// common_scripts/_destructible_types_anim_*.gsc precache modules (e.g.
// _destructible_types_anim_generator.gsc): it fills level._destructible_preanims
// for _destructible_types::get_precached_anim. 3.3.3.3 keeps the firecracker
// anims in the animated_props tree (its animtrees/animated_props.atr), not in
// destructibles, so this module uses that tree. The xanims ship in the
// mp_abandon_sh_mk zone. Called from maps/mp/mp_abandon_sh_mk_precache.gsc.
#using_animtree( "animated_props" );
main()
{
	level._destructible_preanims[ "mp_abandon_firecracker_bottom_fire" ] = %mp_abandon_firecracker_bottom_fire;
	level._destructible_preanims[ "mp_abandon_firecracker_top_fire" ] = %mp_abandon_firecracker_top_fire;
}
