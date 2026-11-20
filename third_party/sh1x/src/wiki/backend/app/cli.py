"""Command line for setting the wiki up and keeping its index fresh.

    python -m app.cli init                 # create tables
    python -m app.cli admin --username me  # create or reset an admin account
    python -m app.cli index                # rebuild the source reference
    python -m app.cli seed                 # load wiki/content/*.md
    python -m app.cli bootstrap            # all of the above, in order
    python -m app.cli stats                # what is currently loaded
"""
from __future__ import annotations

import argparse
import asyncio
import getpass
import os
import secrets
import sys
from pathlib import Path

from sqlalchemy import func, select

from .config import get_settings
from .db import Base, dispose, engine, session_factory
from .indexer.load import load_index
from .models import GscBuiltin, Page, SourceArea, Symbol, User
from .security import hash_password
from .seed.content import seed_content


async def cmd_init() -> None:
    async with engine().begin() as connection:
        await connection.run_sync(Base.metadata.create_all)
    print("schema created")


async def cmd_admin(username: str, password: str | None, display_name: str) -> None:
    generated = False
    if not password:
        password = get_settings().admin_password or os.environ.get("WIKI_ADMIN_PASSWORD", "")
    if not password and sys.stdin.isatty():
        password = getpass.getpass(f"password for {username}: ")
    if not password:
        password = secrets.token_urlsafe(15)
        generated = True
    async with session_factory()() as session:
        user = await session.scalar(select(User).where(User.username == username))
        if user is None:
            user = User(username=username, display_name=display_name or username,
                        role="admin", password_hash=hash_password(password))
            session.add(user)
            action = "created"
        else:
            user.password_hash = hash_password(password)
            user.role, user.is_active = "admin", True
            action = "updated"
        await session.commit()
    print(f"admin {action}: {username}")
    if generated:
        print(f"generated password: {password}")


async def cmd_index(root: Path) -> None:
    async with session_factory()() as session:
        run = await load_index(session, root)
    print(f"indexed {run.files} files, {run.symbols} declarations, {run.builtins} GSC builtins")
    for key, value in sorted(run.summary.get("implementations", {}).items()):
        print(f"  {key:12} {value}")


async def cmd_seed(content_dir: Path, force: bool) -> None:
    async with session_factory()() as session:
        author = await session.scalar(select(User).where(User.role == "admin").limit(1))
        counts = await seed_content(session, content_dir, author, force=force)
    print("content seed: " + ", ".join(f"{key} {value}" for key, value in counts.items()))


async def cmd_stats() -> None:
    async with session_factory()() as session:
        pages = await session.scalar(select(func.count()).select_from(Page))
        symbols = await session.scalar(select(func.count()).select_from(Symbol))
        builtins = await session.scalar(select(func.count()).select_from(GscBuiltin))
        areas = list(await session.scalars(select(SourceArea).order_by(SourceArea.position)))
    print(f"pages {pages} · declarations {symbols} · GSC builtins {builtins}")
    for area in areas:
        flag = "codol" if area.is_codol else "     "
        print(f"  {flag} {area.name:11} {area.file_count:5} files "
              f"{area.line_count:8} lines {area.symbol_count:6} declarations")


def main() -> None:
    settings = get_settings()
    parser = argparse.ArgumentParser(prog="app.cli", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("init", help="create the tables")

    admin = sub.add_parser("admin", help="create or reset an administrator")
    admin.add_argument("--username", default="admin")
    admin.add_argument("--password", default=None,
                       help="prompted for, or taken from WIKI_ADMIN_PASSWORD, "
                            "or generated and printed")
    admin.add_argument("--display-name", default="")

    index = sub.add_parser("index", help="rebuild the source reference")
    index.add_argument("--root", type=Path, default=settings.repo_root)

    seed = sub.add_parser("seed", help="load wiki/content/*.md")
    seed.add_argument("--content", type=Path, default=settings.content_dir)
    seed.add_argument("--force", action="store_true",
                      help="overwrite seed pages that were edited in the browser")

    boot = sub.add_parser("bootstrap", help="init + admin + index + seed")
    boot.add_argument("--username", default="admin")
    boot.add_argument("--password", default=None)
    boot.add_argument("--root", type=Path, default=settings.repo_root)
    boot.add_argument("--content", type=Path, default=settings.content_dir)

    sub.add_parser("stats", help="show what is loaded")

    args = parser.parse_args()

    async def run() -> None:
        try:
            if args.command == "init":
                await cmd_init()
            elif args.command == "admin":
                await cmd_admin(args.username, args.password, args.display_name)
            elif args.command == "index":
                await cmd_index(args.root.resolve())
            elif args.command == "seed":
                await cmd_seed(args.content.resolve(), args.force)
            elif args.command == "stats":
                await cmd_stats()
            elif args.command == "bootstrap":
                await cmd_init()
                await cmd_admin(args.username, args.password, "")
                await cmd_index(args.root.resolve())
                await cmd_seed(args.content.resolve(), False)
        finally:
            await dispose()

    asyncio.run(run())


if __name__ == "__main__":
    main()
