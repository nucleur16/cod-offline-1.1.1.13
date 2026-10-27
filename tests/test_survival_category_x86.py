"""The survival category against C1's own code, in Unicorn: tests/survival_category_x86_fixture.c is
survival_category.c as it ships (install, byte checks, hooks, callbacks); C1.exe supplies the functions it patches.

    python tests/test_survival_category_x86.py --fixture DLL --image C1.exe

Requires pefile + unicorn. Nothing is attached or launched. Runs C1's zone-list builder 0x4CCDF0 (with its load-size
estimate 0x4B6380 and DB_IsZoneLoaded 0x4CD7D0) and its const configstring selector 0x612850, stock and patched, and
requires:
  * every category other than "survival" (mp, combat, war, coop, zm, case variants, near misses, empty) produces the
    same zone lists, flags, size estimate, table lookup, checksum and callee-saved registers with the hooks installed
    as without them, for mp, coop-session and zm maps, with and without the gametype zone already loaded;
  * "survival" (any case) loads patch_/common_/patch_localized_/localized_common_gametype_survival (flags 4) and the
    plain map zones, no gametype zone when common_gametype_survival is loaded, adds no stock size, looks up
    mp/constConfigStrings_survival.csv and stores the CRC-32 checksum of survival_category_policy.h (zlib's crc32);
  * a default table (no zone has the survival table) keeps C1's checksum;
  * a byte mismatch at any site, or "+set codol_survival 0", installs nothing.
Boundary doubles: DB_LoadXAssets 0x8465768 (records), DB_FindXAssetHeader 0x4CB8E0 (fake tables), C1's table
checksum 0x83B094E (a TenProtect VM stub: a per-table stand-in), Com_sprintf 0x6286A0, the error print 0x8308DBD and two
kernel32 import slots.
"""
from pathlib import Path
import argparse, struct, sys, unittest, zlib
if __name__ != '__main__':
    raise unittest.SkipTest('the C1 x86 proof is invoked explicitly with --image and --fixture')
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_PROT_ALL
from unicorn.x86_const import *

P = argparse.ArgumentParser()
P.add_argument('--fixture', required=True)
P.add_argument('--image', required=True)
A, rest = P.parse_known_args()
ROOT = Path(__file__).resolve().parents[1]
CSV = ROOT / 'zones/codol_survival_category/stringtable/mp/constconfigstrings_survival.csv'

PE = pefile.PE(A.image, fast_load=True)
FP = pefile.PE(A.fixture)
EXPORT = {s.name.decode(): FP.OPTIONAL_HEADER.ImageBase + s.address for s in FP.DIRECTORY_ENTRY_EXPORT.symbols}
BASE = PE.OPTIONAL_HEADER.ImageBase
FBASE = FP.OPTIONAL_HEADER.ImageBase
FEND = FBASE + FP.OPTIONAL_HEADER.SizeOfImage
TABLES, STUBS, STACK = 0x30000000, 0x31000000, 0x32000000
STOP = STUBS + 0xF00
SITES = [(0x4CCEB5, 6), (0x4B6461, 6), (0x612850, 7)]
DB_LOAD, DB_FIND, CHECKSUM, SPRINTF, PRINTERR = 0x8465768, 0x4CB8E0, 0x83B094E, 0x6286A0, 0x8308DBD
ZONE_COUNT, ZONE_ORDER, ZONES, ZONE_SELF = 0x18FCFBC, 0x1C402A4, 0x1F0E108, 0x17C6880
SURV = ['patch_common_gametype_survival', 'common_gametype_survival',
        'patch_localized_common_gametype_survival', 'localized_common_gametype_survival']


def survival_rows():
    return [l.rstrip('\n').split(',', 1) for l in CSV.read_text(encoding='ascii').splitlines(True)]


def policy_crc(rows):
    c = 0
    for r in rows:
        for cell in r:
            c = zlib.crc32(cell.encode('ascii') + b'\0', c)
    return c


class Machine:
    def __init__(self):
        u = self.u = Uc(UC_ARCH_X86, UC_MODE_32)
        size = (PE.OPTIONAL_HEADER.SizeOfImage + 0xFFFF) & ~0xFFFF
        u.mem_map(BASE, size, UC_PROT_ALL)
        for s in PE.sections:
            data = s.get_data()
            u.mem_write(BASE + s.VirtualAddress, data[:max(s.Misc_VirtualSize, len(data))])
        fbase = FP.OPTIONAL_HEADER.ImageBase
        u.mem_map(fbase, (FP.OPTIONAL_HEADER.SizeOfImage + 0xFFFF) & ~0xFFFF, UC_PROT_ALL)
        u.mem_write(fbase, FP.get_memory_mapped_image())
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
        self.missing_survival = False
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
            if name.lower() == 'mp/constconfigstrings_survival.csv':
                ptr = self.table('mp/defaultStringTable.csv', [['0', 'default']]) if self.missing_survival \
                    else self.table(name, survival_rows())
            else:
                ptr = self.table(name, [['2', 'cod'], ['3', name]])
            self.ret(ptr)
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
        out = {r: self.u.reg_read(getattr(sys.modules[__name__], 'UC_X86_REG_' + r.upper()))
               for r in ('ebx', 'ebp') + (('esi',) if 'esi' not in regs else ()) + (('edi',) if 'edi' not in regs else ())}
        return out

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
                    esi=self.u.reg_read(UC_X86_REG_ESI), edi_kept=True)


M = Machine()
CATEGORIES = ['mp', 'combat', 'war', 'tdm', 'coop', 'COOP', 'coop_survival', 'zm', 'ZM', 'zm_x', '', 'surv',
              'survival_', 'survivals', 'xsurvival', 'evac', 'cyborg']
MAPS = ['mp_killhouse_sh', 'coop_training_mp_killhouse_sh', 'coop_survival_mp_afghan', 'zm_prototype_sh']
LOADED = [[], ['common_gametype_mp'], ['common_gametype_coop'], ['common_gametype_zm'], ['common_gametype_survival'],
          ['common_mp', 'common_gametype_mp']]


class SurvivalCategory(unittest.TestCase):
    def run_both(self, fn, *args):
        M.patched(False)
        M.fixture_hits = 0
        stock = fn(*args)
        self.assertEqual(M.fixture_hits, 0, 'the stock run never reaches the hooks')
        M.patched(True)
        M.fixture_hits = 0
        hooked = fn(*args)
        self.assertGreater(M.fixture_hits, 0, 'the patched run goes through the hooks')
        M.patched(False)
        return stock, hooked

    def test_install_writes_three_jumps(self):
        M.patched(True)
        for at, n in SITES:
            self.assertEqual(M.u.mem_read(at, 1)[0], 0xE9)
            self.assertEqual(bytes(M.u.mem_read(at + 5, n - 5)), b'\x90' * (n - 5))
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
                with self.subTest(cat=cat, const=True):
                    stock, hooked = self.run_both(M.const_table, cat)
                    self.assertEqual(stock, hooked)
        M.loaded([])
        self.assertGreater(n, 300)

    def test_survival_zone_list(self):
        M.loaded([])
        stock_mp, _ = self.run_both(M.zone_list, 'mp', 'mp_fav_tropical')
        for cat in ('survival', 'SURVIVAL', 'Survival'):
            with self.subTest(cat=cat):
                stock, hooked = self.run_both(M.zone_list, cat, 'mp_fav_tropical')
                self.assertEqual(stock, stock_mp, 'C1 alone treats survival as mp')
                gametype = hooked['loads'][0]
                self.assertEqual([z for z in gametype if z[0] != 'sync'][-4:], [(z, 4, 0) for z in SURV])
                self.assertEqual(gametype[:-5], stock_mp['loads'][0][:-5], 'the common_mp part is C1\'s')
                self.assertFalse(any('gametype_mp' in z[0] or 'gametype_coop' in z[0] for z in gametype))
                self.assertEqual(hooked['loads'][1:], stock_mp['loads'][1:], 'the map zones are C1\'s')
                self.assertEqual([z[0] for z in hooked['loads'][-1][:-1]], ['patch_mp_fav_tropical', 'mp_fav_tropical',
                                                                           'ffotd_client'])
                self.assertEqual(hooked['regs'], stock['regs'])
                self.assertLess(hooked['estimate'], stock_mp['estimate'], 'no mp gametype sizes counted')

    def test_survival_skips_loaded_gametype_zone(self):
        M.loaded(['common_gametype_survival'])
        _, hooked = self.run_both(M.zone_list, 'survival', 'mp_fav_tropical')
        M.loaded([])
        self.assertFalse(any('gametype' in z[0] for z in hooked['loads'][0]))

    def test_survival_const_table_and_checksum(self):
        stock, hooked = self.run_both(M.const_table, 'survival')
        self.assertEqual(stock['finds'], [(0x25, 'mp/constConfigStrings_mp.csv')])
        self.assertEqual(hooked['finds'], [(0x25, 'mp/constConfigStrings_survival.csv')])
        self.assertEqual(hooked['table'], 'mp/constConfigStrings_survival.csv')
        self.assertEqual(hooked['crc'], policy_crc(survival_rows()))
        self.assertEqual(hooked['cleared'], [0, 0, 0])
        self.assertEqual(hooked['regs'], stock['regs'])
        self.assertEqual(hooked['esi'], 0x55555555)
        print('survival const table: %d rows, checksum 0x%08X (%d)' % (len(survival_rows()), hooked['crc'],
                                                                      struct.unpack('<i', struct.pack('<I', hooked['crc']))[0]))

    def test_missing_table_keeps_native_checksum(self):
        M.missing_survival = True
        try:
            _, hooked = self.run_both(M.const_table, 'survival')
        finally:
            M.missing_survival = False
        self.assertEqual(hooked['table'], 'mp/defaultStringTable.csv')
        self.assertEqual(hooked['crc'] & 0xFFF00000, 0xC1000000)

    def test_fixture_checksum_is_zlib_crc32(self):
        rows = survival_rows()
        cells = bytearray()
        for r in rows:
            for cell in r:
                cells += struct.pack('<Ii', M.string(cell), 0)
        cp = M.alloc(bytes(cells))
        sp = STACK + 0xF00000
        M.put(sp, STOP); M.put(sp + 4, cp); M.put(sp + 8, len(rows) * 2)
        M.u.reg_write(UC_X86_REG_ESP, sp)
        M.u.emu_start(EXPORT['STestChecksum'], STOP, count=50000000)
        self.assertEqual(M.u.reg_read(UC_X86_REG_EAX), policy_crc(rows))

    def test_byte_mismatch_or_switch_installs_nothing(self):
        M.patched(False)
        writes = M.get(EXPORT['stest_writes'])
        for at in (0x4CCEB5, 0x4CD0E1, 0x4CCECE, 0x4B6461, 0x4B6596, 0x612850, 0x61288E, 0x612897, 0x6128A1, 0x6128BA):
            with self.subTest(site=hex(at)):
                b = M.get(at, 1)
                M.put(at, b ^ 0xFF, 1)
                M.u.ctl_flush_tb()
                M.call('STestInstall')
                M.put(at, b, 1)
                M.u.ctl_flush_tb()
                self.assertEqual(M.get(EXPORT['stest_writes']), writes)
                for s, n in SITES:
                    self.assertEqual(bytes(M.u.mem_read(s, n)), M.original[s])
        M.u.mem_write(EXPORT['stest_cmdline'], b'codoMP_client_shipRetail.exe +set codol_survival 0\0')
        M.call('STestInstall')
        self.assertEqual(M.get(EXPORT['stest_writes']), writes)
        M.u.mem_write(EXPORT['stest_cmdline'], b'codoMP_client_shipRetail.exe +set codol_survivalx 0\0')
        M.call('STestInstall')
        self.assertEqual(M.get(EXPORT['stest_writes']), writes + 3)
        M.patched(False)


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]] + rest)
