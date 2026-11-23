r"""Folds the lobby camera editor's saved cameras (main\vlobby_cameras.txt, src/vlobby_camui.c) into the merge's camera
table and framing file, so a zone built from them shows those cameras without the file.

    python fold_cameras.py <vlobby_cameras.txt> [camerapostable_vlobby.csv] [framing_vlobby.txt]

The two outputs default to the files next to this script and are rewritten in place (run it after camera_rows.py,
which writes them anew). A line of the saved file is

    <row> [cam <x> <y> <z> <pitch> <yaw>] [dist <d>] [fov <f>] [stand <x> <y> <z> <yaw>] [dof <near> <far> <blur>]

and each group goes where C1 reads it from:
- cam: a new script_origin k33cam_<row> at that origin and angles (a "place" line of the framing file, Framing.cs),
  named by the row's camera column (3); a new entity, because rows share cameras (weapon_camera, player_camera_urb).
- stand: likewise k33stand_<row> (angles 0 <yaw> 0) in the stand column (2).
- dist, fov, dof: the row's columns 4, 13 and 19-21.

A room row's (team_*) camera can be aimed by C1 at a room character instead of standing at its entity (the editor
shows "aimed"); its saved camera is where the view was, so baked it frames that room alike only when C1 does not aim.
Rows the table does not have are reported and left out. Running it again with the same file changes nothing.
"""
import csv
import os
import sys

GROUPS = {"cam": 5, "dist": 1, "fov": 1, "stand": 4, "dof": 3}
COL_STAND, COL_CAMERA, COL_DIST, COL_FOV, COL_DOF = 2, 3, 4, 13, 19


def num(v):
    s = ("%.3f" % float(v)).rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def read_cameras(path):
    out = {}
    with open(path, encoding="latin-1") as f:
        for n, raw in enumerate(f, 1):
            tok = raw.split()
            if not tok or tok[0].startswith("#"):
                continue
            row, groups, i = tok[0], {}, 1
            while i < len(tok):
                g = tok[i].lower()
                if g not in GROUPS or i + GROUPS[g] >= len(tok):
                    sys.exit("%s:%d: \"%s\" is not a group with its values (%s)" % (path, n, tok[i], " ".join(GROUPS)))
                groups[g] = [float(v) for v in tok[i + 1:i + 1 + GROUPS[g]]]
                i += 1 + GROUPS[g]
            out[row] = groups
    return out


def fold(cameras, rows, framing):
    """cameras: {row: {group: values}}; rows: the table (lists, changed in place); framing: the framing file's lines.
    Returns the new framing lines and a log."""
    by = {r[0]: r for r in rows if r}
    places, log = {}, []
    for name, groups in sorted(cameras.items()):
        r = by.get(name)
        if r is None:
            log.append("%s: not a row of the table; left out" % name)
            continue
        while len(r) < COL_DOF + 3:
            r.append("")
        if "cam" in groups:
            x, y, z, pitch, yaw = groups["cam"]
            ent = "k33cam_" + name
            places[ent] = "place %s %s %s %s %s %s 0" % (ent, num(x), num(y), num(z), num(pitch), num(yaw))
            r[COL_CAMERA] = ent
        if "stand" in groups:
            x, y, z, yaw = groups["stand"]
            ent = "k33stand_" + name
            places[ent] = "place %s %s %s %s 0 %s 0" % (ent, num(x), num(y), num(z), num(yaw))
            r[COL_STAND] = ent
        if "dist" in groups:
            r[COL_DIST] = num(groups["dist"][0])
        if "fov" in groups:
            r[COL_FOV] = num(groups["fov"][0])
        if "dof" in groups:
            for k in range(3):
                r[COL_DOF + k] = num(groups["dof"][k])
        log.append("%s: %s" % (name, " ".join(sorted(groups))))
    # a framing camera no row names any more (its row has a saved cam) goes; one whose row kept its camera stays, framed
    # from the scene's own stand as the editor showed it even when the row's stand moved (vlobby_merge keeps that stand)
    named = {r[COL_CAMERA] for r in rows if len(r) > COL_CAMERA}
    kept = []
    for l in framing:
        t = l.split()
        if t[:1] == ["place"] and len(t) > 1 and t[1] in places:
            continue
        if len(t) >= 4 and not t[0].startswith("#") and t[0] != "place" and t[0] not in named:
            log.append("%s: framing camera no row names; left out" % t[0])
            continue
        kept.append(l)
    while kept and not kept[-1].strip():
        kept.pop()
    return kept + [places[e] for e in sorted(places)], log


def main():
    if len(sys.argv) < 2 or len(sys.argv) > 4:
        sys.exit(__doc__)
    here = os.path.dirname(os.path.abspath(__file__))
    cams = sys.argv[1]
    table = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "camerapostable_vlobby.csv")
    frame = sys.argv[3] if len(sys.argv) > 3 else os.path.join(here, "framing_vlobby.txt")
    with open(table, newline="", encoding="latin-1") as f:
        rows = [r for r in csv.reader(f)]
    with open(frame, newline="", encoding="latin-1") as f:
        framing = f.read().split("\n")
    lines, log = fold(read_cameras(cams), rows, framing)
    width = max(len(r) for r in rows)
    with open(table, "w", newline="", encoding="latin-1") as f:
        csv.writer(f, lineterminator="\n").writerows(rows)
    with open(frame, "w", newline="\n", encoding="latin-1") as f:
        f.write("\n".join(lines) + "\n")
    for l in log:
        print(l)
    print("%s: %d rows (widest %d columns); %s: %d lines" % (table, len(rows), width, frame, len(lines)))


if __name__ == "__main__":
    main()
