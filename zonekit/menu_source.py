# -*- coding: utf-8 -*-
r"""The .menu language: IW-style menu source text <-> the 1.1.1.13 struct tree of zonekit/menus.py.

    compile_menufile(text, where, name)   .menu text -> a MenuList (menufile, asset type 25) named `name`
    compile_menu(text, where)             .menu text with one menuDef -> a menuDef_t (menu, asset type 26)
    load(path, name=None, menu=False)     the same from a file
    decompile_menufile(node) / decompile_menu(node)
                                          the struct tree -> .menu text. A decompile compiles its own text again and
                                          refuses to return text that does not give back the same tree
                                          (menus.normalize: what the zone stores), so its output is always an exact
                                          source of the asset.
    parse(text, where)                    the HUD-style dialect of tools/build_hud_styles.py (sources/sm2.menu), below
    python -m zonekit.menu_source decompile|compile ...      the command line (main())

The language is the IW3/IW4 menu language (the menuDef / itemDef files of ui_mp/*.menu) for what 1.1.1.13 stores.

Preprocessor
    // and /* */ comments. #define NAME tokens (one line; a trailing backslash continues it): NAME is replaced wherever
    it appears as a whole word outside a string, recursively. #undef NAME. #include "file": its lines in place, looked
    up beside the including file, then in zonekit/menu_include/. Every error names file:line.

A file
    { menuDef { ... } menuDef { ... } }   (the outer braces are optional). A menufile's own name (its zone asset name,
                                          "ui_mp/scriptmenus/x.menu") is not in the text: its manifest line gives it.
                                          A menu asset (type 26) is a file with one menuDef.

menuDef keywords (offsets: menuDef_t in zonekit/menus.py, W = sizeof(windowDef_t) = 0xA4)
    name "s"  group "s"  rect x y w h [horz vert]  style N (WINDOW_STYLE_*)  border N  borderSize f  ownerdraw N
    ownerdrawFlag N  forecolor r g b a (default 1 1 1 1)  backcolor ..  bordercolor ..  outlinecolor ..  disablecolor ..
    background "material"  visible 1 | 0
    decoration  autowrapped  popup  outOfBoundsClick  legacySplitScreenScale  hiddenDuringFlash  hiddenDuringScope
    hiddenDuringUI  textOnlyFocus      (windowDef_t.staticFlags, IW4's bits 0x100000 0x800000 0x1000000 0x2000000
                                        0x4000000 0x10000000 0x20000000 0x40000000 0x80000000)
    font "s"  fullScreen N  fadeCycle N  fadeClamp f  fadeAmount f  fadeInAmount f  blurWorld f  imageTrack N
    focusColor r g b a (default 1 1 1 1)  allowedBinding "s"  soundLoop "s"
    onOpen { }  onClose { }  onRequestClose { }  onESC { }       event handlers (below)
    execKey "c" { }  execKeyInt N { }                             key handlers, kept in source order
    visible when EXPR;  exp rect X|Y|W|H EXPR;  exp openSound EXPR;  exp closeSound EXPR;
    itemDef { ... }
  menuDef fields IW4 has no keyword for (written by the decompiler only when they are not the default):
    menuExtra N        the int at W+8 that 1.1.1.13 adds before itemCount (IW4's menuDef_t has none). What C1 does with
                       it is not measured: 1.1.1.13's own HUD menus carry 1, the popups converted from 2.6.9.7 carry 0.
    rectClient x y w h horz vert   windowDef_t.rectClient of a menu (0 0 0 0 in every 1.1.1.13 menu)
    cursorItem N (default -1)  fontIndex N  nextTime N   fields the game sets at run time, kept as the zone has them

itemDef keywords: the window keywords above and
    rect x y w h [horz vert]   the item's rect in its menu (windowDef_t.rectClient). The zone also stores the laid-out
                       rect (windowDef_t.rect): by default rectClient moved by its menu's rect x y, with the menu's
                       alignment when the item's is 0 0 (what 1.1.1.13's own compiler wrote; C1 lays it out again at load)
    layoutRect x y w h horz vert   extension: the stored windowDef_t.rect when it is not that (converted and authored
                       items: theirs is their rectClient)
    type N (ITEM_TYPE_*)  align N  textfont N  textalign N  textalignx f  textaligny f  textscale f (default 0.55)
    textstyle N (ITEM_TEXTSTYLE_*)  gamemsgwindowindex N  gamemsgwindowmode N  text "s"  dvar "s"  dvarTest "s"
    localvar "s"  focusSound "sound"  imageTrack N  glowColor r g b a  feeder f (itemDef_s.special; also "special")
    mouseEnterText { }  mouseExitText { }  mouseEnter { }  mouseExit { }  action { }  accept { }  onFocus { }
    leaveFocus { }  execKey "c" { }  execKeyInt N { }
    enableDvar { values }  disableDvar { }  showDvar { }  hideDvar { }  focusDvar { }   (dvarFlags 1 2 4 8 0x10; the
                       values are stored like a script)
    visible when EXPR;  disabled when EXPR;  exp text EXPR;  exp material EXPR;
    exp rect X|Y|W|H EXPR;  exp forecolor R|G|B|RGB|A EXPR;  exp glowcolor ..;  exp backcolor ..;
                       the item's float expressions (ItemFloatExpression), kept in source order
    list box (type 6): elementWidth f  elementHeight f  elementType N  drawPadding N  columns N  pos width maxChars
                       align (N times)  notselectable  noScrollBars  usePaging  selectBorder r g b a
                       selectIcon "material"  doubleClick { }   (listBoxDef_s; listBox alone: all of it 0)
    multi (type 12): dvarStrList { "label" "value" ... }  dvarFloatList { "label" f ... }
    dvar enum (type 13): dvarEnumList "s"
    edit field (types 0 4 9 10 11 14 16 17 18 22 23): maxChars N  maxCharsGotoNext N  maxPaintChars N
  itemDef extensions:
    editField          the item has an edit field data block (editFieldDef_s) with every field 0: 1.1.1.13's own
                       compiler gave one to many text items; without a keyword a text item has none
    minValue f  maxValue f  defValue f  range f  paintOffset N   the other editFieldDef_s fields
    floatExpressionsEmpty   the floatExpressions pointer is set though there are no entries (the 2.6.9.7 conversions)
    mousePos N  startPos N  endPos N   listBoxDef_s run-time fields
    dataType N  itemFlags N  dvarFlags N  cursorPos N  textRect x y w h horz vert  decayActive N  fxBirthTime N
    fxLetterTime N  fxDecayStartTime N  fxDecayDuration N  lastSoundPlayedTime N
                       itemDef_s fields the game sets or derives, kept as the zone has them
  both:
    staticFlags N      static flag bits that have no keyword (added to the keywords' bits)
    dynamicFlags N     the whole dynamic flags word, when it is not what visible / forecolor give: "visible 1" sets
                       0x4 (WINDOW_FLAG_VISIBLE) and "forecolor" sets 0x10000 (WINDOW_FLAG_NON_DEFAULT_FORECOLOR), as
                       IW4's compiler does
  Numbers may be the constants ITEM_TYPE_* (TEXT 0, BUTTON 1, .. LISTBOX 6, .. OWNERDRAW 8, .. MULTI 12, DVARENUM 13,
  .. TEXTSCROLL 21), WINDOW_STYLE_* (EMPTY 0, FILLED 1, GRADIENT 2, SHADER 3, ..), ITEM_TEXTSTYLE_* (NORMAL 0,
  SHADOWED 3, SHADOWEDMORE 6); flags may be hex (0x..).

Event handlers (the { } after onOpen, action, ..: a MenuEventHandlerSet)
    commands           the tokens up to the next if / else / setLocalVar.. or "}" are one script handler (type 0).
                       The zone stores a script as IW's compiler wrote it: a one-character token as it is, any other
                       in quotes, each followed by a space (close menu; -> "close" "menu" ; ). Here a token is written
                       bare, or "in quotes" when it has spaces, quotes or ; { }, or is one of the keywords; quoting
                       never changes what is stored. ";" ends a command (the decompiler puts one per line):
                       setdvar name value; open menu; close self; exec "cmd"; uiScript name args;
                       scriptMenuResponse "x"; play "sound"; lerp alpha from 0 to 1 over 0.15; ...
    if EXPR { ... }    a ConditionalScript (type 1); "else { ... }" right after it is the else handler (type 2)
    setLocalVarBool name EXPR;  setLocalVarInt ..;  setLocalVarFloat ..;  setLocalVarString ..;   (types 3..6)

Expressions (Statement_s: the infix entry stream C1's evaluator reads; one token here is one stored entry)
    EXPR runs to the ";" (after "if": to the "{"). 1 is an int operand, 1.0 a float one (the stored type is kept);
    "s" a string operand; -1 written together is a negative number (after an operand it is "minus 1"); a - 1 is the
    minus operator. ( ) , and == != < <= > >= && || ! + - * / % & | ~ << >> are operator entries.
    Functions are 1.1.1.13's operators (zonekit/menus.py OPS_1113, case-insensitive), in two forms:
      name( args )     IW4's call form: the function's operator, its arguments and the ")" (the "(" is part of the call
                       and is not stored)
      name args        the operator alone. 1.1.1.13's own menus store a call as ( name args ): their "(" comes before
                       the name, e.g. ( dvarint "ui_x" ) == 1, ( ui_active ), ( tablelookup "t" , 0 , 1 , 2 ).
      The table's static dvar operators, dvarint(static) .. dvarstring(static), are named staticdvarint,
      staticdvarbool, staticdvarfloat, staticdvarstring.
    [ EXPR ]           extension: a whole statement as one operand (operand type 3, a "function" in IW4's terms),
                       evaluated on its own: 1.1.1.13's HUD menus nest their conditions that way.

The HUD-style dialect (parse(); tools/build_hud_styles.py's authored styles, sources/sm2.menu): a menuDef names the
1.1.1.13 host menu its items join, and has only name, visible when ( .. ), stockItems N .. | all (the host's own items
it keeps), itemOffset ( x ) ( y ) (added to the x and y of every later item: "exp rect X ( x + ( exprX ) )") and
itemDef. Its items start as the host HUD's own items (static 0x100000 decoration, dynamic 0x10004: visible, forecolor
set), "rect" is both rects, an expression is one ( .. ) group (";" optional) and functions use the call form.
"""
from __future__ import annotations

import os
import re
import struct
from decimal import Decimal

from zonekit import menus

W = menus.W13
OP = menus.OP_1113
FIRST_FUNCTION = OP["dvarint(static)"]
INCLUDE_DIRS = [os.path.join(os.path.dirname(os.path.abspath(__file__)), "menu_include")]


class MenuSourceError(Exception):
    pass


# --- names -----------------------------------------------------------------------------------------------------------
def source_name(op_name):
    """The .menu name of an operator of OPS_1113 ("dvarint(static)" -> "staticdvarint")."""
    return "static" + op_name[:-8] if op_name.endswith("(static)") else op_name


FUNCTIONS = {source_name(n).lower(): i for i, n in enumerate(menus.OPS_1113) if i >= FIRST_FUNCTION}
FUNCTIONS["noop"] = OP["NOOP"]
SYMBOLS = {n: i for i, n in enumerate(menus.OPS_1113[:FIRST_FUNCTION]) if n != "NOOP"}
ITEM_TYPES = ["TEXT", "BUTTON", "RADIOBUTTON", "CHECKBOX", "EDITFIELD", "COMBO", "LISTBOX", "MODEL", "OWNERDRAW",
              "NUMERICFIELD", "SLIDER", "YESNO", "MULTI", "DVARENUM", "BIND", "MENUMODEL", "VALIDFILEFIELD",
              "DECIMALFIELD", "UPREDITFIELD", "GAME_MESSAGE_WINDOW", "NEWSTICKER", "TEXTSCROLL", "EMAILFIELD",
              "PASSWORDFIELD"]
WINDOW_STYLES = ["EMPTY", "FILLED", "GRADIENT", "SHADER", "TEAMCOLOR", "DVAR_SHADER", "LOADBAR"]
TEXT_STYLES = {"NORMAL": 0, "SHADOWED": 3, "SHADOWEDMORE": 6}
CONSTANTS = {"ITEM_TYPE_" + n: i for i, n in enumerate(ITEM_TYPES)}
CONSTANTS.update({"WINDOW_STYLE_" + n: i for i, n in enumerate(WINDOW_STYLES)})
CONSTANTS.update({"ITEM_TEXTSTYLE_" + n: v for n, v in TEXT_STYLES.items()})
# the HUD-style dialect's alignment names (1.1.1.13's numbering: 8 left / top, 9 centre, 10 right / bottom)
CONSTANTS.update({"HORIZONTAL_ALIGN_LEFT": 8, "HORIZONTAL_ALIGN_CENTER": 9, "HORIZONTAL_ALIGN_RIGHT": 10,
                  "VERTICAL_ALIGN_TOP": 8, "VERTICAL_ALIGN_CENTER": 9, "VERTICAL_ALIGN_BOTTOM": 10})
STATIC_FLAGS = [("decoration", 0x100000), ("autowrapped", 0x800000), ("popup", 0x1000000),
                ("outOfBoundsClick", 0x2000000), ("legacySplitScreenScale", 0x4000000),
                ("hiddenDuringFlash", 0x10000000), ("hiddenDuringScope", 0x20000000), ("hiddenDuringUI", 0x40000000),
                ("textOnlyFocus", 0x80000000)]
DYN_VISIBLE, DYN_FORECOLOR = 0x4, 0x10000
DVAR_FLAGS = [("enableDvar", 0x1), ("disableDvar", 0x2), ("showDvar", 0x4), ("hideDvar", 0x8), ("focusDvar", 0x10)]
FLOAT_TARGETS = {("rect", "x"): 0, ("rect", "y"): 1, ("rect", "w"): 2, ("rect", "h"): 3}
for _base, _first in (("forecolor", 4), ("glowcolor", 9), ("backcolor", 14)):
    for _k, _c in enumerate(("r", "g", "b", "rgb", "a")):
        FLOAT_TARGETS[(_base, _c)] = _first + _k
TARGET_NAMES = {v: k for k, v in FLOAT_TARGETS.items()}
MENU_EVENTS = [("onOpen", W + 0x2C), ("onRequestClose", W + 0x30), ("onClose", W + 0x34), ("onESC", W + 0x38)]
ITEM_EVENTS = [(n, W + 0x4C + 4 * i) for i, n in enumerate(menus.EVENTS)]
MENU_EXPS = [("rect X", W + 0x60), ("rect Y", W + 0x64), ("rect W", W + 0x68), ("rect H", W + 0x6C),
             ("openSound", W + 0x70), ("closeSound", W + 0x74)]
ITEM_STATEMENTS = [("visible when", W + 0xA0), ("disabled when", W + 0xA4), ("exp text", W + 0xA8),
                   ("exp material", W + 0xAC)]
EDIT_TYPES = (4, 9, 16, 18, 11, 14, 10, 0, 17, 22, 23)
HANDLER_KEYWORDS = ("if", "else", "setlocalvarbool", "setlocalvarint", "setlocalvarfloat", "setlocalvarstring")
SET_LOCAL = {"setlocalvarbool": 3, "setlocalvarint": 4, "setlocalvarfloat": 5, "setlocalvarstring": 6}
SET_LOCAL_NAMES = {3: "setLocalVarBool", 4: "setLocalVarInt", 5: "setLocalVarFloat", 6: "setLocalVarString"}

# simple fields: (keyword, offset, format); i int, x flags (hex), f float, c colour (4 floats), b a flag keyword (1)
WINDOW_FIELDS = [("style", 0x30, "i"), ("border", 0x34, "i"), ("borderSize", 0x40, "f"), ("ownerdraw", 0x38, "i"),
                 ("ownerdrawFlag", 0x3C, "x"), ("nextTime", 0x4C, "i"), ("backcolor", 0x60, "c"),
                 ("bordercolor", 0x70, "c"), ("outlinecolor", 0x80, "c"), ("disablecolor", 0x90, "c")]
MENU_FIELDS = [("fullScreen", W + 4, "i"), ("menuExtra", W + 8, "i"), ("fontIndex", W + 0x10, "i"),
               ("cursorItem", W + 0x14, "i"), ("fadeCycle", W + 0x18, "i"), ("fadeClamp", W + 0x1C, "f"),
               ("fadeAmount", W + 0x20, "f"), ("fadeInAmount", W + 0x24, "f"), ("blurWorld", W + 0x28, "f"),
               ("imageTrack", W + 0x4C, "i"), ("focusColor", W + 0x50, "c")]
ITEM_FIELDS = [("type", W + 0x14, "i"), ("dataType", W + 0x18, "i"), ("align", W + 0x1C, "i"),
               ("textfont", W + 0x20, "i"), ("textalign", W + 0x24, "i"), ("textalignx", W + 0x28, "f"),
               ("textaligny", W + 0x2C, "f"), ("textscale", W + 0x30, "f"), ("textstyle", W + 0x34, "i"),
               ("gamemsgwindowindex", W + 0x38, "i"), ("gamemsgwindowmode", W + 0x3C, "i"),
               ("itemFlags", W + 0x44, "x"), ("dvarFlags", W + 0x80, "x"), ("feeder", W + 0x88, "f"),
               ("cursorPos", W + 0x8C, "i"), ("imageTrack", W + 0x94, "i"), ("glowColor", W + 0xB0, "c"),
               ("decayActive", W + 0xC0, "i"), ("fxBirthTime", W + 0xC4, "i"), ("fxLetterTime", W + 0xC8, "i"),
               ("fxDecayStartTime", W + 0xCC, "i"), ("fxDecayDuration", W + 0xD0, "i"),
               ("lastSoundPlayedTime", W + 0xD4, "i")]
LISTBOX_FIELDS = [("mousePos", 0, "i"), ("startPos", 4, "i"), ("endPos", 8, "i"), ("drawPadding", 0xC, "i"),
                  ("elementWidth", 0x10, "f"), ("elementHeight", 0x14, "f"), ("elementType", 0x18, "i"),
                  ("notselectable", 0x124, "b"), ("noScrollBars", 0x128, "b"), ("usePaging", 0x12C, "b"),
                  ("selectBorder", 0x130, "c")]
EDIT_FIELDS = [("minValue", 0, "f"), ("maxValue", 4, "f"), ("defValue", 8, "f"), ("range", 0xC, "f"),
               ("maxChars", 0x10, "i"), ("maxCharsGotoNext", 0x14, "i"), ("maxPaintChars", 0x18, "i"),
               ("paintOffset", 0x1C, "i")]


def _table(fields):
    return {n.lower(): (o, f) for n, o, f in fields}


_STATIC = {n.lower(): b for n, b in STATIC_FLAGS}
_WINDOW_FIELD, _MENU_FIELD, _ITEM_FIELD = _table(WINDOW_FIELDS), _table(MENU_FIELDS), _table(ITEM_FIELDS)
_LISTBOX_FIELD, _EDIT_FIELD = _table(LISTBOX_FIELDS), _table(EDIT_FIELDS)
_MENU_EVENT = {n.lower(): o for n, o in MENU_EVENTS}
_ITEM_EVENT = {n.lower(): o for n, o in ITEM_EVENTS}
_DVAR_FLAG = {n.lower(): b for n, b in DVAR_FLAGS}


def base_menu():
    """A menuDef_t with every field at its default."""
    raw = bytearray(menus.MENU13)
    struct.pack_into("<4f", raw, 0x50, 1, 1, 1, 1)
    struct.pack_into("<i", raw, W + 0x14, -1)
    struct.pack_into("<4f", raw, W + 0x50, 1, 1, 1, 1)
    return raw


def base_item():
    """An itemDef_s with every field at its default."""
    raw = bytearray(menus.ITEM13)
    struct.pack_into("<4f", raw, 0x50, 1, 1, 1, 1)
    struct.pack_into("<f", raw, W + 0x30, 0.55)
    return raw


def hud_item():
    """The HUD-style dialect's blank item: the flags every 1.1.1.13 HUD item has (static 0x100000 decoration,
    dynamic 0x10004: visible, forecolor set), colours white / clear, text scale 0.55."""
    raw = base_item()
    struct.pack_into("<I", raw, 0x44, 0x100000)
    struct.pack_into("<I", raw, 0x48, 0x10004)
    return raw


# --- numbers and strings -----------------------------------------------------------------------------------------------
def fmt_float(v):
    """The shortest decimal that gives back the same float32, without an exponent."""
    if v != v or v in (float("inf"), float("-inf")):
        raise menus.MenuError("float %r has no source form" % v)
    want = struct.pack("<f", v)
    for n in range(1, 10):
        s = "%.*g" % (n, v)
        if struct.pack("<f", float(s)) == want:
            break
    if "e" in s:
        s = format(Decimal(s), "f")
    return s


def fmt_float_operand(v):
    s = fmt_float(v)
    return s if "." in s else s + ".0"


def quote(s):
    """A string of the tree (code points 0..255 = the stored bytes) as a "..." literal (UTF-8 text when it is)."""
    b = s.encode("latin-1")
    try:
        chars = b.decode("utf-8")
        byte_mode = False
    except UnicodeDecodeError:
        chars, byte_mode = [chr(o) for o in b], True
    out = []
    for ch in chars:
        o = ord(ch)
        if ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif o < 0x20 or o == 0x7F or (byte_mode and o >= 0x80):
            out.append("\\x%02x" % o)
        else:
            out.append(ch)
    return '"' + "".join(out) + '"'


def unquote(lit):
    """A "..." literal -> the tree's string (the bytes as code points 0..255). Escapes: \\" \\\\ \\n \\t \\xNN."""
    body, out, k = lit[1:-1], bytearray(), 0
    while k < len(body):
        ch = body[k]
        if ch == "\\" and k + 1 < len(body):
            e = body[k + 1]
            if e == "x" and re.match(r"[0-9a-fA-F]{2}$", body[k + 2:k + 4]):
                out.append(int(body[k + 2:k + 4], 16))
                k += 4
                continue
            out += {"n": b"\n", "t": b"\t", '"': b'"', "\\": b"\\"}.get(e, ("\\" + e).encode("utf-8"))
            k += 2
            continue
        out += ch.encode("utf-8")
        k += 1
    return out.decode("latin-1")


# --- preprocessor ------------------------------------------------------------------------------------------------------
def _strip_comments(text):
    out, k, n, q = [], 0, len(text), False
    while k < n:
        ch = text[k]
        if q:
            out.append(ch)
            if ch == "\\" and k + 1 < n:
                out.append(text[k + 1])
                k += 2
                continue
            if ch in '"\n':
                q = False
            k += 1
        elif ch == '"':
            q = True
            out.append(ch)
            k += 1
        elif text.startswith("//", k):
            e = text.find("\n", k)
            k = n if e < 0 else e
        elif text.startswith("/*", k):
            e = text.find("*/", k + 2)
            e = n if e < 0 else e + 2
            out.append("\n" * text.count("\n", k, e))
            k = e
        else:
            out.append(ch)
            k += 1
    return "".join(out)


_WORD = re.compile(r'"(?:[^"\\\n]|\\.)*"|[A-Za-z_][A-Za-z0-9_]*')


def _expand(text, defines, seen=frozenset()):
    def sub(m):
        w = m.group(0)
        if w[0] != '"' and w in defines and w not in seen:
            return _expand(defines[w], defines, seen | {w})
        return w
    return _WORD.sub(sub, text) if defines else text


def preprocess(text, where="menu", defines=None, depth=0):
    """-> [(file, line number, text)]: comments gone, #include files spliced in, #define names replaced."""
    if depth > 16:
        raise MenuSourceError("%s: #include nested too deep" % where)
    defines = {} if defines is None else defines
    lines = _strip_comments(text.replace("\r\n", "\n")).split("\n")
    out, i = [], 0
    while i < len(lines):
        line, no = lines[i], i + 1
        if line.lstrip().startswith("#"):
            full = line
            while full.rstrip().endswith("\\") and i + 1 < len(lines):
                i += 1
                full = full.rstrip()[:-1] + " " + lines[i]
            m = re.match(r"\s*#\s*(\w*)\s*(.*)$", full)
            d, rest = m.group(1), m.group(2).strip()
            if d == "define":
                dm = re.match(r"([A-Za-z_][A-Za-z0-9_]*)\s*(.*)$", rest)
                if not dm:
                    raise MenuSourceError("%s:%d: #define without a name" % (where, no))
                defines[dm.group(1)] = dm.group(2)
            elif d == "undef":
                defines.pop(rest, None)
            elif d == "include":
                im = re.match(r'"([^"]+)"|<([^>]+)>', rest)
                if not im:
                    raise MenuSourceError('%s:%d: #include needs "file"' % (where, no))
                name = im.group(1) or im.group(2)
                path = _find_include(name, where)
                if path is None:
                    raise MenuSourceError("%s:%d: #include %s: not found" % (where, no, name))
                with open(path, encoding="utf-8") as f:
                    out += preprocess(f.read(), path, defines, depth + 1)
            else:
                raise MenuSourceError("%s:%d: unknown directive #%s (#define, #undef and #include are supported)"
                                      % (where, no, d))
        else:
            out.append((where, no, _expand(line, defines)))
        i += 1
    return out


def _find_include(name, where):
    dirs = [os.path.dirname(os.path.abspath(where))] if os.path.isfile(where) else []
    for d in dirs + [os.getcwd()] + INCLUDE_DIRS:
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    return None


# --- lexer -----------------------------------------------------------------------------------------------------------
_NUM = r"-?(?:0[xX][0-9a-fA-F]+|(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?)"
LEX = {
    # keywords, numbers, strings and expressions
    "x": re.compile(r'(?P<str>"(?:[^"\\]|\\.)*")|(?P<num>%s)|(?P<call>[A-Za-z_][A-Za-z0-9_]*\()|'
                    r'(?P<id>[A-Za-z_][A-Za-z0-9_]*)|(?P<op>==|!=|<=|>=|&&|\|\||<<|>>|[-+*/%%<>!&|~(){}\[\],;])'
                    % _NUM),
    # script tokens of an event handler
    "s": re.compile(r'(?P<str>"(?:[^"\\]|\\.)*")|(?P<op>[;{}])|(?P<word>[^\s"{};]+)'),
}


class Tok:
    __slots__ = ("kind", "text", "where", "line", "pos")

    def __init__(self, kind, text, where, line, pos):
        self.kind, self.text, self.where, self.line, self.pos = kind, text, where, line, pos

    def at(self):
        return "%s:%d" % (self.where, self.line)

    def __repr__(self):
        return "%s %r" % (self.kind, self.text)


class Src:
    """The preprocessed lines, read a token at a time in the mode the parser asks for."""

    def __init__(self, lines):
        self.lines, self.li, self.ci, self.pending = lines, 0, 0, []

    def _skip(self):
        while self.li < len(self.lines):
            text = self.lines[self.li][2]
            while self.ci < len(text) and text[self.ci].isspace():
                self.ci += 1
            if self.ci < len(text):
                return True
            self.li, self.ci = self.li + 1, 0
        return False

    def take(self, mode="x"):
        if self.pending:
            return self.pending.pop()
        if not self._skip():
            where, line = (self.lines[-1][0], self.lines[-1][1]) if self.lines else ("menu", 0)
            return Tok("eof", None, where, line, (self.li, 0))
        where, line, text = self.lines[self.li]
        m = LEX[mode].match(text, self.ci)
        if not m:
            raise MenuSourceError("%s:%d: cannot read %r" % (where, line, text[self.ci:self.ci + 20]))
        t = Tok(m.lastgroup, m.group(m.lastgroup), where, line, (self.li, self.ci))
        self.ci = m.end()
        return t

    def peek(self, mode="x"):
        if self.pending:
            return self.pending[-1]
        li, ci = self.li, self.ci
        try:
            return self.take(mode)
        finally:
            self.li, self.ci = li, ci

    def push(self, tok):
        self.pending.append(tok)

    def seek(self, tok, skip):
        """Go back to `skip` characters after the start of `tok` (a token read in another mode)."""
        self.li, self.ci = tok.pos[0], tok.pos[1] + skip

    def number_follows_on(self, line):
        t = self.peek()
        return t.kind != "eof" and t.line == line and (t.kind == "num" or (t.kind == "op" and t.text == "-") or
                                                       (t.kind == "id" and t.text in CONSTANTS))


def err(tok, msg):
    return MenuSourceError("%s: %s" % (tok.at(), msg))


# --- tree nodes --------------------------------------------------------------------------------------------------------
def node(kind, raw, p=None):
    return {"kind": kind, "raw": bytes(raw).hex().upper(), "p": p or {}}


def e_op(i):
    return node("expressionEntry", struct.pack("<III", 0, i, 0))


def e_int(v):
    return node("expressionEntry", struct.pack("<IIi", 1, 0, v))


def e_float(v):
    return node("expressionEntry", struct.pack("<IIf", 1, 1, v))


def e_string(s):
    return node("expressionEntry", struct.pack("<III", 1, 2, 0), {"8": s})


def e_function(stmt):
    return node("expressionEntry", struct.pack("<III", 1, 3, 0), {"8": stmt})


def statement(entries):
    return node("Statement", struct.pack("<IIIII", len(entries), 0, 0, 0, 0), {"4": entries})


def handler_set(handlers):
    return node("MenuEventHandlerSet", struct.pack("<II", len(handlers), 0), {"4": handlers})


def handler(t, data):
    return node("MenuEventHandler", struct.pack("<IB3x", 0, t), {"0": data})


def script_string(tokens):
    """Script tokens as the zone stores them (IW's compiler: one character bare, any other quoted, then a space)."""
    return "".join((t if len(t) == 1 else '"%s"' % t) + " " for t in tokens)


def _entry(e):
    return struct.unpack("<IIi", bytes.fromhex(e["raw"]))


def _num_value(text):
    t = text.lower()
    if "0x" in t:
        return int(t, 16)
    if "." in t or "e" in t:
        return float(t)
    return int(t)


def _ends_operand(es):
    if not es:
        return False
    t, a, _b = _entry(es[-1])
    return t == 1 or a == SYMBOLS[")"]


def _is_operator_not_close(es):
    if not es:
        return True
    t, a, _b = _entry(es[-1])
    return t == 0 and a != SYMBOLS[")"]


def _chain(keys):
    for a, b in zip(keys, keys[1:]):
        a["p"]["8"] = b
    return keys[0]


# --- the parser --------------------------------------------------------------------------------------------------------
class Parser:
    def __init__(self, lines, style=False):
        self.s = Src(lines)
        self.style = style          # the HUD-style dialect (parse())

    def take(self, mode="x"):
        return self.s.take(mode)

    def peek(self, mode="x"):
        return self.s.peek(mode)

    def expect(self, text, mode="x"):
        t = self.take(mode)
        if t.text != text:
            raise err(t, "expected %r, got %r" % (text, t.text if t.kind != "eof" else "the end of the file"))
        return t

    def opt_semicolon(self):
        if self.peek().text == ";":
            self.take()

    def string(self):
        t = self.take()
        if t.kind != "str":
            raise err(t, "expected a string, got %r" % t.text)
        return unquote(t.text)

    def number(self, want=None):
        t = self.take()
        sign = 1
        if t.kind == "op" and t.text == "-":
            sign, t = -1, self.take()
        if t.kind == "id" and t.text in CONSTANTS:
            v = CONSTANTS[t.text] * sign
        elif t.kind == "num":
            v = _num_value(t.text) * sign
        else:
            raise err(t, "expected a number, got %r" % t.text)
        if want == "i":
            if isinstance(v, float):
                if v != int(v):
                    raise err(t, "expected an integer, got %s" % t.text)
                v = int(v)
            if not -0x80000000 <= v <= 0xFFFFFFFF:
                raise err(t, "%s does not fit 32 bits" % t.text)
        return v

    def numbers_on_line(self, n_min, n_max):
        first = self.peek()
        out = []
        while len(out) < n_max and self.s.number_follows_on(first.line):
            out.append(self.number())
        if len(out) < n_min:
            raise err(first, "expected %d numbers on this line" % n_min)
        return out

    def rect(self):
        first = self.peek()
        vals = self.numbers_on_line(4, 6)
        x, y, w, h = (float(a) for a in vals[:4])
        horz, vert = (int(vals[4]), int(vals[5])) if len(vals) == 6 else (0, 0)
        if not (0 <= horz < 256 and 0 <= vert < 256):
            raise err(first, "rect alignment %d %d out of range" % (horz, vert))
        return struct.pack("<4fBBxx", x, y, w, h, horz, vert)

    def color(self):
        return struct.pack("<4f", *(float(self.number()) for _ in range(4)))

    def field(self, raw, off, fmt):
        if fmt == "c":
            raw[off:off + 16] = self.color()
        elif fmt == "f":
            struct.pack_into("<f", raw, off, float(self.number()))
        elif fmt == "b":
            struct.pack_into("<i", raw, off, 1)
        else:
            v = self.number("i")
            struct.pack_into("<I" if v > 0x7FFFFFFF else "<i", raw, off, v)

    # expressions
    def expression(self, stop=";"):
        """An expression up to `stop` (";" is consumed, "{" left for the caller) -> a Statement."""
        if self.style:
            return self.group_expression()
        es = self.entries((stop,))
        if stop == ";":
            self.expect(";")
        return statement(es)

    def entries(self, stops):
        es, first = [], self.peek()
        while True:
            t = self.peek()
            if t.kind == "eof":
                raise err(first, "the expression does not end (%s expected)" % " or ".join(stops))
            if t.kind == "op" and t.text in stops:
                if not es:
                    raise err(t, "an empty expression")
                return es
            self.take()
            self.entry(t, es)

    def entry(self, t, es):
        if t.kind == "str":
            es.append(e_string(unquote(t.text)))
        elif t.kind == "num":
            v = _num_value(t.text)
            if v < 0 and _ends_operand(es):         # a -1 after an operand: the minus operator and 1
                es.append(e_op(SYMBOLS["-"]))
                v = -v
            if isinstance(v, int) and not -0x80000000 <= v <= 0x7FFFFFFF:
                raise err(t, "%s does not fit an int operand" % t.text)
            es.append(e_float(v) if isinstance(v, float) else e_int(v))
        elif t.kind == "call":
            i = FUNCTIONS.get(t.text[:-1].lower())
            if i is None or i == OP["NOOP"]:
                raise err(t, "%s is not a function of 1.1.1.13's menu expressions" % t.text[:-1])
            es.append(e_op(i))
        elif t.kind == "id":
            i = FUNCTIONS.get(t.text.lower())
            if i is None:
                raise err(t, "%s is not a function of 1.1.1.13's menu expressions" % t.text)
            if self.style:
                raise err(t, "%s must be called: %s( .. )" % (t.text, t.text))
            es.append(e_op(i))
        elif t.kind == "op" and t.text == "[" and not self.style:
            sub = self.entries(("]",))
            self.take()
            es.append(e_function(statement(sub)))
        elif t.kind == "op" and t.text in SYMBOLS:
            es.append(e_op(SYMBOLS[t.text]))
        else:
            raise err(t, "unexpected %r in an expression" % t.text)

    def group_expression(self):
        """The HUD-style dialect: one balanced ( .. ) group, its parentheses included; ";" optional after it."""
        t = self.take()
        if t.text != "(":
            raise err(t, "expected ( after an expression keyword, got %r" % t.text)
        es, depth = [e_op(SYMBOLS["("])], 1
        while depth:
            t = self.take()
            if t.kind == "eof":
                raise err(t, "%d unclosed parentheses" % depth)
            if t.kind == "op" and t.text == "-" and self.peek().kind == "num" and _is_operator_not_close(es):
                v = -_num_value(self.take().text)
                es.append(e_float(v) if isinstance(v, float) else e_int(v))
                continue
            if t.kind == "call" or t.text == "(":
                depth += 1
            elif t.text == ")":
                depth -= 1
            self.entry(t, es)
        self.opt_semicolon()
        return statement(es)

    def token_group(self):
        """A balanced ( .. ) group as tokens (the HUD-style dialect's itemOffset and exp rect X / Y)."""
        out = [self.take()]
        if out[0].text != "(":
            raise err(out[0], "expected (")
        depth = 1
        while depth:
            t = self.take()
            if t.kind == "eof":
                raise err(t, "%d unclosed parentheses" % depth)
            depth += 1 if (t.kind == "call" or t.text == "(") else -1 if t.text == ")" else 0
            out.append(t)
        return out

    @staticmethod
    def compile_tokens(toks):
        sub = Parser([], style=True)
        sub.s.pending = list(reversed(toks))
        return sub.group_expression()

    # event handlers
    def handler_keyword(self):
        """if / else / setLocalVar.. when the next token is one (consumed; "if(" leaves its "(" to read), else None."""
        t = self.peek("s")
        if t.kind != "word":
            return None
        low = t.text.lower()
        for kw in HANDLER_KEYWORDS:
            if low == kw or (kw == "if" and low.startswith("if(")):
                self.take("s")
                if low != kw:
                    self.s.seek(t, len(kw))
                return kw
        return None

    def handler_block(self):
        self.expect("{")
        hs, script = [], []

        def flush():
            if script:
                hs.append(handler(0, script_string(script)))
                del script[:]
        while True:
            start = self.peek("s")
            kw = self.handler_keyword()
            if kw:
                flush()
                if kw == "if":
                    cond = self.expression(stop="{")
                    body = self.handler_block()
                    hs.append(handler(1, node("ConditionalScript", bytes(8), {"0": body, "4": cond})))
                elif kw == "else":
                    if not hs or bytes.fromhex(hs[-1]["raw"])[4] != 1:
                        raise err(start, "else without an if right before it")
                    hs.append(handler(2, self.handler_block()))
                else:
                    nt = self.take("s")
                    if nt.kind not in ("str", "word"):
                        raise err(nt, "%s needs a variable name" % start.text)
                    name = unquote(nt.text) if nt.kind == "str" else nt.text
                    hs.append(handler(SET_LOCAL[kw], node("SetLocalVarData", bytes(8),
                                                          {"0": name, "4": self.expression(";")})))
                continue
            t = self.take("s")
            if t.kind == "eof":
                raise err(t, "the handler block does not end (})")
            if t.kind == "op" and t.text == "}":
                flush()
                return handler_set(hs)
            if t.kind == "op" and t.text == "{":
                raise err(t, 'a "{" in a script (write it in quotes)')
            tok = unquote(t.text) if t.kind == "str" else t.text
            if '"' in tok:
                raise err(t, "a script token cannot hold a double quote (the zone stores the tokens in quotes)")
            script.append(tok)

    def key_handler(self, keyword, chain):
        t = self.take()
        if keyword == "execkey":
            s = unquote(t.text) if t.kind == "str" else None
            if s is None or len(s) != 1:
                raise err(t, 'execKey takes one character: execKey "a" { .. } (execKeyInt for a key number)')
            key = ord(s)
        else:
            self.s.push(t)
            key = self.number("i")
        chain.append(node("ItemKeyHandler", struct.pack("<iII", key, 0, 0), {"4": self.handler_block()}))

    def dvar_values(self):
        """{ values } of enableDvar / showDvar ..: stored like a script."""
        self.expect("{")
        toks = []
        while True:
            t = self.take("s")
            if t.kind == "eof":
                raise err(t, "the list does not end (})")
            if t.kind == "op" and t.text == "}":
                return script_string(toks)
            if t.kind == "op" and t.text == "{":
                raise err(t, "unexpected {")
            toks.append(unquote(t.text) if t.kind == "str" else t.text)

    # windows
    def window_keyword(self, key, raw, p, st):
        """A keyword every window has -> True if it was one. st: the flag keywords seen (flags())."""
        if key == "name":
            p["0"] = self.string()
        elif key == "group":
            p["44"] = self.string()
        elif key == "background":
            p["160"] = {"asset": "material", "name": "," + self.string().lstrip(",")}
        elif key == "forecolor":
            raw[0x50:0x60] = self.color()
            st["forecolor"] = True
        elif key == "visible" and self.peek().text != "when":
            st["visible"] = bool(self.number("i"))
        elif key in _STATIC:
            st["static"] |= _STATIC[key]
        elif key == "staticflags":
            st["static"] |= self.number("i") & 0xFFFFFFFF
        elif key == "dynamicflags":
            st["dynamic"] = self.number("i") & 0xFFFFFFFF
        elif key in _WINDOW_FIELD:
            self.field(raw, *_WINDOW_FIELD[key])
        else:
            return False
        return True

    @staticmethod
    def flags(raw, st, base_dynamic=0):
        struct.pack_into("<I", raw, 0x44, struct.unpack_from("<I", raw, 0x44)[0] | st["static"])
        dyn = st["dynamic"]
        if dyn is None:
            dyn = base_dynamic
            if st["visible"] is True:
                dyn |= DYN_VISIBLE
            elif st["visible"] is False:
                dyn &= ~DYN_VISIBLE
            if st["forecolor"]:
                dyn |= DYN_FORECOLOR
        struct.pack_into("<I", raw, 0x48, dyn)

    def keyword(self, what):
        """The next keyword (an identifier; "X(" written together counts as X followed by "("), or the "}" / eof."""
        t = self.take()
        if t.kind == "call":
            self.s.push(Tok("op", "(", t.where, t.line, t.pos))
            t = Tok("id", t.text[:-1], t.where, t.line, t.pos)
        if t.kind == "eof":
            raise err(t, "the %s does not end (})" % what)
        if t.kind not in ("id", "op") or (t.kind == "op" and t.text != "}"):
            raise err(t, "unexpected %r in a %s" % (t.text, what))
        return t

    def menudef(self):
        """menuDef { .. } (the keyword read) -> a menuDef node."""
        self.expect("{")
        raw, p, items, keys = base_menu(), {}, [], []
        st = {"static": 0, "dynamic": None, "visible": None, "forecolor": False}
        while True:
            t = self.keyword("menuDef")
            if t.text == "}":
                break
            key = t.text.lower()
            if key == "itemdef":
                items.append(self.itemdef())
                continue
            if self.window_keyword(key, raw, p, st):
                pass
            elif key == "rect":
                raw[4:0x18] = self.rect()
            elif key == "rectclient":
                raw[0x18:0x2C] = self.rect()
            elif key in ("font", "allowedbinding", "soundloop"):
                p[str(W + {"font": 0, "allowedbinding": 0x44, "soundloop": 0x48}[key])] = self.string()
            elif key in _MENU_EVENT:
                p[str(_MENU_EVENT[key])] = self.handler_block()
            elif key in ("execkey", "execkeyint"):
                self.key_handler(key, keys)
            elif key == "visible":
                self.expect("when")
                p[str(W + 0x40)] = self.expression()
            elif key == "exp":
                o = self.menu_exp(t)
                p[str(o)] = self.expression()
            elif key in _MENU_FIELD:
                self.field(raw, *_MENU_FIELD[key])
            else:
                raise err(t, "unknown menuDef keyword %r" % t.text)
            self.opt_semicolon()
        self.flags(raw, st)
        if keys:
            p[str(W + 0x3C)] = _chain(keys)
        for it in items:
            self.layout(it, raw)
        if items:
            p[str(W + 0x78)] = items
        struct.pack_into("<i", raw, W + 0xC, len(items))
        return node("menuDef", raw, p)

    def exp_target(self):
        """exp <what> [component]: -> (what, component) in lower case."""
        what = self.keyword("exp")
        w = what.text.lower()
        if w in ("rect", "forecolor", "backcolor", "glowcolor"):
            return w, self.keyword("exp").text.lower()
        return w, None

    def menu_exp(self, t):
        w, c = self.exp_target()
        name = "rect " + c.upper() if w == "rect" else {"opensound": "openSound", "closesound": "closeSound"}.get(w)
        for n, o in MENU_EXPS:
            if n == name:
                return o
        raise err(t, "exp %s%s is not a menuDef expression (exp rect X|Y|W|H, openSound, closeSound)"
                  % (w, " " + c if c else ""))

    @staticmethod
    def layout(item, menu_raw):
        """The item's stored rect: rectClient moved by the menu's origin, unless layoutRect gave it."""
        lay = item.pop("_layout", None)
        raw = bytearray(bytes.fromhex(item["raw"]))
        raw[4:0x18] = lay if lay is not None else laid_out(raw, menu_raw)
        item["raw"] = raw.hex().upper()

    def itemdef(self, offset=None):
        self.expect("{")
        raw = hud_item() if self.style else base_item()
        p, floats, keys = {}, [], []
        st = {"static": 0, "dynamic": None, "visible": None, "forecolor": False}
        td = {"kind": None, "raw": None, "p": {}}
        rect_set, rect_exp, layout, empty_floats = False, {}, None, False
        while True:
            t = self.keyword("itemDef")
            if t.text == "}":
                break
            key = t.text.lower()
            if key == "rect":
                r = self.rect()
                raw[0x18:0x2C] = r
                if self.style:
                    raw[4:0x18] = r
                rect_set = True
            elif key == "layoutrect" and not self.style:
                layout = self.rect()
            elif key == "textrect":
                raw[W:W + 0x14] = self.rect()
            elif key == "visible" and self.style and self.peek().text != "when":
                if self.number("i") != 1:
                    raise err(t, "visible 0 is not supported (leave the item out)")
            elif self.window_keyword(key, raw, p, st):
                pass
            elif key == "special":
                struct.pack_into("<f", raw, W + 0x88, float(self.number()))
            elif key in _ITEM_FIELD:
                self.field(raw, *_ITEM_FIELD[key])
            elif key in ("text", "dvar", "dvartest", "localvar"):
                p[str(W + {"text": 0x40, "dvar": 0x6C, "dvartest": 0x70, "localvar": 0x7C}[key])] = self.string()
            elif key == "focussound":
                p[str(W + 0x84)] = {"asset": "sound", "name": "," + self.string().lstrip(",")}
            elif key in _DVAR_FLAG:
                p[str(W + 0x78)] = self.dvar_values()
                struct.pack_into("<I", raw, W + 0x80, struct.unpack_from("<I", raw, W + 0x80)[0] | _DVAR_FLAG[key])
            elif key in _ITEM_EVENT:
                p[str(_ITEM_EVENT[key])] = self.handler_block()
            elif key in ("execkey", "execkeyint"):
                self.key_handler(key, keys)
            elif key in ("visible", "disabled"):
                self.expect("when")
                p[str(W + (0xA0 if key == "visible" else 0xA4))] = self.expression()
            elif key == "exp":
                self.item_exp(t, p, floats, rect_exp, offset)
            elif key == "floatexpressionsempty":
                empty_floats = True
            elif key == "editfield" or key in _EDIT_FIELD:
                self.typedata(t, td, "editFieldDef", 0x20)
                if key != "editfield":
                    self.field(td["raw"], *_EDIT_FIELD[key])
            elif key in _LISTBOX_FIELD or key in ("listbox", "columns", "selecticon", "doubleclick"):
                self.typedata(t, td, "listBoxDef", 0x144)
                if key == "columns":
                    n = self.number("i")
                    if not 0 <= n <= 16:
                        raise err(t, "columns: at most 16")
                    struct.pack_into("<i", td["raw"], 0x1C, n)
                    for c in range(n):
                        struct.pack_into("<4i", td["raw"], 0x20 + 16 * c, *(self.number("i") for _ in range(4)))
                elif key == "selecticon":
                    td["p"]["320"] = {"asset": "material", "name": "," + self.string().lstrip(",")}
                elif key == "doubleclick":
                    td["p"]["288"] = self.handler_block()
                elif key != "listbox":
                    self.field(td["raw"], *_LISTBOX_FIELD[key])
            elif key in ("dvarstrlist", "dvarfloatlist"):
                self.typedata(t, td, "multiDef", 0x188)
                self.multi(td, key == "dvarstrlist")
            elif key == "dvarenumlist":
                self.typedata(t, td, "enum", 0)
                td["value"] = self.string()
            else:
                raise err(t, "unknown itemDef keyword %r" % t.text)
            self.opt_semicolon()
        if self.style and not rect_set:
            raise err(t, "an itemDef without a rect")
        self.flags(raw, st, struct.unpack_from("<I", raw, 0x48)[0] if self.style else 0)
        if self.style and offset:
            rect_xy = struct.unpack_from("<2f", raw, 0x18)
            for k, axis in enumerate(("x", "y")):
                group = _offset_group(rect_exp.get(axis, rect_xy[k]), offset[k])
                floats.insert(k, node("ItemFloatExpression", struct.pack("<II", FLOAT_TARGETS[("rect", axis)], 0),
                                      {"4": self.compile_tokens(group)}))
        if floats or empty_floats or self.style:
            p[str(W + 0x9C)] = floats
        struct.pack_into("<i", raw, W + 0x98, len(floats))
        if keys:
            p[str(W + 0x74)] = _chain(keys)
        itype = struct.unpack_from("<i", raw, W + 0x14)[0]
        if td["kind"]:
            want = {"listBoxDef": (6,), "multiDef": (12,), "enum": (13,), "editFieldDef": EDIT_TYPES}[td["kind"]]
            if itype not in want:
                raise err(t, "the item's %s keywords need its type to be %s, it is %d" % (
                    "dvarEnumList" if td["kind"] == "enum" else td["kind"], " or ".join(map(str, want)), itype))
            p[str(W + 0x90)] = td["value"] if td["kind"] == "enum" else node(td["kind"], td["raw"], td["p"])
        it = node("itemDef", raw, p)
        if not self.style:
            it["_layout"] = layout
        return it

    @staticmethod
    def typedata(t, td, kind, size):
        if td["kind"] is None:
            td["kind"], td["raw"] = kind, bytearray(size)
        elif td["kind"] != kind:
            raise err(t, "an itemDef with both %s and %s keywords" % (td["kind"], kind))

    def multi(self, td, strings):
        self.expect("{")
        raw, n = td["raw"], 0
        while self.peek().text != "}":
            if n >= 32:
                raise err(self.peek(), "at most 32 entries")
            td["p"][str(4 * n)] = self.string()
            if strings:
                td["p"][str(0x80 + 4 * n)] = self.string()
            else:
                struct.pack_into("<f", raw, 0x100 + 4 * n, float(self.number()))
            n += 1
            self.opt_semicolon()
        self.take()
        struct.pack_into("<ii", raw, 0x180, n, 1 if strings else 0)

    def item_exp(self, t, p, floats, rect_exp, offset):
        w, c = self.exp_target()
        if w in ("text", "material"):
            p[str(W + (0xA8 if w == "text" else 0xAC))] = self.expression()
            return
        target = FLOAT_TARGETS.get((w, c))
        if target is None:
            raise err(t, "exp %s%s is not supported" % (w, " " + c if c else ""))
        if self.style and w == "rect" and c in ("x", "y") and offset:
            rect_exp[c] = self.token_group()
            self.opt_semicolon()
            return
        floats.append(node("ItemFloatExpression", struct.pack("<II", target, 0), {"4": self.expression()}))

    # files
    def menudefs(self):
        out = []
        while True:
            t = self.take()
            if t.kind == "eof":
                return out
            if t.kind == "op" and t.text in ("{", "}"):
                continue
            if t.kind != "id" or t.text.lower() != "menudef":
                raise err(t, "expected menuDef, got %r" % t.text)
            out.append(self.style_menudef() if self.style else self.menudef())

    def style_menudef(self):
        self.expect("{")
        menu = {"name": None, "visible": None, "items": [], "stock": []}
        offset, start = None, self.peek()
        while True:
            t = self.take()
            if t.kind == "eof":
                raise err(t, "the menuDef does not end (})")
            if t.text == "}":
                break
            if t.text == "name":
                menu["name"] = self.string()
            elif t.text == "stockItems":
                if self.peek().text == "all":
                    self.take()
                    menu["stock"] = "all"
                else:
                    menu["stock"] += [int(n) for n in self.numbers_on_line(1, 999)]
            elif t.text == "visible":
                self.expect("when")
                menu["visible"] = self.expression()
            elif t.text == "itemOffset":
                offset = (self.token_group(), self.token_group())
                for g in offset:
                    self.compile_tokens(g)                     # reported here, not at each item
            elif t.text == "itemDef":
                menu["items"].append(self.itemdef(offset))
            else:
                raise err(t, "unknown menuDef keyword %r" % t.text)
            self.opt_semicolon()
        if not menu["name"]:
            raise err(start, "a menuDef without a name")
        return menu


def laid_out(item_raw, menu_raw):
    """The windowDef_t.rect 1.1.1.13's compiler stores for an item: its rectClient moved by the menu's rect x y,
    with the menu's alignment when the item's is 0 0."""
    x, y, w, h = struct.unpack_from("<4f", item_raw, 0x18)
    mx, my = struct.unpack_from("<2f", menu_raw, 4)
    al = bytes(item_raw[0x28:0x2C])
    if al[:2] == b"\0\0":
        al = bytes(menu_raw[0x14:0x16]) + al[2:]
    return struct.pack("<4f", x + mx, y + my, w, h) + al


def _offset_group(base, extra):
    """( ( base ) + ( extra ) ) as tokens; base is a token group or a number."""
    t0 = extra[0]

    def tok(kind, text):
        return Tok(kind, text, t0.where, t0.line, t0.pos)
    if isinstance(base, float):
        b = ([tok("op", "-")] if base < 0 else []) + [tok("num", repr(abs(base)))]
    else:
        b = list(base)
    return [tok("op", "("), tok("op", "(")] + b + [tok("op", ")"), tok("op", "+")] + list(extra) + [tok("op", ")")]


# --- compiling ---------------------------------------------------------------------------------------------------------
def compile_menus(text, where="menu"):
    """.menu text -> [menuDef node] in source order."""
    return Parser(preprocess(text, where)).menudefs()


def compile_menufile(text, where="menu", name=None):
    """.menu text -> a MenuList named `name` (the asset name the zone gives it)."""
    ms = compile_menus(text, where)
    p = {"0": name} if name is not None else {}
    p["8"] = ms
    return node("MenuList", struct.pack("<IiI", 0, len(ms), 0), p)


def compile_menu(text, where="menu"):
    """.menu text with exactly one menuDef -> that menuDef (a menu asset, type 26)."""
    ms = compile_menus(text, where)
    if len(ms) != 1:
        raise MenuSourceError("%s: a menu asset is one menuDef, the file has %d" % (where, len(ms)))
    return ms[0]


def load(path, name=None, menu=False):
    """A .menu file -> its MenuList (named `name`), or with menu=True its one menuDef."""
    with open(path, encoding="utf-8") as f:
        text = f.read()
    return compile_menu(text, str(path)) if menu else compile_menufile(text, str(path), name)


def parse(text, where="menu"):
    """The HUD-style dialect: -> [{"name": host menu, "visible": Statement or None, "items": [itemDef node, ..],
    "stock": [host item index, ..] or "all"}] in source order."""
    return Parser(preprocess(text, where), style=True).menudefs()


# --- decompiling -------------------------------------------------------------------------------------------------------
def _u32(raw, o):
    return struct.unpack_from("<I", raw, o)[0]


def _i32(raw, o):
    return struct.unpack_from("<i", raw, o)[0]


def _fmt_field(raw, o, fmt):
    if fmt == "c":
        return " ".join(fmt_float(v) for v in struct.unpack_from("<4f", raw, o))
    if fmt == "f":
        return fmt_float(struct.unpack_from("<f", raw, o)[0])
    if fmt == "x":
        return "0x%X" % _u32(raw, o)
    return str(_i32(raw, o))


def _rect_text(raw, o):
    x, y, w, h = struct.unpack_from("<4f", raw, o)
    if raw[o + 0x12] or raw[o + 0x13]:
        raise menus.MenuError("rect +%#x: its padding bytes are not 0" % o)
    return "%s %s %s %s %d %d" % (fmt_float(x), fmt_float(y), fmt_float(w), fmt_float(h), raw[o + 0x10],
                                  raw[o + 0x11])


_BARE = re.compile(r"^[!-~]+$")
_SPLIT = re.compile(r'(?:"(?:[^"\\]|\\.)*"|[^\s"])+')
_CANON = re.compile(r'"([^"]*)" |([^\s"]) ')


def script_token(tok):
    """A script token as source: bare when the script lexer reads it back as that one token, else quoted."""
    if (_BARE.match(tok) and not any(c in tok for c in '"{};\\') and not tok.startswith("#") and
            "//" not in tok and "/*" not in tok and tok.lower() not in HANDLER_KEYWORDS and
            not tok.lower().startswith("if(")):
        return tok
    return quote(tok)


def script_tokens(s):
    """A stored script string -> its tokens; MenuError unless it is in the form script_string() writes."""
    pos, toks = 0, []
    while pos < len(s):
        m = _CANON.match(s, pos)
        if not m or (m.group(1) is not None and len(m.group(1)) == 1):
            raise menus.MenuError("script %r is not in the token form IW's compiler writes" % s[:80])
        toks.append(m.group(1) if m.group(1) is not None else m.group(2))
        pos = m.end()
    if script_string(toks) != s:
        raise menus.MenuError("script %r does not round trip" % s[:80])
    return toks


def _calls(dec, c1):
    """-> (the function entries written name(, how many parentheses stay unmatched). c1: a function right after a
    "(" takes that "(" as its own (1.1.1.13's ( name args ) form)."""
    call, stack, unmatched = set(), [], 0
    for i, (t, a, _b) in enumerate(dec):
        if t != 0:
            continue
        if a == SYMBOLS["("]:
            stack.append((i, "("))
        elif a >= FIRST_FUNCTION:
            stack.append((i, "f"))
        elif a == SYMBOLS[")"]:
            if not stack:
                unmatched += 1
                continue
            j, kind = stack.pop()
            if kind == "f":
                if c1 and stack and stack[-1] == (j - 1, "("):
                    stack.pop()          # ( name args ): this ")" closes the "(" before the name
                else:
                    call.add(j)
    return call, unmatched + sum(1 for _i, kind in stack if kind == "(")


def expression_text(stmt):
    """A Statement -> its source tokens. A function operator is written in the call form name( when a ")" closes it
    and no "(" stands right before it; otherwise bare (1.1.1.13's ( name args ) form, or an operator on its own)."""
    es = stmt["p"].get("4")
    if not es:
        raise menus.MenuError("a statement without entries has no source form")
    dec = [_entry(e) for e in es]
    # both readings of a function right after a "(" (1.1.1.13's ( name args ), or IW4's ( name( args ) ...): the
    # one that leaves fewer parentheses unmatched is written (1.1.1.13's on a tie). Either gives the same entries.
    call = min((_calls(dec, c1) for c1 in (True, False)), key=lambda r: r[1])[0]
    out = []
    for i, ((t, a, b), e) in enumerate(zip(dec, es)):
        if t == 0:
            if not 0 <= a < len(menus.OPS_1113):
                raise menus.MenuError("operator %d is not in 1.1.1.13's table" % a)
            if b != 0:
                raise menus.MenuError("operator entry %s holds %#x" % (menus.OPS_1113[a], b & 0xFFFFFFFF))
            n = menus.OPS_1113[a]
            out.append(source_name(n) + ("(" if i in call else "") if (a >= FIRST_FUNCTION or a == 0) else n)
        elif t == 1 and a == 0:
            out.append(str(b))
        elif t == 1 and a == 1:
            out.append(fmt_float_operand(struct.unpack("<f", struct.pack("<i", b))[0]))
        elif t == 1 and a == 2 and "8" in e["p"]:
            out.append(quote(e["p"]["8"]))
        elif t == 1 and a == 3 and "8" in e["p"]:
            out.append("[ " + expression_text(e["p"]["8"]) + " ]")
        else:
            raise menus.MenuError("expression entry %d / %d (or an operand without its data) has no source form"
                                  % (t, a))
    return " ".join(out)


def _ref(a, kind):
    if not isinstance(a, dict) or a.get("asset") != kind:
        raise menus.MenuError("expected a %s reference" % kind)
    n = a.get("name") or ""
    if not n.startswith(","):
        raise menus.MenuError("%s %r is an inline asset (only a reference has a source form)" % (kind, n))
    return n[1:]


class Decompiler:
    def __init__(self):
        self.out = []

    def line(self, ind, text):
        self.out.append("    " * ind + text)

    def kv(self, ind, key, value):
        self.line(ind, ("%-20s %s" % (key, value)).rstrip())

    def long(self, ind, text, width=118):
        """A line with an expression, broken at token boundaries (an expression runs on to its ";" or "{")."""
        toks = _SPLIT.findall(text)
        cur = "    " * ind + toks[0]
        for t in toks[1:]:
            if len(cur) + 1 + len(t) > width or (t in ("&&", "||") and len(cur) > width * 3 // 4):
                self.out.append(cur)
                cur = "    " * (ind + 2) + t
            else:
                cur += " " + t
        self.out.append(cur)

    def handlers(self, ind, s):
        hs = s["p"].get("4")
        if hs is None:
            raise menus.MenuError("an event handler set without its handler array has no source form")
        prev = None
        for h in hs:
            if h is None:
                raise menus.MenuError("a null event handler has no source form")
            raw = bytes.fromhex(h["raw"])
            t, v = raw[4], h["p"].get("0")
            if any(raw[5:8]) or v is None:
                raise menus.MenuError("event handler of type %d without data, or with padding" % t)
            if t == 0:
                if prev == 0:
                    raise menus.MenuError("two script handlers in a row have no source form (they would join)")
                cur = []
                for tok in script_tokens(v):
                    if tok == ";":
                        self.line(ind, " ".join(cur) + ";")
                        cur = []
                    else:
                        cur.append(script_token(tok))
                if cur:
                    self.line(ind, " ".join(cur))
            elif t == 1:
                if "4" not in v["p"] or "0" not in v["p"]:
                    raise menus.MenuError("a conditional handler without its expression or handlers")
                self.long(ind, "if " + expression_text(v["p"]["4"]))
                self.block(ind, v["p"]["0"])
            elif t == 2:
                if prev != 1:
                    raise menus.MenuError("an else handler that does not follow an if")
                self.line(ind, "else")
                self.block(ind, v)
            elif t in SET_LOCAL_NAMES:
                if "0" not in v["p"] or "4" not in v["p"]:
                    raise menus.MenuError("setLocalVar without its name or expression")
                name = v["p"]["0"]
                if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", name) or name.lower() in HANDLER_KEYWORDS:
                    name = quote(name)
                self.long(ind, "%s %s %s;" % (SET_LOCAL_NAMES[t], name, expression_text(v["p"]["4"])))
            else:
                raise menus.MenuError("event handler type %d" % t)
            prev = t

    def block(self, ind, s):
        self.line(ind, "{")
        self.handlers(ind + 1, s)
        self.line(ind, "}")

    def keys(self, ind, k):
        while k is not None:
            key = _i32(bytes.fromhex(k["raw"]), 0)
            if "4" not in k["p"]:
                raise menus.MenuError("a key handler without its handlers")
            if 33 <= key < 127 and chr(key) not in '"\\':
                self.line(ind, 'execKey "%s"' % chr(key))
            else:
                self.line(ind, "execKeyInt %d" % key)
            self.block(ind, k["p"]["4"])
            k = k["p"].get("8")

    def fields(self, ind, raw, base, table, skip=()):
        for n, o, f in table:
            size = 16 if f == "c" else 4
            if n in skip or raw[o:o + size] == base[o:o + size]:
                continue
            if f == "b":
                if _i32(raw, o) != 1:
                    raise menus.MenuError("%s is %d (the keyword stores 1)" % (n, _i32(raw, o)))
                self.line(ind, n)
                continue
            v = _fmt_field(raw, o, f)
            if n == "style" and 0 <= _i32(raw, o) < len(WINDOW_STYLES):
                v = "WINDOW_STYLE_" + WINDOW_STYLES[_i32(raw, o)]
            elif n == "textstyle" and _i32(raw, o) in TEXT_STYLES.values():
                v = "ITEM_TEXTSTYLE_" + [k for k, c in TEXT_STYLES.items() if c == _i32(raw, o)][0]
            self.kv(ind, n, v)

    def window(self, ind, raw, p, base, menu_raw=None):
        if "0" in p:
            self.kv(ind, "name", quote(p["0"]))
        if "44" in p:
            self.kv(ind, "group", quote(p["44"]))
        if menu_raw is None:                                # a menu: its rect is windowDef_t.rect
            self.kv(ind, "rect", _rect_text(raw, 4))
            if raw[0x18:0x2C] != base[0x18:0x2C]:
                self.kv(ind, "rectClient", _rect_text(raw, 0x18))
        else:                                               # an item: rect is rectClient, rect laid out from it
            self.kv(ind, "rect", _rect_text(raw, 0x18))
            if raw[4:0x18] != laid_out(raw, menu_raw):
                self.kv(ind, "layoutRect", _rect_text(raw, 4))
        static = _u32(raw, 0x44)
        for n, b in STATIC_FLAGS:
            if static & b:
                self.line(ind, n)
                static &= ~b
        if static:
            self.kv(ind, "staticFlags", "0x%X" % static)
        dyn = _u32(raw, 0x48)
        if dyn & DYN_VISIBLE:
            self.kv(ind, "visible", "1")
        fore = raw[0x50:0x60] != base[0x50:0x60] or bool(dyn & DYN_FORECOLOR)
        if fore:
            self.kv(ind, "forecolor", _fmt_field(raw, 0x50, "c"))
        if dyn != (dyn & DYN_VISIBLE) | (DYN_FORECOLOR if fore else 0):
            self.kv(ind, "dynamicFlags", "0x%X" % dyn)
        self.fields(ind, raw, base, WINDOW_FIELDS)
        if "160" in p:
            self.kv(ind, "background", quote(_ref(p["160"], "material")))

    def menu(self, ind, m):
        raw, p, base = bytes.fromhex(m["raw"]), m["p"], base_menu()
        if len(raw) != menus.MENU13:
            raise menus.MenuError("menuDef is %d bytes" % len(raw))
        if any(raw[W + 0x7C:]):
            raise menus.MenuError("menu %s: its transitions (menuDef_t +%#x..) are not 0" % (p.get("0"), W + 0x7C))
        self.line(ind, "menuDef")
        self.line(ind, "{")
        i = ind + 1
        self.window(i, raw, p, base)
        for o, n in ((0, "font"), (0x44, "allowedBinding"), (0x48, "soundLoop")):
            if str(W + o) in p:
                self.kv(i, n, quote(p[str(W + o)]))
        self.fields(i, raw, base, MENU_FIELDS)
        if str(W + 0x40) in p:
            self.long(i, "visible when " + expression_text(p[str(W + 0x40)]) + ";")
        for n, o in MENU_EXPS:
            if str(o) in p:
                self.long(i, "exp %s %s;" % (n, expression_text(p[str(o)])))
        for n, o in MENU_EVENTS:
            if str(o) in p:
                self.line(i, n)
                self.block(i, p[str(o)])
        if str(W + 0x3C) in p:
            self.keys(i, p[str(W + 0x3C)])
        for it in p.get(str(W + 0x78), []):
            self.item(i, it, raw)
        self.line(ind, "}")

    def item(self, ind, it, menu_raw):
        raw, p, base = bytes.fromhex(it["raw"]), it["p"], base_item()
        if len(raw) != menus.ITEM13:
            raise menus.MenuError("itemDef is %d bytes" % len(raw))
        self.line(ind, "itemDef")
        self.line(ind, "{")
        i = ind + 1
        itype = _i32(raw, W + 0x14)
        self.window(i, raw, p, base, menu_raw)
        if any(raw[W:W + 0x14]):
            self.kv(i, "textRect", _rect_text(raw, W))
        if itype != 0:
            self.kv(i, "type", "ITEM_TYPE_" + ITEM_TYPES[itype] if 0 <= itype < len(ITEM_TYPES) else str(itype))
        self.fields(i, raw, base, ITEM_FIELDS, skip=("type",) + (("dvarFlags",) if str(W + 0x78) in p else ()))
        for o, n in ((0x40, "text"), (0x6C, "dvar"), (0x70, "dvarTest"), (0x7C, "localvar")):
            if str(W + o) in p:
                self.kv(i, n, quote(p[str(W + o)]))
        if str(W + 0x78) in p:
            flags = _u32(raw, W + 0x80)
            names = [n for n, b in DVAR_FLAGS if b == flags]
            if not names:
                raise menus.MenuError("enableDvar with dvarFlags %#x (one of enable/disable/show/hide/focus wanted)"
                                      % flags)
            vals = " ".join(script_token(t) for t in script_tokens(p[str(W + 0x78)]))
            self.line(i, "%s { %s }" % (names[0], vals) if vals else "%s { }" % names[0])
        if str(W + 0x84) in p:
            self.kv(i, "focusSound", quote(_ref(p[str(W + 0x84)], "sound")))
        if str(W + 0x90) in p:
            self.typedata(i, itype, p[str(W + 0x90)])
        for n, o in ITEM_EVENTS:
            if str(o) in p:
                self.line(i, n)
                self.block(i, p[str(o)])
        if str(W + 0x74) in p:
            self.keys(i, p[str(W + 0x74)])
        for n, o in ITEM_STATEMENTS:
            if str(o) in p:
                self.long(i, "%s %s;" % (n, expression_text(p[str(o)])))
        if str(W + 0x9C) in p:
            fes = p[str(W + 0x9C)]
            if not fes:
                self.line(i, "floatExpressionsEmpty")
            for fe in fes:
                target = _i32(bytes.fromhex(fe["raw"]), 0)
                if target not in TARGET_NAMES or "4" not in fe["p"]:
                    raise menus.MenuError("float expression of target %d has no source form" % target)
                what, comp = TARGET_NAMES[target]
                self.long(i, "exp %s %s %s;" % (what, comp.upper(), expression_text(fe["p"]["4"])))
        self.line(ind, "}")

    def typedata(self, ind, itype, td):
        if itype == 13:
            self.kv(ind, "dvarEnumList", quote(td))
            return
        raw = bytes.fromhex(td["raw"])
        if td["kind"] == "editFieldDef":
            if not any(raw):
                self.line(ind, "editField")
            self.fields(ind, raw, bytes(0x20), EDIT_FIELDS)
        elif td["kind"] == "listBoxDef":
            if not any(raw) and not td["p"]:
                self.line(ind, "listBox")
            self.fields(ind, raw, bytes(0x144), LISTBOX_FIELDS)
            n = _i32(raw, 0x1C)
            if n or any(raw[0x20:0x120]):
                if not 0 <= n <= 16 or any(raw[0x20 + 16 * n:0x120]):
                    raise menus.MenuError("list box column data beyond its column count")
                cols = " ".join("%d %d %d %d" % struct.unpack_from("<4i", raw, 0x20 + 16 * c) for c in range(n))
                self.line(ind, ("columns %d %s" % (n, cols)).rstrip())
            if "320" in td["p"]:
                self.kv(ind, "selectIcon", quote(_ref(td["p"]["320"], "material")))
            if "288" in td["p"]:
                self.line(ind, "doubleClick")
                self.block(ind, td["p"]["288"])
        elif td["kind"] == "multiDef":
            n, strdef = _i32(raw, 0x180), _i32(raw, 0x184)
            if strdef not in (0, 1) or not 0 <= n <= 32:
                raise menus.MenuError("multiDef count %d strDef %d" % (n, strdef))
            parts = []
            for k in range(n):
                parts.append(quote(td["p"].get(str(4 * k), "")))
                parts.append(quote(td["p"].get(str(0x80 + 4 * k), "")) if strdef else
                             fmt_float(struct.unpack_from("<f", raw, 0x100 + 4 * k)[0]))
            self.line(ind, "%s { %s }" % ("dvarStrList" if strdef else "dvarFloatList", " ".join(parts)))
        else:
            raise menus.MenuError("item type %d data (%s) has no source form" % (itype, td["kind"]))


def _text(menu_nodes, header):
    d = Decompiler()
    d.out += ["// " + h if h else "//" for h in header]
    d.line(0, "{")
    for k, m in enumerate(menu_nodes):
        if k:
            d.out.append("")
        d.menu(1, m)
    d.line(0, "}")
    return "\n".join(d.out) + "\n"


def default_header(name, what):
    return [name, "%s, in the IW menu language (zonekit/menu_source.py describes it)." % what]


def decompile_menufile(ml, header=None):
    """A MenuList -> .menu text (checked: compiling the text gives back the same tree). `header`: comment lines."""
    ml = menus.normalize(ml)
    name = ml["p"].get("0")
    text = _text(ml["p"].get("8", []), header or default_header(name or "menufile", "A menufile (MenuList)"))
    check(compile_menufile(text, name or "menufile", name), ml)
    return text


def decompile_menu(m, header=None):
    """A menuDef (menu asset, type 26) -> .menu text with its one menuDef (checked like decompile_menufile)."""
    m = menus.normalize(m, menu=True)
    name = m["p"].get("0") or "menu"
    text = _text([m], header or default_header(name, "A menu asset (menuDef_t)"))
    check(compile_menu(text, name), m, menu=True)
    return text


def check(compiled, original, menu=False):
    """MenuError unless the two trees are the same source (menus.normalize: what the zone stores of them)."""
    a, b = menus.normalize(compiled, menu), menus.normalize(original, menu)
    if a != b:
        raise menus.MenuError("the .menu text does not reproduce the tree: %s" % (diff(a, b) or "?"))


def diff(a, b, path="$"):
    """The first difference of two struct trees as text, None when they are equal."""
    if type(a) is not type(b):
        return "%s: %s against %s" % (path, type(a).__name__, type(b).__name__)
    if isinstance(a, list):
        if len(a) != len(b):
            return "%s: %d entries against %d" % (path, len(a), len(b))
        for k, (x, y) in enumerate(zip(a, b)):
            d = diff(x, y, "%s[%d]" % (path, k))
            if d:
                return d
        return None
    if isinstance(a, dict) and "kind" in a and "raw" in a:
        ra, rb = bytes.fromhex(a["raw"]), bytes.fromhex(b.get("raw", ""))
        if a["kind"] != b.get("kind") or ra != rb:
            o = next((k for k in range(min(len(ra), len(rb))) if ra[k] != rb[k]), min(len(ra), len(rb)))
            return "%s (%s) +%#x: %s against %s" % (path, a["kind"], o, ra[o:o + 4].hex(), rb[o:o + 4].hex())
        pa, pb = a.get("p", {}), b.get("p", {})
        if set(pa) != set(pb):
            return "%s (%s): fields %s against %s" % (path, a["kind"], sorted(set(pa) - set(pb)),
                                                      sorted(set(pb) - set(pa)))
        for k in sorted(pa, key=int):
            d = diff(pa[k], pb[k], "%s.%s" % (path, k))
            if d:
                return d
        return None
    return None if a == b else "%s: %r against %r" % (path, a, b)


# --- command line ----------------------------------------------------------------------------------------------------
def main(argv=None):
    import argparse
    import json
    ap = argparse.ArgumentParser(description="the .menu compiler and decompiler (zonekit/menu_source.py)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("decompile", help="a struct tree (CODOL-AssetExplorer zone-menus .json, or a zone blob .bin) "
                                         "-> .menu")
    d.add_argument("source")
    d.add_argument("out")
    d.add_argument("--menu", action="store_true", help="a menu asset (menuDef_t, type 26) rather than a menufile")
    c = sub.add_parser("compile", help=".menu -> the zone blob")
    c.add_argument("source")
    c.add_argument("out")
    c.add_argument("--name", help="the menufile's asset name (ui_mp/scriptmenus/x.menu)")
    c.add_argument("--menu", action="store_true", help="a menu asset (one menuDef)")
    a = ap.parse_args(argv)
    try:
        if a.cmd == "decompile":
            if a.source.lower().endswith(".bin"):
                with open(a.source, "rb") as f:
                    data = f.read()
                tree = menus.decode_menu(data) if a.menu else menus.decode_menulist(data)
            else:
                with open(a.source, encoding="utf-8") as f:
                    tree = json.load(f)
            text = decompile_menu(tree) if a.menu else decompile_menufile(tree)
            with open(a.out, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
        else:
            tree = load(a.source, a.name, a.menu)
            with open(a.out, "wb") as f:
                f.write(menus.encode_menu(tree) if a.menu else menus.encode_menulist(tree))
    except (MenuSourceError, menus.MenuError) as e:
        raise SystemExit(str(e))


if __name__ == "__main__":
    main()
