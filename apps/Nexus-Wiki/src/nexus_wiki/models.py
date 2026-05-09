from __future__ import annotations

from datetime import UTC, datetime
from pydantic import BaseModel, Field


def now_utc() -> datetime:
    return datetime.now(UTC)


class HealthResponse(BaseModel):
    status: str = "ok"
    service: str = "nexus-wiki"


class WikiPageCreate(BaseModel):
    title: str = Field(min_length=1, max_length=200)
    content: str = Field(min_length=1)


class WikiPageUpdate(BaseModel):
    title: str | None = Field(default=None, min_length=1, max_length=200)
    content: str | None = Field(default=None, min_length=1)
    editor_id: str | None = Field(default=None, min_length=1, max_length=120)
    change_summary: str | None = Field(default=None, min_length=1, max_length=500)


class WikiPage(BaseModel):
    slug: str
    title: str
    content: str
    version: int
    created_at: datetime
    updated_at: datetime


class WikiPageSummary(BaseModel):
    slug: str
    title: str
    updated_at: datetime


class WikiPageListResponse(BaseModel):
    items: list[WikiPageSummary]
    total: int


class WikiPageRevision(BaseModel):
    slug: str
    version: int
    title: str
    content: str
    editor_id: str
    change_summary: str
    created_at: datetime


class WikiPageRevisionListResponse(BaseModel):
    items: list[WikiPageRevision]
    total: int


class NexusCloudRegisterRequest(BaseModel):
    service_name: str = Field(min_length=1, max_length=120)
    service_url: str = Field(min_length=1, max_length=500)
    health_endpoint: str = Field(min_length=1, max_length=200)
    capabilities: list[str] = Field(default_factory=list)


class NexusCloudRegisterResponse(BaseModel):
    status: str
    upstream_status_code: int
    upstream_body: dict | list | str | None = None
