"""Build codoMP_client_shipRetail.exe (1.1.1.13) that loads Kernel33.dll, and unpacked.z.

Copies the pristine packed 1.1.1.13 client (codoMP_client_shipRetail.exe.bak, same build as
C1.exe) and renames its only import, Kernel32.dll, to Kernel33.dll (same length, so no
layout changes). The Authenticode signature is invalidated, which Windows does not enforce
for an exe.

unpacked.z holds the packer section as C1.exe has it, i.e. after TenProtect's loader has
decompressed the game code into it (VA 0x08849000-0x09A10000), zlib-compressed. build.sh embeds
it in Kernel33.dll as a resource, and the DLL inflates it over the packed exe's memory instead of
letting TenProtect run.

    python Kernel33/make_exe.py    # writes ../codoMP_client_shipRetail.exe and Kernel33/unpacked.z
"""
import os
import shutil
import struct
import sys
import zlib

import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "codoMP_client_shipRetail.exe.bak")
DST = os.path.join(ROOT, "codoMP_client_shipRetail.exe")
C1 = os.path.join(ROOT, "C1.exe")
IMG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "unpacked.z")
BUILD_TIMESTAMP = 0x5314DEF0  # 1.1.1.13, identical to C1.exe
UNPACKED_VA, UNPACKED_END = 0x08849000, 0x09A10000  # must match kernel33.c


def write_image():
    """unpacked.z: 'K33Z', build timestamp, VA, size, then C1.exe's bytes for that range, zlib."""
    pe = pefile.PE(C1, fast_load=True)
    if pe.FILE_HEADER.TimeDateStamp != BUILD_TIMESTAMP:
        sys.exit("%s is not the 1.1.1.13 build" % C1)
    base = pe.OPTIONAL_HEADER.ImageBase
    size = UNPACKED_END - UNPACKED_VA
    body = pe.get_memory_mapped_image()[UNPACKED_VA - base:UNPACKED_END - base]
    pe.close()
    if len(body) != size:
        sys.exit("C1.exe image too short for %#x-%#x" % (UNPACKED_VA, UNPACKED_END))
    with open(IMG, "wb") as f:
        packed = zlib.compress(body, 9)
        f.write(struct.pack("<4sIII", b"K33Z", BUILD_TIMESTAMP, UNPACKED_VA, size))
        f.write(packed)
    print("wrote %s (%#x bytes at %#x, %d compressed)" % (IMG, size, UNPACKED_VA, len(packed)))


def main():
    pe = pefile.PE(SRC, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    if pe.FILE_HEADER.TimeDateStamp != BUILD_TIMESTAMP:
        sys.exit("%s is not the 1.1.1.13 build (timestamp %#x)" % (SRC, pe.FILE_HEADER.TimeDateStamp))
    imports = pe.DIRECTORY_ENTRY_IMPORT
    if len(imports) != 1 or imports[0].dll.lower() != b"kernel32.dll":
        sys.exit("unexpected import table: %r" % [d.dll for d in imports])
    name_off = pe.get_offset_from_rva(imports[0].struct.Name)
    pe.close()

    data = bytearray(open(SRC, "rb").read())
    assert data[name_off:name_off + 12].lower() == b"kernel32.dll"
    data[name_off:name_off + 12] = b"Kernel33.dll"

    if os.path.exists(DST):
        existing = pefile.PE(DST, fast_load=True)
        stamp = existing.FILE_HEADER.TimeDateStamp
        existing.close()
        if stamp != BUILD_TIMESTAMP:
            keep = os.path.join(ROOT, "codoMP_client_shipRetail.%#x.exe" % stamp)
            shutil.move(DST, keep)
            print("kept the other build as", keep)
    open(DST, "wb").write(data)
    print("wrote %s (import name at file offset %#x)" % (DST, name_off))
    write_image()


if __name__ == "__main__":
    main()
