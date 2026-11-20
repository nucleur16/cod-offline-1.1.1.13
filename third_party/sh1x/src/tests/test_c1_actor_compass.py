"""Execute the original C1 compass driver, without replacing any engine function.

Requires the private, unchanged C1.exe and pefile/unicorn. Synthetic inputs represent
one real actor snapshot and a real sweep crossing. This proves native admission and
ping production, not a rendered frame or live-player visual acceptance.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import unittest
import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ESI
IMAGE = None

def run_actor(category='survival', radar=True, team=1, valid=True, etype=18, previous=900):
    """Map actual code; initialize one actor crossing a native swept-UAV line."""
    image = IMAGE
    p = pefile.PE(image)
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, p.OPTIONAL_HEADER.SizeOfImage + 4095 & ~4095)
    for s in p.sections:
        u.mem_write(0x400000 + s.VirtualAddress, s.get_data())
    u.mem_map(0x20000000, 0x20000)
    u.mem_map(0x30000000, 0x1000000)

    def w(a, v):
        u.mem_write(a, struct.pack('<I', v))
    w(0x6c3d2ac, 0)
    w(0x16d3b58, 0x30000000 ^ 0x490b400c)
    w(0x300035e0, 0x30400000)
    w(0x300035e4, 0x30400000)
    w(0x300035e8, 0x30400000)
    w(0x6c3d2b4, 1)
    w(0x16d3b5c, 0x30800000 ^ 0x189bfead)
    w(0x6c3d2b0, 2)
    w(0x16d3b60, 0x30e00000 ^ 0x3958bead)
    u.mem_write(0x30e00020, category.encode() + b'\x00')
    w(0x300b0350, 1000)
    w(0x3015ae70, 2)
    w(0x3015ae24, 1)
    w(0x30403374, 1)
    w(0x3040e404, 821)
    w(0x30800000 + 821 * 552 + 232, 18)
    w(0x30800000 + 821 * 552 + 384, 821)
    w(0x30168514, 1)
    w(0x30168528, 1)

    def f(a, v):
        u.mem_write(a, struct.pack('<f', v))
    cent = 0x30800000 + 821 * 552
    x = struct.unpack('<I', struct.pack('<f', 200))[0] ^ 0x73ff35a
    y = struct.unpack('<I', struct.pack('<f', 100))[0] ^ x ^ 0xedb436a0
    z = struct.unpack('<I', struct.pack('<f', 10))[0] ^ y ^ 0x893fead3
    w(cent + 24, x)
    w(cent + 28, y)
    w(cent + 32, z)
    w(0x30168520, 821)
    w(0x3000021c, 1)
    w(0x30000224, 1)
    f(0x300c8b00, 0.25)
    f(0x300bfa0c, 1)
    f(0x300bfa10, 0)
    f(0x300bfa20, 1000)
    for i, a in enumerate([0x16c7548, 0x16c7594, 0x16c752c]):
        w(a, 0x30f00000 + i * 128)
        f(0x30f0000c + i * 128, [1, 0, 1][i])
    record = 0x16c7a00 + 19 * 184
    w(record, 900)
    f(record + 4, 200)
    f(record + 8, -100)
    f(record + 12, 10)
    w(0x3000021c, int(radar))
    w(0x30000224, 0)
    w(0x30168528, team)
    w(0x30168514, int(valid))
    w(cent + 232, etype)
    w(record, previous)
    w(0x20010000, 0x20000000)
    w(0x20010004, 0)
    u.reg_write(UC_X86_REG_ESP, 0x20010000)
    u.reg_write(UC_X86_REG_ESI, 0)
    returned = []

    def stop(uc, pc, size, user):
        if pc == 0x20000000:
            returned.append(True)
            uc.emu_stop()
    u.hook_add(unicorn.UC_HOOK_CODE, stop)
    u.emu_start(0x433a70, 0, count=200000)
    assert returned, 'C1 driver did not return'
    return {'time': struct.unpack('<I', u.mem_read(record, 4))[0], 'position': struct.unpack('<3f', u.mem_read(record + 4, 12)), 'flags': struct.unpack('<I', u.mem_read(record + 68, 4))[0], 'ping_time': struct.unpack('<I', u.mem_read(record + 48, 4))[0], 'ping_position': struct.unpack('<3f', u.mem_read(record + 16, 12))}

class NativeActorCompass(unittest.TestCase):

    def test_actual_actor_driver_and_radar_ping(self):
        for category in ('survival', 'coop', 'war'):
            with self.subTest(category=category):
                result = run_actor(category)
                self.assertEqual(result['time'], 1000)
                self.assertEqual(result['position'], (200.0, 100.0, 10.0))
                self.assertEqual(result['flags'] & 1, 1)
                self.assertEqual(result['ping_time'], 1000)
                self.assertEqual(result['ping_position'], (200.0, -100.0, 10.0))

    def test_radar_disabled_does_not_ping(self):
        self.assertEqual(run_actor(radar=False)['ping_time'], 0)

    def test_friendly_actor_does_not_enemy_ping(self):
        result = run_actor(team=2)
        self.assertEqual(result['flags'] & 1, 0)
        self.assertEqual(result['ping_time'], 0)

    def test_invalid_actor_info_does_not_update(self):
        result = run_actor(valid=False)
        self.assertEqual(result['time'], 900)
        self.assertEqual(result['ping_time'], 0)

    def test_other_entity_type_does_not_update(self):
        for etype in (0, 20):
            with self.subTest(etype=etype):
                result = run_actor(etype=etype)
                self.assertEqual(result['time'], 900)
                self.assertEqual(result['ping_time'], 0)

    def test_stale_record_does_not_generate_sweep_ping(self):
        result = run_actor(previous=100)
        self.assertEqual(result['time'], 1000)
        self.assertEqual(result['ping_time'], 0)
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--image', required=True, help='Unmodified private C1.exe')
    args, remaining = parser.parse_known_args()
    IMAGE = args.image
    expected = 'D990610732258B1D00DD6FE63D6691F294312A2E0562955A01CE16216115390D'
    if hashlib.sha256(Path(IMAGE).read_bytes()).hexdigest().upper() != expected:
        parser.error('Expected the original C1 image SHA256 ' + expected)
    unittest.main(argv=[__file__] + remaining)
