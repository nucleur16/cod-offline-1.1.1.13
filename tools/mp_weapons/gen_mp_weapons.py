# -*- coding: utf-8 -*-
r"""gen_mp_weapons.py -- the stringtables of mp_weapons (Kernel33 patch zone, loaded ahead of code_pre_gfx).

    python gen_mp_weapons.py --stock <dir> --kernel33 <Kernel33 worktree>

<dir> holds `CodolSoundDumper zone-stringtables --input <zone>.ff --out <dir>/<zone>` dumps of patch_code_post_gfx_mp,
patch_common_gametype_mp and common_gametype_mp (exact strings; the dumps are comma-joined raw cells). Writes
<kernel33>/zones/mp_weapons/stringtable/mp/... and prints the asset lines of zone_source/mp_weapons.csv.

Why every table is a whole copy: C1 links the FIRST stringtable of a name it loads. A Kernel33 patch zone named
mp_* (no _postgfx_mp/_ui_mp suffix) loads ahead of code_pre_gfx, so its tables win over patch_code_post_gfx_mp's
(startup) and over the gametype / ui_viewer copies -- in every session, zombies and co-op included. Each table here is
therefore the effective stock table (patch zone first, the C1 rule) plus rows for m14ebr_mp only. Measured before
writing: ui_viewer_mp's coweapitems tables equal patch_common_gametype_mp's, and no group table written here exists in
the zm or coop zones, so the copies are exact for every session. The sh1x server mounts the same zone (mp_*.ff of the
patch folder) with every MP map, ranked ahead of stock, so client and server read the same rows.

The M14 EBR (m14ebr_mp, marketplace item 1057) is C1's own unfinished MP weapon: the attachment table, the part tables
and the store product exist; it had no base gun (mp/coweapmodels/m14ebr_mp.csv), no class-editor slot rows and was
not released (mp/comarketplaceitems.csv column 6 = 0). The base gun is the zombies M14's receiver
(mp_weapons_postgfx_mp: wea_m14ebr_m14ebr_<camo>[_vmgun]).
"""
import argparse
import csv
import os

WEAPON = "m14ebr_mp"

# the released (comarketplaceitems column 6 = 1) attachments most CoDOL assault rifles take that m14ebr_mp lacked:
# (item, weaponattach asset, variant byte, coweapitems group, the weapon whose model rows it shares)
ADD = [
    ("bip_ar", "bipod_base", "0", "bip", "m16_mp"),
    ("for_gripafg", "grip_afg", "0", "bas", "m16_mp"),
    ("for_griphandstop", "grip_handstop", "0", "bas", "m16_mp"),
    ("red_reflex", "reflex_m14ebr", "1", "sco", "m16_mp"),
    ("sra_riflelaser", "laser_assault", "14", "sra", "m16_mp"),
    ("sto_ar15fix", "stock_shareda", "7", "sto", "m16_mp"),
    ("sto_ar15prs", "stock_shareda", "0", "sto", "m16_mp"),
    ("sto_ar15ubr", "stock_shareda", "0", "sto", "m16_mp"),
]
# the class editor's slot rows: the M21 EBR's (same receiver family, same stock and tracker variants)
UISLOT_FROM = "m21ebr_mp"
CAMOS = 22      # C1 camo rows 0..21 (mp/coweapitems/cam.csv gives m14ebr_mp ids 1..21)
QUOTE = b'"'


def read(path, cols):
    rows = []
    with open(path, "rb") as f:
        for line in f.read().split(b"\n"):
            if not line:
                continue
            cells = line.rstrip(b"\r").split(b",")
            if len(cells) != cols:
                raise SystemExit("%s: a row has %d cells, not %d: %r" % (path, len(cells), cols, line[:80]))
            if any(QUOTE in c for c in cells):
                raise SystemExit("%s: a cell holds a quote: %r" % (path, line[:80]))
            rows.append([c.decode("latin-1") for c in cells])
    return rows


def write(path, rows):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        for r in rows:
            w.writerow(r)


def effective(stock, name):
    for zone in ("patch_common_gametype_mp", "common_gametype_mp"):
        p = os.path.join(stock, zone, *name.split("/"))
        if os.path.isfile(p):
            return p
    raise SystemExit("no stock dump of " + name)


def insert_after_block(rows, key, new):
    """`new` after the last row whose (col0, col1) is `key`'s."""
    at = [i for i, r in enumerate(rows) if r[0] == key[0] and r[1] == key[1]]
    i = at[-1] + 1 if at else len(rows)
    return rows[:i] + new + rows[i:]


def camo(c):
    return "blk" if c == 0 else "%02d" % c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stock", required=True)
    ap.add_argument("--kernel33", required=True)
    a = ap.parse_args()
    zdir = os.path.join(a.kernel33, "zones", "mp_weapons", "stringtable")
    tables = []                 # (name, columns)

    # --- startup tables (patch_code_post_gfx_mp) ---
    att = read(os.path.join(a.stock, "patch_code_post_gfx_mp", "mp", "coweaponattachmenttable.csv"), 4)
    have = {r[1] for r in att if r[0] == WEAPON}
    for item, asset, variant, _g, _ref in ADD:
        if item in have:
            raise SystemExit("%s already has %s" % (WEAPON, item))
        block = [i for i, r in enumerate(att) if r[0] == WEAPON]
        # the weapon's rows stay together and in item order (the table is sorted by weapon, then item), so the
        # weapons' first appearance -- C1's weapon registration order -- is unchanged
        pos = next((i for i in block if att[i][1] > item), block[-1] + 1)
        att.insert(pos, [WEAPON, item, asset, variant])
    write(os.path.join(zdir, "mp", "coweaponattachmenttable.csv"), att)
    tables.append(("mp/coweaponattachmenttable.csv", 4))

    items = read(os.path.join(a.stock, "patch_code_post_gfx_mp", "mp", "comarketplaceitems.csv"), 11)
    row = [r for r in items if r[1] == WEAPON and r[2].lower() == "wea"]
    if len(row) != 1 or row[0][6] != "0":
        raise SystemExit("comarketplaceitems: %s row %r" % (WEAPON, row))
    row[0][6] = "1"
    write(os.path.join(zdir, "mp", "comarketplaceitems.csv"), items)
    tables.append(("mp/comarketplaceitems.csv", 11))

    slots = read(os.path.join(a.stock, "patch_code_post_gfx_mp", "mp", "coweaponuislottable.csv"), 3)
    if any(r[0] == WEAPON for r in slots):
        raise SystemExit("coweaponuislottable already lists " + WEAPON)
    copy = [[WEAPON] + r[1:] for r in slots if r[0] == UISLOT_FROM]
    if len(copy) != 6:
        raise SystemExit("coweaponuislottable: %s has %d rows" % (UISLOT_FROM, len(copy)))
    after = max(i for i, r in enumerate(slots) if r[0] < WEAPON)
    slots = slots[:after + 1] + copy + slots[after + 1:]
    write(os.path.join(zdir, "mp", "coweaponuislottable.csv"), slots)
    tables.append(("mp/coweaponuislottable.csv", 3))

    # --- the base gun, one row per camo ---
    models = [[WEAPON, str(c), "wea_m14ebr_m14ebr_" + camo(c), "wea_m14ebr_m14ebr_%s_vmgun" % camo(c)]
              for c in range(CAMOS)]
    write(os.path.join(zdir, "mp", "coweapmodels", WEAPON + ".csv"), models)
    tables.append(("mp/coweapmodels/%s.csv" % WEAPON, 4))

    # --- the part tables ---
    for group in sorted({g for _i, _a, _v, g, _r in ADD}):
        name = "mp/coweapitems/%s.csv" % group
        rows = read(effective(a.stock, name), 6)
        for item, _a, _v, g, ref in ADD:
            if g != group:
                continue
            if any(r[0] == item and r[1] == WEAPON for r in rows):
                raise SystemExit("%s already has %s rows" % (name, item))
            new = [[item, WEAPON] + r[2:] for r in rows if r[0] == item and r[1] == ref]
            if not new:
                raise SystemExit("%s: %s has no %s rows" % (name, ref, item))
            rows = insert_after_block(rows, (item, ref), new)
        write(os.path.join(zdir, *name.split("/")), rows)
        tables.append((name, 6))

    for name, cols in tables:
        print("stringtable,%s,columns=%d,emptyptr=null" % (name, cols))


if __name__ == "__main__":
    main()
