"""
NLPL Package Registry Client

Provides integration with a remote NexusLang package registry for:
  - Searching available packages
  - Fetching package metadata (versions, checksums, dependencies)
  - Downloading and caching package archives locally
  - Publishing packages to the registry

Registry wire protocol summary (REST/JSON over HTTPS)
------------------------------------------------------
GET  {url}/api/v1/index                         -> [{name, latest, description}, ...]
GET  {url}/api/v1/packages/{name}               -> PackageInfo (all versions)
GET  {url}/api/v1/packages/{name}/{version}     -> VersionInfo (manifest + checksum)
GET  {url}/api/v1/packages/{name}/{version}/dl  -> binary tar.gz archive
GET  {url}/api/v1/search?q={query}&limit={n}    -> [{name, version, description, downloads}, ...]
POST {url}/api/v1/packages                      -> publish (multipart form; requires auth token)

Local cache layout (~/.nlpl/cache/registry/)
--------------------------------------------
  index-{host}.json                    - cached full package index
  {name}/{version}/archive.tar.gz      - downloaded archive
  {name}/{version}/extracted/          - extracted package tree
  {name}/{version}/meta.json           - version metadata snapshot
"""

from __future__ import annotations

import gzip
import hashlib
import json
import os
import shutil
import tarfile
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Any

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

DEFAULT_REGISTRY_URL: str = "https://registry.nlpl.dev"
CACHE_TTL_SECONDS: int = 300          # 5-minute index cache freshness
DOWNLOAD_TIMEOUT_SECONDS: int = 60    # per-request timeout
MAX_SEARCH_RESULTS: int = 50

_TOOLING_DIR = Path(__file__).parent


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

@dataclass
class RegistryConfig:
    """Registry connection configuration.

    Can be loaded from the project ``[registry]`` section in ``nlpl.toml``
    or from the global ``~/.nlpl/config.toml`` file.
    """

    url: str = DEFAULT_REGISTRY_URL
    token: Optional[str] = None         # API token for authenticated operations
    cache_dir: Optional[Path] = None    # Defaults to ``~/.nlpl/cache/registry``

    def __post_init__(self) -> None:
        # Normalise: strip trailing slash
        self.url = self.url.rstrip("/")
        if self.cache_dir is None:
            self.cache_dir = _default_cache_dir()

    # ------------------------------------------------------------------
    # Loading helpers
    # ------------------------------------------------------------------

    @classmethod
    def from_project(cls, project_root: Path) -> "RegistryConfig":
        """Load registry configuration from ``nlpl.toml``'s ``[registry]`` section.

        Falls back to the global user config, then to built-in defaults if
        neither config file is found.

        Args:
            project_root: Root directory containing ``nlpl.toml``.

        Returns:
            Merged RegistryConfig (project overrides global overrides defaults).
        """
        config = cls._from_global()

        manifest_path = project_root / "nexuslang.toml"
        if manifest_path.exists():
            try:
                project_section = _read_toml_section(manifest_path, "registry")
                if "url" in project_section:
                    config.url = project_section["url"].rstrip("/")
                if "token" in project_section:
                    config.token = project_section["token"]
            except Exception:
                pass  # Silently fall back to existing config

        # Allow environment variable override
        env_token = os.environ.get("NLPL_REGISTRY_TOKEN")
        if env_token:
            config.token = env_token

        env_url = os.environ.get("NLPL_REGISTRY_URL")
        if env_url:
            config.url = env_url.rstrip("/")

        return config

    @classmethod
    def _from_global(cls) -> "RegistryConfig":
        """Load registry config from ``~/.nlpl/config.toml``."""
        config = cls()
        global_config_path = Path.home() / ".nxl" / "config.toml"
        if global_config_path.exists():
            try:
                section = _read_toml_section(global_config_path, "registry")
                if "url" in section:
                    config.url = section["url"].rstrip("/")
                if "token" in section:
                    config.token = section["token"]
            except Exception:
                pass
        return config


# ---------------------------------------------------------------------------
# Data models
# ---------------------------------------------------------------------------

@dataclass
class VersionInfo:
    """Metadata for a single published version of a package."""

    name: str
    version: str
    description: str = ""
    authors: List[str] = field(default_factory=list)
    license: Optional[str] = None
    nxl_version_req: Optional[str] = None  # e.g. ">=0.3"
    dependencies: Dict[str, str] = field(default_factory=dict)  # name -> version req
    checksum: str = ""       # "sha256:<hex>" of the archive
    download_url: str = ""
    downloads: int = 0
    published_at: str = ""
    yanked: bool = False      # Whether this version has been yanked

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "VersionInfo":
        return cls(
            name=data.get("name", ""),
            version=data.get("version", ""),
            description=data.get("description", ""),
            authors=data.get("authors", []),
            license=data.get("license"),
            nxl_version_req=data.get("nxl_version_req"),
            dependencies=data.get("dependencies", {}),
            checksum=data.get("checksum", ""),
            download_url=data.get("download_url", ""),
            downloads=data.get("downloads", 0),
            published_at=data.get("published_at", ""),
            yanked=data.get("yanked", False),
        )

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "version": self.version,
            "description": self.description,
            "authors": self.authors,
            "license": self.license,
            "nxl_version_req": self.nxl_version_req,
            "dependencies": self.dependencies,
            "checksum": self.checksum,
            "download_url": self.download_url,
            "downloads": self.downloads,
            "published_at": self.published_at,
            "yanked": self.yanked,
        }


@dataclass
class PackageInfo:
    """All published versions and summary data for a package."""

    name: str
    description: str = ""
    latest_version: str = ""
    total_downloads: int = 0
    homepage: Optional[str] = None
    repository: Optional[str] = None
    versions: Dict[str, VersionInfo] = field(default_factory=dict)  # version -> VersionInfo

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "PackageInfo":
        versions: Dict[str, VersionInfo] = {}
        for ver_data in data.get("versions", []):
            vi = VersionInfo.from_dict(ver_data)
            versions[vi.version] = vi
        return cls(
            name=data.get("name", ""),
            description=data.get("description", ""),
            latest_version=data.get("latest_version", ""),
            total_downloads=data.get("total_downloads", 0),
            homepage=data.get("homepage"),
            repository=data.get("repository"),
            versions=versions,
        )

    def get_version(self, version: str) -> Optional[VersionInfo]:
        return self.versions.get(version)

    def latest(self) -> Optional[VersionInfo]:
        return self.versions.get(self.latest_version)


@dataclass
class SearchResult:
    """A single result from a registry search query."""

    name: str
    version: str
    description: str
    downloads: int = 0

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "SearchResult":
        return cls(
            name=data.get("name", ""),
            version=data.get("version", ""),
            description=data.get("description", ""),
            downloads=data.get("downloads", 0),
        )


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _default_cache_dir() -> Path:
    """Return the OS-appropriate NexusLang registry cache directory."""
    xdg = os.environ.get("XDG_CACHE_HOME")
    if xdg:
        return Path(xdg) / "nexuslang" / "registry"
    # macOS
    if os.name == "posix" and (Path.home() / "Library" / "Caches").exists():
        return Path.home() / "Library" / "Caches" / "nexuslang" / "registry"
    # Windows
    appdata = os.environ.get("LOCALAPPDATA")
    if appdata:
        return Path(appdata) / "nexuslang" / "registry"
    # Default
    return Path.home() / ".nxl" / "cache" / "registry"


def _read_toml_section(path: Path, section: str) -> Dict[str, Any]:
    """Read a single top-level section from a TOML file.

    Returns an empty dict if the section does not exist.
    """
    try:
        import tomllib          # Python 3.11+
    except ImportError:
        import tomli as tomllib  # type: ignore[no-redef]
    with open(path, "rb") as f:
        data = tomllib.load(f)
    return data.get(section, {})


def _make_request(
    url: str,
    *,
    method: str = "GET",
    headers: Optional[Dict[str, str]] = None,
    body: Optional[bytes] = None,
    timeout: int = DOWNLOAD_TIMEOUT_SECONDS,
) -> bytes:
    """Perform an HTTP request and return the response body as bytes.

    Raises:
        urllib.error.HTTPError:  For 4xx/5xx HTTP responses.
        urllib.error.URLError:   For network-level errors.
        TimeoutError:            If the request exceeds *timeout* seconds.
    """
    req_headers: Dict[str, str] = {
        "User-Agent": "nlpl-toolchain/1.0",
        "Accept": "application/json",
    }
    if headers:
        req_headers.update(headers)

    request = urllib.request.Request(
        url,
        data=body,
        headers=req_headers,
        method=method,
    )
    try:
        with urllib.request.urlopen(request, timeout=timeout) as resp:
            return resp.read()
    except urllib.error.HTTPError:
        raise
    except OSError as exc:
        # urllib.error.URLError is a subclass of OSError; also catch plain OSError
        raise urllib.error.URLError(reason=str(exc)) from exc


def _verify_checksum(path: Path, expected: str) -> None:
    """Verify SHA-256 checksum of a file.

    Args:
        path:     File to check.
        expected: Expected checksum in the form ``"sha256:<hex>"``.

    Raises:
        ValueError: If the checksum does not match.
    """
    if not expected.startswith("sha256:"):
        raise ValueError(f"Unsupported checksum format: '{expected}'. Expected 'sha256:<hex>'.")

    expected_hex = expected[len("sha256:"):]
    sha256 = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            sha256.update(chunk)
    actual_hex = sha256.hexdigest()
    if actual_hex != expected_hex:
        raise ValueError(
            f"Checksum mismatch for {path.name}:\n"
            f"  expected: {expected_hex}\n"
            f"  got:      {actual_hex}"
        )


def _extract_archive(archive_path: Path, dest: Path) -> None:
    """Extract a .tar.gz archive to *dest*."""
    dest.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive_path, "r:gz") as tf:
        # Security: avoid path traversal attacks by filtering member names
        def _safe_members():
            for member in tf.getmembers():
                member_path = Path(member.name)
                # Normalise and reject absolute paths or ".." components
                parts = member_path.parts
                if any(p in ("", "..") for p in parts):
                    continue
                if member_path.is_absolute():
                    continue
                yield member
        tf.extractall(dest, members=_safe_members())  # type: ignore[call-arg]


def _create_archive(source_dir: Path, dest: Path) -> None:
    """Create a .tar.gz archive of *source_dir* at *dest*.

    Only ``*.nlpl`` files and ``nlpl.toml`` are included.  Binary build
    artefacts and hidden files are excluded.
    """
    dest.parent.mkdir(parents=True, exist_ok=True)

    def _should_include(path: Path) -> bool:
        rel = path.relative_to(source_dir)
        # Exclude hidden files/dirs, build output, lock file
        for part in rel.parts:
            if part.startswith("."):
                return False
            if part in ("build", "target", "__pycache__"):
                return False
        if path.name == "nexuslang.lock":
            return False
        return path.suffix in (".nxl", ".toml") or path.name == "nexuslang.toml"

    with tarfile.open(dest, "w:gz") as tf:
        for file in sorted(source_dir.rglob("*")):
            if file.is_file() and _should_include(file):
                arcname = str(file.relative_to(source_dir))
                tf.add(file, arcname=arcname)


# ---------------------------------------------------------------------------
# Registry client
# ---------------------------------------------------------------------------

class RegistryClient:
    """HTTP client for the NexusLang package registry.

    All network I/O is synchronous.  The index is cached on disk for
    ``CACHE_TTL_SECONDS`` to avoid repeated network round-trips.

    Args:
        config: Registry connection configuration.
    """

    def __init__(self, config: RegistryConfig) -> None:
        self.config = config
        self._cache_dir: Path = config.cache_dir  # type: ignore[assignment]
        self._cache_dir.mkdir(parents=True, exist_ok=True)

        # Cache host slug for index file naming (safe file name)
        host = urllib.parse.urlparse(config.url).netloc.replace(":", "_")
        self._index_cache_path = self._cache_dir / f"index-{host}.json"

    # ------------------------------------------------------------------
    # Auth helpers
    # ------------------------------------------------------------------

    def _auth_headers(self) -> Dict[str, str]:
        """Return Authorization header dict if a token is configured."""
        if self.config.token:
            return {"Authorization": f"Bearer {self.config.token}"}
        return {}

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def search(
        self,
        query: str,
        *,
        limit: int = MAX_SEARCH_RESULTS,
    ) -> List[SearchResult]:
        """Search the registry for packages matching *query*.

        Args:
            query: Free-text search term.
            limit: Maximum number of results (default 50).

        Returns:
            List of SearchResult objects ordered by relevance (server-side).

        Raises:
            urllib.error.URLError: On network failure.
            RegistryError:         On registry-level errors.
        """
        encoded_query = urllib.parse.quote(query, safe="")
        url = f"{self.config.url}/api/v1/search?q={encoded_query}&limit={limit}"
        try:
            raw = _make_request(url, headers=self._auth_headers())
            data = json.loads(raw)
            return [SearchResult.from_dict(r) for r in data.get("results", [])]
        except urllib.error.HTTPError as exc:
            raise RegistryError(f"Search failed (HTTP {exc.code}): {exc.reason}") from exc
        except (urllib.error.URLError, OSError) as exc:
            raise RegistryError(f"Network error during search: {exc}") from exc
        except (json.JSONDecodeError, KeyError) as exc:
            raise RegistryError(f"Unexpected registry response format: {exc}") from exc

    def get_package_info(self, name: str) -> PackageInfo:
        """Fetch full package information including all published versions.

        Args:
            name: Package name (lowercase).

        Returns:
            PackageInfo with all version metadata.

        Raises:
            PackageNotFoundError: If the package does not exist.
            RegistryError:        On other errors.
        """
        url = f"{self.config.url}/api/v1/packages/{urllib.parse.quote(name, safe='')}"
        try:
            raw = _make_request(url, headers=self._auth_headers())
            data = json.loads(raw)
            return PackageInfo.from_dict(data)
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                raise PackageNotFoundError(f"Package '{name}' not found in registry") from exc
            raise RegistryError(f"Registry error (HTTP {exc.code}): {exc.reason}") from exc
        except (urllib.error.URLError, OSError) as exc:
            raise RegistryError(f"Network error fetching '{name}': {exc}") from exc
        except (json.JSONDecodeError, KeyError) as exc:
            raise RegistryError(f"Unexpected registry response for '{name}': {exc}") from exc

    def get_version_info(self, name: str, version: str) -> VersionInfo:
        """Fetch metadata for a specific version of a package.

        Args:
            name:    Package name.
            version: Exact version string.

        Returns:
            VersionInfo for the requested version.

        Raises:
            PackageNotFoundError: If the package or version does not exist.
            RegistryError:        On other errors.
        """
        n = urllib.parse.quote(name, safe="")
        v = urllib.parse.quote(version, safe="")
        url = f"{self.config.url}/api/v1/packages/{n}/{v}"
        try:
            raw = _make_request(url, headers=self._auth_headers())
            data = json.loads(raw)
            return VersionInfo.from_dict(data)
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                raise PackageNotFoundError(
                    f"Package '{name}@{version}' not found in registry"
                ) from exc
            raise RegistryError(f"Registry error (HTTP {exc.code}): {exc.reason}") from exc
        except (urllib.error.URLError, OSError) as exc:
            raise RegistryError(f"Network error fetching '{name}@{version}': {exc}") from exc
        except (json.JSONDecodeError, KeyError) as exc:
            raise RegistryError(
                f"Unexpected registry response for '{name}@{version}': {exc}"
            ) from exc

    def download(
        self,
        name: str,
        version: str,
        *,
        force: bool = False,
        quiet: bool = False,
    ) -> Path:
        """Download and cache a package archive locally.

        If the archive is already cached and its checksum is valid the
        download is skipped unless *force* is True.

        Args:
            name:    Package name.
            version: Exact version string.
            force:   Re-download even if a valid cached copy exists.
            quiet:   Suppress progress messages.

        Returns:
            Path to the extracted package directory.

        Raises:
            PackageNotFoundError: If the package or version does not exist.
            RegistryError:        On network or checksum errors.
        """
        pkg_cache = self._cache_dir / name / version
        extracted_dir = pkg_cache / "extracted"
        archive_path = pkg_cache / "archive.tar.gz"
        meta_path = pkg_cache / "meta.json"

        # Fast path: cached and valid
        if not force and extracted_dir.is_dir() and archive_path.is_file() and meta_path.is_file():
            try:
                meta = json.loads(meta_path.read_text(encoding="utf-8"))
                cached_checksum = meta.get("checksum", "")
                if cached_checksum:
                    _verify_checksum(archive_path, cached_checksum)
                    if not quiet:
                        print(f"  Using cached {name}@{version}")
                    return extracted_dir
            except ValueError:
                pass  # Checksum mismatch — fall through to re-download

        if not quiet:
            print(f"  Downloading {name}@{version} ...")

        # Fetch version metadata to get download URL and checksum
        ver_info = self.get_version_info(name, version)
        if ver_info.yanked:
            raise RegistryError(
                f"Package '{name}@{version}' has been yanked from the registry "
                "and cannot be downloaded. Use `nlpl add {name}` to get the "
                "latest non-yanked version."
            )

        download_url = ver_info.download_url
        if not download_url:
            # Fall back to conventional URL pattern
            n = urllib.parse.quote(name, safe="")
            v = urllib.parse.quote(version, safe="")
            download_url = f"{self.config.url}/api/v1/packages/{n}/{v}/dl"

        # Download to temp file first, then verify and move
        pkg_cache.mkdir(parents=True, exist_ok=True)
        tmp_fd, tmp_path_str = tempfile.mkstemp(dir=pkg_cache, suffix=".tar.gz.tmp")
        tmp_path = Path(tmp_path_str)
        try:
            try:
                raw = _make_request(download_url, headers=self._auth_headers())
            except urllib.error.HTTPError as exc:
                if exc.code == 404:
                    raise PackageNotFoundError(
                        f"Package archive '{name}@{version}' not found in registry"
                    ) from exc
                raise RegistryError(
                    f"Download failed (HTTP {exc.code}): {exc.reason}"
                ) from exc
            except (urllib.error.URLError, OSError) as exc:
                raise RegistryError(
                    f"Network error downloading '{name}@{version}': {exc}"
                ) from exc

            os.write(tmp_fd, raw)
            os.close(tmp_fd)
            tmp_fd = -1  # Mark as closed

            # Verify checksum if provided
            if ver_info.checksum:
                try:
                    _verify_checksum(tmp_path, ver_info.checksum)
                except ValueError as exc:
                    raise RegistryError(str(exc)) from exc

            # Atomically replace archive
            tmp_path.replace(archive_path)

        except Exception:
            if tmp_fd != -1:
                try:
                    os.close(tmp_fd)
                except OSError:
                    pass
            if tmp_path.exists():
                tmp_path.unlink(missing_ok=True)
            raise

        # Extract archive
        if extracted_dir.exists():
            shutil.rmtree(extracted_dir)
        _extract_archive(archive_path, extracted_dir)

        # Cache metadata
        meta_path.write_text(
            json.dumps(ver_info.to_dict(), indent=2) + "\n",
            encoding="utf-8",
        )

        if not quiet:
            print(f"  Downloaded  {name}@{version}")

        return extracted_dir

    def _build_upload_body(
        self,
        name: str,
        version: str,
        pkg_meta: dict,
        archive_bytes: bytes,
        checksum_str: str,
    ):
        """Build multipart form body for package upload.

        Returns:
            (body: bytes, content_type: str)
        """
        boundary = "----NLPLPublishBoundary"
        header_meta = json.dumps({
            "name": name,
            "version": version,
            "description": pkg_meta.get("description", ""),
            "authors": pkg_meta.get("authors", []),
            "license": pkg_meta.get("license"),
            "checksum": checksum_str,
        }).encode("utf-8")

        body_parts = [
            (
                f'--{boundary}\r\n'
                f'Content-Disposition: form-data; name="metadata"\r\n'
                f'Content-Type: application/json\r\n\r\n'
            ).encode("utf-8"),
            header_meta,
            (
                f'\r\n--{boundary}\r\n'
                f'Content-Disposition: form-data; name="archive"; '
                f'filename="{name}-{version}.tar.gz"\r\n'
                f'Content-Type: application/gzip\r\n\r\n'
            ).encode("utf-8"),
            archive_bytes,
            f"\r\n--{boundary}--\r\n".encode("utf-8"),
        ]
        body = b"".join(body_parts)
        content_type = f"multipart/form-data; boundary={boundary}"
        return body, content_type

    def _upload_package(
        self,
        name: str,
        version: str,
        body: bytes,
        content_type: str,
    ) -> None:
        """POST archive to the registry.  Raises RegistryError / AuthError on failure."""
        upload_url = f"{self.config.url}/api/v1/packages"
        try:
            _make_request(
                upload_url,
                method="POST",
                headers={
                    **self._auth_headers(),
                    "Content-Type": content_type,
                    "Content-Length": str(len(body)),
                },
                body=body,
            )
        except urllib.error.HTTPError as exc:
            if exc.code == 401:
                raise AuthError(
                    "Registry authentication failed. "
                    "Check your API token."
                ) from exc
            if exc.code == 409:
                raise RegistryError(
                    f"Version '{name}@{version}' already exists in the registry. "
                    "Bump the version in nlpl.toml and try again."
                ) from exc
            raise RegistryError(
                f"Upload failed (HTTP {exc.code}): {exc.reason}"
            ) from exc
        except (urllib.error.URLError, OSError) as exc:
            raise RegistryError(f"Network error during publish: {exc}") from exc

    def publish(
        self,
        project_root: Path,
        *,
        dry_run: bool = False,
        quiet: bool = False,
    ) -> None:
        """Publish a package to the registry.

        Reads ``nlpl.toml`` to obtain package metadata, creates a ``.tar.gz``
        archive of the source tree, computes a SHA-256 checksum, and POSTs
        the archive to the registry.

        The operation requires a valid API token in the registry config.

        Args:
            project_root: Root directory of the package to publish.
            dry_run:      If True, create the archive and validate metadata but
                          do not upload.
            quiet:        Suppress progress output.

        Raises:
            FileNotFoundError: If ``nlpl.toml`` is not found.
            ValueError:        If package metadata is incomplete.
            RegistryError:     On upload failure or auth error.
            AuthError:         If no API token is configured.
        """
        manifest_path = project_root / "nexuslang.toml"
        if not manifest_path.exists():
            raise FileNotFoundError(
                f"nexuslang.toml not found in {project_root}. "
                "Nothing to publish."
            )

        if not self.config.token and not dry_run:
            raise AuthError(
                "No registry API token configured. "
                "Set token in [registry] section of nlpl.toml, "
                "in ~/.nlpl/config.toml, or via the NLPL_REGISTRY_TOKEN "
                "environment variable."
            )

        # Read and validate package metadata
        pkg_meta = _read_package_metadata(manifest_path)
        name = pkg_meta.get("name", "").strip()
        version = pkg_meta.get("version", "").strip()

        if not name:
            raise ValueError(
                "Package name is missing from nexuslang.toml [package] section."
            )
        if not version:
            raise ValueError(
                "Package version is missing from nexuslang.toml [package] section."
            )

        import re
        if not re.match(r'^[a-z0-9_-]+$', name):
            raise ValueError(
                f"Invalid package name '{name}'.  Registry names must be "
                "lowercase and may contain hyphens or underscores only."
            )

        if not quiet:
            print(f"  Publishing  {name}@{version}")

        # Build archive in temp directory
        with tempfile.TemporaryDirectory() as tmp_dir:
            archive_path = Path(tmp_dir) / f"{name}-{version}.tar.gz"
            _create_archive(project_root, archive_path)

            sha256 = hashlib.sha256()
            with open(archive_path, "rb") as f:
                while chunk := f.read(65536):
                    sha256.update(chunk)
            checksum_str = f"sha256:{sha256.hexdigest()}"

            if not quiet:
                print(f"  Archive     {archive_path.name} ({archive_path.stat().st_size} bytes)")
                print(f"  Checksum    {checksum_str}")

            if dry_run:
                if not quiet:
                    print("  Dry run: skipping upload.")
                return

            archive_bytes = archive_path.read_bytes()
            body, content_type = self._build_upload_body(
                name, version, pkg_meta, archive_bytes, checksum_str
            )
            self._upload_package(name, version, body, content_type)

        if not quiet:
            print(f"  Published   {name}@{version} -> {self.config.url}")

    def clear_cache(self, name: Optional[str] = None, version: Optional[str] = None) -> int:
        """Remove locally cached registry downloads.

        Args:
            name:    If given, only remove cached entries for this package.
            version: If given (together with *name*), remove only that version.

        Returns:
            Number of cache directories removed.
        """
        removed = 0
        if name and version:
            target = self._cache_dir / name / version
            if target.exists():
                shutil.rmtree(target)
                removed = 1
        elif name:
            target = self._cache_dir / name
            if target.exists():
                shutil.rmtree(target)
                removed = 1
        else:
            for child in self._cache_dir.iterdir():
                if child.is_dir() and child.name.startswith("index-"):
                    child.unlink()
                elif child.is_dir():
                    shutil.rmtree(child)
                    removed += 1
        return removed

    # ------------------------------------------------------------------
    # Index helpers
    # ------------------------------------------------------------------

    def list_all(self, *, use_cache: bool = True) -> List[SearchResult]:
        """Fetch the full package index from the registry.

        The result is cached on disk for ``CACHE_TTL_SECONDS``.

        Args:
            use_cache: Return the cached index if it is still fresh.

        Returns:
            List of SearchResult objects (one per package, latest version).
        """
        if use_cache and self._index_cache_path.exists():
            age = time.time() - self._index_cache_path.stat().st_mtime
            if age < CACHE_TTL_SECONDS:
                try:
                    raw = self._index_cache_path.read_bytes()
                    data = json.loads(raw)
                    return [SearchResult.from_dict(r) for r in data]
                except Exception:
                    pass  # Cache corrupt — fall through to fetch

        url = f"{self.config.url}/api/v1/index"
        try:
            raw = _make_request(url, headers=self._auth_headers())
            data = json.loads(raw)
            # Write fresh cache
            self._index_cache_path.write_bytes(raw)
            return [SearchResult.from_dict(r) for r in data]
        except urllib.error.HTTPError as exc:
            raise RegistryError(
                f"Failed to fetch package index (HTTP {exc.code}): {exc.reason}"
            ) from exc
        except (urllib.error.URLError, OSError) as exc:
            raise RegistryError(f"Network error fetching index: {exc}") from exc
        except (json.JSONDecodeError, KeyError) as exc:
            raise RegistryError(f"Malformed registry index: {exc}") from exc


# ---------------------------------------------------------------------------
# Version resolution
# ---------------------------------------------------------------------------

def resolve_version(available_versions: List[str], requirement: str) -> Optional[str]:
    """Resolve a version requirement to the best available version.

    Supports the following requirement operators:
      ``*``         - Any (returns latest)
      ``=1.2.3``    - Exact match
      ``^1.2.3``    - Caret: compatible (same major)
      ``~1.2.3``    - Tilde: patch-compatible (same major.minor)
      ``>=1.2.3``   - Greater-than-or-equal
      ``>1.2.3``    - Strictly greater-than
      ``<=1.2.3``   - Less-than-or-equal
      ``<1.2.3``    - Strictly less-than
      ``1.2.3``     - Bare version string treated as ``=1.2.3``

    Args:
        available_versions: Sorted (ascending) list of version strings.
        requirement:        Version requirement string.

    Returns:
        The highest version satisfying the requirement, or ``None`` if none match.
    """
    if not available_versions:
        return None

    # Lazy import to avoid heavy dependency
    parsed = [_parse_semver(v) for v in available_versions]
    valid = [(raw, sem) for raw, sem in zip(available_versions, parsed) if sem is not None]

    req = requirement.strip()

    def _matches(sem: tuple) -> bool:
        major, minor, patch = sem
        if req in ("*", ""):
            return True
        if req.startswith("^"):
            r_major, r_minor, r_patch = _parse_semver_req(req[1:])
            if r_major is None:
                return False
            if r_major == 0:
                if r_minor == 0:
                    return major == 0 and minor == 0 and patch >= r_patch
                return major == 0 and minor == r_minor and patch >= r_patch
            return major == r_major and (minor, patch) >= (r_minor, r_patch)
        if req.startswith("~"):
            r_major, r_minor, r_patch = _parse_semver_req(req[1:])
            if r_major is None:
                return False
            return major == r_major and minor == r_minor and patch >= r_patch
        if req.startswith(">="):
            r = _parse_semver_req(req[2:])
            return r[0] is not None and (major, minor, patch) >= r
        if req.startswith(">"):
            r = _parse_semver_req(req[1:])
            return r[0] is not None and (major, minor, patch) > r
        if req.startswith("<="):
            r = _parse_semver_req(req[2:])
            return r[0] is not None and (major, minor, patch) <= r
        if req.startswith("<"):
            r = _parse_semver_req(req[1:])
            return r[0] is not None and (major, minor, patch) < r
        if req.startswith("="):
            r = _parse_semver_req(req[1:])
            return (major, minor, patch) == r
        # Bare version — treat as exact
        r = _parse_semver_req(req)
        return (major, minor, patch) == r

    candidates = [(raw, sem) for raw, sem in valid if _matches(sem)]
    if not candidates:
        return None
    # Return the highest matching version
    return max(candidates, key=lambda x: x[1])[0]


def _parse_semver(version: str) -> Optional[tuple]:
    """Parse a semver string into a (major, minor, patch) int tuple, or None."""
    import re
    m = re.match(r'^(\d+)\.(\d+)\.(\d+)', version.strip().lstrip("v"))
    if m:
        return int(m.group(1)), int(m.group(2)), int(m.group(3))
    # Try major.minor
    m = re.match(r'^(\d+)\.(\d+)$', version.strip().lstrip("v"))
    if m:
        return int(m.group(1)), int(m.group(2)), 0
    # Try bare major
    m = re.match(r'^(\d+)$', version.strip().lstrip("v"))
    if m:
        return int(m.group(1)), 0, 0
    return None


def _parse_semver_req(req_str: str) -> tuple:
    """Parse requirement version string and return (major, minor, patch) or (None, None, None)."""
    result = _parse_semver(req_str)
    if result is None:
        return (None, None, None)
    return result


# ---------------------------------------------------------------------------
# Metadata helper
# ---------------------------------------------------------------------------

def _read_package_metadata(manifest_path: Path) -> Dict[str, Any]:
    """Read the ``[package]`` section from nexuslang.toml."""
    section = _read_toml_section(manifest_path, "package")
    return section


# ---------------------------------------------------------------------------
# Exceptions
# ---------------------------------------------------------------------------

class RegistryError(Exception):
    """Raised when a registry operation fails."""


class PackageNotFoundError(RegistryError):
    """Raised when a requested package does not exist in the registry."""


class AuthError(RegistryError):
    """Raised when registry authentication fails or a token is missing."""
