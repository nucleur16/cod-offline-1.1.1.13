# -*- coding: utf-8 -*-
r"""Every zone this repo carries: where the shipped file is, where it installs, and what stock data it is built on.

`origin` is where the zone was decompiled from: G = the 1.1.1.13 install ("Call of Duty Online 1.1.1.13"),
Z = codol-classic-zz, R:<commit>:<dir> = a .ff this repository used to commit (patch/, removed when the zones moved
here), new = a zone authored here (no shipped original: --decompile leaves it alone, --verify checks the build
against the md5 recorded when it was made). Where two places carry a zone, the newest build is the source.

`stock` names stock 1.1.1.13 zones (client_shipRetail\chinese) whose asset blobs this zone reuses unchanged: the
manifest names those blobs by sha1 and the build takes them from the stock zone instead of the repo.
`gfx` maps a .gfx rawfile to the stock movie it patches ("<stock zone>:<rawfile>").
`maps` declares the zone's per-usermap data, which shipping_maps.txt filters (zonekit/shipping.py).
`handwritten` maps a .gfx rawfile to the classes whose as/ file in the zone is a hand-written source (compiled into
the stored tag), not FFDec's decompile: --decompile keeps those files instead of exporting over them.
The usermap map zones themselves (mp_*.ff, zm_sumpf_sh.ff and their _load zones) are not part of this repository.
The client_shipRetail\patch zones below are source zones: they install inside the consolidated zones of zone_packs/
(zonekit/packs.py: common_extras, common_ui_mp, common_postgfx_mp, english_locale, chinese_locale), except
infected_cyborg_postgfx_mp (C#-written: its assets point into its own runtime blocks) and the mp_weapons zones (the
sh1x server mounts mp_*.ff), which install on their own.
"""

PATCH = "client_shipRetail/patch"

# Every zone renamed when "codol" was dropped from the zone names (2026-11): {name: its old name}. Installs from before
# that, and the installs a G:/Z: origin zone was decompiled from, carry the old file names (build_zones.py finds an
# origin under it; kernel33.c skips an old file once its new one is in the patch folder, patch_locale_policy.h).
LEGACY_NAME = {
    "000_party_modes": "000_codol_party_modes",  # LEGACY_ZONE
    "english": "codol_english",  # LEGACY_ZONE
    "english_font": "codol_english_font",  # LEGACY_ZONE
    "english_ui": "codol_english_ui",  # LEGACY_ZONE
    "english_ui_extra": "codol_english_ui_extra",  # LEGACY_ZONE
    "english_usermaps": "codol_english_usermaps",  # LEGACY_ZONE
    "english_zombies_base": "codol_english_zombies_base",  # LEGACY_ZONE
    "english_hud_styles": "codol_english_hud_styles",  # LEGACY_ZONE
    "chinese_ui": "codol_chinese_ui",  # LEGACY_ZONE
    "chinese_ui_extra": "codol_chinese_ui_extra",  # LEGACY_ZONE
    "chinese_hud_styles": "codol_chinese_hud_styles",  # LEGACY_ZONE
    "field_orders_postgfx_mp": "codol_field_orders_postgfx_mp",  # LEGACY_ZONE
    "infected_cyborg_postgfx_mp": "codol_infected_cyborg_postgfx_mp",  # LEGACY_ZONE
    "loading_mode_names": "codol_loading_mode_names",  # LEGACY_ZONE
    "zm_deathmachine_audio": "codol_zm_deathmachine_audio",  # LEGACY_ZONE
    "m1911_arms_diag": "codol_zm_m1911_arms_diag",  # LEGACY_ZONE
    "mp_weapons": "mp_codol_weapons",  # LEGACY_ZONE
    "mp_weapons_postgfx_mp": "mp_codol_weapons_postgfx_mp",  # LEGACY_ZONE
    "common_extras": "codol_common",  # LEGACY_ZONE
    "common_ui_mp": "codol_common_ui_mp",  # LEGACY_ZONE
    "common_postgfx_mp": "codol_common_postgfx_mp",  # LEGACY_ZONE
    "english_locale": "codol_english_locale",  # LEGACY_ZONE
    "chinese_locale": "codol_chinese_locale",  # LEGACY_ZONE
}
# Zones that installed under their own (new) name and were packed later: {name: the pack}. An install from before
# that still has <name>.ff; kernel33.c skips it once the pack is in the patch folder (patch_locale_policy.h).
PACKED_LATER = {
    "m1911_arms_diag": "common_extras",  # LEGACY_ZONE
}
CHINESE = "client_shipRetail/chinese"

FONTS = "patch_code_post_gfx_mp:scaleform/fonts_ingame.gfx"
CODO = "patch_ui_mp:scaleform/default/codo.gfx"
LOGIN = "ui_mp:scaleform/default/codologin.gfx"
HUD = "patch_common_mp:scaleform/default/codohud.gfx"
INGAME = "patch_common_mp:scaleform/default/codoingame.gfx"

ZONES = {
    # --- client_shipRetail\patch (Kernel33 loads these ahead of code_pre_gfx; first asset linked under a name wins)
    "000_party_modes": dict(origin="Z:" + PATCH, install=PATCH),
    "english": dict(origin="Z:" + PATCH, install=PATCH),
    "english_font": dict(origin="Z:" + PATCH, install=PATCH, gfx={"scaleform/fonts_ingame.gfx": FONTS}),
    # the text of the shared UI movies (voice_ui_mp, controller_ingame): their K33UI_* localize keys, in English
    # (english_ui*: loc_english 1) or Chinese (chinese_ui*: loc_english 0); Kernel33 loads one set (ui_keys.py)
    "chinese_ui": dict(origin="new", install=PATCH),
    "chinese_ui_extra": dict(origin="new", install=PATCH),
    "english_ui": dict(origin="Z:" + PATCH, install=PATCH),
    "english_ui_extra": dict(origin="Z:" + PATCH, install=PATCH),
    "english_zombies_base": dict(origin="Z:" + PATCH, install=PATCH),
    # usermaps_patch's map names and descriptions in English (its own are GBK Chinese); ahead of usermaps_patch, so
    # they win while English is on, filtered like usermaps_patch's (a map that does not ship has no strings here either)
    "english_usermaps": dict(origin="new", install=PATCH, maps=dict(localize_from="usermaps_patch")),
    "infected_cyborg_postgfx_mp": dict(origin="Z:" + PATCH, install=PATCH,
                                             stock=["code_post_gfx_mp", "patch_code_post_gfx_mp", "common_mp",
                                                    "common_gametype_zm", "patch_common_mp"]),
    "loading_mode_names": dict(origin="Z:" + PATCH, install=PATCH),
    "zm_deathmachine_audio": dict(origin="Z:" + PATCH, install=PATCH),
    # the eight upgraded M1911 xanims (zonekit/xanim_opposite_arm.py); part of common_extras, with its script strings
    "m1911_arms_diag": dict(origin="new", install=PATCH),
    # the in-match movies, shared by both languages (codol-classic's English codohud / codoingame with every text a
    # localize key, the Chinese entries here, the English ones in english_ui_extra) and the pause menu's pad
    # navigation; ahead of code_pre_gfx, so they stay loaded in a match
    "controller_ingame": dict(origin="R:3317112:patch", install=PATCH,
                              gfx={"scaleform/default/codohud.gfx": HUD, "scaleform/default/codoingame.gfx": INGAME},
                              handwritten={"scaleform/default/codohud.gfx": {"components.scoreboard_part.ScoreboardTeamPanel"},
                                           "scaleform/default/codoingame.gfx": {"MainPause",
                                                                                "components.scoreboard_part.ScoreboardTeamPanel"}}),
    # Native HUD menus reference materials/defaults initialized by code_post_gfx. Its menufile also carries the
    # HUD styles' items for killstreaks_hd and scorebar_hd (one copy of a menu: tools/build_survival_hud.py styled()).
    "survival_hud_postgfx_mp": dict(origin="new", install=PATCH),
    # The selectable HUD styles (cg_hudStyle, hud_style.c): later builds' native HUD items inside C1's weaponbar_hd
    # and minimap_fullscreen, each gated on the dvar, with the `2d` materials and A8R8G8B8 images they draw
    # (k33hud<NN>_*). tools/build_hud_styles.py writes it from sources/; docs/HUD_STYLES_20261112.md.
    "hud_styles_postgfx_mp": dict(origin="new", install=PATCH),
    # the "@KEY" texts of those items that 1.1.1.13 has no localize entry for, per language
    "english_hud_styles": dict(origin="new", install=PATCH),
    "chinese_hud_styles": dict(origin="new", install=PATCH),
    # the usermaps' front-end data, filtered by shipping_maps.txt (zonekit/shipping.py): only "ship" maps' rows,
    # arena entries, strings, vision files and map-select images are in a shipping build
    "usermaps_patch": dict(origin="G:" + PATCH, install=PATCH, older="Z:" + PATCH,
                           # stock_edits: every zombies map offers endless (Asylum and Prototype first, then the Der Riese, Shi No Numa and Kino backports) (column 23; Cam's
                           # codol_zombies_endless zone, folded in here: two zones with mp/comaplist.csv replace each other)
                           maps=dict(table=("mp/comaplist.csv", 4, "patch_code_post_gfx_mp"),
                                     # Survival levels: authored placements (maps/mp/survival/<level>.gsc) or the generic ones
                                     # derived at run time (maps/mp/survival/_generic.gsc). Left out: mp_hunger, mp_vacant (dev
                                     # maps, in no map pack), mp_shipment (no zone in any install), the co-op mission bases
                                     # (mp_bridge, mp_crash_intro, ...: no MP row).
                                     survival_levels=['mp_fav_tropical', 'mp_abandon_sh', 'mp_ambush_sh', 'mp_crash_tropical', 'mp_cross_fire', 'mp_firingrange', 'mp_hardhat_sh', 'mp_highrise', 'mp_qadeem_sh', 'mp_seatown', 'mp_storm_spring', 'mp_strike_sh', 'mp_subbase_sh', 'mp_terminal_cls',
                                                      # stock levels on generic placements
                                                      'mp_afghan', 'mp_bloc_sh', 'mp_bog_sh', 'mp_cargoship_sh', 'mp_estate_tropical', 'mp_killhouse_sh', 'mp_nuked', 'mp_overgrown', 'mp_rust', 'mp_underpass',
                                                      # map pack levels on generic placements
                                                      'mp_abandon_sh_mk', 'mp_arkaden_sh', 'mp_awp_arena', 'mp_backlot_sh', 'mp_boomtown', 'mp_bootleg_sh', 'mp_carbon_sh', 'mp_cha_quad', 'mp_derail_sh', 'mp_district', 'mp_dome', 'mp_fallen_sh', 'mp_foundation_sh', 'mp_interchange_sh', 'mp_invasion_sh', 'mp_italy_sh', 'mp_liberation_sh', 'mp_lockdown_sh', 'mp_mideast', 'mp_mission_sh', 'mp_mogadishu_sh', 'mp_nightshift_sh', 'mp_offshore', 'mp_overwatch_sh', 'mp_radiation_sh', 'mp_resistance_sh', 'mp_rust_long', 'mp_sd_jardin', 'mp_shipment_long', 'mp_showdown', 'mp_tunisia', 'mp_village', 'mp_wasteland_sh'],
                                     # the stock coop_survival_mp_afghan row gets its name, description, image and lists
                                     stock_edits={"coop_survival_mp_afghan": [1, 2, 3, 18], "coop_survival_mp_fav_tropical": list(range(32)), "zm_asylum_sh": [23], "zm_prototype_sh": [23], "zm_factory_sh": [23], "zm_sumpf_sh": [23], "zm_theater_sh": [23]},
                                     arena=("mp/basemaps.arena", "code_post_gfx_mp"),
                                     # stock arena entries of maps no install has and no comaplist row names (and a stock
                                     # duplicate): C1 reads only 128 entries (shipping.ARENA_MAX_ENTRIES), so they make room
                                     # for the usermaps
                                     arena_prune=["mp_invasion", "mp_checkpoint", "mp_favela", "mp_brecourt", "mp_derail", "mp_estate",
                                                  "mp_storm", "mp_compact", "mp_crash", "mp_abandon", "mp_fuel2", "mp_crossfire",
                                                  "mp_killhouse", "mp_cargoship", "mp_bloc", "mp_bog", "coop_takeover_mp_estate",
                                                  "coop_training_mp_killhouse", "mp_hunted", "coop_zac130_mp_derail",
                                                  "coop_sniperfi4_mp_con_spring", "mp_oilrig2", "mp_stop", "coop_spec_mp_bloc",
                                                  "zm_sumpf", "mp_cliffhanger"],
                                     assets=["rawfile:vision/{map}.vision"], localize=True)),
    "usermaps_ui_mp": dict(origin="G:" + PATCH, install=PATCH, older="Z:" + PATCH,
                           maps=dict(assets=["image:sfproto_{map}_usermap"])),
    # the front-end movies, shared by both languages: codol-classic's English codo.gfx (theatre, survival button, party
    # modes, ...) and codologin.gfx with every text a localize key (Chinese entries here, English in english_ui),
    # plus Kernel33's Options classes (Voice, Max FPS, Controller tab)
    "voice_ui_mp": dict(origin="R:3317112:patch", install=PATCH,
                        gfx={"scaleform/default/codo.gfx": CODO, "scaleform/default/codologin.gfx": LOGIN},
                        handwritten={"scaleform/default/codo.gfx": {"codo.views.option_sub.OptionView_audio",
                                                                    "codo.views.option_sub.OptionView_video",
                                                                    "codo.views.OptionView",
                                                                    "codo.views.ServerMessageView",
                                                                    "codo.views.MatchMPCTMenuView",
                                                                    "codo.views.MatchRoomListView",
                                                                    "codo.views.MatchSelectModeHomeView",
                                                                    "codo.views.MatchMiniRoomCreatorView",
                                                                    "codo.views.matchmpctroomcreator_sub.MatchMPCTRoomCreatorView_mpcreator",
                                                                    "codo.views.MatchMiniRoomCreatorCyborgsView",
                                                                    "codo.views.InRoomBaseView",
                                                                    "codo.k33.LocalCreateRoomOption",
                                                                    "codo.k33.LocalMatchControls",
                                                                    "codo.k33.PadSnap", "components.CodoUIComponent"},
                                     "scaleform/default/codologin.gfx": {"codo.k33.PadSnap",
                                                                         "components.CodoUIComponent"}}),
    # --- client_shipRetail\chinese: the Pack-a-Punch rebuild of a stock zone
    "patch_common_gametype_zm": dict(origin="G:" + CHINESE, install=CHINESE,
                                     stock=["patch_common_gametype_zm", "common_gametype_zm",
                                            "zm_prototype_sh"]),
    # --- client_shipRetail\patch, *_postgfx_mp: loaded after code_post_gfx (permanent; images allowed)
    # the controller button icons controller_glyphs.c draws in text: 4 sets x 18 images (64x64 A8R8G8B8, 7 levels,
    # from the former res/controller_glyphs.bin) and a material each (hud_status_dead's layout, ",2d")
    "controller_glyphs_postgfx_mp": dict(origin="new", install=PATCH),
    # --- client_shipRetail\patch, mp_*: the MP weapon zones (the sh1x server mounts mp_*.ff of the patch folder with
    # every MP map, so client and server read the same tables and models); standalone, not part of a pack
    # the weapon tables, whole (ahead of code_pre_gfx: stringtables only), from mp_m14_staging gen_mp_weapons.py
    "mp_weapons": dict(origin="new", install=PATCH),
    # the art (after code_post_gfx, permanent: models, materials, images): the M14 EBR's base gun, the zombies M14
    # models with their default parts hidden, by mp_m14_staging's CodolSoundDumper mp-weapons-zone (C#-written)
    "mp_weapons_postgfx_mp": dict(origin="new", install=PATCH, stock=["common_gametype_mp", "common_gametype_zm"]),
    # Historical Survival staging sources remain reproducible; the integrated
    # shipping kiosks use patch_common_gametype_survival instead.
    "survival_menus": dict(origin="new", install=CHINESE, staging=True),
    "survival_localize_chinese": dict(origin="new", install=CHINESE, staging=True),
    "survival_localize_english": dict(origin="new", install=CHINESE, staging=True),
}
