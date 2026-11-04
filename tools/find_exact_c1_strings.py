"""Read-only PE string address and raw code-reference locator."""
import argparse
import hashlib
from pathlib import Path
import struct

from audit_exact_c1_pe import dword_refs


def sections(raw):
    pe = struct.unpack_from("<I", raw, 0x3C)[0]
    if raw[:2] != b"MZ" or raw[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    n = struct.unpack_from("<H", raw, pe + 6)[0]
    optional = pe + 24
    header = struct.unpack_from("<H", raw, pe + 20)[0]
    base = struct.unpack_from("<I", raw, optional + 28)[0]
    for i in range(n):
        pos = optional + header + i * 40
        name = raw[pos:pos + 8].split(b"\0")[0].decode("ascii", "replace")
        _, address, size, offset = struct.unpack_from("<IIII", raw, pos + 8)
        yield name, base + address, raw[offset:offset + size]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--substring", action="store_true", help="find embedded text without requiring a trailing NUL")
    parser.add_argument("--all-refs", action="store_true", help="also report raw pointer candidates outside .text")
    parser.add_argument("strings", nargs="+")
    args = parser.parse_args()
    raw = args.image.read_bytes()
    print("sha256", hashlib.sha256(raw).hexdigest())
    for query in args.strings:
        needle = query.encode("utf-8") + (b"" if args.substring else b"\0")
        print(repr(query))
        for name, va, data in sections(raw):
            at = data.find(needle)
            while at >= 0:
                address = va + at
                refs = ", ".join(f"{ref:08x}" for ref in dword_refs(raw, address))
                if args.all_refs:
                    pointer = struct.pack("<I", address)
                    other = []
                    for ref_section, ref_va, ref_data in sections(raw):
                        if ref_section == ".text":
                            continue
                        at_ref = ref_data.find(pointer)
                        while at_ref >= 0:
                            other.append(f"{ref_section}:{ref_va + at_ref:08x}")
                            at_ref = ref_data.find(pointer, at_ref + 1)
                    if other:
                        refs = ", ".join(filter(None, [refs, *other]))
                print(f"  {name} {address:08x} refs [{refs}]")
                at = data.find(needle, at + 1)


if __name__ == "__main__":
    main()
