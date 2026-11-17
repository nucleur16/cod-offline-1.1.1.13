// character\character_so_survival -- the Spec Ops Survival enemies' looks.
//
// IW5's survival aitypes (aitype\enemy_so_*.gsc) wear MW3 characters
// (character\character_so_regular_smg, _so_juggernaut_mid, ...) that neither
// C1 nor this server has.  CoDOL's survival builds dressed the same classnames
// in their own models; each spawner of its addons names its body in "model"
// (2.6.9.7 coop_survival_mp_fav_tropical), and its common_gametype_survival
// ships the matching heads.  docs/CODOL_SURVIVAL_PLAN.md section 2.3 lists
// them.  Those models come to 1.1.1.13 in the survival zones (phase 4); until a
// model is mounted the look falls back to one a coop match always mounts
// (codescripts\character keeps unmounted names away from setModel/attach).

look( kind )
{
	l = spawnStruct();
	l.bodies = [];
	l.heads = [];
	l.extra = undefined;
	switch ( kind )
	{
	case "easy":
		l.bodies[ 0 ] = "body_opforce_arab_shotgun_a";
		l.heads = arab_heads();
		break;
	case "regular":
		l.bodies[ 0 ] = "body_work_civ_male_a";
		l.bodies[ 1 ] = "body_work_civ_male_b";
		l.bodies[ 2 ] = "body_work_civ_male_c";
		l.heads = civ_heads();
		break;
	case "hardened":
		l.bodies[ 0 ] = "body_airborne_assault_a";
		l.bodies[ 1 ] = "body_airborne_smg";
		l.bodies[ 2 ] = "body_airborne_shotgun";
		l.heads = composite_heads();
		break;
	case "veteran":
		l.bodies[ 0 ] = "body_shadow_co_assault";
		l.heads[ 0 ] = "head_shadow_co_b";
		l.heads[ 1 ] = "head_shadow_co_c";
		l.heads[ 2 ] = "head_shadow_co_d";
		break;
	case "elite":
	case "riotshield":
		l.bodies[ 0 ] = "body_opforce_fsb_assault_a";
		l.bodies[ 1 ] = "body_opforce_fsb_smg";
		l.heads[ 0 ] = "head_opforce_fsb_a";
		l.heads[ 1 ] = "head_opforce_fsb_b";
		break;
	case "scrambler":
		l.bodies[ 0 ] = "body_opforce_fsb_assault_a_red";
		l.heads[ 0 ] = "head_opforce_fsb_a_red";
		break;
	case "martyrdom":
		l.bodies[ 0 ] = "body_work_civ_male_a_bomb";
		l.bodies[ 1 ] = "body_work_civ_male_b_bomb";
		l.bodies[ 2 ] = "body_work_civ_male_c_bomb";
		l.heads = civ_heads();
		break;
	case "emp":
		l.bodies[ 0 ] = "body_work_civ_male_a_bomb_red";
		l.heads[ 0 ] = "head_work_civ_male_a_red";
		break;
	case "claymore":
	case "demolitionist":
		l.bodies[ 0 ] = "body_composite_smg";
		l.heads = composite_heads();
		break;
	case "chemical":
		// IW5 chemical_ability_attach_tank: the gas tank on his back.
		l.bodies[ 0 ] = "body_work_civ_male_a";
		l.heads[ 0 ] = "head_russian_gas_mask";
		l.extra = "gas_canisters_backpack";
		break;
	case "hazmat":
		l.bodies[ 0 ] = "c_usa_rebirth_hazmat_body_yellow";
		l.heads[ 0 ] = "c_usa_rebirth_hazmat_mask_opaque";
		break;
	case "rpg":
		l.bodies[ 0 ] = "body_opforce_merc_assault_b";
		l.heads[ 0 ] = "head_opforce_merc_b_hat";
		l.heads[ 1 ] = "head_opforce_merc_c";
		l.heads[ 2 ] = "head_opforce_merc_d";
		l.heads[ 3 ] = "head_opforce_merc_e";
		l.heads[ 4 ] = "head_opforce_merc_f";
		break;
	case "sniper":
		l.bodies[ 0 ] = "body_opforce_sniper_ghillie";
		l.bodies[ 1 ] = "body_opforce_sniper";
		l.heads[ 0 ] = "head_opforce_sniper_ghillie";
		break;
	case "noob":
		l.bodies[ 0 ] = "body_militia_assault_aa_blk";
		l.heads[ 0 ] = "head_militia_bb_blk_hat";
		l.heads[ 1 ] = "head_militia_bc_blk";
		l.heads[ 2 ] = "head_militia_bd_blk";
		break;
	case "armored":
		l.bodies[ 0 ] = "fullbody_juggernaut_c";
		break;
	case "armored_blk":
		l.bodies[ 0 ] = "fullbody_juggernaut_c_blk";
		break;
	case "jug_regular":
	case "jug_riotshield":
		l.bodies[ 0 ] = "fullbody_juggernaut_novisor_b";
		break;
	case "jug_explosive":
		l.bodies[ 0 ] = "fullbody_juggernaut_explosive_so";
		l.bodies[ 1 ] = "mp_fullbody_juggernaut_explosive_so";
		break;
	case "jug_minigun":
	default:
		break;
	}
	// The one body every coop match mounts (common_gametype_coop).
	l.fallback = "body_complete_sp_juggernaut";
	return l;
}

arab_heads()
{
	h = [];
	h[ 0 ] = "head_opforce_arab_a";
	h[ 1 ] = "head_opforce_arab_b";
	h[ 2 ] = "head_opforce_arab_c";
	h[ 3 ] = "head_opforce_arab_d_hat";
	h[ 4 ] = "head_opforce_arab_e";
	return h;
}

civ_heads()
{
	h = [];
	h[ 0 ] = "head_airport_a";
	h[ 1 ] = "head_airport_b";
	h[ 2 ] = "head_airport_c";
	return h;
}

composite_heads()
{
	h = [];
	h[ 0 ] = "head_composite_a";
	h[ 1 ] = "head_composite_b";
	return h;
}

main( kind )
{
	l = look( kind );
	bodies = codescripts\character::mountedArray( l.bodies );
	if ( !bodies.size )
	{
		self setModel( l.fallback );
		self.headModel = undefined;
		return;
	}
	self setModel( bodies[ randomInt( bodies.size ) ] );
	if ( l.heads.size )
		codescripts\character::attachHead( "alias_so_survival_" + kind, l.heads );
	else
		self.headModel = undefined;
	if ( isDefined( l.extra ) && codescripts\character::modelMounted( l.extra ) )
		self attach( l.extra, "j_spine4", true );
}

precache( kind )
{
	l = look( kind );
	precacheModel( l.fallback );
	codescripts\character::precacheModelArray( l.bodies );
	codescripts\character::precacheModelArray( l.heads );
	if ( isDefined( l.extra ) )
		codescripts\character::precacheMounted( l.extra );
}
