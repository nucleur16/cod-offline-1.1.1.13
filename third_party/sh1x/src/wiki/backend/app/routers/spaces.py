"""Spaces: the top-level sections pages are filed under."""
from __future__ import annotations

from fastapi import APIRouter, HTTPException, Response, status
from sqlalchemy import select

from ..deps import AdminUser, SessionDep
from ..models import AuditEvent, Page, Space
from ..schemas import SpaceOut, SpaceWrite

router = APIRouter(prefix="/spaces", tags=["spaces"])


@router.get("", response_model=list[SpaceOut])
async def list_spaces(session: SessionDep):
    return list(await session.scalars(select(Space).order_by(Space.position, Space.title)))


@router.post("", response_model=SpaceOut, status_code=status.HTTP_201_CREATED)
async def create_space(payload: SpaceWrite, admin: AdminUser, session: SessionDep):
    if await session.scalar(select(Space.id).where(Space.slug == payload.slug)):
        raise HTTPException(status.HTTP_409_CONFLICT, f"Space '{payload.slug}' already exists.")
    space = Space(**payload.model_dump())
    session.add(space)
    session.add(AuditEvent(actor=admin.username, action="space.create", target=payload.slug))
    await session.commit()
    await session.refresh(space)
    return space


@router.patch("/{slug}", response_model=SpaceOut)
async def update_space(slug: str, payload: SpaceWrite, admin: AdminUser, session: SessionDep):
    space = await session.scalar(select(Space).where(Space.slug == slug))
    if space is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, f"No space '{slug}'.")
    for field, value in payload.model_dump().items():
        setattr(space, field, value)
    session.add(AuditEvent(actor=admin.username, action="space.update", target=slug))
    await session.commit()
    await session.refresh(space)
    return space


@router.delete("/{slug}", status_code=status.HTTP_204_NO_CONTENT,
             response_class=Response, response_model=None)
async def delete_space(slug: str, admin: AdminUser, session: SessionDep) -> None:
    space = await session.scalar(select(Space).where(Space.slug == slug))
    if space is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, f"No space '{slug}'.")
    pages = await session.scalar(select(Page.id).where(Page.space_id == space.id).limit(1))
    if pages:
        raise HTTPException(status.HTTP_409_CONFLICT,
                            "Move or delete this space's pages before deleting the space.")
    await session.delete(space)
    session.add(AuditEvent(actor=admin.username, action="space.delete", target=slug))
    await session.commit()
