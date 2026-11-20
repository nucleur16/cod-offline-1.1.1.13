"""Request and response models."""
from __future__ import annotations

from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field

ORM = ConfigDict(from_attributes=True)


class LoginRequest(BaseModel):
    username: str
    password: str


class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    expires_in: int
    user: "UserOut"


class UserOut(BaseModel):
    model_config = ORM

    id: int
    username: str
    display_name: str
    email: str
    role: str
    is_active: bool
    created_at: datetime | None = None
    last_login_at: datetime | None = None


class UserCreate(BaseModel):
    username: str = Field(min_length=2, max_length=64, pattern=r"^[A-Za-z0-9_.-]+$")
    password: str = Field(min_length=8, max_length=72)
    display_name: str = ""
    email: str = ""
    role: str = Field(default="editor", pattern=r"^(admin|editor|reader)$")


class UserUpdate(BaseModel):
    display_name: str | None = None
    email: str | None = None
    role: str | None = Field(default=None, pattern=r"^(admin|editor|reader)$")
    is_active: bool | None = None
    password: str | None = Field(default=None, min_length=8, max_length=72)


class PasswordChange(BaseModel):
    current_password: str
    new_password: str = Field(min_length=8, max_length=72)


class SpaceOut(BaseModel):
    model_config = ORM

    id: int
    slug: str
    title: str
    summary: str
    source_area: str
    position: int


class SpaceWrite(BaseModel):
    slug: str = Field(min_length=1, max_length=64, pattern=r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
    title: str = Field(min_length=1, max_length=128)
    summary: str = ""
    source_area: str = ""
    position: int = 0


class PageSummary(BaseModel):
    model_config = ORM

    id: int
    slug: str
    title: str
    summary: str
    status: str
    origin: str
    position: int
    version: int
    tags: list[str] = []
    updated_at: datetime | None = None
    space: SpaceOut | None = None


class PageOut(PageSummary):
    body: str
    created_at: datetime | None = None
    updated_by: UserOut | None = None
    created_by: UserOut | None = None


class PageWrite(BaseModel):
    slug: str = Field(min_length=1, max_length=160, pattern=r"^[a-z0-9]+(?:[-/][a-z0-9]+)*$")
    title: str = Field(min_length=1, max_length=200)
    body: str = ""
    summary: str = ""
    space_slug: str | None = None
    status: str = Field(default="published", pattern=r"^(published|draft)$")
    position: int = 0
    tags: list[str] = []
    comment: str = ""


class PagePatch(BaseModel):
    slug: str | None = Field(default=None, max_length=160,
                             pattern=r"^[a-z0-9]+(?:[-/][a-z0-9]+)*$")
    title: str | None = Field(default=None, min_length=1, max_length=200)
    body: str | None = None
    summary: str | None = None
    space_slug: str | None = None
    status: str | None = Field(default=None, pattern=r"^(published|draft)$")
    position: int | None = None
    tags: list[str] | None = None
    comment: str = ""
    #: Optimistic concurrency: refuse the write if the page moved on since load.
    expected_version: int | None = None


class RevisionOut(BaseModel):
    model_config = ORM

    id: int
    version: int
    title: str
    summary: str
    comment: str
    created_at: datetime | None = None
    author: UserOut | None = None


class RevisionDetail(RevisionOut):
    body: str


class SymbolSummary(BaseModel):
    model_config = ORM

    id: int
    name: str
    kind: str
    area: str
    file: str
    line: int
    signature: str
    implementation: str
    is_codol: bool


class SymbolOut(SymbolSummary):
    end_line: int
    doc: str
    declaration: str
    detail: dict[str, Any] = {}


class BuiltinOut(BaseModel):
    model_config = ORM

    id: int
    name: str
    kind: str
    binding: str
    area: str
    implementation: str
    developer_only: bool
    registered_file: str
    registered_line: int
    binding_file: str
    binding_line: int
    signature: str
    doc: str


class AreaOut(BaseModel):
    model_config = ORM

    name: str
    title: str
    summary: str
    path: str
    is_codol: bool
    file_count: int
    line_count: int
    symbol_count: int
    languages: dict[str, int] = {}
    position: int


class SourceFileOut(BaseModel):
    model_config = ORM

    path: str
    area: str
    language: str
    role: str
    lines: int
    bytes: int
    sha256: str
    symbol_count: int


class Paged(BaseModel):
    total: int
    limit: int
    offset: int


class PagedSymbols(Paged):
    items: list[SymbolSummary]


class PagedBuiltins(Paged):
    items: list[BuiltinOut]


class PagedPages(Paged):
    items: list[PageSummary]


class SearchHit(BaseModel):
    type: str           # page | symbol | builtin
    title: str
    subtitle: str
    href: str
    snippet: str
    rank: float


class SearchResponse(BaseModel):
    query: str
    hits: list[SearchHit]
    counts: dict[str, int]


class IndexRunOut(BaseModel):
    model_config = ORM

    id: int
    started_at: datetime | None = None
    finished_at: datetime | None = None
    repo_root: str
    git_commit: str
    files: int
    symbols: int
    builtins: int
    summary: dict[str, Any] = {}


class SiteStats(BaseModel):
    site_name: str
    tagline: str
    pages: int
    spaces: int
    symbols: int
    builtins: int
    files: int
    lines: int
    implementations: dict[str, int]
    last_index: IndexRunOut | None = None


TokenResponse.model_rebuild()
