"""
NLPL Build System - Dependency Manager

Handles adding and removing dependencies in nlpl.toml and maintaining
the nlpl.lock file in sync with the manifest.

Public API
----------
add_dependency(project_root, package_spec, ...)
remove_dependency(project_root, package_name, ...)
list_dependencies(project_root)
update_lockfile(project_root)
"""

from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from .lockfile import LockFile, generate_lockfile


# ---------------------------------------------------------------------------
# TOML helpers (read and write without disturbing unrelated sections)
# ---------------------------------------------------------------------------

def _load_toml(path: Path) -> Dict[str, Any]:
    try:
        import tomllib  # Python 3.11+
    except ImportError:
        import tomli as tomllib  # type: ignore[no-redef]
    with open(path, "rb") as f:
        return tomllib.load(f)


def _save_toml(path: Path, data: Dict[str, Any]) -> None:
    try:
        import tomli_w
    except ImportError:
        raise RuntimeError(
            "tomli_w is required to edit nlpl.toml. "
            "Install it with: pip install tomli-w"
        )
    with open(path, "wb") as f:
        tomli_w.dump(data, f)


# ---------------------------------------------------------------------------
# Spec parsing
# ---------------------------------------------------------------------------

def _parse_package_spec(package_spec: str) -> Tuple[str, Optional[str]]:
    """Parse ``name[@version_req]`` into ``(name, version_req | None)``.

    Examples::

        "requests"       -> ("requests", None)
        "requests@^2.0"  -> ("requests", "^2.0")
        "my_lib@1.0.0"   -> ("my_lib", "1.0.0")
    """
    if not package_spec or not package_spec.strip():
        raise ValueError("Package spec must not be empty.")
    if "@" in package_spec:
        name, version = package_spec.split("@", 1)
        name = name.strip()
        if not name:
            raise ValueError(f"Invalid package spec (empty name): {package_spec!r}")
        return name, version.strip()
    return package_spec.strip(), None


# ---------------------------------------------------------------------------
# Lock file maintenance
# ---------------------------------------------------------------------------

def _regenerate_lock(
    project_root: Path,
    quiet: bool = False,
    offline: bool = False,
) -> None:
    """Regenerate nlpl.lock from the current nlpl.toml state.

    Prints a status line unless *quiet* is True.  On failure prints a
    warning rather than raising so that the calling command still succeeds
    (the user can run ``nlpl lock`` manually to retry).

    Args:
        project_root: Root directory of the NexusLang project.
        quiet:        Suppress output when True.
        offline:      Skip registry network calls when True.
    """
    lock_path = project_root / "nexuslang.lock"
    try:
        lock = generate_lockfile(project_root, resolve_registry=not offline)
        lock.save(lock_path)
        n = len(lock.packages)
        if not quiet:
            print(f"  Updated nlpl.lock ({n} package{'s' if n != 1 else ''})")
    except Exception as exc:
        print(f"  Warning: Could not update nlpl.lock: {exc}")
        print("  Run `nlpl lock` to retry.")


# ---------------------------------------------------------------------------
# Public commands
# ---------------------------------------------------------------------------

def add_dependency(
    project_root: Path,
    package_spec: str,
    *,
    dev: bool = False,
    path: Optional[str] = None,
    git: Optional[str] = None,
    branch: Optional[str] = None,
    tag: Optional[str] = None,
    rev: Optional[str] = None,
) -> None:
    """Add a dependency to nlpl.toml and update nlpl.lock.

    Args:
        project_root:  Root of the NexusLang project (must contain nlpl.toml).
        package_spec:  Package name with optional version: ``"name"`` or
                       ``"name@^1.2"``.
        dev:           When True, add to ``[dev-dependencies]`` instead of
                       ``[dependencies]``.
        path:          Local filesystem path to the dependency package.
        git:           Git repository URL.
        branch:        Git branch (used together with *git*).
        tag:           Git tag (used together with *git*).
        rev:           Exact git revision / commit hash.

    Raises:
        FileNotFoundError: If nlpl.toml is not found.
        ValueError:        If the package spec is invalid or the combination
                           of *path*, *git* arguments is contradictory.
    """
    manifest_path = project_root / "nexuslang.toml"
    if not manifest_path.exists():
        raise FileNotFoundError(
            f"nexuslang.toml not found in {project_root}. "
            "Run `nlpl init` or `nlpl new` first."
        )

    name, version_req = _parse_package_spec(package_spec)

    # Validate name
    import re
    if not re.match(r'^[a-z0-9_-]+$', name):
        raise ValueError(
            f"Invalid package name '{name}'. "
            "Names must be lowercase, and may contain hyphens or underscores."
        )

    # Contradict check
    if path and git:
        raise ValueError("Specify either --path or --git, not both.")

    # Build dep value
    dep_value: Any
    if path:
        dep_value = {"path": path}
        if version_req:
            dep_value["version"] = version_req
    elif git:
        dep_value = {"git": git}
        if branch:
            dep_value["branch"] = branch
        if tag:
            dep_value["tag"] = tag
        if rev:
            dep_value["rev"] = rev
        if version_req:
            dep_value["version"] = version_req
    else:
        dep_value = version_req or "*"

    section = "dev-dependencies" if dev else "dependencies"

    data = _load_toml(manifest_path)
    if section not in data:
        data[section] = {}

    action = "Updating" if name in data[section] else "Adding"
    data[section][name] = dep_value
    _save_toml(manifest_path, data)

    print(f"  {action} '{name}' in [{section}]")
    print(f"  Updated nlpl.toml")

    _regenerate_lock(project_root)


def remove_dependency(
    project_root: Path,
    package_name: str,
    *,
    dev: bool = False,
) -> None:
    """Remove a dependency from nexuslang.toml and update nlpl.lock.

    If *dev* is False (the default) both ``[dependencies]`` and
    ``[dev-dependencies]`` are searched so that the caller does not need to
    know which section the package lives in.  When *dev* is True only
    ``[dev-dependencies]`` is searched.

    Args:
        project_root:  Root of the NexusLang project.
        package_name:  Name of the package to remove.
        dev:           Restrict search to ``[dev-dependencies]``.

    Raises:
        FileNotFoundError: If nlpl.toml is not found.
        ValueError:        If the package is not found in any searched section.
    """
    manifest_path = project_root / "nexuslang.toml"
    if not manifest_path.exists():
        raise FileNotFoundError(f"nexuslang.toml not found in {project_root}.")

    data = _load_toml(manifest_path)

    search_sections: List[str]
    if dev:
        search_sections = ["dev-dependencies"]
    else:
        search_sections = ["dependencies", "dev-dependencies", "build-dependencies"]

    removed = False
    for section in search_sections:
        if section in data and package_name in data[section]:
            del data[section][package_name]
            removed = True
            print(f"  Removed '{package_name}' from [{section}]")
            break

    if not removed:
        searched = ", ".join(f"[{s}]" for s in search_sections)
        raise ValueError(
            f"Package '{package_name}' not found in {searched}."
        )

    _save_toml(manifest_path, data)
    print(f"  Updated nlpl.toml")

    _regenerate_lock(project_root)


def update_lockfile(project_root: Path, *, offline: bool = False) -> None:
    """Regenerate nlpl.lock from nexuslang.toml, printing a summary.

    Args:
        project_root: Root directory of the NexusLang project.
        offline:      Skip registry network calls when True.

    Raises:
        FileNotFoundError: If nlpl.toml is not found.
        ValueError:        If any dependency cannot be resolved.
    """
    lock_path = project_root / "nexuslang.lock"
    lock = generate_lockfile(project_root, resolve_registry=not offline)
    lock.save(lock_path)

    n = len(lock.packages)
    print(f"Generated nlpl.lock ({n} package{'s' if n != 1 else ''})")
    for pkg in sorted(lock.packages.values(), key=lambda p: p.name):
        source_desc = pkg.source
        if pkg.source == "path" and pkg.resolved_path:
            source_desc = f"path: {pkg.resolved_path}"
        elif pkg.source == "git" and pkg.git_url:
            commit_short = (pkg.git_commit or "unknown")[:8]
            source_desc = f"git: {pkg.git_url} #{commit_short}"
        print(f"  {pkg.name} {pkg.version} ({source_desc})")


def list_dependencies(project_root: Path) -> None:
    """Print all dependencies and their lock status to stdout.

    Args:
        project_root: Root of the NexusLang project.

    Raises:
        FileNotFoundError: If nlpl.toml is not found.
    """
    manifest_path = project_root / "nexuslang.toml"
    if not manifest_path.exists():
        raise FileNotFoundError(f"nexuslang.toml not found in {project_root}")

    data = _load_toml(manifest_path)

    lock_path = project_root / "nexuslang.lock"
    locked: Dict[str, Any] = {}
    if lock_path.exists():
        try:
            lf = LockFile.load(lock_path)
            locked = lf.packages
        except Exception:
            pass

    sections = [
        ("dependencies", "Runtime"),
        ("dev-dependencies", "Dev"),
        ("build-dependencies", "Build"),
    ]

    any_deps = False
    for section_key, label in sections:
        deps = data.get(section_key, {})
        if not deps:
            continue
        any_deps = True
        print(f"\n{label} dependencies:")
        for name, spec in sorted(deps.items()):
            version_req = spec if isinstance(spec, str) else spec.get("version", "*")
            locked_pkg = locked.get(name)

            if locked_pkg:
                v = locked_pkg.version
                src = locked_pkg.source
                if src == "path" and locked_pkg.resolved_path:
                    detail = f"path:{locked_pkg.resolved_path}"
                elif src == "git" and locked_pkg.git_commit:
                    detail = f"git#{locked_pkg.git_commit[:8]}"
                else:
                    detail = src
                lock_info = f" -> {v} ({detail})"
            else:
                lock_info = " (not locked - run `nlpl lock`)"

            print(f"  {name} {version_req}{lock_info}")

    if not any_deps:
        print("No dependencies declared.")
