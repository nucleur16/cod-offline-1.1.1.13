"""Builds the wall-buy spec (JSON) for zm-wallbuy-patch from T5 entity strings and T5 chalk decal quads.

Inputs (scratchpad): t5/<t5map>.ents, t5/<c1map>.ents, mapx/<t5map>/<sel>/<sel>.cast + .map.json (T5 maps ported to
1.1.1.13 format with port-map and exported with CodolSoundDumper export-map --scope selection --select "*chalk*").
"""
import json, math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from castread import load as castload, walk
import numpy as np
from wallcheck import tris as world_tris, hit as ray_hit
from terminals import terminal
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def V(s): return [float(x) for x in s.split()]
def sub(a, b): return [a[i] - b[i] for i in range(3)]
def add(*vs): return [sum(v[i] for v in vs) for i in range(3)]
def mul(a, s): return [x * s for x in a]
def dot(a, b): return sum(a[i] * b[i] for i in range(3))
def cross(a, b): return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
def norm(a):
    l = math.sqrt(dot(a, a)); return [x / l for x in a]


def axis(ang):
    """Q3/IW AnglesToAxis: forward, left, up."""
    p, y, r = [math.radians(x) for x in ang]
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    f = [cp*cy, cp*sy, -sp]
    right = [-sr*sp*cy + cr*sy, -sr*sp*sy - cr*cy, -sr*cp]
    up = [cr*sp*cy + sr*sy, cr*sp*sy - sr*cy, cr*cp]
    return f, mul(right, -1), up


def angles_of(F, L, U):
    yaw = math.degrees(math.atan2(F[1], F[0])); pitch = math.degrees(math.atan2(-F[2], math.hypot(F[0], F[1])))
    best = None
    for i in range(3600):
        r = i / 10.0
        f, l, u = axis([pitch, yaw, r])
        e = 2 - dot(u, U) - dot(l, L)
        if best is None or e < best[0]: best = (e, r)
    lo = best[1] - 0.1
    for i in range(201):
        rr = lo + i * 0.001
        f, l, u = axis([pitch, yaw, rr]); e = 2 - dot(u, U) - dot(l, L)
        if e < best[0]: best = (e, rr)
    r = best[1]
    if r > 180: r -= 360
    if abs(r) < 0.0005: r = 0.0
    return [round(pitch, 4), round(yaw % 360, 4), round(r, 4)]


def ents(name):
    txt = open(os.path.join(HERE, 't5', name + '.ents'), encoding='latin1').read()
    return [dict(re.findall(r'"([^"]*)" "([^"]*)"', m.group(1))) for m in re.finditer(r'\{([^{}]*)\}', txt)]


def quads(t5map, sel):
    base = os.path.join(HERE, 'mapx', t5map, sel, sel)
    meta = json.load(open(base + '.map.json'))
    bm = {}
    for e in ents(t5map):
        if e.get('model', '').startswith('*') and 'origin' in e:
            bm[e['model']] = (V(e['origin']), V(e.get('angles', '0 0 0')), e.get('targetname'))
    out = []; meshes = []
    def first_mesh(n):
        if n['id'] == 'mesh': return n['props']
        for k in n['kids']:
            m = first_mesh(k)
            if m is not None: return m
        return None
    def fn(n, d):
        if n['id'] == 'modl': meshes.append(first_mesh(n))
    for r in castload(base + '.cast'): walk(r, 0, fn)
    assert len(meshes) == len(meta['objects']), (len(meshes), len(meta['objects']))
    for o, P in zip(meta['objects'], meshes):
        m = re.search(r'\*(\d+) /', o['path'])
        xf = bm.get('*' + m.group(1)) if ('Brush Entities' in o['path'] and m) else None
        pts = P['vp']; uv = P['u0']; F = P['f']
        if xf:
            f, l, u = axis(xf[1]); pts = [add(xf[0], mul(f, p[0]), mul(l, p[1]), mul(u, p[2])) for p in pts]
        i0, i1, i2 = F[0], F[1], F[2]
        p0, p1, p2 = pts[i0], pts[i1], pts[i2]; t0, t1, t2 = uv[i0], uv[i1], uv[i2]
        e1 = sub(p1, p0); e2 = sub(p2, p0); du1 = t1[0]-t0[0]; dv1 = t1[1]-t0[1]; du2 = t2[0]-t0[0]; dv2 = t2[1]-t0[1]
        det = du1*dv2 - du2*dv1
        if abs(det) < 1e-9: continue
        dPdu = mul(sub(mul(e1, dv2), mul(e2, dv1)), 1/det); dPdv = mul(sub(mul(e2, du1), mul(e1, du2)), 1/det)
        wind = norm(cross(sub(p1, p0), sub(p2, p0)))
        uv0 = sub(p0, add(mul(dPdu, t0[0]), mul(dPdv, t0[1])))
        cen = [sum(p[i] for p in pts) / len(pts) for i in range(3)]
        mn=[p[i] for i in range(3) for p in [min(pts,key=lambda p:p[i])]]; mx=[p[i] for i in range(3) for p in [max(pts,key=lambda p:p[i])]]
        out.append(dict(mat=o['asset'], path=o['path'], center=cen, dPdu=dPdu, dPdv=dPdv, uv0=uv0, wind=wind, layered=o['asset'].startswith('*'), mn=mn, mx=mx))
    return out


# C1 glow image sizes on CoDOL's own wall outlines (zm_prototype_sh / zm_asylum_sh BSP, MEASURED)
GLOW_SIZE = {'ithaca': (45, 16), 'm14': (45.5, 23), 'm16': (42, 20.5), 'mpl': (28.5, 14), 'pm63': (22.5, 13),
             'rottweil72': (54, 29), 'ak74u': (36, 18), 'mp40': (36.25, 19.25), 'mp5': (35, 17)}
# C1 wall weapon -> (glow image, world model, model offset (along the muzzle, out of the wall, up) from the outline centre).
# MEASURED from all 19 CoDOL wall buys of zm_prototype_sh / zm_asylum_sh: the model always points its muzzle at the
# outline's image left, and each weapon sits at a constant offset from the outline centre.
C1W = {
    'm14ebr_zm': ('m14', 'codo_weapon_m14ebr_world', (0.8, 0.0, 2.5)),
    'tmp_zm': ('pm63', 'codo_weapon_tmp_world', (8.8, 0.5, 5.5)),
    'ump45_zm': ('ak74u', 'codo_weapon_ump45_world', (10.0, 0.0, 3.0)),
    'm16_zm': ('m16', 'codo_weapon_m16_world', (3.5, 0.0, 2.2)),
    'uzi_zm': ('mp40', 'codo_weapon_miniuzi_world', (11.6, 1.5, 4.1)),
    'spas12_zm': ('ithaca', 'codo_weapon_spas12_world', (0.5, 0.5, 4.5)),
    'mp5k_zm': ('mp5', 'codo_weapon_mp5k_world', (10.5, 0.0, 2.0)),
    'ranger_zm': ('rottweil72', 'codo_weapon_ranger_world', (0.6, 0.0, 3.5)),
    'pp2000_zm': ('mpl', 'codo_weapon_pp2000_world', (9.8, 0.0, 4.0)),
}
MAPS = {
    'zm_theater_sh': dict(t5='zombie_theater', sel='t5sel', weapons={
        'ithaca_zm': 'spas12_zm', 'mp5k_zm': 'mp5k_zm', 'm14_zm': 'm14ebr_zm', 'mp40_zm': 'uzi_zm', 'm16_zm': 'm16_zm',
        'rottweil72_zm': 'ranger_zm', 'pm63_zm': 'tmp_zm', 'ak74u_zm': 'ump45_zm', 'mpl_zm': 'pp2000_zm',
        'frag_grenade_zm': 'frag_grenade_zm'}),
    'zm_factory_sh': dict(t5='zombie_cod5_factory', sel='t5sel', weapons={
        'zombie_m1carbine': 'ranger_zm', 'zombie_doublebarrel': 'tmp_zm', 'zombie_gewehr43': 'm14ebr_zm',
        'zombie_stg44': 'mp5k_zm', 'zombie_fg42': 'm16_zm', 'zombie_type100_smg': 'ump45_zm',
        'stielhandgranate': 'frag_grenade_zm', 'zombie_thompson': 'pp2000_zm', 'zombie_shotgun': 'spas12_zm',
        'mp40_zm': 'uzi_zm', 'zombie_kar98k': 'm14ebr_zm'}),
    'zm_sumpf_sh': dict(t5='zombie_cod5_sumpf', sel='t5sel', weapons={
        'zombie_m1garand': 'm14ebr_zm', 'zombie_type99_rifle': 'm14ebr_zm', 'zombie_gewehr43': 'm16_zm',
        'stielhandgranate': 'frag_grenade_zm', 'zombie_m1carbine': 'ranger_zm', 'zombie_shotgun': 'spas12_zm',
        'zombie_thompson': 'pp2000_zm', 'mp40_zm': 'uzi_zm', 'zombie_type100_smg': 'tmp_zm', 'zombie_stg44': 'mp5k_zm',
        'zombie_bar': 'ump45_zm'},
        skip={'12614 -1164 -585': 'CoDOL put a power switch (use_elec_switch, elec_switch lever) on this wall'}),
    'zm_asylum_sh': dict(t5='zombie_cod5_asylum', sel='t5sel', weapons={
        'stielhandgranate': 'frag_grenade_zm', 'zombie_doublebarrel': 'tmp_zm'}),
    'zm_prototype_sh': dict(t5='zombie_cod5_prototype', sel='t5sel', weapons={
        'zombie_bar': 'ump45_zm', 'stielhandgranate': 'frag_grenade_zm'},
        skip={'-132 -120 200': 'CoDOL put a power switch (use_elec_switch, elec_switch lever) on this wall'}),
}
SKIP_MATS = ('help_chalk', 'lightningbox', 'teleporter_chalk', 'unknown_chalk', 'guide_115', 'tung_power', 'banishes',
             'reunite', 'wishing', 'ascend', 'salvation', 'bowie', 'bouncing_betty', 'theater_chalkmap', 'zombie_chalkboard')


def main(out):
    spec = {'maps': []}; report = []
    for c1, cfg in MAPS.items():
        T5 = ents(cfg['t5']); C1 = ents(c1)
        c1walls = [V(e['origin']) for e in C1 if e.get('targetname') == 'weapon_upgrade']
        Q = [q for q in quads(cfg['t5'], cfg['sel']) if 'chalk' in q['mat'].lower() and not any(k in q['mat'] for k in SKIP_MATS)]
        world = world_tris(os.path.join(HERE, 'mapx', c1, c1 + '_world', c1 + '_world.cast'))
        def wall(center, F):
            # the C1 wall along -F from 40 units out: its offset from `center` (median of 9 rays), or None
            F = np.array(F); L = np.array([-F[1], F[0], 0.0]); hs = []
            for dz in (-6, 0, 6):
                for dl in (-8, 0, 8):
                    t = ray_hit(world, np.array(center) + F * 40 + np.array([0, 0, dz]) + L * dl, -F)
                    if t is not None: hs.append(40 - t)
            if not hs: return None
            # the frontmost surface (a brace or plank across the wall would hide the outline) unless it stands well
            # proud of the wall itself
            med, top = float(np.median(hs)), float(max(hs))
            return top if top - med <= 4 else med
        buys = []; n = 0
        for e in T5:
            if e.get('targetname') != 'weapon_upgrade': continue
            t5w = e['zombie_weapon_upgrade']; o = V(e['origin'])
            if any(math.dist(o, w) < 80 for w in c1walls) or t5w not in cfg['weapons']: continue
            if e['origin'] in cfg.get('skip', {}):
                report.append(f'{c1}: SKIP {t5w} at {e["origin"]}: {cfg["skip"][e["origin"]]}'); continue
            c1w = cfg['weapons'][t5w]
            q = min(Q, key=lambda q: math.dist(q['center'], o))
            d = math.dist(q['center'], o)
            if d > 40: raise SystemExit(f'{c1} {t5w} {o}: nearest chalk {q["mat"]} at {d:.1f}')
            Fw = q['wind']; Fw = norm([Fw[0], Fw[1], 0])
            if dot(sub(o, q['center']), Fw) < 0: Fw = mul(Fw, -1)  # the side the T5 trigger is on
            if q['layered']:
                # a chalk layer of a blended wall material: no decal UV frame; the union of the layer's surfaces
                key = q['mat'].split(':')[-1]
                group = [g for g in Q if g['layered'] and key in g['mat'] and math.dist(g['center'], q['center']) < 40]
                mn = [min(g['mn'][i] for g in group) for i in range(3)]; mx = [max(g['mx'][i] for g in group) for i in range(3)]
                q = dict(q, center=[(mn[i] + mx[i]) / 2 for i in range(3)])
            # the visible side and the wall itself in CoDOL's geometry (the trigger lies on the chalk, so its side can be
            # ambiguous, and CoDOL moved some walls): the side whose wall is nearest, and the outline snapped onto it
            wa, wb = wall(q['center'], Fw), wall(q['center'], mul(Fw, -1))
            if wb is not None and (wa is None or abs(wb) < abs(wa)):
                Fw = mul(Fw, -1); wa = wb
            moved_to = None
            if wa is None or abs(wa) > 16:
                # CoDOL rebuilt this spot (factory's grenade corner: T5's diagonal wall is now a square corner): the
                # nearest C1 wall in any of 16 horizontal directions from the chalk centre takes the outline
                best = None
                for k in range(16):
                    a = math.radians(22.5 * k); dv = np.array([math.cos(a), math.sin(a), 0.0])
                    t = ray_hit(world, np.array(q['center']), dv)
                    if t is not None and (best is None or t < best[0]): best = (t, dv)
                if best is None or best[0] > 48:
                    raise SystemExit(f'{c1} {t5w} {o}: no C1 wall near the T5 chalk ({wa})')
                Fn = [-float(x) for x in best[1]]
                # keep the drawing's in-plane tilt
                Lold = norm(q['dPdu']); phi = math.atan2(Lold[2], dot(Lold, norm(cross([0, 0, 1], Fw))))
                L0 = norm(cross([0, 0, 1], Fn)); Lnew = add(mul(L0, math.cos(phi)), mul([0, 0, 1], math.sin(phi)))
                Unew = norm(cross(Fn, Lnew)); W_ = math.sqrt(dot(q['dPdu'], q['dPdu'])); H_ = math.sqrt(dot(q['dPdv'], q['dPdv']))
                newc = add(q['center'], mul(best[1], best[0]))
                q = dict(q, center=newc, dPdu=mul(Lnew, W_), dPdv=mul(Unew, -H_),
                         uv0=sub(newc, add(mul(Lnew, W_ / 2), mul(Unew, -H_ / 2))), wind=Fn)
                Fw = Fn; wa = 0.0; moved_to = [round(x, 1) for x in newc]
                report.append(f'{c1}: {t5w} at {e["origin"]}: T5 chalk wall gone in CoDOL, outline moved to the C1 wall at {moved_to}')
            shift = wa + 1.0  # T5's decals float 1 unit off the wall surface they were measured on; keep the quad there
            q = dict(q, center=add(q['center'], mul(Fw, shift)), uv0=add(q['uv0'], mul(Fw, shift)))
            b_shift = round(shift, 2)
            n += 1; bid = f'{c1[3:-3]}{n}'
            b = dict(id=bid, wall_shift=b_shift, t5_weapon=t5w, weapon=c1w, t5_trigger=o, t5_chalk=q['mat'], t5_chalk_path=q['path'])
            up = [0, 0, 1]
            if c1w == 'frag_grenade_zm' and q['layered']:
                F = Fw; L = norm(cross(up, F)); Up = up
                img = 'chalk_weapon_explosives_c' if 'explosives' in q['mat'] else 'grenade_bag_chalk_c'
                chalk = dict(image=img, half=[12.0, 12.0], atlas=img == 'chalk_weapon_explosives_c',
                             origin=[round(x, 3) for x in add(q['center'], mul(F, 0.5))], angles=angles_of(F, L, Up))
                yawF = math.degrees(math.atan2(F[1], F[0]))
                model = dict(model='grenade_bag', origin=[round(x, 2) for x in add(q['center'], mul(Fw, 4.5), [0, 0, -0.5])],
                             angles=[0, round((yawF + 90) % 360, 3), 0])
            elif c1w == 'frag_grenade_zm':
                # T5's own chalk as T5 draws it (the quad's affine UV frame, the whole image: additive, so black is
                # invisible); Kino's explosives image through a 2x1 atlas, right half (T5's quad uses u 0.47..0.99)
                U = q['dPdu']; Vd = q['dPdv']; L = norm(U); Up = mul(norm(Vd), -1)
                if dot(cross(L, Up), Fw) < 0: L = mul(L, -1)  # mirrored in T5: drawn unmirrored
                img = 'chalk_weapon_explosives_c' if 'explosives' in q['mat'] else 'grenade_bag_chalk_c'
                W = math.sqrt(dot(U, U)); H = math.sqrt(dot(Vd, Vd))
                if img == 'chalk_weapon_explosives_c':
                    cen = add(q['uv0'], mul(U, 0.75), mul(Vd, 0.5)); half = (W / 4, H / 2); atlas = True
                else:
                    cen = add(q['uv0'], mul(U, 0.5), mul(Vd, 0.5)); half = (W / 2, H / 2); atlas = False
                F = norm(cross(L, Up))
                chalk = dict(image=img, half=[round(half[0], 3), round(half[1], 3)], atlas=atlas,
                             origin=[round(x, 3) for x in add(cen, mul(F, 0.5))], angles=angles_of(F, L, Up))
                yawF = math.degrees(math.atan2(F[1], F[0]))
                # T5 Kino hangs grenade_bag 4.5 out from its chalk, yawed 90 from the wall normal
                model = dict(model='grenade_bag', origin=[round(x, 2) for x in add(q['center'], mul(Fw, 4.5), [0, 0, -0.5])],
                             angles=[0, round((yawF + 90) % 360, 3), 0])
            else:
                g, wmodel, off = C1W[c1w]
                F = Fw; L = norm(cross(up, F)); Up = up
                cen = q['center']; wsz = GLOW_SIZE[g]
                chalk = dict(image=f'glow_weapon_{g}_c', half=[wsz[0] / 2, wsz[1] / 2], atlas=False,
                             origin=[round(x, 3) for x in add(cen, mul(F, 0.5))], angles=angles_of(F, L, Up))
                a = mul(L, -1)  # the muzzle: the outline image's left
                mo = add(cen, mul(a, off[0]), mul(F, off[1]), mul(up, off[2]))
                model = dict(model=wmodel, origin=[round(x, 2) for x in mo],
                             angles=[0, round(math.degrees(math.atan2(a[1], a[0])) % 360, 3), 0])
            b['chalk'] = chalk; b['model'] = model
            b['use_origin'] = [round(x, 2) for x in add(q['center'], mul(Fw, 2))]
            b['glow'] = dict(origin=[round(x, 3) for x in add(q['center'], mul(Fw, 1.0))],
                             angles=angles_of(Fw, norm(cross(up, Fw)), up))
            b['terminal'] = terminal(b)  # CoDOL's weapon case, as on its own walls (terminals.py)
            buys.append(b)
            report.append(f'{c1}: {bid} {t5w} -> {c1w} use {b["use_origin"]} chalk {chalk["image"]} '
                          f'(T5 {q["mat"]}, {d:.1f} from trigger, moved {b_shift} onto the C1 wall) model {model["model"]} {model["origin"]} {model["angles"]}')
        spec['maps'].append(dict(map=c1, buys=buys))
    json.dump(spec, open(out, 'w'), indent=1)
    print('\n'.join(report))


main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'wallbuys.json'))
