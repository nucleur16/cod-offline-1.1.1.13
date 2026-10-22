# -*- coding: utf-8 -*-
r"""Shipped zone -> zones/<zone>/ (editable files and blobs) + zone_source/<zone>.csv.

Every decoded asset is re-encoded on the spot and must give back the exact blob, so a build of an untouched tree
reproduces the shipped zone image byte for byte; anything that does not round-trip stays a blob.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import zlib

from . import codecs, container, gfx, manifest, zoneimage
from .zoneimage import type_name

SAFE = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_ .,()&+#@$~!'%-]*(/[A-Za-z0-9_][A-Za-z0-9_ .,()&+#@$~!'%-]*)*$")
MARGIN = 0x10000


def safe_file(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", name.lstrip(","))[:80] or "unnamed"


def auto_block(img) -> int:
    return sum(len(b) for _t, _p, b in img.assets) + 16 * len(img.assets) + MARGIN


def decompile(zone_name, cfg, path, exe, repo, stocks, ffdec=None, log=print):
    with open(path, "rb") as f:
        data = f.read()
    u = container.read_unsigned(data, exe)
    img = zoneimage.parse(u.zone)
    zdir = os.path.join(repo, "zones", zone_name)
    # hand-written as/ sources (zonelist "handwritten") and their .inc files survive a refresh
    handwritten = set().union(*(cfg.get("handwritten") or {}).values())
    kept = {}
    for root, _d, files in os.walk(zdir):
        for fn in files:
            rel = os.path.relpath(os.path.join(root, fn), zdir).replace(os.sep, "/")
            if ".gfxpatch/as/" not in rel:
                continue
            cls = rel.split(".gfxpatch/as/", 1)[1]
            if fn.endswith(".inc") or (cls.endswith(".as") and cls[:-3].replace("/", ".") in handwritten):
                with open(os.path.join(root, fn), "rb") as f:
                    kept[rel] = f.read()
    if os.path.isdir(zdir):
        shutil.rmtree(zdir)
    os.makedirs(zdir)
    for rel, kdata in kept.items():
        os.makedirs(os.path.dirname(os.path.join(zdir, rel)), exist_ok=True)
        with open(os.path.join(zdir, rel), "wb") as f:
            f.write(kdata)
    m = manifest.Manifest()
    m.comments.append("%s -- built by build_zones.py from zones/%s/ (zonekit/manifest.py describes the format)"
                      % (zone_name, zone_name))
    m.settings["zone"] = [zone_name]
    m.settings["install"] = [cfg["install"].replace("/", "\\")]
    m.settings["origin"] = [cfg["origin"]]
    m.settings["origin-md5"] = [hashlib.md5(data).hexdigest()]
    m.settings["container"] = ["iwffu100"]
    m.settings["filetime"] = ["%#010x" % u.filetime[0], "%#010x" % u.filetime[1]]
    m.settings["rsa"] = [u.rsa_blob.hex()]
    if container.compress(u.zone, 9) == u.compressed:
        m.settings["zlib"] = ["9"]
    else:
        m.settings["zlib"] = ["9", "shipped stream not zlib level 9 (written by .NET); zone image identical, file differs"]
    blocks = ["%#x" % b for b in img.blocks]
    if img.flat and img.blocks[4] == auto_block(img):
        blocks[4] = "auto"
    m.settings["blocks"] = blocks
    m.settings["xassetlist"] = ["%#x" % img.strings_ptr_field, "%#x" % img.assets_ptr_field]
    if img.string_count_field:
        with open(os.path.join(zdir, "script_strings.json"), "w", newline="\n") as f:
            json.dump(img.strings, f, indent=0)
            f.write("\n")
        m.settings["strings"] = ["script_strings.json"]
        if [0xFFFFFFFF if s is not None else 0 for s in img.strings] != img.string_ptrs:
            raise SystemExit("%s: string pointers not derivable from the strings" % zone_name)
    stock_names = cfg.get("stock") or []
    if stock_names:
        m.settings["stock"] = stock_names
        for s in stock_names:
            stocks.index(s)

    localize = []
    used_paths = set()
    stats = {"decoded": 0, "blob": 0, "stock": 0, "gfxpatch": 0}

    def blob_line(i, t, name, blob, params):
        sha = hashlib.sha1(blob).hexdigest()
        if stocks.blob(sha) is not None and stock_names:
            params["file"] = "@stock:" + sha
            stats["stock"] += 1
        else:
            rel = "%s/%04d_%s.bin" % (type_name(t), i, safe_file(name or "unnamed"))
            os.makedirs(os.path.join(zdir, os.path.dirname(rel)), exist_ok=True)
            with open(os.path.join(zdir, rel), "wb") as f:
                f.write(blob)
            params["file"] = rel
            stats["blob"] += 1
        return (type_name(t), name or "", params)

    for i, (t, ptr, blob) in enumerate(img.assets):
        params = {}
        if ptr != 0xFFFFFFFF:
            params["ptr"] = "%#x" % ptr
        line = None
        try:
            if t == 36:
                name, rdata, p = codecs.decode_rawfile(blob)
                if codecs.encode_rawfile(name, rdata, p) != blob:
                    raise codecs.NotDecodable("rawfile does not re-encode")
                rel = "rawfile/" + name
                if not SAFE.match(name) or rel.lower() in used_paths:
                    raise codecs.NotDecodable("rawfile name not usable as a path")
                params.update(p)
                gref = (cfg.get("gfx") or {}).get(name)
                if gref:
                    stock_movie = stocks.rawfile(gref)
                    pdir = os.path.join(zdir, rel + ".gfxpatch")
                    n = gfx.write_patch(pdir, gref, stock_movie, rdata, ffdec=ffdec,
                                        handwritten=(cfg.get("handwritten") or {}).get(name, ()))
                    if gfx.build_patch(pdir, stock_movie) != rdata:
                        raise SystemExit("%s: gfx patch of %s does not rebuild" % (zone_name, name))
                    params["gfx"] = gref
                    stats["gfxpatch"] += 1
                    log("  %s: %d tags differ from %s" % (name, n, gref))
                else:
                    os.makedirs(os.path.dirname(os.path.join(zdir, rel)), exist_ok=True)
                    with open(os.path.join(zdir, rel), "wb") as f:
                        f.write(rdata)
                used_paths.add(rel.lower())
                line = ("rawfile", name, params)
            elif t == 27:
                name, value = codecs.decode_localize(blob)
                if codecs.encode_localize(name, value) != blob or "\t" in name or "\n" in name:
                    raise codecs.NotDecodable("localize does not re-encode")
                localize.append((name, value))
                line = ("localize", name, params)
            elif t == 37:
                name, rows, p = codecs.decode_stringtable(blob)
                rel = "stringtable/" + name
                if not SAFE.match(name) or rel.lower() in used_paths:
                    raise codecs.NotDecodable("stringtable name not usable as a path")
                enc = codecs.text_encoding([c for r in rows for c in r])
                text = codecs.write_csv(rows, enc)
                if codecs.encode_stringtable(name, codecs.read_csv(text, enc), p) != blob:
                    raise codecs.NotDecodable("stringtable does not re-encode")
                if enc != "ascii":
                    p["enc"] = enc
                os.makedirs(os.path.dirname(os.path.join(zdir, rel)), exist_ok=True)
                with open(os.path.join(zdir, rel), "w", encoding="utf-8", newline="") as f:
                    f.write(text)
                used_paths.add(rel.lower())
                params.update(p)
                line = ("stringtable", name, params)
        except (codecs.NotDecodable, ValueError, zlib.error, UnicodeError) as e:
            line = None
            params = {k: v for k, v in params.items() if k == "ptr"}
            if t in (27, 36, 37):
                log("  asset %d (%s): kept as a blob: %s" % (i, type_name(t), e))
        if line is None:
            line = blob_line(i, t, codecs.guess_name(t, blob), blob, params)
        else:
            stats["decoded"] += 1
            if not img.flat:
                line[2]["len"] = str(len(blob))   # build_zones warns when an edit resizes it (back references)
        m.assets.append(line)

    if localize:
        enc = codecs.text_encoding([v for _n, v in localize])
        text = codecs.write_str(localize, enc)
        if codecs.read_str(text, enc) != localize:
            raise SystemExit("%s: localize table does not round-trip" % zone_name)
        with open(os.path.join(zdir, "localize.str"), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        m.settings["localize"] = ["localize.str", enc]
    os.makedirs(os.path.join(repo, "zone_source"), exist_ok=True)
    with open(os.path.join(repo, "zone_source", zone_name + ".csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write(manifest.write(m))
    return stats, img
