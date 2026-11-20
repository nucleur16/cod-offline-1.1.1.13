"""The wall-buy terminal: CoDOL mounts a codo_weapon_box (a weapon case) on every one of its own wall buys
(zm_prototype_sh 6, zm_asylum_sh 13 static models, one per wall, none elsewhere). MEASURED (zm_perkclip_staging
tool_src/terminal WallBox + ray casts against the exported C1 worlds):
  - its yaw is the wall weapon's (the muzzle): its +y axis points out of the wall;
  - its origin is 4.3 units out of the wall surface (median of 19; 2.8..6.3), so the case's back (model y -6.25)
    sits 2 units in the wall and its front (y +12.35) 16.6 out;
  - from CoDOL's outline centre (gen_wallbuys.C1W offsets): +2.2 along the muzzle, 17.3 below (medians of 19).
A buy's spec chalk origin is the outline centre 1.5 out of the wall (gen_wallbuys: the T5 decal +1, the quad +0.5),
so the case goes 2.8 further out. Usage: python terminals.py <wallbuys.json> (adds/replaces "terminal" per buy)."""
import json, math, sys

MODEL = 'codo_weapon_box'
ALONG, OUT_FROM_CHALK, UP = 2.2, 2.8, -17.3


def terminal(b):
    yaw = math.radians(b['chalk']['angles'][1])
    F = [math.cos(yaw), math.sin(yaw), 0.0]          # out of the wall (the outline faces it)
    L = [-F[1], F[0], 0.0]                            # cross(up, F)
    a = [-L[0], -L[1], 0.0]                           # the muzzle: the outline image's left
    c = b['chalk']['origin']
    o = [c[i] + a[i] * ALONG + F[i] * OUT_FROM_CHALK for i in range(3)]
    o[2] += UP
    return dict(model=MODEL, origin=[round(x, 2) for x in o],
                angles=[0, round(math.degrees(math.atan2(a[1], a[0])) % 360, 3), 0])


if __name__ == '__main__':
    path = sys.argv[1]
    spec = json.load(open(path))
    for m in spec['maps']:
        for b in m['buys']:
            b['terminal'] = terminal(b)
        print(f"{m['map']}: {len(m['buys'])} walls, {len(m['buys'])} terminals")
    json.dump(spec, open(path, 'w'), indent=1)
