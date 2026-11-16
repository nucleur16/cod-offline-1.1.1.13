# -*- coding: utf-8 -*-
r"""DefineFont3 (wide codes, layout) parse / byte-exact rebuild, and a glyph-level patch format.

Font3 and the bit helpers are taken from tools/english/gfx_font.py (CoD4x_Server / localizedStrings-1.1.1.13),
which builds codol_english_font.ff; its Font3.build() is byte-exact on every font of C1's fonts_ingame.gfx.
"""
from __future__ import annotations

import json
import struct

EM = 20480                       # DefineFont3 units per em
T_END, T_DEFINEFONT3 = 0, 75


# --- bit streams ------------------------------------------------------------------------------
class BitReader:
    def __init__(self, data, pos):
        self.d, self.p = data, pos * 8

    def ub(self, n):
        v = 0
        for _ in range(n):
            v = (v << 1) | ((self.d[self.p >> 3] >> (7 - (self.p & 7))) & 1)
            self.p += 1
        return v

    def sb(self, n):
        if n == 0:
            return 0
        v = self.ub(n)
        return v - (1 << n) if v & (1 << (n - 1)) else v

    def align(self):
        self.p = (self.p + 7) & ~7

    @property
    def byte(self):
        return self.p >> 3


class BitWriter:
    def __init__(self):
        self.out, self.acc, self.n = bytearray(), 0, 0

    def ub(self, n, v):
        for i in range(n - 1, -1, -1):
            self.acc = (self.acc << 1) | ((v >> i) & 1)
            self.n += 1
            if self.n == 8:
                self.out.append(self.acc)
                self.acc = self.n = 0

    def sb(self, n, v):
        self.ub(n, v & ((1 << n) - 1))

    def getvalue(self):
        if self.n:
            self.out.append(self.acc << (8 - self.n))
            self.acc = self.n = 0
        return bytes(self.out)


def sbits(*vals):
    n = 1
    for v in vals:
        while not -(1 << (n - 1)) <= v < (1 << (n - 1)):
            n += 1
    return n


def read_rect(data, pos):
    r = BitReader(data, pos)
    nb = r.ub(5)
    v = [r.sb(nb) for _ in range(4)]
    r.align()
    return v, r.byte


def write_rect(xmin, xmax, ymin, ymax):
    w = BitWriter()
    nb = sbits(xmin, xmax, ymin, ymax)
    w.ub(5, nb)
    for v in (xmin, xmax, ymin, ymax):
        w.sb(nb, v)
    return w.getvalue()



class Font3(object):
    """DefineFont3 with wide codes and layout (every font C1 ships)."""

    @classmethod
    def parse(cls, body):
        f = cls()
        d = body
        f.fid, f.flags, f.lang, nl = struct.unpack_from("<HBBB", d, 0)
        p = 5
        f.name = bytes(d[p:p + nl])
        p += nl
        ng = struct.unpack_from("<H", d, p)[0]
        p += 2
        if not f.flags & 0x04 or not f.flags & 0x80:
            raise ValueError("only DefineFont3 with wide codes and layout is supported")
        base = p
        offs = struct.unpack_from("<%d%s" % (ng + 1, "I" if f.flags & 0x08 else "H"), d, p)
        f.shapes = [bytes(d[base + offs[i]:base + offs[i + 1]]) for i in range(ng)]
        p = base + offs[ng]
        f.codes = list(struct.unpack_from("<%dH" % ng, d, p))
        p += 2 * ng
        f.ascent, f.descent, f.leading = struct.unpack_from("<hhh", d, p)
        p += 6
        f.adv = list(struct.unpack_from("<%dh" % ng, d, p))
        p += 2 * ng
        f.bounds = []
        for _ in range(ng):
            r, p = read_rect(d, p)
            f.bounds.append(r)
        nk = struct.unpack_from("<H", d, p)[0]
        p += 2
        f.kern = [struct.unpack_from("<HHh", d, p + 6 * i) for i in range(nk)]
        p += 6 * nk
        f.tail = bytes(d[p:])
        return f

    @property
    def display_name(self):
        return self.name.rstrip(b"\0").decode("latin-1")

    def index(self, code):
        try:
            return self.codes.index(code)
        except ValueError:
            return None


    def build(self):
        order = sorted(range(len(self.codes)), key=lambda i: self.codes[i])   # CodeTable ascending
        ng = len(order)
        shapes = [self.shapes[i] for i in order]
        size = sum(len(s) for s in shapes)
        wide = 2 * (ng + 1) + size > 0xFFFF
        flags = (self.flags & ~0x08) | (0x08 if wide else 0) | 0x84
        out = bytearray(struct.pack("<HBBB", self.fid, flags, self.lang, len(self.name)))
        out += self.name + struct.pack("<H", ng)
        o = (4 if wide else 2) * (ng + 1)
        offs = []
        for s in shapes:
            offs.append(o)
            o += len(s)
        offs.append(o)
        out += struct.pack("<%d%s" % (ng + 1, "I" if wide else "H"), *offs)
        for s in shapes:
            out += s
        out += struct.pack("<%dH" % ng, *[self.codes[i] for i in order])
        out += struct.pack("<hhh", self.ascent, self.descent, self.leading)
        out += struct.pack("<%dh" % ng, *[self.adv[i] for i in order])
        for i in order:
            out += write_rect(*self.bounds[i])
        out += struct.pack("<H", len(self.kern))
        for k in self.kern:
            out += struct.pack("<HHh", *k)
        return bytes(out + self.tail)


# --- glyph patch ---------------------------------------------------------------------------------------------------
TAG_HEADER_LONG = 0x3F


def tag_body(tag: bytes):
    cl = struct.unpack_from("<H", tag, 0)[0]
    code, ln = cl >> 6, cl & 0x3F
    if ln == TAG_HEADER_LONG:
        return code, tag[6:], True
    return code, tag[2:], False


def make_tag(code, body, long_form=True):
    if long_form or len(body) >= 0x3F:
        return struct.pack("<HI", (code << 6) | 0x3F, len(body)) + body
    return struct.pack("<H", (code << 6) | len(body)) + body


def diff_font(stock_tag: bytes, new_tag: bytes):
    """A JSON-able dict that turns stock_tag into new_tag, or None when new_tag is not a rebuild of a parsed font."""
    sc, sb, _ = tag_body(stock_tag)
    nc, nb, nlong = tag_body(new_tag)
    if sc != 75 or nc != 75:
        return None
    a, b = Font3.parse(sb), Font3.parse(nb)
    if b.build() != nb:
        return None
    ga = {c: (a.shapes[i], a.adv[i], list(a.bounds[i])) for i, c in enumerate(a.codes)}
    glyphs = {}
    for i, c in enumerate(b.codes):
        g = (b.shapes[i], b.adv[i], list(b.bounds[i]))
        if ga.get(c) != g:
            glyphs["%04X" % c] = {"shape": g[0].hex(), "adv": g[1], "bounds": g[2]}
    removed = sorted("%04X" % c for c in set(ga) - set(b.codes))
    patch = {"font": b.display_name, "fid": b.fid, "flags": b.flags, "lang": b.lang, "name": b.name.hex(),
             "ascent": b.ascent, "descent": b.descent, "leading": b.leading, "long_form": nlong,
             "kern": None if b.kern == a.kern else [list(k) for k in b.kern],
             "tail": None if b.tail == a.tail else b.tail.hex(),
             "removed": removed, "glyphs": glyphs}
    if apply_font(stock_tag, patch) != new_tag:
        return None
    return patch


def apply_font(stock_tag: bytes, patch) -> bytes:
    _c, sb, _ = tag_body(stock_tag)
    f = Font3.parse(sb)
    by = {c: i for i, c in enumerate(f.codes)}
    for k in patch["removed"]:
        i = by.pop(int(k, 16))
        f.shapes[i] = None
    for k, g in patch["glyphs"].items():
        c = int(k, 16)
        val = (bytes.fromhex(g["shape"]), g["adv"], list(g["bounds"]))
        if c in by:
            i = by[c]
            f.shapes[i], f.adv[i], f.bounds[i] = val
        else:
            f.codes.append(c)
            f.shapes.append(val[0])
            f.adv.append(val[1])
            f.bounds.append(val[2])
            by[c] = len(f.codes) - 1
    keep = [i for i in range(len(f.codes)) if f.shapes[i] is not None]
    f.codes = [f.codes[i] for i in keep]
    f.shapes = [f.shapes[i] for i in keep]
    f.adv = [f.adv[i] for i in keep]
    f.bounds = [f.bounds[i] for i in keep]
    f.fid, f.flags, f.lang = patch["fid"], patch["flags"], patch["lang"]
    f.name = bytes.fromhex(patch["name"])
    f.ascent, f.descent, f.leading = patch["ascent"], patch["descent"], patch["leading"]
    if patch["kern"] is not None:
        f.kern = [tuple(k) for k in patch["kern"]]
    if patch["tail"] is not None:
        f.tail = bytes.fromhex(patch["tail"])
    return make_tag(75, f.build(), patch["long_form"])


def dumps(patch) -> str:
    return json.dumps(patch, indent=1, sort_keys=True) + "\n"
