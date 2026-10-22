# -*- coding: utf-8 -*-
r"""Every zone this repo carries: where the shipped file is, where it installs, and what stock data it is built on.

`origin` is where the zone was decompiled from: G = the 1.1.1.13 install ("Call of Duty Online 1.1.1.13"),
Z = codol-classic-zz, R:<commit>:<dir> = a .ff this repository used to commit (patch/, removed when the zones moved
here). Where two places carry a zone, the newest build is the source.

`stock` names stock 1.1.1.13 zones (client_shipRetail\chinese) whose asset blobs this zone reuses unchanged: the
manifest names those blobs by sha1 and the build takes them from the stock zone instead of the repo.
`gfx` maps a .gfx rawfile to the stock movie it patches ("<stock zone>:<rawfile>").
`as` maps a .gfx rawfile to {class: actionscript/<file>.as}: classes built from the hand-written sources in
actionscript/ (gfx.py "source" lines).
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
    "codol_english_ui": dict(origin="Z:" + PATCH, install=PATCH, gfx={"scaleform/default/codo.gfx": CODO}),
    "codol_english_ui_extra": dict(origin="Z:" + PATCH, install=PATCH,
                                   gfx={"scaleform/default/codologin.gfx": LOGIN,
                                        "scaleform/default/codohud.gfx": HUD,
                                        "scaleform/default/codoingame.gfx": INGAME}),
    "codol_english_zombies_base": dict(origin="Z:" + PATCH, install=PATCH),
    "codol_infected_cyborg_postgfx_mp": dict(origin="Z:" + PATCH, install=PATCH,
                                             stock=["code_post_gfx_mp", "patch_code_post_gfx_mp", "common_mp",
                                                    "common_gametype_zm", "patch_common_mp"]),
    "codol_loading_mode_names": dict(origin="Z:" + PATCH, install=PATCH),
    "codol_zm_deathmachine_audio": dict(origin="Z:" + PATCH, install=PATCH),
    "controller_ingame": dict(origin="R:3317112:patch", install=PATCH, gfx={"scaleform/default/codoingame.gfx": INGAME},
                              **{"as": {"scaleform/default/codoingame.gfx": {"MainPause": "actionscript/MainPause.as"}}}),
    "usermaps_patch": dict(origin="G:" + PATCH, install=PATCH, older="Z:" + PATCH),
    "usermaps_ui_mp": dict(origin="G:" + PATCH, install=PATCH, older="Z:" + PATCH),
    "voice_ui_mp": dict(origin="R:3317112:patch", install=PATCH, gfx={"scaleform/default/codo.gfx": CODO},
                        **{"as": {"scaleform/default/codo.gfx": {
                            "codo.views.option_sub.OptionView_audio": "actionscript/OptionView_audio.as",
                            "codo.views.option_sub.OptionView_video": "actionscript/OptionView_video.as",
                            "codo.views.OptionView": "actionscript/OptionView.as"}}}),
    # --- client_shipRetail\chinese: the Pack-a-Punch rebuild of a stock zone
    "patch_common_gametype_zm": dict(origin="G:" + CHINESE, install=CHINESE,
                                     stock=["patch_common_gametype_zm.ff.pre-pap", "common_gametype_zm",
                                            "zm_prototype_sh"]),
}
