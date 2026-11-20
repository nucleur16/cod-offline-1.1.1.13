/* Constant model and effect rows replaced by the asset the match's zones ship.
 *
 * mp/constConfigStrings_zm.csv (code_post_gfx_mp) was captured with T5-era
 * names: the Sleight of Hand machine (zombie_vending_sleight) and the
 * original mystery box (codo_treasure_chest_box, _lid). No zm zone carries
 * those xmodels (docs/zone_assets: only the sleight machine's surfaces and
 * materials are in patch_common_gametype_zm). Every zm map ships
 * zombie_vending_speedcola -- the Speed Cola machine is the Sleight of Hand
 * perk's machine -- and the box CoDOL actually uses,
 * codo_treasure_chest_box2 / codo_treasure_chest_box2_lid, whose lid the
 * zm_magic_box xanims (codo_treasure_chest_box_open/_close) animate.
 *
 * The same table's rows 1441..1565 (models) and 3709..3789 (effects) are
 * zm_prototype_sh's own map assets: its destructibles, power lever, TVs and
 * createfx effects. C1's CG_Init registers every row on every zm map, so on
 * zm_asylum_sh (whose zone ships none of them) each became C1's default
 * model or effect (Kernel33 parity log, 2026-10-13 22:23, 80 asset lines).
 * Where zm_asylum_sh's zone set ships the same asset under another name --
 * a T5 destructibles/ effect for the IW props/ one, the _asylum moon, the
 * un-prefixed god ray, another variant of the same TV, cabinet or lever --
 * that row is published as the shipped name. Every row applies only on a map
 * whose zones lack the constant and carry the replacement, so zm_prototype_sh
 * (which ships the constants) is untouched.
 *
 * Constants with no equivalent in the zm_asylum_sh zone set, left as they
 * are (the client draws nothing at those slots on that map, as C1 does):
 * the coupe destructible and its parts (vehicle_coupe_white_*,
 * codo_vehicle_coupe_gray_*, vehicle_coupe_black_destroyed,
 * vehicle_coupe_wheel_lf), com_barrel_benzin, com_barrel_piece(2),
 * codo_vehicle_mack_truck_short_white_destroy, me_chainlink_fence_pole,
 * codo_metal_equipment(_new), com_pipe_4x256_gas, and the effects
 * smoke/skybox_smoke, smoke/smoke_plume01, env/smoke/fx_battlefield_smokebank_*,
 * env/fire/fx_fire_wall_smk_0x100y155z, fire/bigcity_destroyed_building_*,
 * fire/electrical_transformer_blacksmoke_fire_*, fire/field_fire_distant*,
 * fire/emb_burst_b, env/water/fx_wtr_pipe_spill_*, env/water/fx_water_wake_flow_*,
 * env/water/fx_water_splash_leak_md, maps/ber2/fx_debris_*,
 * env/fire/fx_fire_bookshelf_wide, env/fire/fx_fire_ceiling_*,
 * maps/zombie/fx_zombie_lamp_glow_amber, smoke/car_damage_whitesmoke,
 * smoke/car_damage_blacksmoke_fire, props/car_glass_headlight,
 * impacts/pipe_fire, misc/aircraft_light_blue_blink.
 *
 * Coop constants with no equivalent in the coop_take_mp_fav_tropical zone set
 * (common_gametype_coop, coop_take_mp_fav_tropical, mp_fav_tropical and their
 * patches), left as they are: weapon_minigun, weapon_mg4_codo, weapon_m79,
 * weapon_c4_detonator, WEAPON_SPAS12, weapon_colt_anaconda,
 * weapon_winchest1200, weapon_benelli_super_90, weapon_magpul_masada_dust,
 * weapon_magpul_masada_clip, prop_suitcase_bomb, basketball,
 * characters_accessories_pencil, machinery_welder_handle, vehicle_blackhawk,
 * vehicle_hummer_viewmodel, vehicle_little_bird_minigun_left/_right; the
 * training cast: body_complete_sp_lw, head_complete_sp_lw,
 * body_complete_sp_sas_ct_* (benjamin, charles, mitchel, neal, william),
 * body_sp_sas_woodland_assault_a/_b, body_sp_sas_woodland_support_a,
 * head_sp_sas_woodland_* (hugh, zied, peter, mac, todd), the C1 instructors
 * (body_ustshirt_tf_m(_clean_yel/_clean_blk), body_codotshirt_tf_m,
 * body_bdurolled_tf_m, body_uniform_tf_m, body_complete_tf_tan,
 * head_complete_tf_tan, hea_hernandez_tf_bdurolled/_clean,
 * hea_veiga_tf_uniform/_clean, hea_hjelmaa_tf_codotshirt),
 * body_seal_soccom_assault_a/_c_blk, head_seal_soccom_d, body_tf141_assault_a,
 * body_desert_tf141_assault_a, body_forest_tf141_shotgun/_smg,
 * head_tf141_forest_a.._d, body_hero_soap_socom, head_hero_soap_soccom_a,
 * head_hero_price_desert, head_hero_russian_president, body_vil_shepherd; and
 * the effects misc/aircraft_light_cockpit_blue, weather/fog_river_200,
 * misc/insect_trail_runner, dust/amb_dust_hangar,
 * dust/light_shaft_dust_large/_med, misc/welding_runner,
 * impacts/ball_bounce_dust_runner. The vehicle rows (blackhawk, humvee) and
 * the training HUD materials (hud_arrow_down, map_training_lookup_arrow,
 * map_training_lookdown_arrow) have no favela equivalent either, and this
 * table substitutes only models and effects.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "const_models.h"
#include <string.h>

enum { CONST_MODEL, CONST_EFFECT };

static const struct {
    const char *zone_type;
    int kind;
    const char *from, *to;
} substitutes[] = {
    { "zm", CONST_MODEL, "zombie_vending_sleight", "zombie_vending_speedcola" },
    { "zm", CONST_MODEL, "codo_treasure_chest_box", "codo_treasure_chest_box2" },
    { "zm", CONST_MODEL, "codo_treasure_chest_box_lid", "codo_treasure_chest_box2_lid" },
    /* zm_prototype_sh's map models, as zm_asylum_sh ships them. */
    { "zm", CONST_MODEL, "zombie_power_lever_handle_arm01", "zombie_power_lever_handle_arm02" },
    { "zm", CONST_MODEL, "codo_vehicle_pickup_technical_destroyed", "vehicle_pickup_technical_destroyed" },
    { "zm", CONST_MODEL, "codo_ma_flatscreen_tv_wallmount_02", "codo_ma_flatscreen_tv_wallmount_04" },
    { "zm", CONST_MODEL, "codo_ma_flatscreen_tv_wallmount_03", "codo_ma_flatscreen_tv_wallmount_04" },
    { "zm", CONST_MODEL, "codo_ma_flatscreen_tv_wallmount_02_off", "codo_ma_flatscreen_tv_wallmount_04" },
    { "zm", CONST_MODEL, "codo_ma_flatscreen_tv_wallmount_03_off", "codo_ma_flatscreen_tv_wallmount_04" },
    { "zm", CONST_MODEL, "codo_ma_flatscreen_tv_wallmount_broken_02", "codo_ma_flatscreen_tv_wallmount_broken_04" },
    { "zm", CONST_MODEL, "codo_ma_flatscreen_tv_wallmount_broken_03", "codo_ma_flatscreen_tv_wallmount_broken_04" },
    { "zm", CONST_MODEL, "codo_static_peleliu_filecabinet_metal_destoryed02",
      "codo_static_peleliu_filecabinet_metal_destoryed01" },
    /* zm_prototype_sh's effects: the IW props/ barrel effects are the T5
     * destructibles/ ones, the moon is the map's own variant, the numbered
     * and codo_ copies of the ceiling ray / fog / god ray / drips / small
     * static fire are the base effect every zm map ships. */
    { "zm", CONST_EFFECT, "props/barrelexp", "destructibles/fx_barrelexp" },
    { "zm", CONST_EFFECT, "props/barrel_ignite", "destructibles/fx_barrel_ignite" },
    { "zm", CONST_EFFECT, "props/barrel_fire_top", "destructibles/fx_barrel_fire_top" },
    { "zm", CONST_EFFECT, "maps/zombie/fx_zomb_moon_proto", "maps/zombie/fx_zomb_moon_proto_asylum" },
    { "zm", CONST_EFFECT, "maps/zombie/fx_dust_ceiling_impact_lg_mdbrown",
      "env/dirt/fx_dust_ceiling_impact_lg_mdbrown" },
    { "zm", CONST_EFFECT, "env/light/codo_fx_ray_ceiling_amber_dim_sm", "env/light/fx_ray_ceiling_amber_dim_sm" },
    { "zm", CONST_EFFECT, "env/light/codo_fx_ray_ceiling_amber_dim_sm01", "env/light/fx_ray_ceiling_amber_dim_sm" },
    { "zm", CONST_EFFECT, "env/light/codo_fx_ray_ceiling_amber_dim_sm02", "env/light/fx_ray_ceiling_amber_dim_sm" },
    { "zm", CONST_EFFECT, "env/light/codo_fx_ray_ceiling_amber_dim_sm03", "env/light/fx_ray_ceiling_amber_dim_sm" },
    { "zm", CONST_EFFECT, "env/light/codo_fx_ray_ceiling_amber_dim_sm04", "env/light/fx_ray_ceiling_amber_dim_sm" },
    { "zm", CONST_EFFECT, "maps/zombie/codo_fx_fog_zombie_thick_600x600_01",
      "maps/zombie/codo_fx_fog_zombie_thick_600x600" },
    { "zm", CONST_EFFECT, "maps/zombie/codo_fx_fog_zombie_thick_600x600_03",
      "maps/zombie/codo_fx_fog_zombie_thick_600x600" },
    { "zm", CONST_EFFECT, "env/light/fx_light_god_ray_sm_single_sh", "env/light/fx_light_god_ray_sm_single" },
    { "zm", CONST_EFFECT, "env/light/fx_light_god_ray_sm_shrt_single", "env/light/fx_light_god_ray_sm_single" },
    { "zm", CONST_EFFECT, "env/water/fx_water_drips_hvy_long", "env/water/fx_water_drips_hvy" },
    { "zm", CONST_EFFECT, "env/fire/fx_static_fire_sm_ndlight01", "env/fire/fx_static_fire_sm_ndlight" },
    { "zm", CONST_EFFECT, "props/car_glass_large", "props/car_glass_med" },

    /* coop: mp/constConfigStrings_coop.csv rows 1960..2087 (models) and
     * 3652..3694 (effects) are coop_training_mp_killhouse_sh's own assets
     * -- the killhouse instructors, the SAS/TF141 cast, the SP weapons on
     * the tables, the blackhawk/little bird/humvee and their effects --
     * captured from that zone. Even there three of them are in no zone: the
     * map's spawners name body_seal_soccom_assault_a and body_tf141_assault_a,
     * which the zone ships as the _c_blk / desert variants, and the benelli
     * is C1's m1014 (Kernel33 parity log, 2026-10-13 23:33, CS 1998, 2011,
     * 2057). On coop_take_mp_fav_tropical 68 model and 15 effect rows became
     * C1's default (23:39); where the favela zone set ships the same asset
     * under its own name -- the IW weapon world model for the codo_ one, the
     * plain model for the _obj glow copy, Ghost in socom gear for Ghost in
     * forest gear, the left door handle for the right, the aerial explosion
     * for the helicopter's, the solid red light for the cockpit's -- the row
     * is published as that. */
    { "coop", CONST_MODEL, "body_seal_soccom_assault_a", "body_seal_soccom_assault_c_blk" },
    { "coop", CONST_MODEL, "body_tf141_assault_a", "body_desert_tf141_assault_a" },
    { "coop", CONST_MODEL, "weapon_benelli_super_90", "weapon_m1014_codo" },
    { "coop", CONST_MODEL, "codo_weapon_m4a1tech_world", "weapon_m4" },
    { "coop", CONST_MODEL, "codo_weapon_cheytac_world", "weapon_cheytac" },
    { "coop", CONST_MODEL, "weapon_usp_codo", "weapon_usp" },
    { "coop", CONST_MODEL, "weapon_m4gre_sp_obj", "weapon_m16_gl" },
    { "coop", CONST_MODEL, "weapon_m67_grenade_obj", "weapon_m67_grenade" },
    { "coop", CONST_MODEL, "body_forest_tf141_ghost", "body_hero_ghost_socom" },
    { "coop", CONST_MODEL, "head_hero_ghost_forest", "head_hero_ghost_soccom" },
    { "coop", CONST_MODEL, "com_door_01_handleright", "com_door_01_handleleft" },
    { "coop", CONST_MODEL, "open_book_static", "open_book" },
    { "coop", CONST_EFFECT, "explosions/helicopter_explosion", "explosions/aerial_explosion_large" },
    { "coop", CONST_EFFECT, "explosions/aerial_explosion_heli_large", "explosions/aerial_explosion_large" },
    { "coop", CONST_EFFECT, "explosions/helicopter_explosion_little_bird", "explosions/aerial_explosion" },
    { "coop", CONST_EFFECT, "explosions/aerial_explosion_littlebird", "explosions/aerial_explosion" },
    { "coop", CONST_EFFECT, "misc/aircraft_light_cockpit_red", "misc/aircraft_light_red_solid" },
    { "coop", CONST_EFFECT, "explosions/vehicle_explosion_hummer_nodoors",
      "explosions/vehicle_explosion_pickuptruck" },
    { "coop", CONST_EFFECT, "impacts/footstep_mud_dark", "impacts/footstep_mud" },
};

/* register_inherited_name (configstrings.c) asks this whether a slot
 * whose constant is `base` and whose published value is `changed` should
 * still answer a lookup by `base`: a script that loads the constant's name on
 * a map where it was substituted then reuses the slot, and the client draws
 * the shipped asset instead of a second, missing registration. */
extern int (*const_alias_hook)(unsigned first, const char *base, const char *changed);
static int const_alias(unsigned first, const char *base, const char *changed)
{
    unsigned row;
    int kind = first == OL_CS_MODELS ? CONST_MODEL : first == CS_EFFECTS ? CONST_EFFECT : -1;
    if (kind < 0 || !base || !changed) return 0;
    for (row = 0; row < sizeof(substitutes) / sizeof(substitutes[0]); ++row)
        if (substitutes[row].kind == kind && !strcmp(substitutes[row].from, base) &&
            !strcmp(substitutes[row].to, changed)) return 1;
    return 0;
}

static int substitute(int kind, configstrings_t *overrides,
    const configstrings_t *inherited, const char *zone_type,
    model_available_fn available, void *context,
    const_model_report_fn report)
{
    unsigned row, slot, changed = 0;
    unsigned first = kind == CONST_MODEL ? OL_CS_MODELS : CS_EFFECTS;
    unsigned count = kind == CONST_MODEL ? MODEL_SLOTS : EFFECT_SLOTS;
    if (!overrides || !inherited || overrides == inherited || !zone_type || !available)
        return -1;
    const_alias_hook = const_alias;
    for (row = 0; row < sizeof(substitutes) / sizeof(substitutes[0]); ++row) {
        if (substitutes[row].kind != kind || strcmp(substitutes[row].zone_type, zone_type)) continue;
        for (slot = 1; slot < count; ++slot) {
            unsigned index = first + slot;
            const char *base = GetConfigstring(inherited, index);
            if (!base || strcmp(base, substitutes[row].from)) continue;
            if (GetConfigstring(overrides, index)) continue;
            if (available(substitutes[row].from, context) ||
                !available(substitutes[row].to, context)) continue;
            if (!OL_SetConfigstring(overrides, index, substitutes[row].to)) return -1;
            if (report) report(index, substitutes[row].from, substitutes[row].to, context);
            ++changed;
        }
    }
    return (int)changed;
}

int SubstituteConstModels(configstrings_t *overrides,
    const configstrings_t *inherited, const char *zone_type,
    model_available_fn available, void *context,
    const_model_report_fn report)
{
    return substitute(CONST_MODEL, overrides, inherited, zone_type, available, context, report);
}

int SubstituteConstEffects(configstrings_t *overrides,
    const configstrings_t *inherited, const char *zone_type,
    model_available_fn available, void *context,
    const_model_report_fn report)
{
    return substitute(CONST_EFFECT, overrides, inherited, zone_type, available, context, report);
}
