#!/usr/bin/env python3
"""Drop the legacy "codol" prefix from this server's own code.

Deterministic and idempotent. The rename table tools/rename_prefix_map.json is
authoritative: `plan` only ADDS entries for names it has not seen (so a name keeps
the replacement it got the first time, even after other branches land), and
`apply` rewrites the tree from the table. Running `apply` twice changes nothing.

  python tools/rename_prefix.py plan  [--system-tokens F] [--system-headers F]
  python tools/rename_prefix.py apply
  python tools/rename_prefix.py report        # every "codol" left in scope, with the reason
  python tools/rename_prefix.py check         # change nothing; exit 1 when apply would change a file

Rules (docs: the table's "rules" entry repeats them):
  * identifiers, macros, types, labels: drop the prefix (codol_x -> x, CODOL_X -> X,
    SV_CodolX -> SV_X, test_codol_x -> test_x, codolPrintLn -> printLn). When the
    result already exists anywhere in the compiled sources or the system headers
    (msg_t, usercmd_t, ...), is a keyword, or two old names would meet, the prefix is
    replaced by "ol_" / "OL_" / "Ol" instead (codol_msg_t -> ol_msg_t).
  * console commands and dvars: always plain drop (codol_devServer -> devServer);
    Kernel33 forwards some by name with the same rule. A clash is an error unless the
    table's config.names_forced settles it (codol_dev_cheats -> dev_cheats, merged by
    hand with the existing dev_cheats).
  * file names follow the identifier rule against every file name in the include
    path and the flat object directory; src/codol/ becomes src/online/.
  * build outputs: codol_dedrun -> sh1x_dedrun, codol_archive -> sh1x_archive,
    GAME=codol -> GAME=sh1x (obj/sh1x), win_codol.res / codol.ico -> sh1x;
    make targets test-codol -> test, codol-run-x -> run-x, codol-archive -> archive.
  * console names the GSC scripts read or set (config.gsc_names, the same table as Kernel33's
    tools/drop_codol_prefix.py GSC_TABLE, which also renames the GSC tree itself with --tree):
    renamed everywhere in scope, the native overlay scripts/codol/ included, although the
    GSC tree uses them (codol_zoneType -> sv_zoneType, scr_codol_bots -> scr_bots).
  * fastfile (zone) names of the client's patch folder (config.zone_names, the same table as Kernel33's
    tools/drop_codol_prefix.py ZONE_TABLE): codol_common -> common_extras, mp_codol_weapons -> mp_weapons, ...,
    in every context and every in-scope file but recorded fixtures (tests/fixtures/), except on a line that
    carries the word LEGACY_ZONE: there the old names stay (an install from before the rename still has
    those files, and the server falls back to them).
  * kept: every other name the GSC tree (bin/home/codol_rawfiles, a separate repository)
    uses -- script builtins, dvars and script paths --; GSC builtins registered by the
    server; string literals that are not a known command/dvar/identifier/file (asset,
    zone, fastfile and wire names, file-format magics such as "codol-persistent-v1",
    key labels such as "codol-archive"); runtime data paths; environment variables
    (config.env); the prose name "CoDOL"; historical records (EXCLUDE).
"""
import argparse
import json
import os
import re
import subprocess
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP_PATH = os.path.join(ROOT, 'tools', 'rename_prefix_map.json')
RAW = 'bin/home/codol_rawfiles'

# Not rewritten: other repositories, frozen records whose bytes are pinned or that
# record what happened, vendored code, runtime state, this tool.
EXCLUDE = (
    RAW + '/', 'scripts/codol/', 'native_profiles/',
    'docs/releases/', 'docs/release-lineage/', 'docs/zone_assets/', 'docs/zm_evidence/',
    'wiki/', 'tools/cod4_dasm/', 'src/mbedtls/', 'src/tomcrypt/', 'plugins/', 'lib/',
    'bin/home/main/', 'bin/home/codol_progress/', 'bin/home/codol_animtrees/',
    'bin/home/codol_sessions.txt', '.gitmodules',
    'tools/discord-assets/', 'tools/rename_prefix.py', 'tools/rename_prefix_map.json',
)
# Scanned only to learn which names are taken.
RESERVED_ROOTS = ('src/', 'plugins/', 'tests/', 'tools/', 'lib/')

C_EXT = {'.c', '.cpp', '.cc', '.h', '.hpp', '.rc', '.js'}
ASM_EXT = {'.asm', '.nasm', '.s'}
PY_EXT = {'.py'}
DOC_EXT = {'.md', '.json', '.txt', '.csv', '.html', '.xml', '.patch', '.diff', '.gsc', '.map', '.cfg', '.log'}

KEYWORDS = set('''auto break case char const continue default do double else enum extern float for goto if
inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void
volatile while _Bool bool true false class public private protected virtual template typename namespace using
new delete this operator friend throw try catch explicit mutable export asm alignas alignof decltype noexcept
nullptr static_assert thread_local constexpr and or not xor main errno stdin stdout stderr NULL
None True False def lambda yield import from as with pass raise global nonlocal assert del elif except finally is in
section global extern bits default times db dw dd dq resb resw resd'''.split())

CODOL = re.compile(r'codol', re.I)
FILE_CTX = re.compile(r'\.(?:h|c|cpp|hpp|inc|asm|o|d|py|ps1|sh|cmd|md|json|txt|exe)\b')
TOK = re.compile(r'[A-Za-z0-9_]+')
STANDALONE = {'codol', 'CODOL', 'CoDOL', 'Codol', 'CoDol'}

# Fixed names (every context). Build outputs and things whose drop would be meaningless.
OVERRIDES = {
    'codol_dedrun': 'sh1x_dedrun',
    'codol_dedrun_next': 'sh1x_dedrun_next',
    'codol_dedrun_verify': 'sh1x_dedrun_verify',
    'codol_archive': 'sh1x_archive',
    'win_codol': 'win_sh1x',
}
FILE_OVERRIDES = {
    'src/win32/codol.ico': 'src/win32/sh1x.ico',
    'tools/BUILD-CODOL.ps1': 'tools/BUILD-SERVER.ps1',
}
# Plain-text replacements applied before the token pass (code files and docs).
TEXT_RULES = [
    (re.compile(r'(?<=src/)codol(?=/|\b)'), 'online'),
    (re.compile(r'(?<=src\\)codol(?=\\|\b)'), 'online'),
    (re.compile(r'(?<=src\\\\)codol(?=\\\\)'), 'online'),
    (re.compile(r'(?<=\$\(SRC_DIR\)/)codol\b'), 'online'),
    (re.compile(r'''(?<=['"]src['"], ['"])codol(?=['"])'''), 'online'),
    (re.compile(r'''(?<=['"]src['"] / ['"])codol(?=['"])'''), 'online'),
    (re.compile(r"(?<=_codol_src/)'codol'"), "'online'"),
    (re.compile(r"(?<='src'/)'codol'"), "'online'"),
    (re.compile(r'(?<![A-Za-z0-9_])codol(?=/(?:assets|collision|net|player|script|snapshot|stats|vehicle|weapon)/)'), 'online'),
    (re.compile(r'(?<![A-Za-z0-9_])codol\.ico\b'), 'sh1x.ico'),
    (re.compile(r'\bGAME \?= codol\b'), 'GAME ?= sh1x'),
    (re.compile(r'\(\$\(GAME\),codol\)'), '($(GAME),sh1x)'),
    (re.compile(r'GAME must be codol or cod4x'), 'GAME must be sh1x or cod4x'),
    (re.compile(r'\bGAME=codol\b'), 'GAME=sh1x'),
    (re.compile(r'\bobj/codol\b'), 'obj/sh1x'),
    (re.compile(r'\bobj\\codol\b'), 'obj\\sh1x'),
    (re.compile(r'\bSTART-CODOL-SERVER\b'), 'START-SERVER'),
    (re.compile(r'\bBUILD-CODOL\b'), 'BUILD-SERVER'),
]
# docs: paths and file names only (prose such as "GAME=codol" history stays readable either way)
DOC_TEXT_RULES = [r for r in TEXT_RULES if 'GAME' not in r[0].pattern and 'obj' not in r[0].pattern]


def git(*args, cwd=ROOT):
    return subprocess.run(['git'] + list(args), cwd=cwd, capture_output=True, text=True, check=True).stdout


def tracked():
    return [f for f in git('ls-files', '-z').split('\0') if f and os.path.isfile(os.path.join(ROOT, f))]


def in_scope(path):
    return not any(path == e.rstrip('/') or path.startswith(e) for e in EXCLUDE)


def read(path):
    with open(os.path.join(ROOT, path), 'rb') as fh:
        data = fh.read()
    if b'\0' in data[:8192]:
        return None
    return data.decode('latin-1')


def kind_of(path):
    base = os.path.basename(path)
    ext = os.path.splitext(base)[1].lower()
    if ext == '.inc':
        return 'asm' if '/asmsource/' in '/' + path or path.startswith('tests/') else 'c'
    if ext in C_EXT:
        return 'c'
    if ext in ASM_EXT:
        return 'asm'
    if ext in PY_EXT:
        return 'py'
    if ext in DOC_EXT or base.upper().startswith(('README', 'CHANGELOG', 'LICENSE', 'COPYING')):
        return 'doc'
    return 'script'   # makefile, .sh, .ps1, .cmd, .gitignore, ...


LEX = {
    'c': re.compile(r'(?P<comment>//[^\n]*|/\*.*?(?:\*/|\Z))|(?P<string>"(?:[^"\\\n]|\\.)*"?|\'(?:[^\'\\\n]|\\.)*\'?)', re.S),
    'asm': re.compile(r'(?P<comment>;[^\n]*)|(?P<string>"[^"\n]*"?|\'[^\'\n]*\'?|`(?:[^`\\\n]|\\.)*`?)'),
    'py': re.compile(r'(?P<comment>#[^\n]*)|(?P<string>[rRbBuUfF]{0,2}(?:"""[\s\S]*?(?:"""|\Z)|\'\'\'[\s\S]*?(?:\'\'\'|\Z)|"(?:[^"\\\n]|\\.)*"?|\'(?:[^\'\\\n]|\\.)*\'?))'),
}


def segments(text, kind):
    """[(ctx, text)] with ctx code / comment / string / script / doc."""
    if kind in ('script', 'doc'):
        return [(kind, text)]
    out, pos = [], 0
    for m in LEX[kind].finditer(text):
        if m.start() > pos:
            out.append(('code', text[pos:m.start()]))
        out.append(('comment' if m.group('comment') is not None else 'string', m.group(0)))
        pos = m.end()
    if pos < len(text):
        out.append(('code', text[pos:]))
    return out


def split_lead(tok):
    m = re.match(r'^(__wrap_|__real_|_*)(.*)$', tok, re.S)
    return m.group(1), m.group(2)


def drop(core, repl=None):
    """Remove each 'codol' (or replace it with repl) keeping the name readable."""
    out, i = [], 0
    for m in CODOL.finditer(core):
        a, b = m.start(), m.end()
        if a < i:
            continue
        word = m.group(0)
        before = core[a - 1] if a else ''
        after = core[b] if b < len(core) else ''
        out.append(core[i:a])
        if repl is not None:
            r = {'codol': 'ol', 'CODOL': 'OL'}.get(word, 'Ol')
            if word == 'codol' and before and before.isalnum() and after and after.isupper():
                r = 'Ol'
            out.append(r)
            i = b
            continue
        if after == '_':
            i = b + 1 if (not before or before == '_') else b
        elif after == '':
            if out and out[-1].endswith('_'):
                out[-1] = out[-1][:-1]
            i = b
        else:
            i = b
            if word == 'codol' and (not before or before == '_') and after.isupper() and \
                    b + 1 < len(core) and core[b + 1].islower():
                out.append(after.lower())
                i = b + 1
    out.append(core[i:])
    return ''.join(out)


def valid_ident(name):
    return bool(name) and not name[0].isdigit() and re.fullmatch(r'[A-Za-z0-9_]+', name) and name not in KEYWORDS \
        and not re.fullmatch(r'_*', name)


class Scan:
    pass


def raw_tokens():
    """Lower-case codol names the GSC tree uses: script code and strings (.gsc) and its tables (.csv).
    GSC is case-insensitive, and so are dvar names."""
    raw_root = os.path.join(ROOT, RAW)
    if not os.path.isfile(os.path.join(raw_root, 'maps', 'mp', 'gametypes', '_callbacksetup.gsc')):
        sys.exit('rename_prefix: %s is not initialised (git -c core.autocrlf=false submodule update --init -- %s)'
                 % (RAW, RAW))
    out = set()
    for f in git('ls-files', '-z', cwd=raw_root).split(chr(0)):
        p = os.path.join(raw_root, f)
        if not f or not os.path.isfile(p) or not f.endswith(('.gsc', '.csv')):
            continue
        out.update(t.lower() for t in TOK.findall(f) if CODOL.search(t))
        with open(p, 'rb') as fh:
            t = fh.read().decode('latin-1')
        for ctx, seg in segments(t, 'c' if f.endswith('.gsc') else 'doc'):
            if ctx != 'comment':
                out.update(x.lower() for x in TOK.findall(seg) if CODOL.search(x))
    return out


def scan(system_tokens=None, system_headers=None):
    s = Scan()
    s.files = tracked()
    s.scope = [f for f in s.files if in_scope(f)]
    s.code = defaultdict(int)       # cores used in code/script
    s.strings = defaultdict(int)    # cores used in string literals
    s.names = set()                 # console commands / dvars (string cores)
    s.name_ctx = defaultdict(set)
    s.reserved = set()              # case-sensitive taken identifiers
    s.reserved_str = set()          # lower-case taken command/dvar names
    s.file_stems = set()            # lower-case taken file stems (src/tests/tools)
    s.raw = set()                   # lower-case tokens the GSC tree uses (code + strings)
    s.string_only = defaultdict(list)

    # names the GSC tree uses
    s.raw = raw_tokens()

    if system_tokens:
        with open(system_tokens, encoding='latin-1') as fh:
            s.reserved.update(l.strip() for l in fh if l.strip())
    if system_headers:
        with open(system_headers, encoding='latin-1') as fh:
            s.file_stems.update(os.path.splitext(l.strip())[0].lower() for l in fh if l.strip())

    call = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*(?:[^(),;"]*,\s*)?$')
    for f in s.files:
        stem = os.path.splitext(os.path.basename(f))[0]
        if f.startswith(('src/', 'tests/', 'tools/')) and not CODOL.search(stem):
            s.file_stems.add(stem.lower())
        if not (f.startswith(RESERVED_ROOTS) or in_scope(f)):
            continue
        text = read(f)
        if text is None:
            continue
        kind = kind_of(f)
        if kind == 'doc':
            continue
        scoped = in_scope(f)
        for ctx, seg in segments(text, kind):
            toks = TOK.findall(seg)
            if ctx in ('code', 'script'):
                for t in toks:
                    if CODOL.search(t):
                        if scoped:
                            s.code[split_lead(t)[1]] += 1
                    else:
                        s.reserved.add(t)
            elif ctx == 'string':
                if scoped:
                    for t in toks:
                        if CODOL.search(t):
                            s.strings[split_lead(t)[1]] += 1
        if kind not in ('c', 'asm'):
            continue
        # commands / dvars: a whole literal that is the first argument of a Cvar_/Dvar_/Cmd_ call
        for m in re.finditer(r'"([A-Za-z_][A-Za-z0-9_]*)"', text):
            tok = m.group(1)
            head = text[max(0, m.start() - 160):m.start()]
            c = call.search(head)
            fn = c.group(1) if c else ''
            if not (re.match(r'(Cvar|Dvar|Cmd|SV_Cmd|Scr_Cmd)_', fn) or re.search(r'cvar|dvar', fn, re.I)):
                continue
            if not CODOL.search(tok):
                s.reserved_str.add(tok.lower())
            elif scoped:
                s.names.add(tok)
                s.name_ctx[tok].add(fn)
    # +set <name> anywhere in scope (launchers, tests, docs)
    for f in s.scope:
        text = read(f)
        if text is None or kind_of(f) == 'doc':
            continue
        for m in re.finditer(r'\+set\s+"?([A-Za-z_][A-Za-z0-9_]*)', text):
            if CODOL.search(m.group(1)) and m.group(1).lower() not in s.raw:
                s.names.add(m.group(1))
                s.name_ctx[m.group(1)].add('+set')
    for t in s.strings:
        if t not in s.code and t not in s.names:
            s.string_only[t] = s.strings[t]
    return s


def load_map():
    if os.path.exists(MAP_PATH):
        with open(MAP_PATH, encoding='utf-8') as fh:
            return json.load(fh)
    return {}


def plan(args):
    s = scan(args.system_tokens, args.system_headers)
    m = load_map()
    cfg = m.get('config', {})
    keep = set(cfg.get('keep', []))
    extra_names = set(cfg.get('names', []))
    forced = dict(cfg.get('names_forced', {}))
    gsc = {k.lower() for k in cfg.get('gsc_names', {})}
    ident = dict(m.get('identifiers', {}))
    names = dict(m.get('names', {}))
    files = dict(m.get('files', {}))

    # --- commands / dvars: plain drop, must stay unique
    want = sorted((s.names | extra_names) - keep)
    taken = {v.lower() for v in names.values()}
    errors = []
    for t in want:
        if t in names or t.lower() in s.raw or t.lower() in gsc:
            continue
        n = forced.get(t) or OVERRIDES.get(t) or drop(t)
        if t in forced:
            pass
        elif not valid_ident(n) or n.lower() in s.reserved_str or n.lower() in s.raw or n.lower() in taken:
            errors.append('command/dvar %s -> %s collides' % (t, n))
            n = drop(t, 'ol')
        names[t] = n
        taken.add(n.lower())

    # --- files
    new_files = {}
    for f in s.scope:
        if CODOL.search(f):
            new_files[f] = new_path(f, files_map=None)
    code_dirs = ('src/', 'tests/', 'tools/')
    dir_stems = defaultdict(set)
    for f in s.files:
        if f not in new_files:
            dir_stems[os.path.dirname(f)].add(os.path.splitext(os.path.basename(f))[0].lower())
    stem_of = {}   # (is_code, old stem) -> new stem, shared by a .c and its .h
    for f, nf in files.items():
        stem_of[(f.startswith(code_dirs), os.path.splitext(os.path.basename(f))[0])] =             os.path.splitext(os.path.basename(nf))[0]
    code_taken = set(s.file_stems) | {v.lower() for (c, _), v in stem_of.items() if c}
    for f in sorted(new_files):
        if f in files:
            continue
        stem, ext = os.path.splitext(os.path.basename(f))
        if f in FILE_OVERRIDES:
            files[f] = FILE_OVERRIDES[f]
            continue
        if not CODOL.search(stem):
            files[f] = new_files[f]
            continue
        is_code = f.startswith(code_dirs)
        key = (is_code, stem)
        if key not in stem_of:
            taken = code_taken if is_code else dir_stems[os.path.dirname(new_files[f])]
            cand = OVERRIDES.get(stem) or drop_hyphen(stem)
            if not cand or cand[0].isdigit() or cand.lower() in taken:
                cand = drop_hyphen(stem, 'ol')
            if cand.lower() in taken:
                errors.append('file stem %s has no free name' % stem)
            stem_of[key] = cand
            taken.add(cand.lower())
        d = os.path.dirname(new_files[f])
        files[f] = (d + '/' if d else '') + stem_of[key] + ext
    # paths must be unique (case-insensitive)
    seen = defaultdict(list)
    existing = {f.lower() for f in s.files if f not in files}
    present = set(s.files)
    for f, nf in files.items():
        if f not in present:      # already moved by an earlier apply
            continue
        seen[nf.lower()].append(f)
        if nf.lower() in existing:
            errors.append('file %s -> %s exists' % (f, nf))
    for k, v in seen.items():
        if len(v) > 1:
            errors.append('files %s all map to %s' % (v, k))
    stem_new = {}
    for f, nf in files.items():
        a = os.path.splitext(os.path.basename(f))[0]
        b = os.path.splitext(os.path.basename(nf))[0]
        if a != b:
            stem_new.setdefault(a, b)

    # --- identifiers
    used_new = set(ident.values())
    pending = sorted(t for t in s.code if t not in ident and t not in STANDALONE)
    proposal = {}
    for t in pending:
        if t in OVERRIDES:
            proposal[t] = OVERRIDES[t]
        elif t in stem_new:
            proposal[t] = stem_new[t]
        elif t in names and t not in s.reserved:
            proposal[t] = names[t]
        else:
            proposal[t] = drop(t)
    def bad(t, n):
        return (not valid_ident(n)) or n in s.reserved or n in used_new
    groups = defaultdict(list)
    for t, n in proposal.items():
        groups[n].append(t)
    for t in pending:
        n = proposal[t]
        if t in OVERRIDES or t in stem_new:
            pass
        elif bad(t, n) or len(groups[n]) > 1:
            n = drop(t, 'ol')
            if bad(t, n):
                errors.append('identifier %s has no free name (%s)' % (t, n))
        ident[t] = n
        used_new.add(n)

    rules = __doc__.split('Rules (docs: the table\'s "rules" entry repeats them):')[1].strip().splitlines()
    out = {
        'rules': [l.strip() for l in rules if l.strip()],
        'config': {'keep': sorted(keep), 'names': sorted(extra_names), 'string_rename': sorted(cfg.get('string_rename', [])),
                   'names_forced': dict(sorted(forced.items())),
                   'strings': dict(sorted(cfg.get('strings', {}).items())),
                   'env': sorted(cfg.get('env', [])),
                   'gsc_names': dict(sorted(cfg.get('gsc_names', {}).items(), key=lambda kv: kv[0].lower())),
                   'zone_names': dict(sorted(cfg.get('zone_names', {}).items()))},
        'names': dict(sorted(names.items(), key=lambda kv: kv[0].lower())),
        'files': dict(sorted(files.items())),
        'identifiers': dict(sorted(ident.items())),
    }
    with open(MAP_PATH, 'w', encoding='utf-8', newline='\n') as fh:
        json.dump(out, fh, indent=1, sort_keys=False)
        fh.write('\n')
    so = sorted(s.string_only.items(), key=lambda kv: (-kv[1], kv[0]))
    with open(os.path.join(args.out or ROOT, 'rename_prefix_review.txt'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('# string-only names kept (not a command/dvar/identifier/file)\n')
        for t, c in so:
            fh.write('%s %d%s\n' % (t, c, ' RAW' if t.lower() in s.raw else ''))
        fh.write('\n# commands/dvars and where they were found\n')
        for t in sorted(names, key=str.lower):
            fh.write('%s -> %s  %s\n' % (t, names[t], ','.join(sorted(s.name_ctx.get(t, [])))))
        fh.write('\n# identifiers that got the ol_ fallback\n')
        for t, n in sorted(ident.items()):
            if n != drop(t) and t not in OVERRIDES and t not in stem_new:
                fh.write('%s -> %s\n' % (t, n))
        fh.write('\n# files that got the ol_ fallback\n')
        for f, nf in sorted(files.items()):
            st = os.path.splitext(os.path.basename(f))[0]
            if os.path.splitext(os.path.basename(nf))[0] != drop_hyphen(st) and st not in OVERRIDES:
                fh.write('%s -> %s\n' % (f, nf))
    print('names %d, files %d, identifiers %d, string-only kept %d' % (len(names), len(files), len(ident), len(so)))
    for e in errors:
        print('ERROR', e)
    return 1 if errors else 0


def drop_hyphen(stem, repl=None):
    """A file stem: tokens around '-' or '.' are treated separately."""
    parts = re.split(r'([-.])', stem)
    out = []
    for p in parts:
        if p in STANDALONE:
            out.append(None if repl is None else ('ol' if p == 'codol' else 'OL'))
        elif CODOL.search(p):
            out.append(OVERRIDES.get(p) or drop(p, repl))
        else:
            out.append(p)
    # remove a dropped standalone and one separator next to it
    res = []
    for i, p in enumerate(out):
        if p is None:
            if res and res[-1] in '-.':
                res.pop()
            elif i + 1 < len(out) and out[i + 1] in ('-', '.'):
                out[i + 1] = ''
            continue
        res.append(p)
    return ''.join(res)


def new_path(f, files_map):
    if files_map is not None and f in files_map:
        return files_map[f]
    parts = f.split('/')
    out = []
    for i, p in enumerate(parts):
        if i == 1 and parts[0] == 'src' and p == 'codol':
            out.append('online')
        elif i < len(parts) - 1 and CODOL.search(p):
            out.append(OVERRIDES.get(p) or drop_hyphen(p))
        else:
            out.append(p)
    return '/'.join(out)


class Rewriter:
    def __init__(self, m):
        self.ident = m['identifiers']
        self.names = m['names']
        cfg = m.get('config', {})
        self.keep = set(cfg.get('keep', []))
        self.string_rename = set(cfg.get('string_rename', []))
        self.strings_map = dict(cfg.get('strings', {}))
        self.gsc = {k.lower(): v for k, v in cfg.get('gsc_names', {}).items()}
        self.zones = dict(cfg.get('zone_names', {}))
        self.stems = {}
        for f, nf in m['files'].items():
            a = os.path.splitext(os.path.basename(f))[0]
            b = os.path.splitext(os.path.basename(nf))[0]
            if a != b:
                self.stems[a] = b
            da, db = f.split('/'), nf.split('/')
            for x, y in zip(da[:-1], db[:-1]):
                if x != y and x not in STANDALONE:
                    self.stems.setdefault(x, y)
        self.raw = set()

    def map_core(self, core, ctx):
        if core in self.keep or core.lower() in self.raw and ctx != 'code':
            return None
        if core in OVERRIDES:
            return OVERRIDES[core]
        if ctx == 'code':
            return self.ident.get(core)
        if ctx == 'comment':
            return self.ident.get(core) or self.stems.get(core) or self.names.get(core)
        if ctx == 'script':
            return self.names.get(core) or self.stems.get(core) or self.ident.get(core)
        # string / doc
        if core in self.names:
            return self.names[core]
        if core in self.stems:
            return self.stems[core]
        if ctx == 'string' and core in self.ident and (core not in self.raw):
            return self.ident[core] if (core in self.string_rename or self._looks_code(core)) else None
        if ctx == 'doc':
            return self.ident.get(core)
        return None

    @staticmethod
    def _looks_code(core):
        # a C name quoted in a log message or a generated source line: CODOL_Foo, SV_CodolFoo, codol_x_t
        return bool(re.search(r'CODOL_[A-Z][a-z]|_Codol[A-Z]|_CoDOL[A-Z]|^CODOL_[A-Z0-9_]+$|_t$|^codol_[a-z0-9_]+$', core))

    def token(self, text, m, ctx):
        tok = m.group(0)
        if not CODOL.search(tok):
            return tok
        a, b = m.start(), m.end()
        prev = text[a - 1] if a else ''
        nxt = text[b] if b < len(text) else ''
        # -DCODOL_X / -dCODOL_X / -ICODOL_X, %~dp0codol_x
        fm = re.match(r'^([DdIU])((?:CODOL|codol|Codol).*)$', tok)
        if fm and prev == '-' and ctx != 'code':
            r = self.token_core(fm.group(2), ctx)
            return fm.group(1) + (r if r is not None else fm.group(2))
        if tok.startswith('dp0') and text[max(0, a - 2):a] == '%~':
            r = self.token_core(tok[3:], ctx)
            return 'dp0' + (r if r is not None else tok[3:])
        if tok in STANDALONE:
            return tok
        if ctx != 'code' and tok.lower() in self.gsc:
            return self.gsc[tok.lower()]
        if ctx != 'code' and tok in self.strings_map:
            return self.strings_map[tok]
        # a path component (followed by a source extension or a separator) of a renamed file or folder;
        # the same word elsewhere (an asset, a fastfile name) may be kept
        if ctx != 'code' and (FILE_CTX.match(text, b) or nxt in ('/', chr(92))):
            lead, core = split_lead(tok)
            if core in self.stems:
                return lead + self.stems[core]
        if tok.lower() in self.raw and ctx != 'code':
            return tok
        r = self.token_core(tok, ctx)
        return tok if r is None else r

    def token_core(self, tok, ctx):
        if tok in OVERRIDES:
            return OVERRIDES[tok]
        lead, core = split_lead(tok)
        if not core:
            return None
        r = self.map_core(core, ctx)
        return None if r is None else lead + r

    @staticmethod
    def hyphen(text):
        # make targets and the info flag only: test-codol[-x], codol-run-x, codol-archive, --codol-info.
        # Every other hyphenated "codol" is a file-format magic (codol-persistent-v1), a temp/container
        # name or a URL (CODOL-EMU) and stays.
        text = re.sub(r'(?<![A-Za-z0-9_-])test-codol(?![A-Za-z0-9_])', 'test', text)
        text = re.sub(r'(?<![A-Za-z0-9_-])test-codol-(?=[a-z])', 'test-', text)
        text = re.sub(r'(?<![A-Za-z0-9_-])codol-run-', 'run-', text)
        text = re.sub(r'(?<![A-Za-z0-9_-])codol-archive(?![A-Za-z0-9_])', 'archive', text)
        text = re.sub(r'--codol-info(?![A-Za-z0-9_-])', '--info', text)
        return text

    def rewrite(self, text, kind):
        rules = DOC_TEXT_RULES if kind == 'doc' else TEXT_RULES
        for rx, rep in rules:
            text = rx.sub(lambda _m, r=rep: r, text)
        out = []
        for ctx, seg in segments(text, kind):
            if ctx in ('script', 'doc', 'comment'):   # never inside a string literal: "codol-archive" is key material
                seg = self.hyphen(seg)
            out.append(TOK.sub(lambda m, s=seg, c=ctx: self.token(s, m, c), seg))
        return ''.join(out)


LEGACY_MARK = 'LEGACY_ZONE'
ZONE_EXCLUDE = ('tests/fixtures/',)


def zone_text(text, zones):
    """text with every zone_names name (a whole word, case-sensitive) replaced, except on LEGACY_ZONE lines."""
    if not zones:
        return text
    rx = re.compile(r'(?<![A-Za-z0-9_])(' + '|'.join(re.escape(k) for k in sorted(zones, key=lambda s: (-len(s), s)))
                    + r')(?![A-Za-z0-9_])')
    return ''.join(line if LEGACY_MARK in line else rx.sub(lambda m: zones[m.group(1)], line)
                   for line in text.splitlines(True))


# The native overlay GSC (scriptNativeOverlay "scripts/codol") is otherwise excluded: only gsc_names change there.
GSC_OVERLAY = 'scripts/codol/'


def gsc_overlay_text(text, gsc):
    """text with every gsc_names name (any case, a whole word) replaced."""
    if not gsc:
        return text
    rx = re.compile(r'(?<![A-Za-z0-9_])(' + '|'.join(re.escape(k) for k in sorted(gsc, key=lambda s: (-len(s), s)))
                    + r')(?![A-Za-z0-9_])', re.I)
    return rx.sub(lambda m: gsc[m.group(1).lower()], text)


def planned_changes(m):
    """[(path, new path, old text or None, new text or None)] that apply would make; changes nothing."""
    s_raw = raw_tokens()
    rw = Rewriter(m)
    rw.raw = s_raw
    out = []
    for f in tracked():
        if f.startswith(GSC_OVERLAY) and f.endswith('.gsc'):
            text = read(f)
            if text is not None:
                new = gsc_overlay_text(text, rw.gsc)
                if new != text:
                    out.append((f, f, text, new))
            continue
        if not in_scope(f):
            continue
        nf = m['files'].get(f) or new_path(f, None)
        text = read(f)
        new = text
        if text is not None and re.search(r'codol|START-CODOL|BUILD-CODOL', text, re.I):
            new = rw.rewrite(text, kind_of(nf))
            if not f.startswith(ZONE_EXCLUDE):
                new = zone_text(new, rw.zones)
        if nf != f or new != text:
            out.append((f, nf, text, new))
    return out


def check(args):
    m = load_map()
    if not m:
        sys.exit('rename_prefix: run plan first')
    changes = planned_changes(m)
    for f, nf, text, new in changes:
        what = []
        if nf != f:
            what.append('-> ' + nf)
        if new != text:
            what.append('%d line(s)' % sum(1 for a, b in zip(text.split(chr(10)), new.split(chr(10))) if a != b))
        print('  %s %s' % (f, ' '.join(what)))
    print('%d file(s) apply would change' % len(changes))
    return 1 if changes else 0


def apply(args):
    m = load_map()
    if not m:
        sys.exit('rename_prefix: run plan first')
    s_raw = raw_tokens()
    rw = Rewriter(m)
    rw.raw = s_raw
    overlay = 0
    for f in tracked():
        if f.startswith(GSC_OVERLAY) and f.endswith('.gsc'):
            text = read(f)
            new = gsc_overlay_text(text, rw.gsc) if text is not None else text
            if new != text:
                with open(os.path.join(ROOT, f), 'wb') as fh:
                    fh.write(new.encode('latin-1'))
                overlay += 1
    moved = 0
    for f in tracked():
        if not in_scope(f):
            continue
        nf = m['files'].get(f) or new_path(f, None)
        if nf != f:
            os.makedirs(os.path.dirname(os.path.join(ROOT, nf)) or ROOT, exist_ok=True)
            for attempt in range(5):   # an index.lock left by a concurrent git
                try:
                    git('mv', f, nf)
                    break
                except subprocess.CalledProcessError:
                    if attempt == 4:
                        raise
                    import time
                    time.sleep(0.5)
            moved += 1
    changed = 0
    for f in tracked():
        if not in_scope(f):
            continue
        text = read(f)
        if text is None or not re.search(r'codol|START-CODOL|BUILD-CODOL', text, re.I):
            continue
        new = rw.rewrite(text, kind_of(f))
        if not f.startswith(ZONE_EXCLUDE):
            new = zone_text(new, rw.zones)
        if new != text:
            with open(os.path.join(ROOT, f), 'wb') as fh:
                fh.write(new.encode('latin-1'))
            changed += 1
    # remove directories left empty by the moves
    for d, subs, fs in sorted(os.walk(os.path.join(ROOT, 'src', 'codol')), reverse=True):
        if not subs and not fs:
            os.rmdir(d)
    print('moved %d files, rewrote %d files, %d native overlay scripts' % (moved, changed, overlay))


def report(args):
    m = load_map()
    counts = defaultdict(lambda: defaultdict(int))
    for f in tracked():
        if not in_scope(f):
            continue
        text = read(f)
        if text is None:
            if CODOL.search(f):
                counts['binary file name'][f] += 1
            continue
        kind = kind_of(f)
        for ctx, seg in segments(text, kind):
            for t in TOK.findall(seg):
                if CODOL.search(t):
                    key = 'prose' if t in STANDALONE else ctx
                    counts[key][t] += 1
        if CODOL.search(f):
            counts['file name'][f] += 1
    for k in sorted(counts):
        items = sorted(counts[k].items(), key=lambda kv: (-kv[1], kv[0]))
        print('== %s: %d distinct, %d total' % (k, len(items), sum(counts[k].values())))
        for t, c in items[:args.limit]:
            print('  %s %d' % (t, c))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('plan')
    p.add_argument('--system-tokens', help='identifiers of the system headers (one per line)')
    p.add_argument('--system-headers', help='system header file names (one per line)')
    p.add_argument('--out', help='folder for rename_prefix_review.txt (default: repository root)')
    sub.add_parser('apply')
    sub.add_parser('check')
    r = sub.add_parser('report')
    r.add_argument('--limit', type=int, default=60)
    a = ap.parse_args()
    return {'plan': plan, 'apply': apply, 'report': report, 'check': check}[a.cmd](a) or 0


if __name__ == '__main__':
    sys.exit(main())
