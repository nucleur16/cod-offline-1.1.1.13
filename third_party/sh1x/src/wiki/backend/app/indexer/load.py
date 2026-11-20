"""Load a built index into Postgres.

The reference tables are replaced wholesale on each run rather than diffed: the
index is derived data, a full rebuild takes seconds, and a diff would have to
guess whether a moved declaration is the same declaration.
"""
from __future__ import annotations

import subprocess
from datetime import datetime
from pathlib import Path

from sqlalchemy import delete, insert, select
from sqlalchemy.ext.asyncio import AsyncSession

from ..models import GscBuiltin, IndexRun, SourceArea, SourceFile, Symbol
from ..seed.areas import BY_NAME
from .repo import CODOL_AREAS, Index, build_full, summarize

CHUNK = 1000


def git_commit(root: Path) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), "rev-parse", "HEAD"],
            capture_output=True, text=True, timeout=10, check=False,
        )
        return result.stdout.strip()[:64]
    except (OSError, subprocess.SubprocessError):
        return ""


async def _bulk(session: AsyncSession, table, rows: list[dict]) -> None:
    for start in range(0, len(rows), CHUNK):
        await session.execute(insert(table), rows[start:start + CHUNK])


async def load_index(session: AsyncSession, root: Path, index: Index | None = None) -> IndexRun:
    index = index or build_full(root)
    summary = summarize(index)
    run = IndexRun(
        started_at=datetime.now(), repo_root=str(root), git_commit=git_commit(root),
        files=len(index.files), symbols=len(index.symbols), builtins=len(index.builtins),
        summary=summary,
    )
    session.add(run)

    await session.execute(delete(Symbol))
    await session.execute(delete(GscBuiltin))
    await session.execute(delete(SourceFile))
    await session.execute(delete(SourceArea))

    file_area = {record.path: record.area for record in index.files}
    seen: set[tuple] = set()
    symbol_rows = []
    for symbol in index.symbols:
        key = (symbol.name, symbol.kind, symbol.file, symbol.line)
        if key in seen:
            continue  # The same site can match two patterns; keep the first.
        seen.add(key)
        area = file_area.get(symbol.file, "other")
        symbol_rows.append({
            "name": symbol.name, "lower_name": symbol.name.lower(), "kind": symbol.kind,
            "area": area, "file": symbol.file, "line": symbol.line,
            "end_line": symbol.end_line, "signature": symbol.signature[:8000],
            "doc": symbol.doc[:8000], "declaration": symbol.declaration[:20000],
            "implementation": symbol.detail.get("implementation", "") or "",
            "is_codol": area in CODOL_AREAS, "detail": symbol.detail,
        })
    await _bulk(session, Symbol, symbol_rows)

    builtin_seen: set[tuple] = set()
    builtin_rows = []
    for builtin in index.builtins:
        key = (builtin["name"], builtin["kind"], builtin["file"], builtin["line"])
        if key in builtin_seen:
            continue
        builtin_seen.add(key)
        builtin_rows.append({
            "name": builtin["name"], "kind": builtin["kind"], "binding": builtin["binding"],
            "area": builtin.get("area", ""), "implementation": builtin.get("implementation", ""),
            "developer_only": builtin["developer_only"],
            "developer_expression": builtin["developer_expression"][:64],
            "registered_file": builtin["file"], "registered_line": builtin["line"],
            "binding_file": builtin.get("binding_file", ""),
            "binding_line": builtin.get("binding_line", 0),
            "signature": builtin.get("signature", "")[:8000],
            "doc": builtin.get("doc", "")[:8000],
        })
    await _bulk(session, GscBuiltin, builtin_rows)

    await _bulk(session, SourceFile, [
        {"path": f.path, "area": f.area, "language": f.language, "role": f.role,
         "lines": f.lines, "bytes": f.bytes, "sha256": f.sha256,
         "symbol_count": f.symbol_count}
        for f in index.files
    ])

    area_rows = []
    for entry in summary["areas"]:
        described = BY_NAME.get(entry["area"], {})
        area_rows.append({
            "name": entry["area"],
            "title": described.get("title", entry["area"].replace("_", " ").title()),
            "summary": described.get("summary", ""),
            "path": described.get("path", ""),
            "is_codol": entry["codol"],
            "file_count": entry["files"], "line_count": entry["lines"],
            "symbol_count": entry["symbols"], "languages": entry["languages"],
            "position": described.get("position", 999),
        })
    await _bulk(session, SourceArea, area_rows)

    run.finished_at = datetime.now()
    await session.commit()
    return run


async def latest_run(session: AsyncSession) -> IndexRun | None:
    return await session.scalar(select(IndexRun).order_by(IndexRun.started_at.desc()).limit(1))
