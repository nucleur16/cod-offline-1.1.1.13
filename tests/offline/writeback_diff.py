#!/usr/bin/env python3
"""Differential test of the match-report write-back: career/rank_writeback.py against offline/dw_career.cpp.

    export DWDIFF_ROOT=/d/CoD/k33work/offl PYLOBBY_SRC=/d/CoD/k33work/dw-emu
    DWTEST_OBJ=obj_offline_test4 DWTEST_OUT=$DWDIFF_ROOT/slot4/dwtest.exe sh tests/offline/build_dwtest.sh
    python tests/offline/writeback_diff.py --dwtest $DWDIFF_ROOT/slot4/dwtest.exe [--slot 4] [-k name] [-v]

The captures hold no match reports, so the scenarios below are synthetic match_report.json files built
from the report schema (offline/dw_career.h). For every scenario, on a fresh level-1 profile of a fake
uid:

  1. the Python lobby's own rank_writeback.maybe_apply() folds the reports, in a private copy of the
     Python tree ($DWDIFF_ROOT/slot<N>/wb/pylobby - never the original) with its resources redirected
     to the scenario's own folder;
  2. `dwtest --differential --writeback` folds the same bytes through the C++ port;

and the results are compared: every profile (total XP, level, counters, lifetime, challenge / weapon /
coop / intel progress), the GP wallet, the retry state, how the report files were archived, the
unlock-flag set and weapon XP derived from the profile, and the inflated mpdata_s buffers.

THE PORT'S SIDE IS READ BACK FROM ITS FILES. Every step is its own dwtest process, and the port keeps
no profile document: a step rebuilds the career from the player's mpdata_s file and the account.txt
career lines the step before wrote (offline/dw_store.h), credits the report and writes both again. So
a scenario that matches also proves that nothing of the career is lost between two runs of the client.
Two members of the Python profile are not part of that career and are not compared: `history` (the
Python lobby's credit log, a database table) and `seeded_from` (where the profile came from: for the
port that is the file). And where the Python lobby has not rendered a blob yet (it renders when the
profile is served), the port already has one - the file is the career from the first moment - so
there is nothing to compare that buffer with.

CLOCKS. Both sides run at the same pinned time (time.time patched / --fixed-time), so NOTHING is
blanked: the clock-derived values - the profile's created / updated and the secure
DDL's challegeUpdateTime at byte 0x0170 (with the CRC32 at byte 0 that covers it) - must be equal too.
A buffer mismatch is reported field by field through the Python tree's playerdata_editor.

Scenarios marked python_bug are the two places where the offline lobby deliberately does better than
the Python one; they are compared with --differential (which reproduces Python) and then run once more
without it to show the product behaviour.

Nothing here opens a socket. Everything is written under $DWDIFF_ROOT, never into the repository.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import zlib

T = 1790000000                       # the pinned clock of both sides
U = "9000000000000000004"            # the fake account under test
V = "9000000000000000005"            # a second fake account (no profile until a report names it)
W = "9000000000000000006"
NAME = "edtest"


# --------------------------------------------------------------------------- #
# scenario construction
# --------------------------------------------------------------------------- #

def human(slot, uid=U, total=0, kills=0, deaths=0, assists=0, name=NAME, applied_live=0, outcome="win", **extra):
    r = {"slot": slot, "name": name, "bot": False, "team": 2, "kills": kills, "deaths": deaths,
         "assists": assists, "score": total, "outcome": outcome,
         "xp": {"kill_xp": max(0, total - 200), "match_bonus": min(200, total), "total": total,
                "applied_live": applied_live}}
    if uid is not None:
        r["uid"] = uid
    r.update(extra)
    return r


def bot(slot, name="bot0", **extra):
    r = {"slot": slot, "name": name, "bot": True, "team": 1, "kills": 2, "deaths": 10, "assists": 0,
         "score": 200, "outcome": "loss"}
    r.update(extra)
    return r


def report(rows, reason="timelimit", **extra):
    rep = {"format": 1, "created": T - 5, "reason": reason, "server_time_ms": 600000, "duration_s": 600.0,
           "team_scores": [0, 1000], "players": rows}
    rep.update(extra)
    return json.dumps(rep)


def step(text, uid=U, background=False, name="match_report.json", files=None):
    """One maybe_apply() call: the report file(s) dropped before it, the connection uid, the mode."""
    f = dict(files) if files else {name: text}
    return {"files": f, "uid": uid, "background": background}


LIFETIME = {"score": 1250, "kills": 9, "deaths": 3, "assists": 2, "headshots": 4, "suicides": 1, "hits": 31,
            "totalShots": 120, "wins": 1, "timePlayedTotal": 540, "timePlayed": 540, "killStreak": 4,
            "winStreak": 1, "someFutureMember": 7, "accuracy": 2583, "kdRatio": 3000}

A_REPORT = report([human(0, total=1500, kills=9, deaths=3, assists=2), bot(1)])
B_REPORT = report([human(0, total=300, kills=1, deaths=1), bot(1)], reason="scorelimit")

SCENARIOS = [
    ("win_xp", "single player win with XP", [
        step(report([human(0, total=1500, kills=9, deaths=3, assists=2, outcome="win"), bot(1)]))]),
    ("loss_xp", "single player loss with XP", [
        step(report([human(0, total=300, kills=1, deaths=8, assists=1, outcome="loss"), bot(1)], reason="scorelimit"))]),
    ("rankup_gp", "a rank-up that pays GP (level 1 -> 11), the server's own rank-up GP replaced", [
        step(report([human(0, total=40000, kills=30, deaths=4,
                           progression={"gp": 500, "gp_fixed": {"rankups": 300, "play": 200}}), bot(1)]))]),
    ("rankup_twice", "two matches: GP is paid once per level, off the profile's level mark", [
        step(report([human(0, total=2500, kills=3)], reason="m1")),
        step(report([human(0, total=1800, kills=2, progression={"gp": 90})], reason="m2")),
        step(report([human(0, total=9000, kills=5)], reason="m3"))]),
    ("last_rank", "XP far past the last rank: level 70, the rank triple clamped inside it", [
        step(report([human(0, total=99999999, kills=1)]))]),
    ("exact_threshold", "XP exactly on / one short of a rank threshold", [
        step(report([human(0, total=1999)], reason="short")),
        step(report([human(0, total=1)], reason="exact"))]),
    ("lifetime", "kills / deaths / assists and the open-keyed lifetime counters", [
        step(report([human(0, total=1500, kills=9, deaths=3, assists=2, lifetime=LIFETIME), bot(1)])),
        step(report([human(0, total=200, kills=2, deaths=6, assists=0,
                           lifetime={"score": 300, "kills": 2, "deaths": 6, "losses": 1, "timePlayedTotal": 200,
                                     "hits": 5, "totalShots": 61, "killStreak": 1}), bot(1)], reason="m2"))]),
    ("zero_xp_match", "a zero-XP match still moves the lifetime members and the match counter", [
        step(report([human(0, total=0, deaths=5, lifetime={"deaths": 5, "timePlayedTotal": 60, "losses": 1}), bot(1)]))]),
    ("nothing_to_add", "no XP, no lifetime, no progression: consumed without a credit", [
        step(report([human(0, total=0), bot(1)]))]),
    ("challenges", "challenge progress: by ref, by id, mixed case, unknown, deactivated, negative", [
        step(report([human(0, total=100, progression={"challenges": {
            "ch_ghillie": 120, "1": 10, " CH_GrenadeKill ": 45, "ch_not_a_real_challenge": 3,
            "ch_masterblaster": 9, "zm_marksman_m1911_zm": 260, "ch_marksman_ak47_mp": 0}}), bot(1)])),
        step(report([human(0, total=0, progression={"challenges": {"ch_ghillie": 90.5, "60": -500, "1": 2}})], reason="m2"))]),
    ("challenges_flat", "the older flat challenges / weapons keys on the row itself", [
        step(report([human(0, total=50, challenges={"ch_ghillie": 51}, weapons={"ak47_mp": 600},
                           weapon_uses={"ak47_mp": 9}, collectibles={"3": 1})]))]),
    ("weapon_xp", "weapon XP: primary, secondary, mixed case, unknown weapon, usage counts", [
        step(report([human(0, total=700, progression={
            "weapons": {"ak47_mp": 1600, "AK104_MP": 499, "beretta393_mp": 500, "not_a_weapon": 7},
            "weapon_uses": {"ak47_mp": 12, "AK104_MP": 3}}), bot(1)]))]),
    ("weapon_levelup", "weapon level-ups across matches drive the derived mastery challenge", [
        step(report([human(0, total=400, progression={"weapons": {"ak47_mp": 14000}})], reason="m1")),
        step(report([human(0, total=400, progression={"weapons": {"ak47_mp": 30000, "at4_mp": 80000}})], reason="m2")),
        step(report([human(0, total=0, progression={"weapons": {"ak47_mp": 200000}})], reason="m3"))]),
    ("coop_missions", "coop mission progress: a failure, a completion, a replay, the prologue, Spec Ops", [
        step(report([human(0, total=0, progression={"coop_mission_failed": "coop_training_mp_killhouse_sh"})], reason="c1")),
        step(report([human(0, total=250, progression={"coop_mission_completed": "coop_training_mp_killhouse_sh"})], reason="c2")),
        step(report([human(0, total=250, progression={"coop_mission_completed": " COOP_Training_mp_killhouse_sh "})], reason="c3")),
        step(report([human(0, total=0, progression={"coop_mission_completed": "coop_prolog_mp_crash_intro"})], reason="c4")),
        step(report([human(0, total=0, progression={"coop_mission_completed": "coop_spec_mp_estate_tropical"})], reason="c5")),
        step(report([human(0, total=0, progression={"coop_mission_completed": "mp_crash",
                                                     "coop_mission_failed": "coop_take_mp_fav_tropical"})], reason="c6"))]),
    ("zombies", "a zombies match: zm challenges, story intel, lifetime, no XP", [
        step(report([human(0, total=0, kills=140, deaths=3,
                           lifetime={"kills": 140, "deaths": 3, "timePlayedTotal": 1500, "score": 9000},
                           progression={"challenges": {"zm_marksman_m1911_zm": 120, "zm_expert_m1911_zm": 55, "600": 5},
                                        "collectibles": {"0": 1, "1": 1, "23": 1}, "gp": 60})], reason="zm_round_12"))]),
    ("collectibles", "story intel: a set, out of range and unusable indices skipped", [
        step(report([human(0, total=10, progression={"collectibles": {"5": 1, "3": 1, "2000": 1, "7": 0, "-1": 1}})], reason="i1")),
        step(report([human(0, total=10, progression={"collectibles": {"5": 1, "1023": 2}})], reason="i2"))]),
    ("duplicate", "the same report dropped twice (then a third time): credited once", [
        step(A_REPORT), step(A_REPORT), step(A_REPORT)]),
    ("malformed", "malformed reports are archived as .bad and never applied", [
        step('{"format": 1, "players": [{"slot": 0'),
        step(report([human(0, total=500)], format=2)),
        step(json.dumps({"format": 1, "players": []})),
        step(json.dumps({"format": 1, "players": [{"slot": 0, "name": NAME}]})),
        step(report([human(0, total=500, lifetime={"kills": 2.5})])),
        step(report([human(0, total=500, uid=1.5)])),
        step(json.dumps({"format": 1, "players": [{"slot": 0, "name": NAME, "bot": False, "uid": U, "xp": {"total": "x"}}]})),
        step("﻿" + A_REPORT),
        step(json.dumps([1, 2, 3]))]),
    ("unknown_uid", "a report for an account the lobby has never seen; the asking connection did not play", [
        step(report([human(0, uid=V, total=900, kills=4, deaths=7, assists=1, name="stranger"), bot(1)]))]),
    ("bad_row_uid", "a row whose uid cannot name an account is skipped, not folded into another row", [
        step(report([human(0, uid=U, total=600, kills=5, deaths=5), human(1, uid="../../evil", total=5000, kills=99, name="mallory"),
                     human(2, uid=True, total=7000, name="bool")]))]),
    ("int_uid", "the row uid as a JSON integer", [
        step(report([human(0, uid=int(V), total=350, kills=2, name="intuid")]))]),
    ("bots", "a report with bots: their rows are ignored whatever they carry", [
        step(report([bot(0, xp={"total": 9000, "applied_live": 0}, uid=V, lifetime={"kills": 50}),
                     human(1, total=800, kills=6, deaths=2, assists=3, lifetime={"kills": 6, "score": 800}),
                     bot(2, name="bot1", progression={"challenges": {"ch_ghillie": 500}, "gp": 999}),
                     bot(3, name="bot2", xp="nonsense", lifetime=17)]))]),
    ("only_bots", "a report with no human row", [
        step(report([bot(0), bot(1, name="bot1")]))]),
    ("two_humans", "two humans: each profile takes its own row, the connection (a third account) nothing", [
        step(report([human(0, uid=U, total=1500, kills=9, deaths=3, assists=2, lifetime={"kills": 9}),
                     human(1, uid=V, total=900, kills=4, deaths=7, assists=1, name="bravo", lifetime={"kills": 4, "deaths": 7}),
                     bot(7)]), uid=W)]),
    ("reconnect", "one account on two rows (a reconnect): summed, one match", [
        step(report([human(0, total=400, kills=3, deaths=1, lifetime={"kills": 3}),
                     human(4, total=250, kills=2, deaths=2, name="edtest2", lifetime={"kills": 2, "deaths": 2})]))]),
    ("applied_live", "applied_live does not reduce a profile's credit", [
        step(report([human(0, total=2000, applied_live=1500, kills=10, deaths=2)]))]),
    ("legacy_no_uid", "rows without a uid: the poller leaves the report, the asking connection takes the aggregate", [
        step(report([human(0, uid=None, total=300, kills=3, deaths=1, lifetime={"kills": 3}),
                     human(1, uid=None, total=200, kills=2, deaths=2, name="guest", lifetime={"kills": 2}), bot(2)]),
             uid=None, background=True),
        step(None, files={})]),
    ("shared_blob", "no row uid and no connection uid: the shared blob takes total - applied_live", [
        step(report([human(0, uid=None, total=2000, applied_live=1500)]), uid=None),
        step(report([human(0, uid=None, total=40000, applied_live=0)], reason="m2"), uid=None),
        step(report([human(0, uid=None, total=500, applied_live=500)], reason="m3"), uid=None)]),
    ("string_numbers", "numbers carried as strings where int() accepts them", [
        step(json.dumps({"format": 1, "reason": 7, "players": [
            {"slot": "0", "name": NAME, "bot": 0, "uid": U, "kills": "3", "deaths": "x", "assists": 2.9,
             "xp": {"total": "250", "applied_live": None if False else 0},
             "progression": {"challenges": {"ch_ghillie": "50"}, "weapons": {"ak47_mp": "500"}, "gp": "40"}}]}))]),
    ("spool", "the spool is folded oldest first, after the legacy slot; an in-flight .tmp is not a report", [
        step(None, background=True, uid=None, files={
            "match_report.q.1790000001.17760.2.json": report([human(0, total=20, kills=1)], reason="q2"),
            "match_report.q.1790000001.17760.10.json": report([human(0, total=30, kills=1)], reason="q10"),
            "match_report.q.1790000001.17760.1.json": report([human(0, total=10, kills=1)], reason="q1"),
            "match_report.json": report([human(0, total=5, kills=1)], reason="legacy"),
            "match_report.q.1790000001.17760.3.json.tmp": "half a rep"})]),
    ("background_poll", "the once-a-second poller folds a report whose rows name their accounts", [
        step(report([human(0, total=1500, kills=9, lifetime={"kills": 9}), bot(1)]), uid=None, background=True)]),
    ("dup_after_newer", "PYTHON BUG reproduced under --differential: A, B, then A again is credited twice", [
        step(A_REPORT), step(B_REPORT), step(A_REPORT)]),
    ("unusable_progression", "PYTHON BUG reproduced under --differential: an unusable collectible retains the report, "
                             "and every retry credits its XP again", [
        step(report([human(0, total=50, progression={"collectibles": {"abc": 1, "4": 1}})])),
        step(None, files={}),
        step(None, files={}, uid=None, background=True)]),
]
PYTHON_BUGS = {
    "dup_after_newer": "the product refuses the re-dropped report through the ledger: total_xp stays 1800",
    "unusable_progression": "the product archives the report after ONE credit (50 XP) instead of retaining it",
}


# --------------------------------------------------------------------------- #
# the Python side (runs inside the private copy)
# --------------------------------------------------------------------------- #

def inflate_hex(path):
    try:
        with open(path, "rb") as fh:
            return zlib.decompress(fh.read()).hex()
    except (OSError, zlib.error):
        return ""


def match_files(folder):
    return sorted(n for n in os.listdir(folder) if n.startswith("match_report."))


def py_worker(pylobby, scen_dir, steps_path):
    import time
    time.time = lambda: float(T)                     # the same pinned clock as dwtest --fixed-time
    os.chdir(pylobby)
    sys.path.insert(0, pylobby)
    for k in ("CODOL_RANK_WRITEBACK", "CODOL_PER_UID", "CODOL_LOBBY_DB"):
        os.environ.pop(k, None)
    os.environ["CODOL_MARKETPLACE_BALANCE"] = "0"
    from career import rank_writeback, progression
    from store import player_profiles, lobby_db
    res = os.path.join(scen_dir, "resources")
    players = player_profiles.players_dir_for(res)
    with open(steps_path) as fh:
        steps = json.load(fh)
    player_profiles.load(U, players, res, create=True)           # the fresh level-1 account
    notes = []
    for st in steps:
        for name, text in st["files"].items():
            with open(os.path.join(res, name), "wb") as fh:
                fh.write(text.encode("utf-8"))
        notes.append(rank_writeback.maybe_apply(uid=st["uid"], resources=res, background=st["background"]))
    out = {"notes": notes, "profiles": {}, "wallet": {}, "blobs": {}, "flags": {}, "weapon_xp": {}, "mirror_differs": {}}
    db = lobby_db.get_db(lobby_db.db_path_for(res))
    db.submit(lambda conn: None).result(30)                      # the history rows are written behind
    for uid in sorted(player_profiles.list_uids(players)):
        prof = player_profiles.load(uid, players, res, create=False)
        if prof is None:
            continue
        # players/<uid>.json is the Python lobby's profile document; the port has no such file and
        # rebuilds the same value from mpdata_s + account.txt. The credit history is the database's
        # profile_history table (load() answers [] and the JSON mirror therefore only ever holds the
        # latest credit); the port does not keep one, and compare() leaves it out.
        with open(os.path.join(players, "%s.json" % uid), encoding="utf-8") as fh:
            mirror = json.load(fh)
        out["mirror_differs"][uid] = sorted(k for k in set(prof) | set(mirror)
                                            if k != "history" and prof.get(k) != mirror.get(k))
        prof = mirror
        prof["history"] = db.get_history(uid, 50)
        out["profiles"][uid] = prof
        out["wallet"][uid] = db.wallet_get(uid, 1, None)
        out["blobs"][uid] = inflate_hex(os.path.join(res, "mpdata_s.%s" % uid))
        out["flags"][uid] = progression.granted_flag_ids(prof)
        out["weapon_xp"][uid] = {str(k): v for k, v in sorted(progression.weapon_xp_by_item(prof).items())}
    out["shared"] = inflate_hex(os.path.join(res, "mpdata_s"))
    out["state"] = rank_writeback._load_state(res)
    out["files"] = match_files(res)
    sys.stdout.write(json.dumps(out))
    sys.stdout.flush()
    lobby_db.close_db()


# --------------------------------------------------------------------------- #
# comparison
# --------------------------------------------------------------------------- #

def kinds(files):
    """How the report files ended up: {applied, dup, bad, pending}."""
    k = {"applied": 0, "dup": 0, "bad": 0, "pending": 0}
    for n in files:
        if n in ("match_report.journal.log", "match_report.state.json") or ".tmp" in n:
            continue
        for kind in ("applied", "dup", "bad"):
            if n.startswith("match_report.%s." % kind):
                k[kind] += 1
                break
        else:
            if n == "match_report.json" or n.startswith("match_report.q."):
                k["pending"] += 1
    return k


def note_kind(note):
    """The outcome a note reports, without its free-form text."""
    out = []
    for part in note.split(" || "):
        out.append("none" if not part else "bad" if "BAD REPORT" in part else "retained" if "RETAINED" in part
                   else "dup" if "duplicate report" in part else "nothing" if "nothing to add" in part else "applied")
    return out


def norm(v):
    """JSON values as comparable data: 50 and 50.0 are the same number, key order is not data."""
    if isinstance(v, dict):
        return {str(k): norm(x) for k, x in v.items()}
    if isinstance(v, list):
        return [norm(x) for x in v]
    if isinstance(v, bool) or v is None or isinstance(v, str):
        return v
    return float(v)


def blob_field_diff(pylobby, a_hex, b_hex):
    """-> the DDL members whose value differs between two inflated mpdata_s buffers."""
    if not a_hex or not b_hex:
        return ["one side has no blob (python %d B, c++ %d B)" % (len(a_hex) // 2, len(b_hex) // 2)]
    a, b = bytes.fromhex(a_hex), bytes.fromhex(b_hex)
    out = []
    try:
        sys.path.insert(0, pylobby)
        from store import playerdata_editor as P
        ba, bb = P.Blob("mpdata_s", bytearray(a)), P.Blob("mpdata_s", bytearray(b))
    except Exception:                                            # noqa: BLE001
        ba = None
    if ba is not None:
        for f in ba.defn.fields:
            va, vb = ba.get(f), bb.get(f)
            if va != vb:
                out.append("%s: python %r, c++ %r" % (f.path, va, vb))
    if not out:
        offs = [i for i in range(min(len(a), len(b))) if a[i] != b[i]]
        out.append("%d differing bytes, first at 0x%04X" % (len(offs), offs[0] if offs else -1))
    return out[:12]


NOT_THE_CAREER = ("history", "seeded_from")     # see the module text: not kept by the port, on purpose


def compare(pylobby, py, cc):
    """-> list of difference lines (empty = the scenario matches)."""
    diffs = []
    pp, cp = py["profiles"], cc["profiles"]
    if sorted(pp) != sorted(cp):
        diffs.append("profile set: python %s, c++ %s" % (sorted(pp), sorted(cp)))
    for uid in sorted(set(pp) & set(cp)):
        a, b = norm(pp[uid]), norm(cp[uid])
        for key in sorted(set(a) | set(b)):
            if key in NOT_THE_CAREER:
                continue
            if a.get(key, "<absent>") != b.get(key, "<absent>"):
                diffs.append("profile %s.%s: python %r, c++ %r" % (uid, key, a.get(key, "<absent>"), b.get(key, "<absent>")))
        if py["mirror_differs"].get(uid):
            diffs.append("python: players/%s.json and load() disagree on %s" % (uid, py["mirror_differs"][uid]))
        if py["wallet"].get(uid) != cc["wallet"].get(uid):
            diffs.append("wallet GP %s: python %r, c++ %r" % (uid, py["wallet"].get(uid), cc["wallet"].get(uid)))
        if py["flags"].get(uid) != cc["flags"].get(uid):
            fa, fb = set(py["flags"].get(uid) or []), set(cc["flags"].get(uid) or [])
            diffs.append("unlock flags %s: only python %s, only c++ %s" % (uid, sorted(fa - fb)[:10], sorted(fb - fa)[:10]))
        if py["weapon_xp"].get(uid) != cc["weapon_xp"].get(uid):
            diffs.append("weapon xp by item %s: python %r, c++ %r" % (uid, py["weapon_xp"].get(uid), cc["weapon_xp"].get(uid)))
        if not cc["blobs"].get(uid, ""):
            diffs.append("c++: %s has a profile and no mpdata_s file" % uid)
        elif py["blobs"].get(uid, "") and py["blobs"][uid] != cc["blobs"][uid]:   # python renders only when it serves
            for line in blob_field_diff(pylobby, py["blobs"].get(uid, ""), cc["blobs"].get(uid, "")):
                diffs.append("mpdata_s %s: %s" % (uid, line))
        if cc["exported_matches_store"].get(uid) is False:
            diffs.append("c++: the match server's mpdata_s.%s is not the store's blob" % uid)
    if py["shared"] != cc["shared"]:
        for line in blob_field_diff(pylobby, py["shared"], cc["shared"]):
            diffs.append("shared mpdata_s: %s" % line)
    sa, sb = norm(py["state"] or {}), norm(cc["state"] or {})
    if sa != sb:
        diffs.append("retry state: python %r, c++ %r" % (sa, sb))
    if kinds(py["files"]) != kinds(cc["files"]):
        diffs.append("report files: python %r, c++ %r" % (kinds(py["files"]), kinds(cc["files"])))
    ka, kb = [note_kind(n) for n in py["notes"]], [note_kind(n) for n in cc["notes"]]
    if ka != kb:
        diffs.append("outcome per call: python %r, c++ %r" % (ka, kb))
    return diffs


# --------------------------------------------------------------------------- #
# the driver
# --------------------------------------------------------------------------- #

def run_cpp(dwtest, resources, base, steps, differential=True):
    prof, match = os.path.join(base, "profile"), os.path.join(base, "match")
    shutil.rmtree(base, ignore_errors=True)
    os.makedirs(prof)
    os.makedirs(match)
    notes, last = [], None
    for i, st in enumerate(steps):
        for name, text in st["files"].items():
            with open(os.path.join(match, name), "wb") as fh:
                fh.write(text.encode("utf-8"))
        cmd = [dwtest, "--fixed-time", str(T)]
        if differential:
            cmd.append("--differential")
        if st["background"]:
            cmd.append("--wb-background")
        if i == 0:
            cmd += ["--wb-create", U]
        cmd += ["--writeback", resources, prof, match, st["uid"] or "0"]
        r = subprocess.run(cmd, capture_output=True, timeout=120)
        if r.returncode != 0:
            raise RuntimeError("dwtest failed (%d): %s" % (r.returncode, r.stderr.decode("utf-8", "replace")[-400:]))
        last = json.loads(r.stdout.decode("utf-8"))
        notes.append(last["note"])
    last["notes"] = notes
    last["files"] = [n for n in last["files"] if n.startswith("match_report.")]
    return last


def summary(side, uid=U):
    p = side["profiles"].get(uid) or {}
    return "xp %s level %s matches %s GP %s" % (p.get("total_xp"), p.get("level"), p.get("matches"), side["wallet"].get(uid))


def winpath(p):
    """/d/x (as the msys shell spells it) -> d:/x, for a native Python."""
    if p and os.name == "nt" and len(p) > 2 and p[0] == "/" and p[2] == "/" and p[1].isalpha():
        return p[1] + ":" + p[2:]
    return p


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--py-worker":
        return py_worker(sys.argv[2], sys.argv[3], sys.argv[4])
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dwtest", required=True, help="dwtest.exe built from this tree")
    ap.add_argument("--slot", default="4")
    ap.add_argument("--root", default=None, help="work folder (default $DWDIFF_ROOT/slot<slot>/wb)")
    ap.add_argument("--pylobby-src", default=os.environ.get("PYLOBBY_SRC"))
    ap.add_argument("-k", default=None, help="only scenarios whose name contains this")
    ap.add_argument("-v", "--verbose", action="store_true")
    a = ap.parse_args()
    root = a.root or os.path.join(winpath(os.environ.get("DWDIFF_ROOT")) or ap.error("set DWDIFF_ROOT or --root"), "slot%s" % a.slot, "wb")
    if not a.pylobby_src:
        ap.error("set PYLOBBY_SRC or --pylobby-src")
    root = os.path.abspath(winpath(root))
    a.dwtest, a.pylobby_src = os.path.abspath(winpath(a.dwtest)), os.path.abspath(winpath(a.pylobby_src))
    here = os.path.dirname(os.path.abspath(__file__))
    if os.path.commonpath([root, os.path.dirname(os.path.dirname(here))]) == os.path.dirname(os.path.dirname(here)):
        ap.error("the work folder must be outside the repository")
    pylobby = os.path.join(root, "pylobby")
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(root)
    shutil.copytree(a.pylobby_src, pylobby, ignore=shutil.ignore_patterns(
        ".git", "__pycache__", "players", "captures", "lobby.sqlite*", "*.log", "match_report.*"))
    resources = os.path.join(pylobby, "resources")              # pristine: both sides only read it
    seed = "mpdata_s.level1.proposed"
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", PYTHONIOENCODING="utf-8")

    failed, rows = 0, []
    for name, what, steps in SCENARIOS:
        if a.k and a.k not in name:
            continue
        steps = [dict(s, files={k: v for k, v in s["files"].items() if v is not None}) for s in steps]
        sdir = os.path.join(root, name)
        pres = os.path.join(sdir, "py", "resources")
        os.makedirs(pres)
        shutil.copy(os.path.join(resources, seed), os.path.join(pres, seed))
        steps_path = os.path.join(sdir, "steps.json")
        with open(steps_path, "w") as fh:
            json.dump(steps, fh)
        r = subprocess.run([sys.executable, os.path.abspath(__file__), "--py-worker", pylobby, os.path.join(sdir, "py"), steps_path],
                           capture_output=True, env=env, timeout=300)
        if r.returncode != 0 or not r.stdout:
            print("FAIL  %-22s the Python side did not run: %s" % (name, r.stderr.decode("utf-8", "replace")[-600:]))
            failed += 1
            continue
        py = json.loads(r.stdout.decode("utf-8"))
        cc = run_cpp(a.dwtest, resources, os.path.join(sdir, "c"), steps)
        for side, data in (("python.json", py), ("cpp.json", cc)):  # kept beside the scenario for a closer look
            with open(os.path.join(sdir, side), "w") as fh:
                json.dump(data, fh, indent=1, sort_keys=True)
        diffs = compare(pylobby, py, cc)
        nblob = len([u for u in py["blobs"] if py["blobs"][u]]) + (1 if py["shared"] else 0)
        verdict = "FAIL" if diffs else "pass"
        failed += bool(diffs)
        print("%s  %-22s %s" % (verdict, name, what))
        print("      %d call(s) %s | %s | %d profile(s), %d mpdata_s buffer(s) compared" % (
            len(steps), "+".join(k[0] if len(k) == 1 else "(" + ",".join(k) + ")" for k in map(note_kind, py["notes"])),
            summary(py), len(py["profiles"]), nblob))
        for d in diffs:
            print("      DIFF " + d)
        if a.verbose:
            for i, (x, y) in enumerate(zip(py["notes"], cc["notes"])):
                print("      call %d python: %s" % (i, x))
                print("      call %d c++   : %s" % (i, y))
        if name in PYTHON_BUGS:
            prod = run_cpp(a.dwtest, resources, os.path.join(sdir, "c_product"), steps, differential=False)
            print("      product (no --differential): %s, files %s -- %s" % (summary(prod), kinds(prod["files"]), PYTHON_BUGS[name]))
            print("      python                     : %s, files %s" % (summary(py), kinds(py["files"])))
        rows.append((name, verdict))
    print("\n%d scenario(s) compared, %d failed" % (len(rows), failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main() or 0)
