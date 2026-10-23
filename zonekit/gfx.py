# -*- coding: utf-8 -*-
r"""Scaleform GFx movies (SWF with a "GFX" / "CFX" magic) as a patch over a stock movie.

A patched movie is stored as the stock movie it was made from (named by zone and rawfile, checked by sha256) plus the
tags that differ: zones/<zone>/rawfile/<path>.gfxpatch/
    patch.txt        header bytes, tag list: each line "keep <stock index>" or "new <file>"
    tags/NNNN_<code>[_<abc name>].bin   the new tags, complete (header included), byte for byte
    as/<package path>/<Class>.as        the ActionScript source of each new DoABC class: FFDec's decompile, or the
                                        hand-written source the class was compiled from (voice_ui_mp, controller_ingame)
    "as <class> <as/ path> <sha256>"    one patch.txt line per as/ file: the text the stored tag corresponds to
Rebuilding joins the stock tags and the new tag files, so it is byte-identical. An as/ file whose text no longer matches
its recorded sha256 (an edit) is compiled by FFDec (-replace) over the stock class, and only its DoABC tag is replaced;
every other tag is checked unchanged. --compile-as also compiles every as/ file whose text differs from FFDec's
decompile of the stored tag (the hand-written ones), which reproduces their stored tags byte for byte.
A "// @K33DIAGRAMS@" line is replaced by k33_pad_diagrams.as.inc from the same folder before compiling; the include
is part of the recorded sha256.
"""
from __future__ import annotations

import hashlib
import os
import re
import struct
import subprocess
import tempfile
import zlib

T_DOABC = 82
DIAGRAMS = "// @K33DIAGRAMS@"


def header_len(data: bytes) -> int:
    nbits = data[8] >> 3
    return 8 + (5 + 4 * nbits + 7) // 8 + 4


def tags(data: bytes):
    """[(code, start, end, abc name)] of an uncompressed movie, the end tag included."""
    if data[:3] not in (b"GFX", b"FWS"):
        raise ValueError("not an uncompressed GFx/SWF movie (%r)" % data[:3])
    pos = header_len(data)
    out = []
    while pos + 2 <= len(data):
        start = pos
        cl = struct.unpack_from("<H", data, pos)[0]
        code, ln = cl >> 6, cl & 0x3F
        pos += 2
        if ln == 0x3F:
            ln = struct.unpack_from("<I", data, pos)[0]
            pos += 4
        name = ""
        if code == T_DOABC:
            e = data.index(b"\0", pos + 4)
            name = data[pos + 4:e].decode("latin-1")
        out.append((code, start, pos + ln, name))
        pos += ln
        if code == 0:
            break
    if pos != len(data):
        raise ValueError("%d bytes after the end tag" % (len(data) - pos))
    return out


def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def diff(stock: bytes, movie: bytes):
    """-> (header bytes, [("keep", i) | ("new", tag bytes, code, abc name)]). Tags equal to a stock tag at any index are
    kept by index (movies keep their tag order; a moved tag is still found)."""
    st = tags(stock)
    index = {}
    for i, (_c, s, e, _n) in enumerate(st):
        index.setdefault(stock[s:e], i)
    ops = []
    for code, s, e, name in tags(movie):
        body = movie[s:e]
        i = index.get(body)
        ops.append(("keep", i) if i is not None else ("new", body, code, name))
    return movie[:header_len(movie)], ops


def apply(stock: bytes, header: bytes, ops) -> bytes:
    from . import font3
    st = tags(stock)
    out = bytearray(header)
    for op in ops:
        if op[0] == "keep":
            _c, s, e, _n = st[op[1]]
            out += stock[s:e]
        elif op[0] == "font":
            _c, s, e, _n = st[op[1]]
            out += font3.apply_font(stock[s:e], op[2])
        else:
            out += op[1]
    struct.pack_into("<I", out, 4, len(out))
    return bytes(out)


def safe(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", name)[:80]


def source_sha(path: str) -> str:
    """sha256 of an as/ file as compiled: its text plus the diagrams include when it uses it."""
    with open(path, "rb") as f:
        data = f.read()
    if DIAGRAMS.encode() in data:
        with open(os.path.join(os.path.dirname(path), "k33_pad_diagrams.as.inc"), "rb") as f:
            data += b"\0" + f.read()
    return sha(data)


def as_files(dirpath: str):
    """[(class, as/ relative path)] of every .as under <dirpath>/as."""
    out = []
    for root, _d, files in os.walk(os.path.join(dirpath, "as")):
        for fn in files:
            if fn.endswith(".as"):
                rel = os.path.relpath(os.path.join(root, fn), dirpath).replace(os.sep, "/")
                out.append((rel[3:-3].replace("/", "."), rel))
    return sorted(out)


def write_patch(dirpath: str, stock_ref: str, stock: bytes, movie: bytes, ffdec: str | None = None,
                handwritten=()):
    """Write <dirpath>/patch.txt + tags/ (+ as/ decompiled by FFDec when available, except the `handwritten` classes,
    whose as/ files are kept as they are). Returns the number of new tags."""
    from . import font3
    header, ops = diff(stock, movie)
    st = tags(stock)
    fonts = {stock[s + (6 if stock[s] & 0x3F == 0x3F else 2):][:2]: i for i, (c, s, e, _n) in enumerate(st) if c == 75}
    os.makedirs(os.path.join(dirpath, "tags"), exist_ok=True)
    lines = ["# GFx movie as a patch over a stock movie (zonekit/gfx.py)",
             "stock %s" % stock_ref,
             "stock-sha256 %s" % sha(stock),
             "sha256 %s" % sha(movie),
             "header %s" % header.hex()]
    abc = []
    n = 0
    for k, op in enumerate(ops):
        if op[0] == "keep":
            lines.append("keep %d" % op[1])
            continue
        _, body, code, name = op
        if code == 75:
            _c, fb, _l = font3.tag_body(body)
            i = fonts.get(fb[:2])
            if i is not None:
                _c, s0, e0, _n = st[i]
                fp = font3.diff_font(stock[s0:e0], body)
                if fp is not None:
                    fn = "tags/%04d_75_%s.json" % (k, safe(fp["font"]))
                    with open(os.path.join(dirpath, fn), "w", newline="\n") as f:
                        f.write(font3.dumps(fp))
                    lines.append("font %d %s" % (i, fn))
                    n += 1
                    continue
        fn = "tags/%04d_%d%s.bin" % (k, code, ("_" + safe(name)) if name else "")
        with open(os.path.join(dirpath, fn), "wb") as f:
            f.write(body)
        lines.append("new %s" % fn)
        n += 1
        if code == T_DOABC and name and name.replace("/", ".") not in handwritten:
            abc.append(name)
    if ffdec and abc:
        export_as(movie, abc, os.path.join(dirpath, "as"), ffdec)
    for cls, rel in as_files(dirpath):
        lines.append("as %s %s %s" % (cls, rel, source_sha(os.path.join(dirpath, rel))))
    with open(os.path.join(dirpath, "patch.txt"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return n


def read_patch(dirpath: str):
    meta, ops = {"as": []}, []
    with open(os.path.join(dirpath, "patch.txt")) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            k, _, v = line.partition(" ")
            if k == "keep":
                ops.append(("keep", int(v)))
            elif k == "font":
                import json
                i, _, fn = v.partition(" ")
                with open(os.path.join(dirpath, fn)) as g:
                    ops.append(("font", int(i), json.load(g)))
            elif k == "as":
                meta["as"].append(tuple(v.split(" ")))
            elif k == "new":
                with open(os.path.join(dirpath, v), "rb") as g:
                    ops.append(("new", g.read(), v))
            else:
                meta[k] = v
    return meta, ops


def build_patch(dirpath: str, stock: bytes) -> bytes:
    meta, ops = read_patch(dirpath)
    if sha(stock) != meta["stock-sha256"]:
        raise SystemExit("%s: the stock movie (%s) is not the one this patch was made against" % (dirpath, meta["stock"]))
    return apply(stock, bytes.fromhex(meta["header"]), ops)


def export_as(movie: bytes, classes, outdir: str, ffdec: str):
    """FFDec's ActionScript for the DoABC tags `classes` (their tag names: one class each in these movies)."""
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "m.swf")
        swf = b"FWS" + movie[3:]
        with open(src, "wb") as f:
            f.write(swf)
        sel = ",".join(c.replace("/", ".") for c in classes)
        r = subprocess.run([ffdec, "-selectclass", sel, "-export", "script", os.path.join(tmp, "out"), src],
                           capture_output=True, text=True)
        if r.returncode:
            raise SystemExit("FFDec export failed: %s%s" % (r.stdout, r.stderr))
        base = os.path.join(tmp, "out", "scripts")
        for root, _d, files in os.walk(base):
            for fn in files:
                if fn.endswith(".as"):
                    rel = os.path.relpath(os.path.join(root, fn), base)
                    dst = os.path.join(outdir, rel)
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    with open(os.path.join(root, fn), "rb") as a, open(dst, "wb") as b:
                        b.write(a.read().replace(b"\r\n", b"\n"))


def with_stock_class(movie: bytes, stock: bytes, cls: str) -> bytes:
    """`movie` with the DoABC tag of `cls` put back to the stock movie's. FFDec compiles a class into the stock tag
    reproducibly (the shipped tags were made that way); compiling over its own earlier output is not byte-stable."""
    want = cls.replace(".", "/")
    st = [stock[s:e] for c, s, e, n in tags(stock) if c == T_DOABC and n == want]
    hit = [(s, e) for c, s, e, n in tags(movie) if c == T_DOABC and n == want]
    if len(st) != 1 or len(hit) != 1:
        return movie
    out = bytearray(movie[:hit[0][0]] + st[0] + movie[hit[0][1]:])
    struct.pack_into("<I", out, 4, len(out))
    return bytes(out)


def compile_as(movie: bytes, cls: str, as_path: str, ffdec: str) -> bytes:
    """`movie` with class `cls` compiled from `as_path` by FFDec; only that class's DoABC tag is taken from FFDec's
    output, every other tag is checked to be unchanged."""
    want = cls.replace(".", "/")
    with tempfile.TemporaryDirectory() as tmp:
        src, dst = os.path.join(tmp, "in.swf"), os.path.join(tmp, "out.swf")
        with open(src, "wb") as f:
            f.write(b"FWS" + movie[3:])
        with open(as_path, encoding="utf-8") as f:
            text = f.read()
        if DIAGRAMS in text:
            # the pad diagrams (k33_pad_diagrams.as.inc next to the class, pre-generated line art) go in at the marker
            with open(os.path.join(os.path.dirname(as_path), "k33_pad_diagrams.as.inc"), encoding="utf-8") as f:
                text = text.replace(DIAGRAMS, f.read())
            as_path = os.path.join(tmp, os.path.basename(as_path))
            with open(as_path, "w", encoding="utf-8", newline="") as f:
                f.write(text)
        r = subprocess.run([ffdec, "-replace", src, dst, cls, as_path], capture_output=True, text=True)
        if r.returncode or not os.path.isfile(dst):
            raise SystemExit("FFDec could not compile %s:\n%s%s" % (as_path, r.stdout, r.stderr))
        with open(dst, "rb") as f:
            compiled = f.read()
    if compiled[:3] == b"CWS":
        compiled = b"FWS" + compiled[3:8] + zlib.decompress(compiled[8:])
    old, new = tags(movie), tags(compiled)
    if len(old) != len(new):
        raise SystemExit("FFDec changed the tag count (%d -> %d)" % (len(old), len(new)))
    hit = None
    for i, (a, b) in enumerate(zip(old, new)):
        if a[0] == T_DOABC and a[3] == want:
            hit = i
        elif a[0] != b[0] or movie[a[1]:a[2]] != compiled[b[1]:b[2]]:
            raise SystemExit("FFDec changed tag %d (code %d %s), not only %s" % (i, a[0], a[3], want))
    if hit is None:
        raise SystemExit("no DoABC tag %s" % want)
    out = bytearray(movie[:old[hit][1]] + compiled[new[hit][1]:new[hit][2]] + movie[old[hit][2]:])
    struct.pack_into("<I", out, 4, len(out))
    return bytes(out)
