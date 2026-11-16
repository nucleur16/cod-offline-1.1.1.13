"""tools/repair_locked_marksman.py on a synthetic career: which matches it backfills, which it skips,
and that the repaired copy is a valid mpdata_s with the CRC the client checks."""
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
    tables = {
        "challenges": [
            {"id": 312, "ref": "ch_marksman_m4_mp"},
            {"id": 404, "ref": "ch_marksman_vector_mp"},
            {"id": 529, "ref": "ch_marksman_m14ebr_mp"},
        ],
        "deactivated_ids": [529],
    }
    reports = {
        # gate closed: only the field-order counter moved; 46 kills at 100 XP each
        "match_report.applied.1.json": report(46, {"m4_mp": 3600, "vector_mp": 600, "m14ebr_mp": 400}, {"931": 7}),
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
    }
    with tempfile.TemporaryDirectory() as tmp:
        for name, doc in reports.items():
            with open(os.path.join(tmp, name), "w") as f:
                json.dump(doc, f)
        tpath = os.path.join(tmp, "progression_tables.json")
        with open(tpath, "w") as f:
            json.dump(tables, f)
        src = os.path.join(tmp, "mpdata_s")
        with open(src, "wb") as f:
            f.write(blob({312: 20.0, 999: 999999.0}))

        by_ref, deactivated = tool.load_tables(tpath)
        adds, notes = tool.plan(tmp, "77", by_ref, deactivated)
        assert adds == {312: 36, 404: 6 + 4}, adds
        assert any("ch_marksman_m14ebr_mp is deactivated, 4 kills" in n for n in notes), notes
        assert any("applied.4.json: skipped" in n for n in notes), notes

        out = os.path.join(tmp, "repaired")
        assert tool.main(["--reports", tmp, "--uid", "77", "--blob", src, "--tables", tpath, "--out", out]) == 0
        with open(out, "rb") as f:
            buf = zlib.decompress(f.read())
        assert len(buf) == tool.BUF_SIZE
        assert struct.unpack_from("<I", buf, 0)[0] == zlib.crc32(buf[4:]) & 0xFFFFFFFF
        get = lambda cid: struct.unpack_from("<f", buf, tool.CHALLENGES_AT + 4 * cid)[0]
        assert get(312) == 56.0 and get(404) == 10.0 and get(529) == 0.0 and get(999) == 999999.0
        # the input is untouched, and an existing --out is refused
        with open(src, "rb") as f:
            assert struct.unpack_from("<f", zlib.decompress(f.read()), tool.CHALLENGES_AT + 4 * 312)[0] == 20.0
        try:
            tool.main(["--reports", tmp, "--uid", "77", "--blob", src, "--tables", tpath, "--out", out])
            raise AssertionError("an existing --out was overwritten")
        except SystemExit as e:
            assert e.code == 2
    print("PASS: repair_locked_marksman backfills gate-locked marksman kills into a new blob only")


if __name__ == "__main__":
    main()
