# -*- coding: utf-8 -*-
"""zones/<zone>/ + zone_source/<zone>.csv -> output/<install path>/<zone>.ff"""
from __future__ import annotations

import collections
import json
import os

from . import codecs, container, gfx, manifest, zoneimage
from .decompile import MARGIN


class BuildError(Exception):
    pass


def load(repo, zone_name):
    with open(os.path.join(repo, "zone_source", zone_name + ".csv"), encoding="utf-8") as f:
        return manifest.read(f.read())


def assemble(repo, zone_name, m, stocks, ffdec=None, compile_as=False, log=print):
    """-> zone image bytes."""
    zdir = os.path.join(repo, "zones", zone_name)
    for s in m.settings.get("stock", []):
        stocks.index(s)
    loc = collections.defaultdict(collections.deque)
    if "localize" in m.settings:
        rel, enc = m.settings["localize"][:2]
        with open(os.path.join(zdir, rel), encoding="utf-8") as f:
            for name, value in codecs.read_str(f.read(), enc):
                loc[name].append(value)
    assets = []
    for t, name, p in m.assets:
        tid = zoneimage.type_id(t)
        ptr = int(p.get("ptr", "0xffffffff"), 0)
        f = p.get("file")
        if f:
            if f.startswith("@"):
                kind, _, sha = f[1:].partition(":")
                blob = stocks.blob(sha)
                if blob is None:
                    raise BuildError("%s %s: blob %s not found in the %s zones (%s)" % (
                        t, name, sha, kind, "pass --game"))
            else:
                with open(os.path.join(zdir, f), "rb") as g:
                    blob = g.read()
        elif tid == 36:
            path = os.path.join(zdir, "rawfile", name)
            if "gfx" in p:
                pdir = path + ".gfxpatch"
                stock_movie = stocks.rawfile(p["gfx"])
                data = gfx.build_patch(pdir, stock_movie)
                data = compile_as_files(pdir, data, stock_movie, ffdec, compile_as, log)
            else:
                with open(path, "rb") as g:
                    data = g.read()
            blob = codecs.encode_rawfile(name, data, p)
        elif tid == 27:
            if not loc[name]:
                raise BuildError("localize %s: no entry left in localize.str" % name)
            blob = codecs.encode_localize(name, loc[name].popleft())
        elif tid == 37:
            enc = p.get("enc", "ascii")
            with open(os.path.join(zdir, "stringtable", name), encoding="utf-8", newline="") as g:
                rows = codecs.read_csv(g.read(), enc)
            blob = codecs.encode_stringtable(name, rows, p)
        else:
            raise BuildError("%s %s: no file= and no codec" % (t, name))
        if "len" in p and len(blob) != int(p["len"]):
            log("  warning: %s %s is now %d bytes, was %s: this zone has back references (a C#-written zone), so a "
                "resized asset can move data later assets point at; check it in game, or rebuild with its producer"
                % (t, name, len(blob), p["len"]))
        assets.append((tid, ptr, blob))
    left = [n for n, q in loc.items() if q]
    if left:
        log("  warning: localize.str entries not named in the manifest: %s" % ", ".join(left[:8]))
    strings = []
    if "strings" in m.settings:
        with open(os.path.join(zdir, m.settings["strings"][0]), encoding="utf-8") as g:
            strings = json.load(g)
    sp, ap = (int(v, 0) for v in m.settings["xassetlist"])
    blocks = m.settings["blocks"]
    img = zoneimage.ZoneImage(blocks=[0] * 9, string_ptrs=[0xFFFFFFFF if s is not None else 0 for s in strings],
                              strings=strings, string_count_field=len(strings), strings_ptr_field=sp,
                              assets_ptr_field=ap, assets=assets)
    img.blocks = [(sum(len(b) for _t, _p, b in assets) + 16 * len(assets) + MARGIN) if v == "auto" else int(v, 0)
                  for v in blocks]
    if not img.flat and any(v == "auto" for v in blocks):
        raise BuildError("auto block size in a zone with script strings")
    return zoneimage.assemble(img)


def compile_as_files(pdir, movie, stock_movie, ffdec, all_sources, log):
    """Compile the as/ files whose text changed since their tag was stored (gfx.py); with `all_sources` also those
    whose text is not FFDec's decompile of the stored tag (the hand-written sources)."""
    import tempfile
    meta, _ops = gfx.read_patch(pdir)
    for cls, rel, digest in meta["as"]:
        path = os.path.join(pdir, rel)
        if not os.path.isfile(path):
            raise BuildError("%s: %s is listed in patch.txt but missing" % (pdir, rel))
        edited = gfx.source_sha(path) != digest
        if not edited and not all_sources:
            continue
        if not ffdec or not os.path.isfile(ffdec):
            raise BuildError("%s changed since its compiled tag was stored: FFDec (--ffdec) is needed" % rel)
        if not edited:
            with tempfile.TemporaryDirectory() as tmp:
                gfx.export_as(movie, [cls], tmp, ffdec)
                cur = os.path.join(tmp, rel[3:].replace("/", os.sep))
                if os.path.isfile(cur) and open(cur, "rb").read() == open(path, "rb").read():
                    continue
        log("  compiling %s from %s (FFDec)%s" % (cls, rel, "" if edited else ", --compile-as"))
        movie = gfx.compile_as(gfx.with_stock_class(movie, stock_movie, cls), cls, path, ffdec)
    return movie


def build(repo, zone_name, exe, stocks, out_root, fresh_key=False, ffdec=None, compile_as=False, log=print):
    m = load(repo, zone_name)
    zone = assemble(repo, zone_name, m, stocks, ffdec=ffdec, compile_as=compile_as, log=log)
    ft = None if fresh_key else tuple(int(v, 0) for v in m.settings["filetime"])
    rsa = None if fresh_key else bytes.fromhex(m.get("rsa"))
    data = container.write_unsigned(zone, exe, filetime=ft, rsa_blob=rsa, level=int(m.get("zlib", "9")))
    out_dir = os.path.join(out_root, m.get("install").replace("\\", os.sep))
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, zone_name + ".ff")
    with open(out, "wb") as f:
        f.write(data)
    return out, zone, m
