#!/usr/bin/env python
"""Backfill the marksman kills a career lost while challenges were locked.

Until the rawfiles fix (fix/challenge-tracking, _missions.gsc updateChallenges) the match server
counted no challenge progress for a player whose rank was below IW4's "challenges" unlock (rank
index 4, level 5), and only re-read that gate when the player connected: a match started at level 4
or lower counted nothing for the whole match. CoDOL's front end shows the marksman challenges from
level 1, so those kills simply went missing and tier 1 arrived ten counted kills later.

The applied match reports keep what is needed to recover the marksman part of it. A report written
under the gate has kills but no challenge in its "challenges" block other than the field-order
career counters (930 / 931 / 932, credited by _fieldorders.gsc past the gate; any match with challenges
enabled and a kill reports others), and its "weapons" block is the weapon XP of the match, which every kill paid
at the same per-kill rate. kills(weapon) = xp(weapon) * kills / sum(xp) is used only where it is a
whole number for every weapon of that match; anything else is reported and skipped. Headshots,
perk challenges and the like cannot be recovered and are not touched.

The rule the match server applies now is applied here too: a weapon's challenges count only once
that weapon is unlocked (rawfiles _missions.gsc challengeItemUnlocked). The weapon's unlock level is
the lowest uf_rank_N among the store products that sell it (the lobby's marketplace_catalogue.json;
no rank flag = level 1, uf_item_hide = 99), and the player's level is the one the match started at
(match_report.journal.log, "level A->B ... -> <report>"). A weapon above that level counts only if it
is in owned_items.<uid>.txt (bought). A match the journal does not place is skipped.

It never writes in place: it reads a COPY of the player's mpdata_s and writes the repaired blob to
--out, a new file. Put it back only while the game and its lobby are closed (the lobby holds the
career in memory for the run and would write its own copy over it). Tiers the backfill crosses are
completed silently: their XP / GP are not paid.

    python tools/repair_locked_marksman.py --reports <copy of p2p/offline> --uid 8600260456219318799 \\
        --blob <copy of players/Player/mpdata_s> --tables <lobby resources>/progression_tables.json \\
        --catalogue <lobby resources>/marketplace_catalogue.json [--out repaired_mpdata_s]
"""
import argparse
import glob
import json
import os
import re
import struct
import sys
import zlib

BUF_SIZE = 32768
SECURE_VERSION = 11
SECURE_CHECKSUM = 0x096B8F57
CHALLENGES_AT = 0x0174  # challenges[1000] of float, mp/playersecuredata.def v11
CHALLENGE_COUNT = 1000


def load_tables(path):
    with open(path, encoding="utf-8") as f:
        doc = json.load(f)
    by_ref = {str(c["ref"]).lower(): c for c in doc["challenges"]}
    return by_ref


UF_ITEM_HIDE = 663  # mp/unlockFlags.csv; uf_rank_N is flag id N (1..70)


def load_unlock_levels(path):
    """-> {weapon name: the level its store products unlock it at}."""
    with open(path, encoding="utf-8") as f:
        doc = json.load(f)
    names = doc.get("item_names") or {}
    out = {}
    for p in doc.get("products") or ():
        if not p.get("pairs"):
            continue
        cat, _, name = str(names.get(str(p["pairs"][0][0]), "")).partition("/")
        if cat.lower() != "wea" or not name:
            continue
        need = 1
        for flag in p.get("flags") or ():
            flag = int(flag)
            need = max(need, 99 if flag == UF_ITEM_HIDE else flag if 1 <= flag <= 70 else 1)
        out[name.lower()] = min(out.get(name.lower(), 99), need)
    return out


def start_levels(reports_dir):
    """-> {report file name: the player's level when that match started}, from the journal."""
    out = {}
    path = os.path.join(reports_dir, "match_report.journal.log")
    if os.path.exists(path):
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.search(r" level (\d+)->\d+ .*-> (match_report\.applied\.[\w.-]+\.json)", line)
                if m:
                    out[m.group(2)] = int(m.group(1))
    return out


def owned_weapons(reports_dir, uid):
    """-> the weapons the lobby's purchase manifest lists for this uid."""
    out = set()
    path = os.path.join(reports_dir, "owned_items.%s.txt" % uid)
    if os.path.exists(path):
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.search(r"#\s*WEA/(\S+)", line)
                if m:
                    out.add(m.group(1).lower())
    return out


def human_row(report, uid):
    for p in report.get("players") or ():
        if not p.get("bot") and str(p.get("uid")) == str(uid):
            return p
    return None


FIELD_ORDER_IDS = frozenset((930, 931, 932))  # coFieldOrderBootcamp / MPOnboarding / zm_FieldOrder


def lost_kills(report, uid):
    """-> (per-weapon kills, None) for a match the gate ate, (None, reason) to skip it, or (None, None)
    for a match that was counted normally."""
    row = human_row(report, uid)
    if not row:
        return None, None
    prog = row.get("progression") or {}
    kills = int(row.get("kills") or 0)
    counted = [k for k in (prog.get("challenges") or {}) if not (str(k).isdigit() and int(k) in FIELD_ORDER_IDS)]
    if kills <= 0 or counted:
        return None, None
    weapons = {k: int(v) for k, v in (prog.get("weapons") or {}).items() if int(v) > 0}
    total = sum(weapons.values())
    if not total:
        return None, "%d kills but no weapon XP to split them by" % kills
    out = {}
    for w, xp in sorted(weapons.items()):
        if (xp * kills) % total:
            return None, "weapon XP %s does not split %d kills evenly" % (weapons, kills)
        out[w] = xp * kills // total
    return out, None


def plan(reports_dir, uid, by_ref, unlock_levels):
    adds, notes = {}, []
    started = start_levels(reports_dir)
    owned = owned_weapons(reports_dir, uid)
    for path in sorted(glob.glob(os.path.join(reports_dir, "match_report.applied.*.json"))):
        with open(path, encoding="utf-8") as f:
            report = json.load(f)
        per, why = lost_kills(report, uid)
        name = os.path.basename(path)
        if why:
            notes.append("%s: skipped, %s" % (name, why))
        if not per:
            continue
        if name not in started:
            notes.append("%s: skipped, the journal does not say what level it started at" % name)
            continue
        for weapon, n in per.items():
            ref = "ch_marksman_" + weapon.lower()
            ch = by_ref.get(ref)
            if ch is None:
                notes.append("%s: %s has no %s" % (name, weapon, ref))
                continue
            need = unlock_levels.get(weapon.lower(), 1)
            if need > started[name] and weapon.lower() not in owned:
                notes.append("%s: %s unlocks at level %d, the match started at %d: %d kills not credited"
                             % (name, weapon, need, started[name], n))
                continue
            adds[int(ch["id"])] = adds.get(int(ch["id"]), 0) + n
            notes.append("%s: %s +%d" % (name, ref, n))
    return adds, notes


def load_blob(path):
    with open(path, "rb") as f:
        buf = bytearray(zlib.decompress(f.read()))
    if len(buf) != BUF_SIZE:
        raise ValueError("%s inflates to %d bytes, not %d" % (path, len(buf), BUF_SIZE))
    ver, chk = struct.unpack_from("<II", buf, 4)
    if ver != SECURE_VERSION or chk != SECURE_CHECKSUM:
        raise ValueError("%s is not an mpdata_s v11 blob (version %d, checksum 0x%08X)" % (path, ver, chk))
    return buf


def apply(buf, adds):
    for cid, n in adds.items():
        if not 0 <= cid < CHALLENGE_COUNT:
            raise ValueError("challenge id %d is outside challenges[%d]" % (cid, CHALLENGE_COUNT))
        at = CHALLENGES_AT + 4 * cid
        struct.pack_into("<f", buf, at, struct.unpack_from("<f", buf, at)[0] + n)
    struct.pack_into("<I", buf, 0, zlib.crc32(bytes(buf[4:])) & 0xFFFFFFFF)
    return zlib.compress(bytes(buf), 9)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--reports", required=True, help="a copy of the install's p2p/offline folder")
    ap.add_argument("--uid", required=True, help="the player's uid (account.txt uid =)")
    ap.add_argument("--blob", required=True, help="a copy of the player's mpdata_s")
    ap.add_argument("--tables", required=True, help="the lobby's progression_tables.json")
    ap.add_argument("--catalogue", required=True, help="the lobby's marketplace_catalogue.json")
    ap.add_argument("--out", help="write the repaired blob here (a new file; never the input)")
    args = ap.parse_args(argv)

    by_ref = load_tables(args.tables)
    adds, notes = plan(args.reports, args.uid, by_ref, load_unlock_levels(args.catalogue))
    buf = load_blob(args.blob)
    for line in notes:
        print(line)
    if not adds:
        print("nothing to backfill")
        return 0
    refs = {int(c["id"]): r for r, c in by_ref.items()}
    for cid, n in sorted(adds.items()):
        before = struct.unpack_from("<f", buf, CHALLENGES_AT + 4 * cid)[0]
        print("challenge %d %s: %g -> %g" % (cid, refs.get(cid, "?"), before, before + n))
    if not args.out:
        print("dry run: pass --out FILE to write a repaired copy")
        return 0
    if os.path.exists(args.out):
        ap.error("--out %s exists; the tool only writes new files" % args.out)
    data = apply(buf, adds)
    with open(args.out, "wb") as f:
        f.write(data)
    print("wrote %s (%d bytes)" % (args.out, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
