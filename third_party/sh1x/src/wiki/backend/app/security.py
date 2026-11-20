"""Password hashing and session tokens.

``bcrypt`` is used directly rather than through passlib: passlib 1.7.x reads a
private attribute that modern bcrypt releases no longer expose, and that
mismatch fails at hash time rather than at import time.
"""
from __future__ import annotations

from datetime import datetime, timedelta, timezone

import bcrypt
import jwt

from .config import get_settings

ALGORITHM = "HS256"
#: bcrypt silently truncates at 72 bytes; reject rather than hash a prefix.
MAX_PASSWORD_BYTES = 72


class TokenError(Exception):
    pass


def hash_password(password: str) -> str:
    raw = password.encode("utf-8")
    if len(raw) > MAX_PASSWORD_BYTES:
        raise ValueError(f"password must be at most {MAX_PASSWORD_BYTES} bytes")
    return bcrypt.hashpw(raw, bcrypt.gensalt()).decode("ascii")


def verify_password(password: str, password_hash: str) -> bool:
    try:
        return bcrypt.checkpw(password.encode("utf-8"), password_hash.encode("ascii"))
    except ValueError:
        return False


def create_access_token(subject: str, role: str, minutes: int | None = None) -> tuple[str, int]:
    settings = get_settings()
    lifetime = minutes if minutes is not None else settings.access_token_minutes
    expires = datetime.now(timezone.utc) + timedelta(minutes=lifetime)
    payload = {
        "sub": subject,
        "role": role,
        "iat": int(datetime.now(timezone.utc).timestamp()),
        "exp": int(expires.timestamp()),
    }
    token = jwt.encode(payload, settings.secret_key, algorithm=ALGORITHM)
    return token, lifetime * 60


def decode_access_token(token: str) -> dict:
    try:
        return jwt.decode(token, get_settings().secret_key, algorithms=[ALGORITHM])
    except jwt.PyJWTError as error:  # expired, malformed, wrong signature
        raise TokenError(str(error)) from error
