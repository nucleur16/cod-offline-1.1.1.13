"""Wiki pages: read for everyone, write for editors and admins."""
from __future__ import annotations

from fastapi import APIRouter, HTTPException, Query, Response, status
from sqlalchemy import func, select
from sqlalchemy.orm import selectinload

from ..deps import EditorUser, OptionalUser, SessionDep
from ..models import AuditEvent, Page, PageRevision, Space, User
from ..schemas import (
    PageOut,
    PagePatch,
    PageSummary,
    PageWrite,
    PagedPages,
    RevisionDetail,
    RevisionOut,
)

router = APIRouter(prefix="/pages", tags=["pages"])

_LOADERS = (selectinload(Page.space), selectinload(Page.created_by),
            selectinload(Page.updated_by))


def _visible(query, viewer: User | None):
    """Drafts are only listed for accounts that can edit them."""
    if viewer is None or not viewer.can_edit:
        return query.where(Page.status == "published")
    return query


async def _load(session, slug: str, viewer: User | None) -> Page:
    query = _visible(select(Page).where(Page.slug == slug).options(*_LOADERS), viewer)
    page = await session.scalar(query)
    if page is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, f"No page at '{slug}'.")
    return page


async def _resolve_space(session, slug: str | None) -> Space | None:
    if not slug:
        return None
    space = await session.scalar(select(Space).where(Space.slug == slug))
    if space is None:
        raise HTTPException(status.HTTP_400_BAD_REQUEST, f"No space '{slug}'.")
    return space


@router.get("", response_model=PagedPages)
async def list_pages(
    session: SessionDep,
    viewer: OptionalUser,
    space: str | None = None,
    tag: str | None = None,
    status_filter: str | None = Query(default=None, alias="status"),
    limit: int = Query(default=200, ge=1, le=500),
    offset: int = Query(default=0, ge=0),
) -> PagedPages:
    query = _visible(select(Page).options(*_LOADERS), viewer)
    if space:
        query = query.join(Space).where(Space.slug == space)
    if tag:
        query = query.where(Page.tags.contains([tag]))
    if status_filter and viewer is not None and viewer.can_edit:
        query = query.where(Page.status == status_filter)
    total = await session.scalar(
        select(func.count()).select_from(query.order_by(None).subquery())
    )
    rows = await session.scalars(
        query.order_by(Page.position, Page.title).limit(limit).offset(offset)
    )
    return PagedPages(total=total or 0, limit=limit, offset=offset,
                      items=[PageSummary.model_validate(row) for row in rows])


@router.get("/{slug:path}/revisions", response_model=list[RevisionOut])
async def list_revisions(slug: str, session: SessionDep, viewer: OptionalUser):
    page = await _load(session, slug, viewer)
    rows = await session.scalars(
        select(PageRevision)
        .where(PageRevision.page_id == page.id)
        .options(selectinload(PageRevision.author))
        .order_by(PageRevision.version.desc())
    )
    return list(rows)


@router.get("/{slug:path}/revisions/{version}", response_model=RevisionDetail)
async def get_revision(slug: str, version: int, session: SessionDep, viewer: OptionalUser):
    page = await _load(session, slug, viewer)
    revision = await session.scalar(
        select(PageRevision)
        .where(PageRevision.page_id == page.id, PageRevision.version == version)
        .options(selectinload(PageRevision.author))
    )
    if revision is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, f"No version {version} of '{slug}'.")
    return revision


@router.post("/{slug:path}/revert/{version}", response_model=PageOut)
async def revert(slug: str, version: int, editor: EditorUser, session: SessionDep):
    page = await _load(session, slug, editor)
    revision = await session.scalar(
        select(PageRevision).where(PageRevision.page_id == page.id,
                                   PageRevision.version == version)
    )
    if revision is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, f"No version {version} of '{slug}'.")
    _snapshot(session, page, editor, f"Reverted to version {version}")
    page.title, page.summary, page.body = revision.title, revision.summary, revision.body
    page.version += 1
    page.updated_by_id = editor.id
    session.add(AuditEvent(actor=editor.username, action="page.revert", target=slug,
                           detail={"to_version": version}))
    await session.commit()
    return await _load(session, page.slug, editor)


@router.get("/{slug:path}", response_model=PageOut)
async def get_page(slug: str, session: SessionDep, viewer: OptionalUser):
    return await _load(session, slug, viewer)


def _snapshot(session, page: Page, author: User, comment: str) -> None:
    """Keep the page as it stands now before overwriting it."""
    session.add(PageRevision(
        page_id=page.id, version=page.version, title=page.title, summary=page.summary,
        body=page.body, comment=comment, author_id=author.id,
    ))


@router.post("", response_model=PageOut, status_code=status.HTTP_201_CREATED)
async def create_page(payload: PageWrite, editor: EditorUser, session: SessionDep):
    exists = await session.scalar(select(Page.id).where(Page.slug == payload.slug))
    if exists:
        raise HTTPException(status.HTTP_409_CONFLICT, f"A page already lives at '{payload.slug}'.")
    space = await _resolve_space(session, payload.space_slug)
    page = Page(
        slug=payload.slug, title=payload.title, summary=payload.summary, body=payload.body,
        space_id=space.id if space else None, status=payload.status, position=payload.position,
        tags=payload.tags, origin="authored", version=1,
        created_by_id=editor.id, updated_by_id=editor.id,
    )
    session.add(page)
    session.add(AuditEvent(actor=editor.username, action="page.create", target=payload.slug))
    await session.commit()
    return await _load(session, payload.slug, editor)


@router.patch("/{slug:path}", response_model=PageOut)
async def update_page(slug: str, payload: PagePatch, editor: EditorUser, session: SessionDep):
    page = await _load(session, slug, editor)
    if payload.expected_version is not None and payload.expected_version != page.version:
        raise HTTPException(
            status.HTTP_409_CONFLICT,
            f"This page moved to version {page.version} while you were editing "
            f"version {payload.expected_version}. Reload before saving.",
        )
    changed = any(
        getattr(payload, field) is not None and getattr(payload, field) != getattr(page, field)
        for field in ("title", "summary", "body", "status", "position", "tags")
    )
    if changed:
        _snapshot(session, page, editor, payload.comment)
        page.version += 1
    if payload.slug and payload.slug != page.slug:
        taken = await session.scalar(select(Page.id).where(Page.slug == payload.slug))
        if taken:
            raise HTTPException(status.HTTP_409_CONFLICT,
                                f"A page already lives at '{payload.slug}'.")
        page.slug = payload.slug
    for field in ("title", "summary", "body", "status", "position", "tags"):
        value = getattr(payload, field)
        if value is not None:
            setattr(page, field, value)
    if payload.space_slug is not None:
        space = await _resolve_space(session, payload.space_slug or None)
        page.space_id = space.id if space else None
    page.updated_by_id = editor.id
    session.add(AuditEvent(actor=editor.username, action="page.update", target=page.slug,
                           detail={"version": page.version}))
    await session.commit()
    return await _load(session, page.slug, editor)


@router.delete("/{slug:path}", status_code=status.HTTP_204_NO_CONTENT,
             response_class=Response, response_model=None)
async def delete_page(slug: str, editor: EditorUser, session: SessionDep) -> None:
    page = await _load(session, slug, editor)
    await session.delete(page)
    session.add(AuditEvent(actor=editor.username, action="page.delete", target=slug))
    await session.commit()
