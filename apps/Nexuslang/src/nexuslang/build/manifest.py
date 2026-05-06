"""
nexuslang.build.manifest — Parser for nexuslang.toml project manifests.

Parses project configuration files (nexuslang.toml) that describe package
metadata, dependencies, build targets, profiles, features, and workspace
configuration.
"""

from __future__ import annotations

import re
import tomllib
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Union


# ---------------------------------------------------------------------------
# Enums
# ---------------------------------------------------------------------------

class PanicStrategy(str, Enum):
    UNWIND = "unwind"
    ABORT = "abort"


class CrateType(str, Enum):
    LIB = "lib"
    RLIB = "rlib"
    DYLIB = "dylib"
    CDYLIB = "cdylib"
    STATICLIB = "staticlib"
    PROC_MACRO = "proc-macro"
    BIN = "bin"


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------

@dataclass
class PackageMetadata:
    """Package metadata from the [package] section."""
    name: str
    version: str
    edition: str = "2026"
    authors: List[str] = field(default_factory=list)
    license: Optional[str] = None
    description: Optional[str] = None
    repository: Optional[str] = None
    homepage: Optional[str] = None
    documentation: Optional[str] = None
    readme: Optional[str] = None
    keywords: List[str] = field(default_factory=list)
    categories: List[str] = field(default_factory=list)


@dataclass
class Dependency:
    """A dependency specification."""
    name: str
    version_req: Optional[str] = None
    path: Optional[str] = None
    git: Optional[str] = None
    branch: Optional[str] = None
    tag: Optional[str] = None
    rev: Optional[str] = None
    features: List[str] = field(default_factory=list)
    default_features: bool = True
    optional: bool = False


@dataclass
class BinaryTarget:
    """A [[bin]] target."""
    name: str
    path: str
    required_features: List[str] = field(default_factory=list)


@dataclass
class LibraryTarget:
    """A [lib] target."""
    name: str
    path: str
    crate_type: List[CrateType] = field(default_factory=lambda: [CrateType.LIB])
    required_features: List[str] = field(default_factory=list)


@dataclass
class BuildProfile:
    """A build profile (e.g. dev, release, or custom)."""
    name: str
    opt_level: Union[int, str] = 0
    debug: bool = False
    incremental: bool = False
    lto: Union[bool, str] = False
    strip: Union[bool, str] = False
    panic: PanicStrategy = PanicStrategy.UNWIND
    codegen_units: Optional[int] = None
    inherits: Optional[str] = None

    # Extra fields passed via **kwargs stored here
    _extra: Dict[str, Any] = field(default_factory=dict, repr=False, compare=False)


@dataclass
class WorkspaceConfig:
    """Workspace configuration."""
    members: List[str] = field(default_factory=list)
    exclude: List[str] = field(default_factory=list)
    resolver: Optional[str] = None


# ---------------------------------------------------------------------------
# Default profiles
# ---------------------------------------------------------------------------

_DEFAULT_PROFILES: Dict[str, Dict[str, Any]] = {
    "dev": {
        "opt_level": 0,
        "debug": True,
        "incremental": True,
        "lto": False,
        "strip": False,
        "panic": PanicStrategy.UNWIND,
    },
    "release": {
        "opt_level": 3,
        "debug": False,
        "incremental": False,
        "lto": True,
        "strip": True,
        "panic": PanicStrategy.UNWIND,
    },
    "test": {
        "opt_level": 0,
        "debug": True,
        "incremental": True,
        "lto": False,
        "strip": False,
        "panic": PanicStrategy.UNWIND,
    },
    "bench": {
        "opt_level": 3,
        "debug": False,
        "incremental": False,
        "lto": True,
        "strip": False,
        "panic": PanicStrategy.UNWIND,
    },
}

_PACKAGE_NAME_RE = re.compile(r'^[a-z0-9][a-z0-9_-]*$')
_VERSION_RE = re.compile(r'^\d+\.\d+(\.\d+)?([.-][a-zA-Z0-9.+]+)?$')


# ---------------------------------------------------------------------------
# Manifest
# ---------------------------------------------------------------------------

class Manifest:
    """Parsed representation of a nexuslang.toml manifest file."""

    def __init__(self, path: Optional[Union[str, Path]] = None):
        self._path: Optional[Path] = Path(path) if path is not None else None
        self.package: Optional[PackageMetadata] = None
        self.dependencies: Dict[str, Dependency] = {}
        self.dev_dependencies: Dict[str, Dependency] = {}
        self.build_dependencies: Dict[str, Dependency] = {}
        self.binary_targets: List[BinaryTarget] = []
        self.library_target: Optional[LibraryTarget] = None
        self.profiles: Dict[str, BuildProfile] = {}
        self.features: Dict[str, List[str]] = {}
        self.workspace: Optional[WorkspaceConfig] = None
        self.target_specific_deps: Dict[str, Dict[str, Dependency]] = {}

        if self._path is not None:
            self._load(self._path)

    # ------------------------------------------------------------------
    # Loading
    # ------------------------------------------------------------------

    def _load(self, path: Path) -> None:
        if not path.exists():
            raise FileNotFoundError(f"Could not find nlpl.toml at {path}")

        with open(path, "rb") as f:
            data = tomllib.load(f)

        self._parse(data)

    def _parse(self, data: Dict[str, Any]) -> None:
        if "package" in data:
            self.package = self._parse_package(data["package"])

        self.dependencies = self._parse_deps(data.get("dependencies", {}))
        self.dev_dependencies = self._parse_deps(data.get("dev-dependencies", {}))
        self.build_dependencies = self._parse_deps(data.get("build-dependencies", {}))

        for bin_data in data.get("bin", []):
            self.binary_targets.append(self._parse_binary(bin_data))

        if "lib" in data:
            self.library_target = self._parse_library(data["lib"])

        self._parse_profiles(data.get("profile", {}))

        self.features = {k: v for k, v in data.get("features", {}).items()}

        if "workspace" in data:
            self.workspace = self._parse_workspace(data["workspace"])

        target_section = data.get("target", {})
        if isinstance(target_section, dict):
            for target_spec, target_cfg in target_section.items():
                if isinstance(target_cfg, dict):
                    deps = target_cfg.get("dependencies", {})
                    self.target_specific_deps[target_spec] = self._parse_deps(deps)

        if self.package is None and self.workspace is None:
            raise ValueError("Manifest must contain either [package] or [workspace]")

    # ------------------------------------------------------------------
    # Section parsers
    # ------------------------------------------------------------------

    def _parse_package(self, pkg: Dict[str, Any]) -> PackageMetadata:
        name = pkg.get("name")
        if name is None:
            raise ValueError("Missing required field 'name' in [package]")
        version = pkg.get("version")
        if version is None:
            raise ValueError("Missing required field 'version' in [package]")

        if not _PACKAGE_NAME_RE.match(name):
            raise ValueError(
                f"Invalid package name '{name}': must be lowercase alphanumeric, "
                "hyphens, or underscores, and must start with a letter or digit"
            )
        if not _VERSION_RE.match(version):
            raise ValueError(
                f"Invalid version '{version}': must follow semantic versioning (e.g. 1.2.3)"
            )

        return PackageMetadata(
            name=name,
            version=version,
            edition=pkg.get("edition", "2026"),
            authors=list(pkg.get("authors", [])),
            license=pkg.get("license"),
            description=pkg.get("description"),
            repository=pkg.get("repository"),
            homepage=pkg.get("homepage"),
            documentation=pkg.get("documentation"),
            readme=pkg.get("readme"),
            keywords=list(pkg.get("keywords", [])),
            categories=list(pkg.get("categories", [])),
        )

    def _parse_deps(self, deps: Dict[str, Any]) -> Dict[str, Dependency]:
        result: Dict[str, Dependency] = {}
        for name, spec in deps.items():
            if isinstance(spec, str):
                result[name] = Dependency(name=name, version_req=spec)
            elif isinstance(spec, dict):
                result[name] = Dependency(
                    name=name,
                    version_req=spec.get("version"),
                    path=spec.get("path"),
                    git=spec.get("git"),
                    branch=spec.get("branch"),
                    tag=spec.get("tag"),
                    rev=spec.get("rev"),
                    features=list(spec.get("features", [])),
                    default_features=spec.get("default-features", True),
                    optional=spec.get("optional", False),
                )
            else:
                raise ValueError(f"Invalid dependency specification for '{name}'")
        return result

    def _parse_binary(self, data: Dict[str, Any]) -> BinaryTarget:
        return BinaryTarget(
            name=data["name"],
            path=data.get("path", f"src/bin/{data['name']}.nxl"),
            required_features=list(data.get("required-features", [])),
        )

    def _parse_library(self, data: Dict[str, Any]) -> LibraryTarget:
        raw_crate_types = data.get("crate-type", ["lib"])
        crate_types = [CrateType(ct) for ct in raw_crate_types]
        return LibraryTarget(
            name=data.get("name", ""),
            path=data.get("path", "src/lib.nxl"),
            crate_type=crate_types,
            required_features=list(data.get("required-features", [])),
        )

    def _parse_profiles(self, profiles_data: Dict[str, Any]) -> None:
        # Start with built-in defaults
        for name, defaults in _DEFAULT_PROFILES.items():
            self.profiles[name] = BuildProfile(name=name, **defaults)

        # Apply user-defined profiles (may override or add new ones)
        for name, raw in profiles_data.items():
            inherits = raw.get("inherits")
            base: Dict[str, Any] = {}

            if inherits and inherits in self.profiles:
                # Inherit from the named profile
                parent = self.profiles[inherits]
                base = {
                    "opt_level": parent.opt_level,
                    "debug": parent.debug,
                    "incremental": parent.incremental,
                    "lto": parent.lto,
                    "strip": parent.strip,
                    "panic": parent.panic,
                    "codegen_units": parent.codegen_units,
                }
            elif name in _DEFAULT_PROFILES:
                base = dict(_DEFAULT_PROFILES[name])

            # Map TOML keys to dataclass field names
            opt_level = raw.get("opt-level", base.get("opt_level", 0))
            debug = raw.get("debug", base.get("debug", False))
            incremental = raw.get("incremental", base.get("incremental", False))
            lto = raw.get("lto", base.get("lto", False))
            strip = raw.get("strip", base.get("strip", False))
            panic_raw = raw.get("panic", base.get("panic", PanicStrategy.UNWIND))
            if isinstance(panic_raw, str) and panic_raw not in (p.value for p in PanicStrategy):
                panic_val = PanicStrategy.UNWIND
            elif isinstance(panic_raw, str):
                panic_val = PanicStrategy(panic_raw)
            else:
                panic_val = panic_raw
            codegen_units = raw.get("codegen-units", base.get("codegen_units"))

            self.profiles[name] = BuildProfile(
                name=name,
                opt_level=opt_level,
                debug=debug,
                incremental=incremental,
                lto=lto,
                strip=strip,
                panic=panic_val,
                codegen_units=codegen_units,
                inherits=inherits,
            )

    def _parse_workspace(self, data: Dict[str, Any]) -> WorkspaceConfig:
        return WorkspaceConfig(
            members=list(data.get("members", [])),
            exclude=list(data.get("exclude", [])),
            resolver=data.get("resolver"),
        )

    # ------------------------------------------------------------------
    # Utility methods
    # ------------------------------------------------------------------

    def has_feature(self, feature: str) -> bool:
        return feature in self.features

    def get_feature_dependencies(self, feature: str) -> List[str]:
        return list(self.features.get(feature, []))

    def get_all_dependencies(self, include_dev: bool = False) -> Dict[str, Dependency]:
        result = dict(self.dependencies)
        if include_dev:
            result.update(self.dev_dependencies)
        return result

    def resolve_path(self, relative: Union[str, Path]) -> Path:
        if self._path is None:
            raise RuntimeError("Manifest has no associated file path")
        return self._path.parent / relative

    @property
    def manifest_path(self) -> Optional[Path]:
        return self._path


# ---------------------------------------------------------------------------
# load_manifest helper
# ---------------------------------------------------------------------------

def load_manifest(path: Optional[Union[str, Path]] = None) -> Manifest:
    """Load a manifest from an explicit path or search for nexuslang.toml upward from cwd."""
    if path is not None:
        return Manifest(path)

    # Search current directory (no upward walk — keep it simple)
    candidates = ["nexuslang.toml", "nlpl.toml"]
    import os
    cwd = Path(os.getcwd())
    for candidate in candidates:
        candidate_path = cwd / candidate
        if candidate_path.exists():
            return Manifest(candidate_path)

    raise FileNotFoundError(
        "Could not find nlpl.toml or nexuslang.toml in the current directory"
    )


__all__ = [
    "Manifest",
    "load_manifest",
    "Dependency",
    "BuildProfile",
    "PanicStrategy",
    "CrateType",
    "PackageMetadata",
    "BinaryTarget",
    "LibraryTarget",
    "WorkspaceConfig",
]
