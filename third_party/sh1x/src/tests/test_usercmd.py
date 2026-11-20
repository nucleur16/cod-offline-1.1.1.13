"""Compile and exercise native CoDOL commands against alpha0022's writer/captures.
Usage: python tests/test_usercmd.py --reference PATH [-v]
The reference is read only; no game process or lobby is started.
"""
import argparse
import ctypes as C
import importlib.util
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import unittest
import pathlib as _ol_pl
_ol_src=_ol_pl.Path(__file__).resolve().parents[1]/'src'
OL_INCLUDE=['-I'+str(_ol_src)]+['-I'+str(d) for d in sorted((_ol_src/'online').iterdir()) if d.is_dir()]

ROOT = Path(__file__).resolve().parents[1]
LIB = REF = None


class Msg(C.Structure):
    _fields_ = [('data', C.c_void_p), ('size', C.c_size_t), ('capacity', C.c_size_t),
                ('cursor', C.c_size_t), ('bit', C.c_size_t), ('failed', C.c_int)]


class Cmd(C.Structure):
    _fields_ = [('serverTime', C.c_uint32), ('buttons', C.c_uint32),
                ('angles', C.c_uint16 * 3), ('weapon', C.c_uint16),
                ('offHandIndex', C.c_uint16), ('altModeWeapon', C.c_uint16),
                ('meleeChargeYaw', C.c_uint16), ('direction', C.c_uint8),
                ('meleeChargeDist', C.c_uint8), ('selectedLocation', C.c_uint8 * 2),
                ('selectedLocationAngle', C.c_uint8), ('remoteAngles', C.c_uint8 * 2)]


def fields(cmd):
    return {'serverTime': cmd.serverTime, 'buttons': cmd.buttons,
            'angles': tuple(cmd.angles), 'weapon': cmd.weapon,
            'offHandIndex': cmd.offHandIndex, 'altModeWeapon': cmd.altModeWeapon,
            'meleeChargeYaw': cmd.meleeChargeYaw, 'dir': cmd.direction,
            'meleeChargeDist': cmd.meleeChargeDist,
            'selectedLoc': tuple(cmd.selectedLocation),
            'selectedLocAngle': cmd.selectedLocationAngle,
            'remoteAngles': tuple(cmd.remoteAngles)}


def read(body, key):
    backing = C.create_string_buffer(body)
    m, old = Msg(), Cmd()
    LIB.MsgReadInit(C.byref(m), backing, len(body))
    reliable, commands = [], []
    for _ in range(257):
        op = LIB.ReadBits(C.byref(m), 3)
        if m.failed:
            raise ValueError('opcode overflow')
        if op == 2:
            seq = LIB.ReadLong(C.byref(m))
            text = bytearray()
            while True:
                b = LIB.ReadByte(C.byref(m))
                if m.failed: raise ValueError('string overflow')
                if not b: break
                text.append(b)
            reliable.append((seq, bytes(text)))
        elif op in (0, 1):
            count = LIB.ReadByte(C.byref(m))
            if not count or m.failed: raise ValueError('count')
            for _ in range(count):
                to = Cmd()
                if not LIB.ReadUsercmd(C.byref(m), key, C.byref(old), C.byref(to)):
                    raise ValueError('usercmd')
                commands.append(fields(to)); old = to
            trailer = (C.c_uint8 * 16)()
            if not LIB.OL_ReadData(C.byref(m), trailer, 16): raise ValueError('trailer')
            return commands, reliable, struct.unpack('<3fI', bytes(trailer)), m.cursor
        else:
            raise ValueError('unexpected opcode')
    raise ValueError('reliable overflow')


class Commands(unittest.TestCase):
    def test_interleaved_stream(self):
        buf, m = C.create_string_buffer(16), Msg()
        LIB.MsgWriteInit(C.byref(m), buf, 16)
        LIB.WriteBits(C.byref(m), 5, 3)
        LIB.WriteLong(C.byref(m), 0x12345678)
        LIB.WriteBits(C.byref(m), 0x15, 5)
        LIB.WriteByte(C.byref(m), 9)
        LIB.WriteBits(C.byref(m), 0xfedcba98, 32)
        self.assertFalse(m.failed)
        self.assertEqual(buf.raw[:m.size], bytes.fromhex('ad785634120998badcfe'))
        LIB.MsgReadInit(C.byref(m), buf, m.size)
        self.assertEqual(LIB.ReadBits(C.byref(m), 3), 5)
        self.assertEqual(LIB.ReadLong(C.byref(m)), 0x12345678)
        self.assertEqual(LIB.ReadBits(C.byref(m), 5), 0x15)
        self.assertEqual(LIB.ReadByte(C.byref(m)), 9)
        self.assertEqual(LIB.ReadBits(C.byref(m), 32), 0xfedcba98)
        self.assertFalse(m.failed)
        LIB.ReadBits(C.byref(m), 1)
        self.assertTrue(m.failed)

    def test_weapon_ids_never_truncate(self):
        LIB.ResetWeaponMap()
        self.assertEqual(LIB.WeaponToEngine(769), -1)
        self.assertEqual(LIB.MapWeapon(769, 25), 1)
        self.assertEqual(LIB.WeaponToEngine(769), 25)
        self.assertEqual(LIB.WeaponToWire(25), 769)
        self.assertEqual(LIB.WeaponToEngine(1), -1)
        self.assertEqual(LIB.MapWeapon(513, 25), 0)
        self.assertEqual(LIB.MapWeapon(1024, 26), 0)
        self.assertEqual(LIB.MapWeapon(50, 128), 0)
        LIB.ResetWeaponMap()
        self.assertEqual(LIB.WeaponToEngine(769), -1)
        self.assertEqual(LIB.WeaponToWire(25), -1)
        self.assertEqual(LIB.WeaponToEngine(0), 0)

    def test_filters_preserve_wire(self):
        for buttons, want in ((0x200001, 0), (0x10000b03, 0x10000b02), (0x803, 0x803)):
            cmd, filtered = Cmd(), Cmd()
            cmd.buttons = buttons; cmd.direction = 9; cmd.weapon = 901
            LIB.FilterUsercmd(C.byref(cmd), C.byref(filtered))
            self.assertEqual(cmd.buttons, buttons)
            self.assertEqual(filtered.buttons, want)
            self.assertEqual(filtered.weapon, 901)
            self.assertEqual(filtered.direction, 0 if buttons & 0x10200000 else 9)

    def test_reference_generated_forms(self):
        rng = random.Random(11113)
        for n_reliable in range(21):
            key = rng.getrandbits(32)
            rel = [(i + 1, b'userinfo "name=%\xff\x80"') for i in range(n_reliable)]
            a = dict(REF._NULLCMD, serverTime=65536, buttons=0x222005,
                     angles=(120, 45678, 65535), dir=9, weapon=769, offHandIndex=1001,
                     altModeWeapon=1023, selectedLoc=(123, 254), selectedLocAngle=213,
                     remoteAngles=(128, 255), meleeChargeYaw=32100, meleeChargeDist=167)
            b = dict(a, serverTime=65552, angles=(32768, 29, 65535), dir=4, buttons=a['buttons'] ^ 1)
            c = dict(b, serverTime=65566)
            expected = [(a, 'C'), (b, 'B'), (c, 'A')]
            body = REF.build_client_message(key, rel, expected, pred=64000)
            got, reliable, trailer, cursor = read(body, key)
            self.assertEqual(got, [a, b, c])
            self.assertEqual(reliable, rel)
            self.assertEqual(trailer[3], 64000)  # stale prediction must not discard input
            self.assertEqual(cursor, len(body) - 5)

    def test_reference_captured_messages(self):
        captures = [(REF._WIRE_N5_BODY, REF._WIRE_N5_KEY)]
        for name, value in vars(REF).items():
            if name.startswith('_RUN_') and isinstance(value, list):
                captures += [(bytes.fromhex(raw), key) for raw, key in value]
        self.assertGreater(len(captures), 20)
        for body, key in captures:
            with self.subTest(key=key, bytes=len(body)):
                reference = REF.decode_usercmds(body, key, max_cmds=255)
                self.assertIsNotNone(reference)
                got, _, _, _ = read(body, key)
                frame = REF.clc_frame(body)
                self.assertIsNotNone(frame)
                cursor = REF._Msg(body, frame['uc_bit'], frame['count_idx'] + 1)
                old = dict(REF._NULLCMD)
                expected = []
                for _ in range(frame['count']):
                    old = REF._read_usercmd(cursor, key, old)
                    expected.append({k: old[k] for k in got[0]})
                self.assertEqual(got, expected)
                self.assertEqual([c['serverTime'] for c in got],
                                 [c['serverTime'] for c in reference['cmds']])
                self.assertEqual([c['buttons'] for c in got],
                                 [c['buttons'] for c in reference['cmds']])

    def test_command_count_and_truncation(self):
        a = dict(REF._NULLCMD, serverTime=300)
        for count in (1, 6, 32, 33, 64, 255):
            commands = [(dict(a, serverTime=300 + 12 * i), 'A') for i in range(count)]
            body = REF.build_client_message(37, [], commands)
            self.assertEqual(len(read(body, 37)[0]), count)
        a = dict(a, weapon=800, angles=(0, 14, 1023))
        body = REF.build_client_message(67, [], [(a, 'C')], tail=0)
        for size in range(len(body)):
            with self.assertRaises(ValueError): read(body[:size], 67)
        for raw in (b'', b'\0', b'\0\1'):
            with self.assertRaises(ValueError): read(raw, 67)

    def test_key_parity_against_reference(self):
        for text in (b'', b'hello', b'%\xff\x80', bytes(range(1, 100))):
            self.assertEqual(LIB.UsercmdKey(12345, 45678, text, len(text)),
                             REF.usercmd_key(12345, text, 45678))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--reference', required=True, type=Path)
    args, rest = parser.parse_known_args()
    spec = importlib.util.spec_from_file_location('codol_reference', args.reference / 'usercmd_codec.py')
    REF = importlib.util.module_from_spec(spec); spec.loader.exec_module(REF)
    with tempfile.TemporaryDirectory(prefix='codol-usercmd-') as tmp:
        target = Path(tmp) / ('cmd.dll' if os.name == 'nt' else 'cmd.so')
        subprocess.run([os.environ.get('CC', 'gcc'), *OL_INCLUDE, '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-shared', '-fPIC', '-O2', str(ROOT/'src/online/net/ol_msg.c'),
                        str(ROOT/'src/online/player/usercmd.c'), '-o', str(target)], check=True)
        LIB = C.CDLL(str(target)); ptr = C.POINTER(Msg)
        for name in ('ReadInit', 'WriteInit'):
            getattr(LIB, 'Msg' + name).argtypes = [ptr, C.c_void_p, C.c_size_t]
        LIB.ReadBits.argtypes = [ptr, C.c_uint]
        for name in ('Bits', 'Byte', 'Short', 'Long'):
            fun = getattr(LIB, 'Read' + name); fun.restype = C.c_uint32
            if name != 'Bits': fun.argtypes = [ptr]
            getattr(LIB, 'Write' + name).argtypes = [ptr, C.c_uint32] + ([C.c_uint] if name == 'Bits' else [])
        LIB.OL_ReadData.argtypes = [ptr, C.c_void_p, C.c_size_t]
        LIB.ReadUsercmd.argtypes = [ptr, C.c_uint32, C.POINTER(Cmd), C.POINTER(Cmd)]
        LIB.UsercmdKey.argtypes = [C.c_uint32, C.c_uint32, C.c_void_p, C.c_size_t]
        LIB.UsercmdKey.restype = C.c_uint32
        LIB.FilterUsercmd.argtypes = [C.POINTER(Cmd), C.POINTER(Cmd)]
        run = unittest.main(argv=[__file__] + rest, exit=False)
        if os.name == 'nt':
            import _ctypes
            _ctypes.FreeLibrary(LIB._handle)
    sys.exit(0 if run.result.wasSuccessful() else 1)
