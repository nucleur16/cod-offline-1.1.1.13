"""The selectable HUD styles (tools/build_hud_styles.py, hud_style.c): portable, no game files.

What is checked is what can be checked without a client: the generated zone is what the sources give, the stock HUD
is untouched under its gate, exactly one style's items can be visible for any k33_hudStyle, every kept item uses only
what 1.1.1.13 has, and the DLL publishes the list the zone was built for. docs/HUD_STYLES_20261112.md lists what still
needs a live client.
"""
import copy
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from zonekit import manifest, menus, packs  # noqa: E402


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / (name + ".py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


hs = load("build_hud_styles")
survival = load("build_survival_hud")
W = hs.W13
VIS, ITEMS, IVIS = str(W + 0x40), str(W + 0x78), str(W + 0xA0)
OPS = menus.OPS_1113


def tokens(stmt):
    out = []
    for e in stmt["p"].get("4", []):
        t, a, b = struct.unpack_from("<IIi", bytes.fromhex(e["raw"]))
        if t == 0:
            out.append(("op", OPS[a]))
        elif a == 0:
            out.append(("int", b))
        elif a == 2:
            out.append(("str", e["p"]["8"]))
        elif a == 3:
            out.append(("stmt", e["p"]["8"]))
        else:
            raise AssertionError("operand type %d in a gate" % a)
    return out


class Gate:
    """Evaluates the expressions this tool writes: ( ) == != && ||, dvarint("x") / dvarstring("x"), constants and
    whole statements as operands. A wrapped stock statement is an operand: `inner` says what it evaluates to. The
    last closing parenthesis of a statement is left out, as in every stock expression."""

    def __init__(self, dvars, inner):
        self.dvars, self.inner = dvars, inner

    def run(self, stmt):
        self.t, self.i = tokens(stmt), 0
        v = self.or_()
        if self.i != len(self.t):
            raise AssertionError("tokens left after the expression: %r" % (self.t[self.i:],))
        return v

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else (None, None)

    def take(self, kind=None, value=None):
        k, v = self.peek()
        if (kind and k != kind) or (value is not None and v != value):
            raise AssertionError("expected %s %s at %d, got %s %r" % (kind, value, self.i, k, v))
        self.i += 1
        return v

    def close(self):
        if self.peek() == ("op", ")"):
            self.i += 1
        elif self.i != len(self.t):
            raise AssertionError("missing ) at %d" % self.i)

    def or_(self):
        v = self.and_()
        while self.peek() == ("op", "||"):
            self.i += 1
            w = self.and_()
            v = bool(v) or bool(w)
        return v

    def and_(self):
        v = self.cmp()
        while self.peek() == ("op", "&&"):
            self.i += 1
            w = self.cmp()
            v = bool(v) and bool(w)
        return v

    def cmp(self):
        v = self.primary()
        while self.peek() in (("op", "=="), ("op", "!=")):
            o = self.take()
            w = self.primary()
            v = (v == w) if o == "==" else (v != w)
        return v

    def primary(self):
        k, v = self.peek()
        self.i += 1
        if (k, v) == ("op", "("):
            r = self.or_()
            self.close()
            return r
        if k == "op" and v in ("dvarint", "dvarstring"):
            name = self.take("str")
            self.close()
            d = self.dvars.get(name, 0 if v == "dvarint" else "")
            return int(d) if v == "dvarint" else str(d)
        if k in ("int", "str"):
            return v
        if k == "stmt":
            return self.inner(v)
        raise AssertionError("unexpected %s %r at %d" % (k, v, self.i - 1))


def visible(stmt, style, gametype="war", inner=lambda s: True):
    if stmt is None:
        return True
    return bool(Gate({hs.DVAR: style, "g_gametype": gametype}, inner).run(stmt))


def survival_rule(gametype):
    """What a wrapped statement evaluates to when only the survival HUD's own gate (tools/build_survival_hud.py
    scoped(): ( dvarstring "g_gametype" ) != "survival" && ( .. )) decides."""
    head = [("op", "("), ("op", "dvarstring"), ("str", "g_gametype"), ("op", ")"), ("op", "!="), ("str", "survival")]
    return lambda s: tokens(s)[:6] != head or gametype != "survival"


def strip_gate(stmt):
    """The statement a gate wraps (None when the gate stands alone)."""
    last = stmt["p"]["4"][-1]
    t, a = struct.unpack_from("<II", bytes.fromhex(last["raw"]))
    return last["p"]["8"] if (t, a) == (1, 3) else None


class HudStyles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.parts, cls.report, cls.convs = hs.all_parts()
        cls.numbers = sorted(cls.parts)
        cls.stock = hs.stock_menus()
        cls.own = hs.author(cls.parts)
        original = json.loads((hs.SURVIVAL_DIR / "survival_hud_visibility.original.json").read_text())
        cls.original = original
        cls.authored = survival.author(original)
        cls.folded = hs.fold_survival(cls.authored, original, cls.parts)
        cls.menus = {m["p"]["0"]: m for m in cls.own["p"]["8"] + cls.folded["p"]["8"]}
        cls.facts = hs.load_facts()

    def test_the_checked_in_zone_is_what_the_sources_give(self):
        z = hs.ZDIR
        self.assertEqual(json.loads((z / "menufile/hud_styles.json").read_text(encoding="utf-8")), self.own)
        self.assertEqual((z / "menufile/hud_styles.txt").read_text(encoding="utf-8"), menus.listing(self.own))
        self.assertEqual((z / "menufile/hud_styles.report.txt").read_text(encoding="utf-8"),
                         "k33_hudStyleList %s\n" % hs.style_list() + "\n".join(self.report) + "\n")
        self.assertEqual((hs.SURVIVAL_DIR / "survival_hud_visibility.bin").read_bytes(),
                         menus.encode_menulist(self.folded))
        names = hs.carried(self.convs)
        self.assertEqual(sorted(p.stem for p in (z / "image").glob("*.png")), names)
        self.assertEqual(sorted(p.stem for p in (z / "material").glob("*.bin")), names)
        for n in names:
            self.assertEqual((z / "material" / (n + ".bin")).read_bytes(), hs.material_blob(n, n))
        m = manifest.read((ROOT / "zone_source" / (hs.ZONE + ".csv")).read_text(encoding="utf-8"))
        self.assertEqual(["%s,%s%s" % (t, n, "".join(",%s=%s" % kv for kv in q.items())) for t, n, q in m.assets],
                         hs.manifest_rows(names))
        self.assertEqual(m.get("origin"), "new")
        menus.encode_menulist(self.own)

    def test_shipped_styles_and_numbers(self):
        self.assertEqual(hs.STYLE_NUMBERS, {"stock": 0, "v12": 1, "v26": 2, "v33": 3, "v35": 4})
        # v12 is not shipped: in a client its converted HUD could not be told from v26's (same art, layout
        # differences of a few units; docs/HUD_STYLES_20261112.md)
        self.assertEqual(hs.shipped(), [(2, "v26")])
        self.assertEqual(hs.style_list(), "0:stock;2:v26")
        c = (ROOT / "src" / "hud_style.c").read_text(encoding="utf-8")
        self.assertIn('#define HUD_STYLE_LIST      "%s"' % hs.style_list(), c)
        self.assertIn('#define HUD_STYLE_DVAR      "%s"' % hs.DVAR, c)
        self.assertIn("#define HUD_STYLE_MAX       %d" % max(hs.STYLE_NUMBERS.values()), c)
        k = (ROOT / "src" / "kernel33.c").read_text(encoding="utf-8", errors="replace")
        self.assertEqual(k.count('#include "hud_style.c"'), 1)
        self.assertEqual(k.count("InstallHudStyle();"), 1)
        self.assertLess(k.index('#include "k33tick.c"'), k.index('#include "hud_style.c"'))

    def test_every_host_is_one_menu_with_the_stock_items_first(self):
        self.assertEqual(sorted(self.menus), ["killstreaks_hd", "minimap_fullscreen", "scorebar_hd", "weaponbar_hd"])
        self.assertEqual([m["p"]["0"] for m in self.own["p"]["8"]], list(hs.OWN_HOSTS))
        self.assertEqual([m["p"]["0"] for m in self.folded["p"]["8"]], [m["p"]["0"] for m in self.authored["p"]["8"]])
        for name, menu in self.menus.items():
            with self.subTest(menu=name):
                stock = hs.references(self.stock[name])
                before = {m["p"]["0"]: m for m in self.authored["p"]["8"]}.get(name, stock)
                count = len(before["p"][ITEMS])
                items = menu["p"][ITEMS]
                extra = sum(len(self.parts[n][name]["items"]) for n in self.numbers)
                self.assertEqual(len(items), count + extra)
                self.assertEqual(struct.unpack_from("<I", bytes.fromhex(menu["raw"]), W + 0xC)[0], len(items))
                # nothing of the menu itself changes but its item count and its visibility
                raw, was = bytearray(bytes.fromhex(menu["raw"])), bytearray(bytes.fromhex(stock["raw"]))
                raw[W + 0xC:W + 0x10] = was[W + 0xC:W + 0x10] = bytes(4)
                self.assertEqual(raw, was)
                self.assertEqual({k: v for k, v in menu["p"].items() if k not in (VIS, ITEMS)},
                                 {k: v for k, v in stock["p"].items() if k not in (VIS, ITEMS)})
                # each stock item (and the survival HUD's own gating of it, and its Survival panel) is intact
                # under the style gate
                for got, want in zip(items[:count], before["p"][ITEMS]):
                    restored = copy.deepcopy(got)
                    panel = name == "scorebar_hd" and want not in self.authored["p"]["8"][1]["p"][ITEMS][:33]
                    if not panel:
                        inner = strip_gate(got["p"][IVIS])
                        if inner is None:
                            del restored["p"][IVIS]
                        else:
                            restored["p"][IVIS] = inner
                    self.assertEqual(restored, want)

    def test_exactly_the_selected_style_is_visible(self):
        for name, menu in self.menus.items():
            stock_count = len(self.stock[name]["p"][ITEMS])
            items = menu["p"][ITEMS]
            groups = {0: items[:stock_count]}
            at = len(items) - sum(len(self.parts[n][name]["items"]) for n in self.numbers)
            panel = items[stock_count:at]
            for n in self.numbers:
                k = len(self.parts[n][name]["items"])
                groups[n] = items[at:at + k]
                at += k
            for value in range(0, 6):
                shown = value if value in self.numbers else 0       # a value that is not shipped shows the stock HUD
                with self.subTest(menu=name, k33_hudStyle=value):
                    self.assertTrue(visible(menu["p"].get(VIS), value))
                    for n, group in groups.items():
                        for it in group:
                            self.assertEqual(visible(it["p"].get(IVIS), value), n == shown)
                    for it in panel:      # the Survival panel does not depend on the style
                        self.assertNotIn(hs.DVAR, menus.expr(it["p"][IVIS]))

    def test_menu_visibility_follows_the_selected_build(self):
        for name, menu in self.menus.items():
            stock_vis = self.stock[name]["p"].get(VIS)
            for value in range(0, 6):
                shown = value if value in self.numbers else 0
                want = stock_vis if shown == 0 else self.parts[shown][name]["visible"]
                seen = []

                def inner(s, seen=seen):
                    seen.append(s)
                    return s == want or (s["p"]["4"] and any(
                        struct.unpack_from("<II", bytes.fromhex(e["raw"]))[:2] == (1, 3) and inner(e["p"]["8"])
                        for e in s["p"]["4"]))
                with self.subTest(menu=name, k33_hudStyle=value):
                    self.assertTrue(visible(menu["p"][VIS], value, inner=inner))
                    self.assertIn(want, seen)
                    # only the selected build's rule decides: with it false the menu is hidden
                    self.assertFalse(visible(menu["p"][VIS], value, inner=lambda s: False))

    def test_survival_keeps_its_panel_and_hides_the_score_and_streak_menus(self):
        ks, sb = self.menus["killstreaks_hd"], self.menus["scorebar_hd"]
        for value in range(0, 4):
            with self.subTest(k33_hudStyle=value):
                self.assertFalse(visible(ks["p"][VIS], value, "survival"))
                self.assertTrue(visible(ks["p"][VIS], value, "war"))
                stock_count = len(self.stock["scorebar_hd"]["p"][ITEMS])
                for it in sb["p"][ITEMS][:stock_count]:
                    self.assertFalse(visible(it["p"][IVIS], value, "survival", survival_rule("survival")))
                    self.assertEqual(visible(it["p"][IVIS], value, "war", survival_rule("war")),
                                     value not in self.numbers)
                for n in self.numbers:
                    part = self.parts[n]["scorebar_hd"]
                    at = len(sb["p"][ITEMS]) - sum(len(self.parts[m]["scorebar_hd"]["items"]) for m in self.numbers if m >= n)
                    for it, own in zip(sb["p"][ITEMS][at:at + len(part["items"])], part["own"]):
                        if own:       # the later build's score panel is an MP panel
                            self.assertFalse(visible(it["p"][IVIS], n, "survival"))
                        self.assertEqual(visible(it["p"][IVIS], n, "war"), True)

    def test_every_kept_item_uses_only_what_1_1_1_13_has(self):
        carried = set(hs.carried(self.convs))
        stock_ok = {n for st in self.facts["styles"].values() for n, how in st["materials"].items()
                    if how["action"] == "stock"}
        for name, menu in self.menus.items():
            stock_count = len(self.stock[name]["p"][ITEMS])
            for i, it in enumerate(menu["p"][ITEMS]):
                with self.subTest(menu=name, item=i):
                    self.assertEqual(len(bytes.fromhex(it["raw"])), menus.ITEM13)
                    ops = set()
                    hs.walk_ops(it, ops)
                    self.assertLess(max(ops), len(OPS))
                    if i < stock_count:
                        continue
                    raw = bytes.fromhex(it["raw"])
                    od = struct.unpack_from("<i", raw, 0x38)[0]
                    self.assertTrue(od == 0 or od in self.facts["c1_ownerdraws"] or od in hs.HUD_OWNERDRAWS)
                    self.assertNotIn(struct.unpack_from("<i", raw, W + 0x20)[0], hs.FONTS)
                    listing = menus.expr({"kind": "Statement", "raw": "00" * 20, "p": {"4": [
                        hs.nested(v) for k, v in it["p"].items() if isinstance(v, dict) and v.get("kind") == "Statement"]}})
                    for field in re.findall(r'\bplayer "([^"]*)"', listing):
                        self.assertIn(field, hs.C1_PLAYER_FIELDS)
                    for word in ("istrain", "isevac", "issurvival", "getweaponfiretype", "hasperk"):
                        self.assertNotIn(" %s " % word, " %s " % listing)
                    names = [it["p"]["160"]["name"]] if "160" in it["p"] else []
                    names += [e["p"]["8"] for e in hs.material_strings(it)]
                    for n in names:
                        n = n.lstrip(",")
                        self.assertTrue(n in carried or n in stock_ok, n)
                    for key in hs.loc_keys(it):
                        self.assertTrue(self.facts["c1_localize"].get(key) or key in self.facts["localize"] or
                                        key in self.facts["c1_menu_keys"], key)
        for style, st in self.facts["styles"].items():
            for key, same in st["tables"].items():
                self.assertEqual(same["differ"], 0, "%s %s" % (style, key))
                self.assertGreater(same["shared"], 0)

    def test_dropped_items_are_reported_with_a_reason(self):
        text = "\n".join(self.report)
        for _n, style in hs.shipped():
            cfg = hs.STYLES[style]
            src = json.loads((hs.SRC / ("menus_%s.json" % style)).read_text(encoding="utf-8"))
            self.assertEqual(src["build"], cfg["build"])
            self.assertEqual(src["windowSize"], cfg["window"])
            for host, names in cfg["hosts"].items():
                for name in names:
                    total = len(src["menus"][name]["p"].get(str(cfg["window"] + 0x78), []))
                    kept = int(re.search(r"^%s %s -> %s: (\d+) of %d items$" % (style, name, host, total), text, re.M).group(1))
                    dropped = len(re.findall(r"^%s %s\[\d+\][^\n]* -> %s: dropped: \S" % (style, name, host), text, re.M))
                    self.assertEqual(kept + dropped, total, "%s %s" % (style, name))
        # the later menus' own operator tables: 1.2.1.12's is a prefix of 2.6.9.7's
        self.assertEqual(len(hs.STYLES["v12"]["ops"]), 232)
        self.assertEqual(hs.STYLES["v12"]["ops"][-1], "isevac")
        # what the docs call visibly missing really is dropped, and the main panels really are kept
        # implemented natively now (hud_style.c): the fire-mode tag, the assists, the ammo display
        self.assertIn('v26 rewrite: getweaponfiretype -> dvarstring("k33_hudFireType")', text)
        self.assertIn('v26 rewrite: player("assists") -> dvarint("k33_hudAssists")', text)
        self.assertIn("v26 weaponbar_hd[4] -> weaponbar_hd: ownerDraw 5 written as clip count, clip bar and reserve count", text)
        # what stays out, with the reason the docs give
        self.assertRegex(text, r'v26 kd_rate_bar\[10\] -> scorebar_hd: dropped: player\("revives"\)')
        self.assertRegex(text, r"v26 weaponbar_hd_extend\[2\] chargeAttackAnime -> weaponbar_hd: dropped: ownerDraw 297")
        for line in ("v26 weaponbar_hd -> weaponbar_hd: 26 of 26 items", "v26 killstreaks_hd -> killstreaks_hd: 83 of 83 items",
                     "v26 minimap_fullscreen -> minimap_fullscreen: 17 of 17 items", "v26 kd_rate_bar -> scorebar_hd: 16 of 20 items"):
            self.assertIn(line, text)

    def test_the_isCombat_rewrite_is_what_the_later_onOpen_stores(self):
        for _n, style in hs.shipped():
            cfg = hs.STYLES[style]
            src = json.loads((hs.SRC / ("menus_%s.json" % style)).read_text(encoding="utf-8"))
            one = {"kind": "MenuList", "raw": "00" * 12, "p": {"0": "x", "8": [src["menus"]["scorebar_hd"]]}}
            text = menus.listing(one, W=cfg["window"], ops=cfg["ops"])
            if 'localvarint "isCombat"' not in text:
                continue
            self.assertIn('    if ( dvarstring(static) "ui_gametype" ) == "combat":\n        setLocalVarInt isCombat = 1\n'
                          '    else:\n        setLocalVarInt isCombat = 0\n', text)
            self.assertEqual(hs.LOCALVARS["isCombat"], ("ui_gametype", "combat"))

    def test_ammo_display_items(self):
        """ownerDraw 5 of the later weapon bar as clip count, gauge and reserve; the gauge at the later build's
        screen position (-133, -41 in the later build's placement: the host menu's origin is taken off)."""
        wb = self.menus["weaponbar_hd"]
        host = bytes.fromhex(self.stock["weaponbar_hd"]["raw"])
        ox, oy = struct.unpack_from("<2f", host, 4)
        items = [it for it in wb["p"][ITEMS] if struct.unpack_from("<i", bytes.fromhex(it["raw"]), 0x38)[0] in hs.HUD_OWNERDRAWS]
        self.assertEqual(sorted(struct.unpack_from("<i", bytes.fromhex(it["raw"]), 0x38)[0] for it in items),
                         [hs.OD_CLIP, hs.OD_CLIP_BAR])
        for it in items:
            raw = bytes.fromhex(it["raw"])
            self.assertEqual(struct.unpack_from("<i", raw, W + 0x14)[0], 8)        # an ownerdraw item
            if struct.unpack_from("<i", raw, 0x38)[0] == hs.OD_CLIP_BAR:
                x, y, w, h = struct.unpack_from("<4f", raw, 0x18)
                self.assertAlmostEqual(x + ox, -133.0, places=3)
                self.assertAlmostEqual(y + oy, -41.0, places=3)
                self.assertEqual((w, h), (99.0, 3.5))
        c = (ROOT / "src/hud_style.c").read_text(encoding="utf-8")
        self.assertIn("#define HUD_OD_CLIP         %d" % hs.OD_CLIP, c)
        self.assertIn("#define HUD_OD_CLIP_BAR     %d" % hs.OD_CLIP_BAR, c)
        self.assertIn('#define HUD_FIRE_DVAR       "%s"' % hs.FIRE_DVAR, c)
        self.assertIn('#define HUD_ASSISTS_DVAR    "%s"' % hs.ASSISTS_DVAR, c)
        v = (ROOT / "src/audio/voice.c").read_text(encoding="utf-8")
        self.assertIn("    if (HudStyleOwnerDraw(args)) return 1;", v)
        # nothing reads the old tick-published clip dvars any more
        listing = (hs.ZDIR / "menufile/hud_styles.txt").read_text(encoding="utf-8")
        self.assertNotIn("k33_hudClip", listing)
        self.assertNotIn('player "clipAmmo_left"', listing)

    def test_carried_materials_and_images(self):
        glyph = ROOT / "zones/controller_glyphs_postgfx_mp/material/0073_k33pad_xbox360_a.bin"
        if glyph.is_file():      # the layout another zone of this pack already ships, byte for byte
            self.assertEqual(hs.material_blob("k33pad_xbox360_a", "k33pad_xbox360_a"), glyph.read_bytes())
        m = manifest.read((ROOT / "zone_source" / (hs.ZONE + ".csv")).read_text(encoding="utf-8"))
        kinds = [t for t, _n, _q in m.assets]
        # images, then the materials that name them, then the menus that name those
        self.assertEqual(kinds, sorted(kinds, key=["rawfile", "image", "material", "menufile"].index))
        for t, n, q in m.assets:
            if t != "image":
                continue
            w, h = hs.png_size(hs.ZDIR / "image" / (n + ".png"))
            self.assertEqual(int(q["levels"]), hs.image_levels(w, h))
            self.assertEqual(q["lflags"], "0x1")
            self.assertTrue(n.startswith("k33hud"))
        self.assertEqual(hs.image_levels(64, 64), 7)
        self.assertEqual(hs.image_levels(261, 37), 9)
        # a carried asset never takes a stock name
        for st in self.facts["styles"].values():
            for name, how in st["materials"].items():
                if how["action"] == "carry":
                    self.assertRegex(how["asset"], r"^k33hud\d\d_" + re.escape(name) + "$")
                    if name in hs.TEAM_TINT:      # carried white, tinted with 1.1.1.13's team colour
                        self.assertIn(how["asset"] + hs.WHITE, set(hs.carried(self.convs)))
                        self.assertNotIn(how["asset"], set(hs.carried(self.convs)))
        # the team bars take the stock scorebar's own constants
        stock_colours = set()
        for it in self.original["p"]["8"][1]["p"][ITEMS]:
            name = (it["p"].get("160") or {}).get("name", "")
            if name in (",hud_scorebar_topbar", ",hud_scorebar_bottombar"):
                stock_colours.add(tuple(round(v, 3) for v in struct.unpack_from("<4f", bytes.fromhex(it["raw"]), 0x50)))
        self.assertEqual(stock_colours, {hs.STOCK_OWN_TEAM, hs.STOCK_OTHER_TEAM})
        tinted = 0
        for it in self.menus["scorebar_hd"]["p"][ITEMS]:
            name = (it["p"].get("160") or {}).get("name", "")
            if name.endswith(hs.WHITE):
                tinted += 1
                colour = tuple(round(v, 3) for v in struct.unpack_from("<4f", bytes.fromhex(it["raw"]), 0x50))
                self.assertIn(colour, stock_colours)
        self.assertEqual(tinted, 4)

    def test_zone_is_packed_after_code_post_gfx_and_ahead_of_the_survival_hud(self):
        pack = packs.load(str(ROOT), "codol_common_postgfx_mp")
        names = [n for n, _f, _q in packs.parts(pack)]
        self.assertIn(hs.ZONE, names)
        # the survival HUD's menus name this zone's materials: they are linked first
        self.assertLess(names.index(hs.ZONE), names.index("survival_hud_postgfx_mp"))
        self.assertEqual(packs.stage(hs.ZONE), "post_gfx")
        for lang, zone in hs.LOCALE_ZONES.items():
            self.assertIn(zone, [n for n, _f, _q in packs.parts(packs.load(str(ROOT), "codol_%s_locale" % lang))])
            self.assertEqual(packs.locales(zone), packs.locales("codol_%s_locale" % lang))
            rows = hs.locale_rows(self.convs)[lang]
            self.assertEqual([k for k, _v in rows], sorted(self.facts["localize"]))
            m = manifest.read((ROOT / "zone_source" / (zone + ".csv")).read_text(encoding="utf-8"))
            self.assertEqual([n for t, n, _q in m.assets if t == "localize"], [k for k, _v in rows])
        for key, value in self.facts["localize"].items():
            self.assertFalse(self.facts["c1_localize"][key])
            value["chinese"].encode("gbk")
            value["english"].encode("ascii")

    def test_gate_evaluator_reads_the_stock_shape(self):
        # ( dvarint "x" ) == 2 && {..}: one opening parenthesis, the last ")" left out, like a stock expression
        st = hs.gate_style(2, hs.statement([hs.integer(1)]))
        self.assertEqual(menus.expr(st), '( dvarint "k33_hudStyle" ) == 2 && {1}')
        self.assertEqual(menus.expr(hs.gate_stock([1, 2], None)),
                         '( dvarint "k33_hudStyle" ) != 1 && dvarint "k33_hudStyle" ) != 2')
        self.assertEqual(menus.expr(hs.gate_style(2, None, survival=True)),
                         '( dvarint "k33_hudStyle" ) == 2 && dvarstring "g_gametype" ) != "survival"')
        self.assertTrue(visible(st, 2) and not visible(st, 0) and not visible(st, 2, inner=lambda s: False))


if __name__ == "__main__":
    unittest.main()
