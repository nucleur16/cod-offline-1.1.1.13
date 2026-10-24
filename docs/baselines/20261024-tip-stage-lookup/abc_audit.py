"""Independent bounded ABC reader for preservation and compiled-method tests."""
import hashlib
import struct


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, count):
        out = self.data[self.pos:self.pos + count]
        if len(out) != count:
            raise ValueError('truncated ABC')
        self.pos += count
        return out

    def u8(self):
        return self.take(1)[0]

    def u30(self):
        value = 0
        for shift in range(0, 35, 7):
            byte = self.u8()
            value |= (byte & 127) << shift
            if not byte & 128:
                return value
        raise ValueError('invalid ABC variable integer')


def read(data):
    r = Reader(data)
    minor, major = struct.unpack('<HH', r.take(4))
    if major != 46:
        raise ValueError('unreviewed ABC version')
    pools, values = {}, {}

    def pool(name, fn, default=None):
        raw, parsed = [], [default]
        for _ in range(max(0, r.u30() - 1)):
            start = r.pos
            parsed.append(fn())
            raw.append(data[start:r.pos])
        pools[name], values[name] = raw, parsed

    pool('int', r.u30, 0)
    pool('uint', r.u30, 0)
    pool('double', lambda: struct.unpack('<d', r.take(8))[0], 0.0)
    pool('string', lambda: r.take(r.u30()).decode('utf-8'), '')
    pool('namespace', lambda: (r.u8(), r.u30()))
    pool('nsset', lambda: [r.u30() for _ in range(r.u30())])

    def multiname():
        kind = r.u8()
        if kind in (7, 13, 9, 14):
            return (kind, r.u30(), r.u30())
        if kind in (15, 16, 27, 28):
            return (kind, r.u30())
        if kind in (17, 18):
            return (kind,)
        if kind == 29:
            return (kind, r.u30(), [r.u30() for _ in range(r.u30())])
        raise ValueError('unreviewed multiname kind ' + str(kind))

    pool('multiname', multiname)
    table_start = r.pos
    methods = []
    for _ in range(r.u30()):
        params = r.u30()
        r.u30()  # return type
        for _ in range(params):
            r.u30()
        name, flags = r.u30(), r.u8()
        if flags & 8:
            for _ in range(r.u30()):
                r.u30()
                r.u8()
        if flags & 128:
            for _ in range(params):
                r.u30()
        methods.append(values['string'][name])
    for _ in range(r.u30()):
        r.u30()
        count = r.u30()
        for _ in range(count * 2):
            r.u30()

    def traits():
        for _ in range(r.u30()):
            r.u30()
            kind = r.u8()
            role = kind & 15
            r.u30()  # slot/dispatch ID
            r.u30()  # type/method/class/function
            if role in (0, 6):
                if r.u30():
                    r.u8()
            elif role not in (1, 2, 3, 4, 5):
                raise ValueError('unreviewed trait kind')
            if kind & 64:
                for _ in range(r.u30()):
                    r.u30()

    classes = r.u30()
    for _ in range(classes):
        r.u30()
        r.u30()
        if r.u8() & 8:
            r.u30()
        for _ in range(r.u30()):
            r.u30()
        r.u30()
        traits()
    for _ in range(classes):
        r.u30()
        traits()
    for _ in range(r.u30()):
        r.u30()
        traits()
    tables = data[table_start:r.pos]
    bodies = {}
    for _ in range(r.u30()):
        start = r.pos
        method = r.u30()
        stack, regs, init_scope, max_scope = [r.u30() for _ in range(4)]
        code = r.take(r.u30())
        exceptions = r.u30()
        for _ in range(exceptions * 5):
            r.u30()
        traits()
        bodies[method] = {'name': methods[method], 'code': code, 'raw': data[start:r.pos],
                          'max_stack': stack, 'regs': regs, 'init_scope': init_scope,
                          'max_scope': max_scope, 'exceptions': exceptions}
    if r.pos != len(data):
        raise ValueError('unconsumed ABC data')
    return {'version': (minor, major), 'pools': pools, 'values': values,
            'tables': tables, 'bodies': bodies}


def method(abc, name):
    matches = [body for body in abc['bodies'].values()
               if body['name'].endswith(':' + name) or body['name'].endswith('/' + name)]
    if len(matches) != 1:
        raise ValueError('method identity is ambiguous: ' + name)
    return matches[0]


def compare(before, after, allowed):
    old, new = read(before), read(after)
    if old['version'] != new['version'] or old['tables'] != new['tables']:
        raise ValueError('ABC method signatures/traits/class/script metadata changed')
    for name, pool in old['pools'].items():
        if pool != new['pools'][name][:len(pool)]:
            raise ValueError('existing constant-pool entry changed: ' + name)
    if old['bodies'].keys() != new['bodies'].keys():
        raise ValueError('method body inventory changed')
    expected = {method(old, name)['name'] for name in allowed}
    changed = []
    for index, body in old['bodies'].items():
        other = new['bodies'][index]
        if body['raw'] != other['raw']:
            changed.append(body['name'])
            if body['name'] not in expected:
                raise ValueError('unapproved method changed: ' + body['name'])
    if set(changed) != expected:
        raise ValueError('approved method did not change')
    return {'changed_methods': changed, 'unchanged_method_bodies': len(old['bodies']) - len(changed),
            'signatures_traits_scripts_identical': True, 'existing_constant_pool_entries_identical': True,
            'before_abc_sha256': hashlib.sha256(before).hexdigest(),
            'after_abc_sha256': hashlib.sha256(after).hexdigest()}


def qname(abc, index):
    value = abc['values']['multiname'][index]
    if value[0] in (7, 13):
        return abc['values']['string'][value[2]]
    if value[0] in (9, 14, 15, 16):
        return abc['values']['string'][value[1]]
    raise ValueError('runtime multiname unsupported')
