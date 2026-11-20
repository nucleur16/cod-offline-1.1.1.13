"""Full-text search across authored pages and the generated reference."""
from __future__ import annotations

from fastapi import APIRouter, Query
from sqlalchemy import func, or_, select

from ..config import get_settings
from ..deps import OptionalUser, SessionDep
from ..models import GscBuiltin, IndexRun, Page, SourceArea, Space, Symbol
from ..schemas import IndexRunOut, SearchHit, SearchResponse, SiteStats

router = APIRouter(tags=["search"])


@router.get("/search", response_model=SearchResponse)
async def search(
    session: SessionDep,
    viewer: OptionalUser,
    q: str = Query(min_length=1, max_length=200),
    kind: str = Query(default="all", pattern=r"^(all|pages|symbols|builtins)$"),
    limit: int = Query(default=25, ge=1, le=100),
) -> SearchResponse:
    """Rank pages, declarations and GSC builtins for one query.

    Page text is matched with ``websearch_to_tsquery`` so quoted phrases and
    ``-word`` work the way people expect. Identifiers are matched by prefix as
    well, because ``CODOL_Write`` should find ``CODOL_WriteSnapshot`` even
    though no stemmer would relate them.
    """
    text = q.strip()
    needle = f"%{text.lower()}%"
    prefix = f"{text.lower()}%"
    tsquery = func.websearch_to_tsquery("english", text)
    hits: list[SearchHit] = []
    counts: dict[str, int] = {}

    if kind in {"all", "pages"}:
        page_rank = func.ts_rank(Page.search_vector, tsquery).label("rank")
        query = (
            select(Page, page_rank, Space.slug)
            .outerjoin(Space, Page.space_id == Space.id)
            .where(or_(Page.search_vector.op("@@")(tsquery),
                       func.lower(Page.title).like(needle)))
        )
        if viewer is None or not viewer.can_edit:
            query = query.where(Page.status == "published")
        rows = (await session.execute(query.order_by(page_rank.desc()).limit(limit))).all()
        counts["pages"] = len(rows)
        for page, rank, space_slug in rows:
            hits.append(SearchHit(
                type="page", title=page.title,
                subtitle=f"page · {space_slug or 'unfiled'}",
                href=f"/wiki/{page.slug}",
                snippet=(page.summary or page.body)[:240],
                rank=float(rank or 0) + 1.0,  # authored prose outranks raw symbols
            ))

    if kind in {"all", "symbols"}:
        symbol_rank = func.ts_rank(Symbol.search_vector, tsquery).label("rank")
        query = (
            select(Symbol, symbol_rank)
            .where(or_(Symbol.search_vector.op("@@")(tsquery),
                       Symbol.lower_name.like(prefix),
                       Symbol.lower_name.like(needle)))
            .order_by(Symbol.is_codol.desc(), symbol_rank.desc(), Symbol.lower_name)
            .limit(limit)
        )
        rows = (await session.execute(query)).all()
        counts["symbols"] = len(rows)
        for symbol, rank in rows:
            note = f" · {symbol.implementation}" if symbol.implementation else ""
            hits.append(SearchHit(
                type="symbol", title=symbol.name,
                subtitle=f"{symbol.kind} · {symbol.area}{note}",
                href=f"/reference/symbols/{symbol.id}",
                snippet=symbol.doc[:200] or symbol.signature[:200],
                rank=float(rank or 0) + (0.5 if symbol.lower_name == text.lower() else 0.0),
            ))

    if kind in {"all", "builtins"}:
        builtin_rank = func.ts_rank(GscBuiltin.search_vector, tsquery).label("rank")
        query = (
            select(GscBuiltin, builtin_rank)
            .where(or_(GscBuiltin.search_vector.op("@@")(tsquery),
                       func.lower(GscBuiltin.name).like(prefix)))
            .order_by(builtin_rank.desc(), GscBuiltin.name)
            .limit(limit)
        )
        rows = (await session.execute(query)).all()
        counts["builtins"] = len(rows)
        for builtin, rank in rows:
            hits.append(SearchHit(
                type="builtin", title=builtin.name,
                subtitle=f"GSC {builtin.kind} · {builtin.binding}",
                href=f"/reference/gsc/{builtin.name}",
                snippet=builtin.doc[:200] or builtin.signature[:200],
                rank=float(rank or 0) + 0.75,
            ))

    hits.sort(key=lambda hit: hit.rank, reverse=True)
    return SearchResponse(query=text, hits=hits[:limit], counts=counts)


@router.get("/site", response_model=SiteStats)
async def site(session: SessionDep) -> SiteStats:
    settings = get_settings()
    last = await session.scalar(select(IndexRun).order_by(IndexRun.started_at.desc()).limit(1))
    implementations = dict(
        (await session.execute(
            select(Symbol.implementation, func.count())
            .where(Symbol.kind.in_(("function", "prototype")))
            .group_by(Symbol.implementation)
        )).all()
    )
    lines = await session.scalar(select(func.coalesce(func.sum(SourceArea.line_count), 0)))
    return SiteStats(
        site_name=settings.site_name,
        tagline=settings.site_tagline,
        pages=await session.scalar(select(func.count()).select_from(Page)) or 0,
        spaces=await session.scalar(select(func.count()).select_from(Space)) or 0,
        symbols=await session.scalar(select(func.count()).select_from(Symbol)) or 0,
        builtins=await session.scalar(select(func.count()).select_from(GscBuiltin)) or 0,
        files=await session.scalar(select(func.coalesce(func.sum(SourceArea.file_count), 0))) or 0,
        lines=lines or 0,
        implementations={k or "n/a": v for k, v in implementations.items()},
        last_index=IndexRunOut.model_validate(last) if last else None,
    )
