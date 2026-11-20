"""Lexical C/C++ declaration extractor for the sh1x server source tree.

This is deliberately a *lexical* scanner, not a compiler front end. It never
preprocesses, never evaluates ``#if`` branches and never resolves macros, so a
macro-generated declaration is invisible to it and a declaration inside a
disabled branch is still reported. Everything it emits carries ``file`` and
``line`` so a reader can check the original source; nothing here should be
presented as compiler-level truth.
"""
from __future__ import annotations

import hashlib
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator

C_LIKE = {".c", ".cpp", ".h", ".hpp", ".inc"}
ASM = {".asm"}

#: Keywords that may lead a declaration but are never the declared name.
_LEADING = {
    "static", "extern", "inline", "const", "volatile", "register", "signed",
    "unsigned", "struct", "union", "enum", "class", "typedef", "constexpr",
    "explicit", "virtual", "friend", "template", "public", "private",
    "protected", "__cdecl", "__stdcall", "__fastcall", "__attribute__",
    "qboolean", "void", "char", "short", "int", "long", "float", "double",
}

_NOT_A_FUNCTION = {
    "if", "else", "for", "while", "switch", "do", "return", "sizeof", "case",
    "catch", "defined", "typeof", "static_assert", "assert", "__attribute__",
}

_SINGLE_QUOTE = "'"
_DOUBLE_QUOTE = '"'


def strip_comments(text: str) -> tuple[str, dict[int, str]]:
    """Blank out comments while preserving every byte offset and newline.

    Returns the blanked text plus a map from the end offset of each comment to
    its raw content, which is how doc comments are recovered later.
    """
    out = list(text)
    comments: dict[int, str] = {}
    i, n = 0, len(text)
    while i < n:
        ch = text[i]
        if ch == _SINGLE_QUOTE or ch == _DOUBLE_QUOTE:
            quote, i = ch, i + 1
            while i < n and text[i] != quote:
                i += 2 if text[i] == "\\" else 1
            i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            comments[end] = text[i:end]
            for j in range(i, end):
                if out[j] != "\n":
                    out[j] = " "
            i = end
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            end = text.find("\n", i)
            end = n if end < 0 else end
            comments[end] = text[i:end]
            for j in range(i, end):
                out[j] = " "
            i = end
            continue
        i += 1
    return "".join(out), comments


#: A directive line plus any backslash-continued lines that follow it.
_DIRECTIVE = re.compile(r"^[ \t]*#(?:[^\r\n\\]|\\\r?\n|\\)*", re.M)


def blank_directives(clean: str) -> str:
    """Blank preprocessor directive lines, preserving every offset.

    Without this, ``#pragma pack(pop)`` reads as a call and the declaration
    scanner happily swallows the prototype that follows it as the argument
    list. Directive *bodies* are left alone, so declarations inside an
    ``#ifdef`` are still found.
    """
    out = list(clean)
    for match in _DIRECTIVE.finditer(clean):
        for i in range(match.start(), match.end()):
            if out[i] != "\n":
                out[i] = " "
    return "".join(out)


def _clean_doc(raw: str) -> str:
    lines = []
    for line in raw.splitlines():
        line = line.strip()
        line = re.sub(r"^/\*+<?|^//+<?|\*+/$", "", line).strip()
        line = re.sub(r"^\*\s?", "", line)
        lines.append(line)
    while lines and not lines[0]:
        lines.pop(0)
    while lines and not lines[-1]:
        lines.pop()
    return "\n".join(lines).strip()


def doc_before(text: str, comments: dict[int, str], start: int) -> str:
    """Return the comment block immediately above ``start``, if any.

    Preprocessor lines between the comment and the declaration are stepped
    over, because the common shape in this tree is a doc comment, then
    ``#pragma pack(push, 8)`` or ``#ifdef CODOL_SERVER``, then the declaration
    the comment is about. A blank line still ends the search: that is how an
    author says the comment belongs to something else.
    """
    gap = text[:start]
    while True:
        trimmed = gap.rstrip()
        if gap[len(trimmed):].count("\n") > 1:
            return ""
        for end in (len(trimmed), len(trimmed) + 1, len(trimmed) - 1):
            raw = comments.get(end)
            if raw is not None:
                return _clean_doc(raw)
        line_start = trimmed.rfind("\n") + 1
        if not trimmed[line_start:].lstrip().startswith("#"):
            return ""
        gap = trimmed[:line_start]


@dataclass
class Symbol:
    """One declaration found in the source tree."""

    name: str
    kind: str  # struct | union | enum | class | typedef | function | prototype | macro
    file: str
    line: int
    end_line: int
    signature: str
    doc: str = ""
    declaration: str = ""
    detail: dict = field(default_factory=dict)

    def key(self) -> tuple[str, str, str, int]:
        return (self.name, self.kind, self.file, self.line)

    def as_dict(self) -> dict:
        return {
            "name": self.name,
            "kind": self.kind,
            "file": self.file,
            "line": self.line,
            "end_line": self.end_line,
            "signature": self.signature,
            "doc": self.doc,
            "declaration": self.declaration,
            "detail": self.detail,
        }


def _line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def _match_brace(text: str, open_index: int) -> int:
    """Index just past the ``}`` matching the ``{`` at ``open_index``."""
    depth, i, n = 0, open_index, len(text)
    while i < n:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return n


def _collapse(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def _signature(head: str, name: str, params: str, qualifiers: str) -> str:
    """Rejoin a return type and a name without gluing ``int`` onto ``Foo``."""
    lead = _collapse(head)
    separator = "" if lead.endswith(("*", "&")) else " "
    return _collapse(f"{lead}{separator}{name}({_collapse(params)}) {qualifiers}")


def _trim_head(clean: str, start: int, head: str) -> str:
    """Drop a preprocessor line that a return type was glued onto.

    ``#define MAX 1024u`` followed by ``int Foo(void);`` matches as one head
    because ``#`` is not part of the identifier class the pattern walks back
    over. When the matched head began inside a directive, keep only its last
    line, which is the real return type.
    """
    line_start = clean.rfind("\n", 0, start) + 1
    if "#" in clean[line_start:start]:
        head = head.rsplit("\n", 1)[-1]
    return _collapse(head)


_MEMBER_ARRAY = re.compile(r"^(?P<name>[A-Za-z_]\w*)(?P<array>(?:\s*\[[^\]]*\])*)$")


def parse_members(body: str, comments: dict[int, str], base: int) -> list[dict]:
    """Split a struct/union body into member records.

    A nested anonymous struct or union stays one member entry whose ``type`` is
    the collapsed nested declaration. Field order is preserved; no layout or
    offset is computed, because packing pragmas make that a compiler question.
    """
    members: list[dict] = []
    depth, start, i, n = 0, 0, 0, len(body)
    while i < n:
        ch = body[i]
        if ch in "{([":
            depth += 1
        elif ch in "})]":
            depth -= 1
        elif ch == ";" and depth == 0:
            trailing = ""
            line_end = body.find("\n", i)
            line_end = n if line_end < 0 else line_end
            for end in (base + i + 1, base + line_end):
                if end in comments:
                    trailing = comments[end]
                    break
            members.extend(_member_record(body[start:i], _clean_doc(trailing) if trailing else ""))
            start = i + 1
        i += 1
    return members


_DECLARATOR = re.compile(
    r"^(?P<type>.*[\s\*&])(?P<name>[A-Za-z_]\w*)(?P<array>(?:\s*\[[^\]]*\])*)"
    r"\s*(?::\s*(?P<bits>.+))?$"
)


def _member_record(chunk: str, comment: str) -> list[dict]:
    """Turn one ``;``-terminated member declaration into member records.

    ``int a, *b[4];`` becomes two records sharing the base type, which is why
    the declarator list is split before the type is matched rather than after.
    """
    text = _collapse(chunk)
    if not text or text.startswith("#"):
        return []
    if "(" in text and ")" in text and "*" in text:  # function pointer member
        pointer = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)", text)
        if pointer:
            return [{"name": pointer.group(1), "type": text, "comment": comment, "bits": ""}]
    pieces, depth, start = [], 0, 0
    for i, ch in enumerate(text + ","):
        if ch in "{([":
            depth += 1
        elif ch in "})]":
            depth -= 1
        elif ch == "," and depth == 0:
            pieces.append(text[start:i].strip())
            start = i + 1
    out: list[dict] = []
    base_type = ""
    for index, piece in enumerate(pieces):
        if not piece:
            continue
        if index and base_type:
            piece = f"{base_type} {piece}"
        matched = _DECLARATOR.match(piece)
        if not matched:
            out.append({"name": "", "type": _collapse(piece), "comment": comment, "bits": ""})
            continue
        if not index:
            base_type = matched.group("type").strip().rstrip("*& ").strip()
        out.append({
            "name": matched.group("name"),
            "type": _collapse(matched.group("type") + matched.group("array")),
            "comment": comment,
            "bits": (matched.group("bits") or "").strip(),
        })
    return out


def parse_enumerators(body: str, comments: dict[int, str], base: int) -> list[dict]:
    values: list[dict] = []
    depth, start, i, n = 0, 0, 0, len(body)
    pieces: list[tuple[int, str]] = []
    while i <= n:
        ch = body[i] if i < n else ","
        if ch in "{([":
            depth += 1
        elif ch in "})]":
            depth -= 1
        elif ch == "," and depth == 0:
            pieces.append((i, body[start:i]))
            start = i + 1
        i += 1
    for offset, piece in pieces:
        text = _collapse(piece)
        if not text or text.startswith("#"):
            continue
        name, _, value = text.partition("=")
        line_end = body.find("\n", offset)
        line_end = n if line_end < 0 else line_end
        comment = comments.get(base + line_end) or ""
        values.append({
            "name": name.strip(),
            "value": value.strip(),
            "comment": _clean_doc(comment) if comment else "",
        })
    return values


def split_params(params: str) -> list[dict]:
    out: list[dict] = []
    depth, start = 0, 0
    for i, ch in enumerate(params + ","):
        if ch in "(<[":
            depth += 1
        elif ch in ")>]":
            depth -= 1
        elif ch == "," and depth == 0:
            piece = _collapse(params[start:i])
            if piece and piece != "void":
                name = ""
                matched = re.search(r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*$", piece)
                if matched and matched.group(1) not in _LEADING:
                    name = matched.group(1)
                out.append({"type": piece, "name": name})
            start = i + 1
    return out


_TAGGED = re.compile(
    r"(?P<typedef>\btypedef\s+)?\b(?P<kw>struct|union|enum|class)\b"
    r"(?P<attrs>(?:\s+(?:__attribute__\s*\(\([^)]*\)\)|final))*)"
    r"(?:\s+(?P<tag>[A-Za-z_]\w*))?\s*(?P<bases>:[^{;=]*)?\{"
)
_FUNC = re.compile(
    r"(?P<head>(?:[A-Za-z_][\w:<>,\s\*&\.]*?[\s\*&]))(?P<name>[A-Za-z_]\w*)\s*"
    r"\((?P<params>[^;{}]*)\)\s*(?P<qual>(?:const|noexcept|override|final|\s)*)(?P<tail>[;{])"
)
_FN_TYPEDEF = re.compile(
    r"\btypedef\s+(?P<ret>[\w\s\*]+?)\(\s*(?:__cdecl|__stdcall)?\s*\*\s*"
    r"(?P<name>[A-Za-z_]\w*)\s*\)\s*\((?P<params>[^;]*)\)\s*;"
)
_SIMPLE_TYPEDEF = re.compile(
    r"\btypedef\s+(?P<body>[^;{}()]+?)\s+(?P<name>[A-Za-z_]\w*)\s*(?P<array>(?:\[[^\]]*\])*)\s*;"
)
_MACRO = re.compile(
    r"^[ \t]*#[ \t]*define[ \t]+(?P<name>[A-Za-z_]\w*)(?P<args>\([^)]*\))?"
    r"[ \t]*(?P<value>[^\r\n\\]*(?:\\\r?\n[^\r\n]*)*)",
    re.M,
)


def extract(rel_path: str, text: str, language: str = "") -> list[Symbol]:
    """Extract declarations from one C/C++ translation unit or header.

    ``language`` records how a function body is actually implemented ("c" or
    "cpp"). Functions whose only implementation is retained x86 come from
    :func:`extract_asm` instead and are marked ``x86-asm``.
    """
    language = language or ("cpp" if rel_path.endswith((".cpp", ".hpp")) else "c")
    clean, comments = strip_comments(text)
    code = blank_directives(clean)
    symbols: list[Symbol] = []
    consumed: list[tuple[int, int]] = []

    def inside(pos: int) -> bool:
        return any(start <= pos < end for start, end in consumed)

    # 1. Tagged aggregates first. Their bodies are then excluded from the
    #    function scan so member functions cannot masquerade as free functions.
    for match in _TAGGED.finditer(code):
        open_index = match.end() - 1
        close = _match_brace(code, open_index)
        body = code[open_index + 1:close - 1]
        tail = re.match(r"\s*(?P<names>[A-Za-z_][\w\s\*,]*)?;", code[close:close + 400])
        typedef_names: list[str] = []
        if match.group("typedef") and tail and tail.group("names"):
            typedef_names = [n.strip().lstrip("*").strip()
                             for n in tail.group("names").split(",") if n.strip()]
        name = typedef_names[0] if typedef_names else (match.group("tag") or "")
        if not name:
            continue
        keyword = match.group("kw")
        start = match.start()
        detail: dict = {}
        if keyword == "enum":
            detail["values"] = parse_enumerators(body, comments, open_index + 1)
        else:
            detail["members"] = parse_members(body, comments, open_index + 1)
            methods = [_collapse(fm.group(0)).rstrip("{").strip()
                       for fm in _FUNC.finditer(body)
                       if fm.group("name") not in _NOT_A_FUNCTION]
            if methods:
                detail["methods"] = methods
            if match.group("bases"):
                detail["bases"] = _collapse(match.group("bases").lstrip(":"))
        if match.group("tag"):
            detail["tag"] = match.group("tag")
        if len(typedef_names) > 1:
            detail["aliases"] = typedef_names[1:]
        prefix = "typedef " if match.group("typedef") else ""
        symbols.append(Symbol(
            name=name,
            kind=keyword,
            file=rel_path,
            line=_line_of(code, start),
            end_line=_line_of(code, close),
            signature=_collapse(f"{prefix}{keyword} {match.group('tag') or ''} {{ ... }}"
                                + (f" {name};" if typedef_names else "")),
            doc=doc_before(text, comments, start),
            declaration=text[start:close],
            detail=detail,
        ))
        consumed.append((start, close))

    for match in _FN_TYPEDEF.finditer(code):
        symbols.append(Symbol(
            name=match.group("name"), kind="typedef", file=rel_path,
            line=_line_of(clean, match.start()), end_line=_line_of(clean, match.end()),
            signature=_collapse(match.group(0)),
            doc=doc_before(text, comments, match.start()),
            declaration=_collapse(match.group(0)),
            detail={"returns": _collapse(match.group("ret")),
                    "params": split_params(match.group("params")), "callable": True},
        ))
        consumed.append((match.start(), match.end()))

    for match in _SIMPLE_TYPEDEF.finditer(code):
        if inside(match.start()):
            continue
        symbols.append(Symbol(
            name=match.group("name"), kind="typedef", file=rel_path,
            line=_line_of(clean, match.start()), end_line=_line_of(clean, match.end()),
            signature=_collapse(match.group(0)),
            doc=doc_before(text, comments, match.start()),
            declaration=_collapse(match.group(0)),
            detail={"underlying": _collapse(match.group("body")) + match.group("array")},
        ))
        consumed.append((match.start(), match.end()))

    # 2. Free functions: definitions (a body follows) and prototypes.
    for match in _FUNC.finditer(code):
        if inside(match.start()):
            continue
        name = match.group("name")
        if name in _NOT_A_FUNCTION:
            continue
        head = _trim_head(code, match.start(), match.group("head"))
        if not head or head.split()[-1] in _NOT_A_FUNCTION or re.search(r"[=\)\],]$", head):
            continue
        definition = match.group("tail") == "{"
        end = _match_brace(code, match.end() - 1) if definition else match.end()
        symbols.append(Symbol(
            name=name,
            kind="function" if definition else "prototype",
            file=rel_path,
            line=_line_of(clean, match.start()),
            end_line=_line_of(clean, end),
            signature=_signature(head, name, match.group("params"), match.group("qual")),
            doc=doc_before(text, comments, match.start()),
            declaration=_collapse(f"{head}{name}({match.group('params')})"),
            detail={"returns": head.rstrip("*& ").strip(),
                    "params": split_params(match.group("params")),
                    "storage": "static" if head.startswith("static") else "extern",
                    "implementation": language if definition else ""},
        ))
        if definition:
            consumed.append((match.start(), end))

    for match in _MACRO.finditer(clean):
        value = _collapse(match.group("value") or "")
        if not value and not match.group("args"):
            continue
        args = match.group("args")
        symbols.append(Symbol(
            name=match.group("name"), kind="macro", file=rel_path,
            line=_line_of(clean, match.start()), end_line=_line_of(clean, match.end()),
            signature=_collapse(f"#define {match.group('name')}{args or ''} {value}")[:400],
            doc=doc_before(text, comments, match.start()),
            declaration=_collapse(match.group(0))[:800],
            detail={"parameters": split_params(args[1:-1]) if args else [], "value": value[:400]},
        ))
    return symbols


_ASM_LABEL = re.compile(r"^([A-Za-z_][\w@$?]*):[ \t]*(?:;.*)?$", re.M)
_ASM_LOCAL = re.compile(r"^(?P<owner>.+)_\d+$")
_ASM_EXTERN = re.compile(r"^\s*extern\s+([A-Za-z_][\w@$?]*)", re.M)
_ASM_GLOBAL = re.compile(r"^\s*global\s+([A-Za-z_][\w@$?]*)", re.M)


def looks_like_asm(text: str) -> bool:
    """True for a NASM include; ``.inc`` is used for both NASM and C fragments."""
    head = "\n".join(text.splitlines()[:40])
    return bool(re.search(r"^\s*(%define|%ifdef|%include|section |global |extern )", head, re.M)) or \
        bool(re.search(r"^\s*(dd|dw|db|dq)\s+\S", head, re.M))


def extract_asm(rel_path: str, text: str) -> list[Symbol]:
    """Retained IW3 functions kept as assembly expose only their labels.

    Most pre-fork CoD4x engine functions were never reimplemented in C: they are
    bare x86 copied out of the original game binary and reassembled here. A
    label that reads ``Name_12`` under a ``Name`` label is a basic-block target
    from that disassembly, not a separate function, so it is skipped.
    """
    out: list[Symbol] = []
    seen: set[str] = set()
    imports = sorted(set(_ASM_EXTERN.findall(text)))
    exports = set(_ASM_GLOBAL.findall(text))
    lines = text.splitlines()
    labels = list(_ASM_LABEL.finditer(text))
    for index, match in enumerate(labels):
        name = match.group(1)
        local = _ASM_LOCAL.match(name)
        if local and local.group("owner") in seen:
            continue
        seen.add(name)
        end = labels[index + 1].start() if index + 1 < len(labels) else len(text)
        start_line = _line_of(text, match.start())
        end_line = _line_of(text, end)
        body = "\n".join(lines[start_line:min(start_line + 6, len(lines))])
        out.append(Symbol(
            name=name,
            kind="function",
            file=rel_path,
            line=start_line,
            end_line=end_line,
            signature=f"{name}:  ; x86 assembly, retained from the game binary",
            declaration=f"{name}:\n{body}",
            detail={
                "implementation": "x86-asm",
                "language": "x86 assembly (NASM)",
                "retained": True,
                "exported": name in exports,
                "instruction_lines": max(end_line - start_line - 1, 0),
                "imports": imports[:64],
            },
        ))
    return out


def sha256(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8", "replace")).hexdigest()


def iter_source(root: Path, relative: str) -> Iterator[tuple[str, Path]]:
    base = root / relative
    if not base.exists():
        return
    for path in sorted(base.rglob("*")):
        if path.is_file() and path.suffix.lower() in C_LIKE | ASM:
            yield (path.relative_to(root).as_posix(), path)
