"""Nexus-Wiki database layer — PostgreSQL with SQLAlchemy async."""
from __future__ import annotations

from datetime import UTC, datetime
from sqlalchemy import Column, String, Integer, DateTime, Text, ForeignKey, Index, func
from sqlalchemy.orm import DeclarativeBase, relationship
from sqlalchemy.dialects.postgresql import TSVECTOR
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession
from sqlalchemy import select, update, delete, insert, text
import re

def utcnow() -> datetime:
    return datetime.now(UTC)

def slugify(value: str) -> str:
    compact = re.sub(r"\s+", "-", value.strip().lower())
    cleaned = re.sub(r"[^a-z0-9\-]", "", compact)
    normalized = re.sub(r"\-+", "-", cleaned).strip("-")
    if not normalized:
        raise ValueError("Could not derive slug from title")
    return normalized

# ── Models ─────────────────────────────────────────────────────

class Base(DeclarativeBase):
    pass

class WikiPage(Base):
    __tablename__ = "wiki_pages"

    slug = Column(String(300), primary_key=True)
    title = Column(String(300), nullable=False)
    content = Column(Text, nullable=False)
    content_html = Column(Text, nullable=True)
    category = Column(String(200), nullable=True, index=True)
    tags = Column(String(500), nullable=True)  # comma-separated
    version = Column(Integer, nullable=False, default=1)
    editor_id = Column(String(100), nullable=True)
    change_summary = Column(String(500), nullable=True)
    created_at = Column(DateTime(timezone=True), nullable=False, default=utcnow)
    updated_at = Column(DateTime(timezone=True), nullable=False, default=utcnow, onupdate=utcnow)
    # Full-text search vector
    search_vector = Column(TSVECTOR, nullable=True)

class PageRevision(Base):
    __tablename__ = "wiki_page_revisions"

    id = Column(Integer, primary_key=True, autoincrement=True)
    slug = Column(String(300), ForeignKey("wiki_pages.slug", ondelete="CASCADE"), nullable=False, index=True)
    version = Column(Integer, nullable=False)
    title = Column(String(300), nullable=False)
    content = Column(Text, nullable=False)
    editor_id = Column(String(100), nullable=False, default="anonymous")
    change_summary = Column(String(500), nullable=False, default="")
    created_at = Column(DateTime(timezone=True), nullable=False, default=utcnow)

class WikiUser(Base):
    __tablename__ = "wiki_users"

    id = Column(String(64), primary_key=True)
    username = Column(String(100), unique=True, nullable=False)
    password_hash = Column(String(200), nullable=False)
    is_admin = Column(Integer, nullable=False, default=False)
    created_at = Column(DateTime(timezone=True), nullable=False, default=utcnow)

class WikiUpload(Base):
    __tablename__ = "wiki_uploads"

    id = Column(String(64), primary_key=True)
    filename = Column(String(300), nullable=False)
    content_type = Column(String(100), nullable=False)
    size = Column(Integer, nullable=False)
    storage_key = Column(String(300), nullable=False)
    uploaded_by = Column(String(100), nullable=True)
    created_at = Column(DateTime(timezone=True), nullable=False, default=utcnow)

# ── Indexes ────────────────────────────────────────────────────

Index("ix_wiki_pages_search", WikiPage.search_vector, postgresql_using="gin")
Index("ix_wiki_pages_category", WikiPage.category)
Index("ix_wiki_pages_updated", WikiPage.updated_at.desc())

# ── Engine + Session ───────────────────────────────────────────

_engine = None
_SessionLocal = None

def get_engine(database_url: str | None = None):
    global _engine, _SessionLocal
    if _engine is None:
        url = database_url or "postgresql+asyncpg://nexus:nexus@localhost:5432/nexus_wiki"
        _engine = create_async_engine(url, echo=False, pool_size=10, max_overflow=20)
        _SessionLocal = async_sessionmaker(_engine, expire_on_commit=False)
    return _engine

async def get_session() -> AsyncSession:
    if _SessionLocal is None:
        get_engine()
    async with _SessionLocal() as session:
        yield session

async def init_db(database_url: str | None = None):
    engine = get_engine(database_url)
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
        # Create full-text search trigger
        await conn.execute(text("""
            CREATE OR REPLACE FUNCTION wiki_search_update() RETURNS trigger AS $$
            BEGIN
                NEW.search_vector := to_tsvector('english', COALESCE(NEW.title, '') || ' ' || COALESCE(NEW.content, ''));
                RETURN NEW;
            END;
            $$ LANGUAGE plpgsql;
        """))
        await conn.execute(text("""
            DROP TRIGGER IF EXISTS wiki_search_trigger ON wiki_pages;
            CREATE TRIGGER wiki_search_trigger
            BEFORE INSERT OR UPDATE ON wiki_pages
            FOR EACH ROW EXECUTE FUNCTION wiki_search_update();
        """))
