"""Pure full resolved ABC pool/ABI identities; no compiler/runtime."""
import abc_reader as A
def ns(x, i):
    if not i:
        return None
    kind, s = x['values']['namespace'][i]
    return (kind, x['values']['string'][s], i) if kind == 5 else (kind, x['values']['string'][s])

def mn(x, i):
    if not i:
        return '*'
    p = x['values']
    v = p['multiname'][i]
    k = v[0]
    if k in (7, 13):
        return (k, ns(x, v[1]), p['string'][v[2]])
    if k in (9, 14):
        return (k, p['string'][v[1]], tuple((ns(x, n) for n in p['nsset'][v[2]])))
    if k in (15, 16):
        return (k, p['string'][v[1]])
    if k in (17, 18):
        return (k,)
    if k in (27, 28):
        return (k, tuple((ns(x, n) for n in p['nsset'][v[1]])))
    if k == 29:
        return (k, mn(x, v[1]), tuple((mn(x, n) for n in v[2])))
    raise ValueError(v)

def tables(x):
    r = A.Reader(x['tables'])
    p = x['values']
    methods = []

    def constant(i, k):
        return p[{1: 'string', 3: 'int', 4: 'uint', 6: 'double'}[k]][i] if k in (1, 3, 4, 6) else ns(x, i) if k in (5, 8, 22, 23, 24, 25, 26) else (i, k)
    for _ in range(r.u30()):
        c = r.u30()
        ret = mn(x, r.u30())
        params = [mn(x, r.u30()) for _ in range(c)]
        label = p['string'][r.u30()]
        flags = r.u8()
        opts = []
        names = []
        if flags & 8:
            for _ in range(r.u30()):
                i, k = (r.u30(), r.u8())
                opts.append((k, constant(i, k)))
        if flags & 128:
            names = [p['string'][r.u30()] for _ in range(c)]
        methods.append(dict(returns=ret, args=params, name=label, flags=flags, options=opts, paramnames=names))
    metadata = []
    for _ in range(r.u30()):
        label = p['string'][r.u30()]
        c = r.u30()
        keys = [p['string'][r.u30()] for _ in range(c)]
        vals = [p['string'][r.u30()] for _ in range(c)]
        metadata.append((label, list(zip(keys, vals))))

    def traits():
        rows = []
        for _ in range(r.u30()):
            name = mn(x, r.u30())
            kind = r.u8()
            role = kind & 15
            slot = r.u30()
            value = r.u30()
            entry = dict(name=name, kind=kind, slot=slot)
            if role in (0, 6):
                entry['type'] = mn(x, value)
                d = r.u30()
                entry['default'] = constant(d, r.u8()) if d else None
            elif role in (1, 2, 3, 5):
                entry['method'] = value
            elif role == 4:
                entry['class'] = value
            else:
                raise ValueError(role)
            if kind & 64:
                entry['metadata'] = [r.u30() for _ in range(r.u30())]
            rows.append(entry)
        return rows
    c = r.u30()
    classes = []
    for _ in range(c):
        name = mn(x, r.u30())
        superclass = mn(x, r.u30())
        flags = r.u8()
        protected = ns(x, r.u30()) if flags & 8 else None
        interfaces = [mn(x, r.u30()) for _ in range(r.u30())]
        init = r.u30()
        classes.append(dict(name=name, superclass=superclass, flags=flags, protected=protected, interfaces=interfaces, initializer=init, traits=traits()))
    static = [dict(initializer=r.u30(), traits=traits()) for _ in range(c)]
    scripts = [dict(initializer=r.u30(), traits=traits()) for _ in range(r.u30())]
    assert r.pos == len(x['tables'])
    return dict(methods=methods, metadata=metadata, classes=classes, static=static, scripts=scripts)
