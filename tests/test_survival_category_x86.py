"""The survival category against C1's own code, in Unicorn: tests/survival_category_x86_fixture.c is
survival_category.c as it ships (install, byte checks, hook, callback); C1.exe supplies the functions it patches.

    python tests/test_survival_category_x86.py --fixture DLL --image C1.exe

Requires pefile + unicorn. Nothing is attached or launched. Runs C1's zone-list builder 0x4CCDF0 (with its load-size
estimate 0x4B6380 and DB_IsZoneLoaded 0x4CD7D0) and its const configstring selector 0x612850, stock and patched, and
requires:
  * every category other than "survival" (mp, combat, war, coop, zm, case variants, near misses, empty) produces the
    same zone lists, flags, size estimate, table lookup, checksum and callee-saved registers with the hook installed
    as without it, for mp, coop-session and zm maps, with and without gametype zones already loaded;
  * "survival" (any case) loads, in one list after the common_mp set, patch_/common_/patch_localized_/localized_
    common_gametype_survival and then C1's own mp gametype set (flags 4), then the plain map zones; each set is
    skipped when its common_gametype_* zone is loaded; the size estimate, mp/constConfigStrings_mp.csv and its
    checksum are exactly C1's for an mp category;
  * the install writes one jump and nothing else; a byte mismatch at any site, or "+set ui_survival 0", installs
    nothing.
Boundary doubles: DB_LoadXAssets 0x8465768 (records), DB_FindXAssetHeader 0x4CB8E0 (fake tables), C1's table
checksum 0x83B094E (a TenProtect VM stub: a per-table stand-in), Com_sprintf 0x6286A0, the error print 0x8308DBD and two
kernel32 import slots.
"""
import argparse, struct, sys, unittest
if __name__ != '__main__':
    raise unittest.SkipTest('the C1 x86 proof is invoked explicitly with --image and --fixture')
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_PROT_ALL
from unicorn.x86_const import *

P = argparse.ArgumentParser()
P.add_argument('--fixture', required=True)
P.add_argument('--image', required=True)
A, rest = P.parse_known_args()

PE = pefile.PE(A.image, fast_load=True)
FP = pefile.PE(A.fixture)
EXPORT = {s.name.decode(): FP.OPTIONAL_HEADER.ImageBase + s.address for s in FP.DIRECTORY_ENTRY_EXPORT.symbols}
BASE = PE.OPTIONAL_HEADER.ImageBase
FBASE = FP.OPTIONAL_HEADER.ImageBase
FEND = FBASE + FP.OPTIONAL_HEADER.SizeOfImage
TABLES, STUBS, STACK = 0x30000000, 0x31000000, 0x32000000
STOP = STUBS + 0xF00
SITES = [(0x4CCEB5, 6), (0x8458F2A, 7)]
UNTOUCHED = [(0x4B6380, 0x400), (0x612850, 0x70), (0x4CCDF0, 0xC5), (0x4CCEBB, 0x480)]
DB_LOAD, DB_FIND, CHECKSUM, SPRINTF, PRINTERR = 0x8465768, 0x4CB8E0, 0x83B094E, 0x6286A0, 0x8308DBD
ZONE_COUNT, ZONE_ORDER, ZONES, ZONE_SELF = 0x18FCFBC, 0x1C402A4, 0x1F0E108, 0x17C6880
SURV = ['patch_common_gametype_survival', 'common_gametype_survival',
        'patch_localized_common_gametype_survival', 'localized_common_gametype_survival']
MPSET = ['patch_common_gametype_mp', 'common_gametype_mp',
         'patch_localized_common_gametype_mp', 'localized_common_gametype_mp']


class Machine:
    def __init__(self):
        u = self.u = Uc(UC_ARCH_X86, UC_MODE_32)
        size = (PE.OPTIONAL_HEADER.SizeOfImage + 0xFFFF) & ~0xFFFF
        u.mem_map(BASE, size, UC_PROT_ALL)
        for s in PE.sections:
            data = s.get_data()
            u.mem_write(BASE + s.VirtualAddress, data[:max(s.Misc_VirtualSize, len(data))])
        u.mem_map(FBASE, (FP.OPTIONAL_HEADER.SizeOfImage + 0xFFFF) & ~0xFFFF, UC_PROT_ALL)
        u.mem_write(FBASE, FP.get_memory_mapped_image())
        for at in (TABLES, STUBS, STACK):
            u.mem_map(at, 0x1000000, UC_PROT_ALL)
        # kernel32 slots: 0x9A1D35C (1 argument) and 0x9A1D350 (2 arguments, 0 = skip the reload block)
        u.mem_write(STUBS, b'\xc2\x04\x00')
        u.mem_write(STUBS + 0x10, b'\x31\xc0\xc2\x08\x00')
        self.put(0x9A1D35C, STUBS)
        self.put(0x9A1D350, STUBS + 0x10)
        self.put(0x187490C, 0, 1)        # no extra pre-list
        self.put(0x6C44E35, 1, 1)        # skip the connection-state jump table
        self.put(ZONE_SELF, 0)
        self.heap = TABLES
        self.tables = {}
        self.loads, self.finds = [], []
        self.fixture_hits = 0
        self.original = {at: bytes(u.mem_read(at, n)) for at, n in SITES}
        u.hook_add(UC_HOOK_CODE, self.hook)

    # memory helpers
    def put(self, at, v, n=4): self.u.mem_write(at, int(v).to_bytes(n, 'little'))
    def get(self, at, n=4): return int.from_bytes(self.u.mem_read(at, n), 'little')
    def cstr(self, at):
        out = bytearray()
        while True:
            b = self.u.mem_read(at + len(out), 64)
            i = b.find(b'\0')
            if i >= 0:
                return (out + b[:i]).decode('latin-1')
            out += b

    def alloc(self, data):
        at = self.heap
        self.u.mem_write(at, data)
        self.heap = (at + len(data) + 15) & ~15
        return at

    def string(self, s): return self.alloc(s.encode('latin-1') + b'\0')

    def table(self, name, rows):
        key = name.lower()
        if key not in self.tables:
            cells = bytearray()
            for r in rows:
                for cell in r:
                    cells += struct.pack('<Ii', self.string(cell), 0)
            cp = self.alloc(bytes(cells) or b'\0')
            self.tables[key] = self.alloc(struct.pack('<IiiI', self.string(name), 2, len(rows), cp))
        return self.tables[key]

    def ret(self, value=None):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        self.u.reg_write(UC_X86_REG_ESP, sp + 4)
        self.u.reg_write(UC_X86_REG_EIP, self.get(sp))
        if value is not None:
            self.u.reg_write(UC_X86_REG_EAX, value)

    def arg(self, i): return self.get(self.u.reg_read(UC_X86_REG_ESP) + 4 + 4 * i)

    def hook(self, u, at, size, data):
        if FBASE <= at < FEND:
            self.fixture_hits += 1
        if at == STOP:
            u.emu_stop()
        elif at == DB_LOAD:
            lst, n = self.arg(0), self.arg(1)
            self.loads.append([(self.cstr(self.get(lst + 12 * i)), self.get(lst + 12 * i + 4), self.get(lst + 12 * i + 8))
                               for i in range(n)] + [('sync', self.arg(2), 0)])
            self.ret()
        elif at == DB_FIND:
            t, name = self.arg(0), self.cstr(self.arg(1))
            self.finds.append((t, name))
            self.ret(self.table(name, [['2', 'cod'], ['3', name]]))
        elif at == CHECKSUM:
            # stand-in for C1's VM checksum: a value per table
            self.ret(0xC1000000 | (self.arg(0) & 0xFFFFF))
        elif at == SPRINTF:
            buf, n, fmt = self.arg(0), self.arg(1), self.cstr(self.arg(2))
            out, k, i = '', 3, 0
            while i < len(fmt):
                if fmt[i] == '%' and fmt[i + 1] == 's':
                    out += self.cstr(self.arg(k)); k += 1; i += 2
                elif fmt[i] == '%' and fmt[i + 1] == 'd':
                    out += str(self.arg(k)); k += 1; i += 2
                else:
                    out += fmt[i]; i += 1
            self.u.mem_write(buf, (out[:n - 1] + '\0').encode('latin-1'))
            self.ret(len(out))
        elif at == PRINTERR:
            self.ret()

    # state
    def loaded(self, names):
        """C1's zone table: the zones DB_IsZoneLoaded 0x4CD7D0 finds."""
        self.put(ZONE_COUNT, len(names))
        for i, name in enumerate(names):
            self.put(ZONE_ORDER + i, i, 1)
            self.u.mem_write(ZONES + 0xA8 * i + 0xC, name.encode() + b'\0')

    def patched(self, on):
        if on:
            self.call('STestInstall')
        else:
            for at, n in SITES:
                self.u.mem_write(at, self.original[at])
        self.u.ctl_flush_tb()  # code written behind Unicorn's translation cache

    def call(self, name):
        self.native(EXPORT[name], {})

    def native(self, entry, regs):
        sp = STACK + 0xF00000
        self.put(sp, STOP)
        init = dict(ebx=0x11111111, esi=0x22222222, edi=0x33333333, ebp=0x44444444)
        init.update(regs)
        for r, v in init.items():
            self.u.reg_write(getattr(sys.modules[__name__], 'UC_X86_REG_' + r.upper()), v)
        self.u.reg_write(UC_X86_REG_ESP, sp)
        self.u.emu_start(entry, STOP, count=20000000)
        assert self.u.reg_read(UC_X86_REG_EIP) == STOP, hex(self.u.reg_read(UC_X86_REG_EIP))
        assert self.u.reg_read(UC_X86_REG_ESP) == sp + 4, 'stack imbalance'
        return {r: self.u.reg_read(getattr(sys.modules[__name__], 'UC_X86_REG_' + r.upper()))
                for r in ('ebx', 'ebp') + (('esi',) if 'esi' not in regs else ()) + (('edi',) if 'edi' not in regs else ())}

    def zone_list(self, category, mapname):
        self.loads = []
        cat, m = self.string(category), self.string(mapname)
        regs = self.native(0x4CCDF0, dict(ecx=cat, edx=m))
        return dict(loads=self.loads, estimate=self.get(0x17203FC), regs=regs)

    def const_table(self, category):
        self.finds = []
        for at in range(0x2A99A40, 0x2A99A54, 4):
            self.put(at, 0xDEADBEEF)
        regs = self.native(0x612850, dict(edi=self.string(category), esi=0x55555555))
        table = self.get(0x2A99A44)
        return dict(finds=self.finds, table=self.cstr(self.get(table)) if table else None, crc=self.get(0x2A99A50),
                    cleared=[self.get(a) for a in (0x2A99A40, 0x2A99A48, 0x2A99A4C)], regs=regs,
                    esi=self.u.reg_read(UC_X86_REG_ESI))


M = Machine()
CATEGORIES = ['mp', 'combat', 'war', 'tdm', 'coop', 'COOP', 'coop_survival', 'zm', 'ZM', 'zm_x', '', 'surv',
              'survival_', 'survivals', 'xsurvival', 'evac', 'cyborg']
MAPS = ['mp_killhouse_sh', 'coop_training_mp_killhouse_sh', 'coop_survival_mp_afghan', 'zm_prototype_sh']
LOADED = [[], ['common_gametype_mp'], ['common_gametype_coop'], ['common_gametype_zm'], ['common_gametype_survival'],
          ['common_mp', 'common_gametype_mp'], ['common_gametype_survival', 'common_gametype_mp']]


class SurvivalCategory(unittest.TestCase):
    def run_both(self, fn, *args, through_hook=True):
        M.patched(False)
        M.fixture_hits = 0
        stock = fn(*args)
        self.assertEqual(M.fixture_hits, 0, 'the stock run never reaches the hook')
        M.patched(True)
        M.fixture_hits = 0
        hooked = fn(*args)
        if through_hook:
            self.assertGreater(M.fixture_hits, 0, 'the patched run goes through the hook')
        else:
            self.assertEqual(M.fixture_hits, 0, 'this function is not hooked')
        M.patched(False)
        return stock, hooked

    def test_install_writes_one_jump_and_nothing_else(self):
        M.patched(False)
        before = [bytes(M.u.mem_read(at, n)) for at, n in UNTOUCHED]
        M.patched(True)
        self.assertEqual(M.u.mem_read(0x4CCEB5, 1)[0], 0xE9)
        self.assertEqual(bytes(M.u.mem_read(0x4CCEBA, 1)), b'\x90')
        self.assertEqual([bytes(M.u.mem_read(at, n)) for at, n in UNTOUCHED], before)
        M.patched(False)

    def test_every_other_category_is_byte_identical(self):
        n = 0
        for loaded in LOADED:
            M.loaded(loaded)
            for cat in CATEGORIES:
                for mapname in MAPS:
                    with self.subTest(cat=cat, map=mapname, loaded=loaded):
                        stock, hooked = self.run_both(M.zone_list, cat, mapname)
                        self.assertEqual(stock, hooked)
                        n += 1
        for cat in CATEGORIES + ['survival']:
            with self.subTest(cat=cat, const=True):
                stock, hooked = self.run_both(M.const_table, cat, through_hook=False)
                self.assertEqual(stock, hooked)
        M.loaded([])
        self.assertGreater(n, 400)

    def test_survival_zone_list(self):
        M.loaded([])
        stock_mp, _ = self.run_both(M.zone_list, 'mp', 'mp_fav_tropical')
        first_mp = stock_mp['loads'][0]
        self.assertEqual([z[0] for z in first_mp[-5:-1]], MPSET)
        for cat in ('survival', 'SURVIVAL', 'Survival'):
            with self.subTest(cat=cat):
                stock, hooked = self.run_both(M.zone_list, cat, 'mp_fav_tropical')
                self.assertEqual(stock, stock_mp, 'C1 alone treats survival as mp')
                self.assertEqual(hooked['loads'][0], first_mp[:-5] + [(z, 4, 0) for z in SURV] + first_mp[-5:])
                self.assertEqual(hooked['loads'][1:], stock_mp['loads'][1:], 'the map zones are C1\'s')
                self.assertEqual(hooked['regs'], stock['regs'])
                self.assertEqual(hooked['estimate'], stock_mp['estimate'], 'the loading estimate is the mp one')
        _, hooked = self.run_both(M.zone_list, 'survival', 'mp_fav_tropical')
        print('survival load order (map mp_fav_tropical, nothing loaded):')
        for i, call in enumerate(hooked['loads']):
            print('  DB_LoadXAssets #%d (sync %d): %s' % (i + 1, call[-1][1],
                                                         ', '.join('%s (flags %d)' % (z, f) for z, f, _ in call[:-1])))

    def test_survival_respects_each_loaded_set(self):
        cases = [(['common_gametype_survival'], [], True), (['common_gametype_mp'], SURV, False),
                 (['common_gametype_survival', 'common_gametype_mp'], [], False)]
        for loaded, surv, mp in cases:
            with self.subTest(loaded=loaded):
                M.loaded(loaded)
                _, hooked = self.run_both(M.zone_list, 'survival', 'mp_fav_tropical')
                names = [z[0] for z in hooked['loads'][0][:-1]]
                self.assertEqual([z for z in names if 'survival' in z], surv)
                self.assertEqual([z for z in names if 'gametype_mp' in z], MPSET if mp else [])
                self.assertFalse(any('coop' in z or 'gametype_zm' in z for z in names))
        M.loaded([])

    def test_survival_const_table_is_c1s_mp_table(self):
        stock_mp, _ = self.run_both(M.const_table, 'mp', through_hook=False)
        stock, hooked = self.run_both(M.const_table, 'survival', through_hook=False)
        self.assertEqual(stock, hooked)
        self.assertEqual(hooked['finds'], [(0x25, 'mp/constConfigStrings_mp.csv')])
        self.assertEqual(hooked['crc'], stock_mp['crc'])

    def test_byte_mismatch_or_switch_installs_nothing(self):
        M.patched(False)
        writes = M.get(EXPORT['stest_writes'])
        for at in (0x4CCEB5, 0x4CCEC5, 0x4CCECE, 0x4CD03E, 0x4CD053, 0x4CD0DC, 0x4CD0E8, 0x8458F2A, 0x8458F36, 0x43C4BE, 0x43C4C4):
            with self.subTest(site=hex(at)):
                b = M.get(at, 1)
                M.put(at, b ^ 0xFF, 1)
                M.call('STestInstall')
                M.put(at, b, 1)
                M.u.ctl_flush_tb()
                self.assertEqual(M.get(EXPORT['stest_writes']), writes)
                for s, n in SITES:
                    self.assertEqual(bytes(M.u.mem_read(s, n)), M.original[s])
        M.u.mem_write(EXPORT['stest_cmdline'], b'codoMP_client_shipRetail.exe +set ui_survival 0\0')
        M.call('STestInstall')
        self.assertEqual(M.get(EXPORT['stest_writes']), writes)
        M.u.mem_write(EXPORT['stest_cmdline'], b'codoMP_client_shipRetail.exe +set ui_survivalx 0\0')
        M.call('STestInstall')
        self.assertEqual(M.get(EXPORT['stest_writes']), writes + 2)
        M.patched(False)


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]] + rest)
