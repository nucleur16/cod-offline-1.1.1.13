"""Seed the wiki from the markdown under ``wiki/content/``.

Each file starts with a small frontmatter block::

    ---
    slug: snapshot-pipeline
    title: The snapshot pipeline
    space: subsystems
    summary: How one server frame becomes one client snapshot.
    tags: [snapshot, net]
    position: 20
    ---

Seeding is idempotent and never silently discards browser edits: a seed page
that someone has since edited (``version > 1``) is left alone unless ``force``
is set, in which case the current text is filed as a revision first.
"""
from __future__ import annotations

import re
from pathlib import Path

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from ..models import Page, PageRevision, Space, User

FRONTMATTER = re.compile(r"^---\s*\n(?P<meta>.*?)\n---\s*\n?(?P<body>.*)$", re.S)


def parse_scalar(raw: str):
    value = raw.strip()
    if value.startswith("[") and value.endswith("]"):
        inner = value[1:-1].strip()
        return [item.strip().strip("'\"") for item in inner.split(",") if item.strip()]
    if value.lower() in {"true", "false"}:
        return value.lower() == "true"
    if re.fullmatch(r"-?\d+", value):
        return int(value)
    return value.strip("'\"")


def parse_document(text: str) -> tuple[dict, str]:
    match = FRONTMATTER.match(text)
    if not match:
        return {}, text
    meta: dict = {}
    for line in match.group("meta").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or ":" not in line:
            continue
        key, _, value = line.partition(":")
        meta[key.strip()] = parse_scalar(value)
    return meta, match.group("body").lstrip("\n")


DEFAULT_SPACES = [
    {"slug": "start", "title": "Start here", "position": 10,
     "summary": "What this server is, how it is laid out, and how to build and run it."},
    {"slug": "architecture", "title": "Architecture", "position": 20,
     "summary": "How a frame, a connection and a script call move through the server."},
    {"slug": "subsystems", "title": "Subsystems", "position": 30,
     "summary": "One page per source area: what it owns, its types and its entry points."},
    {"slug": "reference", "title": "Reference", "position": 40,
     "summary": "Guides to the generated index: declarations, GSC builtins, source files."},
    {"slug": "operations", "title": "Operations", "position": 50,
     "summary": "Building, launching, logging, testing and debugging the server."},
    {"slug": "wiki", "title": "About this wiki", "position": 60,
     "summary": "How the wiki itself is built, indexed and edited."},
]


async def ensure_spaces(session: AsyncSession) -> dict[str, Space]:
    existing = {space.slug: space for space in await session.scalars(select(Space))}
    for entry in DEFAULT_SPACES:
        space = existing.get(entry["slug"])
        if space is None:
            space = Space(source_area="", **entry)
            session.add(space)
            existing[entry["slug"]] = space
    await session.commit()
    return {space.slug: space for space in await session.scalars(select(Space))}


async def seed_content(session: AsyncSession, content_dir: Path, author: User | None = None,
                       force: bool = False) -> dict[str, int]:
    """Create or refresh the seed pages. Returns a count per outcome."""
    spaces = await ensure_spaces(session)
    counts = {"created": 0, "updated": 0, "kept": 0, "skipped": 0}
    if not content_dir.exists():
        return counts

    for path in sorted(content_dir.rglob("*.md")):
        meta, body = parse_document(path.read_text(encoding="utf-8"))
        slug = str(meta.get("slug") or path.stem)
        if not slug:
            counts["skipped"] += 1
            continue
        space = spaces.get(str(meta.get("space", "")))
        title = str(meta.get("title") or slug.replace("-", " ").title())
        summary = str(meta.get("summary", ""))
        tags = meta.get("tags") or []
        position = int(meta.get("position", 0) or 0)

        page = await session.scalar(select(Page).where(Page.slug == slug))
        if page is None:
            session.add(Page(
                slug=slug, title=title, summary=summary, body=body,
                space_id=space.id if space else None, origin="seed", status="published",
                position=position, tags=list(tags), version=1,
                created_by_id=author.id if author else None,
                updated_by_id=author.id if author else None,
            ))
            counts["created"] += 1
            continue
        if page.version > 1 and not force:
            counts["kept"] += 1  # Someone edited it in the browser; leave it be.
            continue
        if (page.title, page.summary, page.body, page.position) == (title, summary, body, position):
            counts["kept"] += 1
            continue
        session.add(PageRevision(
            page_id=page.id, version=page.version, title=page.title, summary=page.summary,
            body=page.body, comment="Replaced by content seed", author_id=author.id if author else None,
        ))
        page.title, page.summary, page.body = title, summary, body
        page.position, page.tags = position, list(tags)
        if space:
            page.space_id = space.id
        page.version += 1
        counts["updated"] += 1

    await session.commit()
    return counts
