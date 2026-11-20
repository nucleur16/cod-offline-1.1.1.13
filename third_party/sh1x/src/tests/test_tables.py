"""Validate native string-table parsing against all cached shipped zone tables."""
import argparse
import ctypes as C
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import pathlib as _ol_pl
_ol_src=_ol_pl.Path(__file__).resolve().parents[1]/'src'
OL_INCLUDE=['-I'+str(_ol_src)]+['-I'+str(d) for d in sorted((_ol_src/'online').iterdir()) if d.is_dir()]

class Asset(C.Structure):
    _fields_ = [('type', C.c_uint32), ('data', C.c_void_p), ('size', C.c_size_t)]

class Cell(C.Structure):
    _fields_ = [('reference', C.c_uint32), ('hash', C.c_uint32), ('text', C.c_char_p)]

class Table(C.Structure):
    _fields_ = [('storage', C.c_void_p), ('name', C.c_char_p), ('name_reference', C.c_uint32),
                ('values_reference', C.c_uint32), ('columns', C.c_uint32), ('rows', C.c_uint32),
                ('unresolved', C.c_uint32), ('cells', C.POINTER(Cell))]

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('zones', type=Path)
    args = parser.parse_args()
    total = inline = unresolved = 0
    with tempfile.TemporaryDirectory(prefix='codol-tables-') as directory:
        target = Path(directory)/('tables.dll' if os.name == 'nt' else 'tables.so')
        source = Path(__file__).resolve().parents[1]/'src/online/assets/ol_stringtable.c'
        subprocess.run([os.environ.get('CC', 'gcc'), *OL_INCLUDE, '-shared', '-fPIC', '-O2', '-Wall', '-Wextra',
                        '-Werror', str(source), '-o', str(target)], check=True)
        lib = C.CDLL(str(target))
        lib.ReadStringTable.argtypes = [C.POINTER(Asset), C.POINTER(Table)]
        lib.FreeStringTable.argtypes = [C.POINTER(Table)]
        for path in sorted(args.zones.glob('*.zone')):
            data = path.read_bytes()
            strings, _, assets = struct.unpack_from('<III', data, 40)
            pos = 56 + strings*4
            for i in range(strings):
                if struct.unpack_from('<I', data, 56+i*4)[0] == 0xffffffff:
                    pos = data.index(0, pos)+1
            for i in range(assets):
                kind, offset, _, size = struct.unpack_from('<IIII', data, pos+16*i)
                if kind != 37:
                    continue
                blob = data[offset+40:offset+40+size]
                buffer = C.create_string_buffer(blob)
                asset = Asset(kind, C.addressof(buffer), len(blob)); table = Table()
                if not lib.ReadStringTable(C.byref(asset), C.byref(table)):
                    if os.name == 'nt':
                        import _ctypes
                        _ctypes.FreeLibrary(lib._handle)
                    raise AssertionError((path.name, i))
                total += 1
                unresolved += table.unresolved
                inline += table.rows*table.columns-table.unresolved
                lib.FreeStringTable(C.byref(table))
        assert total, 'No tables tested'
        print(f'PASS: {total} shipped tables; {inline} hash-checked inline cells; {unresolved} explicit pooled references')
        if os.name == 'nt':
            import _ctypes
            _ctypes.FreeLibrary(lib._handle)
