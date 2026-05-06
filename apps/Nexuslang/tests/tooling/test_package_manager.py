"""Tests for dependency management: registry, workspace, local path dependencies.

Covers:
  Registry (registry.py):
    - RegistryConfig loading from project, global, and env vars
    - RegistryClient.search (mocked HTTP)
    - RegistryClient.get_package_info / get_version_info (mocked HTTP)
    - RegistryClient.download — cache hit, cache miss, checksum verification,
                                 yanked version guard, archive extraction
    - RegistryClient.publish — dry-run, missing token, metadata validation,
                                duplicate version 409 handling
    - RegistryClient.clear_cache
    - resolve_version — all operators (^, ~, =, >=, >, <=, <, *, bare)
    - _verify_checksum — pass and fail
    - _create_archive / _extract_archive — round-trip integrity

  Workspace (workspace.py):
    - WorkspaceManifest.load — valid, missing file, missing section
    - WorkspaceManifest.expand_members — glob patterns, literal paths,
                                          missing nlpl.toml guard
    - WorkspaceMember.load — valid, missing file, missing [package]
    - WorkspaceMember.intra_workspace_deps — path deps, non-path deps, no match
    - WorkspaceResolver.resolve — single member, multi-member, cycle detection,
                                   duplicate name detection
    - WorkspaceResolver.build order — topological correctness
    - WorkspaceResolver.regenerate_shared_lock — merge + conflict detection
    - init_workspace — creates manifest + .gitignore + packages/
    - load_workspace — walks up to find manifest
    - WorkspaceBuilder.status

  Local path dependencies (lockfile.py + dependency_manager.py):
    - resolve_path_dependency — version from dep manifest, checksum computed,
                                  missing path raises FileNotFoundError
    - generate_lockfile: resolve_registry=False records verbatim,
                         path deps fully resolved, git deps resolved
    - add_dependency with --path writes correct entry to nlpl.toml
    - update_lockfile with offline=True skips registry
"""

import hashlib
import json
import os
import sys
import tarfile
import tempfile
from io import BytesIO
from pathlib import Path
from typing import Any, Dict
from unittest.mock import MagicMock, patch, PropertyMock
import urllib.error

import pytest

sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from nexuslang.tooling.registry import (
    AuthError,
    PackageInfo,
    PackageNotFoundError,
    RegistryClient,
    RegistryConfig,
    RegistryError,
    SearchResult,
    VersionInfo,
    _verify_checksum,
    _create_archive,
    _extract_archive,
    resolve_version,
)
from nexuslang.tooling.workspace import (
    WorkspaceBuilder,
    WorkspaceError,
    WorkspaceManifest,
    WorkspaceMember,
    WorkspaceResolver,
    init_workspace,
    load_workspace,
    WORKSPACE_MANIFEST_NAME,
)
from nexuslang.tooling.lockfile import (
    LockFile,
    LockedPackage,
    compute_directory_checksum,
    generate_lockfile,
    resolve_path_dependency,
)
from nexuslang.tooling.dependency_manager import (
    add_dependency,
    update_lockfile,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _write_toml(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")


def _make_nxl_project(root: Path, name: str = "mylib", version: str = "0.1.0") -> Path:
    """Create a minimal NexusLang project directory."""
    root.mkdir(parents=True, exist_ok=True)
    (root / "src").mkdir(exist_ok=True)
    _write_toml(root / "nexuslang.toml", f"""\
[package]
name = "{name}"
version = "{version}"
authors = []
description = "A test library"

[build]
source_dir = "src"
output_dir = "build"
target = "c"
""")
    (root / "src" / "main.nxl").write_text(f'function main\n    print text "{name}"\nend\n')
    return root


def _sha256_of(data: bytes) -> str:
    return "sha256:" + hashlib.sha256(data).hexdigest()


# ---------------------------------------------------------------------------
# Registry: RegistryConfig
# ---------------------------------------------------------------------------

class TestRegistryConfig:
    def test_defaults(self):
        cfg = RegistryConfig()
        assert cfg.url == "https://registry.nlpl.dev"
        assert cfg.token is None
        assert cfg.cache_dir is not None

    def test_trailing_slash_stripped(self):
        cfg = RegistryConfig(url="https://example.com/")
        assert cfg.url == "https://example.com"

    def test_env_token_override(self, tmp_path, monkeypatch):
        _make_nxl_project(tmp_path)
        monkeypatch.setenv("NLPL_REGISTRY_TOKEN", "mytoken123")
        cfg = RegistryConfig.from_project(tmp_path)
        assert cfg.token == "mytoken123"

    def test_env_url_override(self, tmp_path, monkeypatch):
        _make_nxl_project(tmp_path)
        monkeypatch.setenv("NLPL_REGISTRY_URL", "https://private.example.com")
        cfg = RegistryConfig.from_project(tmp_path)
        assert cfg.url == "https://private.example.com"

    def test_project_toml_registry_section(self, tmp_path):
        proj = _make_nxl_project(tmp_path)
        _write_toml(tmp_path / "nexuslang.toml", """\
[package]
name = "proj"
version = "0.1.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[registry]
url = "https://corp.example.com"
token = "secret-token"
""")
        cfg = RegistryConfig.from_project(tmp_path)
        assert cfg.url == "https://corp.example.com"
        assert cfg.token == "secret-token"

    def test_cache_dir_default_exists(self, tmp_path):
        cfg = RegistryConfig(cache_dir=tmp_path / "cache")
        cfg.cache_dir.mkdir(parents=True, exist_ok=True)
        assert cfg.cache_dir.exists()


# ---------------------------------------------------------------------------
# Registry: resolve_version
# ---------------------------------------------------------------------------

class TestResolveVersion:
    VERSIONS = ["0.1.0", "0.1.5", "0.2.0", "1.0.0", "1.2.3", "1.3.0", "2.0.0"]

    def test_wildcard(self):
        assert resolve_version(self.VERSIONS, "*") == "2.0.0"

    def test_empty_req(self):
        assert resolve_version(self.VERSIONS, "") == "2.0.0"

    def test_exact_match(self):
        assert resolve_version(self.VERSIONS, "=1.2.3") == "1.2.3"
        assert resolve_version(self.VERSIONS, "1.2.3") == "1.2.3"

    def test_exact_no_match(self):
        assert resolve_version(self.VERSIONS, "=9.9.9") is None

    def test_caret_same_major(self):
        result = resolve_version(self.VERSIONS, "^1.0.0")
        assert result == "1.3.0"

    def test_caret_zero_major(self):
        result = resolve_version(self.VERSIONS, "^0.1.0")
        assert result == "0.1.5"

    def test_caret_zero_zero(self):
        result = resolve_version(self.VERSIONS, "^0.0.0")
        assert result is None  # 0.0.x patch range requires same major.minor

    def test_tilde(self):
        result = resolve_version(self.VERSIONS, "~1.2.0")
        assert result == "1.2.3"

    def test_gte(self):
        result = resolve_version(self.VERSIONS, ">=1.3.0")
        assert result == "2.0.0"

    def test_gt(self):
        result = resolve_version(self.VERSIONS, ">1.3.0")
        assert result == "2.0.0"

    def test_lte(self):
        result = resolve_version(self.VERSIONS, "<=1.2.3")
        assert result == "1.2.3"

    def test_lt(self):
        result = resolve_version(self.VERSIONS, "<1.0.0")
        assert result == "0.2.0"

    def test_no_match(self):
        assert resolve_version(self.VERSIONS, ">=99.0.0") is None

    def test_empty_list(self):
        assert resolve_version([], "^1.0.0") is None


# ---------------------------------------------------------------------------
# Registry: _verify_checksum
# ---------------------------------------------------------------------------

class TestVerifyChecksum:
    def test_valid_checksum(self, tmp_path):
        f = tmp_path / "file.bin"
        data = b"hello world"
        f.write_bytes(data)
        expected = "sha256:" + hashlib.sha256(data).hexdigest()
        _verify_checksum(f, expected)  # Should not raise

    def test_invalid_checksum(self, tmp_path):
        f = tmp_path / "file.bin"
        f.write_bytes(b"hello world")
        with pytest.raises(ValueError, match="Checksum mismatch"):
            _verify_checksum(f, "sha256:" + "a" * 64)

    def test_unsupported_format(self, tmp_path):
        f = tmp_path / "file.bin"
        f.write_bytes(b"data")
        with pytest.raises(ValueError, match="Unsupported checksum format"):
            _verify_checksum(f, "md5:abc123")


# ---------------------------------------------------------------------------
# Registry: archive round-trip
# ---------------------------------------------------------------------------

class TestArchiveRoundTrip:
    def test_create_and_extract(self, tmp_path):
        src = tmp_path / "src_pkg"
        src.mkdir()
        (src / "nexuslang.toml").write_text("[package]\nname = \"test\"\nversion = \"1.0.0\"\n")
        (src / "main.nxl").write_text("function main\n    print text \"hi\"\nend\n")
        subdir = src / "utils"
        subdir.mkdir()
        (subdir / "helper.nxl").write_text("function helper\nend\n")

        archive = tmp_path / "pkg.tar.gz"
        _create_archive(src, archive)
        assert archive.exists()
        assert archive.stat().st_size > 0

        dest = tmp_path / "extracted"
        _extract_archive(archive, dest)
        assert (dest / "nexuslang.toml").exists()
        assert (dest / "main.nxl").exists()
        assert (dest / "utils" / "helper.nxl").exists()

    def test_build_dir_excluded(self, tmp_path):
        src = tmp_path / "src_pkg"
        src.mkdir()
        (src / "nexuslang.toml").write_text("[package]\nname = \"x\"\nversion = \"0.1.0\"\n")
        (src / "build").mkdir()
        (src / "build" / "output.o").write_bytes(b"\x7f\x45\x4c\x46")

        archive = tmp_path / "pkg.tar.gz"
        _create_archive(src, archive)

        dest = tmp_path / "out"
        _extract_archive(archive, dest)
        assert not (dest / "build" / "output.o").exists()

    def test_no_path_traversal_in_extract(self, tmp_path):
        # Craft a malicious archive with path traversal
        archive = tmp_path / "evil.tar.gz"
        evil_file = tmp_path / "evil_content.txt"
        evil_file.write_text("pwned")
        with tarfile.open(archive, "w:gz") as tf:
            info = tarfile.TarInfo(name="../../../tmp/pwned.txt")
            info.size = len(evil_file.read_bytes())
            evil_bytes = evil_file.read_bytes()
            tf.addfile(info, BytesIO(evil_bytes))

        dest = tmp_path / "safe_extract"
        _extract_archive(archive, dest)
        # The traversal target must NOT have been created
        assert not (tmp_path.parent.parent.parent / "tmp" / "pwned.txt").exists()


# ---------------------------------------------------------------------------
# Registry: RegistryClient — mocked HTTP
# ---------------------------------------------------------------------------

def _make_client(tmp_path: Path) -> RegistryClient:
    cache = tmp_path / "cache"
    cache.mkdir()
    cfg = RegistryConfig(url="https://test.registry", cache_dir=cache)
    return RegistryClient(cfg)


class TestRegistryClientSearch:
    def test_search_returns_results(self, tmp_path):
        client = _make_client(tmp_path)
        payload = json.dumps({
            "results": [
                {"name": "mylib", "version": "1.0.0", "description": "A library", "downloads": 42},
                {"name": "otherlib", "version": "2.1.0", "description": "", "downloads": 0},
            ]
        }).encode()

        with patch("nexuslang.tooling.registry._make_request", return_value=payload):
            results = client.search("mylib")

        assert len(results) == 2
        assert results[0].name == "mylib"
        assert results[0].version == "1.0.0"
        assert results[0].downloads == 42

    def test_search_network_error(self, tmp_path):
        client = _make_client(tmp_path)
        with patch(
            "nexuslang.tooling.registry._make_request",
            side_effect=urllib.error.URLError("connection refused"),
        ):
            with pytest.raises(RegistryError, match="Network error"):
                client.search("anything")

    def test_search_404(self, tmp_path):
        client = _make_client(tmp_path)
        with patch(
            "nexuslang.tooling.registry._make_request",
            side_effect=urllib.error.HTTPError(None, 404, "Not Found", {}, None),
        ):
            with pytest.raises(RegistryError, match="Search failed"):
                client.search("missing")

    def test_search_empty_results(self, tmp_path):
        client = _make_client(tmp_path)
        payload = json.dumps({"results": []}).encode()
        with patch("nexuslang.tooling.registry._make_request", return_value=payload):
            results = client.search("nonexistent_pkg")
        assert results == []


class TestRegistryClientGetPackageInfo:
    def test_get_existing_package(self, tmp_path):
        client = _make_client(tmp_path)
        payload = json.dumps({
            "name": "coolpkg",
            "description": "A cool package",
            "latest_version": "1.0.0",
            "total_downloads": 1000,
            "versions": [
                {
                    "name": "coolpkg",
                    "version": "1.0.0",
                    "description": "A cool package",
                    "checksum": "sha256:" + "a" * 64,
                    "download_url": "https://test.registry/api/v1/packages/coolpkg/1.0.0/dl",
                    "yanked": False,
                }
            ],
        }).encode()

        with patch("nexuslang.tooling.registry._make_request", return_value=payload):
            info = client.get_package_info("coolpkg")

        assert info.name == "coolpkg"
        assert info.latest_version == "1.0.0"
        assert "1.0.0" in info.versions
        latest = info.latest()
        assert latest is not None
        assert latest.checksum == "sha256:" + "a" * 64

    def test_get_missing_package(self, tmp_path):
        client = _make_client(tmp_path)
        with patch(
            "nexuslang.tooling.registry._make_request",
            side_effect=urllib.error.HTTPError(None, 404, "Not Found", {}, None),
        ):
            with pytest.raises(PackageNotFoundError, match="not found in registry"):
                client.get_package_info("ghost-pkg")


class TestRegistryClientDownload:
    def _make_archive(self, tmp_path: Path, name: str = "pkg", version: str = "1.0.0") -> bytes:
        src = tmp_path / "src"
        src.mkdir(exist_ok=True)
        (src / "nexuslang.toml").write_text(
            f'[package]\nname = "{name}"\nversion = "{version}"\n'
        )
        (src / "main.nxl").write_text("function main\nend\n")
        archive = tmp_path / "archive.tar.gz"
        _create_archive(src, archive)
        return archive.read_bytes()

    def test_download_fresh(self, tmp_path):
        client = _make_client(tmp_path)
        archive_bytes = self._make_archive(tmp_path)
        checksum = _sha256_of(archive_bytes)

        ver_info_payload = json.dumps({
            "name": "mypkg",
            "version": "1.0.0",
            "checksum": checksum,
            "download_url": "https://test.registry/api/v1/packages/mypkg/1.0.0/dl",
            "yanked": False,
        }).encode()

        call_count = {"n": 0}

        def _fake_request(url, **kwargs):
            call_count["n"] += 1
            if "/dl" in url:
                return archive_bytes
            return ver_info_payload

        with patch("nexuslang.tooling.registry._make_request", side_effect=_fake_request):
            extracted = client.download("mypkg", "1.0.0", quiet=True)

        assert extracted.is_dir()
        assert (extracted / "nexuslang.toml").exists()

    def test_download_uses_cache(self, tmp_path):
        client = _make_client(tmp_path)
        archive_bytes = self._make_archive(tmp_path)
        checksum = _sha256_of(archive_bytes)

        ver_info = {
            "name": "mypkg",
            "version": "1.0.0",
            "checksum": checksum,
            "download_url": "https://test.registry/api/v1/packages/mypkg/1.0.0/dl",
            "yanked": False,
        }

        def _fake_request(url, **kwargs):
            if "/dl" in url:
                return archive_bytes
            return json.dumps(ver_info).encode()

        with patch("nexuslang.tooling.registry._make_request", side_effect=_fake_request):
            client.download("mypkg", "1.0.0", quiet=True)

        # Second download — must use cache (no network)
        with patch(
            "nexuslang.tooling.registry._make_request",
            side_effect=AssertionError("Should not make network call on cache hit"),
        ):
            extracted = client.download("mypkg", "1.0.0", quiet=True)

        assert (extracted / "nexuslang.toml").exists()

    def test_download_yanked_raises(self, tmp_path):
        client = _make_client(tmp_path)
        ver_info_payload = json.dumps({
            "name": "oldpkg",
            "version": "0.1.0",
            "checksum": "",
            "download_url": "",
            "yanked": True,
        }).encode()

        with patch("nexuslang.tooling.registry._make_request", return_value=ver_info_payload):
            with pytest.raises(RegistryError, match="yanked"):
                client.download("oldpkg", "0.1.0", quiet=True)

    def test_download_checksum_mismatch_raises(self, tmp_path):
        client = _make_client(tmp_path)
        archive_bytes = self._make_archive(tmp_path)
        bad_checksum = "sha256:" + "0" * 64  # wrong checksum

        ver_info_payload = json.dumps({
            "name": "badpkg",
            "version": "1.0.0",
            "checksum": bad_checksum,
            "download_url": "https://test.registry/api/v1/packages/badpkg/1.0.0/dl",
            "yanked": False,
        }).encode()

        def _fake_request(url, **kwargs):
            if "/dl" in url:
                return archive_bytes
            return ver_info_payload

        with patch("nexuslang.tooling.registry._make_request", side_effect=_fake_request):
            with pytest.raises(RegistryError, match="Checksum mismatch"):
                client.download("badpkg", "1.0.0", quiet=True)

    def test_download_force_bypass_cache(self, tmp_path):
        client = _make_client(tmp_path)
        archive_bytes = self._make_archive(tmp_path)
        checksum = _sha256_of(archive_bytes)

        ver_info = {
            "name": "mypkg",
            "version": "1.0.0",
            "checksum": checksum,
            "download_url": "https://test.registry/api/v1/packages/mypkg/1.0.0/dl",
            "yanked": False,
        }
        network_calls = {"n": 0}

        def _fake_request(url, **kwargs):
            network_calls["n"] += 1
            if "/dl" in url:
                return archive_bytes
            return json.dumps(ver_info).encode()

        with patch("nexuslang.tooling.registry._make_request", side_effect=_fake_request):
            client.download("mypkg", "1.0.0", quiet=True)
            first_calls = network_calls["n"]
            client.download("mypkg", "1.0.0", force=True, quiet=True)
            # force=True should have made additional network calls
            assert network_calls["n"] > first_calls


class TestRegistryClientPublish:
    def test_dry_run_no_upload(self, tmp_path):
        proj = _make_nxl_project(tmp_path, name="myapp", version="1.0.0")
        cfg = RegistryConfig(url="https://test.registry", cache_dir=tmp_path / "cache")
        cfg.cache_dir.mkdir(parents=True, exist_ok=True)
        client = RegistryClient(cfg)

        with patch(
            "nexuslang.tooling.registry._make_request",
            side_effect=AssertionError("Should not make network call in dry-run"),
        ):
            client.publish(proj, dry_run=True, quiet=True)  # Should not raise

    def test_missing_token_raises_auth_error(self, tmp_path):
        proj = _make_nxl_project(tmp_path, name="myapp", version="1.0.0")
        cfg = RegistryConfig(url="https://test.registry", token=None, cache_dir=tmp_path / "cache")
        cfg.cache_dir.mkdir(parents=True, exist_ok=True)
        client = RegistryClient(cfg)

        with pytest.raises(AuthError, match="No registry API token"):
            client.publish(proj)

    def test_missing_manifest_raises(self, tmp_path):
        cfg = RegistryConfig(url="https://test.registry", token="tok", cache_dir=tmp_path)
        client = RegistryClient(cfg)
        with pytest.raises(FileNotFoundError):
            client.publish(tmp_path)

    def test_duplicate_version_409(self, tmp_path):
        proj = _make_nxl_project(tmp_path, name="dup", version="1.0.0")
        cfg = RegistryConfig(url="https://test.registry", token="tok", cache_dir=tmp_path / "cache")
        cfg.cache_dir.mkdir(parents=True, exist_ok=True)
        client = RegistryClient(cfg)

        with patch(
            "nexuslang.tooling.registry._make_request",
            side_effect=urllib.error.HTTPError(None, 409, "Conflict", {}, None),
        ):
            with pytest.raises(RegistryError, match="already exists"):
                client.publish(proj, quiet=True)

    def test_auth_error_401(self, tmp_path):
        proj = _make_nxl_project(tmp_path, name="myapp", version="0.1.0")
        cfg = RegistryConfig(url="https://test.registry", token="bad-token", cache_dir=tmp_path / "cache")
        cfg.cache_dir.mkdir(parents=True, exist_ok=True)
        client = RegistryClient(cfg)

        with patch(
            "nexuslang.tooling.registry._make_request",
            side_effect=urllib.error.HTTPError(None, 401, "Unauthorized", {}, None),
        ):
            with pytest.raises(AuthError, match="authentication failed"):
                client.publish(proj, quiet=True)

    def test_invalid_package_name_raises(self, tmp_path):
        tmp_path.mkdir(parents=True, exist_ok=True)
        _write_toml(tmp_path / "nexuslang.toml", """\
[package]
name = "INVALID NAME!"
version = "1.0.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"
""")
        (tmp_path / "src").mkdir(exist_ok=True)
        cfg = RegistryConfig(url="https://test.registry", token="tok", cache_dir=tmp_path / "cache")
        cfg.cache_dir.mkdir(parents=True, exist_ok=True)
        client = RegistryClient(cfg)

        with pytest.raises(ValueError, match="Invalid package name"):
            client.publish(tmp_path, quiet=True)


class TestRegistryClientClearCache:
    def test_clear_all(self, tmp_path):
        client = _make_client(tmp_path)
        (client._cache_dir / "pkg1" / "1.0.0").mkdir(parents=True)
        (client._cache_dir / "pkg2" / "2.0.0").mkdir(parents=True)
        removed = client.clear_cache()
        assert removed == 2

    def test_clear_specific_package(self, tmp_path):
        client = _make_client(tmp_path)
        (client._cache_dir / "pkga" / "1.0.0").mkdir(parents=True)
        (client._cache_dir / "pkgb" / "1.0.0").mkdir(parents=True)
        client.clear_cache(name="pkga")
        assert not (client._cache_dir / "pkga").exists()
        assert (client._cache_dir / "pkgb").exists()

    def test_clear_specific_version(self, tmp_path):
        client = _make_client(tmp_path)
        (client._cache_dir / "pkg" / "1.0.0").mkdir(parents=True)
        (client._cache_dir / "pkg" / "2.0.0").mkdir(parents=True)
        client.clear_cache(name="pkg", version="1.0.0")
        assert not (client._cache_dir / "pkg" / "1.0.0").exists()
        assert (client._cache_dir / "pkg" / "2.0.0").exists()


# ---------------------------------------------------------------------------
# Workspace: WorkspaceManifest
# ---------------------------------------------------------------------------

class TestWorkspaceManifest:
    def test_load_valid(self, tmp_path):
        _write_toml(tmp_path / WORKSPACE_MANIFEST_NAME, """\
[workspace]
members = ["packages/*", "tools/mytool"]
""")
        manifest = WorkspaceManifest.load(tmp_path)
        assert manifest.root == tmp_path.resolve()
        assert "packages/*" in manifest.member_globs
        assert "tools/mytool" in manifest.member_globs

    def test_load_missing_file(self, tmp_path):
        with pytest.raises(FileNotFoundError, match=WORKSPACE_MANIFEST_NAME):
            WorkspaceManifest.load(tmp_path)

    def test_load_missing_section(self, tmp_path):
        _write_toml(tmp_path / WORKSPACE_MANIFEST_NAME, "[other]\nkey = 1\n")
        with pytest.raises(ValueError, match=r"\[workspace\]"):
            WorkspaceManifest.load(tmp_path)

    def test_load_invalid_members_type(self, tmp_path):
        _write_toml(tmp_path / WORKSPACE_MANIFEST_NAME, "[workspace]\nmembers = \"not-a-list\"\n")
        with pytest.raises(ValueError, match="list"):
            WorkspaceManifest.load(tmp_path)

    def test_save_roundtrip(self, tmp_path):
        manifest = WorkspaceManifest(
            root=tmp_path,
            member_globs=["packages/*", "extras/tool"],
            description="My workspace",
        )
        manifest.save()
        reloaded = WorkspaceManifest.load(tmp_path)
        assert reloaded.member_globs == ["packages/*", "extras/tool"]

    def test_expand_members_glob(self, tmp_path):
        for pkg in ("libfoo", "libbar"):
            _make_nxl_project(tmp_path / "packages" / pkg, name=pkg)
        _write_toml(tmp_path / WORKSPACE_MANIFEST_NAME, """\
[workspace]
members = ["packages/*"]
""")
        manifest = WorkspaceManifest.load(tmp_path)
        members = manifest.expand_members()
        names = {m.name for m in members}
        assert "libfoo" in names
        assert "libbar" in names

    def test_expand_members_literal(self, tmp_path):
        _make_nxl_project(tmp_path / "tools" / "mytool", name="mytool")
        _write_toml(tmp_path / WORKSPACE_MANIFEST_NAME, """\
[workspace]
members = ["tools/mytool"]
""")
        manifest = WorkspaceManifest.load(tmp_path)
        members = manifest.expand_members()
        assert len(members) == 1
        assert members[0].name == "mytool"

    def test_expand_members_missing_nxl_toml(self, tmp_path):
        (tmp_path / "packages" / "broken").mkdir(parents=True)
        # No nlpl.toml in that dir
        _write_toml(tmp_path / WORKSPACE_MANIFEST_NAME, """\
[workspace]
members = ["packages/*"]
""")
        manifest = WorkspaceManifest.load(tmp_path)
        with pytest.raises(WorkspaceError, match=r"nlpl\.toml"):
            manifest.expand_members()


# ---------------------------------------------------------------------------
# Workspace: WorkspaceMember
# ---------------------------------------------------------------------------

class TestWorkspaceMember:
    def test_load_valid(self, tmp_path):
        proj = _make_nxl_project(tmp_path, name="mylib", version="1.2.3")
        member = WorkspaceMember.load(proj)
        assert member.name == "mylib"
        assert member.version == "1.2.3"

    def test_load_missing_manifest(self, tmp_path):
        with pytest.raises(FileNotFoundError):
            WorkspaceMember.load(tmp_path)

    def test_load_missing_package_section(self, tmp_path):
        _write_toml(tmp_path / "nexuslang.toml", "[build]\nsource_dir = \"src\"\n")
        with pytest.raises(ValueError, match=r"\[package\]"):
            WorkspaceMember.load(tmp_path)

    def test_intra_workspace_deps_path(self, tmp_path):
        lib = _make_nxl_project(tmp_path / "lib", name="lib")
        app_root = tmp_path / "app"
        _make_nxl_project(app_root, name="app")
        _write_toml(app_root / "nexuslang.toml", f"""\
[package]
name = "app"
version = "0.1.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[dependencies]
lib = {{ path = "../lib" }}
""")
        app_member = WorkspaceMember.load(app_root)
        lib_member = WorkspaceMember.load(lib)
        all_members = {"lib": lib_member, "app": app_member}
        deps = app_member.intra_workspace_deps(all_members)
        assert "lib" in deps

    def test_intra_workspace_deps_no_match(self, tmp_path):
        _make_nxl_project(tmp_path / "app", name="app")
        member = WorkspaceMember.load(tmp_path / "app")
        assert member.intra_workspace_deps({}) == []


# ---------------------------------------------------------------------------
# Workspace: WorkspaceResolver
# ---------------------------------------------------------------------------

class TestWorkspaceResolver:
    def _make_workspace(self, tmp_path, members_config):
        """Create a workspace with the given member configs.

        members_config is a list of (name, version, deps) where deps is a
        dict {dep_name: dep_path_str} for path deps.
        """
        for name, version, deps in members_config:
            pkg_root = tmp_path / "packages" / name
            _make_nxl_project(pkg_root, name=name, version=version)
            if deps:
                dep_lines = "\n".join(
                    f'{d} = {{ path = "{p}" }}' for d, p in deps.items()
                )
                extra = f"\n[dependencies]\n{dep_lines}\n"
                toml = (pkg_root / "nexuslang.toml").read_text()
                (pkg_root / "nexuslang.toml").write_text(toml + extra)

        _write_toml(tmp_path / WORKSPACE_MANIFEST_NAME, """\
[workspace]
members = ["packages/*"]
""")
        manifest = WorkspaceManifest.load(tmp_path)
        return WorkspaceResolver(manifest)

    def test_resolve_single_member(self, tmp_path):
        resolver = self._make_workspace(tmp_path, [("mylib", "0.1.0", {})])
        resolver.resolve()
        assert "mylib" in resolver.members
        assert resolver.build_order == ["mylib"]

    def test_resolve_multi_member_no_deps(self, tmp_path):
        resolver = self._make_workspace(
            tmp_path,
            [("alpha", "1.0.0", {}), ("beta", "2.0.0", {}), ("gamma", "3.0.0", {})],
        )
        resolver.resolve()
        assert len(resolver.members) == 3
        # All three should appear in build order
        assert set(resolver.build_order) == {"alpha", "beta", "gamma"}

    def test_resolve_with_intra_dep(self, tmp_path):
        resolver = self._make_workspace(
            tmp_path,
            [
                ("base", "1.0.0", {}),
                ("mid", "1.0.0", {"base": "../../packages/base"}),
                ("top", "1.0.0", {"mid": "../../packages/mid"}),
            ],
        )
        resolver.resolve()
        order = resolver.build_order
        assert order.index("base") < order.index("mid")
        assert order.index("mid") < order.index("top")

    def test_resolve_cycle_detection(self, tmp_path):
        # Create two packages that depend on each other
        a_root = tmp_path / "packages" / "a"
        b_root = tmp_path / "packages" / "b"
        _make_nxl_project(a_root, name="a")
        _make_nxl_project(b_root, name="b")

        # a depends on b (via path)
        _write_toml(a_root / "nexuslang.toml", f"""\
[package]
name = "a"
version = "0.1.0"
[build]
source_dir = "src"
output_dir = "build"
target = "c"
[dependencies]
b = {{ path = "../../packages/b" }}
""")
        # b depends on a (via path) — creates a cycle
        _write_toml(b_root / "nexuslang.toml", f"""\
[package]
name = "b"
version = "0.1.0"
[build]
source_dir = "src"
output_dir = "build"
target = "c"
[dependencies]
a = {{ path = "../../packages/a" }}
""")

        _write_toml(tmp_path / WORKSPACE_MANIFEST_NAME, """\
[workspace]
members = ["packages/*"]
""")
        manifest = WorkspaceManifest.load(tmp_path)
        resolver = WorkspaceResolver(manifest)
        with pytest.raises(WorkspaceError, match="[Cc]ircular"):
            resolver.resolve()

    def test_resolve_duplicate_name(self, tmp_path):
        a1 = tmp_path / "pkg1"
        a2 = tmp_path / "pkg2"
        _make_nxl_project(a1, name="duplicate")
        _make_nxl_project(a2, name="duplicate")

        _write_toml(tmp_path / WORKSPACE_MANIFEST_NAME, """\
[workspace]
members = ["pkg1", "pkg2"]
""")
        manifest = WorkspaceManifest.load(tmp_path)
        resolver = WorkspaceResolver(manifest)
        with pytest.raises(WorkspaceError, match="[Dd]uplicate"):
            resolver.resolve()

    def test_get_unknown_member(self, tmp_path):
        resolver = self._make_workspace(tmp_path, [("only-one", "1.0.0", {})])
        resolver.resolve()
        with pytest.raises(KeyError):
            resolver.get("nonexistent")


# ---------------------------------------------------------------------------
# Workspace: init_workspace
# ---------------------------------------------------------------------------

class TestInitWorkspace:
    def test_creates_manifest(self, tmp_path):
        manifest = init_workspace(tmp_path, quiet=True)
        assert (tmp_path / WORKSPACE_MANIFEST_NAME).exists()
        assert manifest.member_globs == ["packages/*"]

    def test_creates_packages_dir(self, tmp_path):
        init_workspace(tmp_path, quiet=True)
        assert (tmp_path / "packages").is_dir()

    def test_creates_gitignore(self, tmp_path):
        init_workspace(tmp_path, quiet=True)
        assert (tmp_path / ".gitignore").exists()

    def test_custom_members(self, tmp_path):
        init_workspace(tmp_path, members=["libs/*", "tools/*"], quiet=True)
        manifest = WorkspaceManifest.load(tmp_path)
        assert "libs/*" in manifest.member_globs
        assert "tools/*" in manifest.member_globs

    def test_already_exists_raises(self, tmp_path):
        init_workspace(tmp_path, quiet=True)
        with pytest.raises(FileExistsError):
            init_workspace(tmp_path, quiet=True)

    def test_with_description(self, tmp_path):
        init_workspace(tmp_path, description="My monorepo", quiet=True)
        manifest = WorkspaceManifest.load(tmp_path)
        assert manifest.description == "My monorepo"


# ---------------------------------------------------------------------------
# Workspace: load_workspace / _find_workspace_root
# ---------------------------------------------------------------------------

class TestLoadWorkspace:
    def test_find_in_parent(self, tmp_path):
        init_workspace(tmp_path, quiet=True)
        deep_dir = tmp_path / "packages" / "mypkg" / "src"
        deep_dir.mkdir(parents=True)
        # Temporarily chdir is not needed — we pass the path directly
        _make_nxl_project(tmp_path / "packages" / "mypkg", name="mypkg")

        manifest, resolver = load_workspace(deep_dir)
        assert manifest.root == tmp_path.resolve()

    def test_no_workspace_raises(self, tmp_path):
        with pytest.raises(FileNotFoundError, match=WORKSPACE_MANIFEST_NAME):
            load_workspace(tmp_path)

    def test_member_count(self, tmp_path):
        init_workspace(tmp_path, quiet=True)
        for name in ("pkg1", "pkg2", "pkg3"):
            _make_nxl_project(tmp_path / "packages" / name, name=name)

        _, resolver = load_workspace(tmp_path)
        assert len(resolver.members) == 3


# ---------------------------------------------------------------------------
# Local path dependencies: resolve_path_dependency
# ---------------------------------------------------------------------------

class TestResolvePathDependency:
    def test_basic_resolution(self, tmp_path):
        lib = _make_nxl_project(tmp_path / "mylib", name="mylib", version="1.5.0")
        app = _make_nxl_project(tmp_path / "myapp", name="myapp")

        spec = {"path": "../mylib"}
        pkg = resolve_path_dependency("mylib", spec, app)

        assert pkg.name == "mylib"
        assert pkg.version == "1.5.0"
        assert pkg.source == "path"
        assert pkg.resolved_path == str(lib.resolve())
        assert pkg.checksum is not None
        assert pkg.checksum.startswith("sha256:")

    def test_missing_path_raises(self, tmp_path):
        app = _make_nxl_project(tmp_path / "myapp", name="myapp")
        spec = {"path": "../nonexistent-lib"}
        with pytest.raises(FileNotFoundError, match="not found"):
            resolve_path_dependency("nonexistentlib", spec, app)

    def test_missing_spec_path_raises(self, tmp_path):
        app = _make_nxl_project(tmp_path / "myapp", name="myapp")
        with pytest.raises(ValueError, match="must specify 'path'"):
            resolve_path_dependency("somepkg", {}, app)

    def test_checksum_changes_on_file_change(self, tmp_path):
        lib = _make_nxl_project(tmp_path / "lib", name="lib")
        app = _make_nxl_project(tmp_path / "app", name="app")
        spec = {"path": "../lib"}

        pkg1 = resolve_path_dependency("lib", spec, app)

        # Modify a source file
        (lib / "src" / "extra.nxl").write_text("function extra\nend\n")

        pkg2 = resolve_path_dependency("lib", spec, app)
        assert pkg1.checksum != pkg2.checksum

    def test_version_from_manifest_fallback(self, tmp_path):
        lib = tmp_path / "lib"
        lib.mkdir()
        (lib / "src").mkdir()
        # No nlpl.toml in lib — version should default to "0.0.0"
        spec = {"path": "../lib", "version": "0.0.0"}
        app = _make_nxl_project(tmp_path / "app", name="app")
        # lib dir exists but has no nlpl.toml
        pkg = resolve_path_dependency("lib", spec, app)
        assert pkg.version == "0.0.0"


# ---------------------------------------------------------------------------
# generate_lockfile: path deps, registry, offline mode
# ---------------------------------------------------------------------------

class TestGenerateLockfile:
    def test_path_dep_resolved(self, tmp_path):
        lib = _make_nxl_project(tmp_path / "lib", name="lib", version="0.5.0")
        app = tmp_path / "app"
        _make_nxl_project(app, name="app")
        _write_toml(app / "nexuslang.toml", """\
[package]
name = "app"
version = "0.1.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[dependencies]
lib = { path = "../lib" }
""")
        lock = generate_lockfile(app, resolve_registry=False)
        assert "lib" in lock.packages
        pkg = lock.packages["lib"]
        assert pkg.source == "path"
        assert pkg.version == "0.5.0"
        assert pkg.resolved_path is not None

    def test_registry_dep_offline(self, tmp_path):
        app = _make_nxl_project(tmp_path / "app", name="app")
        _write_toml(app / "nexuslang.toml", """\
[package]
name = "app"
version = "0.1.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[dependencies]
somelib = "^1.0"
""")
        lock = generate_lockfile(app, resolve_registry=False)
        assert "somelib" in lock.packages
        pkg = lock.packages["somelib"]
        assert pkg.source == "registry"
        assert pkg.version == "^1.0"  # Recorded verbatim

    def test_registry_dep_with_resolver(self, tmp_path):
        """Registry resolution falls back gracefully when registry is unavailable."""
        app = _make_nxl_project(tmp_path / "app", name="app")
        _write_toml(app / "nexuslang.toml", """\
[package]
name = "app"
version = "0.1.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[dependencies]
netlib = "^2.0"
""")
        # The registry is unreachable -> fallback recording
        with patch(
            "nexuslang.tooling.lockfile.resolve_registry_dependency",
            side_effect=Exception("Network unavailable"),
        ):
            import io, contextlib
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                lock = generate_lockfile(app, resolve_registry=True)
            # Should have fallen back gracefully
            assert "netlib" in lock.packages

    def test_missing_path_dep_raises(self, tmp_path):
        app = _make_nxl_project(tmp_path / "app", name="app")
        _write_toml(app / "nexuslang.toml", """\
[package]
name = "app"
version = "0.1.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[dependencies]
missing = { path = "../missing-lib" }
""")
        with pytest.raises(ValueError, match="Failed to resolve"):
            generate_lockfile(app, resolve_registry=False)

    def test_dev_deps_also_resolved(self, tmp_path):
        lib = _make_nxl_project(tmp_path / "testlib", name="testlib")
        app = _make_nxl_project(tmp_path / "app", name="app")
        _write_toml(app / "nexuslang.toml", """\
[package]
name = "app"
version = "0.1.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[dev-dependencies]
testlib = { path = "../testlib" }
""")
        lock = generate_lockfile(app, resolve_registry=False)
        assert "testlib" in lock.packages


# ---------------------------------------------------------------------------
# add_dependency with --path
# ---------------------------------------------------------------------------

class TestAddDependencyPath:
    def test_add_path_dep(self, tmp_path):
        lib = _make_nxl_project(tmp_path / "lib", name="lib")
        app = _make_nxl_project(tmp_path / "app", name="app")

        add_dependency(app, "lib", path="../lib")

        import tomllib
        with open(app / "nexuslang.toml", "rb") as f:
            data = tomllib.load(f)

        assert "lib" in data["dependencies"]
        assert data["dependencies"]["lib"]["path"] == "../lib"

    def test_add_path_dep_with_version(self, tmp_path):
        _make_nxl_project(tmp_path / "lib", name="lib")
        app = _make_nxl_project(tmp_path / "app", name="app")

        add_dependency(app, "lib@^1.0", path="../lib")

        import tomllib
        with open(app / "nexuslang.toml", "rb") as f:
            data = tomllib.load(f)

        dep = data["dependencies"]["lib"]
        assert dep["path"] == "../lib"
        assert dep["version"] == "^1.0"

    def test_add_path_and_git_mutually_exclusive(self, tmp_path):
        app = _make_nxl_project(tmp_path / "app", name="app")
        with pytest.raises(ValueError, match="either --path or --git"):
            add_dependency(app, "lib", path="../lib", git="https://github.com/x/y")

    def test_add_dep_updates_lockfile(self, tmp_path):
        lib = _make_nxl_project(tmp_path / "lib", name="lib")
        app = _make_nxl_project(tmp_path / "app", name="app")

        add_dependency(app, "lib", path="../lib")

        lock_path = app / "nexuslang.lock"
        assert lock_path.exists()
        lf = LockFile.load(lock_path)
        assert "lib" in lf.packages


# ---------------------------------------------------------------------------
# update_lockfile offline flag
# ---------------------------------------------------------------------------

class TestUpdateLockfileOffline:
    def test_offline_flag_skips_registry(self, tmp_path):
        app = _make_nxl_project(tmp_path / "app", name="app")
        _write_toml(app / "nexuslang.toml", """\
[package]
name = "app"
version = "0.1.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[dependencies]
remotelib = "^1.0"
""")
        with patch(
            "nexuslang.tooling.lockfile.resolve_registry_dependency",
            side_effect=AssertionError("Should not be called in offline mode"),
        ):
            update_lockfile(app, offline=True)

        assert (app / "nexuslang.lock").exists()

    def test_online_calls_registry(self, tmp_path):
        app = _make_nxl_project(tmp_path / "app", name="app")
        _write_toml(app / "nexuslang.toml", """\
[package]
name = "app"
version = "0.1.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[dependencies]
netpkg = "^2.0"
""")
        called = {"n": 0}

        def _fake_resolve(name, version_req, project_root):
            called["n"] += 1
            return LockedPackage(name=name, version="2.1.0", source="registry")

        with patch("nexuslang.tooling.lockfile.resolve_registry_dependency", side_effect=_fake_resolve):
            update_lockfile(app, offline=False)

        assert called["n"] == 1
        lock = LockFile.load(app / "nexuslang.lock")
        assert lock.packages["netpkg"].version == "2.1.0"
