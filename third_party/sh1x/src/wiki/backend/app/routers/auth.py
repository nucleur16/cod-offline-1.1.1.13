"""Sign-in and account management."""
from __future__ import annotations

from datetime import datetime

from fastapi import APIRouter, HTTPException, Response, status
from sqlalchemy import func, select

from ..deps import AdminUser, CurrentUser, SessionDep
from ..models import AuditEvent, User
from ..schemas import (
    LoginRequest,
    PasswordChange,
    TokenResponse,
    UserCreate,
    UserOut,
    UserUpdate,
)
from ..security import create_access_token, hash_password, verify_password

router = APIRouter(prefix="/auth", tags=["auth"])


@router.post("/login", response_model=TokenResponse)
async def login(payload: LoginRequest, session: SessionDep) -> TokenResponse:
    user = await session.scalar(
        select(User).where(func.lower(User.username) == payload.username.strip().lower())
    )
    # Verify even when the user is missing so a wrong username and a wrong
    # password take the same time to answer.
    known_hash = user.password_hash if user else "$2b$12$" + "." * 53
    valid = verify_password(payload.password, known_hash)
    if not user or not valid or not user.is_active:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Incorrect username or password.")
    token, expires_in = create_access_token(user.username, user.role)
    user.last_login_at = datetime.now()
    session.add(AuditEvent(actor=user.username, action="login", target=user.username))
    await session.commit()
    return TokenResponse(access_token=token, expires_in=expires_in,
                         user=UserOut.model_validate(user))


@router.get("/me", response_model=UserOut)
async def me(user: CurrentUser) -> User:
    return user


@router.post("/password", status_code=status.HTTP_204_NO_CONTENT,
             response_class=Response, response_model=None)
async def change_password(payload: PasswordChange, user: CurrentUser,
                          session: SessionDep) -> None:
    if not verify_password(payload.current_password, user.password_hash):
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "Current password is incorrect.")
    user.password_hash = hash_password(payload.new_password)
    session.add(AuditEvent(actor=user.username, action="password", target=user.username))
    await session.commit()


@router.get("/users", response_model=list[UserOut])
async def list_users(_: AdminUser, session: SessionDep) -> list[User]:
    return list(await session.scalars(select(User).order_by(User.username)))


@router.post("/users", response_model=UserOut, status_code=status.HTTP_201_CREATED)
async def create_user(payload: UserCreate, admin: AdminUser, session: SessionDep) -> User:
    exists = await session.scalar(
        select(User).where(func.lower(User.username) == payload.username.lower())
    )
    if exists:
        raise HTTPException(status.HTTP_409_CONFLICT, "That username is taken.")
    user = User(
        username=payload.username,
        display_name=payload.display_name or payload.username,
        email=payload.email,
        role=payload.role,
        password_hash=hash_password(payload.password),
    )
    session.add(user)
    session.add(AuditEvent(actor=admin.username, action="user.create", target=payload.username))
    await session.commit()
    await session.refresh(user)
    return user


@router.patch("/users/{username}", response_model=UserOut)
async def update_user(username: str, payload: UserUpdate, admin: AdminUser,
                      session: SessionDep) -> User:
    user = await session.scalar(select(User).where(User.username == username))
    if user is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "No such user.")
    if user.id == admin.id and payload.role and payload.role != "admin":
        raise HTTPException(status.HTTP_400_BAD_REQUEST,
                            "Removing your own administrator role would lock you out.")
    if payload.display_name is not None:
        user.display_name = payload.display_name
    if payload.email is not None:
        user.email = payload.email
    if payload.role is not None:
        user.role = payload.role
    if payload.is_active is not None:
        user.is_active = payload.is_active
    if payload.password:
        user.password_hash = hash_password(payload.password)
    session.add(AuditEvent(actor=admin.username, action="user.update", target=username))
    await session.commit()
    await session.refresh(user)
    return user


@router.delete("/users/{username}", status_code=status.HTTP_204_NO_CONTENT,
             response_class=Response, response_model=None)
async def delete_user(username: str, admin: AdminUser, session: SessionDep) -> None:
    user = await session.scalar(select(User).where(User.username == username))
    if user is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "No such user.")
    if user.id == admin.id:
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "You cannot delete your own account.")
    await session.delete(user)
    session.add(AuditEvent(actor=admin.username, action="user.delete", target=username))
    await session.commit()
