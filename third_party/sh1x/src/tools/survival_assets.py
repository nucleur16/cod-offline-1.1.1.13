r"""List the assets Spec Ops Survival actually references: the root set of common_gametype_survival.

The user's rule, 2026-10-27: "just don't have survival load more assets than it needs".  The zone may
carry only what survival uses that the zones loaded with it do not already provide.  Since the first
definition of a name wins, a duplicate is pure waste.  The zones loaded with it are common_mp,
common_gametype_mp and their patch/localized zones, and the level zone.

This tool walks what survival can reach and prints the names it references, by asset type, with where
each came from.  The zone builder (AssetExplorer port-map --roots-file) then keeps those roots and their
dependencies, minus anything the loaded zones define.  The walk covers:

  * the scripts only survival reaches: tools/script_resolve.py's survival reachable set minus
    its mp set, i.e. the survival gametype, its modules, the per-level data, the soldier
    animscripts, the survival aitypes and characters, and the shared _laststand.  MP scripts' assets
    are MP's, and the MP zones provide them;
  * the wave tables of the 14 survival levels (each level's own table, else tier_1).  Only the AI
    types those tables name are used, so only those types' aitypes and character looks count;
  * the AI type table (weapon, alt weapon), the loot table, and the kiosk rows the menus offer
    (icons, names, prices);
  * the menus, the localized strings and the animlists the actors' trees use.

Usage:  python tools/survival_assets.py [--rawfiles DIR] [--json OUT.json] [--roots OUT.txt]

--roots writes one "<type>:<name>" per line, the format port-map --roots-file reads.
"""
import argparse
import csv
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
RAW = os.path.join(REPO, "bin", "home", "codol_rawfiles")


def read_csv(path):
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8", errors="replace", newline="") as f:
        return [r for r in csv.reader(f)]


def rows_by_index(rows):
    out = {}
    for r in rows:
        if r and r[0].strip().lstrip("-").isdigit():
            out[int(r[0])] = r
    return out


def cell(r, i):
    return r[i].strip() if r is not None and i < len(r) else ""


class Roots:
    def __init__(self):
        self.items = {}

    def add(self, kind, name, why):
        if not name:
            return
        self.items.setdefault(kind, {}).setdefault(name, set()).add(why)

    def as_json(self):
        return {k: {n: sorted(w) for n, w in sorted(v.items())} for k, v in sorted(self.items.items())}


def reachable(category, gametype, level, rawfiles):
    cmd = [sys.executable, os.path.join(HERE, "script_resolve.py"), "--category", category,
           "--gametype", gametype, "--map", level, "--list-reachable", "--root", rawfiles]
    out = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO).stdout
    return {l.split()[0] for l in out.splitlines() if l.strip() and l.split()[0].endswith(".gsc")}


LITERAL_CALLS = [
    # (regex of the call, asset type)
    (r'\b(?:precacheModel|setModel|attach|precacheMounted|modelMounted|pick_model)\s*\(\s*"([^"]+)"', "xmodel"),
    (r'\bpick_model\s*\(\s*"[^"]+"\s*,\s*"([^"]+)"', "xmodel"),
    (r'\bloadFx\s*\(\s*"([^"]+)"', "fx"),
    (r'\b(?:playSound|playLoopSound|playSoundAtPosition|playLocalSound|playSoundOnTag)\s*\(\s*"([^"]+)"', "sound"),
    (r'\bprecacheVehicle\s*\(\s*"([^"]+)"', "vehicle"),
    (r'\bspawn_heli\s*\([^;]*?"([a-z0-9_]+_mp)"', "vehicle"),
    (r'\b(?:precacheShader|precacheMaterial|setShader)\s*\(\s*"([^"]+)"', "material"),
    (r'\bprecacheMenu\s*\(\s*"([^"]+)"', "menu"),
    (r'\bprecacheShellShock\s*\(\s*"([^"]+)"', "shellshock"),
    (r'\bprecacheRumble\s*\(\s*"([^"]+)"', "rumble"),
    (r'\b(?:precacheItem|codol_isWeaponMounted|codol_getPartyWeapon|giveWeapon|compose_weapon)\s*\(\s*"([^"|]+)', "weapon"),
    (r'\bladder\s*\[[^\]]*\]\s*=\s*"([^"|]+)', "weapon"),
    (r'level\.survival_chopper_(?:gun|rocket)\s*=\s*"([^"]+)"', "weapon"),
    (r'\btableLookup\w*\s*\(\s*"([^"]+\.csv)"', "stringtable"),
    (r'level\.survival_\w*table\s*=\s*"([^"]+\.csv)"', "stringtable"),
    (r'&"([A-Z0-9_]+)"', "localize"),
    (r'level\.survival_shield_model\s*=\s*"([^"]+)"', "xmodel"),
    (r'level\.survival_kiosk_models\s*\[[^\]]*\]\s*=\s*"([^"]+)"', "xmodel"),
]

# Scripts whose model names come through the table walk (only used types count).
TABLE_DRIVEN = ("aitype/", "character/character_so_survival.gsc", "character/character_coop_soldier.gsc")


def scan_scripts(files, rawfiles, roots):
    for rel in sorted(files):
        if rel.startswith(TABLE_DRIVEN):
            continue
        path = os.path.join(rawfiles, rel)
        if not os.path.exists(path):
            continue
        text = open(path, encoding="utf-8", errors="replace").read()
        text = re.sub(r"//[^\n]*", "", text)
        for rx, kind in LITERAL_CALLS:
            for m in re.finditer(rx, text):
                roots.add(kind, m.group(1), rel)


def looks(rawfiles):
    """character_so_survival look(kind): kind -> models (bodies, heads, extra, fallback)."""
    text = open(os.path.join(rawfiles, "character", "character_so_survival.gsc"), encoding="utf-8").read()
    heads = {}
    for name, body in re.findall(r"\n(\w+_heads)\(\)\s*\{(.*?)\n\}", text, re.S):
        heads[name] = re.findall(r'"([^"]+)"', body)
    body = re.search(r"\nlook\( kind \)\s*\{(.*?)\n\}", text, re.S).group(1)
    fallback = re.search(r'l\.fallback\s*=\s*"([^"]+)"', body).group(1)
    out = {}
    pending = []
    for line in body.splitlines():
        m = re.match(r'\s*case "([^"]+)":', line)
        if m:
            pending.append(m.group(1))
            continue
        if re.match(r"\s*default:", line):
            pending.append("default")
            continue
        for k in pending:
            out.setdefault(k, [])
        for k in pending:
            out[k] += re.findall(r'"([a-z0-9_]+)"', line) if "l." in line and "=" in line else []
            hm = re.search(r"l\.heads\s*=\s*(\w+_heads)\(\)", line)
            if hm:
                out[k] += heads.get(hm.group(1), [])
        if re.match(r"\s*break;", line):
            pending = []
    return out, fallback


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--rawfiles", default=RAW)
    ap.add_argument("--json")
    ap.add_argument("--roots")
    ap.add_argument("--level", default="mp_fav_tropical", help="any survival level, for the reachability walk")
    a = ap.parse_args(argv)
    raw = a.rawfiles
    roots = Roots()

    # 1. survival-only scripts
    sv = reachable("survival", "survival", a.level, raw)
    mp = reachable("mp", "war", a.level, raw)
    only = sv - mp
    if not only:
        sys.exit("no survival-only scripts found: the resolver walk failed")
    scan_scripts(only, raw, roots)

    # 2. the levels and their wave tables
    levels = sorted(f[:-4] for f in os.listdir(os.path.join(raw, "maps", "mp", "survival")) if f.endswith(".gsc"))
    type_table = "mp/survival_waves.csv"
    tables = set([type_table])
    for lv in levels:
        short = lv[3:] if lv.startswith("mp_") else lv
        t = "mp/survival_waves_%s.csv" % short
        if os.path.exists(os.path.join(raw, t)):
            tables.add(t)
        else:
            tables.add("mp/survival_waves_tier_1.csv")
    for t in tables:
        roots.add("stringtable", t, "wave table")
    refs = set()
    for t in tables:
        for r in read_csv(os.path.join(raw, t)):
            if not r or not r[0].strip().isdigit() or int(r[0]) >= 100:
                continue
            for col in (3, 5, 7, 8):
                refs.update(cell(r, col).split())
            title = cell(r, 11)
            if title:
                roots.add("localize", title, t)

    # 3. the AI types the tables use
    types = rows_by_index(read_csv(os.path.join(raw, type_table)))
    by_ref = {cell(r, 1): r for i, r in types.items() if 100 <= i < 200}
    look, fallback = looks(raw)
    used_models = False
    for ref in sorted(refs):
        r = by_ref.get(ref)
        if r is None:
            continue
        cls = cell(r, 4)
        for w in (cell(r, 5).split("|")[0], cell(r, 6)):
            if w:
                roots.add("weapon", w, "AI type " + ref)
        if not cls:
            continue  # the choppers and dogs (MP dog, scripted helicopters)
        aitype = os.path.join(raw, "aitype", cls[len("actor_"):] + ".gsc")
        if not os.path.exists(aitype):
            continue
        m = re.search(r'character_so_survival::main\(\s*"([^"]+)"', open(aitype, encoding="utf-8").read())
        if not m:
            continue
        kind = m.group(1)
        for model in look.get(kind, []):
            roots.add("xmodel", model, "AI type %s (%s)" % (ref, kind))
        used_models = True
    if used_models:
        roots.add("xmodel", fallback, "character fallback (precached)")

    # 4. loot and kiosk tables
    for r in read_csv(os.path.join(raw, "mp", "survival_loot.csv")):
        if r and r[0].strip().isdigit() and int(r[0]) <= 20 and cell(r, 2) == "weapon":
            roots.add("weapon", cell(r, 1), "loot")
    kiosks = rows_by_index(read_csv(os.path.join(raw, "mp", "kiosks.csv")))
    for i in list(range(0, 5)) + [10, 11, 13, 14] + list(range(90, 100)):
        r = kiosks.get(i)
        if r is None:
            continue
        roots.add("material", cell(r, 6), "kiosk row %d" % i)
        roots.add("localize", cell(r, 4), "kiosk row %d" % i)
        roots.add("localize", cell(r, 5), "kiosk row %d" % i)
    for t in ("mp/kiosks.csv", "mp/survival_loot.csv", "mp/survival_perks.csv", "mp/survivalbounties.csv"):
        roots.add("stringtable", t, "survival table")

    # 5. what the actors' trees and the HUD need
    for al in ("animlists/common_human.animlist", "animlists/common_actor_shared.animlist"):
        roots.add("rawfile", al, "soldier species (serverinfo 2482 bit 0x01)")
    roots.add("rawfile", "animlists/common_dog.animlist", "dog species (bit 0x02)")

    data = roots.as_json()
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump({"levels": levels, "tables": sorted(tables), "types": sorted(refs & set(by_ref)),
                       "roots": data}, f, indent=1)
    if a.roots:
        with open(a.roots, "w", encoding="utf-8", newline="\n") as f:
            for k, v in data.items():
                for n in v:
                    f.write("%s:%s\n" % (k, n))
    for k, v in data.items():
        print("%-12s %4d  %s" % (k, len(v), " ".join(list(v)[:12]) + (" ..." if len(v) > 12 else "")))
    print("types used: %s" % " ".join(sorted(refs & set(by_ref))))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
