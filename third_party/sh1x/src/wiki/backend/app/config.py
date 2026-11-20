"""Runtime settings, read from the environment (or ``wiki/.env``)."""
from __future__ import annotations

from functools import lru_cache
from pathlib import Path

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict

BACKEND_DIR = Path(__file__).resolve().parent.parent
WIKI_DIR = BACKEND_DIR.parent
REPO_ROOT = WIKI_DIR.parent


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=(WIKI_DIR / ".env", BACKEND_DIR / ".env"),
        env_prefix="WIKI_",
        extra="ignore",
    )

    database_url: str = Field(
        default="postgresql+asyncpg://wiki:wiki@localhost:5432/sh1x_wiki",
        description="SQLAlchemy async URL; must use the asyncpg driver.",
    )
    #: Signs the session JWTs. Override in any deployment that is not a laptop.
    secret_key: str = "change-me-in-.env-before-exposing-this-wiki"
    access_token_minutes: int = 60 * 12
    cors_origins: str = "http://localhost:3000,http://127.0.0.1:3000"

    #: The server source tree this wiki documents.
    repo_root: Path = REPO_ROOT
    #: Authored markdown that seeds the wiki on first run.
    content_dir: Path = WIKI_DIR / "content"

    #: Only these run migrations/seeds implicitly; production should use the CLI.
    auto_create_schema: bool = True

    #: Optional bootstrap password for `python -m app.cli admin`.
    admin_password: str = ""

    site_name: str = "sh1x server wiki"
    site_tagline: str = "Call of Duty Online Closed Alpha Server — engineering reference"

    @property
    def origins(self) -> list[str]:
        return [origin.strip() for origin in self.cors_origins.split(",") if origin.strip()]


@lru_cache
def get_settings() -> Settings:
    return Settings()
