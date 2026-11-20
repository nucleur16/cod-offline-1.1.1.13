"""The symbols of the host library's relocatable object to make local (makefile target host-lib).

    python tools/sh1x_host_lib_symbols.py [--objdump OBJDUMP] [--keep PREFIX ...] sh1x_host_lib.o > list

Prints every external symbol the object defines, one per line as `objcopy --localize-symbols` reads them, except:
  - the host API (names starting with _Sh1xHost_, or --keep prefixes): what the client calls;
  - symbols in COMDAT (link-once) sections: C++ inline functions, template instances, vtables and type info that
    the client's own objects may define too; the linker keeps one copy of each, as in any C++ link.

Everything else the server defines (its engine, its zlib, its __cxa_atexit, the mbedtls/tomcrypt archives it was
linked with) then resolves only inside the server: it can neither clash with nor silently replace a symbol of the
program it is linked into. Undefined symbols (the CRT, Windows, libstdc++) stay as they are.
"""
import argparse
import re
import subprocess
import sys

SYM = re.compile(r"^\[\s*\d+\]\(sec\s+(-?\d+)\)\(fl [^)]*\)\(ty\s+[0-9a-fA-F]+\)\(scl\s+(\d+)\)\s+\(nx\s+\d+\)\s+0x[0-9a-fA-F]+\s+(\S+)")


def link_once_sections(objdump, obj):
    out = subprocess.run([objdump, "-h", obj], check=True, capture_output=True, text=True).stdout.splitlines()
    sections = set()
    for i, line in enumerate(out):
        parts = line.split()
        if len(parts) >= 7 and parts[0].isdigit():
            flags = out[i + 1] if i + 1 < len(out) else ""
            if "LINK_ONCE" in flags:
                sections.add(int(parts[0]) + 1)  # objdump -h counts from 0, the symbol table from 1
    return sections


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--objdump", default="objdump")
    ap.add_argument("--keep", action="append", default=["_Sh1xHost_"])
    ap.add_argument("obj")
    a = ap.parse_args(argv[1:])
    once = link_once_sections(a.objdump, a.obj)
    table = subprocess.run([a.objdump, "-t", a.obj], check=True, capture_output=True, text=True).stdout.splitlines()
    names = set()
    kept = 0
    for line in table:
        m = SYM.match(line)
        if not m:
            continue
        sec, scl, name = int(m.group(1)), int(m.group(2)), m.group(3)
        if scl != 2 or sec <= 0:  # external definitions only (scl 2 = C_EXT; sec 0 = undefined/common)
            continue
        if sec in once or any(name.startswith(k) for k in a.keep):
            kept += 1
            continue
        names.add(name)
    if not any(n for n in names):
        print("sh1x_host_lib_symbols: no symbols found (wrong objdump?)", file=sys.stderr)
        return 1
    sys.stdout.write("".join(n + "\n" for n in sorted(names)))
    print(f"sh1x_host_lib_symbols: {len(names)} symbols made local, {kept} kept global", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
