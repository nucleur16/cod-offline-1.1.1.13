"""Database schema.

Two kinds of content live side by side:

* **Authored pages** — written by people, editable in the browser by an admin,
  every save keeping a full revision.
* **Generated reference** — areas, files, symbols and GSC builtins produced by
  :mod:`app.indexer` from the checked-in source. These are rebuilt by the
  indexer and are read-only in the UI, because the source is the truth.

A page may cite generated symbols (``page_symbols``), which is how a hand
written subsystem note links to the structs and functions it describes.
"""
from __future__ import annotations

from datetime import datetime

from sqlalchemy import (
    Boolean,
    Computed,
    DateTime,
    ForeignKey,
    Index,
    Integer,
    String,
    Text,
    UniqueConstraint,
    func,
)
from sqlalchemy.dialects.postgresql import JSONB, TSVECTOR
from sqlalchemy.orm import Mapped, mapped_column, relationship

from .db import Base

ROLES = ("admin", "editor", "reader")


def _now() -> datetime:
    return datetime.now()


class User(Base):
    __tablename__ = "users"

    id: Mapped[int] = mapped_column(primary_key=True)
    username: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    display_name: Mapped[str] = mapped_column(String(128), default="")
    email: Mapped[str] = mapped_column(String(255), default="")
    password_hash: Mapped[str] = mapped_column(String(255))
    #: ``admin`` may write anything, ``editor`` may edit pages, ``reader`` may not.
    role: Mapped[str] = mapped_column(String(16), default="reader")
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    last_login_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    @property
    def can_edit(self) -> bool:
        return self.is_active and self.role in {"admin", "editor"}

    @property
    def is_admin(self) -> bool:
        return self.is_active and self.role == "admin"


class Space(Base):
    """A top-level section of the wiki (Architecture, Subsystems, Reference...)."""

    __tablename__ = "spaces"

    id: Mapped[int] = mapped_column(primary_key=True)
    slug: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    title: Mapped[str] = mapped_column(String(128))
    summary: Mapped[str] = mapped_column(Text, default="")
    #: Optional link to a source area (``net``, ``snapshot``, ...).
    source_area: Mapped[str] = mapped_column(String(32), default="")
    position: Mapped[int] = mapped_column(Integer, default=0)

    pages: Mapped[list["Page"]] = relationship(back_populates="space")


class Page(Base):
    __tablename__ = "pages"

    id: Mapped[int] = mapped_column(primary_key=True)
    slug: Mapped[str] = mapped_column(String(160), unique=True, index=True)
    title: Mapped[str] = mapped_column(String(200))
    summary: Mapped[str] = mapped_column(Text, default="")
    body: Mapped[str] = mapped_column(Text, default="")
    space_id: Mapped[int | None] = mapped_column(ForeignKey("spaces.id", ondelete="SET NULL"))
    #: ``seed`` pages ship with the repository; ``authored`` pages were created
    #: in the browser. Both are editable; the origin is kept for provenance.
    origin: Mapped[str] = mapped_column(String(16), default="authored")
    status: Mapped[str] = mapped_column(String(16), default="published")
    position: Mapped[int] = mapped_column(Integer, default=0)
    version: Mapped[int] = mapped_column(Integer, default=1)
    tags: Mapped[list] = mapped_column(JSONB, default=list)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now()
    )
    created_by_id: Mapped[int | None] = mapped_column(ForeignKey("users.id", ondelete="SET NULL"))
    updated_by_id: Mapped[int | None] = mapped_column(ForeignKey("users.id", ondelete="SET NULL"))

    search_vector: Mapped[str] = mapped_column(
        TSVECTOR,
        Computed(
            "setweight(to_tsvector('english', coalesce(title, '')), 'A') || "
            "setweight(to_tsvector('english', coalesce(summary, '')), 'B') || "
            "setweight(to_tsvector('english', coalesce(body, '')), 'C')",
            persisted=True,
        ),
    )

    space: Mapped[Space | None] = relationship(back_populates="pages")
    revisions: Mapped[list["PageRevision"]] = relationship(
        back_populates="page", cascade="all, delete-orphan",
        order_by="PageRevision.version.desc()",
    )
    created_by: Mapped[User | None] = relationship(foreign_keys=[created_by_id])
    updated_by: Mapped[User | None] = relationship(foreign_keys=[updated_by_id])

    __table_args__ = (
        Index("ix_pages_search", search_vector, postgresql_using="gin"),
        Index("ix_pages_space_position", "space_id", "position"),
    )


class PageRevision(Base):
    """A full snapshot of a page as it was before a save."""

    __tablename__ = "page_revisions"

    id: Mapped[int] = mapped_column(primary_key=True)
    page_id: Mapped[int] = mapped_column(ForeignKey("pages.id", ondelete="CASCADE"), index=True)
    version: Mapped[int] = mapped_column(Integer)
    title: Mapped[str] = mapped_column(String(200))
    summary: Mapped[str] = mapped_column(Text, default="")
    body: Mapped[str] = mapped_column(Text, default="")
    comment: Mapped[str] = mapped_column(String(500), default="")
    author_id: Mapped[int | None] = mapped_column(ForeignKey("users.id", ondelete="SET NULL"))
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())

    page: Mapped[Page] = relationship(back_populates="revisions")
    author: Mapped[User | None] = relationship()

    __table_args__ = (UniqueConstraint("page_id", "version", name="uq_revision_version"),)


class SourceArea(Base):
    """One directory family of the server source; see ``docs/CODOL_SOURCE_LAYOUT.md``."""

    __tablename__ = "source_areas"

    id: Mapped[int] = mapped_column(primary_key=True)
    name: Mapped[str] = mapped_column(String(32), unique=True, index=True)
    title: Mapped[str] = mapped_column(String(128), default="")
    summary: Mapped[str] = mapped_column(Text, default="")
    path: Mapped[str] = mapped_column(String(160), default="")
    is_codol: Mapped[bool] = mapped_column(Boolean, default=False)
    file_count: Mapped[int] = mapped_column(Integer, default=0)
    line_count: Mapped[int] = mapped_column(Integer, default=0)
    symbol_count: Mapped[int] = mapped_column(Integer, default=0)
    languages: Mapped[dict] = mapped_column(JSONB, default=dict)
    position: Mapped[int] = mapped_column(Integer, default=0)


class SourceFile(Base):
    __tablename__ = "source_files"

    id: Mapped[int] = mapped_column(primary_key=True)
    path: Mapped[str] = mapped_column(String(400), unique=True, index=True)
    area: Mapped[str] = mapped_column(String(32), index=True)
    language: Mapped[str] = mapped_column(String(8))
    role: Mapped[str] = mapped_column(String(16))
    lines: Mapped[int] = mapped_column(Integer, default=0)
    bytes: Mapped[int] = mapped_column(Integer, default=0)
    sha256: Mapped[str] = mapped_column(String(64))
    symbol_count: Mapped[int] = mapped_column(Integer, default=0)


class Symbol(Base):
    """A declaration found in the source: struct, union, enum, class, typedef,
    function, prototype, macro — or an assembly label for retained x86."""

    __tablename__ = "symbols"

    id: Mapped[int] = mapped_column(primary_key=True)
    name: Mapped[str] = mapped_column(String(200), index=True)
    lower_name: Mapped[str] = mapped_column(String(200), index=True)
    kind: Mapped[str] = mapped_column(String(16), index=True)
    area: Mapped[str] = mapped_column(String(32), index=True)
    file: Mapped[str] = mapped_column(String(400), index=True)
    line: Mapped[int] = mapped_column(Integer)
    end_line: Mapped[int] = mapped_column(Integer)
    signature: Mapped[str] = mapped_column(Text, default="")
    doc: Mapped[str] = mapped_column(Text, default="")
    declaration: Mapped[str] = mapped_column(Text, default="")
    #: ``c`` | ``cpp`` | ``x86-asm`` | ``unresolved`` for callables, else empty.
    implementation: Mapped[str] = mapped_column(String(16), default="", index=True)
    is_codol: Mapped[bool] = mapped_column(Boolean, default=False, index=True)
    detail: Mapped[dict] = mapped_column(JSONB, default=dict)

    search_vector: Mapped[str] = mapped_column(
        TSVECTOR,
        Computed(
            "setweight(to_tsvector('english', coalesce(name, '')), 'A') || "
            "setweight(to_tsvector('english', coalesce(signature, '')), 'B') || "
            "setweight(to_tsvector('english', coalesce(doc, '')), 'C')",
            persisted=True,
        ),
    )

    __table_args__ = (
        UniqueConstraint("name", "kind", "file", "line", name="uq_symbol_site"),
        Index("ix_symbols_search", search_vector, postgresql_using="gin"),
        Index("ix_symbols_kind_area", "kind", "area"),
    )


class GscBuiltin(Base):
    """A GSC name registered with ``Scr_AddFunction`` / ``Scr_AddMethod``."""

    __tablename__ = "gsc_builtins"

    id: Mapped[int] = mapped_column(primary_key=True)
    name: Mapped[str] = mapped_column(String(128), index=True)
    #: ``function`` is called bare; ``method`` is called on an entity receiver.
    kind: Mapped[str] = mapped_column(String(8), index=True)
    binding: Mapped[str] = mapped_column(String(200), index=True)
    area: Mapped[str] = mapped_column(String(32), index=True, default="")
    implementation: Mapped[str] = mapped_column(String(16), default="", index=True)
    developer_only: Mapped[bool] = mapped_column(Boolean, default=False)
    developer_expression: Mapped[str] = mapped_column(String(64), default="")
    registered_file: Mapped[str] = mapped_column(String(400), default="")
    registered_line: Mapped[int] = mapped_column(Integer, default=0)
    binding_file: Mapped[str] = mapped_column(String(400), default="")
    binding_line: Mapped[int] = mapped_column(Integer, default=0)
    signature: Mapped[str] = mapped_column(Text, default="")
    doc: Mapped[str] = mapped_column(Text, default="")

    search_vector: Mapped[str] = mapped_column(
        TSVECTOR,
        Computed(
            "setweight(to_tsvector('english', coalesce(name, '')), 'A') || "
            "setweight(to_tsvector('english', coalesce(binding, '')), 'B') || "
            "setweight(to_tsvector('english', coalesce(doc, '')), 'C')",
            persisted=True,
        ),
    )

    __table_args__ = (
        UniqueConstraint("name", "kind", "registered_file", "registered_line",
                         name="uq_builtin_site"),
        Index("ix_builtins_search", search_vector, postgresql_using="gin"),
    )


class PageSymbol(Base):
    """Links an authored page to a generated symbol it documents."""

    __tablename__ = "page_symbols"

    page_id: Mapped[int] = mapped_column(ForeignKey("pages.id", ondelete="CASCADE"),
                                         primary_key=True)
    symbol_id: Mapped[int] = mapped_column(ForeignKey("symbols.id", ondelete="CASCADE"),
                                           primary_key=True)


class IndexRun(Base):
    """Provenance for one run of the source indexer."""

    __tablename__ = "index_runs"

    id: Mapped[int] = mapped_column(primary_key=True)
    started_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    finished_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    repo_root: Mapped[str] = mapped_column(String(400), default="")
    git_commit: Mapped[str] = mapped_column(String(64), default="")
    files: Mapped[int] = mapped_column(Integer, default=0)
    symbols: Mapped[int] = mapped_column(Integer, default=0)
    builtins: Mapped[int] = mapped_column(Integer, default=0)
    summary: Mapped[dict] = mapped_column(JSONB, default=dict)


class AuditEvent(Base):
    __tablename__ = "audit_events"

    id: Mapped[int] = mapped_column(primary_key=True)
    at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    actor: Mapped[str] = mapped_column(String(64), default="")
    action: Mapped[str] = mapped_column(String(32))
    target: Mapped[str] = mapped_column(String(200), default="")
    detail: Mapped[dict] = mapped_column(JSONB, default=dict)
