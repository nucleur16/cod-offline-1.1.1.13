#!/usr/bin/env python3
"""The offline lobby's weapon attachment table against the client's.

    python tests/offline/test_m14_attach_table.py <dwtest.exe>

The lobby embeds the Python lobby's attach table (catalogue/weapon_tables.py at feat/mp-m14, resource
weapon_attachments.txt). The client and the match server read mp/coWeaponAttachmentTable.csv from the
Kernel33 patch zone mp_codol_weapons, whose source is zones/mp_codol_weapons/ in this repository. Both
must name the same (weapon, attachment, unlock level) rows: for m14ebr_mp that is the 50 stock rows
plus the eight the zone adds (bip_ar, for_gripafg, for_griphandstop, red_reflex, sra_riflelaser,
sto_ar15fix, sto_ar15prs, sto_ar15ubr).
"""
import csv
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ZONE_TABLE = os.path.join(ROOT, "zones", "mp_codol_weapons", "stringtable", "mp", "coweaponattachmenttable.csv")
ADDED = {"bip_ar": 0, "for_gripafg": 0, "for_griphandstop": 0, "red_reflex": 1, "sra_riflelaser": 14,
         "sto_ar15fix": 7, "sto_ar15prs": 0, "sto_ar15ubr": 0}


def zone_rows():
    rows = {}
    with open(ZONE_TABLE, encoding="latin1", newline="") as fh:
        for row in csv.reader(fh):
            if len(row) < 4 or not row[0] or row[0].startswith("#"):
                continue
            try:
                level = int(row[3] or 0)
            except ValueError:
                continue  # a header row
            rows[(row[0], row[1])] = level
    return rows


def lobby_rows(dwtest):
    out = subprocess.run([dwtest, "--attach-table"], stdout=subprocess.PIPE, check=True).stdout.decode("ascii")
    rows = {}
    for line in out.splitlines():
        weapon, item, level = line.split(",")
        rows[(weapon, item)] = int(level)
    return rows


def main():
    zone, lobby = zone_rows(), lobby_rows(sys.argv[1])
    fails = []
    m14_zone = {k[1]: v for k, v in zone.items() if k[0] == "m14ebr_mp"}
    m14_lobby = {k[1]: v for k, v in lobby.items() if k[0] == "m14ebr_mp"}
    if len(m14_zone) != 58:
        fails.append("the zone has %d m14ebr_mp rows, not 58" % len(m14_zone))
    if m14_zone != m14_lobby:
        fails.append("m14ebr_mp differs: zone only %s, lobby only %s" % (
            sorted(set(m14_zone.items()) - set(m14_lobby.items())), sorted(set(m14_lobby.items()) - set(m14_zone.items()))))
    for name, level in ADDED.items():
        if m14_lobby.get(name) != level:
            fails.append("m14ebr_mp %s: lobby says %r, expected level %d" % (name, m14_lobby.get(name), level))
    # every other weapon: the lobby's table is the client's
    only_zone = sorted(set(zone.items()) - set(lobby.items()))
    only_lobby = sorted(set(lobby.items()) - set(zone.items()))
    if only_zone or only_lobby:
        fails.append("tables differ: %d rows only in the zone (%s), %d only in the lobby (%s)" % (
            len(only_zone), only_zone[:5], len(only_lobby), only_lobby[:5]))
    if fails:
        for f in fails:
            print("FAIL " + f)
        return 1
    print("m14 attach table: %d rows, %d weapons, m14ebr_mp 58 rows: lobby == mp_codol_weapons zone" % (
        len(lobby), len({k[0] for k in lobby})))
    return 0


if __name__ == "__main__":
    sys.exit(main())
