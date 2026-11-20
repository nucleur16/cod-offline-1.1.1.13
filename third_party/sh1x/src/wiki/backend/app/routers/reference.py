"""The generated reference: source areas, files, declarations and GSC builtins.

These endpoints are read-only. Their content comes from the indexer, so the way
to change what they say is to change the server source and re-run
``python -m app.cli index``.
"""
from __future__ import annotations

from fastapi import APIRouter, HTTPException, Query, status
from sqlalchemy import case, func, literal, or_, select

from ..deps import SessionDep
from ..models import GscBuiltin, IndexRun, SourceArea, SourceFile, Symbol
from ..schemas import (
    AreaOut,
    BuiltinOut,
    IndexRunOut,
    PagedBuiltins,
    PagedSymbols,
    SourceFileOut,
    SymbolOut,
    SymbolSummary,
)

router = APIRouter(prefix="/reference", tags=["reference"])

KINDS = ("struct", "union", "enum", "class", "typedef", "function", "prototype", "macro")


@router.get("/areas", response_model=list[AreaOut])
async def areas(session: SessionDep):
    return list(await session.scalars(
        select(SourceArea).order_by(SourceArea.position, SourceArea.name)
    ))


@router.get("/areas/{name}", response_model=AreaOut)
async def area(name: str, session: SessionDep):
    found = await session.scalar(select(SourceArea).where(SourceArea.name == name))
    if found is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, f"No source area '{name}'.")
    return found


@router.get("/files", response_model=list[SourceFileOut])
async def files(session: SessionDep, area: str | None = None,
                limit: int = Query(default=500, ge=1, le=2000)):
    query = select(SourceFile).order_by(SourceFile.path).limit(limit)
    if area:
        query = query.where(SourceFile.area == area)
    return list(await session.scalars(query))


@router.get("/symbols", response_model=PagedSymbols)
async def symbols(
    session: SessionDep,
    q: str | None = None,
    kind: str | None = None,
    area: str | None = None,
    file: str | None = None,
    implementation: str | None = None,
    codol_only: bool = False,
    limit: int = Query(default=50, ge=1, le=200),
    offset: int = Query(default=0, ge=0),
) -> PagedSymbols:
    query = select(Symbol)
    if q:
        needle = f"%{q.strip().lower()}%"
        query = query.where(or_(Symbol.lower_name.like(needle),
                                func.lower(Symbol.signature).like(needle)))
    if kind:
        query = query.where(Symbol.kind.in_(kind.split(",")))
    if area:
        query = query.where(Symbol.area.in_(area.split(",")))
    if file:
        query = query.where(Symbol.file == file)
    if implementation:
        query = query.where(Symbol.implementation.in_(implementation.split(",")))
    if codol_only:
        query = query.where(Symbol.is_codol.is_(True))
    total = await session.scalar(select(func.count()).select_from(query.subquery()))
    # A query can match a name or a signature that merely mentions the type, so
    # put the thing that is actually called `q` at the top.
    relevance = (
        case((Symbol.lower_name == (q or "").strip().lower(), 0),
             (Symbol.lower_name.like(f"{(q or '').strip().lower()}%"), 1),
             else_=2)
        if q else literal(0)
    )
    rows = await session.scalars(
        query.order_by(relevance, Symbol.lower_name, Symbol.file, Symbol.line)
        .limit(limit).offset(offset)
    )
    return PagedSymbols(total=total or 0, limit=limit, offset=offset,
                        items=[SymbolSummary.model_validate(row) for row in rows])


@router.get("/symbols/id/{symbol_id}", response_model=SymbolOut)
async def symbol_by_id(symbol_id: int, session: SessionDep):
    found = await session.get(Symbol, symbol_id)
    if found is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "No such symbol.")
    return found


@router.get("/symbols/by-name/{name}", response_model=list[SymbolOut])
async def symbol_by_name(name: str, session: SessionDep, kind: str | None = None):
    """Every declaration site for a name.

    A name can appear more than once: a header prototype and its definition, a
    static helper repeated per translation unit, or a struct declared under
    ``#ifdef`` in two shapes. All sites are returned so the caller can see that.
    """
    query = select(Symbol).where(Symbol.lower_name == name.lower())
    if kind:
        query = query.where(Symbol.kind == kind)
    rows = list(await session.scalars(query.order_by(Symbol.kind, Symbol.file, Symbol.line)))
    if not rows:
        raise HTTPException(status.HTTP_404_NOT_FOUND, f"Nothing declared as '{name}'.")
    return rows


@router.get("/builtins", response_model=PagedBuiltins)
async def builtins(
    session: SessionDep,
    q: str | None = None,
    kind: str | None = None,
    area: str | None = None,
    implementation: str | None = None,
    developer_only: bool | None = None,
    limit: int = Query(default=50, ge=1, le=1000),
    offset: int = Query(default=0, ge=0),
) -> PagedBuiltins:
    query = select(GscBuiltin)
    if q:
        needle = f"%{q.strip().lower()}%"
        query = query.where(or_(func.lower(GscBuiltin.name).like(needle),
                                func.lower(GscBuiltin.binding).like(needle)))
    if kind:
        query = query.where(GscBuiltin.kind == kind)
    if area:
        query = query.where(GscBuiltin.area.in_(area.split(",")))
    if implementation:
        query = query.where(GscBuiltin.implementation.in_(implementation.split(",")))
    if developer_only is not None:
        query = query.where(GscBuiltin.developer_only.is_(developer_only))
    total = await session.scalar(select(func.count()).select_from(query.subquery()))
    rows = await session.scalars(
        query.order_by(GscBuiltin.name).limit(limit).offset(offset)
    )
    return PagedBuiltins(total=total or 0, limit=limit, offset=offset,
                         items=[BuiltinOut.model_validate(row) for row in rows])


@router.get("/builtins/{name}", response_model=list[BuiltinOut])
async def builtin(name: str, session: SessionDep):
    rows = list(await session.scalars(
        select(GscBuiltin).where(func.lower(GscBuiltin.name) == name.lower())
        .order_by(GscBuiltin.kind)
    ))
    if not rows:
        raise HTTPException(status.HTTP_404_NOT_FOUND, f"No GSC builtin named '{name}'.")
    return rows


@router.get("/runs", response_model=list[IndexRunOut])
async def runs(session: SessionDep, limit: int = Query(default=10, ge=1, le=50)):
    return list(await session.scalars(
        select(IndexRun).order_by(IndexRun.started_at.desc()).limit(limit)
    ))
