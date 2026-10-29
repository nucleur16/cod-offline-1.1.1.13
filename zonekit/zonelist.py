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
(zonekit/packs.py: codol_common, codol_common_ui_mp, codol_common_postgfx_mp, codol_english_locale,
codol_chinese_locale), except codol_infected_cyborg_postgfx_mp, which installs on its own.
"""

PATCH = "client_shipRetail/patch"
CHINESE = "client_shipRetail/chinese"

FONTS = "patch_code_post_gfx_mp:scaleform/fonts_ingame.gfx"
CODO = "patch_ui_mp:scaleform/default/codo.gfx"
LOGIN = "ui_mp:scaleform/default/codologin.gfx"
HUD = "patch_common_mp:scaleform/default/codohud.gfx"
INGAME = "patch_common_mp:scaleform/default/codoingame.gfx"

ZONES = {
    # --- client_shipRetail\patch (Kernel33 loads these ahead of code_pre_gfx; first asset linked under a name wins)
    "000_codol_party_modes": dict(origin="Z:" + PATCH, install=PATCH),
    "codol_english": dict(origin="Z:" + PATCH, install=PATCH),
    "codol_english_font": dict(origin="Z:" + PATCH, install=PATCH, gfx={"scaleform/fonts_ingame.gfx": FONTS}),
    # the text of the shared UI movies (voice_ui_mp, controller_ingame): their K33UI_* localize keys, in English
    # (codol_english_ui*: k33_english 1) or Chinese (codol_chinese_ui*: k33_english 0); Kernel33 loads one set (ui_keys.py)
    "codol_chinese_ui": dict(origin="new", install=PATCH),
    "codol_chinese_ui_extra": dict(origin="new", install=PATCH),
    "codol_english_ui": dict(origin="Z:" + PATCH, install=PATCH),
    "codol_english_ui_extra": dict(origin="Z:" + PATCH, install=PATCH),
    "codol_english_zombies_base": dict(origin="Z:" + PATCH, install=PATCH),
    # usermaps_patch's map names and descriptions in English (its own are GBK Chinese); ahead of usermaps_patch, so
    # they win while English is on, filtered like usermaps_patch's (a map that does not ship has no strings here either)
    "codol_english_usermaps": dict(origin="new", install=PATCH, maps=dict(localize_from="usermaps_patch")),
    "codol_infected_cyborg_postgfx_mp": dict(origin="Z:" + PATCH, install=PATCH,
                                             stock=["code_post_gfx_mp", "patch_code_post_gfx_mp", "common_mp",
                                                    "common_gametype_zm", "patch_common_mp"]),
    "codol_loading_mode_names": dict(origin="Z:" + PATCH, install=PATCH),
    "codol_zm_deathmachine_audio": dict(origin="Z:" + PATCH, install=PATCH),
    # the in-match movies, shared by both languages (codol-classic's English codohud / codoingame with every text a
    # localize key, the Chinese entries here, the English ones in codol_english_ui_extra) and the pause menu's pad
    # navigation; ahead of code_pre_gfx, so they stay loaded in a match
    "controller_ingame": dict(origin="R:3317112:patch", install=PATCH,
                              gfx={"scaleform/default/codohud.gfx": HUD, "scaleform/default/codoingame.gfx": INGAME},
                              handwritten={"scaleform/default/codoingame.gfx": {"MainPause"}}),
    # the usermaps' front-end data, filtered by shipping_maps.txt (zonekit/shipping.py): only "ship" maps' rows,
    # arena entries, strings, vision files and map-select images are in a shipping build
    "usermaps_patch": dict(origin="G:" + PATCH, install=PATCH, older="Z:" + PATCH,
                           # stock_edits: every zombies map offers endless (Asylum and Prototype first, then the Der Riese, Shi No Numa and Kino backports) (column 23; Cam's
                           # codol_zombies_endless zone, folded in here: two zones with mp/comaplist.csv replace each other)
                           maps=dict(table=("mp/comaplist.csv", 4, "patch_code_post_gfx_mp"),
                                     survival_levels=['mp_fav_tropical', 'mp_abandon_sh', 'mp_ambush_sh', 'mp_crash_tropical', 'mp_cross_fire', 'mp_firingrange', 'mp_hardhat_sh', 'mp_highrise', 'mp_qadeem_sh', 'mp_seatown', 'mp_storm_spring', 'mp_strike_sh', 'mp_subbase_sh', 'mp_terminal_cls'],
                                     stock_edits={"coop_survival_mp_fav_tropical": list(range(32)), "zm_asylum_sh": [23], "zm_prototype_sh": [23], "zm_factory_sh": [23], "zm_sumpf_sh": [23], "zm_theater_sh": [23]},
                                     arena=("mp/basemaps.arena", "code_post_gfx_mp"),
                                     assets=["rawfile:vision/{map}.vision"], localize=True)),
    "usermaps_ui_mp": dict(origin="G:" + PATCH, install=PATCH, older="Z:" + PATCH,
                           maps=dict(assets=["image:sfproto_{map}_usermap"])),
    # the front-end movies, shared by both languages: codol-classic's English codo.gfx (theatre, survival button, party
    # modes, ...) and codologin.gfx with every text a localize key (Chinese entries here, English in codol_english_ui),
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
                                                                    "codo.views.InRoomBaseView",
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
    # the weapon tables, whole (ahead of code_pre_gfx: stringtables only), from mp_m14_staging gen_mp_codol_weapons.py
    "mp_codol_weapons": dict(origin="new", install=PATCH),
    # the art (after code_post_gfx, permanent: models, materials, images): the M14 EBR's base gun, the zombies M14
    # models with their default parts hidden, by mp_m14_staging's CodolSoundDumper mp-weapons-zone (C#-written)
    "mp_codol_weapons_postgfx_mp": dict(origin="new", install=PATCH, stock=["common_gametype_mp", "common_gametype_zm"]),
    # Historical Survival staging sources remain reproducible; the integrated
    # shipping kiosks use patch_common_gametype_survival instead.
    "survival_menus": dict(origin="new", install=CHINESE, staging=True),
    "survival_localize_chinese": dict(origin="new", install=CHINESE, staging=True),
    "survival_localize_english": dict(origin="new", install=CHINESE, staging=True),
}
