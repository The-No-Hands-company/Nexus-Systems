"""Nexus-Wiki authentication — JWT tokens + bcrypt passwords."""
from __future__ import annotations

from datetime import UTC, datetime, timedelta
from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from jose import JWTError, jwt
from passlib.context import CryptContext
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from .database import WikiUser, get_session

SECRET_KEY = "nexus-wiki-dev-secret-change-in-production"
ALGORITHM = "HS256"
TOKEN_EXPIRE = timedelta(hours=24)

pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")
bearer = HTTPBearer(auto_error=False)

def hash_password(password: str) -> str:
    return pwd_context.hash(password)

def verify_password(password: str, hashed: str) -> bool:
    return pwd_context.verify(password, hashed)

def create_token(user_id: str, username: str) -> str:
    return jwt.encode(
        {"sub": user_id, "username": username, "exp": datetime.now(UTC) + TOKEN_EXPIRE},
        SECRET_KEY, algorithm=ALGORITHM
    )

def decode_token(token: str) -> dict | None:
    try:
        return jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
    except JWTError:
        return None

async def current_user(
    credentials: HTTPAuthorizationCredentials | None = Depends(bearer),
    session: AsyncSession = Depends(get_session),
) -> dict | None:
    """Extract current user from JWT token. Returns None if not authenticated."""
    if not credentials:
        return None
    payload = decode_token(credentials.credentials)
    if not payload:
        return None
    user_id = payload.get("sub")
    if not user_id:
        return None
    result = await session.execute(select(WikiUser).where(WikiUser.id == user_id))
    user = result.scalar_one_or_none()
    if not user:
        return None
    return {"user_id": user.id, "username": user.username, "is_admin": bool(user.is_admin)}

async def require_user(user: dict | None = Depends(current_user)) -> dict:
    if not user:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Authentication required")
    return user
