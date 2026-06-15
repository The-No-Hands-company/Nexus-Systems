from __future__ import annotations

import os
import re
from dataclasses import dataclass, field
from typing import Protocol

from sqlalchemy import create_engine, text

from .models import (
    WikiPage,
    WikiPageCreate,
    WikiPageRevision,
    WikiPageSummary,
    WikiPageUpdate,
    now_utc,
)


class WikiStoreAdapter(Protocol):
    def list_pages(self) -> list[WikiPageSummary]: ...

    def create_page(self, payload: WikiPageCreate) -> WikiPage: ...

    def get_page(self, slug: str) -> WikiPage | None: ...

    def update_page(self, slug: str, payload: WikiPageUpdate) -> WikiPage | None: ...

    def search(self, query: str) -> list[WikiPageSummary]: ...

    def list_revisions(self, slug: str) -> list[WikiPageRevision]: ...

    def ensure_ready(self) -> None: ...

    def reset_for_tests(self) -> None: ...


def slugify(value: str) -> str:
    compact = re.sub(r"\s+", "-", value.strip().lower())
    cleaned = re.sub(r"[^a-z0-9\-]", "", compact)
    normalized = re.sub(r"\-+", "-", cleaned).strip("-")
    if not normalized:
        raise ValueError("Could not derive slug from title")
    return normalized


def _to_page(row: dict) -> WikiPage:
    return WikiPage(
        slug=row["slug"],
        title=row["title"],
        content=row["content"],
        version=row["version"],
        created_at=row["created_at"],
        updated_at=row["updated_at"],
    )


def _to_revision(row: dict) -> WikiPageRevision:
    return WikiPageRevision(
        slug=row["slug"],
        version=row["version"],
        title=row["title"],
        content=row["content"],
        editor_id=row["editor_id"],
        change_summary=row["change_summary"],
        created_at=row["created_at"],
    )


@dataclass
class InMemoryWikiStore:
    _pages: dict[str, WikiPage] = field(default_factory=dict)
    _revisions: dict[str, list[WikiPageRevision]] = field(default_factory=dict)

    def list_pages(self) -> list[WikiPageSummary]:
        ordered = sorted(self._pages.values(), key=lambda p: p.updated_at, reverse=True)
        return [
            WikiPageSummary(slug=page.slug, title=page.title, updated_at=page.updated_at)
            for page in ordered
        ]

    def create_page(self, payload: WikiPageCreate) -> WikiPage:
        slug = slugify(payload.title)
        if slug in self._pages:
            raise KeyError("Page already exists")

        timestamp = now_utc()
        page = WikiPage(
            slug=slug,
            title=payload.title,
            content=payload.content,
            version=1,
            created_at=timestamp,
            updated_at=timestamp,
        )
        self._pages[slug] = page
        self._revisions[slug] = [
            WikiPageRevision(
                slug=slug,
                version=1,
                title=page.title,
                content=page.content,
                editor_id="system",
                change_summary="Initial creation",
                created_at=timestamp,
            )
        ]
        return page

    def get_page(self, slug: str) -> WikiPage | None:
        return self._pages.get(slug)

    def update_page(self, slug: str, payload: WikiPageUpdate) -> WikiPage | None:
        page = self._pages.get(slug)
        if page is None:
            return None

        updated = page.model_copy(deep=True)
        if payload.title is not None:
            updated.title = payload.title
        if payload.content is not None:
            updated.content = payload.content
        updated.version = page.version + 1
        updated.updated_at = now_utc()
        self._pages[slug] = updated
        self._revisions.setdefault(slug, []).append(
            WikiPageRevision(
                slug=slug,
                version=updated.version,
                title=updated.title,
                content=updated.content,
                editor_id=payload.editor_id or "system",
                change_summary=payload.change_summary or "Article updated",
                created_at=updated.updated_at,
            )
        )
        return updated

    def search(self, query: str) -> list[WikiPageSummary]:
        needle = query.strip().lower()
        if not needle:
            return []

        results: list[WikiPageSummary] = []
        for page in self._pages.values():
            haystack = f"{page.title}\n{page.content}".lower()
            if needle in haystack:
                results.append(
                    WikiPageSummary(
                        slug=page.slug,
                        title=page.title,
                        updated_at=page.updated_at,
                    )
                )

        return sorted(results, key=lambda p: p.updated_at, reverse=True)

    def list_revisions(self, slug: str) -> list[WikiPageRevision]:
        return sorted(self._revisions.get(slug, []), key=lambda r: r.version, reverse=True)

    def ensure_ready(self) -> None:
        return None

    def reset_for_tests(self) -> None:
        self._pages.clear()
        self._revisions.clear()


class PostgresWikiStore:
    def __init__(self, database_url: str) -> None:
        self._engine = create_engine(database_url, future=True, pool_pre_ping=True)
        self._init_schema()

    def _init_schema(self) -> None:
        with self._engine.begin() as conn:
            conn.execute(
                text(
                    """
                    CREATE TABLE IF NOT EXISTS wiki_pages (
                      slug TEXT PRIMARY KEY,
                      title TEXT NOT NULL,
                      content TEXT NOT NULL,
                      version INTEGER NOT NULL,
                      created_at TIMESTAMPTZ NOT NULL,
                      updated_at TIMESTAMPTZ NOT NULL
                    )
                    """
                )
            )

    def ensure_ready(self) -> None:
        try:
            with self._engine.begin() as conn:
                conn.execute(text("SELECT 1"))
                self._init_schema()
        except Exception as exc:  # pragma: no cover - startup guard path
            raise RuntimeError(f"PostgreSQL startup migration check failed: {exc}") from exc
            conn.execute(
                text(
                    """
                    CREATE TABLE IF NOT EXISTS wiki_page_revisions (
                      slug TEXT NOT NULL,
                      version INTEGER NOT NULL,
                      title TEXT NOT NULL,
                      content TEXT NOT NULL,
                      editor_id TEXT NOT NULL,
                      change_summary TEXT NOT NULL,
                      created_at TIMESTAMPTZ NOT NULL,
                      PRIMARY KEY (slug, version),
                      FOREIGN KEY (slug) REFERENCES wiki_pages(slug) ON DELETE CASCADE
                    )
                    """
                )
            )

    def list_pages(self) -> list[WikiPageSummary]:
        with self._engine.connect() as conn:
            rows = conn.execute(
                text(
                    """
                    SELECT slug, title, updated_at
                    FROM wiki_pages
                    ORDER BY updated_at DESC
                    """
                )
            ).mappings()
            return [
                WikiPageSummary(
                    slug=row["slug"],
                    title=row["title"],
                    updated_at=row["updated_at"],
                )
                for row in rows
            ]

    def create_page(self, payload: WikiPageCreate) -> WikiPage:
        slug = slugify(payload.title)
        timestamp = now_utc()

        with self._engine.begin() as conn:
            exists = conn.execute(
                text("SELECT slug FROM wiki_pages WHERE slug = :slug"),
                {"slug": slug},
            ).first()
            if exists is not None:
                raise KeyError("Page already exists")

            conn.execute(
                text(
                    """
                    INSERT INTO wiki_pages (slug, title, content, version, created_at, updated_at)
                    VALUES (:slug, :title, :content, :version, :created_at, :updated_at)
                    """
                ),
                {
                    "slug": slug,
                    "title": payload.title,
                    "content": payload.content,
                    "version": 1,
                    "created_at": timestamp,
                    "updated_at": timestamp,
                },
            )
            conn.execute(
                text(
                    """
                    INSERT INTO wiki_page_revisions (
                      slug, version, title, content, editor_id, change_summary, created_at
                    )
                    VALUES (
                      :slug, :version, :title, :content, :editor_id, :change_summary, :created_at
                    )
                    """
                ),
                {
                    "slug": slug,
                    "version": 1,
                    "title": payload.title,
                    "content": payload.content,
                    "editor_id": "system",
                    "change_summary": "Initial creation",
                    "created_at": timestamp,
                },
            )

        return WikiPage(
            slug=slug,
            title=payload.title,
            content=payload.content,
            version=1,
            created_at=timestamp,
            updated_at=timestamp,
        )

    def get_page(self, slug: str) -> WikiPage | None:
        with self._engine.connect() as conn:
            row = conn.execute(
                text(
                    """
                    SELECT slug, title, content, version, created_at, updated_at
                    FROM wiki_pages
                    WHERE slug = :slug
                    """
                ),
                {"slug": slug},
            ).mappings().first()
            if row is None:
                return None
            return _to_page(row)

    def update_page(self, slug: str, payload: WikiPageUpdate) -> WikiPage | None:
        with self._engine.begin() as conn:
            row = conn.execute(
                text(
                    """
                    SELECT slug, title, content, version, created_at, updated_at
                    FROM wiki_pages
                    WHERE slug = :slug
                    """
                ),
                {"slug": slug},
            ).mappings().first()
            if row is None:
                return None

            next_title = payload.title if payload.title is not None else row["title"]
            next_content = payload.content if payload.content is not None else row["content"]
            next_version = int(row["version"]) + 1
            updated_at = now_utc()

            conn.execute(
                text(
                    """
                    UPDATE wiki_pages
                    SET title = :title, content = :content, version = :version, updated_at = :updated_at
                    WHERE slug = :slug
                    """
                ),
                {
                    "slug": slug,
                    "title": next_title,
                    "content": next_content,
                    "version": next_version,
                    "updated_at": updated_at,
                },
            )
            conn.execute(
                text(
                    """
                    INSERT INTO wiki_page_revisions (
                      slug, version, title, content, editor_id, change_summary, created_at
                    )
                    VALUES (
                      :slug, :version, :title, :content, :editor_id, :change_summary, :created_at
                    )
                    """
                ),
                {
                    "slug": slug,
                    "version": next_version,
                    "title": next_title,
                    "content": next_content,
                    "editor_id": payload.editor_id or "system",
                    "change_summary": payload.change_summary or "Article updated",
                    "created_at": updated_at,
                },
            )

            return WikiPage(
                slug=slug,
                title=next_title,
                content=next_content,
                version=next_version,
                created_at=row["created_at"],
                updated_at=updated_at,
            )

    def search(self, query: str) -> list[WikiPageSummary]:
        needle = query.strip()
        if not needle:
            return []

        with self._engine.connect() as conn:
            rows = conn.execute(
                text(
                    """
                    SELECT slug, title, updated_at
                    FROM wiki_pages
                    WHERE title ILIKE :needle OR content ILIKE :needle
                    ORDER BY updated_at DESC
                    """
                ),
                {"needle": f"%{needle}%"},
            ).mappings()
            return [
                WikiPageSummary(
                    slug=row["slug"],
                    title=row["title"],
                    updated_at=row["updated_at"],
                )
                for row in rows
            ]

    def list_revisions(self, slug: str) -> list[WikiPageRevision]:
        with self._engine.connect() as conn:
            rows = conn.execute(
                text(
                    """
                    SELECT slug, version, title, content, editor_id, change_summary, created_at
                    FROM wiki_page_revisions
                    WHERE slug = :slug
                    ORDER BY version DESC
                    """
                ),
                {"slug": slug},
            ).mappings()
            return [_to_revision(row) for row in rows]

    def reset_for_tests(self) -> None:
        with self._engine.begin() as conn:
            conn.execute(text("TRUNCATE TABLE wiki_page_revisions, wiki_pages RESTART IDENTITY"))


class WikiStore:
    def __init__(self) -> None:
        backend = os.getenv("NEXUS_WIKI_STORE_BACKEND", "auto").strip().lower()
        database_url = os.getenv("DATABASE_URL", "").strip()

        if backend == "postgres" or (
            backend == "auto" and database_url.startswith("postgresql")
        ):
            if not database_url:
                raise RuntimeError("DATABASE_URL is required when using PostgreSQL backend")
            self._adapter: WikiStoreAdapter = PostgresWikiStore(database_url)
        else:
            self._adapter = InMemoryWikiStore()

    def list_pages(self) -> list[WikiPageSummary]:
        return self._adapter.list_pages()

    def create_page(self, payload: WikiPageCreate) -> WikiPage:
        return self._adapter.create_page(payload)

    def get_page(self, slug: str) -> WikiPage | None:
        return self._adapter.get_page(slug)

    def update_page(self, slug: str, payload: WikiPageUpdate) -> WikiPage | None:
        return self._adapter.update_page(slug, payload)

    def search(self, query: str) -> list[WikiPageSummary]:
        return self._adapter.search(query)

    def list_revisions(self, slug: str) -> list[WikiPageRevision]:
        return self._adapter.list_revisions(slug)

    def ensure_ready(self) -> None:
        self._adapter.ensure_ready()

    def reset_for_tests(self) -> None:
        self._adapter.reset_for_tests()

# ── Backlink tracking ──────────────────────────────────────────

class BacklinkStore:
    """Tracks [[page links]] between wiki articles for What Links Here."""
    def __init__(self):
        self._links: dict[str, set[str]] = {}

    def index_links(self, slug: str, content: str):
        import re
        linked = set(re.findall(r'\[\[([^\]|]+)(?:\|[^\]]+)?\]\]', content))
        self._links[slug] = {s.strip().lower().replace(" ", "-") for s in linked}

    def get_backlinks(self, slug: str) -> list[tuple[str, str]]:
        results = []
        for source, targets in self._links.items():
            if slug in targets:
                title = source.replace("-", " ").title()
                results.append((source, title))
        return results

    def rebuild(self, store: "WikiStoreAdapter"):
        self._links.clear()
        for page in store.list_pages():
            full = store.get_page(page.slug)
            if full:
                self.index_links(page.slug, full.content)


# ── User store (in-memory) ─────────────────────────────────────

class InMemoryUserStore:
    def __init__(self):
        self._users: dict[str, dict] = {}

    def create(self, uid: str, username: str, pw_hash: str):
        self._users[uid] = {"user_id": uid, "username": username, "password_hash": pw_hash}

    def get(self, uid: str) -> dict | None:
        return self._users.get(uid)


# ── Extended WikiStore ─────────────────────────────────────────

_backlinks = BacklinkStore()
_users = InMemoryUserStore()

# Monkey-patch new methods onto the WikiStore instance
def _add_methods(store_instance):
    def get_backlinks(slug):
        return _backlinks.get_backlinks(slug)
    def create_user(uid, username, pw_hash):
        _users.create(uid, username, pw_hash)
    def get_user(uid):
        return _users.get(uid)
    store_instance.get_backlinks = get_backlinks
    store_instance.create_user = create_user
    store_instance.get_user = get_user
    return store_instance

_add_methods(store)
