"""Generate c1_dvars.h: readable names for the numeric dvars of C1.exe (CoDOL 1.1.1.13).

C1 registers almost every dvar under a decimal string ("591" is cg_drawFPS). The IDs follow IW4's
registration order, so the names come back by aligning C1's registrations with IW4's:
  1. every call to a Dvar_Register* wrapper in C1.exe and in iw4x.exe is decoded by simulating the
     argument pushes (name, type, default, domain, flags, description);
  2. C1's registration functions are matched with IW4's (the iw4x.pdb symbols give IW4's function
     names; cdb dumps them) by the longest common subsequence of type/default/domain, and each
     numeric name takes the IW4 name it lines up with;
  3. names from alpha0022's docs/dvars_c1exe.md and the hand-checked notes below take precedence.

    python Kernel33/gen_dvars.py      (needs C1.exe, the alpha0022 docs, iw4x.exe + iw4x.pdb, cdb)
"""
import bisect
import collections
import os
import re
import struct
import subprocess

import capstone
import pefile
TYPES = ['bool', 'float', 'vec2', 'vec3', 'vec4', 'int', 'enum', 'string', 'color', 'float3color']


class Image:
    def __init__(self, path):
        pe = pefile.PE(path, fast_load=True)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.img = pe.get_memory_mapped_image()
        self.secs = [(s.Name.rstrip(b'\0').decode('latin1'), self.base + s.VirtualAddress, s.Misc_VirtualSize)
                     for s in pe.sections]

    def ok(self, va, n=1):
        o = va - self.base
        return 0 <= o and o + n <= len(self.img)

    def read(self, va, n):
        o = va - self.base
        return self.img[o:o + n]

    def u32(self, va):
        return struct.unpack('<I', self.read(va, 4))[0] if self.ok(va, 4) else None

    def f32(self, va):
        return struct.unpack('<f', self.read(va, 4))[0] if self.ok(va, 4) else None

    def cstr(self, va, maxlen=512):
        if va is None or not isinstance(va, int) or not self.ok(va):
            return None
        o = va - self.base
        e = self.img.find(b'\0', o, o + maxlen)
        if e < 0:
            return None
        b = self.img[o:e]
        try:
            s = b.decode('latin1')
        except Exception:
            return None
        if any(ord(c) < 9 for c in s):
            return None
        return s


MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
REG32 = ['eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp']


def f2i(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def i2f(i):
    return struct.unpack('<f', struct.pack('<I', i & 0xffffffff))[0]


def window(img, site, back=320):
    """Instructions ending exactly at `site`, after the last call/ret/jmp."""
    best = None
    for b in range(back, 8, -1):
        ins = list(MD.disasm(img.read(site - b, b), site - b))
        if ins and ins[-1].address + ins[-1].size == site and sum(i.size for i in ins) == b:
            best = ins
            break
    if not best:
        return []
    out = []
    for i in reversed(best):
        if i.mnemonic in ('call', 'ret', 'jmp') or i.mnemonic.startswith('j'):
            break
        if i.mnemonic == 'int3':
            break
        out.append(i)
    return list(reversed(out))


def simulate(img, ins):
    sp = 0
    mem = {}
    regs = {}
    fst = []
    xmm = {}

    def val(op):
        op = op.strip()
        if re.fullmatch(r'-?0x[0-9a-f]+|-?\d+', op):
            return int(op, 16) & 0xffffffff if '0x' in op else int(op) & 0xffffffff
        if op in regs:
            return regs[op]
        return None

    def addr(op):
        m = re.fullmatch(r'dword ptr \[(\w+)(?: ([+-]) (0x[0-9a-f]+|\d+))?\]', op.strip())
        if not m:
            return None
        base, sign, off = m.groups()
        off = int(off, 0) if off else 0
        if sign == '-':
            off = -off
        if base == 'esp':
            return sp + off
        r = regs.get(base)
        if isinstance(r, tuple) and r[0] == 'sp':
            return r[1] + off
        return None

    for i in ins:
        mn, ops = i.mnemonic, i.op_str
        parts = [p.strip() for p in ops.split(',')] if ops else []
        try:
            if mn == 'push':
                sp -= 4
                m = re.fullmatch(r'dword ptr \[(0x[0-9a-f]+)\]', parts[0])
                mem[sp] = val(parts[0]) if not m else ('mem', int(m.group(1), 16))
            elif mn == 'pop':
                if parts[0] in REG32:
                    regs[parts[0]] = mem.get(sp)
                sp += 4
            elif mn == 'sub' and parts[0] == 'esp':
                sp -= int(parts[1], 0)
            elif mn == 'add' and parts[0] == 'esp':
                sp += int(parts[1], 0)
            elif mn == 'mov' and parts[0] in REG32:
                if parts[1] == 'esp':
                    regs[parts[0]] = ('sp', sp)
                else:
                    m = re.fullmatch(r'dword ptr \[(0x[0-9a-f]+)\]', parts[1])
                    regs[parts[0]] = val(parts[1]) if not m else ('mem', int(m.group(1), 16))
            elif mn == 'lea' and parts[0] in REG32:
                m = re.fullmatch(r'\[esp(?: \+ (0x[0-9a-f]+|\d+))?\]', parts[1])
                regs[parts[0]] = ('sp', sp + (int(m.group(1), 0) if m and m.group(1) else 0)) if m else None
            elif mn == 'xor' and parts[0] == parts[1] and parts[0] in REG32:
                regs[parts[0]] = 0
            elif mn == 'mov' and parts[0].startswith('dword ptr ['):
                a = addr(parts[0])
                if a is not None:
                    mem[a] = val(parts[1])
            elif mn == 'mov' and parts[0].startswith('byte ptr ['):
                a = addr(parts[0].replace('byte', 'dword'))
                if a is not None:
                    v = val(parts[1])
                    mem[a] = v & 0xff if isinstance(v, int) else None
            elif mn in ('fld', 'fild'):
                m = re.fullmatch(r'dword ptr \[(0x[0-9a-f]+)\]', parts[0])
                if m and mn == 'fld':
                    fst.append(img.u32(int(m.group(1), 16)))
                else:
                    a = addr(parts[0]) if parts else None
                    fst.append(mem.get(a) if (a is not None and mn == 'fld') else None)
            elif mn == 'fld1':
                fst.append(f2i(1.0))
            elif mn == 'fldz':
                fst.append(0)
            elif mn in ('fstp', 'fst'):
                a = addr(parts[0])
                v = fst[-1] if fst else None
                if mn == 'fstp' and fst:
                    fst.pop()
                if a is not None:
                    mem[a] = v
            elif mn == 'movss':
                if parts[0].startswith('xmm'):
                    m = re.fullmatch(r'dword ptr \[(0x[0-9a-f]+)\]', parts[1])
                    xmm[parts[0]] = img.u32(int(m.group(1), 16)) if m else None
                else:
                    a = addr(parts[0])
                    if a is not None:
                        mem[a] = xmm.get(parts[1])
            elif mn in ('xorps',) and parts[0] == parts[1]:
                xmm[parts[0]] = 0
            elif parts and parts[0] in REG32:
                regs[parts[0]] = None
        except Exception:
            pass
    return [mem.get(sp + 4 * k) for k in range(12)]


def fmt_float(v):
    if v is None:
        return None
    f = i2f(v)
    if f != f:
        return None
    s = ('%.6g' % f)
    return s


def harvest(path, wrappers, text_lo, text_hi):
    """Every call to a wrapper in [text_lo, text_hi): one dict per site."""
    img = Image(path)
    o0 = text_lo - img.base
    blob = img.img[o0:text_hi - img.base]
    out = []
    targets = set(wrappers)
    for m in re.finditer(rb'\xe8', blob):
        k = m.start()
        if k + 5 > len(blob):
            break
        t = (text_lo + k + 5 + struct.unpack_from('<i', blob, k + 1)[0]) & 0xffffffff
        if t not in targets:
            continue
        site = text_lo + k
        kind = wrappers[t]
        a = simulate(img, window(img, site))
        name = img.cstr(a[0]) if isinstance(a[0], int) else None
        rec = {'site': site, 'wrapper': kind, 'name': name, 'type': None, 'default': None,
               'min': None, 'max': None, 'flags': None, 'desc': None, 'enum': None}
        if kind == 'bool':
            rec.update(type='bool', default=a[1] & 0xff if isinstance(a[1], int) else None, flags=a[2], desc=a[3])
        elif kind == 'int':
            sv = lambda v: (v - (1 << 32) if v & 0x80000000 else v) if isinstance(v, int) else None
            rec.update(type='int', default=sv(a[1]), min=sv(a[2]), max=sv(a[3]), flags=a[4], desc=a[5])
        elif kind == 'float':
            rec.update(type='float', default=fmt_float(a[1]) if isinstance(a[1], int) else None,
                       min=fmt_float(a[2]) if isinstance(a[2], int) else None,
                       max=fmt_float(a[3]) if isinstance(a[3], int) else None, flags=a[4], desc=a[5])
        elif kind in ('vec2', 'vec3', 'vec4'):
            n = int(kind[3])
            vals = [fmt_float(x) if isinstance(x, int) else None for x in a[1:1 + n]]
            rec.update(type=kind, default=' '.join(str(v) for v in vals),
                       min=fmt_float(a[1 + n]) if isinstance(a[1 + n], int) else None,
                       max=fmt_float(a[2 + n]) if isinstance(a[2 + n], int) else None,
                       flags=a[3 + n], desc=a[4 + n])
        elif kind == 'float3color':
            vals = [fmt_float(x) if isinstance(x, int) else None for x in a[1:4]]
            rec.update(type='float3color', default=' '.join(str(v) for v in vals),
                       max=fmt_float(a[4]) if isinstance(a[4], int) else None, flags=a[5], desc=a[6])
        elif kind == 'string':
            s = img.cstr(a[1]) if isinstance(a[1], int) else None
            rec.update(type='string', default=s, flags=a[2], desc=a[3])
        elif kind == 'enum':
            lst = []
            p = a[1]
            if isinstance(p, int) and img.ok(p, 4):
                for j in range(64):
                    q = img.u32(p + 4 * j)
                    if not q:
                        break
                    s = img.cstr(q)
                    if s is None:
                        lst = None
                        break
                    lst.append(s)
            rec.update(type='enum', enum=lst, default=a[2] if isinstance(a[2], int) else None, flags=a[3], desc=a[4])
        elif kind == 'color':
            vals = [fmt_float(x) if isinstance(x, int) else None for x in a[1:5]]
            rec.update(type='color', default=' '.join(str(v) for v in vals), flags=a[5], desc=a[6])
        elif kind == 'variant':
            t = a[1] & 0xff if isinstance(a[1], int) else None
            rec['type'] = TYPES[t] if t is not None and t < len(TYPES) else None
            rec['flags'] = a[2]
            rec['desc'] = a[9]
            v = a[3:7]
            lim = a[7:9]
            ty = rec['type']
            if ty == 'bool':
                rec['default'] = v[0] & 0xff if isinstance(v[0], int) else None
            elif ty == 'int':
                sv = lambda x: (x - (1 << 32) if x & 0x80000000 else x) if isinstance(x, int) else None
                rec.update(default=sv(v[0]), min=sv(lim[0]), max=sv(lim[1]))
            elif ty == 'float':
                rec.update(default=fmt_float(v[0]) if isinstance(v[0], int) else None,
                           min=fmt_float(lim[0]) if isinstance(lim[0], int) else None,
                           max=fmt_float(lim[1]) if isinstance(lim[1], int) else None)
            elif ty == 'string':
                rec['default'] = img.cstr(v[0]) if isinstance(v[0], int) else None
            elif ty in ('vec2', 'vec3', 'vec4', 'float3color'):
                n = {'vec2': 2, 'vec3': 3, 'vec4': 4, 'float3color': 3}[ty]
                rec['default'] = ' '.join(str(fmt_float(x) if isinstance(x, int) else None) for x in v[:n])
                rec.update(min=fmt_float(lim[0]) if isinstance(lim[0], int) else None,
                           max=fmt_float(lim[1]) if isinstance(lim[1], int) else None)
        if isinstance(rec['desc'], int):
            rec['desc'] = img.cstr(rec['desc'])
        elif not isinstance(rec['desc'], str):
            rec['desc'] = None
        if not isinstance(rec['flags'], int):
            rec['flags'] = None
        out.append(rec)
    return img, out


# ---- inputs ------------------------------------------------------------------------------------

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
C1_EXE = os.path.join(ROOT, 'C1.exe')
DOC_MD = os.path.join(ROOT, 'CODOL', '1.1.1.13_server', 'alpha0022', 'docs', 'dvars_c1exe.md')
IW4_DIR = os.environ.get('IW4_DIR', r'D:\CoD\iw4x_development')
IW4_EXE = os.path.join(IW4_DIR, 'iw4x.exe')
CDB = os.environ.get('CDB', r'C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe')
SYMS_CACHE = os.path.join(HERE, 'iw4x_syms.txt')  # not committed; regenerated with cdb when missing

C1_WRAPPERS = {0x624370: 'bool', 0x624420: 'int', 0x6244D0: 'float', 0x624570: 'vec3', 0x624620: 'float3color',
               0x624690: 'vec4', 0x624740: 'string', 0x624790: 'enum', 0x624840: 'color', 0x6242C0: 'variant'}
IW4_WRAPPERS = {0x4CE1A0: 'bool', 0x479830: 'int', 0x648440: 'float', 0x4F6070: 'vec2', 0x4EF8E0: 'vec3',
                0x471500: 'vec4', 0x4FC7E0: 'string', 0x412E40: 'enum', 0x4F28E0: 'color', 0x648390: 'variant'}

# Hand-verified (kernel33.c / README notes and reading the registration sites). Names marked
# "codol" are CoDOL additions with no IW4 counterpart; the name is descriptive, not original.
NOTES = {
    '1848': ('com_maxfps', 'kernel33.c PatchDvarRegistrations (Dvar_RegisterInt @0x597E21)'),
    '591': ('cg_drawFPS', 'kernel33.c PatchDvarRegistrations (Dvar_RegisterEnum @0x44C631)'),
    '972': ('cl_mouseAccel', 'kernel33.c raw mouse notes (CL_GetMouseMovement 0x48FF40)'),
    '2289': ('sys_gpu', 'kernel33.c gpu rating notes: the saved GPU name'),
    '1908': ('com_recommendedSet', 'kernel33.c gpu rating notes (0x597D31)'),
    '2308': ('r_autopriority', 'registered by the main WndProc on WM_CREATE (0x63367E), as IW4'),
    '864': ('monkeytoy', 'Con_Init 0x489090 (IW4 con_restricted, here default 0)'),
    '865': ('con_matchPrefixOnly', 'Con_Init 0x489090'),
    '2658': ('com_gpuRating', 'codol: GPU benchmark score (kernel33.c PatchGpuRating, 0x5988C8)'),
    '1436': ('com_logPrefix', 'codol: console log name prefix (README multi-instance notes)'),
    '2225': ('ui_scaleform', 'codol: Scaleform UI on/off, compared with "on" (Menus_OpenByName 0x5F4FB0)'),
}


def iw4_symbols():
    if not os.path.exists(SYMS_CACHE):
        out = subprocess.run([CDB, '-z', IW4_EXE, '-y', IW4_DIR, '-c',
                              '.symopt+0x40;.reload /f iw4x.exe=0x400000;x iw4x!*;q'],
                             capture_output=True, text=True, errors='replace', cwd=IW4_DIR).stdout
        with open(SYMS_CACHE, 'w', encoding='utf8') as f:
            f.write(out)
    syms = []
    for line in open(SYMS_CACHE, encoding='utf8', errors='replace'):
        m = re.match(r'([0-9a-f]{8})\s+iw4x!(\S+)', line)
        if m and 0x401000 <= int(m.group(1), 16) < 0x6D7000:
            syms.append((int(m.group(1), 16), m.group(2)))
    syms.sort()
    if len(syms) < 1000:
        raise SystemExit('no IW4 symbols: need %s and iw4x.pdb next to %s' % (CDB, IW4_EXE))
    return syms


def parse_doc():
    """dvars_c1exe.md rows: `| `1286 (player_stunWhiteFlash)` | bool | 0 | | CHEAT | 0x0052A6C0 |`
    and the replicated table `| `2485` | sv_maxclients | int | 12 | ... |`."""
    types = ('bool', 'int', 'float', 'string', 'enum', 'color', 'vec2', 'vec3', 'vec4', 'float3color', 'None')
    out = {}
    for line in open(DOC_MD, encoding='utf8'):
        if not line.startswith('| `'):
            continue
        cells = [c.strip() for c in line.strip().strip('|').split('|')]
        m = re.match(r'`([^`]+)`', cells[0])
        mm = m and re.match(r'^(\d+)(?: \(([^)]*)\))?$', m.group(1))
        if not mm:
            continue
        num, name, conf = mm.group(1), mm.group(2), 1.0
        if name:
            q = re.match(r'^(.*?)\?(\d+)%$', name)
            if q:
                name, conf = q.group(1), int(q.group(2)) / 100
            name = name.strip()
        rec = out.setdefault(num, {})
        if len(cells) >= 6 and cells[1] in types:
            rec.update(type=cells[1], default=cells[2], range=cells[3])
        elif len(cells) >= 6 and not name and re.match(r'^[A-Za-z_]\w*$', cells[1]) and cells[1] != 'CODOL':
            name = cells[1]
            rec.update(type=cells[2], default=cells[3])
        if name and name != 'CODOL' and re.match(r'^[A-Za-z_]\w*$', name):
            rec['name'], rec['conf'] = name, conf
    return out


# ---- alignment -----------------------------------------------------------------------------------

def sig(r):
    return (r['type'], str(r['default']), str(r['min']), str(r['max']), tuple(r['enum']) if r.get('enum') else None)


def loose(r):
    return (r['type'], str(r['min']), str(r['max']), tuple(r['enum']) if r.get('enum') else None)


def align(c1, iw, c1img, syms):
    saddr = [a for a, _ in syms]

    def iw_func(site):
        k = bisect.bisect_right(saddr, site) - 1
        return syms[k][1] if k >= 0 else '?'

    blob = c1img.read(0x401000, 0xB42000 - 0x401000)
    starts = set()
    for m in re.finditer(rb'\xe8', blob):
        k = m.start()
        if k + 5 <= len(blob):
            t = (0x401000 + k + 5 + struct.unpack_from('<i', blob, k + 1)[0]) & 0xffffffff
            if 0x401000 <= t < 0xB42000:
                starts.add(t)
    for m in re.finditer(rb'\xcc\xcc+', blob):
        starts.add(0x401000 + m.end())
    starts = sorted(starts)

    def c1_func(site):
        return starts[bisect.bisect_right(starts, site) - 1]

    plain_c1 = {r['name'] for r in c1 if r['name'] and not r['name'].isdigit()}
    iw_by_func, c1_by_func = collections.OrderedDict(), collections.OrderedDict()
    for r in sorted(iw, key=lambda r: r['site']):
        if r['name']:
            iw_by_func.setdefault(iw_func(r['site']), []).append(r)
    for r in sorted(c1, key=lambda r: r['site']):
        if r['name']:
            c1_by_func.setdefault(c1_func(r['site']), []).append(r)

    def score(a, b):
        if not a['name'].isdigit():
            return 10 if a['name'].lower() == b['name'].lower() else 0
        if b['name'] in plain_c1 or a['type'] != b['type']:
            return 0
        if sig(a) == sig(b):
            return 5 if a['flags'] == b['flags'] else 4
        if loose(a) == loose(b):
            return 2
        if a['type'] in ('bool', 'string', 'color') and str(a['default']) == str(b['default']):
            return 2
        return 1 if a['type'] == 'bool' else 0

    def lcs(A, B):
        n, m = len(A), len(B)
        D = [[0] * (m + 1) for _ in range(n + 1)]
        for i in range(n - 1, -1, -1):
            for j in range(m - 1, -1, -1):
                s = score(A[i], B[j])
                D[i][j] = max(D[i + 1][j], D[i][j + 1], (D[i + 1][j + 1] + s) if s else 0)
        pairs, i, j = [], 0, 0
        while i < n and j < m:
            s = score(A[i], B[j])
            if s and D[i][j] == D[i + 1][j + 1] + s:
                pairs.append((i, j, s))
                i, j = i + 1, j + 1
            elif D[i + 1][j] >= D[i][j + 1]:
                i += 1
            else:
                j += 1
        return D[0][0], pairs

    result = collections.defaultdict(list)
    iw_items = list(iw_by_func.items())
    for f, A in c1_by_func.items():
        if not any(r['name'].isdigit() for r in A):
            continue
        used, remaining = set(), list(range(len(A)))
        for _ in range(6):
            sub = [A[i] for i in remaining]
            if not any(r['name'].isdigit() for r in sub):
                break
            best = None
            for g, B in iw_items:
                if g in used:
                    continue
                sc, pairs = lcs(sub, B)
                npairs = sum(1 for p in pairs if p[2] >= 2)
                if npairs and (best is None or (sc, npairs, -len(B)) > best[0]):
                    best = ((sc, npairs, -len(B)), g, B, pairs)
            if not best or (best[0][0] < 6 and best[0][1] < 2):
                break
            _, g, B, pairs = best
            used.add(g)
            matched = set()
            for i, j, s in pairs:
                matched.add(remaining[i])
                if sub[i]['name'].isdigit():
                    result[sub[i]['name']].append((B[j]['name'], {5: 0.97, 4: 0.95, 2: 0.85, 1: 0.6}[s], B[j]))
            anchors = [(-1, -1)] + [(i, j) for i, j, s in pairs] + [(len(sub), len(B))]
            for (i0, j0), (i1, j1) in zip(anchors, anchors[1:]):
                gi, gj = list(range(i0 + 1, i1)), list(range(j0 + 1, j1))
                if gi and len(gi) == len(gj) and (i0 >= 0 or i1 < len(sub)):
                    for i, j in zip(gi, gj):
                        a, b = sub[i], B[j]
                        if a['name'].isdigit() and a['type'] == b['type'] and b['name'] not in plain_c1:
                            result[a['name']].append((b['name'], 0.7, b))
                            matched.add(remaining[i])
            remaining = [i for i in remaining if i not in matched]

    final = {}
    for n, props in result.items():
        votes = collections.defaultdict(float)
        for name, conf, b in props:
            votes[name] += conf
        name = max(votes, key=votes.get)
        conf = min(0.99, max(c for nm, c, _ in props if nm == name)) * (0.8 if len(votes) > 1 else 1)
        final[n] = (name, conf)
    return final


# ---- output ---------------------------------------------------------------------------------------

def c_str(s):
    if s is None:
        return 'NULL'
    out = []
    for ch in s.encode('latin1', 'replace'):
        if ch in (0x22, 0x5C):
            out.append('\\' + chr(ch))
        elif 0x20 <= ch < 0x7F:
            out.append(chr(ch))
        else:
            out.append('\\%03o' % ch)
    return '"' + ''.join(out) + '"'


def domain_text(r):
    t = r['type']
    if t == 'bool':
        return '0/1'
    if t == 'enum':
        return '|'.join(r['enum']) if r.get('enum') else ''
    if t in ('int', 'float', 'vec2', 'vec3', 'vec4', 'float3color'):
        lo, hi = r.get('min'), r.get('max')
        def short(v):
            if v is None:
                return '?'
            s = str(v)
            return {'3.40282e+38': 'inf', '-3.40282e+38': '-inf', '2147483647': 'max', '-2147483648': 'min'}.get(s, s)
        return '%s..%s' % (short(lo), short(hi))
    if t == 'string':
        return 'text'
    if t == 'color':
        return 'r g b a'
    return ''


TYPE_IDS = {t: i for i, t in enumerate(TYPES)}


def main():
    c1img, c1 = harvest(C1_EXE, C1_WRAPPERS, 0x401000, 0xB42000)
    iwimg, iw = harvest(IW4_EXE, IW4_WRAPPERS, 0x401000, 0x6D7000)
    syms = iw4_symbols()
    aligned = align(c1, iw, c1img, syms)
    doc = parse_doc()

    iw_desc = {}
    for r in iw:
        if r['name'] and r.get('desc') and r['name'].lower() not in iw_desc:
            iw_desc[r['name'].lower()] = r['desc']
    c1_first = {}
    for r in sorted(c1, key=lambda r: r['site']):
        if r['name'] and r['name'].isdigit():
            c1_first.setdefault(r['name'], r)
    plain = {r['name'].lower() for r in c1 if r['name'] and not r['name'].isdigit()}

    # candidates per id: (priority, conf, name, source)
    cands = collections.defaultdict(list)
    for n, (name, why) in NOTES.items():
        cands[n].append((3, 1.0, name, 'notes'))
    for n, v in doc.items():
        if v.get('name'):
            cands[n].append((2 if v['conf'] >= 0.9 else 1, v['conf'], v['name'], 'doc'))
    for n, (name, conf) in aligned.items():
        cands[n].append((2 if conf >= 0.85 else 1, conf, name, 'iw4'))

    chosen = {}
    for n, lst in cands.items():
        lst.sort(key=lambda c: (c[0], c[1]), reverse=True)
        pri, conf, name, src = lst[0]
        agree = [c for c in lst if c[2].lower() == name.lower()]
        if len({c[3] for c in agree}) > 1:
            conf = max(conf, 0.97)
            src = '+'.join(sorted({c[3] for c in agree}))
        if (conf < 0.5 and src == 'iw4') or name.lower() in plain:
            continue
        chosen[n] = (pri, conf, name, src)
    # one id per name
    by_name = collections.defaultdict(list)
    for n, c in chosen.items():
        by_name[c[2].lower()].append((c[0], c[1], n))
    for name, lst in by_name.items():
        lst.sort(reverse=True)
        for _, _, n in lst[1:]:
            del chosen[n]

    rows = []
    for n in sorted(chosen, key=int):
        pri, conf, name, src = chosen[n]
        r = c1_first.get(n)
        if r:
            ty = r['type'] or doc.get(n, {}).get('type')
            default = r['default']
            if ty == 'enum' and isinstance(default, int) and r.get('enum') and 0 <= default < len(r['enum']):
                default = r['enum'][default]
            dom = domain_text(r)
        else:
            ty = doc.get(n, {}).get('type')
            default = doc.get(n, {}).get('default')
            dom = doc.get(n, {}).get('range', '')
        if ty not in TYPE_IDS:
            ty = None
        desc = iw_desc.get(name.lower()) or (NOTES[n][1] if n in NOTES and NOTES[n][1].startswith('codol') else '')
        rows.append((n, name, ty, default, dom, desc, int(round(conf * 100)), src))

    src_ids = {'notes': 1, 'doc': 2, 'iw4': 4}
    lines = [
        '/* Generated by gen_dvars.py: readable names for C1.exe\'s numeric dvars (1.1.1.13).',
        ' * Sources: dvars_c1exe.md (alpha0022 docs), kernel33.c notes, and an IW4 alignment of every',
        ' * Dvar_Register* site in C1.exe against iw4x.exe (pdb symbols): same registration function,',
        ' * same type/default/domain sequence. conf < 85 = alignment by position only, treat as a hint.',
        ' * %d named of %d numeric dvars registered in C1.exe\'s .text. Do not edit by hand. */' % (
            len(rows), len(c1_first)),
        '',
        '#define C1DV_SRC_NOTES 1',
        '#define C1DV_SRC_DOC   2',
        '#define C1DV_SRC_IW4   4',
        '',
        'typedef struct {',
        '    const char *id;      /* the name C1 registers, e.g. "591" */',
        '    const char *name;    /* readable (IW4) name, e.g. "cg_drawFPS" */',
        '    signed char type;    /* dvar type 0 bool .. 9 float3color, -1 unknown */',
        '    unsigned char conf;  /* 0..100 */',
        '    unsigned char src;   /* C1DV_SRC_* bits */',
        '    const char *def;     /* default as registered in C1.exe (text) */',
        '    const char *domain;  /* "0..100", "Off|Simple|...", "0/1", "text" */',
        '    const char *desc;    /* IW4 description (C1 strips them) */',
        '} C1DvarName;',
        '',
        'static const C1DvarName kC1DvarNames[] = {',
    ]
    for n, name, ty, default, dom, desc, conf, src in rows:
        bits = sum(v for k, v in src_ids.items() if k in src.split('+'))
        lines.append('    { %s, %s, %d, %d, %d, %s, %s, %s },' % (
            c_str(n), c_str(name), TYPE_IDS[ty] if ty else -1, conf, bits,
            c_str(None if default is None else str(default)), c_str(dom), c_str(desc or '')))
    lines += ['};', '#define C1_DVAR_NAME_COUNT (sizeof(kC1DvarNames) / sizeof(kC1DvarNames[0]))', '']
    with open(os.path.join(HERE, 'c1_dvars.h'), 'w', newline='\n', encoding='latin1') as f:
        f.write('\n'.join(lines))
    stats = collections.Counter(r[7] for r in rows)
    print('c1_dvars.h: %d of %d numeric dvars named (%d at conf >= 85); by source %s' % (
        len(rows), len(c1_first), sum(1 for r in rows if r[6] >= 85), dict(stats)))


if __name__ == '__main__':
    main()
