from __future__ import annotations

import os
from contextlib import asynccontextmanager

import httpx
from fastapi import FastAPI, HTTPException, Query, status

from .models import (
    HealthResponse,
    NexusCloudRegisterRequest,
    NexusCloudRegisterResponse,
    WikiPage,
    WikiPageCreate,
    WikiPageListResponse,
    WikiPageRevisionListResponse,
    WikiPageUpdate,
)
from .store import WikiStore

store = WikiStore()


@asynccontextmanager
async def lifespan(_: FastAPI):
    store.ensure_ready()
    yield

app = FastAPI(
    title="Nexus Wiki",
    version="0.1.0",
    description="Wikipedia-like knowledge platform for Nexus Systems",
    lifespan=lifespan,
)


@app.get("/health", response_model=HealthResponse)
def health() -> HealthResponse:
    return HealthResponse()


@app.get("/api/v1/pages", response_model=WikiPageListResponse)
def list_pages() -> WikiPageListResponse:
    items = store.list_pages()
    return WikiPageListResponse(items=items, total=len(items))


@app.post("/api/v1/pages", response_model=WikiPage, status_code=status.HTTP_201_CREATED)
def create_page(payload: WikiPageCreate) -> WikiPage:
    try:
        return store.create_page(payload)
    except KeyError as exc:
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(exc)) from exc
    except ValueError as exc:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=str(exc)) from exc


@app.get("/api/v1/pages/{slug}", response_model=WikiPage)
def get_page(slug: str) -> WikiPage:
    page = store.get_page(slug)
    if page is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Page not found")
    return page


@app.patch("/api/v1/pages/{slug}", response_model=WikiPage)
def update_page(slug: str, payload: WikiPageUpdate) -> WikiPage:
    page = store.update_page(slug, payload)
    if page is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Page not found")
    return page


@app.get("/api/v1/pages/{slug}/revisions", response_model=WikiPageRevisionListResponse)
def list_revisions(slug: str) -> WikiPageRevisionListResponse:
    if store.get_page(slug) is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Page not found")
    items = store.list_revisions(slug)
    return WikiPageRevisionListResponse(items=items, total=len(items))


@app.get("/api/v1/search", response_model=WikiPageListResponse)
def search_pages(q: str = Query(min_length=1, description="Search query")) -> WikiPageListResponse:
    items = store.search(q)
    return WikiPageListResponse(items=items, total=len(items))


@app.get("/api/v1/pages/random", response_model=WikiPage)
def random_page() -> WikiPage:
    import random
    pages = store.list_pages()
    if not pages:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="No pages exist")
    choice = random.choice(pages)
    return store.get_page(choice.slug)


@app.get("/api/v1/recent", response_model=WikiPageListResponse)
def recent_changes() -> WikiPageListResponse:
    items = store.list_pages()[:20]
    return WikiPageListResponse(items=items, total=len(items))


@app.get("/api/v1/categories", response_model=dict)
def list_categories() -> dict:
    cats: dict[str, int] = {}
    for p in store.list_pages():
        page = store.get_page(p.slug)
        if page and page.category:
            cats[page.category] = cats.get(page.category, 0) + 1
    return {"categories": [{"name": k, "count": v} for k, v in sorted(cats.items())]}


@app.get("/api/v1/pages/{slug}/links", response_model=dict)
def what_links_here(slug: str) -> dict:
    """Find all pages that link to this article."""
    links = store.get_backlinks(slug)
    return {"slug": slug, "backlinks": [{"slug": s, "title": t} for s, t in links]}


@app.get("/api/v1/diff/{slug}", response_model=dict)
def diff_versions(slug: str, v1: int = Query(None), v2: int = Query(None)) -> dict:
    """Compare two versions of an article. Returns unified diff."""
    revisions = store.list_revisions(slug)
    if not revisions:
        raise HTTPException(status_code=404, detail="No revisions found")
    va = v1 or max(r.version for r in revisions)
    vb = v2 or va - 1
    r1 = next((r for r in revisions if r.version == va), None)
    r2 = next((r for r in revisions if r.version == vb), None)
    if not r1 or not r2:
        raise HTTPException(status_code=404, detail="Version not found")
    import difflib
    diff = list(difflib.unified_diff(
        r2.content.splitlines(), r1.content.splitlines(),
        fromfile=f"v{r2.version}", tofile=f"v{r1.version}", lineterm=""
    ))
    return {"slug": slug, "v1": va, "v2": vb, "diff": diff}


@app.post("/api/v1/pages/{slug}/talk", response_model=WikiPage)
def create_talk_page(slug: str, payload: WikiPageCreate) -> WikiPage:
    talk_slug = f"{slug}/talk"
    payload.title = f"Talk: {payload.title}"
    try:
        return store.create_page(payload, slug_override=talk_slug)
    except KeyError:
        page = store.get_page(talk_slug)
        return store.update_page(talk_slug, WikiPageUpdate(
            content=payload.content, editor_id=payload.editor_id or "anonymous",
            change_summary="Talk page updated"
        ))


@app.get("/api/v1/pages/{slug}/talk", response_model=WikiPage)
def get_talk_page(slug: str) -> WikiPage:
    talk_slug = f"{slug}/talk"
    page = store.get_page(talk_slug)
    if not page:
        # Return empty talk page placeholder
        return WikiPage(
            slug=talk_slug, title=f"Talk: {slug}", content="*No discussions yet. Start a new topic.*",
            category=None, tags=[], version=0, created_at=now_utc(), updated_at=now_utc()
        )
    return page


# ── User/auth (basic) ──────────────────────────────────────────
from pydantic import BaseModel as PydanticBase

class UserCreate(PydanticBase):
    username: str = Field(min_length=2, max_length=50)
    password: str = Field(min_length=4)

@app.post("/api/v1/auth/register")
def register_user(payload: UserCreate) -> dict:
    import hashlib
    uid = hashlib.sha256(payload.username.encode()).hexdigest()[:12]
    pw_hash = hashlib.sha256(payload.password.encode()).hexdigest()
    store.create_user(uid, payload.username, pw_hash)
    return {"user_id": uid, "username": payload.username}

@app.post("/api/v1/auth/login")
def login_user(payload: UserCreate) -> dict:
    import hashlib
    uid = hashlib.sha256(payload.username.encode()).hexdigest()[:12]
    pw_hash = hashlib.sha256(payload.password.encode()).hexdigest()
    user = store.get_user(uid)
    if not user or user.get("password_hash") != pw_hash:
        raise HTTPException(status_code=401, detail="Invalid credentials")
    return {"user_id": uid, "username": payload.username}


@app.post(
    "/api/v1/integrations/nexus-cloud/register",
    response_model=NexusCloudRegisterResponse,
)
def register_with_nexus_cloud(payload: NexusCloudRegisterRequest) -> NexusCloudRegisterResponse:
    systems_api_url = os.getenv(
        "NEXUS_CLOUD_SYSTEMS_API_URL",
        "http://localhost:8080/api/v1/systems/register",
    )

    try:
        with httpx.Client(timeout=10.0) as client:
            response = client.post(systems_api_url, json=payload.model_dump())
    except httpx.HTTPError as exc:
        raise HTTPException(
            status_code=status.HTTP_502_BAD_GATEWAY,
            detail=f"Failed to contact Nexus-Cloud Systems API: {exc}",
        ) from exc

    response_body: dict | list | str | None
    try:
        response_body = response.json()
    except ValueError:
        response_body = response.text

    if response.status_code >= 400:
        raise HTTPException(
            status_code=status.HTTP_502_BAD_GATEWAY,
            detail={
                "message": "Nexus-Cloud registration rejected",
                "upstream_status_code": response.status_code,
                "upstream_body": response_body,
            },
        )

    return NexusCloudRegisterResponse(
        status="registered",
        upstream_status_code=response.status_code,
        upstream_body=response_body,
    )
