"""Read-only exact-image disassembly helper for local C1 security review."""
import argparse
import hashlib
from pathlib import Path
import struct
import sys

def image_section(raw, va):
    if raw[:2] != b"MZ":
        raise ValueError("not a PE image")
    pe = struct.unpack_from("<I", raw, 0x3C)[0]
    if raw[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    n = struct.unpack_from("<H", raw, pe + 6)[0]
    optional = pe + 24
    header = struct.unpack_from("<H", raw, pe + 20)[0]
    base = struct.unpack_from("<I", raw, optional + 28)[0]
    rva = va - base
    for i in range(n):
        pos = optional + header + i * 40
        name = raw[pos:pos + 8].split(b"\0")[0].decode("ascii", "replace")
        virtual, address, size, offset = struct.unpack_from("<IIII", raw, pos + 8)
        if address <= rva < address + min(virtual, size):
            return name, raw[offset + rva - address:offset + size]
    raise ValueError("address outside file-backed section")


def direct_calls(raw, target):
    """List raw E8 rel32 candidates; inspect context to discard embedded bytes."""
    pe = struct.unpack_from("<I", raw, 0x3C)[0]
    optional = pe + 24
    base = struct.unpack_from("<I", raw, optional + 28)[0]
    n = struct.unpack_from("<H", raw, pe + 6)[0]
    header = struct.unpack_from("<H", raw, pe + 20)[0]
    for i in range(n):
        pos = optional + header + i * 40
        name = raw[pos:pos + 8].split(b"\0")[0]
        if name != b".text":
            continue
        _, address, size, offset = struct.unpack_from("<IIII", raw, pos + 8)
        code = raw[offset:offset + size]
        for at in range(len(code) - 4):
            if code[at] == 0xE8 and base + address + at + 5 + struct.unpack_from("<i", code, at + 1)[0] == target:
                yield base + address + at


def dword_refs(raw, target):
    """List raw little-endian dword candidates in .text, including operands."""
    pe = struct.unpack_from("<I", raw, 0x3C)[0]
    optional = pe + 24
    base = struct.unpack_from("<I", raw, optional + 28)[0]
    n = struct.unpack_from("<H", raw, pe + 6)[0]
    header = struct.unpack_from("<H", raw, pe + 20)[0]
    needle = struct.pack("<I", target)
    for i in range(n):
        pos = optional + header + i * 40
        if raw[pos:pos + 8].split(b"\0")[0] != b".text":
            continue
        _, address, size, offset = struct.unpack_from("<IIII", raw, pos + 8)
        code = raw[offset:offset + size]
        at = code.find(needle)
        while at >= 0:
            yield base + address + at
            at = code.find(needle, at + 1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("address", type=lambda value: int(value, 0))
    parser.add_argument("--count", type=int, default=80)
    parser.add_argument("--xrefs", action="store_true", help="raw direct-call candidates")
    parser.add_argument("--refs", action="store_true", help="raw dword reference candidates")
    parser.add_argument("--bytes", type=int, default=0, help="print first N mapped bytes")
    parser.add_argument("--string", action="store_true", help="print a NUL-terminated mapped string")
    args = parser.parse_args()
    raw = args.image.read_bytes()
    print("sha256", hashlib.sha256(raw).hexdigest())
    if args.xrefs:
        for address in direct_calls(raw, args.address):
            print(f"call candidate {address:08x} -> {args.address:08x}")
        return
    if args.refs:
        for address in dword_refs(raw, args.address):
            print(f"dword candidate {address:08x} -> {args.address:08x}")
        return
    name, code = image_section(raw, args.address)
    print("section", name)
    if args.bytes:
        print("mapped bytes", len(code), code[:args.bytes].hex(" "))
    if args.string:
        print("mapped string", repr(code.split(b"\0", 1)[0][:256].decode("utf-8", "replace")))
        return
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "analysis_deps"))
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    for insn in decoder.disasm(code, args.address, count=args.count):
        print(f"{insn.address:08x}: {insn.mnemonic:<8} {insn.op_str}")


if __name__ == "__main__":
    main()
