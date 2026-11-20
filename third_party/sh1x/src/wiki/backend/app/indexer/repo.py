"""Walk the sh1x server repository and build the reference index.

The index has three layers:

``areas``
    The source areas of :doc:`docs/CODOL_SOURCE_LAYOUT.md` plus the retained
    CoD4x engine and its assembly, with per-area counts.
``files``
    One record per indexed source file: area, language, line count, sha256.
``symbols``
    Declarations found by :mod:`app.indexer.csource`, with each function's
    implementation resolved to C, C++ or retained x86 assembly.

Nothing here compiles or runs the server. It is a lexical pass over checked-in
text, so it reports what the source *says*, not what the linker resolves.
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from .csource import ASM, C_LIKE, Symbol, extract, extract_asm, looks_like_asm, sha256

#: Ordered so that the first matching prefix wins.
AREA_RULES: list[tuple[str, str]] = [
    ("src/codol/net/", "net"),
    ("src/codol/snapshot/", "snapshot"),
    ("src/codol/player/", "player"),
    ("src/codol/weapon/", "weapon"),
    ("src/codol/vehicle/", "vehicle"),
    ("src/codol/script/", "script"),
    ("src/codol/assets/", "assets"),
    ("src/codol/collision/", "collision"),
    ("src/codol/stats/", "stats"),
    ("src/asmsource/", "asmsource"),
    ("src/mbedtls/", "vendor"),
    ("src/zlib/", "vendor"),
    ("src/win32/", "platform"),
    ("src/xassets/", "engine"),
    ("src/", "engine"),
    ("tools/", "tools"),
    ("tests/", "tests"),
]

#: Areas whose source is CoDOL work rather than retained CoD4x code.
CODOL_AREAS = {"net", "snapshot", "player", "weapon", "vehicle", "script",
               "assets", "collision", "stats"}

SKIP_DIRS = {"obj", "bin", "node_modules", ".git", "dis", "lib", "lib-native", "wiki",
             "__pycache__"}

#: ``tools/cod4_dasm`` is the full disassembly of the original CoD4 binary. It
#: is the pool that ``src/asmsource`` was drawn from, not server source, and at
#: ~1.8M lines it would swamp the index, so it is described in prose instead.
SKIP_PREFIXES = ("tools/cod4_dasm/", "tools/debugbreak/", "src/mbedtls/tests/")


def area_of(rel_path: str) -> str:
    for prefix, area in AREA_RULES:
        if rel_path.startswith(prefix):
            return area
    return "other"


def language_of(rel_path: str, text: str) -> str:
    suffix = Path(rel_path).suffix.lower()
    if suffix in ASM:
        return "asm"
    if suffix in {".cpp", ".hpp"}:
        return "cpp"
    if suffix == ".inc":
        return "asm" if looks_like_asm(text) else "c"
    return "c"


@dataclass
class FileRecord:
    path: str
    area: str
    language: str
    lines: int
    bytes: int
    sha256: str
    role: str            # header | source | include | assembly
    symbol_count: int = 0


@dataclass
class Index:
    files: list[FileRecord] = field(default_factory=list)
    symbols: list[Symbol] = field(default_factory=list)
    builtins: list[dict] = field(default_factory=list)
    documents: list[dict] = field(default_factory=list)

    def as_dict(self) -> dict:
        return {
            "files": [vars(f) for f in self.files],
            "symbols": [s.as_dict() for s in self.symbols],
            "builtins": self.builtins,
            "documents": self.documents,
        }


def role_of(rel_path: str) -> str:
    suffix = Path(rel_path).suffix.lower()
    if suffix in ASM:
        return "assembly"
    if suffix == ".inc":
        return "include"
    if suffix in {".h", ".hpp"}:
        return "header"
    return "source"


def iter_files(root: Path) -> list[tuple[str, Path]]:
    out: list[tuple[str, Path]] = []
    for base in ("src", "tools", "tests"):
        start = root / base
        if not start.exists():
            continue
        for path in sorted(start.rglob("*")):
            relative = path.relative_to(root).as_posix()
            if any(part in SKIP_DIRS for part in path.relative_to(root).parts[:-1]):
                continue
            if relative.startswith(SKIP_PREFIXES):
                continue
            if path.is_file() and path.suffix.lower() in C_LIKE | ASM:
                out.append((path.relative_to(root).as_posix(), path))
    return out


_REGISTER = re.compile(
    r"\bScr_Add(?P<kind>Function|Method)\s*\(\s*\"(?P<name>[^\"]+)\"\s*,\s*"
    r"(?:\(\s*xfunction_t\s*\)\s*)?&?\s*(?P<binding>[\w:\.\[\]\s\*&+]+?)\s*,\s*(?P<developer>[^)]*)\)"
)


def extract_builtins(rel_path: str, text: str) -> list[dict]:
    """Find GSC builtin registrations.

    ``Scr_AddFunction`` registers a free function callable as ``name(...)``;
    ``Scr_AddMethod`` registers a method callable as ``entity name(...)``. The
    third argument is the developer-only flag: a non-zero value means the
    builtin is only reachable when the server runs with ``developer 1``.
    """
    out = []
    for match in _REGISTER.finditer(text):
        binding = match.group("binding").strip()
        if not re.fullmatch(r"[A-Za-z_]\w*", binding):
            continue  # A computed binding (plugin stubs) has no single owner.
        developer = match.group("developer").strip()
        out.append({
            "name": match.group("name"),
            "kind": "method" if match.group("kind") == "Method" else "function",
            "binding": binding,
            "developer_only": developer not in {"0", "qfalse", "false"},
            "developer_expression": developer,
            "file": rel_path,
            "line": text.count("\n", 0, match.start()) + 1,
        })
    return out


def build(root: Path) -> Index:
    index = Index()
    definitions: dict[str, list[Symbol]] = {}
    asm_definitions: dict[str, list[Symbol]] = {}

    for rel_path, path in iter_files(root):
        text = path.read_text(encoding="utf-8", errors="replace")
        language = language_of(rel_path, text)
        if language == "asm":
            symbols = extract_asm(rel_path, text)
        else:
            symbols = extract(rel_path, text, language)
            index.builtins.extend(extract_builtins(rel_path, text))
        for symbol in symbols:
            if symbol.kind != "function":
                continue
            table = asm_definitions if language == "asm" else definitions
            table.setdefault(symbol.name, []).append(symbol)
        index.files.append(FileRecord(
            path=rel_path, area=area_of(rel_path), language=language,
            lines=text.count("\n") + 1, bytes=len(text.encode("utf-8", "replace")),
            sha256=sha256(text), role=role_of(rel_path), symbol_count=len(symbols),
        ))
        index.symbols.extend(symbols)

    _resolve_implementations(index, definitions, asm_definitions)
    _resolve_builtins(index, definitions, asm_definitions)
    _resolve_aliases(index)
    return index


_ALIAS = re.compile(r"^(?:struct|union|enum)\s+(?P<tag>[A-Za-z_]\w*)$")


def _resolve_aliases(index: Index) -> None:
    """Tie ``typedef struct client_s client_t;`` to ``struct client_s``.

    The engine headers routinely declare the tagged aggregate in one place and
    the ``_t`` alias in another, so a reader searching for ``client_t`` has to
    land on ``client_s`` to see any members.
    """
    tagged: dict[str, Symbol] = {}
    for symbol in index.symbols:
        if symbol.kind in {"struct", "union", "enum", "class"}:
            tagged.setdefault(symbol.detail.get("tag") or symbol.name, symbol)
    for symbol in index.symbols:
        if symbol.kind != "typedef":
            continue
        match = _ALIAS.match(symbol.detail.get("underlying", "").strip())
        if not match:
            continue
        target = tagged.get(match.group("tag"))
        if not target:
            continue
        symbol.detail["alias_of"] = {"name": target.name, "kind": target.kind,
                                     "file": target.file, "line": target.line}
        target.detail.setdefault("aliases", []).append(symbol.name)


def _resolve_implementations(index: Index, definitions: dict[str, list[Symbol]],
                             asm_definitions: dict[str, list[Symbol]]) -> None:
    """Say where each declared function is actually implemented.

    A prototype in a header whose only definition is an assembly label is the
    normal shape for pre-fork CoD4x code: those functions were never rewritten
    in C, they are the original game binary's x86 reassembled under the same
    name. The wiki marks them so a reader does not go looking for C source.
    """
    for symbol in index.symbols:
        if symbol.kind not in {"function", "prototype"}:
            continue
        c_defs = definitions.get(symbol.name, [])
        asm_defs = asm_definitions.get(symbol.name, [])
        if symbol.kind == "function":
            implementation = symbol.detail.get("implementation") or "c"
        elif c_defs:
            implementation = c_defs[0].detail.get("implementation") or "c"
        elif asm_defs:
            implementation = "x86-asm"
        else:
            implementation = "unresolved"
        symbol.detail["implementation"] = implementation
        if implementation == "x86-asm":
            symbol.detail["retained"] = True
            symbol.detail["implementation_note"] = (
                "Implemented in retained x86 assembly, disassembled from the "
                "original game binary rather than rewritten in C."
            )
        symbol.detail["defined_in"] = [
            {"file": d.file, "line": d.line, "language": d.detail.get("implementation", "c")}
            for d in c_defs
        ] + [{"file": d.file, "line": d.line, "language": "x86-asm"} for d in asm_defs]


def _resolve_builtins(index: Index, definitions: dict[str, list[Symbol]],
                      asm_definitions: dict[str, list[Symbol]]) -> None:
    prototypes: dict[str, Symbol] = {}
    for symbol in index.symbols:
        if symbol.kind == "prototype" and symbol.name not in prototypes:
            prototypes[symbol.name] = symbol
    for builtin in index.builtins:
        binding = builtin["binding"]
        c_defs = definitions.get(binding, [])
        asm_defs = asm_definitions.get(binding, [])
        source = (c_defs or asm_defs or [prototypes.get(binding)])[0]
        builtin["implementation"] = (
            "x86-asm" if not c_defs and asm_defs
            else (c_defs[0].detail.get("implementation", "c") if c_defs else "unresolved")
        )
        builtin["area"] = area_of(source.file) if source else ""
        builtin["binding_file"] = source.file if source else ""
        builtin["binding_line"] = source.line if source else 0
        builtin["doc"] = source.doc if source else ""
        builtin["signature"] = source.signature if source else ""


def load_documents(root: Path) -> list[dict]:
    """Read the repository's own design notes so the wiki can link to them."""
    out = []
    for relative in ("docs", ):
        base = root / relative
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix.lower() not in {".md", ".json"} or not path.is_file():
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            rel = path.relative_to(root).as_posix()
            title = ""
            if path.suffix.lower() == ".md":
                heading = re.search(r"^#\s+(.+)$", text, re.M)
                title = heading.group(1).strip() if heading else path.stem
            else:
                title = path.stem.replace("CODOL_", "").replace("_", " ").title()
            out.append({
                "path": rel, "title": title, "format": path.suffix.lstrip("."),
                "bytes": len(text.encode("utf-8", "replace")), "sha256": sha256(text),
                "body": text,
            })
    return out


def build_full(root: Path) -> Index:
    index = build(root)
    index.documents = load_documents(root)
    return index


def summarize(index: Index) -> dict:
    areas: dict[str, dict] = {}
    for record in index.files:
        bucket = areas.setdefault(record.area, {
            "area": record.area, "files": 0, "lines": 0, "symbols": 0,
            "languages": {}, "codol": record.area in CODOL_AREAS,
        })
        bucket["files"] += 1
        bucket["lines"] += record.lines
        bucket["symbols"] += record.symbol_count
        bucket["languages"][record.language] = bucket["languages"].get(record.language, 0) + 1
    kinds: dict[str, int] = {}
    implementations: dict[str, int] = {}
    for symbol in index.symbols:
        kinds[symbol.kind] = kinds.get(symbol.kind, 0) + 1
        if symbol.kind in {"function", "prototype"}:
            key = symbol.detail.get("implementation", "unresolved")
            implementations[key] = implementations.get(key, 0) + 1
    return {
        "areas": sorted(areas.values(), key=lambda a: a["area"]),
        "kinds": kinds,
        "implementations": implementations,
        "builtins": len(index.builtins),
        "documents": len(index.documents),
    }


def main() -> None:  # pragma: no cover - developer convenience
    import argparse

    parser = argparse.ArgumentParser(description="Build the sh1x wiki source index")
    parser.add_argument("--root", default="..", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    index = build_full(args.root.resolve())
    print(json.dumps(summarize(index), indent=2))
    if args.out:
        args.out.write_text(json.dumps(index.as_dict()), encoding="utf-8")


if __name__ == "__main__":  # pragma: no cover
    main()
