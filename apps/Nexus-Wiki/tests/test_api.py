import pytest
from fastapi.testclient import TestClient

from nexus_wiki.main import app, store

client = TestClient(app)


@pytest.fixture(autouse=True)
def clear_store() -> None:
    store.reset_for_tests()


def test_health_ok() -> None:
    response = client.get("/health")
    assert response.status_code == 200
    assert response.json() == {"status": "ok", "service": "nexus-wiki"}


def test_create_and_get_page() -> None:
    payload = {"title": "Nexus Core Values", "content": "Open, modular, and reliable."}
    create_response = client.post("/api/v1/pages", json=payload)

    assert create_response.status_code == 201
    body = create_response.json()
    assert body["slug"] == "nexus-core-values"
    assert body["version"] == 1

    get_response = client.get("/api/v1/pages/nexus-core-values")
    assert get_response.status_code == 200
    assert get_response.json()["title"] == payload["title"]


def test_duplicate_page_slug_conflict() -> None:
    payload = {"title": "Nexus Platform", "content": "First content."}
    first = client.post("/api/v1/pages", json=payload)
    second = client.post("/api/v1/pages", json=payload)

    assert first.status_code == 201
    assert second.status_code == 409


def test_update_page() -> None:
    payload = {"title": "Nexus API", "content": "Initial"}
    client.post("/api/v1/pages", json=payload)

    patch = client.patch("/api/v1/pages/nexus-api", json={"content": "Updated"})
    assert patch.status_code == 200
    assert patch.json()["content"] == "Updated"
    assert patch.json()["version"] == 2


def test_search_pages() -> None:
    client.post(
        "/api/v1/pages",
        json={"title": "Wiki Governance", "content": "Nexus article moderation"},
    )
    response = client.get("/api/v1/search", params={"q": "moderation"})

    assert response.status_code == 200
    data = response.json()
    assert data["total"] >= 1


def test_get_missing_page() -> None:
    response = client.get("/api/v1/pages/missing-page")
    assert response.status_code == 404


def test_page_revision_history() -> None:
    client.post(
        "/api/v1/pages",
        json={"title": "Nexus History", "content": "v1"},
    )
    client.patch(
        "/api/v1/pages/nexus-history",
        json={"content": "v2", "editor_id": "alice", "change_summary": "Update section"},
    )

    response = client.get("/api/v1/pages/nexus-history/revisions")
    assert response.status_code == 200
    data = response.json()
    assert data["total"] == 2
    assert data["items"][0]["version"] == 2
    assert data["items"][0]["editor_id"] == "alice"
    assert data["items"][0]["change_summary"] == "Update section"


def test_register_with_nexus_cloud_success(monkeypatch: pytest.MonkeyPatch) -> None:
    class DummyResponse:
        status_code = 201

        @staticmethod
        def json() -> dict:
            return {"registration_id": "abc123"}

    class DummyClient:
        def __init__(self, timeout: float) -> None:
            self.timeout = timeout

        def __enter__(self) -> "DummyClient":
            return self

        def __exit__(self, exc_type, exc, tb) -> None:
            return None

        def post(self, url: str, json: dict) -> DummyResponse:
            assert "systems/register" in url
            assert json["service_name"] == "nexus-wiki"
            return DummyResponse()

    monkeypatch.setattr("nexus_wiki.main.httpx.Client", DummyClient)

    response = client.post(
        "/api/v1/integrations/nexus-cloud/register",
        json={
            "service_name": "nexus-wiki",
            "service_url": "http://localhost:9000",
            "health_endpoint": "/health",
            "capabilities": ["wiki", "search"],
        },
    )
    assert response.status_code == 200
    body = response.json()
    assert body["status"] == "registered"
    assert body["upstream_status_code"] == 201


def test_register_with_nexus_cloud_upstream_failure(monkeypatch: pytest.MonkeyPatch) -> None:
    class DummyResponse:
        status_code = 400

        @staticmethod
        def json() -> dict:
            return {"error": "invalid payload"}

    class DummyClient:
        def __init__(self, timeout: float) -> None:
            self.timeout = timeout

        def __enter__(self) -> "DummyClient":
            return self

        def __exit__(self, exc_type, exc, tb) -> None:
            return None

        def post(self, url: str, json: dict) -> DummyResponse:
            return DummyResponse()

    monkeypatch.setattr("nexus_wiki.main.httpx.Client", DummyClient)

    response = client.post(
        "/api/v1/integrations/nexus-cloud/register",
        json={
            "service_name": "nexus-wiki",
            "service_url": "http://localhost:9000",
            "health_endpoint": "/health",
            "capabilities": ["wiki", "search"],
        },
    )
    assert response.status_code == 502
