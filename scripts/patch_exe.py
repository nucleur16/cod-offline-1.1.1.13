"""Write a codoMP_client_shipRetail.exe that loads Kernel33.dll, from a 1.1.1.13 client exe.

    python scripts/patch_exe.py <source exe> <destination exe>

The source is the stock packed client (its only import is Kernel32.dll; the name is changed to Kernel33.dll, same
length) or one that is already patched (copied as it is). scripts/make_exe.py does the same for the folder the
repository sits in and also writes src/unpacked.z; this one only makes the exe, for build.bat's dist folder.
"""
import sys

import pefile

BUILD_TIMESTAMP = 0x5314DEF0  # 1.1.1.13


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    pe = pefile.PE(src, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    if pe.FILE_HEADER.TimeDateStamp != BUILD_TIMESTAMP:
        sys.exit("%s is not the 1.1.1.13 client (timestamp %#x)" % (src, pe.FILE_HEADER.TimeDateStamp))
    imports = pe.DIRECTORY_ENTRY_IMPORT
    name = imports[0].dll.lower() if len(imports) == 1 else None
    if name not in (b"kernel32.dll", b"kernel33.dll"):
        sys.exit("unexpected import table: %r" % [d.dll for d in imports])
    name_off = pe.get_offset_from_rva(imports[0].struct.Name)
    pe.close()
    data = bytearray(open(src, "rb").read())
    data[name_off:name_off + 12] = b"Kernel33.dll"
    open(dst, "wb").write(data)
    print("wrote %s (%s)" % (dst, "already patched" if name == b"kernel33.dll" else "Kernel32.dll -> Kernel33.dll"))


if __name__ == "__main__":
    main()
