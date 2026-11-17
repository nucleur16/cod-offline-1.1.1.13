# -*- coding: utf-8 -*-
r"""Menu files (MenuList, asset type 25) and menus (menuDef_t, type 26) of 1.1.1.13: the struct tree <-> the zone blob
(Writer / Reader), and the converter from the menus of the later builds (2.6.9.7, which loads menus with the 3.3.3.3
loaders).

The sources in zones/ are .menu text (zonekit/menu_source.py compiles it to this struct tree and decompiles a tree
back to it). The struct tree is also what CODOL-AssetExplorer's `zone-menus` command dumps from its zone-load emulator
(the reference dumps tools/build_hud_styles.py converts, sources/menus_*.json, are such dumps). Every struct is
{"kind", "raw": hex of its bytes, "p": {byte offset: what the pointer there holds}}; a pointer holds a string, a
struct, a list of structs (an array of pointers or an inline array) or an asset reference {"asset": "material" |
"sound", "name": ",white"}. The pointer fields' bytes in "raw" are ignored: the build writes 0xFFFFFFFF (data follows
inline) or 0 (no "p" entry); normalize() gives the tree exactly as the zone stores it. listing() prints a tree of any
build's layout as items, event scripts and expressions (a debugging view).

Layouts (1.1.1.13: Generated/V1_1_1_13 Load_XAsset case 25 sub_4C1980 -> sub_4C18C0, menuDef_t sub_4C12B0, itemDef_s
sub_4C0C90; 2.6.9.7 / 3.3.3.3: Generated/V3_3_3_3 sub_4904E0 -> sub_48FF80, sub_48F890). W = sizeof(windowDef_t):
  windowDef_t   W = 0xA4 {name @0, rect @4, rectClient @0x18, group @0x2C, style @0x30, border, ownerDraw,
                ownerDrawFlags, borderSize, staticFlags @0x44, dynamicFlags @0x48, nextTime, fore/back/border/outline/
                disableColor @0x50.., background Material* @0xA0}. 2.6.9.7: W = 0xA8, + GfxImage* @0xA4 (no 1.1.1.13
                equivalent; the converter refuses a window that sets it).
  menuDef_t     0x190 (W + 0xEC): font @W, fullScreen @W+4, (C1) @W+8, itemCount @W+0xC, fontIndex, cursorItem,
                fadeCycle, fadeClamp, fadeAmount, fadeInAmount, blurRadius @W+0x28, onOpen @W+0x2C,
                onCloseRequest @W+0x30, onClose @W+0x34, onESC @W+0x38, onKey @W+0x3C, visibleExp @W+0x40,
                allowedBinding @W+0x44, soundName @W+0x48, imageTrack, focusColor @W+0x50, rectX/Y/W/H/openSound/
                closeSoundExp @W+0x60..0x74, items @W+0x78, transitions to the end. 2.6.9.7: 0x194, the same after W.
  itemDef_s     0x17C: window, textRect @W, type @W+0x14, dataType, alignment, fontEnum @W+0x20, textAlignMode,
                textalignx/y, textscale, textStyle, gameMsgWindowIndex/Mode, text @W+0x40, itemFlags, parent,
                mouseEnterText .. leaveFocus @W+0x4C..0x68, dvar @W+0x6C, dvarTest, onKey @W+0x74, enableDvar @W+0x78,
                localVar @W+0x7C, dvarFlags, focusSound @W+0x84, special, cursorPos, typeData @W+0x90, imageTrack,
                floatExpressionCount @W+0x98, floatExpressions @W+0x9C, visible/disabled/text/materialExp @W+0xA0..
                0xAC, glowColor .. lastSoundPlayedTime (0x28). 2.6.9.7: 0x188, six statements @W+0xA0..0xB4 of which
                0 visible, 1 disabled, 2 text and 4 material are IW4's four (slot 4 holds every `exp material` of the
                survival menus; 3 and 5 are unused there and have no 1.1.1.13 field).
  Statement_s   0x14 {numEntries, entries (12 bytes each: {type 0 operator | 1 operand, op or dataType,
                value}; dataType 2 string, 3 function = Statement_s*)}; MenuEventHandlerSet 8 {count, handlers**};
                MenuEventHandler 8 {data, type byte @4: 0 script string, 1 ConditionalScript* {eventHandlerSet,
                eventExpression}, 2 else MenuEventHandlerSet*, 3..6 SetLocalVarData* (bool/int/float/string)
                {localVarName, expression}}; ItemKeyHandler 12 {key, action, next}; ItemFloatExpression 8 {target,
                expression}; typeData by item type: 6 listBoxDef 0x144 (onDoubleClick @0x120, selectIcon @0x140),
                4/9/16/18/11/14/10/0/17/22/23 editFieldDef 0x20, 12 multiDef 0x188 (dvarList[32], dvarStr[32]),
                13 enumDvarName (a string), 20 newsTickerDef 0x1C, 21 textScrollDef 4. Identical in both builds.
  Asset references are inline headers with a ","-prefixed name (1.1.1.13 Material: 0x60 bytes, name @0).

Expression operators are numbered by each client's operator table (1.1.1.13: 218 names at 0x00CD08A8, read from
codoMP_client_shipRetail.exe). 2.6.9.7's executable is packed; its menus decode consistently with the table of the
unpacked 2.2.1.3 client (240 names at 0x00CEB260: `abs` at 30, getclient* at 98..103 and getprestige at 214 inserted,
issurvival .. getweaponfiretype appended), so OPS_2697 is that table. Fonts: 1.1.1.13 UI_GetFontHandle 0x005EC790
knows 3 small, 4 bold, 5 console, 6 objective, 7 normal, 8 extrabig, 9 hudbig, 10 hudsmall; anything else picks a
font by scale, which FONT_2697 maps 2.6.9.7's 11 and 14 to explicitly (0).
"""
from __future__ import annotations

import json
import struct

INLINE = 0xFFFFFFFF

# 1.1.1.13 operator table (0x00CD08A8)
OPS_1113 = (
    "NOOP ) * / % + - ! < <= > >= == != && || ( , & | ~ << >> dvarint(static) dvarbool(static) dvarfloat(static) "
    "dvarstring(static) int string float sin cos min max milliseconds dvarint dvarbool dvarfloat dvarstring dvarsubint "
    "dvarsubbool dvarsubfloat dvarsubstring stat ui_active flashbanged usingvehicle missilecam scoped scopedthermal "
    "scoreboard_visible inkillcam inkillcamnpc player usingabilitymodifier getperk selecting_location "
    "selecting_direction team otherteam marinesfield opforfield menuisopen writingdata inlobby inprivateparty "
    "privatepartyhost privatepartyhostinlobby aloneinparty adsjavelin weaplockblink weapattacktop weapattackdirect "
    "weaplocking weaplocked weaplocktooclose weaplockscreenposx weaplockscreenposy secondsastime tablelookup "
    "tablelookupbyrow tablegetrownum locstring localvarint localvarbool localvarfloat localvarstring timeleft "
    "secondsascountdown gamemsgwndactive gametypename gametype gametypedescription scoreatrank friendsonline "
    "spectatingclient spectatingfree statrangeanybitsset keybinding actionslotusable hudfade maxrecommendedplayers "
    "acceptinginvite isintermission gamehost partyismissingmappack partymissingmappackerror anynewmappacks "
    "amiselected partystatusstring attachedcontrollercount issplitscreenonlinepossible splitscreenplayercount "
    "getplayerdata getplayerdatasplitscreen experienceforlevel levelforexperience getchallengelevelforxp "
    "getChallengeSplashLevel isitemunlocked isitemunlockedsplitscreen debugprint getplayerdataanybooltrue "
    "weaponclassnew weaponname isreloading savegameavailable unlockeditemcount unlockeditemcountsplitscreen "
    "unlockeditem unlockeditemsplitscreen mailsubject mailfrom mailreceived mailbody maillootlocalized mailgivesloot "
    "anynewmail mailtimetofollowup mailloottype mailranlottery lotterylootlocalized radarisjammed radarjamintensity "
    "radarisenabled isempjammed playerads weaponheatactive weaponheatvalue weaponheatoverheated getsplashtext "
    "getsplashdescription getsplashmaterial splashhasicon splashrownum getfocuseditemname getfocuseditemx "
    "getfocuseditemy getfocuseditemwidth getfocuseditemheight getitemx getitemy getitemwidth getitemheight playlist "
    "scoreboardexternalmutenotice getclientmatchdata getclientmatchdatadef getmapname getmapimage getmapcustom "
    "getmigrationstatus getplayercardinfo isofflineprofileselected coopplayer iscoop getpartystatus getsearchparams "
    "gettimeplayed isselectedplayerfriend getcharbyindex getprofiledata isprofilesignedin getwaitpopupstatus "
    "getnattype getlocalizednattype getadjustedsafeareahorizontal getadjustedsafeareavertical connectioninfo "
    "offlineprofilecansave allsplitscreenprofilescansave allsplitscreenprofilesaresignedin coopready dowehavemappack "
    "demoplaying demofollowcamera demofreecamera democapturingscreenshot gettime gamesubtypename gamedifficultyname "
    "floattostring cinematicisplaying select ischineselanguage hasaltweapon getrank getdwready isloadoutunlocked "
    "onlydefaultloadout iscoopstory iszm actionslothudiconoverride getFOGoal getFOProgress getFOPercent getFOReward "
    "getFOStatus").split()
assert len(OPS_1113) == 218


def _ops_2697():
    t = list(OPS_1113[:30]) + ["abs"] + list(OPS_1113[30:97])
    t += ["getclientname", "isclientdead", "getclientteam", "getclientteamposition", "getclientkillstreaknum",
          "getclientkillstreakstring"]
    t += list(OPS_1113[97:207]) + ["getprestige"] + list(OPS_1113[207:])
    t += ["getMarketPlaceNameFromID", "getMarketPlaceIconFromID", "canChargeAttackIconShow", "issurvival", "istrain",
          "isevac", "isdeathmarch", "hasperk", "getperktime", "getperkstartindex", "getperkstartcount",
          "getperksshow", "getEvac20sCountDownHint", "getweaponfiretype"]
    assert len(t) == 240 and t[80] == "tablelookup" and t[182] == "iscoop" and t[229] == "issurvival"
    return t


OPS_2697 = _ops_2697()
OP_1113 = {n: i for i, n in enumerate(OPS_1113)}
FONT_2697 = {11: 0, 14: 0}
# materials 2.6.9.7 menus name that no stock 1.1.1.13 zone has (docs/zone_assets ALL_material.txt), with the closest
# stock one. Used only with --stock-materials: by default a menu keeps its own material, which the zone it ships in
# must carry (common_gametype_survival, ported from 2.6.9.7, has navbar_selection_bar_focus), ahead of the menus.
MATERIAL_2697 = {"navbar_selection_bar_focus": "gradient_fadein"}   # the 1.1.1.13 menus' selection highlight

W13 = 0xA4
MENU13 = 0x190
ITEM13 = 0x17C


class MenuError(Exception):
    pass


# --- serializer (1.1.1.13 load order) --------------------------------------------------------------------------------
class Writer:
    def __init__(self):
        self.out = bytearray()

    def struct(self, node, size, kind):
        if node.get("kind") != kind:
            raise MenuError("expected %s, got %s" % (kind, node.get("kind")))
        raw = bytearray(bytes.fromhex(node["raw"]))
        if len(raw) != size:
            raise MenuError("%s is %d bytes, expected %d" % (kind, len(raw), size))
        return raw

    def emit(self, raw, ptrs, p):
        """Write the struct: every pointer field in `ptrs` (byte offsets) INLINE if p has it, else 0."""
        for o in ptrs:
            struct.pack_into("<I", raw, o, INLINE if str(o) in p else 0)
        self.out += raw

    def string(self, s):
        self.out += s.encode("latin-1") + b"\0"

    def opt_string(self, p, o):
        if str(o) in p:
            self.string(p[str(o)])

    def asset(self, ref):
        kind, name = ref["asset"], ref.get("name") or ""
        if not name.startswith(","):
            raise MenuError("%s %r is not a reference (inline %s assets are not supported)" % (kind, name, kind))
        if kind == "material":
            self.out += struct.pack("<I", INLINE) + bytes(0x5C)
        elif kind == "sound":
            self.out += struct.pack("<I", INLINE) + bytes(8)     # snd_alias_list_t {name, head, count}
        else:
            raise MenuError("asset reference of type %s" % kind)
        self.string(name)

    # MenuList (sub_4C18C0)
    def menulist(self, node):
        raw = self.struct(node, 12, "MenuList")
        p = node["p"]
        menus = p.get("8", [])
        struct.pack_into("<i", raw, 4, len(menus))
        self.emit(raw, (0, 8), p)
        self.opt_string(p, 0)
        if "8" in p:
            self.out += struct.pack("<I", INLINE) * len(menus)
            for m in menus:
                self.menu(m)

    def window(self, p):
        self.opt_string(p, 0)
        self.opt_string(p, 0x2C)
        if "160" in p:
            self.asset(p["160"])

    # menuDef_t (sub_4C12B0)
    def menu(self, node):
        W = W13
        raw = self.struct(node, MENU13, "menuDef")
        p = node["p"]
        items = p.get(str(W + 0x78), [])
        struct.pack_into("<i", raw, W + 0xC, len(items))
        ptrs = [0, 0x2C, 0xA0, W] + [W + o for o in (0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48)] + \
               [W + o for o in range(0x60, 0x7C, 4)]
        self.emit(raw, ptrs, p)
        self.window(p)
        self.opt_string(p, W)
        for o in (0x2C, 0x34, 0x30, 0x38):        # onOpen, onClose, onCloseRequest, onESC: the loader's order
            if str(W + o) in p:
                self.eventset(p[str(W + o)])
        if str(W + 0x3C) in p:
            self.keyhandler(p[str(W + 0x3C)])
        if str(W + 0x40) in p:
            self.statement(p[str(W + 0x40)])
        self.opt_string(p, W + 0x44)
        self.opt_string(p, W + 0x48)
        for o in range(0x60, 0x78, 4):
            if str(W + o) in p:
                self.statement(p[str(W + o)])
        if str(W + 0x78) in p:
            self.out += struct.pack("<I", INLINE) * len(items)
            for it in items:
                self.item(it)

    # itemDef_s (sub_4C0C90)
    def item(self, node):
        W = W13
        raw = self.struct(node, ITEM13, "itemDef")
        p = node["p"]
        itype = struct.unpack_from("<i", raw, W + 0x14)[0]
        floats = p.get(str(W + 0x9C), [])
        struct.pack_into("<i", raw, W + 0x98, len(floats))
        struct.pack_into("<I", raw, W + 0x48, 0)      # parent: set at load
        ptrs = [0, 0x2C, 0xA0, W + 0x40] + [W + o for o in range(0x4C, 0x80, 4)] + [W + 0x84, W + 0x90, W + 0x9C] + \
               [W + o for o in range(0xA0, 0xB0, 4)]
        self.emit(raw, ptrs, p)
        self.window(p)
        self.opt_string(p, W + 0x40)
        for o in range(0x4C, 0x6C, 4):
            if str(W + o) in p:
                self.eventset(p[str(W + o)])
        self.opt_string(p, W + 0x6C)
        self.opt_string(p, W + 0x70)
        if str(W + 0x74) in p:
            self.keyhandler(p[str(W + 0x74)])
        self.opt_string(p, W + 0x78)
        self.opt_string(p, W + 0x7C)
        if str(W + 0x84) in p:
            self.asset(p[str(W + 0x84)])
        td = p.get(str(W + 0x90))
        if td is not None:
            self.typedata(itype, td)
        if str(W + 0x9C) in p:
            for fe in floats:
                r = self.struct(fe, 8, "ItemFloatExpression")
                struct.pack_into("<I", r, 4, INLINE if "4" in fe["p"] else 0)
                self.out += r
            for fe in floats:
                if "4" in fe["p"]:
                    self.statement(fe["p"]["4"])
        for o in range(0xA0, 0xB0, 4):
            if str(W + o) in p:
                self.statement(p[str(W + o)])

    def typedata(self, itype, td):
        if itype == 6:
            raw = self.struct(td, 0x144, "listBoxDef")
            self.emit(raw, (0x120, 0x140), td["p"])
            if "288" in td["p"]:
                self.eventset(td["p"]["288"])
            if "320" in td["p"]:
                self.asset(td["p"]["320"])
        elif itype in (4, 9, 16, 18, 11, 14, 10, 0, 17, 22, 23):
            self.out += self.struct(td, 0x20, "editFieldDef")
        elif itype == 12:
            raw = self.struct(td, 0x188, "multiDef")
            self.emit(raw, list(range(0, 0x100, 4)), td["p"])
            for o in range(0, 0x100, 4):
                self.opt_string(td["p"], o)
        elif itype == 13:
            self.string(td)
        elif itype == 20:
            self.out += self.struct(td, 0x1C, "newsTickerDef")
        elif itype == 21:
            self.out += self.struct(td, 4, "textScrollDef")
        else:
            raise MenuError("item type %d has no typeData" % itype)

    # MenuEventHandlerSet (sub_4C0630)
    def eventset(self, node):
        raw = self.struct(node, 8, "MenuEventHandlerSet")
        hs = node["p"].get("4", [])
        struct.pack_into("<i", raw, 0, len(hs))
        self.emit(raw, (4,), node["p"])
        if "4" not in node["p"]:
            return
        self.out += b"".join(struct.pack("<I", INLINE if h is not None else 0) for h in hs)
        for h in hs:
            if h is None:
                continue
            raw = self.struct(h, 8, "MenuEventHandler")
            self.emit(raw, (0,), h["p"])
            t = raw[4]
            v = h["p"].get("0")
            if v is None:
                continue
            if t == 0:
                self.string(v)
            elif t == 1:                          # sub_4C0330: the expression first, then the handler set
                r = self.struct(v, 8, "ConditionalScript")
                self.emit(r, (0, 4), v["p"])
                if "4" in v["p"]:
                    self.statement(v["p"]["4"])
                if "0" in v["p"]:
                    self.eventset(v["p"]["0"])
            elif t == 2:
                self.eventset(v)
            elif t in (3, 4, 5, 6):
                r = self.struct(v, 8, "SetLocalVarData")
                self.emit(r, (0, 4), v["p"])
                self.opt_string(v["p"], 0)
                if "4" in v["p"]:
                    self.statement(v["p"]["4"])
            else:
                raise MenuError("event handler type %d" % t)

    # ItemKeyHandler (sub_4C0980)
    def keyhandler(self, node):
        while node is not None:
            raw = self.struct(node, 12, "ItemKeyHandler")
            self.emit(raw, (4, 8), node["p"])
            if "4" in node["p"]:
                self.eventset(node["p"]["4"])
            node = node["p"].get("8")

    # Statement_s (sub_4C0260 / sub_4C00C0)
    def statement(self, node):
        raw = self.struct(node, 20, "Statement")
        es = node["p"].get("4", [])
        struct.pack_into("<i", raw, 0, len(es))
        struct.pack_into("<I", raw, 8, 0)             # supportingData: not in the zone
        self.emit(raw, (4,), node["p"])
        if "4" not in node["p"]:
            return
        for e in es:
            r = self.struct(e, 12, "expressionEntry")
            t, dt = struct.unpack_from("<II", r, 0)
            if t != 0 and dt in (2, 3):
                struct.pack_into("<I", r, 8, INLINE if "8" in e["p"] else 0)
            self.out += r
        for e in es:
            r = bytes.fromhex(e["raw"])
            t, dt = struct.unpack_from("<II", r, 0)
            if t != 0 and "8" in e["p"]:
                if dt == 2:
                    self.string(e["p"]["8"])
                elif dt == 3:
                    self.statement(e["p"]["8"])


def encode_menulist(node) -> bytes:
    w = Writer()
    w.menulist(node)
    return bytes(w.out)


def encode_menu(node) -> bytes:
    """A menu asset (menuDef_t, type 26) on its own: the menuDef as a MenuList writes it."""
    w = Writer()
    w.menu(node)
    return bytes(w.out)


# --- reader (the zone blob -> the struct tree; the inverse of Writer) ------------------------------------------------
class Reader:
    """Reads what Writer writes: every pointer field is 0 (absent) or INLINE (its data follows in load order).
    Anything else (a back reference, an inline asset) is a MenuError: such a blob has no struct-tree source."""

    def __init__(self, data):
        self.d, self.k = bytes(data), 0

    def take(self, n):
        if self.k + n > len(self.d):
            raise MenuError("blob ends at %#x, %d more bytes wanted" % (len(self.d), n))
        b = self.d[self.k:self.k + n]
        self.k += n
        return b

    def node(self, kind, size, ptrs):
        raw = self.take(size)
        present = []
        for o in ptrs:
            v = struct.unpack_from("<I", raw, o)[0]
            if v == INLINE:
                present.append(o)
            elif v != 0:
                raise MenuError("%s +%#x: pointer %#x is not inline data" % (kind, o, v))
        return {"kind": kind, "raw": raw.hex().upper(), "p": {}}, raw, present

    def string(self):
        e = self.d.index(b"\0", self.k)
        s = self.d[self.k:e].decode("latin-1")
        self.k = e + 1
        return s

    def asset(self, kind):
        size = {"material": 0x60, "sound": 12}[kind]
        head = self.take(size)
        if struct.unpack_from("<I", head)[0] != INLINE or any(head[4:]):
            raise MenuError("an inline %s (only references are supported)" % kind)
        return {"asset": kind, "name": self.string()}

    def menulist(self):
        n, raw, have = self.node("MenuList", 12, (0, 8))
        count = struct.unpack_from("<i", raw, 4)[0]
        if 0 in have:
            n["p"]["0"] = self.string()
        if 8 in have:
            ptrs = struct.unpack("<%dI" % count, self.take(4 * count))
            if any(v != INLINE for v in ptrs):
                raise MenuError("MenuList: a menu pointer that is not inline")
            n["p"]["8"] = [self.menu() for _ in range(count)]
        return n

    def window(self, n, have):
        for o in (0, 0x2C):
            if o in have:
                n["p"][str(o)] = self.string()
        if 0xA0 in have:
            n["p"]["160"] = self.asset("material")

    def menu(self):
        W = W13
        ptrs = [0, 0x2C, 0xA0, W] + [W + o for o in (0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48)] + \
               [W + o for o in range(0x60, 0x7C, 4)]
        n, raw, have = self.node("menuDef", MENU13, ptrs)
        p = n["p"]
        self.window(n, have)
        if W in have:
            p[str(W)] = self.string()
        for o in (0x2C, 0x34, 0x30, 0x38):
            if W + o in have:
                p[str(W + o)] = self.eventset()
        if W + 0x3C in have:
            p[str(W + 0x3C)] = self.keyhandler()
        if W + 0x40 in have:
            p[str(W + 0x40)] = self.statement()
        for o in (0x44, 0x48):
            if W + o in have:
                p[str(W + o)] = self.string()
        for o in range(0x60, 0x78, 4):
            if W + o in have:
                p[str(W + o)] = self.statement()
        if W + 0x78 in have:
            count = struct.unpack_from("<i", raw, W + 0xC)[0]
            if any(v != INLINE for v in struct.unpack("<%dI" % count, self.take(4 * count))):
                raise MenuError("menuDef: an item pointer that is not inline")
            p[str(W + 0x78)] = [self.item() for _ in range(count)]
        return n

    def item(self):
        W = W13
        ptrs = [0, 0x2C, 0xA0, W + 0x40] + [W + o for o in range(0x4C, 0x80, 4)] + [W + 0x84, W + 0x90, W + 0x9C] + \
               [W + o for o in range(0xA0, 0xB0, 4)]
        n, raw, have = self.node("itemDef", ITEM13, ptrs)
        p = n["p"]
        itype = struct.unpack_from("<i", raw, W + 0x14)[0]
        self.window(n, have)
        if W + 0x40 in have:
            p[str(W + 0x40)] = self.string()
        for o in range(0x4C, 0x6C, 4):
            if W + o in have:
                p[str(W + o)] = self.eventset()
        for o in (0x6C, 0x70):
            if W + o in have:
                p[str(W + o)] = self.string()
        if W + 0x74 in have:
            p[str(W + 0x74)] = self.keyhandler()
        for o in (0x78, 0x7C):
            if W + o in have:
                p[str(W + o)] = self.string()
        if W + 0x84 in have:
            p[str(W + 0x84)] = self.asset("sound")
        if W + 0x90 in have:
            p[str(W + 0x90)] = self.typedata(itype)
        if W + 0x9C in have:
            count = struct.unpack_from("<i", raw, W + 0x98)[0]
            fes = []
            for _ in range(count):
                fe, _r, fh = self.node("ItemFloatExpression", 8, (4,))
                fes.append((fe, fh))
            for fe, fh in fes:
                if 4 in fh:
                    fe["p"]["4"] = self.statement()
            p[str(W + 0x9C)] = [fe for fe, _fh in fes]
        for o in range(0xA0, 0xB0, 4):
            if W + o in have:
                p[str(W + o)] = self.statement()
        return n

    def typedata(self, itype):
        if itype == 6:
            n, _raw, have = self.node("listBoxDef", 0x144, (0x120, 0x140))
            if 0x120 in have:
                n["p"]["288"] = self.eventset()
            if 0x140 in have:
                n["p"]["320"] = self.asset("material")
            return n
        if itype in (4, 9, 16, 18, 11, 14, 10, 0, 17, 22, 23):
            return self.node("editFieldDef", 0x20, ())[0]
        if itype == 12:
            n, _raw, have = self.node("multiDef", 0x188, tuple(range(0, 0x100, 4)))
            for o in have:
                n["p"][str(o)] = self.string()
            return n
        if itype == 13:
            return self.string()
        if itype == 20:
            return self.node("newsTickerDef", 0x1C, ())[0]
        if itype == 21:
            return self.node("textScrollDef", 4, ())[0]
        raise MenuError("item type %d has no typeData" % itype)

    def eventset(self):
        n, raw, have = self.node("MenuEventHandlerSet", 8, (4,))
        if 4 not in have:
            return n
        count = struct.unpack_from("<i", raw, 0)[0]
        ptrs = struct.unpack("<%dI" % count, self.take(4 * count))
        hs = []
        for v in ptrs:
            if v == 0:
                hs.append(None)
                continue
            if v != INLINE:
                raise MenuError("MenuEventHandlerSet: handler pointer %#x" % v)
            h, hraw, hh = self.node("MenuEventHandler", 8, (0,))
            hs.append(h)
            if 0 not in hh:
                continue
            t = hraw[4]
            if t == 0:
                h["p"]["0"] = self.string()
            elif t == 1:
                c, _cr, ch = self.node("ConditionalScript", 8, (0, 4))
                if 4 in ch:
                    c["p"]["4"] = self.statement()
                if 0 in ch:
                    c["p"]["0"] = self.eventset()
                h["p"]["0"] = c
            elif t == 2:
                h["p"]["0"] = self.eventset()
            elif t in (3, 4, 5, 6):
                s, _sr, sh = self.node("SetLocalVarData", 8, (0, 4))
                if 0 in sh:
                    s["p"]["0"] = self.string()
                if 4 in sh:
                    s["p"]["4"] = self.statement()
                h["p"]["0"] = s
            else:
                raise MenuError("event handler type %d" % t)
        n["p"]["4"] = hs
        return n

    def keyhandler(self):
        n, _raw, have = self.node("ItemKeyHandler", 12, (4, 8))
        if 4 in have:
            n["p"]["4"] = self.eventset()
        if 8 in have:
            n["p"]["8"] = self.keyhandler()
        return n

    def statement(self):
        n, raw, have = self.node("Statement", 20, (4,))
        if 4 not in have:
            return n
        count = struct.unpack_from("<i", raw, 0)[0]
        es = []
        for _ in range(count):
            es.append({"kind": "expressionEntry", "raw": self.take(12).hex().upper(), "p": {}})
        for e in es:
            t, dt, v = struct.unpack("<III", bytes.fromhex(e["raw"]))
            if t != 0 and dt in (2, 3):
                if v == INLINE:
                    e["p"]["8"] = self.string() if dt == 2 else self.statement()
                elif v != 0:
                    raise MenuError("expression operand pointer %#x" % v)
        n["p"]["4"] = es
        return n


def _finish(r, what):
    if r.k != len(r.d):
        raise MenuError("%s: %d bytes left after the asset" % (what, len(r.d) - r.k))


def decode_menulist(data):
    """A menufile (MenuList, type 25) blob -> its struct tree."""
    r = Reader(data)
    n = r.menulist()
    _finish(r, "MenuList")
    return n


def decode_menu(data):
    """A menu (menuDef_t, type 26) blob -> its struct tree."""
    r = Reader(data)
    n = r.menu()
    _finish(r, "menuDef")
    return n


def normalize(node, menu=False):
    """The tree exactly as the zone stores it: what Writer writes, read back (pointer fields, counts and the fields
    the loader sets rewritten, hex in upper case). Two trees are the same source when these are equal."""
    return decode_menu(encode_menu(node)) if menu else decode_menulist(encode_menulist(node))


# --- the converter from 2.6.9.7 ----------------------------------------------------------------------------------------
W26 = 0xA8


class Converter:
    """2.6.9.7 dump (zone-menus) -> 1.1.1.13 source. Rewrites what 1.1.1.13 lacks; `notes` lists each rewrite."""

    def __init__(self, getplayerdata_to_dvar=True, stock_materials=False):
        self.notes = []
        self.gpd = getplayerdata_to_dvar
        self.materials = MATERIAL_2697 if stock_materials else {}

    def note(self, s):
        if s not in self.notes:
            self.notes.append(s)

    def window(self, p, out, where):
        for k in ("0", "44"):
            if k in p:
                out[k] = p[k]
        if "160" in p:
            out["160"] = self.any(p["160"])
        if "164" in p:
            raise MenuError("%s: window image (GfxImage* @0xA4) has no 1.1.1.13 field" % where)

    def menulist(self, node):
        p = node["p"]
        return {"kind": "MenuList", "raw": node["raw"], "p": {"0": p["0"], "8": [self.menu(m) for m in p.get("8", [])]}}

    def menu(self, node):
        raw = bytes.fromhex(node["raw"])
        if len(raw) != 0x194:
            raise MenuError("2.6.9.7 menuDef_t is 0x194 bytes, this one %#x" % len(raw))
        new = raw[:W13] + raw[W26:]
        p = node["p"]
        out = {}
        name = p.get("0")
        self.window(p, out, "menu %s" % name)
        for k, v in p.items():
            o = int(k)
            if o < W26:
                continue
            out[str(o - 4)] = self.value(o - W26, v, "menu")
        return {"kind": "menuDef", "raw": new.hex().upper(), "p": out}

    def value(self, rel, v, owner):
        """A menu field's pointee at offset W+rel."""
        if owner == "menu" and rel == 0x78:
            return [self.item(i) for i in v]
        return self.any(v)

    def item(self, node):
        raw = bytearray(bytes.fromhex(node["raw"]))
        if len(raw) != 0x188:
            raise MenuError("2.6.9.7 itemDef_s is 0x188 bytes, this one %#x" % len(raw))
        p = node["p"]
        where = "item %s" % p.get("0")
        font = struct.unpack_from("<i", raw, W26 + 0x20)[0]
        if font in FONT_2697:
            struct.pack_into("<i", raw, W26 + 0x20, FONT_2697[font])
            self.note("fontEnum %d -> %d (1.1.1.13 has no font %d; 0 picks one by scale)" % (font, FONT_2697[font], font))
        s = W26 + 0xA0
        stmts = raw[s:s + 24]
        new = bytes(raw[:W13]) + bytes(raw[W26:s]) + stmts[0:12] + stmts[16:20] + bytes(raw[s + 24:])
        assert len(new) == ITEM13
        out = {}
        self.window(p, out, where)
        for k, v in p.items():
            o = int(k)
            if o < W26:
                continue
            rel = o - W26
            if 0xA0 <= rel < 0xB8:
                slot = (rel - 0xA0) // 4
                if slot in (3, 5):
                    raise MenuError("%s: statement slot %d has no 1.1.1.13 field" % (where, slot))
                slot = {0: 0, 1: 1, 2: 2, 4: 3}[slot]
                out[str(W13 + 0xA0 + 4 * slot)] = self.any(v)
                continue
            out[str(W13 + rel)] = self.any(v)
        return {"kind": "itemDef", "raw": new.hex().upper(), "p": out}

    def any(self, v):
        if isinstance(v, list):
            return [self.any(x) for x in v]
        if not isinstance(v, dict) or "asset" in v:
            if isinstance(v, dict) and "asset" in v:
                n = v.get("name") or ""
                if v["asset"] == "material" and n.lstrip(",") in self.materials:
                    m = self.materials[n.lstrip(",")]
                    self.note("material %s -> ,%s (not in 1.1.1.13)" % (n, m))
                    return {"asset": "material", "name": "," + m}
                if not n.startswith(","):
                    self.note("%s %s: inline in the 2.6.9.7 zone, a reference here (the zone must carry it)"
                              % (v["asset"], n))
                    return {"asset": v["asset"], "name": "," + n}
            return v
        k = v["kind"]
        if k == "Statement":
            return self.statement(v)
        if k == "MenuEventHandlerSet":
            return self.eventset(v)
        return {"kind": k, "raw": v["raw"], "p": {kk: self.any(x) for kk, x in v["p"].items()}}

    def statement(self, v):
        es = []
        for e in v["p"].get("4", []):
            r = bytearray(bytes.fromhex(e["raw"]))
            t, a = struct.unpack_from("<II", r, 0)
            ep = dict(e["p"])
            if t == 0:
                name = OPS_2697[a] if a < len(OPS_2697) else None
                if name == "getplayerdata" and self.gpd:
                    name = "dvarint"
                    self.note("getplayerdata(<field>) -> dvarint(<field>): 1.1.1.13's playerdata has no survival "
                              "fields; the server sets them with setClientDvar")
                if name not in OP_1113:
                    raise MenuError("operator %s (%s) has no 1.1.1.13 equivalent" % (a, name))
                struct.pack_into("<I", r, 4, OP_1113[name])
            elif a == 3 and "8" in ep:
                ep["8"] = self.statement(ep["8"])
            es.append({"kind": "expressionEntry", "raw": r.hex().upper(), "p": ep})
        out = {"kind": "Statement", "raw": v["raw"], "p": {}}
        if "4" in v["p"]:
            out["p"]["4"] = es
        return out

    def eventset(self, v):
        hs = []
        last_purchase = None
        for h in v["p"].get("4", []):
            if h is None:
                hs.append(None)
                continue
            raw = bytes.fromhex(h["raw"])
            t = raw[4]
            d = h["p"].get("0")
            if t == 0 and isinstance(d, str):
                d = self.script(d, last_purchase)
            elif t == 6 and d is not None and d["p"].get("0") == "purchase_item":
                last_purchase = constant_string(d["p"].get("4"))
            nd = d if (t == 0 or d is None) else self.any(d)
            hs.append({"kind": "MenuEventHandler", "raw": h["raw"], "p": {} if nd is None else {"0": nd}})
        out = {"kind": "MenuEventHandlerSet", "raw": v["raw"], "p": {}}
        if "4" in v["p"]:
            out["p"]["4"] = hs
        return out

    def script(self, s, purchase):
        dyn = '"scriptmenuresponse" "localVarString" ( "purchase_item" )'
        if dyn in s:
            if purchase is None:
                raise MenuError("scriptmenuresponse localVarString(purchase_item) without a constant purchase_item")
            s = s.replace(dyn, '"scriptmenuresponse" "%s"' % purchase)
            self.note('scriptmenuresponse localVarString("purchase_item") -> the constant it holds there '
                      '("purchase_<kiosks.csv index>")')
        return s


def constant_string(stmt):
    """The value of a constant expression ( "a" + 3 ) as 2.6.9.7 stores it, else None."""
    if stmt is None:
        return None
    parts = []
    for e in stmt["p"].get("4", []):
        t, a, b = struct.unpack_from("<IIi", bytes.fromhex(e["raw"]))
        if t == 0:
            if OPS_2697[a] in ("(", ")", "+"):
                continue
            return None
        if a == 0:
            parts.append(str(b))
        elif a == 2:
            parts.append(e["p"].get("8", ""))
        else:
            return None
    return "".join(parts)


# --- listing ---------------------------------------------------------------------------------------------------------
def expr(st, ops=OPS_1113):
    out = []
    for e in st["p"].get("4", []):
        r = bytes.fromhex(e["raw"])
        t, a, b = struct.unpack_from("<IIi", r)
        if t == 0:
            out.append(ops[a] if a < len(ops) else "op%d" % a)
        elif a == 0:
            out.append(str(b))
        elif a == 1:
            out.append("%g" % struct.unpack_from("<f", r, 8)[0])
        elif a == 2:
            out.append(json.dumps(e["p"].get("8")))
        elif a == 3:
            out.append("{" + expr(e["p"]["8"], ops) + "}")
    return " ".join(out)


EVENTS = ["mouseEnterText", "mouseExitText", "mouseEnter", "mouseExit", "action", "accept", "onFocus", "leaveFocus"]


def listing(node, W=W13, ops=OPS_1113):
    L = []

    def hs(s, ind):
        for h in (s or {"p": {}})["p"].get("4", []):
            if h is None:
                continue
            t = bytes.fromhex(h["raw"])[4]
            v = h["p"].get("0")
            if t == 0:
                L.append(ind + "script " + (v or ""))
            elif t == 1:
                L.append(ind + "if " + expr(v["p"]["4"], ops) + ":")
                hs(v["p"].get("0"), ind + "    ")
            elif t == 2:
                L.append(ind + "else:")
                hs(v, ind + "    ")
            else:
                L.append(ind + "setLocalVar%s %s = %s" % ({3: "Bool", 4: "Int", 5: "Float", 6: "String"}[t],
                                                         v["p"].get("0"), expr(v["p"]["4"], ops) if "4" in v["p"] else ""))

    def win(n, ind):
        raw = bytes.fromhex(n["raw"])
        x, y, w, h = struct.unpack_from("<4f", raw, 4)
        L.append(ind + "name %r group %r rect %g %g %g %g align %d %d style %d ownerdraw %d static %#x dynamic %#x" % (
            n["p"].get("0"), n["p"].get("44"), x, y, w, h, raw[0x14], raw[0x15], struct.unpack_from("<i", raw, 0x30)[0],
            struct.unpack_from("<i", raw, 0x38)[0], struct.unpack_from("<I", raw, 0x44)[0],
            struct.unpack_from("<I", raw, 0x48)[0]))
        L.append(ind + "forecolor %s backcolor %s" % ("%g %g %g %g" % struct.unpack_from("<4f", raw, 0x50),
                                                      "%g %g %g %g" % struct.unpack_from("<4f", raw, 0x60)))
        if "160" in n["p"]:
            L.append(ind + "background %s" % n["p"]["160"].get("name"))

    L.append("menufile %s" % node["p"].get("0"))
    for m in node["p"].get("8", []):
        p = m["p"]
        L.append("")
        L.append("menu")
        win(m, "  ")
        for o, n in ((0x2C, "onOpen"), (0x30, "onCloseRequest"), (0x34, "onClose"), (0x38, "onESC")):
            if str(W + o) in p:
                L.append("  " + n + ":")
                hs(p[str(W + o)], "    ")
        k = p.get(str(W + 0x3C))
        while k is not None:
            L.append("  onKey %d:" % struct.unpack_from("<i", bytes.fromhex(k["raw"]))[0])
            hs(k["p"].get("4"), "    ")
            k = k["p"].get("8")
        if str(W + 0x40) in p:
            L.append("  visible when " + expr(p[str(W + 0x40)], ops))
        for i, n in enumerate(("rectX", "rectY", "rectW", "rectH", "openSound", "closeSound")):
            if str(W + 0x60 + 4 * i) in p:
                L.append("  exp %s %s" % (n, expr(p[str(W + 0x60 + 4 * i)], ops)))
        for it in p.get(str(W + 0x78), []):
            ip = it["p"]
            r = bytes.fromhex(it["raw"])
            L.append("  item type %d font %d textscale %g textstyle %d align %d" % (
                struct.unpack_from("<i", r, W + 0x14)[0], struct.unpack_from("<i", r, W + 0x20)[0],
                struct.unpack_from("<f", r, W + 0x30)[0], struct.unpack_from("<i", r, W + 0x34)[0],
                struct.unpack_from("<i", r, W + 0x24)[0]))
            win(it, "    ")
            if str(W + 0x40) in ip:
                L.append("    text %r" % ip[str(W + 0x40)])
            for i, n in enumerate(EVENTS):
                if str(W + 0x4C + 4 * i) in ip:
                    L.append("    " + n + ":")
                    hs(ip[str(W + 0x4C + 4 * i)], "      ")
            for o, n in ((0x6C, "dvar"), (0x70, "dvarTest"), (0x78, "enableDvar"), (0x7C, "localVar")):
                if str(W + o) in ip:
                    L.append("    %s %r" % (n, ip[str(W + o)]))
            for fe in ip.get(str(W + 0x9C), []):
                L.append("    exp float[%d] %s" % (struct.unpack_from("<i", bytes.fromhex(fe["raw"]))[0],
                                                   expr(fe["p"]["4"], ops)))
            for i, n in enumerate(("visible when", "disabled when", "exp text", "exp material")):
                if str(W + 0xA0 + 4 * i) in ip:
                    L.append("    %s %s" % (n, expr(ip[str(W + 0xA0 + 4 * i)], ops)))
    return "\n".join(L) + "\n"


def menus_of(dump, names):
    """The menufiles of a zone-menus dump whose name is ui_mp/scriptmenus/<name>.menu, in `names` order."""
    by = {a["value"]["p"].get("0"): a["value"] for a in dump["assets"] if a["type"] == "menufile"}
    return [by["ui_mp/scriptmenus/%s.menu" % n] for n in names]


def main(argv=None):
    import argparse
    import os
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("convert", help="2.6.9.7 zone-menus dump -> 1.1.1.13 .menu sources (<out>/<name>.menu)")
    c.add_argument("--dump", required=True)
    c.add_argument("--out", required=True)
    c.add_argument("--stock-materials", action="store_true",
                   help="replace the materials no stock 1.1.1.13 zone has (MATERIAL_2697) instead of referencing them")
    c.add_argument("names", nargs="+")
    li = sub.add_parser("list", help="print the listing of a struct tree (.json dump, or a menufile .bin blob)")
    li.add_argument("sources", nargs="+")
    a = ap.parse_args(argv)
    if a.cmd == "convert":
        with open(a.dump, encoding="utf-8") as f:
            dump = json.load(f)
        if dump.get("build") != "2.6.9.7":
            raise SystemExit("%s is a %s dump; the converter reads 2.6.9.7 menus" % (a.dump, dump.get("build")))
        os.makedirs(a.out, exist_ok=True)
        for name, ml in zip(a.names, menus_of(dump, a.names)):
            from zonekit import menu_source
            cv = Converter(stock_materials=a.stock_materials)
            node = cv.menulist(ml)
            encode_menulist(node)
            header = [node["p"]["0"] + ": 2.6.9.7's menu, converted to 1.1.1.13 by `python -m zonekit.menus convert`",
                      "(rewrites: %s). IW menu language: zonekit/menu_source.py." % ("; ".join(cv.notes) or "none")]
            with open(os.path.join(a.out, name + ".menu"), "w", encoding="utf-8", newline="\n") as f:
                f.write(menu_source.decompile_menufile(node, header))
            print("%s: %s" % (name, "; ".join(cv.notes) or "no rewrites"))
    else:
        for s in a.sources:
            if s.lower().endswith(".bin"):
                with open(s, "rb") as f:
                    node = decode_menulist(f.read())
            else:
                with open(s, encoding="utf-8") as f:
                    node = json.load(f)
            print(listing(node), end="")


if __name__ == "__main__":
    main()
