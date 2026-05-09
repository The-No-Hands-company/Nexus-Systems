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
