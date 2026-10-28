# -*- coding: utf-8 -*-
"""render_xmodel_export.py -- flat-shaded side/top views of xmodel_export files (CodolSoundDumper export-models), to
compare a composed gun (base + bone-merged parts) with the zombies M14 it is cut from.

    python render_xmodel_export.py out.png a.xmodel_export [b.xmodel_export ...]
"""
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.collections import PolyCollection  # noqa: E402
import numpy as np  # noqa: E402


def load(path):
    verts, tris, mats = [], [], []
    lines = open(path, encoding="latin-1").read().split("\n")
    i = 0
    n = len(lines)
    while i < n:
        s = lines[i].strip()
        if s.startswith("NUMVERTS"):
            count = int(s.split()[1])
            i += 1
            while len(verts) < count and i < n:
                t = lines[i].strip()
                if t.startswith("OFFSET"):
                    verts.append([float(x) for x in t.split()[1:4]])
                i += 1
            continue
        if s.startswith("TRI "):
            obj = int(s.split()[1])
            mat = int(s.split()[2])
            face = []
            i += 1
            while len(face) < 3 and i < n:
                t = lines[i].strip()
                if t.startswith("VERT "):
                    face.append(int(t.split()[1]))
                i += 1
            tris.append(face)
            mats.append(mat)
            continue
        i += 1
    return np.array(verts), np.array(tris), np.array(mats)


def draw(ax, v, t, m, axes, title):
    a, b = axes
    light = np.array([0.3, -0.8, 0.5])
    light /= np.linalg.norm(light)
    p = v[t]
    nrm = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    ln = np.linalg.norm(nrm, axis=1)
    ln[ln == 0] = 1
    shade = np.abs((nrm / ln[:, None]) @ light)
    depth = p[:, :, 3 - a - b].mean(axis=1)
    order = np.argsort(depth if (a, b) != (0, 1) else -depth)
    cmap = plt.get_cmap("tab10")
    colors = [tuple(np.array(cmap(int(m[k]) % 10)[:3]) * (0.35 + 0.65 * shade[k])) for k in order]
    polys = p[order][:, :, [a, b]]
    ax.add_collection(PolyCollection(polys, facecolors=colors, edgecolors="none"))
    ax.set_xlim(v[:, a].min() - 1, v[:, a].max() + 1)
    ax.set_ylim(v[:, b].min() - 1, v[:, b].max() + 1)
    ax.set_aspect("equal")
    ax.set_title(title, fontsize=8)


def main():
    out = sys.argv[1]
    files = sys.argv[2:]
    fig, axs = plt.subplots(len(files), 2, figsize=(14, 3.2 * len(files)))
    if len(files) == 1:
        axs = [axs]
    for row, f in zip(axs, files):
        v, t, m = load(f)
        name = f.replace("\\", "/").split("/")[-1]
        draw(row[0], v, t, m, (0, 2), "%s  side (x/z)  %d verts %d tris" % (name, len(v), len(t)))
        draw(row[1], v, t, m, (0, 1), "top (x/y)")
    fig.tight_layout()
    fig.savefig(out, dpi=110)


if __name__ == "__main__":
    main()
