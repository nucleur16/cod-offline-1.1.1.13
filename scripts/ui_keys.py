# -*- coding: utf-8 -*-
r"""ui_keys.py -- turn the shared UI movies' text into localize keys, so one movie serves both languages.

The front-end and in-match movies (voice_ui_mp: codo.gfx, codologin.gfx; controller_ingame: codohud.gfx,
codoingame.gfx) are codol-classic's English movies plus Kernel33's classes. C1's Scaleform translator replaces any text
that starts with "@" by the localize entry of that name ("@SF_UI_REFRESH" -> SF_UI_REFRESH), when a text field is
created from the movie and whenever code sets .text / .label (codo.utils.LocalizeUtil.localize relies on it; a missing
entry shows as the "@NAME" itself). So:
  - every static text (DefineEditText) whose stock text is Chinese and whose English movie text differs becomes the
    plain text "@KEY" (an HTML text with the key inside its tags is not translated, so the key is the whole text);
    texts with the same English and Chinese share one key, named after the first: K33UI_<MOVIE>_<tag index>;
  - the string literals codol-classic's classes show (LITERALS) are replaced in their DoABC string pools by keys
    (zonekit/abcstr.py: only strings that are not also names); codo.constants.LocalizationDict goes back to the stock
    class (codol-classic had put English text in place of six of C1's own keys);
  - the hand-written classes (OptionView*, MatchMPCTMenuView, MatchRoomListView, MatchSelectModeHomeView,
    MainPause) use keys in their source (NAMED below); a class whose literal is also a name in its ABC
    (MatchRoomListView: "MAP") must be one of those.
Entries (GBK): English in english_ui (codo, login keys) / english_ui_extra (hud, ingame), Chinese in
chinese_ui / chinese_ui_extra. Kernel33 loads one set: english* with loc_english 1, chinese*
with 0 (C1 holds at most 12000 localize assets, and an entry loaded twice takes two). An entry already in a
localize.str keeps its text, so those files are where a text is changed; running this again only adds what is missing
and drops text keys no movie uses any more.

Adding a string: use "@K33UI_<NAME>" (for .text / .label / a list label) or LocalizeUtil.localize("@K33UI_<NAME>")
(to build a string; LocalizeUtil.replace fills #A# #B#) in the .as file, add the key to NAMED with its English and
Chinese text and the movie, and run this (it compiles the class, adds the entries to the English and Chinese zones and
their manifests).

    python scripts/ui_keys.py           convert (needs FFDec when a hand-written class changed) and update zones\ + zone_source\
"""
from __future__ import annotations

import html
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # the repository root (this file is in scripts/)
sys.path.insert(0, HERE)
from zonekit import abcstr, build, codecs, gfx, manifest, stock, zonelist  # noqa: E402

GAME = os.environ.get("K33_GAME", r"D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13")
FFDEC = os.environ.get("K33_FFDEC", r"C:\Program Files (x86)\FFDec\ffdec-cli.exe")

# (zone, rawfile, key prefix, (English zone, Chinese zone))
UI = ("english_ui", "chinese_ui")
UI_EXTRA = ("english_ui_extra", "chinese_ui_extra")
MOVIES = [("voice_ui_mp", "scaleform/default/codo.gfx", "CODO", UI),
          ("voice_ui_mp", "scaleform/default/codologin.gfx", "LOGIN", UI),
          ("controller_ingame", "scaleform/default/codohud.gfx", "HUD", UI_EXTRA),
          ("controller_ingame", "scaleform/default/codoingame.gfx", "INGAME", UI_EXTRA)]
TEXT_KEY = re.compile(r"K33UI_(CODO|LOGIN|HUD|INGAME)_\d+$")

# codol-classic's literals: class -> {literal: replacement}; "@SF_UI_*" are C1's own entries
LITERALS = {
    "codo.gfx": {
        "codo/views/BottomStartGameView": {"WATCH": "@K33UI_THEATRE_WATCH"},
        "codo/views/QuickMatchChooseNewView": {"Modes": "@K33UI_MODES"},
        "components/list_part/RoomListItemRenderer": {"Watch": "@K33UI_THEATRE_WATCH_ROW"},
        "Main": {"No matches recorded yet": "@K33UI_THEATRE_EMPTY", "Unknown Map": "@K33UI_UNKNOWN_MAP",
                 "Unknown Mode": "@K33UI_UNKNOWN_MODE"},
        "codo/services/SettingsService": {"Fullscreen Windowed": "@K33UI_DISPLAY_BORDERLESS"},
    },
}
STOCK_CLASSES = {"codo.gfx": ["codo/constants/LocalizationDict"]}

# named entries: key -> (English, Chinese, movie)
NAMED = {
    # main screen, theatre (codo.gfx: MatchSelectModeHomeView, MatchMPCTMenuView, MatchRoomListView, Main, ...)
    "K33UI_SURVIVAL": ("SURVIVAL", "生存模式", "codo.gfx"),
    "K33UI_THEATRE": ("THEATRE", "影院", "codo.gfx"),
    "K33UI_THEATRE_ALL": ("All matches", "全部比赛", "codo.gfx"),
    "K33UI_THEATRE_PAGE": ("Page #A# of #B#", "第 #A# 页, 共 #B# 页", "codo.gfx"),
    "K33UI_THEATRE_HELP": ("Your Multiplayer and Zombies matches.", "你的多人竞技和僵尸围城比赛录像。", "codo.gfx"),
    "K33UI_THEATRE_MY_MATCHES": ("MY MATCHES", "我的比赛", "codo.gfx"),
    "K33UI_THEATRE_EMPTY": ("No matches recorded yet", "还没有比赛录像", "codo.gfx"),
    "K33UI_THEATRE_RECORDED": ("RECORDED", "录制时间", "codo.gfx"),
    "K33UI_THEATRE_LENGTH": ("LENGTH", "时长", "codo.gfx"),
    "K33UI_THEATRE_WATCH": ("WATCH", "观看", "codo.gfx"),
    "K33UI_THEATRE_WATCH_ROW": ("Watch", "观看", "codo.gfx"),
    "K33UI_PREVIOUS": ("Previous", "上一页", "codo.gfx"),
    "K33UI_NEXT": ("Next", "下一页", "codo.gfx"),
    "K33UI_MODES": ("Modes", "模式", "codo.gfx"),
    "K33UI_UNKNOWN_MAP": ("Unknown Map", "未知地图", "codo.gfx"),
    "K33UI_UNKNOWN_MODE": ("Unknown Mode", "未知模式", "codo.gfx"),
    "K33UI_DISPLAY_BORDERLESS": ("Fullscreen Windowed", "无边框窗口", "codo.gfx"),
    # C1's own key that its Chinese strings lack (LocalizationDict.QUESTION_CANCEL)
    "SF_UI_QUESTION_CANCEL": (None, "取消", "codo.gfx"),
    # Options > Audio / Video (OptionView_audio, OptionView_video)
    "K33UI_VOICE_MODE": ("Voice Mode", "语音模式", "codo.gfx"),
    "K33UI_VOICE_INPUT_DEVICE": ("Input Device", "输入设备", "codo.gfx"),
    "K33UI_VOICE_DEFAULT_DEVICE": ("Default Device", "默认设备", "codo.gfx"),
    "K33UI_VOICE_PTT": ("Push to talk", "按键说话", "codo.gfx"),
    "K33UI_VOICE_VAD": ("Voice activation", "语音激活", "codo.gfx"),
    "K33UI_VOICE_NO_MIC": ("no mic", "无麦克风", "codo.gfx"),
    "K33UI_VOICE_MIC_OFF": ("off", "关闭", "codo.gfx"),
    "K33UI_MAX_FPS": ("Max FPS", "最大帧率", "codo.gfx"),
    "K33UI_FPS_UNLIMITED": ("Unlimited", "无限制", "codo.gfx"),
    # Options > Controller (OptionView)
    "K33UI_ON": ("On", "开", "codo.gfx"),
    "K33UI_OFF": ("Off", "关", "codo.gfx"),
    "K33UI_PAD_TAB": ("Controller", "手柄", "codo.gfx"),
    "K33UI_PAD_LAYOUT": ("Pad Layout", "手柄图示", "codo.gfx"),
    "K33UI_PAD_BINDS": ("Button Binds", "按键设置", "codo.gfx"),
    "K33UI_PAD_BIND_HINT": ("Click an action, then press the pad button for it (triggers and sticks cannot be bound)",
                            "点击动作, 再按要使用的手柄按键 (扳机和摇杆不能绑定)", "codo.gfx"),
    "K33UI_PAD_CLEAR": ("Clear Binding", "清除所选", "codo.gfx"),
    "K33UI_PAD_CLEAR_PICK": ("Click the action to clear", "点击要清除的动作", "codo.gfx"),
    "K33UI_PAD_NONE": ("No controller connected", "未连接手柄", "codo.gfx"),
    "K33UI_PAD_PRESS": ("Press a pad button...", "请按下手柄按键...", "codo.gfx"),
    "K33UI_PAD_UNBOUND": ("Unbound", "未绑定", "codo.gfx"),
    "K33UI_PAD_HOLD": ("Hold #A#", "按住 #A#", "codo.gfx"),
    "K33UI_PAD_AND_HOLD": (", hold #A#", ", 按住#A#", "codo.gfx"),
    "K33UI_PAD_FEEL_RESPONSIVE": ("Responsive", "灵敏", "codo.gfx"),
    "K33UI_PAD_FEEL_SMOOTH": ("Smooth", "平滑", "codo.gfx"),
    "K33UI_PAD_FEEL_CLASSIC": ("Classic", "经典", "codo.gfx"),
    "K33UI_PAD_SET_SWAPSTICKS": ("Swap sticks (lefty)", "交换摇杆 (左手模式)", "codo.gfx"),
    "K33UI_PAD_SET_SWAPTRIGGERS": ("Swap triggers (LT fire)", "交换扳机 (LT 开火)", "codo.gfx"),
    "K33UI_PAD_SET_LOOKFEEL": ("Look feel", "视角手感", "codo.gfx"),
    "K33UI_PAD_SET_SENS": ("Look sensitivity", "视角灵敏度", "codo.gfx"),
    "K33UI_PAD_SET_INVERT": ("Invert Y axis", "反转 Y 轴", "codo.gfx"),
    "K33UI_PAD_SET_AIMASSIST": ("Aim assist", "辅助瞄准", "codo.gfx"),
    "K33UI_PAD_SET_DEADZONE": ("Stick deadzone", "摇杆死区", "codo.gfx"),
    "K33UI_PAD_SET_DIVE": ("Dive from sprint", "冲刺时蹲伏键飞扑", "codo.gfx"),
    "K33UI_PAD_SET_SNAP": ("Menu cursor snap", "菜单光标吸附", "codo.gfx"),
    "K33UI_PAD_SET_SNAPSTR": ("Snap strength", "吸附强度", "codo.gfx"),
    "K33UI_PAD_ACT_JUMP": ("Jump", "跳跃", "codo.gfx"),
    "K33UI_PAD_ACT_STANCE": ("Crouch/Prone", "蹲伏/卧倒", "codo.gfx"),
    "K33UI_PAD_ACT_RELOAD": ("Reload", "换弹", "codo.gfx"),
    "K33UI_PAD_ACT_USE": ("Use/Pick up", "使用/拾取", "codo.gfx"),
    "K33UI_PAD_ACT_SWITCH": ("Switch Weapon", "切换武器", "codo.gfx"),
    "K33UI_PAD_ACT_ATTACH": ("Attachment", "武器附件", "codo.gfx"),
    "K33UI_PAD_ACT_MELEE": ("Melee", "近战", "codo.gfx"),
    "K33UI_PAD_ACT_SPRINT": ("Sprint/Hold Breath", "冲刺/屏息", "codo.gfx"),
    "K33UI_PAD_ACT_LETHAL": ("Lethal", "致命装备", "codo.gfx"),
    "K33UI_PAD_ACT_TACTICAL": ("Tactical", "战术装备", "codo.gfx"),
    "K33UI_PAD_ACT_SPECIAL": ("Special", "特殊装备", "codo.gfx"),
    "K33UI_PAD_ACT_KS_SELECT": ("Select Killstreak", "选择连杀奖励", "codo.gfx"),
    "K33UI_PAD_ACT_KS_USE": ("Use Killstreak", "使用连杀奖励", "codo.gfx"),
    "K33UI_PAD_ACT_MENU": ("Menu", "菜单", "codo.gfx"),
    "K33UI_PAD_ACT_SCORES": ("Scoreboard", "计分板", "codo.gfx"),
    "K33UI_PAD_ACT_TALK": ("Talk", "语音", "codo.gfx"),
    "K33UI_PAD_ACT_FIRE": ("Fire", "开火", "codo.gfx"),
    "K33UI_PAD_ACT_AIM": ("Aim", "瞄准", "codo.gfx"),
    "K33UI_PAD_BTN_DPAD_UP": ("D-pad Up", "十字键上", "codo.gfx"),
    "K33UI_PAD_BTN_DPAD_DOWN": ("D-pad Down", "十字键下", "codo.gfx"),
    "K33UI_PAD_BTN_DPAD_LEFT": ("D-pad Left", "十字键左", "codo.gfx"),
    "K33UI_PAD_BTN_DPAD_RIGHT": ("D-pad Right", "十字键右", "codo.gfx"),
    "K33UI_PAD_BTN_LSTICK": ("L Stick", "左摇杆", "codo.gfx"),
    "K33UI_PAD_BTN_RSTICK": ("R Stick", "右摇杆", "codo.gfx"),
    "K33UI_PAD_BTN_FACE_DOWN": ("Down", "下键", "codo.gfx"),
    "K33UI_PAD_BTN_FACE_RIGHT": ("Right", "右键", "codo.gfx"),
    "K33UI_PAD_BTN_FACE_LEFT": ("Left", "左键", "codo.gfx"),
    "K33UI_PAD_BTN_FACE_UP": ("Up", "上键", "codo.gfx"),
    "K33UI_PAD_BTN_GENERIC_START": ("Start", "开始", "codo.gfx"),
    "K33UI_PAD_BTN_GENERIC_BACK": ("Back", "返回", "codo.gfx"),
    # Options > Graphics (OptionView_video) and the pause menu's Options list (codoingame.gfx MainPause): the
    # Language / Button icons / HUD style rows (docs/UI_STYLE_OPTIONS_20261112.md). Both movies show these keys;
    # they are entered once, here (<language>_ui), not also in <language>_ui_extra: both string zones are parts of the
    # same <language>_locale pack, which is loaded whole, and an entry loaded twice takes two of C1's slots.
    "K33UI_LANGUAGE": ("Language", "语言", "codo.gfx"),
    "K33UI_GLYPHS": ("Button icons", "按键图标", "codo.gfx"),
    "K33UI_HUD_STYLE": ("HUD style", "HUD 样式", "codo.gfx"),
    "K33UI_LANG_NAME_EN": ("English", "English", "codo.gfx"),
    "K33UI_LANG_NAME_ZH": ("中文 (Chinese)", "中文", "codo.gfx"),
    "K33UI_LANG_RESTART": ("Restart the game to apply the language", "重启游戏后语言生效", "codo.gfx"),
    "K33UI_LANG_SWITCHING": ("Switching language...", "正在切换语言...", "codo.gfx"),
    "K33UI_GLYPHS_AUTO": ("Auto", "自动", "codo.gfx"),
    "K33UI_GLYPHS_XBOX": ("Xbox", "Xbox", "codo.gfx"),
    "K33UI_GLYPHS_PS": ("PlayStation", "PlayStation", "codo.gfx"),
    "K33UI_GLYPHS_NINTENDO": ("Wii U / Switch", "Wii U / Switch", "codo.gfx"),
    "K33UI_GLYPHS_GENERIC": ("Generic", "通用", "codo.gfx"),
    "K33UI_GLYPHS_TEXT": ("Text only", "仅文字", "codo.gfx"),
    "K33UI_GLYPHS_KBM": ("Keyboard & mouse", "键盘和鼠标", "codo.gfx"),
    "K33UI_HUD_STYLE_STOCK": ("Classic (1.1.1.13)", "经典 (1.1.1.13)", "codo.gfx"),
    "K33UI_HUD_STYLE_V12": ("Version 1.2", "1.2 版", "codo.gfx"),
    "K33UI_HUD_STYLE_V26": ("Version 2.6", "2.6 版", "codo.gfx"),
    "K33UI_HUD_STYLE_V33": ("Version 3.3", "3.3 版", "codo.gfx"),
    "K33UI_HUD_STYLE_V35": ("Version 3.5", "3.5 版", "codo.gfx"),
    "K33UI_HUD_STYLE_SM2": ("SM2", "SM2", "codo.gfx"),
    "K33UI_HUD_SWAY": ("HUD sway", "HUD 晃动", "codo.gfx"),
    "K33UI_HUD_SWAY_OFF": ("Off", "关", "codo.gfx"),
    "K33UI_HUD_SWAY_ON": ("On", "开", "codo.gfx"),
    "K33UI_GAME_TAB": ("Game", "游戏", "codo.gfx"),
    "K33UI_LOBBY_SCENE": ("Lobby scene", "大厅场景", "codo.gfx"),
    "K33UI_VLOBBY_CENTER": ("Default", "默认", "codo.gfx"),
    "K33UI_VLOBBY_V0_MENU": ("Alpha menu", "早期主菜单", "codo.gfx"),
    "K33UI_VLOBBY_V3_LOBBY": ("Lobby 3.19", "大厅 3.19", "codo.gfx"),
    "K33UI_VLOBBY_V3_DMZ": ("DMZ lobby", "DMZ 大厅", "codo.gfx"),
    "K33UI_VLOBBY_V2_LOBBY": ("Lobby 2.6", "大厅 2.6", "codo.gfx"),
    "K33UI_VLOBBY_V2_TEAM": ("Team lobby 2.6", "组队大厅 2.6", "codo.gfx"),
}


# --- DefineEditText -----------------------------------------------------------------------------------------------
def _edit_text_split(tag: bytes):
    """-> (header length, body, offset of the initial text in body or None)"""
    cl = struct.unpack_from("<H", tag, 0)[0]
    hl = 6 if cl & 0x3F == 0x3F else 2
    b = tag[hl:]
    at = 2
    at += (5 + 4 * (b[at] >> 3) + 7) // 8
    f1, f2 = b[at], b[at + 1]
    at += 2
    if f1 & 0x01:
        at += 2
    if f2 & 0x80:
        at = b.index(b"\0", at) + 1
    if f1 & 0x01 or f2 & 0x80:
        at += 2
    if f1 & 0x04:
        at += 4
    if f1 & 0x02:
        at += 2
    if f2 & 0x20:
        at += 9
    at = b.index(b"\0", at) + 1                 # variable name
    return hl, b, (at if f1 & 0x80 else None)


def edit_text(tag: bytes):
    _hl, b, at = _edit_text_split(tag)
    return None if at is None else b[at:b.index(b"\0", at)]


def set_edit_text(tag: bytes, text: bytes) -> bytes:
    _hl, b, at = _edit_text_split(tag)
    body = b[:at] + text + b"\0" + b[b.index(b"\0", at) + 1:]
    return struct.pack("<HI", (37 << 6) | 0x3F, len(body)) + body


def plain(text: bytes) -> str:
    """an edit text (HTML or not) as the plain text a localize entry holds"""
    t = text.decode("utf-8")
    if "<" in t:
        t = re.sub(r"</p>\s*<p[^>]*>", "\n", t)
        t = re.sub(r"<br\s*/?>", "\n", t, flags=re.I)
        t = re.sub(r"<[^>]+>", "", t)
        t = html.unescape(t)
    return t.replace("\u00a0", " ").replace("\r", "\n").strip("\n")


# --- conversion ---------------------------------------------------------------------------------------------------
def convert_movie(movie: bytes, stock_movie: bytes, prefix: str, rawname: str, have: dict, keys: dict):
    """-> (movie, set of text keys it uses). `have` {key: (en, zh)}: the entries already written; `keys` {(en, zh): key}
    is shared by the movies of one string zone, so equal texts get one key."""
    mt, st = gfx.tags(movie), gfx.tags(stock_movie)
    # classes Kernel33 added (codo.k33.*: new DoABC tags the stock movie does not have) take no part in the conversion
    stock_abc = {n for c, _s, _e, n in st if c == gfx.T_DOABC}
    added = [i for i, (c, _s, _e, n) in enumerate(mt) if c == gfx.T_DOABC and n not in stock_abc]
    mt_all, mt = mt, [t for i, t in enumerate(mt) if i not in added]
    if len(mt) != len(st):
        raise SystemExit("%s: %d tags, stock %d: the tag lists must line up" % (rawname, len(mt), len(st)))
    used, repl = set(), {}
    lits = LITERALS.get(os.path.basename(rawname), {})
    stock_cls = STOCK_CLASSES.get(os.path.basename(rawname), [])
    done = set()
    for i, ((c, s, e, name), (sc, ss, se, sname)) in enumerate(zip(mt, st)):
        tag, stag = movie[s:e], stock_movie[ss:se]
        if c == 37 and sc == 37 and tag != stag:
            mtxt, stxt = edit_text(tag), edit_text(stag)
            if mtxt is None or stxt is None:
                continue
            if mtxt.startswith(b"@"):            # keyed by an earlier run: its entries say what it shows
                old = mtxt[1:].decode()
                if not TEXT_KEY.match(old):
                    continue
                pair = have.get(old)
                if pair is None:
                    raise SystemExit("%s tag %d shows %s, which no localize.str has" % (rawname, i, old))
            else:
                zh, en = plain(stxt), plain(mtxt)
                if all(ord(ch) < 128 for ch in zh) or zh == en:
                    continue                     # no Chinese to replace: the English movie's tag serves both
                pair = (en, zh)
            key = keys.setdefault(pair, "K33UI_%s_%d" % (prefix, i))
            used.add(key)
            if mtxt != ("@" + key).encode():
                repl[i] = set_edit_text(tag, ("@" + key).encode())
        elif c == gfx.T_DOABC and name in stock_cls:
            repl[i] = stag
            done.add(name)
        elif c == gfx.T_DOABC and name in lits:
            pool = abcstr.strings(tag)
            todo = {k: v for k, v in lits[name].items() if k in pool}
            if todo:
                repl[i] = abcstr.replace(tag, todo)
            done.add(name)
    missing = (set(lits) | set(stock_cls)) - done
    if missing:
        raise SystemExit("%s: classes not found: %s" % (rawname, ", ".join(sorted(missing))))
    out = bytearray(movie[:gfx.header_len(movie)])
    j = 0
    for i, (c, s, e, _n) in enumerate(mt_all):
        if i in added:
            out += movie[s:e]
            continue
        out += repl.get(j, movie[s:e])
        j += 1
    struct.pack_into("<I", out, 4, len(out))
    return bytes(out), used


def read_localize(zone):
    path = os.path.join(HERE, "zones", zone, "localize.str")
    if not os.path.isfile(path):
        return {}
    with open(path, encoding="utf-8") as f:
        return {n: v.decode("gbk") for n, v in codecs.read_str(f.read(), "gbk")}


def write_localize(zone: str, entries: list):
    """zones/<zone>/localize.str and the zone's localize lines = `entries` [(key, text)] (GBK)"""
    for _k, v in entries:
        v.encode("gbk")                         # fails here, not in the build, when a text has no GBK form
    with open(os.path.join(HERE, "zones", zone, "localize.str"), "w", encoding="utf-8", newline="\n") as f:
        f.write(codecs.write_str([(k, v.encode("gbk")) for k, v in entries], "gbk"))
    mpath = os.path.join(HERE, "zone_source", zone + ".csv")
    with open(mpath, encoding="utf-8") as f:
        m = manifest.read(f.read())
    m.assets = [a for a in m.assets if a[0] != "localize"] + [("localize", k, {}) for k, _v in entries]
    with open(mpath, "w", encoding="utf-8", newline="\n") as f:
        f.write(manifest.write(m))


def main():
    exe = os.path.join(GAME, "C1.exe")
    src = stock.ZoneSource(exe, [os.path.join(GAME, "client_shipRetail", "chinese")])
    ffdec = FFDEC if os.path.isfile(FFDEC) else None
    for pair_zones in (UI, UI_EXTRA):
        en_old, zh_old = (read_localize(z) for z in pair_zones)
        have = {k: (en_old.get(k), zh_old.get(k)) for k in set(en_old) | set(zh_old)}
        keys = {}
        for k in sorted(have, key=lambda k: (k.split("_")[1], int(k.rsplit("_", 1)[1])) if TEXT_KEY.match(k) else ("", 0)):
            if TEXT_KEY.match(k):
                keys.setdefault(have[k], k)
        used = set()
        movies = [m for m in MOVIES if m[3] == pair_zones]
        for zone, rawname, prefix, _z in movies:
            pdir = os.path.join(HERE, "zones", zone, "rawfile", rawname + ".gfxpatch")
            meta, _ops = gfx.read_patch(pdir)
            stock_movie = src.rawfile(meta["stock"])
            movie = gfx.build_patch(pdir, stock_movie)
            movie = build.compile_as_files(pdir, movie, stock_movie, ffdec, False, print)
            movie, u = convert_movie(movie, stock_movie, prefix, rawname, have, keys)
            used |= u
            hw = zonelist.ZONES[zone].get("handwritten", {}).get(rawname, ())
            n = gfx.write_patch(pdir, meta["stock"], stock_movie, movie, ffdec, hw)
            print("%s %s: %d new tags, %d text keys" % (zone, rawname, n, len(u)))
        names = {os.path.basename(m[1]) for m in movies}
        named = [(k, v) for k, v in NAMED.items() if v[2] in names]
        text = sorted(((k, p) for p, k in keys.items() if k in used),
                      key=lambda kp: (kp[0].split("_")[1], int(kp[0].rsplit("_", 1)[1])))
        for lang, zone, old in ((0, pair_zones[0], en_old), (1, pair_zones[1], zh_old)):
            out = [(k, old.get(k, v[lang])) for k, v in named if old.get(k, v[lang]) is not None]
            out += [(k, old.get(k, p[lang])) for k, p in text]
            write_localize(zone, out)
            print("%s: %d entries (%d text keys)" % (zone, len(out), len(text)))


if __name__ == "__main__":
    main()
