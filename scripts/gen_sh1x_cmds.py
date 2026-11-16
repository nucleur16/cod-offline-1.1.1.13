#!/usr/bin/env python3
"""Generates src/console/sh1x_cmds.h: what the embedded match server (the submodule server/sh1x) understands,
for the client console's completion, help and command routing (docs/OFFLINE_CONSOLE_20261112.md).

    python scripts/gen_sh1x_cmds.py            write src/console/sh1x_cmds.h
    python scripts/gen_sh1x_cmds.py --check    exit 1 when the committed header differs from the server source

Read from the server SOURCE (not from a running server):
  commands  every Cmd_AddCommand / Cmd_AddPCommand / Cmd_AddHCommand / Cmd_AddCommandGeneric("name", ...) literal
  dvars     every Cvar_Register<Type>("name", ...) literal, with its description when the last argument is one;
            plus the dvars `devset` may write (dev_cheats.c dev_dvars: some are created by GSC, not C)
  cheats    the client command verbs of the developer set: the list cmd_devhelp prints plus devhelp itself,
            each checked to be dispatched by ClientCommand (g_cmds.c) or DevClientCommand
  usage     the server's own usage strings for give, take, setviewpos and devset
  devset    the dev_dvars names, in the server's order

The table is a superset in one direction and a subset in the other: a command registered under an #ifdef the
host build leaves out is listed, one registered from retained assembly or by a script is not. A running host is
asked for its real lists (rcon cmdlist / cvarlist); this table serves before a match and when that fails.
tests/test_offline_console.py runs --check, so the header cannot drift from the pinned submodule unnoticed.

Names are written as the client spells them: the source is read through tools/drop_codol_prefix.py server_text(),
which spells the server's old codol_* commands and dvars by the server's rename rule (the prefix dropped:
devServer, dev_cheats, ...). Once the pinned server registers the new names that is a no-op.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SERVER = ROOT / "server" / "sh1x"
OUT = ROOT / "src" / "console" / "sh1x_cmds.h"
sys.path.insert(0, str(ROOT / "tools"))
import drop_codol_prefix  # noqa: E402  (the server's names in the client's spelling)

CMD_RE = re.compile(r'\bCmd_Add(?:P|H)?Command(?:Generic)?\s*\(\s*"([^"\\]+)"')
DVAR_RE = re.compile(r'\bCvar_Register(Bool|Int|Float|String|Enum|Color|Vec2|Vec3|Vec4)\s*\(\s*"([^"\\]+)"')
STR_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')
NAME_OK = re.compile(r'^[A-Za-z_][A-Za-z0-9_.+-]*$')


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text, flags=re.S)
    return re.sub(r'(?m)^([^"\n]*?)//[^\n]*$', r'\1', text)


def server_source(path):
    """A server source file without comments, its codol_* names spelled the client's way."""
    return drop_codol_prefix.server_text(strip_comments(path.read_text(encoding="latin-1")))


def dev_cheats_source():
    """The server's developer-cheat module (src/online/player/dev_cheats.c; older pins have it under its codol_ name)."""
    hits = sorted((SERVER / "src").rglob("*dev_cheats.c"))
    if len(hits) != 1:
        raise SystemExit("gen_sh1x_cmds: expected one *dev_cheats.c in server/sh1x/src, found %s" % hits)
    return hits[0]


def call_text(text, start):
    """The argument text of the call whose '(' is at or after start, up to its matching ')'."""
    i = text.index('(', start)
    depth, j, in_str = 0, i, False
    while j < len(text):
        ch = text[j]
        if in_str:
            if ch == '\\':
                j += 1
            elif ch == '"':
                in_str = False
        elif ch == '"':
            in_str = True
        elif ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
            if not depth:
                return text[i + 1:j]
        j += 1
    return text[i + 1:]


def description(args):
    """The trailing string literal(s) of a Cvar_Register call, when the last argument is a literal."""
    tail = args.rstrip()
    m = re.search(r'((?:"(?:[^"\\]|\\.)*"\s*)+)$', tail)
    if not m or len(STR_RE.findall(tail)) < 2:
        return ""
    # the literal run must be a whole argument (preceded by a comma)
    if not tail[:m.start()].rstrip().endswith(','):
        return ""
    text = "".join(STR_RE.findall(m.group(1)))
    text = re.sub(r'\\n|\\t', ' ', text).replace('\\"', "'").replace('\\', '')
    text = re.sub(r'\s+', ' ', text).strip()
    text = "".join(c for c in text if 32 <= ord(c) < 127 and c != '"')
    return text[:110]


def scan():
    cmds, dvars = set(), {}
    for path in sorted((SERVER / "src").rglob("*.c")):
        text = server_source(path)
        for m in CMD_RE.finditer(text):
            if NAME_OK.match(m.group(1)):
                cmds.add(m.group(1))
        for m in DVAR_RE.finditer(text):
            name = m.group(2)
            if not NAME_OK.match(name):
                continue
            desc = description(call_text(text, m.start()))
            if name not in dvars or (desc and not dvars[name][1]):
                dvars[name] = (m.group(1).lower(), desc)
    return cmds, dvars


def cheats():
    dev = server_source(dev_cheats_source())
    gcmds = server_source(SERVER / "src/g_cmds.c")
    m = re.search(r'\w*DevPrint\(ent,\s*"dev: ([a-z ]+)"\)', dev)
    if not m:
        raise SystemExit("gen_sh1x_cmds: cmd_devhelp's list was not found in %s" % dev_cheats_source().name)
    verbs = m.group(1).split() + ["devhelp"]
    body = dev[re.search(r'\bint\s+\w*DevClientCommand\b', dev).start():]
    own = set(re.findall(r'Q_stricmp\(verb,\s*"([a-z]+)"\)', body))
    game = set(re.findall(r'Q_stricmp\(\s*cmd,\s*"([a-z_]+)"\s*\)', gcmds[gcmds.index("void ClientCommand"):]))
    for v in verbs:
        if v not in own and v not in game:
            raise SystemExit("gen_sh1x_cmds: devhelp lists '%s' but neither dispatcher handles it" % v)
    extra = own - set(verbs)
    if extra:
        raise SystemExit("gen_sh1x_cmds: the server's DevClientCommand handles %s, which devhelp does not list" % sorted(extra))
    usage = {}
    for verb in ("give", "take", "devset"):
        u = re.search(r'"usage: (%s [^";]+)[";]' % verb, dev)
        if not u:
            raise SystemExit("gen_sh1x_cmds: no usage string for %s" % verb)
        usage[verb] = u.group(1).strip()
    u = re.search(r'\w*DevPrint\(\s*ent,\s*"usage: (setviewpos [^"]+)"', gcmds)
    if not u:
        raise SystemExit("gen_sh1x_cmds: no usage string for setviewpos")
    usage["setviewpos"] = u.group(1).strip()
    table = dev[dev.index("static dev_dvar_t dev_dvars[]"):]
    table = table[:table.index("};")]
    devset = re.findall(r'\{\s*"([A-Za-z_]+)"\s*,\s*"((?:[^"\\]|\\.)*)"', table)
    if not devset:
        raise SystemExit("gen_sh1x_cmds: dev_dvars is empty")
    return verbs, usage, devset


def pin():
    out = subprocess.run(["git", "-C", str(ROOT), "ls-tree", "HEAD", "server/sh1x"], capture_output=True, text=True)
    parts = out.stdout.split()
    return parts[2] if len(parts) >= 3 else "unknown"


def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def render():
    cmds, dvars = scan()
    verbs, usage, devset = cheats()
    for name, what in devset:
        dvars.setdefault(name, ("devset", what))
    lines = [
        "/* Generated by scripts/gen_sh1x_cmds.py from the server source in server/sh1x - do not edit.",
        " * Server submodule commit: %s" % pin(),
        " * What the embedded sh1x match server understands (docs/OFFLINE_CONSOLE_20261112.md). A running host's own",
        " * cmdlist / cvarlist replaces the command and dvar tables; tests/test_offline_console.py fails when this",
        " * header differs from what the pinned server source yields. */",
        "#ifndef K33_SH1X_CMDS_H",
        "#define K33_SH1X_CMDS_H",
        "",
        "typedef struct { const char *name, *desc; } Sh1xName;",
        "",
        "/* server console commands (sorted, case-insensitive) */",
        "static const Sh1xName kSh1xCmds[] = {",
    ]
    for name in sorted(cmds, key=lambda s: (s.lower(), s)):
        lines.append("    { %s, \"\" }," % cstr(name))
    lines += ["};", "#define SH1X_CMD_COUNT (sizeof(kSh1xCmds) / sizeof(kSh1xCmds[0]))", "",
              "/* server dvars (sorted, case-insensitive): name, description from the registration */",
              "static const Sh1xName kSh1xDvars[] = {"]
    for name in sorted(dvars, key=lambda s: (s.lower(), s)):
        lines.append("    { %s, %s }," % (cstr(name), cstr(dvars[name][1])))
    lines += ["};", "#define SH1X_DVAR_COUNT (sizeof(kSh1xDvars) / sizeof(kSh1xDvars[0]))", "",
              "/* the developer/cheat client commands, in cmd_devhelp's order; usage is the server's own string, \"\" where",
              " * the command takes no argument */",
              "static const Sh1xName kSh1xCheats[] = {"]
    for v in verbs:
        lines.append("    { %s, %s }," % (cstr(v), cstr(usage.get(v, ""))))
    lines += ["};", "#define SH1X_CHEAT_COUNT (sizeof(kSh1xCheats) / sizeof(kSh1xCheats[0]))", "",
              "/* the dvars devset may write, in the server's order */",
              "static const Sh1xName kSh1xDevset[] = {"]
    for name, what in devset:
        lines.append("    { %s, %s }," % (cstr(name), cstr(what)))
    lines += ["};", "#define SH1X_DEVSET_COUNT (sizeof(kSh1xDevset) / sizeof(kSh1xDevset[0]))", "", "#endif", ""]
    return "\n".join(lines)


def main():
    if not (SERVER / "src" / "g_cmds.c").is_file():
        raise SystemExit("gen_sh1x_cmds: server/sh1x is not initialised (git submodule update --init server/sh1x)")
    text = render()
    if "--check" in sys.argv[1:]:
        have = OUT.read_text(encoding="ascii").replace("\r\n", "\n") if OUT.is_file() else ""
        if have != text:
            print("src/console/sh1x_cmds.h differs from the server source: run python scripts/gen_sh1x_cmds.py")
            return 1
        print("src/console/sh1x_cmds.h matches server/sh1x")
        return 0
    OUT.write_bytes(text.encode("ascii"))
    print("wrote %s" % OUT.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
