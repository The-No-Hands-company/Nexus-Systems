"""
NLPL Workspace Manager

Provides support for multi-package workspaces similar to Cargo workspaces:

  - A workspace root contains ``nlpl-workspace.toml``
  - Member packages are NexusLang projects (each has its own ``nlpl.toml``)
  - A single, shared ``nlpl.lock`` lives at the workspace root
  - Members can be built individually or all at once in dependency order

Workspace manifest format (``nlpl-workspace.toml``)
----------------------------------------------------
  [workspace]
  members = [
      "packages/*",      # Glob pattern - matches any subdirectory
      "tools/my-tool",   # Explicit path
  ]

  [workspace.metadata]  # Optional
  description = "My awesome NexusLang monorepo"

Usage
-----
  From the workspace root:
    nlpl workspace list          # List all members and their build status
    nlpl workspace build         # Build all members in dependency order
    nlpl workspace build mylib   # Build a single named member
    nlpl workspace clean         # Clean all members
    nlpl workspace test          # Run tests across all members

  When the optional root package coexists with a workspace manifest, the root
  package is treated as an additional workspace member (a "pure workspace"
  means the root has no ``nlpl.toml``).
"""

from __future__ import annotations

import glob as _glob
import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterator, List, Optional, Set, Tuple, Any


# ---------------------------------------------------------------------------
# Internal TOML helpers
# ---------------------------------------------------------------------------

def _load_toml(path: Path) -> Dict[str, Any]:
    try:
        import tomllib          # Python 3.11+
    except ImportError:
        import tomli as tomllib  # type: ignore[no-redef]
    with open(path, "rb") as f:
        return tomllib.load(f)


def _save_toml(path: Path, data: Dict[str, Any]) -> None:
    try:
        import tomli_w
    except ImportError:
        raise RuntimeError(
            "tomli_w is required to write TOML files.  "
            "Install it with:  pip install tomli-w"
        )
    path.write_bytes(tomli_w.dumps(data).encode("utf-8"))


# ---------------------------------------------------------------------------
# Workspace manifest
# ---------------------------------------------------------------------------

WORKSPACE_MANIFEST_NAME = "nlpl-workspace.toml"
PACKAGE_MANIFEST_NAME = "nexuslang.toml"
LOCK_FILE_NAME = "nexuslang.lock"


@dataclass
class WorkspaceManifest:
    """Parsed contents of ``nlpl-workspace.toml``.

    Attributes:
        root:          Absolute path of the workspace root directory.
        member_globs:  Raw glob patterns from ``[workspace] members``.
        description:   Optional workspace description.
    """

    root: Path
    member_globs: List[str] = field(default_factory=list)
    description: str = ""

    # ------------------------------------------------------------------
    # Loading
    # ------------------------------------------------------------------

    @classmethod
    def load(cls, workspace_root: Path) -> "WorkspaceManifest":
        """Parse ``nlpl-workspace.toml`` from *workspace_root*.

        Args:
            workspace_root: Directory that should contain the manifest.

        Returns:
            Parsed WorkspaceManifest.

        Raises:
            FileNotFoundError: If ``nlpl-workspace.toml`` does not exist.
            ValueError:        If the ``[workspace]`` section is missing.
        """
        manifest_path = workspace_root / WORKSPACE_MANIFEST_NAME
        if not manifest_path.exists():
            raise FileNotFoundError(
                f"{WORKSPACE_MANIFEST_NAME} not found in {workspace_root}. "
                "Run `nlpl workspace init` to create one."
            )

        data = _load_toml(manifest_path)
        ws_section = data.get("workspace")
        if ws_section is None:
            raise ValueError(
                f"{WORKSPACE_MANIFEST_NAME} is missing a [workspace] section."
            )

        member_globs = ws_section.get("members", [])
        if not isinstance(member_globs, list):
            raise ValueError(
                "[workspace] members must be a list of glob patterns/paths."
            )

        meta = ws_section.get("metadata", {})
        return cls(
            root=workspace_root.resolve(),
            member_globs=member_globs,
            description=meta.get("description", ""),
        )

    # ------------------------------------------------------------------
    # Saving
    # ------------------------------------------------------------------

    def save(self) -> None:
        """Write this manifest back to ``nlpl-workspace.toml``."""
        data: Dict[str, Any] = {
            "workspace": {
                "members": self.member_globs,
            }
        }
        if self.description:
            data["workspace"]["metadata"] = {"description": self.description}
        _save_toml(self.root / WORKSPACE_MANIFEST_NAME, data)

    # ------------------------------------------------------------------
    # Member glob expansion
    # ------------------------------------------------------------------

    def expand_members(self) -> List[Path]:
        """Expand ``member_globs`` to a deduplicated list of absolute paths.

        Each returned path is a directory that contains an ``nlpl.toml``.
        Patterns that match nothing are silently skipped (a warning is issued
        only if *all* patterns produce no members).

        Returns:
            Sorted list of resolved member root directories.

        Raises:
            WorkspaceError: If a matched directory does not contain nlpl.toml.
        """
        found: Dict[Path, None] = {}  # Ordered set via dict keys

        for pattern in self.member_globs:
            # Make absolute by joining with workspace root
            abs_pattern = str(self.root / pattern)
            matches = sorted(_glob.glob(abs_pattern, recursive=True))
            for match in matches:
                p = Path(match).resolve()
                if p.is_dir():
                    found[p] = None

        # If no members expanded yet and there are explicit names, try treating
        # them as literal directory names (handles cases without glob chars).
        if not found:
            for pattern in self.member_globs:
                direct = (self.root / pattern).resolve()
                if direct.is_dir():
                    found[direct] = None

        # Validate each candidate
        members: List[Path] = []
        for p in found:
            manifet = p / PACKAGE_MANIFEST_NAME
            if not manifet.exists():
                raise WorkspaceError(
                    f"Workspace member '{p}' does not contain an "
                    f"{PACKAGE_MANIFEST_NAME} file (legacy: nlpl.toml)."
                )
            members.append(p)

        return members


# ---------------------------------------------------------------------------
# Workspace member
# ---------------------------------------------------------------------------

@dataclass
class WorkspaceMember:
    """An individual NexusLang package that is a member of a workspace.

    Attributes:
        root:         Absolute path of the member's project root.
        name:         Package name from ``nlpl.toml``.
        version:      Package version from ``nlpl.toml``.
        dependencies: Mapping of dependency name -> dependency spec (raw TOML
                      value) from ``[dependencies]``.
        dev_dependencies: Same, but from ``[dev-dependencies]``.
    """

    root: Path
    name: str
    version: str
    description: str = ""
    dependencies: Dict[str, Any] = field(default_factory=dict)
    dev_dependencies: Dict[str, Any] = field(default_factory=dict)

    # ------------------------------------------------------------------
    # Constructors
    # ------------------------------------------------------------------

    @classmethod
    def load(cls, member_root: Path) -> "WorkspaceMember":
        """Parse a workspace member from its ``nlpl.toml``.

        Args:
            member_root: Root directory of the member project.

        Returns:
            Populated WorkspaceMember.

        Raises:
            FileNotFoundError: If ``nlpl.toml`` is missing.
            ValueError:        If ``[package]`` section is absent.
        """
        manifest_path = member_root / PACKAGE_MANIFEST_NAME
        if not manifest_path.exists():
            raise FileNotFoundError(
                f"{PACKAGE_MANIFEST_NAME} not found in {member_root}."
            )

        data = _load_toml(manifest_path)
        pkg = data.get("package")
        if pkg is None:
            raise ValueError(
                f"{manifest_path}: missing [package] section."
            )

        return cls(
            root=member_root.resolve(),
            name=pkg.get("name", member_root.name),
            version=pkg.get("version", "0.0.0"),
            description=pkg.get("description", ""),
            dependencies=data.get("dependencies", {}),
            dev_dependencies=data.get("dev-dependencies", {}),
        )

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def intra_workspace_deps(self, all_members: Dict[str, "WorkspaceMember"]) -> List[str]:
        """Return names of workspace members that this member depends on.

        Only path-based dependencies pointing into the workspace count as
        intra-workspace dependencies.

        Args:
            all_members: Name -> WorkspaceMember mapping for the whole workspace.

        Returns:
            Sorted list of dependency names that are workspace members.
        """
        intra: List[str] = []
        for dep_name, spec in self.dependencies.items():
            if dep_name not in all_members:
                continue  # External dependency
            if isinstance(spec, dict) and "path" in spec:
                # Verify the path actually points to the other member
                dep_path = (self.root / spec["path"]).resolve()
                if dep_path == all_members[dep_name].root:
                    intra.append(dep_name)
            elif dep_name in all_members:
                # Non-path dep with same name as a workspace member — still a dep
                intra.append(dep_name)
        return sorted(set(intra))

    def __repr__(self) -> str:
        return f"WorkspaceMember({self.name!r} @ {self.root})"


# ---------------------------------------------------------------------------
# Workspace resolver
# ---------------------------------------------------------------------------

class WorkspaceResolver:
    """Discovers, validates, and resolves all members in a workspace.

    After calling :meth:`resolve`, the resolver provides:
      - :attr:`members` — ordered dict of name -> WorkspaceMember
      - :attr:`build_order` — topological build order (list of names)

    Args:
        manifest: Parsed WorkspaceManifest.
    """

    def __init__(self, manifest: WorkspaceManifest) -> None:
        self.manifest = manifest
        self.members: Dict[str, WorkspaceMember] = {}
        self.build_order: List[str] = []
        self._resolved = False

    def resolve(self) -> "WorkspaceResolver":
        """Discover members, load their manifests, and compute build order.

        Returns:
            self (for chaining).

        Raises:
            WorkspaceError: On duplicate member names, missing manifests,
                             or dependency cycles.
        """
        member_paths = self.manifest.expand_members()

        # Optionally include the workspace root itself if it also has nlpl.toml
        root_manifest = self.manifest.root / PACKAGE_MANIFEST_NAME
        if root_manifest.exists() and self.manifest.root not in member_paths:
            member_paths.insert(0, self.manifest.root)

        # Load all members
        name_to_member: Dict[str, WorkspaceMember] = {}
        for path in member_paths:
            try:
                member = WorkspaceMember.load(path)
            except (FileNotFoundError, ValueError) as exc:
                raise WorkspaceError(
                    f"Failed to load workspace member at '{path}': {exc}"
                ) from exc

            if member.name in name_to_member:
                existing = name_to_member[member.name]
                raise WorkspaceError(
                    f"Duplicate workspace member name '{member.name}': "
                    f"'{path}' and '{existing.root}'"
                )
            name_to_member[member.name] = member

        self.members = name_to_member
        self.build_order = self._topological_sort()
        self._resolved = True
        return self

    def _topological_sort(self) -> List[str]:
        """Compute topological build order via Kahn's algorithm.

        Returns:
            Names ordered so each member appears before any member that
            depends on it.

        Raises:
            WorkspaceError: If the dependency graph contains a cycle.
        """
        # Build adjacency: dep -> set of packages that depend on it
        in_degree: Dict[str, int] = {name: 0 for name in self.members}
        dependents: Dict[str, Set[str]] = {name: set() for name in self.members}

        for name, member in self.members.items():
            for dep_name in member.intra_workspace_deps(self.members):
                in_degree[name] += 1
                dependents[dep_name].add(name)

        # Start with nodes that have no intra-workspace deps
        queue: List[str] = sorted(
            name for name, deg in in_degree.items() if deg == 0
        )
        order: List[str] = []

        while queue:
            current = queue.pop(0)
            order.append(current)
            for dependent in sorted(dependents[current]):
                in_degree[dependent] -= 1
                if in_degree[dependent] == 0:
                    queue.append(dependent)

        if len(order) != len(self.members):
            remaining = [n for n in self.members if n not in order]
            raise WorkspaceError(
                "Circular dependency detected among workspace members: "
                + ", ".join(remaining)
            )

        return order

    def get(self, name: str) -> WorkspaceMember:
        """Retrieve a member by name.

        Raises:
            KeyError: If *name* is not a known member.
        """
        if name not in self.members:
            available = ", ".join(sorted(self.members))
            raise KeyError(
                f"No workspace member named '{name}'. "
                f"Available: {available}"
            )
        return self.members[name]

    # ------------------------------------------------------------------
    # Shared lock file
    # ------------------------------------------------------------------

    def shared_lock_path(self) -> Path:
        """Return the path of the workspace-level lock file."""
        return self.manifest.root / LOCK_FILE_NAME

    def regenerate_shared_lock(self, quiet: bool = False) -> None:
        """Regenerate the shared ``nlpl.lock`` for all workspace members.

        Merges all member lockfiles into a single workspace-level lock file.
        Conflicts (same package name, different resolved versions) are
        reported as errors.

        Args:
            quiet: Suppress output.

        Raises:
            WorkspaceError: On version conflicts between members.
        """
        from .lockfile import generate_lockfile, LockFile, LockedPackage

        if not self._resolved:
            self.resolve()

        merged = LockFile.empty()
        conflicts: List[str] = []

        for name in self.build_order:
            member = self.members[name]
            try:
                member_lock = generate_lockfile(member.root)
            except Exception as exc:
                raise WorkspaceError(
                    f"Failed to generate lockfile for member '{name}': {exc}"
                ) from exc

            for pkg_name, pkg in member_lock.packages.items():
                existing = merged.get(pkg_name)
                if existing is not None and existing.version != pkg.version:
                    conflicts.append(
                        f"  '{pkg_name}': '{name}' requires {pkg.version}, "
                        f"but earlier member(s) resolved to {existing.version}"
                    )
                else:
                    merged.add_package(pkg)

        if conflicts:
            raise WorkspaceError(
                "Version conflicts detected in workspace:\n" + "\n".join(conflicts)
            )

        lock_path = self.shared_lock_path()
        merged.save(lock_path)

        if not quiet:
            n = len(merged.packages)
            print(
                f"Workspace lock: {n} package{'s' if n != 1 else ''} "
                f"-> {lock_path.relative_to(self.manifest.root)}"
            )


# ---------------------------------------------------------------------------
# Workspace builder
# ---------------------------------------------------------------------------

class WorkspaceBuilder:
    """Builds, cleans, and tests workspace members.

    Args:
        resolver: A resolved WorkspaceResolver.
    """

    def __init__(self, resolver: WorkspaceResolver) -> None:
        if not resolver._resolved:
            resolver.resolve()
        self.resolver = resolver

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _build_system_for(self, member: WorkspaceMember):
        """Create a BuildSystem instance for *member*."""
        from .config import ConfigLoader
        from .builder import BuildSystem

        manifest = member.root / PACKAGE_MANIFEST_NAME
        config = ConfigLoader.load(str(manifest))
        return BuildSystem(config)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def build_all(
        self,
        *,
        release: bool = False,
        profile: Optional[str] = None,
        features: Optional[List[str]] = None,
        jobs: Optional[int] = None,
        quiet: bool = False,
    ) -> bool:
        """Build all workspace members in topological order.

        Args:
            release: Use the release profile.
            profile: Named build profile (overrides ``release``).
            features: List of feature flags to enable.
            jobs:    Parallel compilation worker count.
            quiet:   Suppress informational output.

        Returns:
            True if all members built successfully, False otherwise.
        """
        feature_list = features or []
        all_ok = True

        for name in self.resolver.build_order:
            member = self.resolver.members[name]
            if not quiet:
                print(f"  Building [{name}] ({member.version})")

            try:
                bs = self._build_system_for(member)
                ok = bs.build(
                    release=release,
                    profile=profile,
                    features=feature_list,
                    jobs=jobs,
                )
            except Exception as exc:
                print(f"error: [{name}] build failed: {exc}")
                ok = False

            if not ok:
                print(f"error: [{name}] build failed")
                all_ok = False
                break  # Stop on first failure (like cargo --fail-fast)

        return all_ok

    def build_member(
        self,
        name: str,
        *,
        release: bool = False,
        profile: Optional[str] = None,
        features: Optional[List[str]] = None,
        jobs: Optional[int] = None,
        quiet: bool = False,
    ) -> bool:
        """Build a single named workspace member.

        Automatically builds any intra-workspace dependencies first (in
        topological order) if they have not already been built.

        Args:
            name:    Name of the member to build.
            release: Use the release profile.
            profile: Named build profile.
            features: Feature flags.
            jobs:    Worker count.
            quiet:   Suppress output.

        Returns:
            True on success, False on failure.
        """
        member = self.resolver.get(name)  # Raises KeyError if unknown
        feature_list = features or []

        # Determine which members need to be built first
        build_set: List[str] = []
        for ordered_name in self.resolver.build_order:
            if ordered_name == name:
                build_set.append(name)
                break
            ordered_member = self.resolver.members[ordered_name]
            if name in ordered_member.intra_workspace_deps(self.resolver.members):
                # This member is a transitive dependency of our target
                continue
            # Only include members that `name` depends on
            if ordered_name in member.intra_workspace_deps(self.resolver.members):
                build_set.append(ordered_name)
        build_set.append(name)

        # Deduplicate while preserving order
        seen: Set[str] = set()
        ordered_build: List[str] = []
        for n in self.resolver.build_order:
            if n in build_set and n not in seen:
                ordered_build.append(n)
                seen.add(n)

        all_ok = True
        for n in ordered_build:
            m = self.resolver.members[n]
            if not quiet:
                label = "Building" if n == name else "  Dep   "
                print(f"  {label} [{n}] ({m.version})")
            try:
                bs = self._build_system_for(m)
                ok = bs.build(
                    release=release,
                    profile=profile,
                    features=feature_list,
                    jobs=jobs,
                )
            except Exception as exc:
                print(f"error: [{n}] build failed: {exc}")
                ok = False

            if not ok:
                print(f"error: [{n}] build failed")
                all_ok = False
                break

        return all_ok

    def clean_all(self, quiet: bool = False) -> None:
        """Run `clean` on every workspace member.

        Args:
            quiet: Suppress output.
        """
        for name in self.resolver.build_order:
            member = self.resolver.members[name]
            if not quiet:
                print(f"  Cleaning [{name}]")
            try:
                bs = self._build_system_for(member)
                bs.clean()
            except Exception as exc:
                print(f"  Warning: [{name}] clean failed: {exc}")

    def test_all(
        self,
        *,
        filter_names: Optional[List[str]] = None,
        release: bool = False,
        features: Optional[List[str]] = None,
        quiet: bool = False,
    ) -> int:
        """Run tests across all workspace members.

        Args:
            filter_names: Only run tests whose filename contains one of these strings.
            release:      Run tests with the release profile.
            features:     Feature flags.
            quiet:        Suppress output.

        Returns:
            0 if all tests passed, non-zero otherwise.
        """
        feature_list = features or []
        overall_exit_code = 0

        for name in self.resolver.build_order:
            member = self.resolver.members[name]
            if not quiet:
                print(f"\n  Testing [{name}] ...")
            try:
                bs = self._build_system_for(member)
                code = bs.test(
                    filter_names=filter_names if filter_names else None,
                    release=release,
                    features=feature_list,
                )
            except Exception as exc:
                print(f"error: [{name}] test setup failed: {exc}")
                code = 1

            if code != 0:
                overall_exit_code = code

        return overall_exit_code

    def status(self) -> List[Dict[str, Any]]:
        """Return status information for all workspace members.

        Returns:
            List of dicts with keys: name, version, description, root, member_count.
        """
        result: List[Dict[str, Any]] = []
        for name in self.resolver.build_order:
            m = self.resolver.members[name]
            result.append({
                "name": name,
                "version": m.version,
                "description": m.description,
                "root": str(m.root),
                "intra_deps": m.intra_workspace_deps(self.resolver.members),
            })
        return result


# ---------------------------------------------------------------------------
# High-level entry points
# ---------------------------------------------------------------------------

def init_workspace(
    root: Path,
    *,
    members: Optional[List[str]] = None,
    description: str = "",
    quiet: bool = False,
) -> WorkspaceManifest:
    """Create a new workspace manifest at *root*.

    If ``nlpl-workspace.toml`` already exists the operation is aborted.

    Args:
        root:        Directory to initialise as a workspace root.
        members:     Initial member glob patterns.  Defaults to
                     ``["packages/*"]``.
        description: Optional workspace description.
        quiet:       Suppress output.

    Returns:
        The newly created WorkspaceManifest.

    Raises:
        FileExistsError: If ``nlpl-workspace.toml`` already exists.
    """
    manifest_path = root / WORKSPACE_MANIFEST_NAME
    if manifest_path.exists():
        raise FileExistsError(
            f"{WORKSPACE_MANIFEST_NAME} already exists in {root}."
        )

    root.mkdir(parents=True, exist_ok=True)
    manifest = WorkspaceManifest(
        root=root.resolve(),
        member_globs=members if members is not None else ["packages/*"],
        description=description,
    )
    manifest.save()

    # Create packages/ directory as a hint
    if members is None:
        (root / "packages").mkdir(exist_ok=True)
        (root / "packages" / ".gitkeep").write_text("")

    # Write a .gitignore pointing out build artefacts if needed
    gi = root / ".gitignore"
    if not gi.exists():
        gi.write_text(
            "# NexusLang workspace\n"
            "build/\n"
            "*.o\n"
            "*.ll\n"
        )

    if not quiet:
        print(f"  Initialised workspace in {root}")
        glob_list = "\n".join(f"    - {g}" for g in manifest.member_globs)
        print(f"  Members:\n{glob_list}")

    return manifest


def load_workspace(path: Path) -> Tuple[WorkspaceManifest, WorkspaceResolver]:
    """Convenience function: load + resolve a workspace from a root directory.

    Walks up the directory tree from *path* until it finds an
    ``nlpl-workspace.toml``.

    Args:
        path: Starting directory (usually ``Path.cwd()``).

    Returns:
        (manifest, resolver) tuple — the resolver has already been resolved.

    Raises:
        FileNotFoundError: If no workspace manifest is found in the tree.
        WorkspaceError:    On manifest or member validation errors.
    """
    workspace_root = _find_workspace_root(path)
    if workspace_root is None:
        raise FileNotFoundError(
            f"No {WORKSPACE_MANIFEST_NAME} found in '{path}' or any parent directory. "
            f"Run `nlpl workspace init` to create a workspace."
        )
    manifest = WorkspaceManifest.load(workspace_root)
    resolver = WorkspaceResolver(manifest).resolve()
    return manifest, resolver


def _find_workspace_root(start: Path) -> Optional[Path]:
    """Walk up from *start* looking for ``nlpl-workspace.toml``."""
    current = start.resolve()
    while True:
        if (current / WORKSPACE_MANIFEST_NAME).exists():
            return current
        parent = current.parent
        if parent == current:
            return None
        current = parent


# ---------------------------------------------------------------------------
# Exceptions
# ---------------------------------------------------------------------------

class WorkspaceError(Exception):
    """Raised when workspace operations fail."""
