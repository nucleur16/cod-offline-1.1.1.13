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
    # the English UI (k33_english, kernel33.c EnglishZone): codo.gfx and codoingame.gfx also carry voice_ui_mp's and
    # controller_ingame's hand-written classes (English strings), since these zones load ahead of those and win
    "codol_english_ui": dict(origin="Z:" + PATCH, install=PATCH, gfx={"scaleform/default/codo.gfx": CODO},
                             handwritten={"scaleform/default/codo.gfx": {"codo.views.option_sub.OptionView_audio",
                                                                         "codo.views.option_sub.OptionView_video",
                                                                         "codo.views.OptionView"}}),
    "codol_english_ui_extra": dict(origin="Z:" + PATCH, install=PATCH,
                                   gfx={"scaleform/default/codologin.gfx": LOGIN,
                                        "scaleform/default/codohud.gfx": HUD,
                                        "scaleform/default/codoingame.gfx": INGAME},
                                   handwritten={"scaleform/default/codoingame.gfx": {"MainPause"}}),
    "codol_english_zombies_base": dict(origin="Z:" + PATCH, install=PATCH),
    # usermaps_patch's map names and descriptions in English (its own are GBK Chinese); ahead of usermaps_patch, so
    # they win while English is on, filtered like usermaps_patch's (a map that does not ship has no strings here either)
    "codol_english_usermaps": dict(origin="new", install=PATCH, maps=dict(localize_from="usermaps_patch")),
    "codol_infected_cyborg_postgfx_mp": dict(origin="Z:" + PATCH, install=PATCH,
                                             stock=["code_post_gfx_mp", "patch_code_post_gfx_mp", "common_mp",
                                                    "common_gametype_zm", "patch_common_mp"]),
    "codol_loading_mode_names": dict(origin="Z:" + PATCH, install=PATCH),
    "codol_zm_deathmachine_audio": dict(origin="Z:" + PATCH, install=PATCH),
    "controller_ingame": dict(origin="R:3317112:patch", install=PATCH, gfx={"scaleform/default/codoingame.gfx": INGAME},
                              handwritten={"scaleform/default/codoingame.gfx": {"MainPause"}}),
    # the usermaps' front-end data, filtered by shipping_maps.txt (zonekit/shipping.py): only "ship" maps' rows,
    # arena entries, strings, vision files and map-select images are in a shipping build
    "usermaps_patch": dict(origin="G:" + PATCH, install=PATCH, older="Z:" + PATCH,
                           maps=dict(table=("mp/comaplist.csv", 4, "patch_code_post_gfx_mp"),
                                     arena=("mp/basemaps.arena", "code_post_gfx_mp"),
                                     assets=["rawfile:vision/{map}.vision"], localize=True)),
    "usermaps_ui_mp": dict(origin="G:" + PATCH, install=PATCH, older="Z:" + PATCH,
                           maps=dict(assets=["image:sfproto_{map}_usermap"])),
    "voice_ui_mp": dict(origin="R:3317112:patch", install=PATCH, gfx={"scaleform/default/codo.gfx": CODO},
                        handwritten={"scaleform/default/codo.gfx": {"codo.views.option_sub.OptionView_audio",
                                                                    "codo.views.option_sub.OptionView_video",
                                                                    "codo.views.OptionView"}}),
    # --- client_shipRetail\chinese: the Pack-a-Punch rebuild of a stock zone
    "patch_common_gametype_zm": dict(origin="G:" + CHINESE, install=CHINESE,
                                     stock=["patch_common_gametype_zm.ff.pre-pap", "common_gametype_zm",
                                            "zm_prototype_sh"]),
    # --- client_shipRetail\patch, *_postgfx_mp: loaded after code_post_gfx (permanent; images allowed)
    # the controller button icons controller_glyphs.c draws in text: 4 sets x 18 images (64x64 A8R8G8B8, 7 levels,
    # from the former res/controller_glyphs.bin) and a material each (hud_status_dead's layout, ",2d")
    "controller_glyphs_postgfx_mp": dict(origin="new", install=PATCH),
}
