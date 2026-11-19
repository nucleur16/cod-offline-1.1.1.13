"""Write the public source tree: the tracked files of this repository minus what only builds from private sources or
needs a dumped client image, with comment wording that names private projects replaced (EXCLUDE, RULES).

    python tools/export_public.py <dest folder>

scripts/gsc stays byte-identical to the script repository in this repository; the replacements are applied to the
exported copy only (comments only in source files: // and # lines, /* */ blocks, docstrings are left to the
whole-line rules below because the matched words occur only in comments there). After writing, every exported text
file is searched for the FORBIDDEN words and the export fails if one is left.

The match server's source is vendored: the files of the server/sh1x submodule's checked-out commit that
`make host-lib` needs (SERVER_PATHS minus SERVER_SKIP) are written to third_party/sh1x/src, where scripts/build.sh
builds it, and the prebuilt third_party/sh1x/libsh1x_host.a is left out. Its few code strings that name private
things are rewritten by SERVER_FIXES (each must match exactly the given number of times), then the RULES apply.
"""
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EXCLUDE = (
    "server/", ".gitmodules", "tests/", "tools/", "third_party/server_libs/",
    "scripts/build-server.sh", "scripts/gen_sh1x_cmds.py", "scripts/gen_dvars.py", "scripts/gen_tables.py",
    "scripts/make_exe.py", "scripts/ui_keys.py", "src/offline/tools/", "src/offline/PORTING.md",
    "scripts/gsc/COLLATION_CONFLICTS.txt", "scripts/gsc/COLLATION_MANIFEST.json",
    "third_party/sh1x/libsh1x_host.a",
)
SERVER = "server/sh1x"
SERVER_DEST = "third_party/sh1x/src"
SERVER_PATHS = ("makefile", "src", "lib/libs.txt", "tools/sh1x_host_lib_symbols.py", "plugins/plugin_declarations.h",
                "plugins/callback_declarations.h", "plugins/declarations.h", "plugins/function_declarations.h",
                "plugins/pinc.h", "License.md", "Copyright.md", "copying.md")
# not compiled by host-lib: the old bot library and BSP compiler, unused code, stray tool binaries
SERVER_SKIP = ("src/bspc/", "src/botlib/", "src/futureusecode/", "src/tmp_mathlibackup/", "src/tomcrypt/codol_")
# (file, old, new, count): code strings, applied before RULES
SERVER_FIXES = [
    ("src/online/assets/client_crypto.c", '"%s/C1.exe"', '"%s/codoMP_client_shipRetail.exe"', 1),
    ("src/filesystem.c", '"CoDOL C1.exe or c1_keys.bin used for archive key derivation; empty uses fs_basepath/C1.exe, '
     'else fs_basepath/c1_keys.bin"', '"CoDOL client exe or c1_keys.bin used for archive key derivation; empty uses '
     'fs_basepath/codoMP_client_shipRetail.exe, else fs_basepath/c1_keys.bin"', 1),
    ("src/online/assets/asset_commands.c", "<C1.exe|- for inflated zones>", "<client exe|- for inflated zones>", 1),
    ("src/online/assets/asset_commands.c", "<file.ff> <C1.exe>", "<file.ff> <client exe>", 1),
    ("src/db_load.cpp", '"codol_rawfiles/%s"', '"rawfile_dump/%s"', 1),
]

# (pattern, replacement), applied in order to the text of every exported text file
RULES = [
    (r"\(Python\)", ""),
    (r"[Tt]he CODOL-EMU 1\.1\.1\.13 Demonware emulator", "the reference Demonware emulator"),
    (r"CODOL-EMU 1\.1\.1\.13 Demonware emulator", "the reference Demonware emulator"),
    (r"CODOL-EMU/13-rawfiles", "the script repository"),
    (r"CODOL-EMU", "the reference project"),
    (r"13-rawfiles", "the script repository"),
    (r"demonware/([A-Za-z0-9_]+)\.py", r"the reference lobby's \1"),
    (r"notices/([A-Za-z0-9_]+)\.py", r"the reference lobby's \1"),
    (r"career/([A-Za-z0-9_]+)\.py", r"the reference lobby's \1"),
    (r"Python lobby", "reference lobby"),
    (r"C1\.exe's", "C1's"),
    (r"C1\.exe", "C1"),
    (r"C1\.dll", "C1's loader"),
    (r"dvars_c1exe\.md \(alpha0022 docs\)", "the reference dvar list"),
    (r"alpha0022 zones/zone_assets\.py", "the reference zone reader"),
    (r"CODOL[\\/]+1\.1\.1\.13_server[\\/]+alpha0022", "the reference server"),
    (r"alpha0022", "the reference server"),
    (r"codol-classic-zz", "an older install"),
    (r"codol-classic's", "the classic build's"),
    (r"codol-classic", "the classic build"),
    (r"bin/home/codol_rawfiles", "the server's script tree"),
    (r"codol_rawfiles", "the server's script tree"),
    (r"tools/english/gfx_font\.py \(CoD4x_Server / localizedStrings-1\.1\.1\.13\)", "an earlier font tool"),
    (r"CoD4x_Server", "CoD4x server"),
    (r"D:[\\/]+CoD[\\/]+[^\s\"'`)]*", "<path>"),
    (r"k33work", "<path>"),
]
FORBIDDEN = re.compile(r"c1\.exe|c1\.dll|CODOL-EMU|13-rawfiles|alpha0022|codol-classic|CoD4x_Server|codol_rawfiles|"
                       r"Python lobby|nucleur16/(?!sh1xo-radiant)|k33work|(?-i:CODOL Builds)|codol_1\.1\.1\.13_alpha_server",
                       re.I)
# the server keeps its upstream's name (CoD4x_Server, in its URLs and notices)
SERVER_RULES = [r for r in RULES if r[0] != r"CoD4x_Server"]
SERVER_FORBIDDEN = re.compile(FORBIDDEN.pattern.replace("CoD4x_Server|", ""), re.I)
TEXT_EXT = {".c", ".h", ".cpp", ".inc", ".py", ".sh", ".bat", ".md", ".txt", ".gsc", ".csc", ".gsh", ".csv",
            ".as", ".json", ".rc", ".def", ".manifest", ".cfg", ".str", ".menu", ".inc", ".atr", ".arena", ""}


def tracked():
    out = subprocess.run(["git", "-C", ROOT, "ls-files", "-z", "--cached"], check=True, capture_output=True).stdout
    return [f for f in out.decode("utf-8").split("\0") if f]


def excluded(f):
    return f == "server/sh1x" or any(f == p.rstrip("/") or f.startswith(p) for p in EXCLUDE)


def scrub(text, rules=None):
    for pat, rep in (RULES if rules is None else rules):
        text = re.sub(pat, rep, text)
    return text


def server_files():
    """(path in the server, mode) of the server files to vendor; gitlinks (its own submodules) are skipped."""
    out = subprocess.run(["git", "-C", os.path.join(ROOT, SERVER), "ls-files", "-s", "-z", "--"] + list(SERVER_PATHS),
                         check=True, capture_output=True).stdout
    files = []
    for entry in out.decode("utf-8").split("\0"):
        if not entry:
            continue
        meta, path = entry.split("\t", 1)
        if meta.split()[0] == "160000" or path.startswith(SERVER_SKIP):
            continue
        files.append(path)
    return files


def vendor_server(dest):
    """Write the server's source to dest/SERVER_DEST; returns (files written, names still left)."""
    srv = os.path.join(ROOT, SERVER)
    if not os.path.isfile(os.path.join(srv, "makefile")):
        raise SystemExit("%s is not checked out (git submodule update --init %s)" % (SERVER, SERVER))
    if subprocess.run(["git", "-C", srv, "status", "--porcelain", "--untracked-files=no"], check=True,
                      capture_output=True).stdout.strip():
        raise SystemExit("%s has uncommitted changes: commit them or reset it first" % SERVER)
    fixes = {}
    for f, old, new, count in SERVER_FIXES:
        fixes.setdefault(f, []).append((old, new, count))
    left = []
    n = 0
    for f in server_files():
        data = subprocess.run(["git", "-C", srv, "show", "HEAD:" + f], check=True, capture_output=True).stdout
        if b"\0" not in data[:4096]:
            try:
                text = data.decode("utf-8")
            except UnicodeDecodeError:
                text = data.decode("latin-1")
                enc = "latin-1"
            else:
                enc = "utf-8"
            for old, new, count in fixes.pop(f, []):
                if text.count(old) != count:
                    raise SystemExit("%s/%s: %r found %d times, expected %d" % (SERVER, f, old, text.count(old), count))
                text = text.replace(old, new)
            text = scrub(text, SERVER_RULES)
            left += ["%s/%s: %s" % (SERVER_DEST, f, m.group(0)) for m in SERVER_FORBIDDEN.finditer(text)][:3]
            data = text.encode(enc)
        d = os.path.join(dest, SERVER_DEST, f)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        open(d, "wb").write(data)
        n += 1
    if fixes:
        raise SystemExit("SERVER_FIXES name files that are not vendored: %s" % ", ".join(fixes))
    os.makedirs(os.path.join(dest, SERVER_DEST, "bin"), exist_ok=True)
    # Mbed TLS ships no licence file in the server's copy; its Apache-2.0 text is in third_party/server_libs
    shutil.copyfile(os.path.join(ROOT, "third_party", "server_libs", "mbedtls-LICENSE"),
                    os.path.join(dest, SERVER_DEST, "src", "mbedtls", "LICENSE"))
    rev = subprocess.run(["git", "-C", srv, "rev-parse", "--short", "HEAD"], check=True,
                         capture_output=True).stdout.decode().strip()
    print("vendored %d server files (commit %s) to %s" % (n, rev, SERVER_DEST))
    return n, left


def main():
    dest = os.path.abspath(sys.argv[1])
    if os.path.exists(dest):
        shutil.rmtree(dest)
    n = changed = 0
    left = []
    for f in tracked():
        if excluded(f):
            continue
        src = os.path.join(ROOT, f)
        if not os.path.isfile(src):
            continue
        d = os.path.join(dest, f)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        data = open(src, "rb").read()
        ext = os.path.splitext(f)[1].lower()
        # .as sources are compiled into stored tags checked by sha256 (zonekit/gfx.py): never rewritten here
        if ext in TEXT_EXT and ext != ".as" and b"\0" not in data[:4096]:
            try:
                text = data.decode("utf-8")
            except UnicodeDecodeError:
                text = None
            if text is not None:
                new = scrub(text)
                if new != text:
                    changed += 1
                    data = new.encode("utf-8")
                if FORBIDDEN.search(new):
                    left += ["%s: %s" % (f, m.group(0)) for m in FORBIDDEN.finditer(new)][:3]
        elif ext == ".as" and FORBIDDEN.search(data.decode("utf-8", "replace")):
            left.append("%s (.as, not rewritten)" % f)
        open(d, "wb").write(data)
        n += 1
    print("exported %d files to %s (%d reworded)" % (n, dest, changed))
    left += vendor_server(dest)[1]
    # the UI baseline pins file hashes: drop the files the export leaves out (development artifacts), then record
    # the hashes the rewording changed
    import json
    bp = os.path.join(dest, "ui_baseline.json")
    base = json.load(open(bp, encoding="utf-8"))
    base["files"] = {k: v for k, v in base["files"].items() if os.path.isfile(os.path.join(dest, k))}
    open(bp, "w", encoding="utf-8", newline="\n").write(json.dumps(base, indent=2, ensure_ascii=False) + "\n")
    subprocess.run([sys.executable, "-m", "zonekit.ui_baseline", "--record"], cwd=dest, check=True)
    if left:
        print("still named:\n  " + "\n  ".join(left))
        sys.exit(1)


if __name__ == "__main__":
    main()
