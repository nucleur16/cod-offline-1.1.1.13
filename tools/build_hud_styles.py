# -*- coding: utf-8 -*-
r"""Selectable HUD styles: the native HUD menus of later Call of Duty Online builds, gated on k33_hudStyle.

The combat HUD of every CODOL build is native menus of ui_mp/hud_720.txt (menu assets, type 26). A style is the items
of a later build's HUD menus converted to the 1.1.1.13 layout and appended to 1.1.1.13's own menus of the same name:
the stock items stay and show while k33_hudStyle is not a shipped style, a style's items show while k33_hudStyle is
that style's number. Menu expressions are evaluated every frame, so the HUD changes the moment the dvar does.

    python tools/build_hud_styles.py                 sources -> zones/hud_styles_postgfx_mp (menufile, materials,
                                                     zone_source csv) and the survival HUD menufile (which owns
                                                     killstreaks_hd and scorebar_hd: one copy of a menu, both gates)
    python tools/build_hud_styles.py extract --dumper <CodolSoundDumper.exe with zone-menus> [--c1 DIR] [--v26 DIR]
                                             [--v12 DIR] [--work DIR]
                                                     later installs -> zones/hud_styles_postgfx_mp/sources + image/
                                                     (read only on the installs; --work holds the dumps)

Style numbers are fixed (hud_style.c, the menus): 0 stock, 1 v12 (1.2.1.12), 2 v26 (2.6.9.7), 3 v33, 4 v35. A style
ships when its sources are checked in (SHIPPED below); hud_style.c publishes the same list in k33_hudStyleList.

What a style's item needs to be kept (anything else is dropped and listed in menufile/hud_styles.report.txt):
  * every expression operator exists in 1.1.1.13 (operator tables: zonekit/menus.py; 1.2.1.12's is the first 232
    names of 2.6.9.7's, MEASURED in its codoMP_client_shipRetail.exe at 0x00D01690). istrain / isevac are modes
    1.1.1.13 does not have (constant 0); issurvival is dvarstring("g_gametype") == "survival", Kernel33's Survival;
  * player("<field>") names a field of 1.1.1.13's operator (MEASURED: C1.exe 0x5DE844..0x5DED9A compares the name
    with teamname otherteamname dead clipAmmo clipAmmo_left stockAmmo fragAmmo smokeAmmo tactAmmo nightvision
    killstreak killstreakTarget killstreakID0..4 killstreakTarget0..4 score deaths kills ping);
  * its ownerDraw number is one a stock 1.1.1.13 HUD menu uses (the numbers agree wherever both builds name the same
    item: offhandFragIcon 103, lowammowarning 120, minimap_map 159, ...; a number 1.1.1.13 never uses is unknown);
  * no 2.6.9.7-only field (window image, statement slots 3 and 5);
  * its "@KEY" texts exist in 1.1.1.13 or are carried by codol_{english,chinese}_hud_styles;
  * its materials exist in 1.1.1.13 under that name with the same pixels, or are carried by this zone as
    k33hud<NN>_<name> (image + a `2d` material in C1's hud_scorebar_topbar layout).
Fonts 11, 12 and 14 do not exist in 1.1.1.13 (UI_GetFontHandle 0x005EC790 knows 3..10): 0, which picks by scale.
"""
from __future__ import annotations

import copy
import json
import os
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from zonekit import menus  # noqa: E402

ZONE = "hud_styles_postgfx_mp"
ZDIR = ROOT / "zones" / ZONE
SRC = ZDIR / "sources"
MENUFILE = "ui_mp/scriptmenus/hud_styles.menu"
DVAR = "k33_hudStyle"
STYLE_NUMBERS = {"stock": 0, "v12": 1, "v26": 2, "v33": 3, "v35": 4}
SURVIVAL_DIR = ROOT / "zones/survival_hud_postgfx_mp/menufile"
LOCALE_ZONES = {"english": "codol_english_hud_styles", "chinese": "codol_chinese_hud_styles"}

W13 = menus.W13
OP = menus.OP_1113
# 1.1.1.13's player() fields: MEASURED, the names C1.exe 0x5DE844..0x5DED9A compares its argument with
C1_PLAYER_FIELDS = frozenset(
    "teamname otherteamname dead clipAmmo clipAmmo_left stockAmmo fragAmmo smokeAmmo tactAmmo nightvision killstreak "
    "killstreakTarget killstreakID0 killstreakID1 killstreakID2 killstreakID3 killstreakID4 killstreakTarget0 "
    "killstreakTarget1 killstreakTarget2 killstreakTarget3 killstreakTarget4 score deaths kills ping".split())
FONTS = {11: 0, 12: 0, 14: 0}
MODE_CONSTANTS = {"istrain": 0, "isevac": 0}       # modes 1.1.1.13 does not have
# scorebar_hd's onOpen in the later builds: if ( dvarstring("ui_gametype") == "combat" ) isCombat = 1 else 0. The
# host keeps 1.1.1.13's menu (no onOpen), so the items read the dvar instead of the local variable.
LOCALVARS = {"isCombat": ("ui_gametype", "combat")}

# A style: the later build, its menu layout and operator table, and which of its HUD menus land in which 1.1.1.13
# menu ("host"). The first source of a host is the later build's menu of the same name (its visibility replaces the
# host's while the style is on); the others are menus 1.1.1.13 does not register, so their items join the host with
# their own menu's visibility and origin.
STYLES = {
    "v26": dict(build="2.6.9.7", window=0xA8, ops=menus.OPS_2697, zone="common_mp", prefix="k33hud26_",
                hosts={"weaponbar_hd": ["weaponbar_hd", "weaponbar_hd_extend", "reloadbar"],
                       "scorebar_hd": ["scorebar_hd", "kd_rate_bar"],
                       "killstreaks_hd": ["killstreaks_hd"],
                       "minimap_fullscreen": ["minimap_fullscreen"]}),
    "v12": dict(build="1.2.1.12", window=0xA4, ops=menus.OPS_2697[:232], zone="common_mp", prefix="k33hud12_",
                hosts={"weaponbar_hd": ["weaponbar_hd", "weaponbar_hd_extend", "reloadbar"],
                       "scorebar_hd": ["scorebar_hd", "kd_rate_bar"],
                       "killstreaks_hd": ["killstreaks_hd"],
                       "minimap_fullscreen": ["minimap_fullscreen"]}),
}
# Items left out on purpose, whatever the checks say (the reason goes to the report):
#  * the later builds' Survival / Evac panels inside scorebar_hd. Evac does not exist in 1.1.1.13, and Kernel33's
#    Survival panel is the survival HUD zone's own component, shown in every style;
#  * a label or plate whose value 1.1.1.13 cannot produce (player("assists") / player("revives") are not fields of
#    its operator, getweaponfiretype is not an operator), so no caption or box is left without its content.
MODE_ONLY = re.compile(r"^\( (\( )?(isevac|issurvival) \)( \|\| (isevac|issurvival) \) \))? &&"
                       r'|^\( dvarint\(static\) "so_survival" \) &&')
DROP_MATERIAL = {"fire_type_bg": "the plate behind the fire-mode text, getweaponfiretype(), which 1.1.1.13 has no "
                                 "operator for"}
DROP_TEXT = {"@MPUI_ASSIST_CAP": 'the label of player("assists"), which 1.1.1.13 has no field for',
             "@MPUI_SCORE_R": 'the label of player("revives"), which 1.1.1.13 has no field for'}

# the hosts this zone carries (1.1.1.13's menu, from sources/menus_stock.json); the survival HUD zone carries the
# other two, with both gates (fold_survival)
OWN_HOSTS = ("weaponbar_hd", "minimap_fullscreen")
SURVIVAL_HOSTS = ("killstreaks_hd", "scorebar_hd")


def shipped():
    """The styles whose converted sources are checked in, in number order: [(number, id)]."""
    return sorted((STYLE_NUMBERS[s], s) for s in STYLES if (SRC / ("menus_%s.json" % s)).is_file())


def style_list():
    """k33_hudStyleList's value (hud_style.c HUD_STYLE_LIST)."""
    return ";".join("%d:%s" % (n, s) for n, s in [(0, "stock")] + shipped())


# --- expressions -------------------------------------------------------------------------------------------------
def op(name):
    return {"kind": "expressionEntry", "raw": struct.pack("<III", 0, OP[name], 0).hex().upper(), "p": {}}


def string(value):
    return {"kind": "expressionEntry", "raw": struct.pack("<III", 1, 2, 0).hex().upper(), "p": {"8": value}}


def integer(value):
    return {"kind": "expressionEntry", "raw": struct.pack("<IIi", 1, 0, value).hex().upper(), "p": {}}


def statement(entries):
    return {"kind": "Statement", "raw": struct.pack("<IIIII", len(entries), 0, 0, 0, 0).hex().upper(),
            "p": {"4": entries}}


def nested(stmt):
    """A whole statement as one operand (dataType 3, what the stock menus' {..} sub-expressions are): it is evaluated
    on its own, so nothing here depends on how the wrapped expression balances its parentheses."""
    return {"kind": "expressionEntry", "raw": struct.pack("<III", 1, 3, 0).hex().upper(), "p": {"8": copy.deepcopy(stmt)}}


def _style_is(n):
    return [op("dvarint"), string(DVAR), op(")"), op("=="), integer(n)]


def _style_is_stock(numbers):
    out = []
    for n in numbers:
        out += ([op("&&")] if out else []) + [op("dvarint"), string(DVAR), op(")"), op("!="), integer(n)]
    return out


def _not_survival():
    return [op("dvarstring"), string("g_gametype"), op(")"), op("!="), string("survival")]


def gate(cond, old):
    """( <cond> && {old} ), written like the stock expressions: one opening parenthesis, the last one left out."""
    return statement([op("(")] + cond + ([op("&&"), nested(old)] if old is not None else []))


def gate_style(n, old, survival=False):
    return gate(_style_is(n) + ([op("&&")] + _not_survival() if survival else []), old)


def gate_stock(numbers, old):
    return gate(_style_is_stock(numbers), old) if numbers else old


def menu_visible(numbers, stock, styles):
    """( ( <stock style> && {stock} ) || ( <style n> && {styles[n]} ) .. ) for a menu several styles share."""
    if not numbers:
        return stock
    out = [op("("), op("(")] + _style_is_stock(numbers) + ([op("&&"), nested(stock)] if stock is not None else [])
    out.append(op(")"))
    for n in numbers:
        st = styles.get(n)
        out += [op("||"), op("(")] + _style_is(n) + ([op("&&"), nested(st)] if st is not None else []) + [op(")")]
    return statement(out)


# --- conversion of a later build's menu ----------------------------------------------------------------------------
class Dropped(Exception):
    pass


class StyleConverter(menus.Converter):
    """A later build's item -> the 1.1.1.13 layout, refusing (Dropped) what 1.1.1.13 cannot express."""

    def __init__(self, style, facts):
        super().__init__(getplayerdata_to_dvar=False)
        self.cfg = STYLES[style]
        self.ops = self.cfg["ops"]
        self.facts = facts
        self.materials = {}
        self.calls = {tuple(c) for c in facts["c1_calls"]}
        self.keys = set()          # "@KEY" texts the kept items use
        self.used_materials = set()
        self.tables = set()

    # menus.Converter.item is the 2.6.9.7 (0xA8, six statements) reader
    def item(self, node):
        try:
            if self.cfg["window"] == 0xA8:
                out = super().item(node)
            else:
                raw = bytes.fromhex(node["raw"])
                if len(raw) != menus.ITEM13:
                    raise Dropped("itemDef_s of %#x bytes" % len(raw))
                out = {"kind": "itemDef", "raw": node["raw"], "p": {k: self.any(v) for k, v in node["p"].items()}}
        except menus.MenuError as e:
            raise Dropped(str(e))
        raw = bytearray(bytes.fromhex(out["raw"]))
        font = struct.unpack_from("<i", raw, W13 + 0x20)[0]
        if font in FONTS:
            struct.pack_into("<i", raw, W13 + 0x20, FONTS[font])
            self.note("fontEnum %d -> 0 (1.1.1.13 has no font %d; 0 picks one by scale)" % (font, font))
        out["raw"] = raw.hex().upper()
        return out

    def menu_statement(self, v):
        try:
            return self.statement(v)
        except menus.MenuError as e:
            raise Dropped(str(e))

    def statement(self, v):
        src = v["p"].get("4", [])
        es = []
        k = 0
        while k < len(src):
            e = src[k]
            r = bytearray(bytes.fromhex(e["raw"]))
            t, a = struct.unpack_from("<II", r, 0)
            ep = dict(e["p"])
            k += 1
            if t != 0:
                if a == 3 and "8" in ep:
                    ep["8"] = self.statement(ep["8"])
                es.append({"kind": "expressionEntry", "raw": r.hex().upper(), "p": ep})
                continue
            name = self.ops[a] if a < len(self.ops) else "#%d (not in the %s table)" % (a, self.cfg["build"])
            if name in MODE_CONSTANTS or name == "issurvival":
                # a function without arguments: the operator, then its ")" (which the last entry of a statement omits)
                closed = k < len(src) and _is_op(src[k], self.ops, ")")
                if not closed and k != len(src):
                    raise menus.MenuError("%s is not followed by )" % name)
                k += closed
                if name in MODE_CONSTANTS:
                    es.append(integer(MODE_CONSTANTS[name]))
                    self.note("%s -> %d (a mode 1.1.1.13 does not have)" % (name, MODE_CONSTANTS[name]))
                else:
                    es += [op("("), op("dvarstring"), string("g_gametype"), op(")"), op("=="), string("survival")]
                    es += [op(")")] if closed else []
                    self.note('issurvival -> dvarstring("g_gametype") == "survival"')
                continue
            if name not in OP:
                raise menus.MenuError("operator %s has no 1.1.1.13 equivalent" % name)
            arg = src[k]["p"].get("8") if k < len(src) and _is_string(src[k]) else None
            if name == "localvarint" and arg in LOCALVARS and k + 1 < len(src) and _is_op(src[k + 1], self.ops, ")"):
                dvar, value = LOCALVARS[arg]
                es += [op("("), op("dvarstring"), string(dvar), op(")"), op("=="), string(value), op(")")]
                self.note('localvarint("%s") -> dvarstring("%s") == "%s" (what the later onOpen stores in it)'
                          % (arg, dvar, value))
                k += 2
                continue
            if name == "player" and arg not in C1_PLAYER_FIELDS:
                raise menus.MenuError('player("%s"): not a field of 1.1.1.13\'s operator' % arg)
            if name in ("getplayerdata", "stat", "getperk", "tablelookup", "tablelookupbyrow", "tablegetrownum",
                        "localvarint", "localvarbool", "localvarfloat", "localvarstring") and \
                    (name, arg) not in self.calls:
                raise menus.MenuError('%s("%s"): no stock 1.1.1.13 HUD menu makes this call' % (name, arg))
            struct.pack_into("<I", r, 4, OP[name])
            es.append({"kind": "expressionEntry", "raw": r.hex().upper(), "p": ep})
        out = {"kind": "Statement", "raw": v["raw"], "p": {}}
        if "4" in v["p"]:
            out["p"]["4"] = es
            raw = bytearray(bytes.fromhex(v["raw"]))
            struct.pack_into("<I", raw, 0, len(es))
            out["raw"] = raw.hex().upper()
        return out


def _is_op(e, ops, name):
    t, a = struct.unpack_from("<II", bytes.fromhex(e["raw"]))
    return t == 0 and a < len(ops) and ops[a] == name


def _is_string(e):
    t, a = struct.unpack_from("<II", bytes.fromhex(e["raw"]))
    return t == 1 and a == 2


def walk_strings(v, out):
    """Every string a struct tree holds: (kind of holder, value)."""
    if isinstance(v, list):
        for x in v:
            walk_strings(x, out)
    elif isinstance(v, dict):
        if "asset" in v:
            return
        for x in v.get("p", {}).values():
            if isinstance(x, str):
                out.append((v.get("kind"), x))
            else:
                walk_strings(x, out)


def walk_ops(v, out):
    if isinstance(v, list):
        for x in v:
            walk_ops(x, out)
    elif isinstance(v, dict) and "asset" not in v:
        if v.get("kind") == "expressionEntry":
            t, a = struct.unpack_from("<II", bytes.fromhex(v["raw"]))
            if t == 0:
                out.add(a)
        for x in v.get("p", {}).values():
            walk_ops(x, out)


def loc_keys(item):
    """The localize keys an item shows: its text when it starts with "@", and every "@KEY" string operand."""
    found = []
    walk_strings(item, found)
    return sorted({s[1:] for _k, s in found if s.startswith("@") and re.fullmatch(r"@[A-Z0-9_]+", s)})


def material_strings(item):
    """The constant strings of an item's material expression that name a material (not a function's argument)."""
    st = item["p"].get(str(W13 + 0xAC))
    out = []

    def scan(s):
        es = s["p"].get("4", [])
        for i, e in enumerate(es):
            t, a = struct.unpack_from("<II", bytes.fromhex(e["raw"]))
            if t == 1 and a == 3 and "8" in e["p"]:
                scan(e["p"]["8"])
            elif t == 1 and a == 2:
                prev = struct.unpack_from("<II", bytes.fromhex(es[i - 1]["raw"])) if i else (0, OP["("])
                if not (prev[0] == 0 and prev[1] >= OP["dvarint(static)"]):      # not a function's first argument
                    out.append(e)
    if st is not None:
        scan(st)
    return out


def table_lookups(node, out):
    """(table, search column, result column) of every tablelookup( "<table>", <int>, .., <int> ) of a struct tree."""
    if isinstance(node, list):
        for x in node:
            table_lookups(x, out)
    elif isinstance(node, dict) and "asset" not in node:
        if node.get("kind") == "Statement":
            es = node["p"].get("4", [])
            for i, e in enumerate(es):
                if not _is_op(e, menus.OPS_1113, "tablelookup") or i + 1 >= len(es) or not _is_string(es[i + 1]):
                    continue
                args, depth, k = [[]], 1, i + 1
                while k < len(es) and depth:
                    t, a, b = struct.unpack_from("<IIi", bytes.fromhex(es[k]["raw"]))
                    if t == 0 and a == OP[")"]:
                        depth -= 1
                    elif t == 0 and (a == OP["("] or a >= OP["dvarint(static)"]):
                        depth += 1
                    if t == 0 and a == OP[","] and depth == 1:
                        args.append([])
                    elif depth:
                        args[-1].append((t, a, b))
                    k += 1
                if len(args) == 4 and all(len(x) == 1 and x[0][:2] == (1, 0) for x in (args[1], args[3])):
                    out.add((es[i + 1]["p"]["8"], args[1][0][2], args[3][0][2]))
        for x in node.get("p", {}).values():
            table_lookups(x, out)


def check_item(conv, style, src_menu, item, owner_draws):
    """Raises Dropped for an item 1.1.1.13 would not draw as the later build does; renames carried materials."""
    facts, st = conv.facts, conv.facts["styles"][style]
    raw = bytes.fromhex(item["raw"])
    od = struct.unpack_from("<i", raw, 0x38)[0]
    if od and od not in owner_draws:
        raise Dropped("ownerDraw %d: no stock 1.1.1.13 HUD menu uses it" % od)
    refs = [item["p"]["160"]] if "160" in item["p"] else []
    for ref in refs + material_strings(item):
        name = (ref["name"] if "asset" in ref else ref["p"]["8"]).lstrip(",")
        how = st["materials"].get(name)
        if how is None:
            raise Dropped("material %s: not resolved (sources/facts.json)" % name)
        if how["action"] == "carry":
            new = how["asset"]
            conv.used_materials.add(new)
        elif how["action"] == "stock":
            new = name
        else:
            raise Dropped("material %s: %s" % (name, how["why"]))
        if "asset" in ref:
            ref["name"] = "," + new
        else:
            ref["p"]["8"] = new
    found = set()
    table_lookups(item, found)
    for table, search, result in sorted(found):
        same = st["tables"].get("%s,%d,%d" % (table, search, result))
        if not same or same["differ"]:
            raise Dropped("tablelookup(%s, %d, .., %d): 1.1.1.13's table is not known to answer as %s's" % (
                table, search, result, STYLES[style]["build"]))
        conv.tables.add((table, search, result))
    for key in loc_keys(item):
        if not facts["c1_localize"].get(key) and key not in facts["localize"] and key not in facts["c1_menu_keys"]:
            raise Dropped("text @%s: not in 1.1.1.13 and not carried" % key)
        conv.keys.add(key)
    return item


def shift(item, dx, dy):
    """Move an item by the difference between its own menu's origin and the host's (rect and rectClient)."""
    if not dx and not dy:
        return
    for fe in item["p"].get(str(W13 + 0x9C), []):
        if struct.unpack_from("<i", bytes.fromhex(fe["raw"]))[0] in (0, 1):
            raise Dropped("its position is an expression, and its menu's origin differs from the host's")
    raw = bytearray(bytes.fromhex(item["raw"]))
    for o in (4, 0x18):
        x, y = struct.unpack_from("<2f", raw, o)
        struct.pack_into("<2f", raw, o, x + dx, y + dy)
    item["raw"] = raw.hex().upper()


def menu_origin(raw):
    return struct.unpack_from("<4f", raw, 4) + (raw[0x14], raw[0x15])


def style_parts(style, facts=None, sources=None, stock=None):
    """-> ({host: {"visible": statement or None, "items": [item, ..]}}, report lines, converter).
    The items are in the 1.1.1.13 layout, ungated, with their foreign menu's visibility already folded in."""
    cfg = STYLES[style]
    facts = facts or load_facts()
    sources = sources or json.loads((SRC / ("menus_%s.json" % style)).read_text(encoding="utf-8"))
    stock = stock or stock_menus()
    conv = StyleConverter(style, facts)
    owner_draws = set(facts["c1_ownerdraws"])
    W = cfg["window"]
    report = []
    out = {}
    for host, names in cfg["hosts"].items():
        host_raw = bytes.fromhex(stock[host]["raw"])
        hx, hy, hw, hh, ha, hv = menu_origin(host_raw)
        part = dict(visible=None, items=[], own=[])
        for i, name in enumerate(names):
            m = sources["menus"].get(name)
            if m is None:
                report.append("%s %s: not in %s" % (style, name, cfg["build"]))
                continue
            raw = bytes.fromhex(m["raw"])
            mx, my, mw, mh, ma, mv = menu_origin(raw)
            vis = m["p"].get(str(W + 0x40))
            try:
                vis = conv.menu_statement(vis) if vis is not None else None
                if (mw, mh, ma, mv) != (hw, hh, ha, hv):
                    raise Dropped("its rect %gx%g align %d %d is not the host's" % (mw, mh, ma, mv))
            except Dropped as e:
                report.append("%s %s -> %s: menu dropped: %s" % (style, name, host, e))
                continue
            if i == 0:
                part["visible"] = vis
            kept = 0
            items = m["p"].get(str(W + 0x78), [])
            for n, it in enumerate(items):
                label = "%s %s[%d]%s" % (style, name, n, " %s" % it["p"]["0"] if it["p"].get("0") else "")
                try:
                    shown = it["p"].get(str(W + 0xA0))
                    if host == "scorebar_hd" and shown is not None and MODE_ONLY.match(menus.expr(shown, cfg["ops"])):
                        raise Dropped("a Survival / Evac item of %s (1.1.1.13: the survival HUD's own panel)" % cfg["build"])
                    if it["p"].get(str(W + 0x40)) in DROP_TEXT:
                        raise Dropped(DROP_TEXT[it["p"][str(W + 0x40)]])
                    plate = (it["p"].get("160") or {}).get("name", "").lstrip(",")
                    if plate in DROP_MATERIAL:
                        raise Dropped(DROP_MATERIAL[plate])
                    new = check_item(conv, style, name, conv.item(it), owner_draws)
                    shift(new, mx - hx, my - hy)
                except Dropped as e:
                    report.append("%s -> %s: dropped: %s" % (label, host, e))
                    continue
                if i:           # a menu 1.1.1.13 does not register: its own visibility goes with each item
                    old = new["p"].get(str(W13 + 0xA0))
                    if vis is not None:
                        new["p"][str(W13 + 0xA0)] = statement([op("("), nested(vis)] + (
                            [op("&&"), nested(old)] if old is not None else []))
                part["items"].append(new)
                part["own"].append(i == 0)
                kept += 1
            report.append("%s %s -> %s: %d of %d items" % (style, name, host, kept, len(items)))
        out[host] = part
    for n in conv.notes:
        if n.startswith("material "):
            continue
        report.append("%s rewrite: %s" % (style, n))
    return out, report, conv


# --- the menus ---------------------------------------------------------------------------------------------------
def stock_menus():
    """1.1.1.13's own menus by name: this zone's (sources/menus_stock.json) and the survival HUD's originals."""
    out = dict(json.loads((SRC / "menus_stock.json").read_text(encoding="utf-8"))["menus"])
    original = json.loads((SURVIVAL_DIR / "survival_hud_visibility.original.json").read_text(encoding="utf-8"))
    for m in original["p"]["8"]:
        out[m["p"]["0"]] = m
    return out


def load_facts():
    return json.loads((SRC / "facts.json").read_text(encoding="utf-8"))


def references(node):
    """Stock menus name the materials their zone defines without a comma: here every one is a reference."""
    if isinstance(node, list):
        return [references(x) for x in node]
    if not isinstance(node, dict):
        return node
    if "asset" in node:
        name = node.get("name") or ""
        return dict(node, name=name if name.startswith(",") else "," + name)
    return {"kind": node["kind"], "raw": node["raw"], "p": {k: references(v) for k, v in node["p"].items()}}


def set_items(menu, items):
    menu["p"][str(W13 + 0x78)] = items
    raw = bytearray(bytes.fromhex(menu["raw"]))
    struct.pack_into("<I", raw, W13 + 0xC, len(items))
    menu["raw"] = raw.hex().upper()


def merge(stock_menu, parts):
    """1.1.1.13's menu with its own items gated to the stock style and every shipped style's items after them.
    parts: {style number: style_parts()[host]}."""
    numbers = sorted(parts)
    menu = {"kind": "menuDef", "raw": stock_menu["raw"], "p": dict(references(stock_menu)["p"])}
    vis, items = str(W13 + 0x40), str(W13 + 0xA0)
    menu["p"][vis] = menu_visible(numbers, menu["p"].get(vis), {n: parts[n]["visible"] for n in numbers})
    if menu["p"][vis] is None:
        del menu["p"][vis]
    out = []
    for it in menu["p"].get(str(W13 + 0x78), []):
        it = copy.deepcopy(it)
        it["p"][items] = gate_stock(numbers, it["p"].get(items))
        if it["p"][items] is None:
            del it["p"][items]
        out.append(it)
    for n in numbers:
        for it in parts[n]["items"]:
            it = copy.deepcopy(it)
            it["p"][items] = gate_style(n, it["p"].get(items))
            out.append(it)
    set_items(menu, out)
    return menu


def all_parts():
    """-> ({number: {host: part}}, report, {style: converter}) over the shipped styles."""
    facts, stock = load_facts(), stock_menus()
    by, report, convs = {}, [], {}
    for n, style in shipped():
        by[n], lines, convs[style] = style_parts(style, facts, None, stock)
        report += lines
    return by, report, convs


def author(parts=None):
    """This zone's MenuList: the hosts the survival HUD zone does not carry."""
    parts = all_parts()[0] if parts is None else parts
    stock = stock_menus()
    out = [merge(stock[h], {n: p[h] for n, p in parts.items() if h in p}) for h in OWN_HOSTS]
    return {"kind": "MenuList", "raw": struct.pack("<III", 0, len(out), 0).hex().upper(), "p": {"0": MENUFILE, "8": out}}


def fold_survival(authored, original, parts=None):
    """tools/build_survival_hud.py's MenuList (killstreaks_hd hidden in Survival by its menu, scorebar_hd's MP items
    by each item, the Survival panel's items after them) with the styles folded in. Survival keeps its panel in every
    style; a style's score items are MP items, so they hide in Survival like the stock ones."""
    parts = all_parts()[0] if parts is None else parts
    numbers = sorted(parts)
    result = copy.deepcopy(authored)
    vis, ivis = str(W13 + 0x40), str(W13 + 0xA0)
    for menu in result["p"]["8"]:
        name = menu["p"]["0"]
        stock = [m for m in original["p"]["8"] if m["p"]["0"] == name][0]
        mine = {n: parts[n][name] for n in numbers if name in parts[n]}
        if not mine:
            continue
        styled = menu_visible(sorted(mine), stock["p"].get(vis), {n: p["visible"] for n, p in mine.items()})
        count = len(stock["p"].get(str(W13 + 0x78), []))
        items = menu["p"][str(W13 + 0x78)]
        if name == "killstreaks_hd":
            # the survival gate is on the menu: ( g_gametype != "survival" && {the styles' visibility} )
            menu["p"][vis] = gate(_not_survival(), styled)
        else:
            menu["p"][vis] = styled
        for it in items[:count]:
            it["p"][ivis] = gate_stock(sorted(mine), it["p"].get(ivis))
        extra = []
        for n in sorted(mine):
            for it, own in zip(mine[n]["items"], mine[n]["own"]):
                it = copy.deepcopy(it)
                # the later build's score panel is an MP panel: hidden in Survival like the stock one. The items of
                # its other menus (the kill/death bar) keep their own rule.
                it["p"][ivis] = gate_style(n, it["p"].get(ivis), survival=(name == "scorebar_hd" and own))
                extra.append(it)
        set_items(menu, items + extra)
    return result


# --- materials and images ------------------------------------------------------------------------------------------
IMAGE_HDR = "03000100000001005455000054550000400040000100000200000000"   # controller_glyphs_postgfx_mp's (in-zone 2D)


def material_blob(name, image):
    """A `2d` material with one colorMap, byte for byte the layout of 1.1.1.13's own HUD materials (MEASURED:
    hud_scorebar_topbar inline in common_mp's ui_mp/hud_720.txt, the same bytes controller_glyphs_postgfx_mp ships):
    Material 0x60 {name @0, gameFlags 0, sortKey 0x30, atlas 1x1, stateBitsEntry[48] @0x18 all 0xFF but technique 4,
    textureCount 1, constantCount 0, stateBitsCount 1, stateFlags 2, cameraRegion 4 @0x48, techniqueSet @0x50,
    textureTable @0x54, constantTable 0, stateBitsTable @0x5C}; name; a MaterialTechniqueSet reference (0xCC bytes,
    name ",2d"); MaterialTextureDef {hash of "colorMap" 0xA0AB1041, 'c', 'p', samplerState 0xE2, semantic 0, image};
    a GfxImage reference (0x24 bytes, ",<image>"); GfxStateBits {0x18125165, 0xE00E0002} (alpha blend, no depth)."""
    inline = b"\xff\xff\xff\xff"
    head = bytearray(0x60)
    head[0:4] = inline
    head[4:8] = bytes([0x00, 0x30, 0x01, 0x01])
    head[0x18:0x48] = b"\xff" * 0x30
    head[0x1C] = 0
    head[0x48:0x50] = bytes([1, 0, 1, 2, 4, 0, 0, 0])
    head[0x50:0x60] = inline + inline + bytes(4) + inline
    return (bytes(head) + name.encode("latin-1") + b"\0" + inline + bytes(0xC8) + b",2d\0" +
            bytes.fromhex("4110ABA06370E200") + inline + bytes(0x20) + inline + b"," + image.encode("latin-1") +
            b"\0" + bytes.fromhex("6551121802000EE0"))


def image_levels(w, h):
    """The full mip chain C1 uploads for an image without the no-mipmaps flag (lflags 0x1, as the glyph images)."""
    n = 1
    while w > 1 or h > 1:
        w, h, n = max(w >> 1, 1), max(h >> 1, 1), n + 1
    return n


def carried(convs=None):
    """-> the asset names (image and material alike) of every carried material the kept items use. Two styles whose
    material has the same pixels share one asset (sources/facts.json "asset")."""
    convs = all_parts()[2] if convs is None else convs
    return sorted({name for _n, style in shipped() for name in convs[style].used_materials})


def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    if head[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("%s is not a PNG" % path)
    return struct.unpack(">II", head[16:24])


# --- the zone ------------------------------------------------------------------------------------------------------
CSV_HEAD = """// Selectable HUD styles (k33_hudStyle): later builds' native HUD items for C1's weapon bar and minimap menus,
// with the materials and images those items draw. Generated by tools/build_hud_styles.py; docs/HUD_STYLES_20261112.md.
>zone,%s
>install,client_shipRetail\\patch
>origin,new
""" % ZONE


def write_text(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def manifest_rows(names):
    rows = ["rawfile,%s" % ZONE]
    for n in names:
        w, h = png_size(ZDIR / "image" / (n + ".png"))
        rows.append("image,%s,hdr=%s,levels=%d,lflags=0x1" % (n, IMAGE_HDR, image_levels(w, h)))
    rows += ["material,%s,file=material/%s.bin" % (n, n) for n in names]
    rows.append("menufile,%s,src=menufile/hud_styles.json" % MENUFILE)
    return rows


def write_manifest(names):
    path = ROOT / "zone_source" / (ZONE + ".csv")
    keep = {}
    if path.is_file():
        for line in path.read_text(encoding="utf-8").split("\n"):
            if line.startswith(">"):
                keep[line.split(",", 1)[0]] = line
    lines = CSV_HEAD.rstrip("\n").split("\n")
    for key in (">build-md5", ">container", ">filetime", ">rsa", ">zlib", ">blocks", ">xassetlist"):
        default = {">container": ">container,iwffu100", ">zlib": ">zlib,9",
                   ">blocks": ">blocks,0x0,0x1000,0x0,0x0,auto,0x0,0x0,0x0,0x0",
                   ">xassetlist": ">xassetlist,0x0,0xffffffff"}.get(key)
        if keep.get(key) or default:
            lines.append(keep.get(key) or default)
    write_text(path, "\n".join(lines + manifest_rows(names)) + "\n")


def locale_rows(convs):
    """The "@KEY" texts 1.1.1.13 lacks: {language: [(key, value)]} from sources/facts.json."""
    facts = load_facts()
    need = sorted({k for c in convs.values() for k in c.keys if k in facts["localize"]})
    return {lang: [(k, facts["localize"][k][lang]) for k in need] for lang in LOCALE_ZONES}


def write_locale(convs):
    from zonekit import codecs
    for lang, rows in locale_rows(convs).items():
        zone = LOCALE_ZONES[lang]
        enc = "gbk" if lang == "chinese" else "ascii"
        write_text(ROOT / "zones" / zone / "localize.str", codecs.write_str([(k, v.encode(enc)) for k, v in rows], enc))
        path = ROOT / "zone_source" / (zone + ".csv")
        keep = {}
        if path.is_file():
            for line in path.read_text(encoding="utf-8").split("\n"):
                if line.startswith(">"):
                    keep[line.split(",", 1)[0]] = line
        lines = ["// The texts of the HUD styles' items that 1.1.1.13 has no localize entry for (%s)." % lang,
                 "// Generated by tools/build_hud_styles.py; docs/HUD_STYLES_20261112.md.",
                 ">zone,%s" % zone, ">install,client_shipRetail\\patch", ">origin,new"]
        for key, default in ((">build-md5", None), (">container", ">container,iwffu100"), (">filetime", None),
                             (">rsa", None), (">zlib", ">zlib,9"),
                             (">blocks", ">blocks,0x0,0x20,0x0,0x0,auto,0x0,0x0,0x0,0x0"),
                             (">xassetlist", ">xassetlist,0x0,0xffffffff"),
                             (">localize", ">localize,localize.str,%s" % enc)):
            if keep.get(key) or default:
                lines.append(keep.get(key) or default)
        lines.append("rawfile,%s" % zone)
        write_text(ROOT / "zones" / zone / "rawfile" / zone, "")
        write_text(path, "\n".join(lines + ["localize,%s" % k for k, _v in rows]) + "\n")


def record_keys(game):
    """A zone_source csv without a recorded container key gets one (as zonekit/packs.py does for a new pack)."""
    from zonekit import build, container, stock
    exe = os.path.join(game, "C1.exe")
    for zone in [ZONE] + sorted(LOCALE_ZONES.values()):
        path = ROOT / "zone_source" / (zone + ".csv")
        text = path.read_text(encoding="utf-8")
        if ">filetime," in text and ">rsa," in text:
            continue
        if not os.path.isfile(exe):
            raise SystemExit("%s has no recorded key and %s is not there to draw one (--game)" % (path, exe))
        src = stock.ZoneSource(exe, [os.path.join(game, "client_shipRetail", "chinese")])
        img = build.assemble(str(ROOT), zone, build.load(str(ROOT), zone), src)
        data = container.write_unsigned(img, exe, level=9)
        hi, lo = (int.from_bytes(data[0x0D + 4 * i:0x11 + 4 * i], "little") for i in range(2))
        lines = text.split("\n")
        at = [i for i, l in enumerate(lines) if l.startswith(">container,")][0]
        lines[at + 1:at + 1] = [">filetime,%#010x,%#010x" % (hi, lo),
                                ">rsa," + data[container.HEADER_LEN:container.BODY_OFF].hex()]
        write_text(path, "\n".join(lines))
        print("%s: drew a container key" % path.name)


def build(game=None):
    parts, report, convs = all_parts()
    node = author(parts)
    menus.encode_menulist(node)
    write_text(ZDIR / "menufile" / "hud_styles.json", json.dumps(node, indent=1) + "\n")
    write_text(ZDIR / "menufile" / "hud_styles.txt", menus.listing(node))
    write_text(ZDIR / "menufile" / "hud_styles.report.txt",
               "k33_hudStyleList %s\n" % style_list() + "\n".join(report) + "\n")
    names = carried(convs)
    mdir = ZDIR / "material"
    mdir.mkdir(parents=True, exist_ok=True)
    for old in mdir.glob("*.bin"):
        if old.stem not in names:
            old.unlink()
    for n in names:
        (mdir / (n + ".bin")).write_bytes(material_blob(n, n))
    for old in (ZDIR / "image").glob("*.png"):
        if old.stem not in names:
            raise SystemExit("%s is not a material any kept item uses (extract again, or remove it)" % old)
    write_text(ZDIR / "rawfile" / ZONE, "")
    write_manifest(names)
    write_locale(convs)
    # the survival HUD zone's menufile: its own authoring, then the styles
    import importlib.util
    spec = importlib.util.spec_from_file_location("build_survival_hud", ROOT / "tools/build_survival_hud.py")
    survival = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(survival)
    survival.main()
    if game:
        record_keys(game)
    for line in report:
        print(line)
    print("k33_hudStyleList %s; %d carried materials" % (style_list(), len(names)))


# --- extraction from the installs (needs CODOL-AssetExplorer's CodolSoundDumper with zone-menus) --------------------
DEFAULT_BUILDS = r"D:\CoD\CODOL Builds"
DEFAULT_INSTALL = {
    "c1": DEFAULT_BUILDS + r"\Call of Duty Online 1.1.1.13",
    "v26": DEFAULT_BUILDS + r"\CODOL_2.6.9.7",
    "v12": DEFAULT_BUILDS + "\\codol-misc-builds\\\u00ca\u00b9\u00c3\u00fc\u00d5\u00d9\u00bb\u00bdOnline",
}


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", nargs="?", default="build", choices=["build", "extract"])
    ap.add_argument("--game", default=DEFAULT_INSTALL["c1"],
                    help="the 1.1.1.13 install (build: only to draw a container key for a new zone_source csv)")
    ap.add_argument("--dumper", help="extract: CodolSoundDumper.exe built with the zone-menus command")
    ap.add_argument("--c1", default=DEFAULT_INSTALL["c1"])
    ap.add_argument("--v26", default=DEFAULT_INSTALL["v26"])
    ap.add_argument("--v12", default=DEFAULT_INSTALL["v12"])
    ap.add_argument("--work", help="extract: a folder for the dumps (default: a temporary one)")
    ap.add_argument("--images", action="append", default=[], metavar="STYLE=DIR",
                    help="extract: read a style's images from this folder instead of its install. 1.2.1.12 has a "
                         "loose main folder without IWDs, which hides the packaged ones from export-textures: give "
                         "a folder that holds only the client exe and a link to the install's IIPS folder")
    ap.add_argument("--styles", default="v26,v12", help="extract: the styles to read (default v26,v12)")
    a = ap.parse_args(argv)
    if a.command == "extract":
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import hud_styles_extract
        hud_styles_extract.run(a)
    build(a.game)


if __name__ == "__main__":
    main()
