# -*- coding: utf-8 -*-
r"""build_hud_styles.py extract: the later installs -> zones/hud_styles_postgfx_mp/sources + image/.

Everything is read with CODOL-AssetExplorer's CodolSoundDumper (the installs are never written to):
  zone-menus       the menus of a zone as struct trees (branch feat/zone-menus; for 1.2.1.12 the 1.1.1.13 layout)
  zone-materials   a zone's materials: name, technique set, images
  export-textures  images as PNG
  export-locale    the localize table
What lands in the repository is what `build_hud_styles.py` needs to build without the installs:
  sources/menus_stock.json   1.1.1.13's weaponbar_hd (patch_common_mp) and minimap_fullscreen (common_mp)
  sources/menus_<style>.json the later build's HUD menus a style uses, in that build's own layout
  sources/facts.json         what was measured in the 1.1.1.13 install for the checks (ownerDraw numbers and
                             function calls its stock HUD menus use, which "@KEY" texts and materials it has) and how
                             each material a style's items name is resolved
  image/<prefix><name>.png   the images of the materials a style carries
"""
from __future__ import annotations

import csv
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_hud_styles as hs  # noqa: E402
from zonekit import menus  # noqa: E402

HUD = "ui_mp/hud_720.txt"
# the zones a 1.1.1.13 match always has loaded: a material a style may take from the stock game is in one of them
C1_ZONES = ("code_post_gfx_mp", "patch_code_post_gfx_mp", "common_mp", "patch_common_mp")
# English for the texts 1.1.1.13 has no localize entry for (the later builds are Chinese only; these are plain
# translations of their values, in the style of the game's own MPUI_*_CAPS strings)
ENGLISH = {
    "MPUI_SCORE_CAPS": "SCORE",
    "MPUI_KILL_CAPS": "K",
    "MPUI_DIE_CAPS": "D",
}


def run_tool(dumper, *args, check=True):
    r = subprocess.run([dumper] + list(args), capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode and check:
        raise SystemExit("%s %s failed:\n%s%s" % (dumper, " ".join(args), r.stdout[-2000:], r.stderr[-2000:]))
    return r.stdout


def fastfile(install, zone):
    """The zone's fast file: loose under client_shipRetail\\chinese (or the 2.6.9.7 launcher's decoded resources),
    else inside the IIPS packages (the dumper's "<IIPSDownload>|<path>" form)."""
    for rel in (r"client_shipRetail\chinese",
                r"CODOL_2697_Launcher\runtime\game\decoded-resources\client_shipretail\chinese"):
        p = os.path.join(install, rel, zone + ".ff")
        if os.path.isfile(p):
            return p
    return os.path.join(install, "IIPS", "IIPSDownload") + "|client_shipRetail/chinese/%s.ff" % zone


def exe_of(install):
    for n in ("C1.exe", "codoMP_client_shipRetail.exe"):
        if os.path.isfile(os.path.join(install, n)):
            return os.path.join(install, n)
    raise SystemExit("no client executable in %s" % install)


def dump_menus(dumper, install, zone, out):
    if not os.path.isfile(out):
        run_tool(dumper, "zone-menus", "--input", fastfile(install, zone), "--exe", exe_of(install), "--out", out)
    with open(out, encoding="utf-8") as f:
        return json.load(f)


def hud_menus(dump):
    """{name: menuDef} of the dump's HUD menu file (a ",name" entry is a reference to another zone's menu)."""
    for a in dump["assets"]:
        v = a["value"]
        if a["type"] == "menufile" and v["p"].get("0") == HUD:
            return {m["p"]["0"]: m for m in v["p"].get("8", []) if m and not m["p"]["0"].startswith(",")}
    raise SystemExit("%s has no %s" % (dump.get("zone"), HUD))


def all_menus(dump):
    for a in dump["assets"]:
        v = a["value"]
        for m in (v["p"].get("8", []) if a["type"] == "menufile" else [v]):
            if m and not (m["p"].get("0") or "").startswith(","):
                yield m


def calls(node, ops, out):
    """(function, its first argument when that is a constant string) for every function call of a struct tree."""
    if isinstance(node, list):
        for x in node:
            calls(x, ops, out)
    elif isinstance(node, dict) and "asset" not in node:
        if node.get("kind") == "Statement":
            es = node["p"].get("4", [])
            for i, e in enumerate(es):
                t, a = struct.unpack_from("<II", bytes.fromhex(e["raw"]))
                if t == 0 and a >= 23 and a < len(ops):
                    nxt = es[i + 1] if i + 1 < len(es) else None
                    out.add((ops[a], nxt["p"].get("8") if nxt is not None and hs._is_string(nxt) else None))
        for x in node.get("p", {}).values():
            calls(x, ops, out)


def materials_of(dumper, install, zone):
    """[(name, technique set, [image names])] of zone-materials (later builds: the name column is not theirs)."""
    out = []
    for line in run_tool(dumper, "zone-materials", "--input", fastfile(install, zone), "--exe", exe_of(install)).splitlines():
        f = line.rstrip("\r").split("\t")
        if len(f) < 4:
            continue
        out.append((f[0], f[1], [x.split(":", 1)[1] for x in f[3].split() if ":" in x]))
    return out


def textures(dumper, install, names, out):
    names = {n for n in names if n}
    if not names:
        return {}
    args = ["export-textures", "--game", install, "--out", out]
    for n in sorted(names):
        args += ["--filter", n]
    run_tool(dumper, *args, check=False)      # it fails as a whole when one image has no pixels to export
    return {n: os.path.join(out, "textures", n + ".png") for n in names
            if os.path.isfile(os.path.join(out, "textures", n + ".png"))}


def same_pixels(a, b):
    from PIL import Image
    x, y = Image.open(a).convert("RGBA"), Image.open(b).convert("RGBA")
    return x.size == y.size and x.tobytes() == y.tobytes()


TABLE_ZONES = ("code_post_gfx_mp", "patch_code_post_gfx_mp", "common_mp", "patch_common_mp")


def stringtable(dumper, install, name, work, tag):
    """The rows of a stringtable as the build's HUD sees it (the first of TABLE_ZONES that has it)."""
    for zone in TABLE_ZONES:
        out = os.path.join(work, "tables_%s_%s" % (tag, zone))
        path = os.path.join(out, name.lower().replace("/", os.sep))
        if not os.path.isdir(out):
            ff = fastfile(install, zone)
            if "|" not in ff and not os.path.isfile(ff):
                continue
            run_tool(dumper, "zone-stringtables", "--input", ff, "--exe", exe_of(install), "--out", out, check=False)
            os.makedirs(out, exist_ok=True)
        if os.path.isfile(path):
            with open(path, "rb") as f:
                return [r for r in csv.reader(f.read().decode("latin-1").splitlines())]
    raise SystemExit("%s: no stringtable %s in %s" % (install, name, ", ".join(TABLE_ZONES)))


def locale(dumper, install, out):
    if not os.path.isfile(out):
        run_tool(dumper, "export-locale", "--game", install, "--out", out, "--format", "csv")
    with open(out, encoding="utf-8-sig", newline="") as f:
        return {r[0]: r[1] for r in csv.reader(f) if len(r) >= 2}


def run(a):
    if not a.dumper or not os.path.isfile(a.dumper):
        raise SystemExit("extract needs --dumper <CodolSoundDumper.exe> (built with the zone-menus command)")
    work = a.work or tempfile.mkdtemp(prefix="hud_styles_")
    os.makedirs(work, exist_ok=True)
    # in number order, so a later style reuses an earlier one's identical images
    styles = sorted([s for s in a.styles.split(",") if s], key=lambda s: hs.STYLE_NUMBERS[s])
    installs = {"v26": a.v26, "v12": a.v12}
    image_installs = dict(x.split("=", 1) for x in a.images)
    hs.SRC.mkdir(parents=True, exist_ok=True)

    # --- 1.1.1.13: the stock menus, and what its HUD menus use ---
    c1 = {z: dump_menus(a.dumper, a.c1, z, os.path.join(work, "c1_%s.json" % z)) for z in ("common_mp", "patch_common_mp")}
    stock = {"weaponbar_hd": hud_menus(c1["patch_common_mp"])["weaponbar_hd"],
             "minimap_fullscreen": hud_menus(c1["common_mp"])["minimap_fullscreen"]}
    hs.write_text(hs.SRC / "menus_stock.json", json.dumps(
        {"build": "1.1.1.13", "from": {"weaponbar_hd": "patch_common_mp", "minimap_fullscreen": "common_mp"},
         "menus": stock}, indent=1) + "\n")
    owner_draws, c1_calls, c1_keys, c1_literals = set(), set(), set(), set()
    for d in c1.values():
        for m in all_menus(d):
            calls(m, menus.OPS_1113, c1_calls)
        for m in hud_menus(d).values():
            for it in m["p"].get(str(hs.W13 + 0x78), []):
                owner_draws.add(struct.unpack_from("<i", bytes.fromhex(it["raw"]), 0x38)[0])
                c1_keys.update(hs.loc_keys(it))
                c1_literals.update(e["p"]["8"] for e in hs.material_strings(it))
                if "160" in it["p"]:
                    c1_literals.add((it["p"]["160"].get("name") or "").lstrip(","))
    owner_draws.discard(0)
    c1_loc = locale(a.dumper, a.c1, os.path.join(work, "c1_locale.csv"))
    c1_mats = {}
    for z in C1_ZONES:
        for name, techset, images in materials_of(a.dumper, a.c1, z):
            c1_mats.setdefault(name.lstrip(","), (z, techset, images))

    facts = {"c1_ownerdraws": sorted(owner_draws),
             "c1_calls": sorted([list(c) for c in c1_calls if c[1] is not None]),
             "c1_menu_keys": sorted(c1_keys), "c1_localize": {}, "localize": {}, "styles": {}}
    wanted_images = {}
    for style in styles:
        cfg = hs.STYLES[style]
        install = installs[style]
        dump = dump_menus(a.dumper, install, cfg["zone"], os.path.join(work, "%s_%s.json" % (style, cfg["zone"])))
        if dump["build"] != cfg["build"]:
            raise SystemExit("%s holds %s, not %s" % (install, dump["build"], cfg["build"]))
        if dump["windowSize"] != cfg["window"]:
            raise SystemExit("%s menus were dumped with a %#x window, the style expects %#x" % (
                style, dump["windowSize"], cfg["window"]))
        hud = hud_menus(dump)
        names = [n for ns in cfg["hosts"].values() for n in ns]
        sources = {"build": cfg["build"], "zone": cfg["zone"], "windowSize": cfg["window"],
                   "menus": {n: hud[n] for n in names if n in hud}}
        hs.write_text(hs.SRC / ("menus_%s.json" % style), json.dumps(sources, indent=1) + "\n")

        # which materials and texts would the items use: convert with everything allowed, then resolve each
        class Anything(dict):
            def get(self, k, d=None):
                return {"action": "stock"}

        probe = dict(facts, c1_localize=_Always(), styles={style: {"materials": Anything(), "tables": _Same()}})
        stock_all = dict(stock)
        original = json.loads((hs.SURVIVAL_DIR / "survival_hud_visibility.original.json").read_text(encoding="utf-8"))
        for m in original["p"]["8"]:
            stock_all[m["p"]["0"]] = m
        named = _named_materials(style, probe, sources, stock_all)
        keys = set(named.pop("@keys"))
        tables = {}
        for table, search, result in sorted(named.pop("@tables")):
            mine, theirs = stringtable(a.dumper, a.c1, table, work, "c1"), stringtable(a.dumper, install, table, work, style)
            # The lookup runs in 1.1.1.13 on 1.1.1.13's table with 1.1.1.13's own key, so the keys need not agree
            # between the builds (the killstreak ids were renumbered). What must hold is that the result column
            # means the same thing: for the rows both builds have, by their name (column 1 of a table searched by
            # its id column 0), the cell is the same.
            ident = 1 if search == 0 else search
            need = max(ident, result)
            rows = {r[ident]: r[result] for r in mine if len(r) > need}
            shared = [(r[ident], r[result]) for r in theirs if len(r) > need and r[ident] in rows]
            differ = [k for k, v in shared if rows[k] != v]
            tables["%s,%d,%d" % (table, search, result)] = {
                "rows_by": ident, "shared": len(shared), "differ": len(differ),
                "only_%s" % cfg["build"]: sorted(r[ident] for r in theirs if len(r) > need and r[ident] not in rows)}
        later = materials_of(a.dumper, install, cfg["zone"])
        by_image = {}
        for name, techset, images in later:
            for im in images:
                by_image.setdefault(im.lstrip(","), []).append((techset, images))
        tdir = os.path.join(work, "tex_" + style)
        got = textures(a.dumper, image_installs.get(style, install), set(named), tdir)
        c1dir = os.path.join(work, "tex_c1_" + style)
        c1got = textures(a.dumper, a.c1, {c1_mats[n][2][0].lstrip(",") for n in named
                                         if n in c1_mats and len(c1_mats[n][2]) == 1}, c1dir)
        mats = {}
        for name, is_ref in sorted(named.items()):
            in_c1 = c1_mats.get(name)
            defs = by_image.get(name, [])
            if is_ref and in_c1:
                mats[name] = dict(action="stock", why="a reference in %s too; 1.1.1.13 %s has it" % (cfg["build"], in_c1[0]))
                continue
            if in_c1 and len(in_c1[2]) == 1 and name in got and in_c1[2][0].lstrip(",") in c1got and \
                    same_pixels(got[name], c1got[in_c1[2][0].lstrip(",")]):
                mats[name] = dict(action="stock", why="1.1.1.13 %s has it with the same image" % in_c1[0])
                continue
            if name not in got:
                # nothing to compare (its image has another name, or it is not an image material): the stock HUD
                # menus draw a material of this name themselves, so the style's item draws that one. Only when the
                # install's images could be exported at all: otherwise nothing says the stock art is the style's.
                if name in c1_literals and got:
                    mats[name] = dict(action="stock", why="no image of this name in %s to compare; 1.1.1.13's own "
                                      "HUD menus draw a material of this name" % cfg["build"])
                else:
                    mats[name] = dict(action="missing", why="no image %s in %s" % (name, cfg["build"]))
                continue
            if not defs or any(t != ",2d" or len(ims) != 1 for t, ims in defs):
                mats[name] = dict(action="missing", why="not a one-image `2d` material in %s (%s)" % (cfg["build"], defs))
                continue
            asset = cfg["prefix"] + name
            for other, path in wanted_images.items():       # an earlier style carries the same pixels: one asset
                if other.split("_", 1)[1] == name and same_pixels(path, got[name]):
                    asset = other
            mats[name] = dict(action="carry", asset=asset, why="%s; image %s as %s" % (
                "1.1.1.13 has no material of this name" if not in_c1 else "1.1.1.13's %s differs" % name, name, asset))
            wanted_images.setdefault(asset, got[name])
        facts["styles"][style] = {"materials": mats, "tables": tables}
        missing = sorted(k for k in keys if k not in c1_loc)
        later_loc = locale(a.dumper, install, os.path.join(work, "%s_locale.csv" % style)) if missing else {}
        for k in keys:
            facts["c1_localize"][k] = k in c1_loc
        for k in missing:
            if k in ENGLISH and k in later_loc and k not in facts["localize"]:
                facts["localize"][k] = {"english": ENGLISH[k], "chinese": later_loc[k], "from": cfg["build"]}
    facts["c1_localize"] = dict(sorted(facts["c1_localize"].items()))
    hs.write_text(hs.SRC / "facts.json", json.dumps(facts, indent=1, ensure_ascii=False) + "\n")

    # the carried images: only what the kept items use after the real checks
    idir = hs.ZDIR / "image"
    idir.mkdir(parents=True, exist_ok=True)
    used = set()
    for _n, style in hs.shipped():
        _parts, _report, conv = hs.style_parts(style)
        used |= conv.used_materials
    for old in idir.glob("*.png"):
        old.unlink()
    for name in sorted(used):
        shutil.copyfile(wanted_images[name], idir / (name + ".png"))
    print("extracted %s into %s (%d images)" % (", ".join(styles), hs.SRC, len(used)))


class _Same(dict):
    def get(self, k, d=None):
        return {"differ": 0}


class _Always(dict):
    def get(self, k, d=None):
        return True


def _named_materials(style, probe, sources, stock):
    """{material name: is a reference in the later build} of the items that pass every other check, + "@keys"."""
    cfg = hs.STYLES[style]
    W = cfg["window"]
    out = {"@keys": set()}
    refs = {}
    for name in [n for ns in cfg["hosts"].values() for n in ns]:
        m = sources["menus"].get(name)
        for it in (m["p"].get(str(W + 0x78), []) if m else []):
            if "160" in it["p"]:
                n = it["p"]["160"].get("name") or ""
                refs[n.lstrip(",")] = n.startswith(",")
    parts, _report, conv = hs.style_parts(style, probe, sources, stock)
    for part in parts.values():
        for it in part["items"]:
            if "160" in it["p"]:
                n = it["p"]["160"]["name"].lstrip(",")
                out[n] = refs.get(n, False)
            for e in hs.material_strings(it):
                out.setdefault(e["p"]["8"], False)
    out["@keys"] = set(conv.keys)
    out["@tables"] = set(conv.tables)
    return out
