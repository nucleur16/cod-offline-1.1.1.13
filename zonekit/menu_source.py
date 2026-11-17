# -*- coding: utf-8 -*-
r"""A small .menu compiler: IW-style menu source text -> the 1.1.1.13 struct tree of zonekit/menus.py.

It reads the subset of the IW4 menu language a HUD needs (no event handlers, no list boxes, no key handlers):

    #define NAME tokens...          (one line; a trailing backslash continues it). Expanded wherever NAME appears as
                                    a whole word, recursively. #include is not supported.
    menuDef {
        name "weaponbar_hd"        the 1.1.1.13 menu these items join (a host)
        visible when ( expr )      optional: the menu's visibility while this style is on (default: the host's own)
        stockItems 2 16 17 | all   optional: items of the host's own (1.1.1.13) menu, by index, that this style
                                   shows too, unchanged and with their own visibility
        itemDef {
            name "sm2_clip"
            rect x y w h [horz vert]                 rect and rectClient (the host menu's coordinates)
            type 0|8 (or ITEM_TYPE_TEXT / ITEM_TYPE_OWNERDRAW)
            style 0|1|3 (or WINDOW_STYLE_EMPTY / _FILLED / _SHADER)
            ownerdraw N
            background "material"                   a reference: the material must be loaded already
            forecolor r g b a   backcolor r g b a
            textfont N  textscale f  textstyle N  textalign N  textalignx f  textaligny f
            text "string"
            visible when ( expr )   |   visible 1
            exp text ( expr )   exp material ( expr )
            exp rect X|Y|W|H ( expr )   exp forecolor R|G|B|A ( expr )   exp backcolor R|G|B|A ( expr )
        a menuDef's  itemOffset ( exprX ) ( exprY )  adds the two to the x and y of every itemDef after it in that
        menuDef: "exp rect X ( x + ( exprX ) )", or "( its own exp rect X ) + ( exprX )" when it has one
        }
    }

Expressions are written as in IW4 source and stored as the infix entry stream C1's evaluator reads (Statement_s,
zonekit/menus.py): a function `name( args )` is the function's operator, its arguments and the ")" that ends it;
`( .. )` is the "(" and ")" operators; numbers are int or float operands, "..." string operands. Function names are
the operators of 1.1.1.13's table (zonekit/menus.py OPS_1113, case-insensitive). Unknown names are an error, so a
.menu that compiles uses only what 1.1.1.13 evaluates.
"""
from __future__ import annotations

import re
import struct

from zonekit import menus

W = menus.W13
OPS = {n.lower(): n for n in menus.OPS_1113}
FIRST_FUNCTION = menus.OP_1113["dvarint(static)"]
BINARY = ["==", "!=", "<=", ">=", "&&", "||", "<<", ">>", "<", ">", "+", "-", "*", "/", "%", "&", "|", "~", "!", ","]
CONSTANTS = {"ITEM_TYPE_TEXT": 0, "ITEM_TYPE_OWNERDRAW": 8, "WINDOW_STYLE_EMPTY": 0, "WINDOW_STYLE_FILLED": 1,
             "WINDOW_STYLE_SHADER": 3, "ITEM_TEXTSTYLE_NORMAL": 0, "ITEM_TEXTSTYLE_SHADOWED": 3,
             "ITEM_TEXTSTYLE_SHADOWEDMORE": 6, "HORIZONTAL_ALIGN_LEFT": 8, "HORIZONTAL_ALIGN_CENTER": 9,
             "HORIZONTAL_ALIGN_RIGHT": 10, "VERTICAL_ALIGN_TOP": 8, "VERTICAL_ALIGN_CENTER": 9,
             "VERTICAL_ALIGN_BOTTOM": 10}
# ItemFloatExpression targets (IW4 ITEM_FLOATEXP_TGT_*; 1.1.1.13's stock HUD uses 0, 1, 2, 8)
FLOAT_TARGETS = {("rect", "x"): 0, ("rect", "y"): 1, ("rect", "w"): 2, ("rect", "h"): 3,
                 ("forecolor", "r"): 4, ("forecolor", "g"): 5, ("forecolor", "b"): 6, ("forecolor", "rgb"): 7,
                 ("forecolor", "a"): 8, ("backcolor", "r"): 14, ("backcolor", "g"): 15, ("backcolor", "b"): 16,
                 ("backcolor", "rgb"): 17, ("backcolor", "a"): 18}
# fields of an itemDef_s (zonekit/menus.py layouts)
O_TYPE, O_FONT, O_TEXTALIGN, O_ALIGNX, O_ALIGNY, O_SCALE, O_TEXTSTYLE = (W + 0x14, W + 0x20, W + 0x24, W + 0x28,
                                                                        W + 0x2C, W + 0x30, W + 0x34)
P_TEXT, P_FLOATS, P_VISIBLE, P_TEXTEXP, P_MATERIALEXP = (str(W + 0x40), str(W + 0x9C), str(W + 0xA0),
                                                         str(W + 0xA8), str(W + 0xAC))


class MenuSourceError(Exception):
    pass


TOKEN = re.compile(r'''\s*(?:(?P<str>"(?:[^"\\]|\\.)*")|(?P<num>\d+\.\d*|\.\d+|\d+)|(?P<id>[A-Za-z_][A-Za-z0-9_]*)|
                       (?P<op>==|!=|<=|>=|&&|\|\||<<|>>|[-+*/%<>!&|~(){},;]))''', re.X)


def _strip_comments(text):
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return _line_comments(text)


def _line_comments(text):
    out = []
    for line in text.split("\n"):
        q = False
        for i, ch in enumerate(line):
            if ch == '"':
                q = not q
            elif not q and line.startswith("//", i):
                line = line[:i]
                break
        out.append(line)
    return "\n".join(out)


def tokenize(text, where="menu"):
    out = []
    for lineno, line in enumerate(text.split("\n"), 1):
        pos = 0
        while pos < len(line):
            if not line[pos:].strip():
                break
            m = TOKEN.match(line, pos)
            if not m:
                raise MenuSourceError("%s:%d: cannot read %r" % (where, lineno, line[pos:pos + 20]))
            kind = m.lastgroup
            value = m.group(kind)
            if kind == "str":
                value = bytes(value[1:-1], "latin-1").decode("unicode_escape")
            out.append((kind, value, lineno))
            pos = m.end()
    return out


def preprocess(text, where="menu"):
    """-> tokens with #define macros expanded."""
    text = _strip_comments(text)
    lines, defines, body = text.split("\n"), {}, []
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.lstrip().startswith("#"):
            full = line
            while full.endswith("\\") and i + 1 < len(lines):
                i += 1
                full = full[:-1] + " " + lines[i]
            m = re.match(r"\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)\s*(.*)$", full)
            if not m:
                raise MenuSourceError("%s:%d: only #define is supported" % (where, i + 1))
            defines[m.group(1)] = tokenize(m.group(2), where)
            body.append("")
        else:
            body.append(line)
        i += 1

    def expand(tokens, seen):
        out = []
        for t in tokens:
            if t[0] == "id" and t[1] in defines and t[1] not in seen:
                # a macro's tokens take the line of its use (a rect's numbers are read up to the end of the line)
                out += [(k, v, t[2]) for k, v, _l in expand(defines[t[1]], seen | {t[1]})]
            else:
                out.append(t)
        return out
    return expand(tokenize("\n".join(body), where), frozenset())


# --- expressions -----------------------------------------------------------------------------------------------------
def _op(name):
    return {"kind": "expressionEntry", "raw": struct.pack("<III", 0, menus.OP_1113[name], 0).hex().upper(), "p": {}}


def _string(v):
    return {"kind": "expressionEntry", "raw": struct.pack("<III", 1, 2, 0).hex().upper(), "p": {"8": v}}


def _int(v):
    return {"kind": "expressionEntry", "raw": struct.pack("<IIi", 1, 0, v).hex().upper(), "p": {}}


def _float(v):
    return {"kind": "expressionEntry", "raw": struct.pack("<IIf", 1, 1, v).hex().upper(), "p": {}}


def statement(entries):
    return {"kind": "Statement", "raw": struct.pack("<IIIII", len(entries), 0, 0, 0, 0).hex().upper(),
            "p": {"4": entries}}


def compile_expression(tokens, where="expression"):
    """Infix tokens (a balanced ( .. ) group, outer parentheses included) -> a Statement."""
    es, depth, k = [], 0, 0
    stack = []
    while k < len(tokens):
        kind, v, line = tokens[k]
        nxt = tokens[k + 1] if k + 1 < len(tokens) else None
        k += 1
        if kind == "id":
            name = OPS.get(v.lower())
            if v.lower() in ("dvarint", "dvarbool", "dvarfloat", "dvarstring"):
                name = v.lower()
            if name is None or menus.OP_1113[name] < FIRST_FUNCTION:
                raise MenuSourceError("%s:%d: %s is not a function of 1.1.1.13's menu expressions" % (where, line, v))
            if not nxt or nxt[1] != "(" or nxt[0] != "op":
                raise MenuSourceError("%s:%d: %s must be called: %s( .. )" % (where, line, v, v))
            k += 1
            es.append(_op(name))
            stack.append("f")
        elif kind == "str":
            es.append(_string(v))
        elif kind == "num":
            es.append(_float(float(v)) if "." in v else _int(int(v)))
        elif v == "(":
            es.append(_op("("))
            stack.append("(")
        elif v == ")":
            if not stack:
                raise MenuSourceError("%s:%d: unbalanced )" % (where, line))
            stack.pop()
            es.append(_op(")"))
        elif v == "-" and nxt and nxt[0] == "num" and (not es or _is_operator_not_close(es[-1])):
            kind2, v2, _l = tokens[k]
            k += 1
            es.append(_float(-float(v2)) if "." in v2 else _int(-int(v2)))
        elif v in BINARY:
            es.append(_op(v))
        else:
            raise MenuSourceError("%s:%d: unexpected %r in an expression" % (where, line, v))
    if stack:
        raise MenuSourceError("%s: %d unclosed parentheses" % (where, len(stack)))
    return statement(es)


def _is_operator_not_close(e):
    t, a = struct.unpack_from("<II", bytes.fromhex(e["raw"]))
    return t == 0 and a != menus.OP_1113[")"]


# --- items -----------------------------------------------------------------------------------------------------------
class _Reader:
    def __init__(self, tokens, where):
        self.t, self.k, self.where = tokens, 0, where

    def peek(self):
        return self.t[self.k] if self.k < len(self.t) else (None, None, 0)

    def take(self):
        if self.k >= len(self.t):
            raise MenuSourceError("%s: unexpected end" % self.where)
        self.k += 1
        return self.t[self.k - 1]

    def expect(self, value):
        kind, v, line = self.take()
        if v != value:
            raise MenuSourceError("%s:%d: expected %r, got %r" % (self.where, line, value, v))

    def number(self):
        kind, v, line = self.take()
        sign = 1
        if v == "-":
            sign = -1
            kind, v, line = self.take()
        if kind == "id" and v in CONSTANTS:
            return CONSTANTS[v] * sign
        if kind != "num":
            raise MenuSourceError("%s:%d: expected a number, got %r" % (self.where, line, v))
        return sign * (float(v) if "." in v else int(v))

    def string(self):
        kind, v, line = self.take()
        if kind != "str":
            raise MenuSourceError("%s:%d: expected a string, got %r" % (self.where, line, v))
        return v

    def group(self):
        """A balanced ( .. ) group, the parentheses included."""
        start = self.k
        kind, v, line = self.take()
        if v != "(":
            raise MenuSourceError("%s:%d: expected ( after an expression keyword, got %r" % (self.where, line, v))
        depth = 1
        while depth:
            kind, v, _l = self.take()
            if kind == "op" and v == "(":
                depth += 1
            elif kind == "op" and v == ")":
                depth -= 1
        return self.t[start:self.k]

    def numbers_on_line(self, n_min, n_max):
        line = self.peek()[2]
        out = []
        while len(out) < n_max and self.peek()[2] == line and (self.peek()[0] == "num" or self.peek()[1] == "-" or
                                                               self.peek()[1] in CONSTANTS):
            out.append(self.number())
        if len(out) < n_min:
            raise MenuSourceError("%s:%d: expected %d numbers" % (self.where, line, n_min))
        return out


def _base_item():
    """A blank itemDef_s with the flags every 1.1.1.13 HUD item has (static 0x100000, dynamic 0x10004: visible),
    colours white / clear, text scale 0.55, rect aligned to the host's corner 0 0."""
    raw = bytearray(menus.ITEM13)
    struct.pack_into("<I", raw, 0x44, 0x100000)
    struct.pack_into("<I", raw, 0x48, 0x10004)
    struct.pack_into("<4f", raw, 0x50, 1, 1, 1, 1)
    struct.pack_into("<f", raw, O_SCALE, 0.55)
    return raw


def _offset_group(base, extra):
    """( ( base ) + ( extra ) ) as a token group; base is a group or a number."""
    line = extra[0][2]
    if isinstance(base, float):
        b = ([("op", "-", line)] if base < 0 else []) + [("num", repr(abs(base)), line)]
    else:
        b = list(base)
    return ([("op", "(", line), ("op", "(", line)] + b + [("op", ")", line), ("op", "+", line)] + list(extra) +
            [("op", ")", line)])


def parse_item(r, offset=None):
    raw = _base_item()
    p = {}
    floats = []
    rect_set = False
    rect_exp = {}                       # "x" / "y": the source group of its exp rect X / Y (for itemOffset)
    r.expect("{")
    while True:
        kind, v, line = r.take()
        if v == "}":
            break
        key = v.lower() if kind == "id" else v
        if key == "name":
            p["0"] = r.string()
        elif key == "rect":
            vals = r.numbers_on_line(4, 6)
            x, y, w, h = (float(a) for a in vals[:4])
            rect_xy = (x, y)
            horz, vert = (int(vals[4]), int(vals[5])) if len(vals) == 6 else (0, 0)
            for o in (4, 0x18):
                struct.pack_into("<4f", raw, o, x, y, w, h)
                raw[o + 0x10], raw[o + 0x11] = horz, vert
            rect_set = True
        elif key in ("type", "style", "ownerdraw", "textfont", "textstyle", "textalign"):
            off = {"type": O_TYPE, "style": 0x30, "ownerdraw": 0x38, "textfont": O_FONT, "textstyle": O_TEXTSTYLE,
                   "textalign": O_TEXTALIGN}[key]
            struct.pack_into("<i", raw, off, int(r.number()))
        elif key in ("textscale", "textalignx", "textaligny"):
            off = {"textscale": O_SCALE, "textalignx": O_ALIGNX, "textaligny": O_ALIGNY}[key]
            struct.pack_into("<f", raw, off, float(r.number()))
        elif key in ("forecolor", "backcolor"):
            struct.pack_into("<4f", raw, 0x50 if key == "forecolor" else 0x60, *(float(r.number()) for _ in range(4)))
        elif key == "background":
            p["160"] = {"asset": "material", "name": "," + r.string().lstrip(",")}
        elif key == "text":
            p[P_TEXT] = r.string()
        elif key == "decoration":
            pass
        elif key == "visible":
            if r.peek()[1] == "when":
                r.take()
                p[P_VISIBLE] = compile_expression(r.group(), "%s:%d" % (r.where, line))
            else:
                if r.number() != 1:
                    raise MenuSourceError("%s:%d: visible 0 is not supported (leave the item out)" % (r.where, line))
        elif key == "exp":
            _k, what, _l = r.take()
            what = what.lower()
            if what in ("text", "material"):
                p[P_TEXTEXP if what == "text" else P_MATERIALEXP] = compile_expression(
                    r.group(), "%s:%d" % (r.where, line))
            elif what in ("rect", "forecolor", "backcolor"):
                _k, comp, _l = r.take()
                target = FLOAT_TARGETS.get((what, comp.lower()))
                if target is None:
                    raise MenuSourceError("%s:%d: exp %s %s is not supported" % (r.where, line, what, comp))
                group = r.group()
                if what == "rect" and comp.lower() in ("x", "y") and offset:
                    rect_exp[comp.lower()] = group
                else:
                    floats.append({"kind": "ItemFloatExpression", "raw": struct.pack("<II", target, 0).hex().upper(),
                                   "p": {"4": compile_expression(group, "%s:%d" % (r.where, line))}})
            else:
                raise MenuSourceError("%s:%d: exp %s is not supported" % (r.where, line, what))
        else:
            raise MenuSourceError("%s:%d: unknown item keyword %r" % (r.where, line, v))
        if r.peek()[1] == ";":
            r.take()
    if not rect_set:
        raise MenuSourceError("%s: an itemDef without a rect" % r.where)
    if offset:
        for k, axis in enumerate(("x", "y")):
            group = _offset_group(rect_exp.get(axis, rect_xy[k]), offset[k])
            floats.insert(k, {"kind": "ItemFloatExpression",
                              "raw": struct.pack("<II", FLOAT_TARGETS[("rect", axis)], 0).hex().upper(),
                              "p": {"4": compile_expression(group, "%s: itemOffset" % r.where)}})
    p[P_FLOATS] = floats
    struct.pack_into("<i", raw, W + 0x98, len(floats))
    return {"kind": "itemDef", "raw": bytes(raw).hex().upper(), "p": p}


def parse(text, where="menu"):
    """-> [{"name": host menu, "visible": Statement or None, "items": [itemDef node, ..]}] in source order."""
    r = _Reader(preprocess(text, where), where)
    out = []
    while r.peek()[0] is not None:
        kind, v, line = r.take()
        if v == "{" or v == "}":        # an optional outer { } around the menuDefs, as in IW4 .menu files
            continue
        if v != "menuDef":
            raise MenuSourceError("%s:%d: expected menuDef, got %r" % (where, line, v))
        r.expect("{")
        menu = {"name": None, "visible": None, "items": [], "stock": []}
        offset = None
        while True:
            kind, v, line = r.take()
            if v == "}":
                break
            if v == "name":
                menu["name"] = r.string()
            elif v == "stockItems":
                # the host's own items (by index in 1.1.1.13's menu) that this style shows too, unchanged
                if r.peek()[1] == "all":
                    r.take()
                    menu["stock"] = "all"
                else:
                    menu["stock"] += [int(n) for n in r.numbers_on_line(1, 999)]
            elif v == "visible":
                r.expect("when")
                menu["visible"] = compile_expression(r.group(), "%s:%d" % (where, line))
            elif v == "itemOffset":
                offset = (r.group(), r.group())
                for g in offset:
                    compile_expression(g, "%s:%d" % (where, line))      # reported here, not at each item
            elif v == "itemDef":
                menu["items"].append(parse_item(r, offset))
            else:
                raise MenuSourceError("%s:%d: unknown menuDef keyword %r" % (where, line, v))
            if r.peek()[1] == ";":
                r.take()
        if not menu["name"]:
            raise MenuSourceError("%s: a menuDef without a name" % where)
        out.append(menu)
    return out
