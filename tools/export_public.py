"""Write the public source tree: the tracked files of this repository minus what only builds from private sources or
needs a dumped client image, with comment wording that names private projects replaced (EXCLUDE, RULES).

    python tools/export_public.py <dest folder>

scripts/gsc stays byte-identical to the script repository in this repository; the replacements are applied to the
exported copy only (comments only in source files: // and # lines, /* */ blocks, docstrings are left to the
whole-line rules below because the matched words occur only in comments there). After writing, every exported text
file is searched for the FORBIDDEN words and the export fails if one is left.
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
)
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
TEXT_EXT = {".c", ".h", ".cpp", ".inc", ".py", ".sh", ".bat", ".md", ".txt", ".gsc", ".csc", ".gsh", ".csv",
            ".as", ".json", ".rc", ".def", ".manifest", ".cfg", ".str", ".menu", ".inc", ".atr", ".arena", ""}


def tracked():
    out = subprocess.run(["git", "-C", ROOT, "ls-files", "-z", "--cached"], check=True, capture_output=True).stdout
    return [f for f in out.decode("utf-8").split("\0") if f]


def excluded(f):
    return f == "server/sh1x" or any(f == p.rstrip("/") or f.startswith(p) for p in EXCLUDE)


def scrub(text):
    for pat, rep in RULES:
        text = re.sub(pat, rep, text)
    return text


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
    if left:
        print("still named:\n  " + "\n  ".join(left))
        sys.exit(1)


if __name__ == "__main__":
    main()
