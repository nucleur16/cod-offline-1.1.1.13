"""Ray-casts each spec wall buy against a map's exported world (export-map --scope world --layout merged): from 40 units
out along the outline's facing back toward the wall, the first world triangle hit is the wall the outline must sit on."""
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from castread import load, walk
import numpy as np

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def tris(cast):
    out = []
    def fn(n, d):
        if n['id'] == 'mesh':
            P = np.array(n['props']['vp'], dtype=np.float64); F = np.array(n['props']['f'], dtype=np.int64).reshape(-1, 3)
            out.append(P[F])
    for r in load(cast): walk(r, 0, fn)
    return np.concatenate(out)


def hit(T, o, d):
    v0, v1, v2 = T[:, 0], T[:, 1], T[:, 2]
    e1 = v1 - v0; e2 = v2 - v0
    p = np.cross(d, e2); det = (e1 * p).sum(1)
    ok = np.abs(det) > 1e-9
    inv = np.where(ok, 1 / np.where(ok, det, 1), 0)
    s = o - v0; u = (s * p).sum(1) * inv
    q = np.cross(s, e1); v = (q * d).sum(1) * inv; t = (e2 * q).sum(1) * inv
    m = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0)
    return t[m].min() if m.any() else None


def main(spec, mapname, cast):
    T = tris(cast)
    s = json.load(open(spec))
    for m in s['maps']:
        if m['map'] != mapname: continue
        for b in m['buys']:
            yaw = math.radians(b['glow']['angles'][1]); F = np.array([math.cos(yaw), math.sin(yaw), 0.0])
            c = np.array(b['chalk']['origin'])
            res = []
            for dz in (-6, 0, 6):
                for dl in (-8, 0, 8):
                    L = np.array([-F[1], F[0], 0])
                    o = c + F * 40 + np.array([0, 0, dz]) + L * dl
                    t = hit(T, o, -F)
                    res.append(None if t is None else round(40 - t, 2))
            print(b['id'], b['weapon'], 'wall offset from outline (+ = in front of it):', res)


if __name__ == "__main__":
    main(*sys.argv[1:4])
