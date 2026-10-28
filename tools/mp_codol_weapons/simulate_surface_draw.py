# -*- coding: utf-8 -*-
"""simulate_surface_draw.py -- draw a surface set the way C1 lays out inline zone surfaces: one vertex buffer of
sum(vertCount) vertices per XModelSurfs record, each surface's vertices copied in at its baseVertIndex (stock inline
surfaces even share vertex data between records, so the offsets, not the zone layout, place them), triangles drawn
from baseVertIndex. Stale offsets push a surface's tail past the buffer.

    python simulate_surface_draw.py out.png before.json after.json [muzzle part xmodel_export]"""
import json
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.collections import PolyCollection  # noqa: E402
import numpy as np  # noqa: E402


def view(p):
    # a first-person-like 3/4 view from behind and above the right side
    yaw, pitch = np.radians(25), np.radians(18)
    r = np.array([[np.cos(yaw), np.sin(yaw), 0], [-np.sin(yaw), np.cos(yaw), 0], [0, 0, 1]])
    q = p @ r.T
    c, s = np.cos(pitch), np.sin(pitch)
    return np.stack([q[..., 0] * c + q[..., 2] * s, q[..., 2] * c - q[..., 0] * s, q[..., 1]], -1)


def draw(ax, path, title):
    surfs = sorted(json.load(open(path)), key=lambda s: s["k"])
    total = sum(len(s["verts"]) // 3 for s in surfs)
    vb = np.full((total, 3), np.nan)
    for s in surfs:
        v = np.array(s["verts"]).reshape(-1, 3)
        end = min(total, s["baseVert"] + len(v))
        if s["baseVert"] < total:
            vb[s["baseVert"]:end] = v[:end - s["baseVert"]]
    polys, cols, lost = [], [], 0
    for s in surfs:
        t = np.array(s["tris"]).reshape(-1, 3) + s["baseVert"]
        for tri in t:
            if tri.max() >= total or np.isnan(vb[tri]).any():
                lost += 1
                continue
            polys.append(vb[tri])
            cols.append(s["k"])
    polys = np.array(polys)
    pv = view(polys)
    order = np.argsort(pv[:, :, 2].mean(1))
    nrm = np.cross(polys[:, 1] - polys[:, 0], polys[:, 2] - polys[:, 0])
    nrm /= np.maximum(np.linalg.norm(nrm, axis=1)[:, None], 1e-9)
    shade = 0.35 + 0.65 * np.abs(nrm @ np.array([0.3, -0.6, 0.7]) / np.linalg.norm([0.3, -0.6, 0.7]))
    colors = [(0.25 * shade[i] + 0.1 * cols[i], 0.3 * shade[i], 0.35 * shade[i] + 0.3) for i in order]
    ax.add_collection(PolyCollection(pv[order][:, :, :2], facecolors=colors, edgecolors="none"))
    allv = pv[:, :, :2].reshape(-1, 2)
    ax.set_xlim(allv[:, 0].min() - 1, allv[:, 0].max() + 1)
    ax.set_ylim(allv[:, 1].min() - 1, allv[:, 1].max() + 1)
    ax.set_aspect("equal")
    ax.set_title("%s: %d triangles drawn, %d lost" % (title, len(polys), lost), fontsize=9)


fig, axs = plt.subplots(2, 1, figsize=(12, 7))
draw(axs[0], sys.argv[2], "installed zone (stale base offsets 1244/1392)")
draw(axs[1], sys.argv[3], "fixed zone (offsets from 0, the stock layout)")
fig.tight_layout()
fig.savefig(sys.argv[1], dpi=110)
