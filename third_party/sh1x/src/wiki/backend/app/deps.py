"""Authentication dependencies."""
from __future__ import annotations

from typing import Annotated

from fastapi import Depends, HTTPException, Request, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from .db import get_session
from .models import User
from .security import TokenError, decode_access_token

SessionDep = Annotated[AsyncSession, Depends(get_session)]


def _bearer(request: Request) -> str | None:
    header = request.headers.get("authorization", "")
    if header.lower().startswith("bearer "):
        return header[7:].strip()
    # The Next.js frontend keeps the token in an httpOnly cookie and proxies
    # requests, so accept it there too for direct browser calls.
    return request.cookies.get("wiki_token")


async def current_user_optional(request: Request, session: SessionDep) -> User | None:
    token = _bearer(request)
    if not token:
        return None
    try:
        payload = decode_access_token(token)
    except TokenError:
        return None
    username = payload.get("sub") or ""
    user = await session.scalar(select(User).where(User.username == username))
    if user is None or not user.is_active:
        return None
    return user


OptionalUser = Annotated["User | None", Depends(current_user_optional)]


async def current_user(user: OptionalUser) -> User:
    if user is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Sign in to continue.",
            headers={"WWW-Authenticate": "Bearer"},
        )
    return user


CurrentUser = Annotated[User, Depends(current_user)]


async def require_editor(user: CurrentUser) -> User:
    if not user.can_edit:
        raise HTTPException(status.HTTP_403_FORBIDDEN,
                            "This account may read the wiki but not change it.")
    return user


async def require_admin(user: CurrentUser) -> User:
    if not user.is_admin:
        raise HTTPException(status.HTTP_403_FORBIDDEN, "Administrator access required.")
    return user


EditorUser = Annotated[User, Depends(require_editor)]
AdminUser = Annotated[User, Depends(require_admin)]
