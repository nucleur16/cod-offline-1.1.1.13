"""tools/repair_locked_marksman.py on a synthetic career: which matches it backfills, which weapons the
unlock rule keeps out, which matches it skips, and that the repaired copy is a valid mpdata_s with the
CRC the client checks."""
import json
import os
import struct
import sys
import tempfile
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import repair_locked_marksman as tool  # noqa: E402


def blob(progress):
    buf = bytearray(tool.BUF_SIZE)
    struct.pack_into("<II", buf, 4, tool.SECURE_VERSION, tool.SECURE_CHECKSUM)
    for cid, v in progress.items():
        struct.pack_into("<f", buf, tool.CHALLENGES_AT + 4 * cid, v)
    struct.pack_into("<I", buf, 0, zlib.crc32(bytes(buf[4:])) & 0xFFFFFFFF)
    return zlib.compress(bytes(buf))


def report(kills, weapons, challenges=None, uid="77"):
    prog = {"weapons": weapons}
    if challenges is not None:
        prog["challenges"] = challenges
    return {"players": [{"uid": uid, "bot": False, "kills": kills, "progression": prog},
                        {"bot": True, "kills": 9}]}


def main():
    tables = {"challenges": [
        {"id": 312, "ref": "ch_marksman_m4_mp"},
        {"id": 404, "ref": "ch_marksman_vector_mp"},
        {"id": 529, "ref": "ch_marksman_m14ebr_mp"},
        {"id": 526, "ref": "ch_marksman_ak104_mp"},
        {"id": 357, "ref": "ch_marksman_cheytac_mp"},
    ], "deactivated_ids": [529]}
    catalogue = {
        "item_names": {"1001": "WEA/m4_mp", "1010": "WEA/vector_mp", "1057": "WEA/m14ebr_mp",
                       "1055": "WEA/ak104_mp", "1021": "WEA/cheytac_mp", "3001": "per/specialty_marathon"},
        "products": [
            {"id": 64, "pairs": [[1001, 1]], "flags": []},
            {"id": 940, "pairs": [[1010, 1]], "flags": []},
            {"id": 1365, "pairs": [[1057, 1]], "flags": []},
            {"id": 905, "pairs": [[1055, 1]], "flags": [70]},
            {"id": 84, "pairs": [[1021, 1]], "flags": [12]},
        ],
    }
    reports = {
        # gate closed: only the field-order counter moved; 50 kills at 100 XP each
        "match_report.applied.1.json": report(50, {"m4_mp": 3600, "vector_mp": 600, "m14ebr_mp": 400,
                                                    "ak104_mp": 200, "cheytac_mp": 200}, {"931": 7}),
        # gate closed, no challenges block at all, 50 XP a kill
        "match_report.applied.2.json": report(4, {"vector_mp": 200}),
        # counted normally: has a real challenge
        "match_report.applied.3.json": report(20, {"m4_mp": 2000}, {"312": 20, "930": 1}),
        # XP that does not split the kills evenly
        "match_report.applied.4.json": report(3, {"m4_mp": 100, "vector_mp": 100}),
        # no kills
        "match_report.applied.5.json": report(0, {}, {"931": 2}),
        # somebody else's
        "match_report.applied.6.json": report(10, {"m4_mp": 1000}, uid="88"),
        # not in the journal: its starting level is unknown
        "match_report.applied.7.json": report(1, {"m4_mp": 100}),
    }
    journal = "".join("2026-11-16 00:00:0%d | rank_writeback: player_profiles: 77 +0 XP level %d->%d (total 0) "
                      "| +0 GP (report gameended, players P) -> match_report.applied.%d.json\n" % (n, lvl, lvl, n)
                      for n, lvl in ((1, 4), (2, 1), (3, 6), (4, 6), (5, 6), (6, 6)))
    with tempfile.TemporaryDirectory() as tmp:
        for name, doc in reports.items():
            with open(os.path.join(tmp, name), "w") as f:
                json.dump(doc, f)
        with open(os.path.join(tmp, "match_report.journal.log"), "w") as f:
            f.write(journal)
        # the CheyTac (level 12) was bought; the AK-104 (level 70) was not
        with open(os.path.join(tmp, "owned_items.77.txt"), "w") as f:
            f.write("# owned_items\n1021   0           1     # WEA/cheytac_mp\n")
        tpath = os.path.join(tmp, "progression_tables.json")
        with open(tpath, "w") as f:
            json.dump(tables, f)
        cpath = os.path.join(tmp, "marketplace_catalogue.json")
        with open(cpath, "w") as f:
            json.dump(catalogue, f)
        src = os.path.join(tmp, "mpdata_s")
        with open(src, "wb") as f:
            f.write(blob({312: 20.0, 999: 999999.0}))

        levels = tool.load_unlock_levels(cpath)
        assert levels == {"m4_mp": 1, "vector_mp": 1, "m14ebr_mp": 1, "ak104_mp": 70, "cheytac_mp": 12}, levels
        adds, notes = tool.plan(tmp, "77", tool.load_tables(tpath), levels)
        # a deactivated challenge (529) is credited like any other; the AK-104 is locked at level 4
        assert adds == {312: 36, 404: 6 + 4, 529: 4, 357: 2}, adds
        assert any("ak104_mp unlocks at level 70, the match started at 4: 2 kills" in n for n in notes), notes
        assert any("applied.4.json: skipped" in n for n in notes), notes
        assert any("applied.7.json: skipped, the journal" in n for n in notes), notes

        out = os.path.join(tmp, "repaired")
        argv = ["--reports", tmp, "--uid", "77", "--blob", src, "--tables", tpath, "--catalogue", cpath, "--out", out]
        assert tool.main(argv) == 0
        with open(out, "rb") as f:
            buf = zlib.decompress(f.read())
        assert len(buf) == tool.BUF_SIZE
        assert struct.unpack_from("<I", buf, 0)[0] == zlib.crc32(buf[4:]) & 0xFFFFFFFF
        get = lambda cid: struct.unpack_from("<f", buf, tool.CHALLENGES_AT + 4 * cid)[0]
        assert get(312) == 56.0 and get(404) == 10.0 and get(529) == 4.0 and get(526) == 0.0 and get(999) == 999999.0
        # the input is untouched, and an existing --out is refused
        with open(src, "rb") as f:
            assert struct.unpack_from("<f", zlib.decompress(f.read()), tool.CHALLENGES_AT + 4 * 312)[0] == 20.0
        try:
            tool.main(argv)
            raise AssertionError("an existing --out was overwritten")
        except SystemExit as e:
            assert e.code == 2
    print("PASS: repair_locked_marksman backfills gate-locked marksman kills of unlocked weapons into a new blob only")


if __name__ == "__main__":
    main()
