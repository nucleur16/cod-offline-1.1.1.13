"""Copies the match server's source into third_party/sh1x/src, where scripts/build.sh builds it.

    python tools/vendor_server.py <server checkout>    (the codol_1.1.1.13_alpha_server repository)

Every file the server's git tracks at HEAD is copied as committed, except what is not source: its own submodules,
.gitmodules, tools/cod4_dasm/ (the CoD4 executables it disassembles and their disassembly), docs/captures/ (recorded
sessions), Mbed TLS's test certificates (mbedtls/certs.c) and binaries (.exe .dll .bin .zip ...). Strings naming C1.exe, which no install has any more, are pointed
at the client exe (SERVER_FIXES). The checkout must be clean; the old copy is replaced, the server's commit is written
to third_party/sh1x/src/VENDORED.txt.
"""
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = os.path.join(ROOT, "third_party", "sh1x", "src")
SKIP_DIRS = ("tools/cod4_dasm/", "docs/captures/")
# mbedtls/certs.*: Mbed TLS's test certificates and keys (nothing uses them; the makefile fix below drops certs.o)
SKIP_FILES = (".gitmodules", "src/mbedtls/certs.c", "src/mbedtls/certs.h")
CERT_EXT = {".pem", ".crt", ".cer", ".der", ".key", ".p12", ".pfx", ".csr"}
SKIP_EXT = {".exe", ".dll", ".bin", ".zip", ".so", ".a", ".lib", ".o", ".obj", ".pdb", ".iwd", ".ff"}
# (file, old, new, count)
SERVER_FIXES = [
    ("src/online/assets/client_crypto.c", '"%s/C1.exe"', '"%s/codoMP_client_shipRetail.exe"', 1),
    ("src/filesystem.c", '"CoDOL C1.exe or c1_keys.bin used for archive key derivation; empty uses fs_basepath/C1.exe, '
     'else fs_basepath/c1_keys.bin"', '"CoDOL client exe or c1_keys.bin used for archive key derivation; empty uses '
     'fs_basepath/codoMP_client_shipRetail.exe, else fs_basepath/c1_keys.bin"', 1),
    ("src/online/assets/asset_commands.c", "<C1.exe|- for inflated zones>", "<client exe|- for inflated zones>", 1),
    ("src/online/assets/asset_commands.c", "<file.ff> <C1.exe>", "<file.ff> <client exe>", 1),
    ("src/mbedtls/makefile", "OBJS_X509=\tcerts.o\t\tpkcs11.o", "OBJS_X509=\tpkcs11.o", 1),
    # the self-tests are the only users of the test certificates; nothing in the server runs them
    ("src/mbedtls/config.h", "\n#define MBEDTLS_SELF_TEST\n", "\n//#define MBEDTLS_SELF_TEST\n", 1),
    ("src/mbedtls/config.h", "\n#define MBEDTLS_CERTS_C\n", "\n//#define MBEDTLS_CERTS_C\n", 1),
]


def git(srv, *args):
    return subprocess.run(["git", "-C", srv] + list(args), check=True, capture_output=True).stdout


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    srv = os.path.abspath(sys.argv[1])
    if not os.path.isfile(os.path.join(srv, "makefile")):
        raise SystemExit("%s is not a server checkout (no makefile)" % srv)
    if git(srv, "status", "--porcelain", "--untracked-files=no").strip():
        raise SystemExit("%s has uncommitted changes: commit them first" % srv)
    files = []
    for entry in git(srv, "ls-files", "-s", "-z").decode("utf-8").split("\0"):
        if not entry:
            continue
        meta, path = entry.split("\t", 1)
        if meta.split()[0] == "160000" or path in SKIP_FILES or path.startswith(SKIP_DIRS):
            continue
        ext = os.path.splitext(path)[1].lower()
        if ext in SKIP_EXT or ext in CERT_EXT:
            continue
        files.append(path)
    fixes = {}
    for f, old, new, count in SERVER_FIXES:
        fixes.setdefault(f, []).append((old, new, count))
    if os.path.isdir(DEST):
        shutil.rmtree(DEST)
    for f in files:
        data = git(srv, "show", "HEAD:" + f)
        for old, new, count in fixes.pop(f, []):
            o, n = old.encode("utf-8"), new.encode("utf-8")
            if data.count(o) != count:
                raise SystemExit("%s: %r found %d times, expected %d" % (f, old, data.count(o), count))
            data = data.replace(o, n)
        d = os.path.join(DEST, f)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        with open(d, "wb") as fh:
            fh.write(data)
    if fixes:
        raise SystemExit("SERVER_FIXES name files that were not copied: %s" % ", ".join(fixes))
    # Mbed TLS ships no licence file in the server's copy; its Apache-2.0 text is in third_party/server_libs
    shutil.copyfile(os.path.join(ROOT, "third_party", "server_libs", "mbedtls-LICENSE"),
                    os.path.join(DEST, "src", "mbedtls", "LICENSE"))
    rev = git(srv, "rev-parse", "HEAD").decode().strip()
    with open(os.path.join(DEST, "VENDORED.txt"), "w", newline="\n") as fh:
        fh.write("codol_1.1.1.13_alpha_server %s, copied by tools/vendor_server.py\n" % rev)
    print("copied %d server files (commit %s) to third_party/sh1x/src" % (len(files), rev[:8]))


if __name__ == "__main__":
    main()
