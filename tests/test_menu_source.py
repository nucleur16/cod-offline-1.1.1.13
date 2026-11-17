"""The .menu sources (zonekit/menu_source.py): every menu in the tree round-trips, the compiler covers the language
and rejects malformed input with file:line, and the zones that carry menus build byte for byte as they did from the
.bin / .json sources the .menu files replaced (portable: the zone images; with the game installed: the .ff files).

    python -B tests/test_menu_source.py
"""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from zonekit import build, manifest, menu_source as ms, menus, shipping  # noqa: E402

W = menus.W13
GAME = Path(os.environ.get("K33_GAME", r"D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13"))
# The zones that carry a menu, built from the sources the .menu files replaced (main at c7822d8: menu/*.bin,
# menufile/*.json and *.bin): md5 of the zone image (zonekit/build.assemble, before the container) and of the
# --parts .ff build_zones.py writes. A .menu that compiles to anything else changes these.
PINNED = {
    "field_orders_postgfx_mp": ("a4af57c1970eba3cae93e0bd05c72278", "7b583c6a32f93d22398aeab60a39575a"),
    "hud_styles_postgfx_mp": ("a27c68055130a2f6d3f8000e1767810a", "ca97ba55c9228a5e5183ab753f2a3177"),
    "survival_hud_postgfx_mp": ("dfd683e524daab9c8a8ca02f0879f2b2", "15b4b0509da2552ed95f77423e2848b0"),
    "patch_common_gametype_survival": ("b9ba3d8084447ce644ce0583946482ec", "68a52c64bb2cf51dd7996a6cf2955bea"),
    "survival_menus": ("223e58ab1ebddc12a3c075fd5a0293ad", "44d8224f89d7d96b6ab256eac11818ae"),
}
# .menu sources that are not zone assets themselves, and their MenuList names (the generators read them)
SOURCES = {
    "zones/survival_hud_postgfx_mp/sources/survival_hud_visibility.original.menu":
        "ui_mp/scriptmenus/survival_hud_visibility.menu",
    "zones/survival_hud_postgfx_mp/components/survival_status.menu":
        "ui_mp/scriptmenus/survival_status_component.menu",
}
HUD_DIALECT = {"zones/hud_styles_postgfx_mp/sources/sm2.menu"}     # parse(): tests/test_hud_styles.py


class NoStocks:
    """The menu zones take nothing from the stock zones."""

    def __getattr__(self, name):
        raise AssertionError("a menu zone asked the stock zones for %s" % name)


def menu_sources():
    """[(path, asset type, asset name)] of every .menu in the tree that is a struct-tree source."""
    out = []
    for csv in sorted((ROOT / "zone_source").glob("*.csv")):
        m = manifest.read(csv.read_text(encoding="utf-8"))
        for t, name, q in m.assets:
            if t in ("menufile", "menu"):
                out.append((ROOT / "zones" / csv.stem / q["src"], t, name))
    out += [(ROOT / rel, "menufile", name) for rel, name in SOURCES.items()]
    return out


def compile_source(path, t, name):
    return ms.load(path, name, menu=(t == "menu"))


def body(text):
    """A .menu text without its leading comment lines (the decompiler's header)."""
    lines = text.split("\n")
    while lines and lines[0].startswith("//"):
        lines.pop(0)
    return "\n".join(lines)


def compile_error(text, where="test.menu"):
    try:
        ms.compile_menufile(text, where, "x")
    except ms.MenuSourceError as e:
        return str(e)
    raise AssertionError("compiled: %r" % text)


# A menu that uses every keyword and expression form of the language, in the forms the decompiler writes.
EVERYTHING = r'''
#define HALF 0.5
#define SHOWN ( dvarint( "ui_x" ) == 1 )
{
    menuDef
    {
        name                 "everything"
        group                "g"
        rect                 10 20 300 200 1 3
        rectClient           1 2 3 4 0 0
        popup
        outOfBoundsClick
        legacySplitScreenScale
        hiddenDuringFlash
        hiddenDuringScope
        hiddenDuringUI
        textOnlyFocus
        decoration
        autowrapped
        staticFlags          0x40
        visible              1
        forecolor            1 HALF 0.25 1
        style                WINDOW_STYLE_FILLED
        border               1
        borderSize           1.5
        ownerdraw            7
        ownerdrawFlag        0x3
        nextTime             9
        backcolor            0 0 0 HALF
        bordercolor          1 0 0 1
        outlinecolor         0 1 0 1
        disablecolor         0 0 1 1
        background           "white"
        font                 "fonts/normalfont"
        allowedBinding       "+attack"
        soundLoop            "music_mainmenu"
        fullScreen           1
        menuExtra            1
        fontIndex            2
        cursorItem           3
        fadeCycle            1
        fadeClamp            1
        fadeAmount           0.1
        fadeInAmount         0.2
        blurWorld            4.8
        imageTrack           2
        focusColor           0.5 0.5 0.5 1
        visible when SHOWN && !inkillcam() && [ ( ui_active ) ];
        exp rect X ( 0 - ( 287 * 0.5 );
        exp rect Y -1 - 2.0;
        exp rect W ( staticdvarint "x" ) + localvarint( "y" );
        exp rect H 1;
        exp openSound "a";
        exp closeSound "b";
        onOpen
        {
            setLocalVarInt ui_timer milliseconds;
            setLocalVarBool "if" 1;
            setLocalVarFloat f 0.5;
            setLocalVarString s "x" + "y";
            ;
            lerp x from - 640 to 0 over 0.15;
            close intel_found close perk_gained;
            setdvar ui_x "a b";
            exec "set x 1; set y 2";
            play "";
            uiScript "if" "{" ;
            if ( localvarint( "ui_timer" ) >= 100 )
            {
                close self;
            }
            else
            {
                if 1
                {
                    open other;
                }
            }
            scriptMenuResponse quit
        }
        onRequestClose
        {
            close self;
        }
        onClose
        {
            setdvar a 1;
        }
        onESC
        {
            close self;
        }
        execKey "a"
        {
            open a;
        }
        execKeyInt 168
        {
            open b;
        }
        itemDef
        {
            name                 "text"
            rect                 -5 6 7 8 2 1
            visible              1
            type                 ITEM_TYPE_BUTTON
            dataType             1
            align                1
            textfont             3
            textalign            10
            textalignx           1.5
            textaligny           -2
            textscale            0.3
            textstyle            ITEM_TEXTSTYLE_SHADOWED
            gamemsgwindowindex   1
            gamemsgwindowmode    2
            itemFlags            0x1
            feeder               3
            cursorPos            1
            imageTrack           1
            glowColor            1 1 0 1
            decayActive          1
            fxBirthTime          1
            fxLetterTime         2
            fxDecayStartTime     3
            fxDecayDuration      4
            lastSoundPlayedTime  5
            text                 "@MENU_X\n\"q\"\\ \xff"
            dvar                 "ui_dvar"
            dvarTest             "ui_test"
            localvar             "ui_local"
            showDvar { "1" 2 }
            focusSound           "mouse_over"
            mouseEnterText
            {
                play a;
            }
            mouseExitText
            {
                play b;
            }
            mouseEnter
            {
                play c;
            }
            mouseExit
            {
                play d;
            }
            action
            {
                play e;
            }
            accept
            {
                play f;
            }
            onFocus
            {
                play g;
            }
            leaveFocus
            {
                play h;
            }
            execKeyInt 13
            {
                close self;
            }
            visible when ( 1;
            disabled when 0;
            exp text locstring( "@X" , ( int timeleft ) );
            exp material "white";
            exp rect X 1;
            exp rect Y 1;
            exp rect W 1;
            exp rect H 1;
            exp forecolor R 1;
            exp forecolor G 1;
            exp forecolor B 1;
            exp forecolor RGB 1;
            exp forecolor A 1;
            exp glowcolor R 1;
            exp glowcolor G 1;
            exp glowcolor B 1;
            exp glowcolor RGB 1;
            exp glowcolor A 1;
            exp backcolor R 1;
            exp backcolor G 1;
            exp backcolor B 1;
            exp backcolor RGB 1;
            exp backcolor A 1;
        }
        itemDef
        {
            rect                 1 2 3 4 0 0
            layoutRect           1 2 3 4 0 0
            textRect             1 1 1 1 0 0
            dynamicFlags         0x4
            forecolor            1 1 1 0.5
            editField
            floatExpressionsEmpty
        }
        itemDef
        {
            rect                 0 0 10 10 0 0
            type                 ITEM_TYPE_EDITFIELD
            maxChars             12
            maxCharsGotoNext     1
            maxPaintChars        8
            minValue             0.5
            maxValue             2
            defValue             1
            range                1.5
            paintOffset          1
            dvar                 "name"
        }
        itemDef
        {
            rect                 0 0 10 10 0 0
            type                 ITEM_TYPE_LISTBOX
            feeder               4
            elementWidth         120
            elementHeight        20
            elementType          1
            drawPadding          2
            mousePos             1
            startPos             2
            endPos               3
            notselectable
            noScrollBars
            usePaging
            selectBorder         1 1 1 1
            columns 2 0 10 20 0 30 40 50 1
            selectIcon           "white"
            doubleClick
            {
                play x;
            }
        }
        itemDef
        {
            rect                 0 0 10 10 0 0
            type                 ITEM_TYPE_LISTBOX
            listBox
        }
        itemDef
        {
            rect                 0 0 10 10 0 0
            type                 ITEM_TYPE_MULTI
            dvarStrList { "Low" "0" "High" "1" }
        }
        itemDef
        {
            rect                 0 0 10 10 0 0
            type                 ITEM_TYPE_MULTI
            dvarFloatList { "Off" 0 "On" 1 "Half" 0.5 }
        }
        itemDef
        {
            rect                 0 0 10 10 0 0
            type                 ITEM_TYPE_DVARENUM
            dvarEnumList         "ui_list"
        }
    }
}
'''


class RoundTrip(unittest.TestCase):
    def test_every_menu_in_the_tree(self):
        """Each .menu compiles; decompiling its tree gives the same text back (the header aside), and that text
        compiles to the same tree: the decompiler and the compiler are each other's inverse on every menu here."""
        found = menu_sources()
        self.assertEqual(len(found), 12)
        menus_in_tree = {str(p.relative_to(ROOT)).replace(os.sep, "/") for p in ROOT.glob("zones/**/*.menu")}
        covered = {str(p.relative_to(ROOT)).replace(os.sep, "/") for p, _t, _n in found} | HUD_DIALECT
        self.assertEqual(menus_in_tree - covered, set(), "a .menu nothing builds or reads")
        for path, t, name in found:
            with self.subTest(menu=str(path.relative_to(ROOT))):
                text = path.read_text(encoding="utf-8")
                tree = compile_source(path, t, name)
                again = ms.decompile_menu(tree) if t == "menu" else ms.decompile_menufile(tree)
                self.assertEqual(body(again), body(text))
                tree2 = ms.compile_menu(again) if t == "menu" else ms.compile_menufile(again, "x", name)
                self.assertEqual(menus.normalize(tree2, t == "menu"), menus.normalize(tree, t == "menu"))
                blob = menus.encode_menu(tree) if t == "menu" else menus.encode_menulist(tree)
                back = menus.decode_menu(blob) if t == "menu" else menus.decode_menulist(blob)
                self.assertEqual(back, menus.normalize(tree, t == "menu"))

    def test_the_reference_dumps(self):
        """The stock 1.1.1.13 dump the HUD styles diff against stays AssetExplorer's JSON: a menu of it that names
        its zone's own (inline) materials has no source form and is refused; every other one round-trips."""
        stock = json.loads((ROOT / "zones/hud_styles_postgfx_mp/sources/menus_stock.json").read_text(encoding="utf-8"))
        self.assertEqual(stock["build"], "1.1.1.13")
        refused = []
        for name, m in stock["menus"].items():
            try:
                text = ms.decompile_menu(m)
            except menus.MenuError as e:
                refused.append(name)
                self.assertIn("is not a reference", str(e))
                continue
            self.assertEqual(menus.normalize(ms.compile_menu(text), True), menus.normalize(m, True))
        self.assertEqual(refused, ["minimap_fullscreen"])

    def test_every_keyword(self):
        tree = ms.compile_menufile(EVERYTHING, "everything.menu", "ui_mp/everything.menu")
        text = ms.decompile_menufile(tree)
        self.assertEqual(menus.normalize(ms.compile_menufile(text, "x", "ui_mp/everything.menu")),
                         menus.normalize(tree))
        self.assertEqual(text, ms.decompile_menufile(ms.compile_menufile(text, "x", "ui_mp/everything.menu")))
        (m,) = tree["p"]["8"]
        raw = bytes.fromhex(m["raw"])
        self.assertEqual(struct.unpack_from("<I", raw, 0x44)[0], 0xF7900040)
        self.assertEqual(struct.unpack_from("<I", raw, 0x48)[0], 0x10004)
        self.assertEqual(struct.unpack_from("<i", raw, W + 0xC)[0], 8)
        self.assertEqual(menus.expr(m["p"][str(W + 0x40)]),
                         '( dvarint "ui_x" ) == 1 ) && ! inkillcam ) && {( ui_active )}')
        self.assertEqual(menus.expr(m["p"][str(W + 0x64)]), "-1 - 2")
        # scripts are stored as IW's compiler writes them
        on_open = m["p"][str(W + 0x2C)]["p"]["4"]
        kinds = [bytes.fromhex(h["raw"])[4] for h in on_open]
        self.assertEqual(kinds, [4, 3, 5, 6, 0, 1, 2, 0])
        self.assertEqual(on_open[4]["p"]["0"], '; "lerp" x "from" - "640" "to" 0 "over" "0.15" ; "close" '
                         '"intel_found" "close" "perk_gained" ; "setdvar" "ui_x" "a b" ; "exec" "set x 1; set y 2" ; '
                         '"play" "" ; "uiScript" "if" { ; ')
        self.assertEqual(on_open[1]["p"]["0"]["p"]["0"], "if")
        self.assertEqual(on_open[7]["p"]["0"], '"scriptMenuResponse" "quit" ')
        key = m["p"][str(W + 0x3C)]
        self.assertEqual((struct.unpack_from("<i", bytes.fromhex(key["raw"]))[0],
                          struct.unpack_from("<i", bytes.fromhex(key["p"]["8"]["raw"]))[0]), (ord("a"), 168))
        items = m["p"][str(W + 0x78)]
        it = items[0]
        self.assertEqual(it["p"][str(W + 0x40)], '@MENU_X\n"q"\\ \xff')
        self.assertEqual(it["p"][str(W + 0x78)], "1 2 ")          # one character: stored bare
        self.assertEqual(struct.unpack_from("<I", bytes.fromhex(it["raw"]), W + 0x80)[0], 4)
        self.assertEqual([struct.unpack_from("<i", bytes.fromhex(f["raw"]))[0] for f in it["p"][str(W + 0x9C)]],
                         list(range(19)))
        # the laid-out rect: rectClient moved by the menu's origin, the menu's alignment for 0 0
        r0 = bytes.fromhex(it["raw"])
        self.assertEqual(struct.unpack_from("<4f", r0, 4), (5.0, 26.0, 7.0, 8.0))
        self.assertEqual((r0[0x14], r0[0x15]), (2, 1))
        r1 = bytes.fromhex(items[1]["raw"])
        self.assertEqual(struct.unpack_from("<4f", r1, 4), (1.0, 2.0, 3.0, 4.0))
        self.assertEqual(struct.unpack_from("<I", r1, 0x48)[0], 4)
        self.assertEqual(items[1]["p"][str(W + 0x9C)], [])
        self.assertEqual(items[1]["p"][str(W + 0x90)]["kind"], "editFieldDef")
        r3 = bytes.fromhex(items[3]["raw"])
        self.assertEqual(struct.unpack_from("<4f", r3, 4)[:2], (10.0, 20.0))
        self.assertEqual((r3[0x14], r3[0x15]), (1, 3))
        lb = items[3]["p"][str(W + 0x90)]
        self.assertEqual(struct.unpack_from("<5i", bytes.fromhex(lb["raw"]), 0x1C), (2, 0, 10, 20, 0))
        multi = bytes.fromhex(items[5]["p"][str(W + 0x90)]["raw"])
        self.assertEqual(struct.unpack_from("<ii", multi, 0x180), (2, 1))
        self.assertEqual(items[7]["p"][str(W + 0x90)], "ui_list")
        blob = menus.encode_menulist(tree)
        self.assertEqual(menus.decode_menulist(blob), menus.normalize(tree))

    def test_call_and_bare_forms(self):
        """name( args ) stores no "(", name args is the operator alone: both are kept as they are stored."""
        def stream(src):
            tree = ms.compile_menufile('menuDef { name "x" rect 0 0 1 1 visible when %s; }' % src, "x", "x")
            st = tree["p"]["8"][0]["p"][str(W + 0x40)]
            return [struct.unpack_from("<III", bytes.fromhex(e["raw"]))[:2] for e in st["p"]["4"]], ms.expression_text(st)
        op = menus.OP_1113
        self.assertEqual(stream('dvarint( "a" )')[0], [(0, op["dvarint"]), (1, 2), (0, op[")"])])
        self.assertEqual(stream('( dvarint "a" )')[0], [(0, op["("]), (0, op["dvarint"]), (1, 2), (0, op[")"])])
        self.assertEqual(stream('( dvarint "a" )')[1], '( dvarint "a" )')
        self.assertEqual(stream('dvarint( "a" ) == 1')[1], 'dvarint( "a" ) == 1')
        self.assertEqual(stream("1 -1")[0], [(1, 0), (0, op["-"]), (1, 0)])
        self.assertEqual(stream("( -1 )")[0], [(0, op["("]), (1, 0), (0, op[")"])])
        self.assertEqual(stream("( - 1 )")[0], [(0, op["("]), (0, op["-"]), (1, 0), (0, op[")"])])
        self.assertEqual(stream("1.0 + 1")[0], [(1, 1), (0, op["+"]), (1, 0)])
        self.assertEqual(stream("staticdvarbool \"a\"")[0], [(0, op["dvarbool(static)"]), (1, 2)])
        self.assertEqual(stream("[ 1 ] && [ ui_active ]")[0], [(1, 3), (0, op["&&"]), (1, 3)])

    def test_preprocessor(self):
        with tempfile.TemporaryDirectory() as tmp:
            Path(tmp, "defs.h").write_text('#define WIDTH 30\n#define SHOWN ( dvarint( "s" ) == 1 )\n',
                                           encoding="utf-8")
            Path(tmp, "bad.h").write_text("\n\n#ifdef PC\n", encoding="utf-8")
            main = Path(tmp, "m.menu")
            main.write_text('#include "defs.h"\n/* a\nblock */ menuDef { name "m" rect 0 0 WIDTH 4\n'
                            '  visible when SHOWN; // comment\n  itemDef { rect 0 0 WIDTH 1 text "// not a comment" } }\n',
                            encoding="utf-8")
            tree = ms.load(main, "m")
            m = tree["p"]["8"][0]
            self.assertEqual(struct.unpack_from("<4f", bytes.fromhex(m["raw"]), 4), (0.0, 0.0, 30.0, 4.0))
            self.assertEqual(m["p"][str(W + 0x78)][0]["p"][str(W + 0x40)], "// not a comment")
            main.write_text('#include "bad.h"\nmenuDef { }\n', encoding="utf-8")
            with self.assertRaisesRegex(ms.MenuSourceError, r"bad\.h:3: unknown directive #ifdef"):
                ms.load(main, "m")
            main.write_text('\n#include "nothere.h"\n', encoding="utf-8")
            with self.assertRaisesRegex(ms.MenuSourceError, r"m\.menu:2: #include nothere\.h: not found"):
                ms.load(main, "m")


class Malformed(unittest.TestCase):
    def test_errors_name_file_and_line(self):
        cases = [
            ('menuDef\n{\n  name "x"\n  bogus 1\n}', 4, "unknown menuDef keyword 'bogus'"),
            ('menuDef {\n itemDef {\n  rect 0 0 1\n } }', 3, "expected 4 numbers"),
            ('menuDef {\n visible when ( 1\n }', 3, "unexpected '}' in an expression"),
            ('menuDef {\n visible when\n  nosuch( 1 );\n}', 3, "nosuch is not a function"),
            ('menuDef {\n visible when 1\n', 2, "the expression does not end"),
            ('menuDef {\n visible when ;\n}', 2, "an empty expression"),
            ('menuDef {\n onOpen\n {\n  close self;', 4, "the handler block does not end"),
            ('menuDef {\n onOpen {\n  else { }\n } }', 3, "else without an if"),
            ('menuDef {\n onOpen {\n  if 1 close self;\n } }', 3, "close is not a function"),
            ('menuDef {\n onOpen { setLocalVarInt ; }\n}', 2, "setLocalVarInt needs a variable name"),
            ('menuDef {\n execKey "ab" { }\n}', 2, "execKey takes one character"),
            ('menuDef {\n itemDef { rect 0 0 1 1\n  type ITEM_TYPE_TEXT\n  dvarStrList { "a" "b" }\n } }', 5,
             "need its type to be 12"),
            ('menuDef {\n itemDef { rect 0 0 1 1 exp rect Q 1; } }', 2, "exp rect q is not supported"),
            ('menuDef {\n fullScreen 1.5\n}', 2, "expected an integer"),
            ('menuDef {\n name x\n}', 2, "expected a string"),
            ('{\n itemDef { }\n}', 2, "expected menuDef"),
            ('menuDef {\n itemDef { rect 0 0 1 1\n  text "unterminated\n } }', 3, "cannot read"),
            ('menuDef {\n onOpen { exec "a\\"b"; }\n}', 2, "cannot hold a double quote"),
            ('\n\nmenuDef {', 3, "the menuDef does not end"),
            ('\n\nmenuDef name', 3, "expected '{'"),
        ]
        for text, line, what in cases:
            with self.subTest(text=text):
                msg = compile_error(text, "bad.menu")
                self.assertRegex(msg, r"^bad\.menu:%d: " % line)
                self.assertIn(what, msg)

    def test_a_menu_asset_is_one_menudef(self):
        with self.assertRaisesRegex(ms.MenuSourceError, "one menuDef, the file has 2"):
            ms.compile_menu('menuDef { name "a" } menuDef { name "b" }', "two.menu")

    def test_the_decompiler_refuses_what_has_no_source(self):
        tree = ms.compile_menufile('menuDef { name "x" onOpen { close self; } }', "x", "x")
        h = tree["p"]["8"][0]["p"][str(W + 0x2C)]["p"]["4"][0]
        h["p"]["0"] = '"close" "self"'                         # no trailing space: not IW's token form
        with self.assertRaisesRegex(menus.MenuError, "token form"):
            ms.decompile_menufile(tree)
        tree["p"]["8"][0]["p"][str(W + 0x2C)]["p"]["4"] = [ms.handler(0, "a "), ms.handler(0, "b ")]
        with self.assertRaisesRegex(menus.MenuError, "in a row"):
            ms.decompile_menufile(tree)

    def test_the_zone_builder_reads_only_menu_sources(self):
        with self.assertRaisesRegex(build.BuildError, "not a .menu source"):
            build.menu_blob(str(ROOT / "zones/hud_styles_postgfx_mp"), "menufile", "x", "sources/menus_stock.json",
                            False)
        with tempfile.TemporaryDirectory() as tmp:
            Path(tmp, "x.menu").write_text('menuDef {\n name "y"\n}\n', encoding="utf-8")
            with self.assertRaisesRegex(build.BuildError, "holds menuDef y"):
                build.menu_blob(tmp, "menu", "x", "x.menu", True)
            Path(tmp, "x.menu").write_text('menuDef {\n nme "y"\n}\n', encoding="utf-8")
            with self.assertRaisesRegex(build.BuildError, r"x\.menu:2: unknown menuDef keyword"):
                build.menu_blob(tmp, "menu", "x", "x.menu", True)


class ByteIdentical(unittest.TestCase):
    def test_zone_images(self):
        """The zones that carry menus assemble to the same zone image as from the sources the .menu files replaced."""
        maps = shipping.ShippingMaps.load(str(ROOT), "ship")
        for zone, (image_md5, _ff) in PINNED.items():
            with self.subTest(zone=zone):
                m = build.load(str(ROOT), zone)
                self.assertTrue(any(t in ("menu", "menufile") for t, _n, _q in m.assets))
                img = build.assemble(str(ROOT), zone, m, NoStocks(), log=lambda *a: None, maps=maps)
                self.assertEqual(hashlib.md5(img).hexdigest(), image_md5)

    def test_fastfiles(self):
        """The same zones as .ff files (the container needs the zone key in the game's C1.exe)."""
        exe = GAME / "C1.exe"
        if not exe.is_file():
            self.skipTest("no 1.1.1.13 install at %s (K33_GAME)" % GAME)
        maps = shipping.ShippingMaps.load(str(ROOT), "ship")
        with tempfile.TemporaryDirectory() as tmp:
            for zone, (_img, ff_md5) in PINNED.items():
                with self.subTest(zone=zone):
                    out, _z, _m, _p = build.build(str(ROOT), zone, str(exe), NoStocks(), tmp, log=lambda *a: None,
                                                  maps=maps)
                    self.assertEqual(hashlib.md5(Path(out).read_bytes()).hexdigest(), ff_md5)

    def test_generators_reproduce_the_checked_in_menus(self):
        """tools/build_survival_kiosks.py and build_survival_status.py write exactly the checked-in .menu text
        (tools/build_hud_styles.py's two menufiles: tests/test_hud_styles.py)."""
        with tempfile.TemporaryDirectory() as tmp:
            for tool, files in (("build_survival_status", ["zones/survival_hud_postgfx_mp/components/survival_status.menu"]),
                                ("build_survival_kiosks", ["zones/patch_common_gametype_survival/menufile/kiosk_locker.menu",
                                                           "zones/patch_common_gametype_survival/menufile/kiosk_killstreak.menu"])):
                with self.subTest(tool=tool):
                    before = {f: (ROOT / f).read_bytes() for f in files}
                    csv = ROOT / "zone_source/patch_common_gametype_survival.csv"
                    csv_before = csv.read_bytes()
                    try:
                        subprocess.run([sys.executable, "-B", str(ROOT / "tools" / (tool + ".py"))], check=True,
                                       capture_output=True, cwd=tmp)
                        for f in files:
                            self.assertEqual((ROOT / f).read_bytes(), before[f], f)
                        self.assertEqual(csv.read_bytes(), csv_before)
                    finally:
                        for f, data in before.items():
                            (ROOT / f).write_bytes(data)
                        csv.write_bytes(csv_before)


if __name__ == "__main__":
    unittest.main()
