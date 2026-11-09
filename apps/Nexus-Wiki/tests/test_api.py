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


def test_categories_and_backlinks() -> None:
    # Create page A linking to B
    client.post(
        "/api/v1/pages",
        json={"title": "Page A", "content": "Welcome to [[Page B]] and more info.", "category": "General"},
    )
    # Create page B
    client.post(
        "/api/v1/pages",
        json={"title": "Page B", "content": "This is page B content.", "category": "General"},
    )

    # Check categories
    cat_res = client.get("/api/v1/categories")
    assert cat_res.status_code == 200
    cats = cat_res.json()["categories"]
    assert any(c["name"] == "General" and c["count"] == 2 for c in cats)

    # Check backlinks for page B
    links_res = client.get("/api/v1/pages/page-b/links")
    assert links_res.status_code == 200
    links = links_res.json()["backlinks"]
    assert any(l["slug"] == "page-a" for l in links)


def test_diff_versions() -> None:
    client.post(
        "/api/v1/pages",
        json={"title": "Diff Target", "content": "Line 1\nLine 2"},
    )
    client.patch(
        "/api/v1/pages/diff-target",
        json={"content": "Line 1\nLine 2 modified\nLine 3"},
    )

    diff_res = client.get("/api/v1/diff/diff-target?v1=2&v2=1")
    assert diff_res.status_code == 200
    diff = diff_res.json()
    assert diff["v1"] == 2
    assert diff["v2"] == 1
    assert len(diff["diff"]) > 0


def test_talk_pages() -> None:
    # Initial talk page placeholder
    res = client.get("/api/v1/pages/diff-target/talk")
    assert res.status_code == 200
    assert "No discussions yet" in res.json()["content"]

    # Post talk
    create_res = client.post(
        "/api/v1/pages/diff-target/talk",
        json={"title": "diff-target", "content": "Can we improve line 2?"},
    )
    assert create_res.status_code == 200

    # Get talk page again
    res2 = client.get("/api/v1/pages/diff-target/talk")
    assert res2.status_code == 200
    assert "Can we improve line 2?" in res2.json()["content"]


def test_auth_register_and_login() -> None:
    reg = client.post("/api/v1/auth/register", json={"username": "alice", "password": "password123"})
    assert reg.status_code == 200
    assert reg.json()["username"] == "alice"

    login_ok = client.post("/api/v1/auth/login", json={"username": "alice", "password": "password123"})
    assert login_ok.status_code == 200

    login_bad = client.post("/api/v1/auth/login", json={"username": "alice", "password": "wrongpassword"})
    assert login_bad.status_code == 401


def test_audit_wiki() -> None:
    # Page A links to non-existent Page B
    client.post(
        "/api/v1/pages",
        json={"title": "Audit Source", "content": "Link to [[Ghost Article]]"},
    )
    # Page C has no links and no backlinks (orphan)
    client.post(
        "/api/v1/pages",
        json={"title": "Orphan Article", "content": "Standing alone."},
    )

    res = client.get("/api/v1/audit")
    assert res.status_code == 200
    data = res.json()
    assert any(b["target_slug"] == "ghost-article" for b in data["broken_links"])
    assert any(o["slug"] == "orphan-article" for o in data["orphans"])


def test_rollback_revision() -> None:
    client.post("/api/v1/pages", json={"title": "Rollback Test", "content": "Original content v1"})
    client.patch("/api/v1/pages/rollback-test", json={"content": "Modified content v2"})
    
    res = client.post("/api/v1/pages/rollback-test/rollback/1?editor_id=bob")
    assert res.status_code == 200
    page = res.json()
    assert page["version"] == 3
    assert page["content"] == "Original content v1"

    revs = client.get("/api/v1/pages/rollback-test/revisions").json()
    assert revs["items"][0]["change_summary"] == "Reverted to revision v1"
    assert revs["items"][0]["editor_id"] == "bob"

    bad = client.post("/api/v1/pages/rollback-test/rollback/999")
    assert bad.status_code == 404


def test_category_index_endpoints() -> None:
    client.post("/api/v1/pages", json={"title": "Dev Docs 1", "content": "Intro", "category": "Engineering"})
    client.post("/api/v1/pages", json={"title": "Dev Docs 2", "content": "API", "category": "Engineering"})
    client.post("/api/v1/pages", json={"title": "Design 1", "content": "Style guide", "category": "Creative"})

    res = client.get("/api/v1/categories/Engineering/pages")
    assert res.status_code == 200
    assert res.json()["total"] >= 2
    assert all(p["category"] == "Engineering" for p in res.json()["items"])

    idx_res = client.get("/api/v1/categories/index")
    assert idx_res.status_code == 200
    cats = {c["name"]: c for c in idx_res.json()["categories"]}
    assert "Engineering" in cats
    assert cats["Engineering"]["count"] >= 2


