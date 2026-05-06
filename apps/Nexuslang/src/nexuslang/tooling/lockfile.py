"""
NLPL Build System - Lock File

Provides reproducible builds by locking dependency versions and checksums.
nlpl.lock is automatically generated and should be committed to version control.

Lock file format (JSON):
    {
      "version": 1,
      "generated_at": "2026-02-22T12:00:00Z",
      "package": [
        {
          "name": "some-lib",
          "version": "1.2.3",
          "source": "path",
          "checksum": "sha256:...",
          "resolved_path": "/absolute/path/to/some-lib"
        }
      ]
    }
"""

import json
import hashlib
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Any
from datetime import datetime, timezone

LOCKFILE_VERSION = 1


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class LockedPackage:
    """A resolved, locked dependency."""

    name: str
    version: str
    source: str  # "registry", "path", or "git"

    # Registry deps
    checksum: Optional[str] = None  # sha256:<hex>

    # Path deps
    resolved_path: Optional[str] = None  # absolute, resolved path

    # Git deps
    git_url: Optional[str] = None
    git_commit: Optional[str] = None  # resolved SHA-1 commit hash

    # Transitive dependencies (list of "name@version" strings)
    dependencies: List[str] = field(default_factory=list)

    def identity(self) -> str:
        """Stable identifier for this package."""
        return f"{self.name}@{self.version}"

    def to_dict(self) -> Dict[str, Any]:
        d: Dict[str, Any] = {
            "name": self.name,
            "version": self.version,
            "source": self.source,
        }
        if self.checksum:
            d["checksum"] = self.checksum
        if self.resolved_path:
            d["resolved_path"] = self.resolved_path
        if self.git_url:
            d["git_url"] = self.git_url
        if self.git_commit:
            d["git_commit"] = self.git_commit
        if self.dependencies:
            d["dependencies"] = sorted(self.dependencies)
        return d

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "LockedPackage":
        return cls(
            name=data["name"],
            version=data["version"],
            source=data["source"],
            checksum=data.get("checksum"),
            resolved_path=data.get("resolved_path"),
            git_url=data.get("git_url"),
            git_commit=data.get("git_commit"),
            dependencies=data.get("dependencies", []),
        )


# ---------------------------------------------------------------------------
# Lock file container
# ---------------------------------------------------------------------------

class LockFile:
    """Parsed nlpl.lock file."""

    def __init__(self, version: int = LOCKFILE_VERSION) -> None:
        self.version = version
        self.packages: Dict[str, LockedPackage] = {}
        self.generated_at: Optional[str] = None

    # ------------------------------------------------------------------
    # Serialisation
    # ------------------------------------------------------------------

    @classmethod
    def load(cls, path: Path) -> "LockFile":
        """Load existing lock file from disk.

        Raises:
            FileNotFoundError: If the lock file does not exist.
            ValueError: If the lock file version is unsupported.
        """
        if not path.exists():
            raise FileNotFoundError(f"Lock file not found: {path}")

        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)

        file_version = data.get("version", LOCKFILE_VERSION)
        if file_version != LOCKFILE_VERSION:
            raise ValueError(
                f"Unsupported lock file version {file_version}. "
                f"Expected {LOCKFILE_VERSION}. Run 'nlpl lock' to regenerate."
            )

        lf = cls(version=file_version)
        lf.generated_at = data.get("generated_at")
        for pkg_data in data.get("package", []):
            pkg = LockedPackage.from_dict(pkg_data)
            lf.packages[pkg.name] = pkg
        return lf

    def save(self, path: Path) -> None:
        """Write the lock file to disk (atomic write via temp file)."""
        data: Dict[str, Any] = {
            "version": self.version,
            "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "package": [
                pkg.to_dict()
                for pkg in sorted(self.packages.values(), key=lambda p: p.name)
            ],
        }

        # Atomic write: write to a temp file then rename
        tmp = path.with_suffix(".lock.tmp")
        tmp.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        tmp.replace(path)

    # ------------------------------------------------------------------
    # Mutation helpers
    # ------------------------------------------------------------------

    def add_package(self, pkg: LockedPackage) -> None:
        self.packages[pkg.name] = pkg

    def remove_package(self, name: str) -> None:
        self.packages.pop(name, None)

    def get(self, name: str) -> Optional[LockedPackage]:
        return self.packages.get(name)

    # ------------------------------------------------------------------
    # Validation helpers
    # ------------------------------------------------------------------

    def verify_paths(self) -> List[str]:
        """Check that all path dependencies still exist on disk.

        Returns:
            List of error messages (empty if everything is OK).
        """
        errors: List[str] = []
        for pkg in self.packages.values():
            if pkg.source == "path" and pkg.resolved_path:
                dep_path = Path(pkg.resolved_path)
                if not dep_path.exists():
                    errors.append(
                        f"Path dependency '{pkg.name}' not found: {dep_path}"
                    )
        return errors

    def is_empty(self) -> bool:
        return len(self.packages) == 0

    @classmethod
    def empty(cls) -> "LockFile":
        return cls()

    def __repr__(self) -> str:
        return f"LockFile(v{self.version}, {len(self.packages)} packages)"


# ---------------------------------------------------------------------------
# Checksum helpers
# ---------------------------------------------------------------------------

def compute_file_checksum(path: Path) -> str:
    """Compute SHA-256 checksum of a single file."""
    sha256 = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            sha256.update(chunk)
    return f"sha256:{sha256.hexdigest()}"


def compute_directory_checksum(path: Path) -> str:
    """Compute a stable SHA-256 checksum of all *.nlpl files in a directory.

    The checksum is computed over a deterministic serialisation of:
      - relative file path
      - file content
    in lexicographic file order, so it is stable across OSes and filesystem
    ordering.
    """
    sha256 = hashlib.sha256()
    for nxl_file in sorted(path.rglob("*.nxl")):
        rel = str(nxl_file.relative_to(path)).encode("utf-8")
        sha256.update(rel)
        sha256.update(b"\x00")
        sha256.update(nxl_file.read_bytes())
        sha256.update(b"\xff")
    return f"sha256:{sha256.hexdigest()}"


# ---------------------------------------------------------------------------
# Dependency resolution helpers
# ---------------------------------------------------------------------------

def _load_toml(path: Path) -> Dict[str, Any]:
    try:
        import tomllib  # Python 3.11+
    except ImportError:
        import tomli as tomllib  # type: ignore[no-redef]
    with open(path, "rb") as f:
        return tomllib.load(f)


def resolve_path_dependency(
    name: str, spec: Dict[str, Any], project_root: Path
) -> LockedPackage:
    """Resolve a local path dependency.

    Reads the dependency's own nlpl.toml to obtain its version, then
    computes a checksum of its source tree.

    Args:
        name:         Dependency name.
        spec:         Dependency spec dict from the depending package's manifest.
        project_root: Root directory of the *depending* package.

    Returns:
        Fully resolved LockedPackage.

    Raises:
        FileNotFoundError: If the dependency directory does not exist.
    """
    dep_path_str = spec.get("path", "")
    if not dep_path_str:
        raise ValueError(f"Path dependency '{name}' must specify 'path'")

    dep_path = (project_root / dep_path_str).resolve()
    if not dep_path.exists():
        raise FileNotFoundError(
            f"Path dependency '{name}' not found: {dep_path}"
        )

    # Try to read version from dep's own manifest
    version = spec.get("version", "0.0.0")
    dep_manifest = dep_path / "nexuslang.toml"
    if dep_manifest.exists():
        try:
            dep_data = _load_toml(dep_manifest)
            version = dep_data.get("package", {}).get("version", version)
        except Exception:
            pass  # Fall back to declared version

    checksum = compute_directory_checksum(dep_path)
    return LockedPackage(
        name=name,
        version=version,
        source="path",
        checksum=checksum,
        resolved_path=str(dep_path),
    )


def resolve_git_dependency(
    name: str, spec: Dict[str, Any], project_root: Path
) -> LockedPackage:
    """Resolve a git dependency by looking up the remote HEAD commit.

    Attempts a ``git ls-remote`` call to resolve the requested ref to a
    commit hash.  If the network is unavailable or git is not installed the
    commit field is left as ``None`` and a warning is printed.

    Args:
        name:         Dependency name.
        spec:         Dependency spec dict containing ``git``, and optionally
                      ``branch``, ``tag``, or ``rev``.
        project_root: Root directory of the depending package (unused but kept
                      for a consistent interface with resolve_path_dependency).

    Returns:
        LockedPackage with git_url and (if network is reachable) git_commit.

    Raises:
        ValueError: If the spec does not contain a ``git`` URL.
    """
    git_url = spec.get("git", "")
    if not git_url:
        raise ValueError(f"Git dependency '{name}' must specify 'git'")

    rev = spec.get("rev")
    tag = spec.get("tag")
    branch = spec.get("branch", "main")
    version = spec.get("version", "0.0.0")

    # Determine which ref to look up
    if rev:
        git_ref = rev
    elif tag:
        git_ref = f"refs/tags/{tag}"
    else:
        git_ref = branch

    # Try to resolve the ref to a commit hash
    commit: Optional[str] = None
    try:
        result = subprocess.run(
            ["git", "ls-remote", "--quiet", git_url, git_ref],
            capture_output=True,
            text=True,
            timeout=15,
        )
        if result.returncode == 0 and result.stdout.strip():
            commit = result.stdout.split()[0]
    except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
        pass  # Leave commit as None

    if commit is None:
        print(
            f"  Warning: Could not resolve git ref '{git_ref}' for '{name}'. "
            "The lock entry will be incomplete until network is available."
        )

    return LockedPackage(
        name=name,
        version=version,
        source="git",
        git_url=git_url,
        git_commit=commit,
    )


# ---------------------------------------------------------------------------
# Main entry point: generate_lockfile
# ---------------------------------------------------------------------------

def resolve_registry_dependency(
    name: str,
    version_req: str,
    project_root: Path,
) -> LockedPackage:
    """Resolve a registry dependency to a specific locked version.

    Contacts the configured package registry, picks the best matching version
    for *version_req*, downloads the archive into the local cache, and returns
    a fully resolved ``LockedPackage`` with the checksum recorded.

    Args:
        name:         Dependency name.
        version_req:  Version requirement string (e.g. ``"^1.2"``).
        project_root: Root directory of the depending package (used to read
                      ``[registry]`` config from ``nlpl.toml``).

    Returns:
        Locked package with resolved version and sha256 checksum.

    Raises:
        ValueError:            If no matching version is found in the registry.
        ImportError:           If the registry module cannot be imported.
    """
    # Lazy import to keep lockfile.py free of heavy dependencies when the
    # registry is not used.
    from .registry import (
        RegistryConfig,
        RegistryClient,
        resolve_version,
        RegistryError,
        PackageNotFoundError,
    )

    config = RegistryConfig.from_project(project_root)
    client = RegistryClient(config)

    try:
        pkg_info = client.get_package_info(name)
    except PackageNotFoundError:
        raise ValueError(
            f"Package '{name}' not found in registry {config.url}."
        )

    available = sorted(pkg_info.versions.keys())
    resolved_version = resolve_version(available, version_req)
    if resolved_version is None:
        raise ValueError(
            f"No version of '{name}' satisfies requirement '{version_req}'. "
            f"Available: {', '.join(available) or '(none)'}"
        )

    ver_info = pkg_info.get_version(resolved_version)
    checksum = ver_info.checksum if ver_info else ""

    # Download and cache the package archive
    try:
        client.download(name, resolved_version, quiet=True)
    except RegistryError:
        pass  # Cache only — don't fail the lock if download fails

    return LockedPackage(
        name=name,
        version=resolved_version,
        source="registry",
        checksum=checksum or None,
    )


def generate_lockfile(
    project_root: Path,
    *,
    resolve_registry: bool = True,
) -> LockFile:
    """Generate a complete lock file from nexuslang.toml.

    Resolves all path and git dependencies in their respective sections
    (``dependencies``, ``dev-dependencies``, ``build-dependencies``).
    Registry dependencies are resolved against the configured package registry
    when *resolve_registry* is True (the default).  If the registry is
    unreachable the version requirement is recorded verbatim and a warning is
    printed so that offline or CI builds can still regenerate the lock file.

    Args:
        project_root:     Root directory containing nlpl.toml.
        resolve_registry: Contact the package registry to resolve version
                          requirements to exact versions and cache archives.
                          Defaults to True.  Pass False to skip all network
                          calls (useful for offline regeneration).

    Returns:
        Fully populated LockFile ready to be saved.

    Raises:
        FileNotFoundError: If nlpl.toml is not found or a path dep is missing.
        ValueError:        If dependency specs are invalid or resolution fails.
    """
    manifest_path = project_root / "nexuslang.toml"
    if not manifest_path.exists():
        raise FileNotFoundError(f"nexuslang.toml not found in {project_root}")

    manifest_data = _load_toml(manifest_path)
    lock = LockFile.empty()
    errors: List[str] = []

    dep_sections = [
        manifest_data.get("dependencies", {}),
        manifest_data.get("dev-dependencies", {}),
        manifest_data.get("build-dependencies", {}),
    ]

    for deps in dep_sections:
        for name, spec in deps.items():
            if name in lock.packages:
                continue  # Already resolved from a previous section

            try:
                if isinstance(spec, str):
                    # Simple version string — registry dependency
                    if resolve_registry:
                        try:
                            pkg = resolve_registry_dependency(name, spec, project_root)
                        except Exception as reg_exc:
                            print(
                                f"  Warning: Could not resolve registry dep '{name}' "
                                f"({reg_exc}). Recording requirement verbatim."
                            )
                            pkg = LockedPackage(name=name, version=spec, source="registry")
                    else:
                        pkg = LockedPackage(name=name, version=spec, source="registry")
                    lock.add_package(pkg)

                elif isinstance(spec, dict):
                    if "path" in spec:
                        pkg = resolve_path_dependency(name, spec, project_root)
                        lock.add_package(pkg)
                    elif "git" in spec:
                        pkg = resolve_git_dependency(name, spec, project_root)
                        lock.add_package(pkg)
                    else:
                        version = spec.get("version", "*")
                        if resolve_registry:
                            try:
                                pkg = resolve_registry_dependency(
                                    name, version, project_root
                                )
                            except Exception as reg_exc:
                                print(
                                    f"  Warning: Could not resolve registry dep "
                                    f"'{name}' ({reg_exc}). Recording requirement verbatim."
                                )
                                pkg = LockedPackage(
                                    name=name, version=version, source="registry"
                                )
                        else:
                            pkg = LockedPackage(
                                name=name, version=version, source="registry"
                            )
                        lock.add_package(pkg)
                else:
                    raise ValueError(f"Invalid dependency spec: {spec!r}")
            except Exception as exc:
                errors.append(f"  {name}: {exc}")

    if errors:
        raise ValueError(
            "Failed to resolve some dependencies:\n" + "\n".join(errors)
        )

    return lock
