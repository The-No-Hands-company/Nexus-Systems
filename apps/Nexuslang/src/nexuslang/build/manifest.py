"""
nexuslang.build.manifest — Parser for nexuslang.toml project manifests.

Parses project configuration files (nexuslang.toml) that describe package
metadata, dependencies, build targets, profiles, features, and workspace
configuration.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

try:
    import tomllib
except ImportError:
    import tomli as tomllib


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
    debug_assertions: bool = False
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
        "debug_assertions": True,
        "incremental": True,
        "lto": False,
        "strip": False,
        "panic": PanicStrategy.UNWIND,
    },
    "release": {
        "opt_level": 3,
        "debug": False,
        "debug_assertions": False,
        "incremental": False,
        "lto": True,
        "strip": True,
        "panic": PanicStrategy.UNWIND,
    },
    "test": {
        "opt_level": 0,
        "debug": True,
        "debug_assertions": True,
        "incremental": True,
        "lto": False,
        "strip": False,
        "panic": PanicStrategy.UNWIND,
    },
    "bench": {
        "opt_level": 3,
        "debug": False,
        "debug_assertions": False,
        "incremental": False,
        "lto": True,
        "strip": False,
        "panic": PanicStrategy.UNWIND,
    },
}

_PACKAGE_NAME_RE = re.compile(r'^[a-z0-9][a-z0-9_-]*$')
_VERSION_RE = re.compile(r'^\d+\.\d+(\.\d+)?([.-][a-zA-Z0-9.+]+)?$')
_OPT_LEVEL_STRINGS = {"s", "z"}
_LTO_STRINGS = {"off", "thin", "fat"}
_STRIP_STRINGS = {"none", "debuginfo", "symbols"}


def load_manifest_data(path: Union[str, Path]) -> Dict[str, Any]:
    """Load and parse manifest TOML from *path*."""
    manifest_path = Path(path)
    with open(manifest_path, "rb") as f:
        return tomllib.load(f)


# ---------------------------------------------------------------------------
# Manifest
# ---------------------------------------------------------------------------

class Manifest:
    """Parsed representation of a nexuslang.toml manifest file."""

    def __init__(
        self,
        path: Optional[Union[str, Path]] = None,
        data: Optional[Dict[str, Any]] = None,
    ):
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

        if data is not None:
            self._parse(data)
        elif self._path is not None:
            self._load(self._path)

    @classmethod
    def from_data(
        cls,
        data: Dict[str, Any],
        path: Optional[Union[str, Path]] = None,
    ) -> "Manifest":
        """Construct a manifest from already-loaded TOML data."""
        return cls(path=path, data=data)

    # ------------------------------------------------------------------
    # Loading
    # ------------------------------------------------------------------

    def _load(self, path: Path) -> None:
        if not path.exists():
            raise FileNotFoundError(f"Could not find nlpl.toml at {path}")

        data = load_manifest_data(path)

        self._parse(data)

    def _parse(self, data: Dict[str, Any]) -> None:
        data = self._ensure_dict(data, "root")

        if "package" in data:
            self.package = self._parse_package(data["package"])

        self.dependencies = self._parse_deps(data.get("dependencies", {}))
        self.dev_dependencies = self._parse_deps(data.get("dev-dependencies", {}))
        self.build_dependencies = self._parse_deps(data.get("build-dependencies", {}))

        for bin_data in data.get("bin", []):
            self.binary_targets.append(self._parse_binary(bin_data))

        if "lib" in data:
            self.library_target = self._parse_library(data["lib"])

        profile_data = data.get("profile", {})
        if profile_data is not None and not isinstance(profile_data, dict):
            raise ValueError("Section 'profile' must be a table")
        self._parse_profiles(profile_data or {})

        features_data = data.get("features", {})
        if features_data is not None and not isinstance(features_data, dict):
            raise ValueError("Section 'features' must be a table")

        self.features = {}
        for feature_name, deps in (features_data or {}).items():
            if not isinstance(feature_name, str):
                raise ValueError("Feature names must be strings")
            if not isinstance(deps, list) or not all(
                isinstance(dep, str) for dep in deps
            ):
                raise ValueError(
                    f"Invalid feature '{feature_name}': expected a list of string entries"
                )
            self.features[feature_name] = list(deps)

        if "workspace" in data:
            self.workspace = self._parse_workspace(data["workspace"])

        target_section = data.get("target", {})
        if isinstance(target_section, dict):
            for target_spec, target_cfg in target_section.items():
                if isinstance(target_cfg, dict):
                    deps = target_cfg.get("dependencies", {})
                    if isinstance(deps, dict):
                        self.target_specific_deps[target_spec] = self._parse_deps(deps)

        if self.package is None and self.workspace is None:
            raise ValueError("Manifest must contain either [package] or [workspace]")

    # ------------------------------------------------------------------
    # Section parsers
    # ------------------------------------------------------------------

    def _ensure_dict(self, value: Any, section: str) -> Dict[str, Any]:
        if not isinstance(value, dict):
            raise ValueError(f"Section '{section}' must be a table")
        return value

    def _validate_opt_level(self, value: Any, profile_name: str) -> Union[int, str]:
        if isinstance(value, bool):
            raise ValueError(
                f"Invalid profile '{profile_name}' field 'opt-level': expected integer in range 0..3 or one of {sorted(_OPT_LEVEL_STRINGS)}"
            )
        if isinstance(value, int):
            if 0 <= value <= 3:
                return value
            raise ValueError(
                f"Invalid profile '{profile_name}' field 'opt-level': expected integer in range 0..3 or one of {sorted(_OPT_LEVEL_STRINGS)}"
            )
        if isinstance(value, str) and value in _OPT_LEVEL_STRINGS:
            return value
        raise ValueError(
            f"Invalid profile '{profile_name}' field 'opt-level': expected integer in range 0..3 or one of {sorted(_OPT_LEVEL_STRINGS)}"
        )

    def _validate_bool_field(self, field_name: str, value: Any, profile_name: str) -> bool:
        if isinstance(value, bool):
            return value
        raise ValueError(
            f"Invalid profile '{profile_name}' field '{field_name}': expected boolean"
        )

    def _validate_lto(self, value: Any, profile_name: str) -> Union[bool, str]:
        if isinstance(value, bool):
            return value
        if isinstance(value, str) and value in _LTO_STRINGS:
            return value
        raise ValueError(
            f"Invalid profile '{profile_name}' field 'lto': expected boolean or one of {sorted(_LTO_STRINGS)}"
        )

    def _validate_strip(self, value: Any, profile_name: str) -> Union[bool, str]:
        if isinstance(value, bool):
            return value
        if isinstance(value, str) and value in _STRIP_STRINGS:
            return value
        raise ValueError(
            f"Invalid profile '{profile_name}' field 'strip': expected boolean or one of {sorted(_STRIP_STRINGS)}"
        )

    def _validate_codegen_units(self, value: Any, profile_name: str) -> Optional[int]:
        if value is None:
            return None
        if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
            raise ValueError(
                f"Invalid profile '{profile_name}' field 'codegen-units': expected a positive integer"
            )
        return value

    def _parse_package(self, pkg: Dict[str, Any]) -> PackageMetadata:
        pkg = self._ensure_dict(pkg, "package")

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
        deps = self._ensure_dict(deps, "dependency")

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
        data = self._ensure_dict(data, "bin")
        if "name" not in data or not isinstance(data["name"], str):
            raise ValueError("Each [[bin]] entry must include string field 'name'")
        return BinaryTarget(
            name=data["name"],
            path=data.get("path", f"src/bin/{data['name']}.nxl"),
            required_features=list(data.get("required-features", [])),
        )

    def _parse_library(self, data: Dict[str, Any]) -> LibraryTarget:
        data = self._ensure_dict(data, "lib")
        raw_crate_types = data.get("crate-type", ["lib"])
        if not isinstance(raw_crate_types, list) or not all(
            isinstance(ct, str) for ct in raw_crate_types
        ):
            raise ValueError("Field 'lib.crate-type' must be a list of strings")
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
            if not isinstance(raw, dict):
                raise ValueError(f"Profile 'profile.{name}' must be a table")

            inherits = raw.get("inherits")
            if inherits is not None and not isinstance(inherits, str):
                raise ValueError(
                    f"Invalid profile '{name}' field 'inherits': expected string"
                )

            base: Dict[str, Any] = {}

            if inherits and inherits in self.profiles:
                # Inherit from the named profile
                parent = self.profiles[inherits]
                base = {
                    "opt_level": parent.opt_level,
                    "debug": parent.debug,
                    "debug_assertions": parent.debug_assertions,
                    "incremental": parent.incremental,
                    "lto": parent.lto,
                    "strip": parent.strip,
                    "panic": parent.panic,
                    "codegen_units": parent.codegen_units,
                }
            elif inherits:
                raise ValueError(
                    f"Invalid profile '{name}' field 'inherits': unknown profile '{inherits}'"
                )
            elif name in _DEFAULT_PROFILES:
                base = dict(_DEFAULT_PROFILES[name])

            # Map TOML keys to dataclass field names
            opt_level = self._validate_opt_level(
                raw.get("opt-level", base.get("opt_level", 0)),
                name,
            )
            debug = self._validate_bool_field(
                "debug",
                raw.get("debug", base.get("debug", False)),
                name,
            )
            debug_assertions = self._validate_bool_field(
                "debug-assertions",
                raw.get(
                "debug-assertions",
                base.get("debug_assertions", debug),
                ),
                name,
            )
            incremental = self._validate_bool_field(
                "incremental",
                raw.get("incremental", base.get("incremental", False)),
                name,
            )
            lto = self._validate_lto(raw.get("lto", base.get("lto", False)), name)
            strip = self._validate_strip(raw.get("strip", base.get("strip", False)), name)
            panic_raw = raw.get("panic", base.get("panic", PanicStrategy.UNWIND))
            if isinstance(panic_raw, str):
                if panic_raw not in (p.value for p in PanicStrategy):
                    raise ValueError(
                        f"Invalid profile '{name}' field 'panic': expected one of {[p.value for p in PanicStrategy]}"
                    )
                panic_val = PanicStrategy(panic_raw)
            elif isinstance(panic_raw, PanicStrategy):
                panic_val = panic_raw
            else:
                raise ValueError(
                    f"Invalid profile '{name}' field 'panic': expected string"
                )
            codegen_units = self._validate_codegen_units(
                raw.get("codegen-units", base.get("codegen_units")),
                name,
            )

            self.profiles[name] = BuildProfile(
                name=name,
                opt_level=opt_level,
                debug=debug,
                debug_assertions=debug_assertions,
                incremental=incremental,
                lto=lto,
                strip=strip,
                panic=panic_val,
                codegen_units=codegen_units,
                inherits=inherits,
            )

    def _parse_workspace(self, data: Dict[str, Any]) -> WorkspaceConfig:
        data = self._ensure_dict(data, "workspace")
        members = data.get("members", [])
        exclude = data.get("exclude", [])
        resolver = data.get("resolver")

        if not isinstance(members, list) or not all(
            isinstance(member, str) for member in members
        ):
            raise ValueError("Field 'workspace.members' must be a list of strings")
        if not isinstance(exclude, list) or not all(
            isinstance(item, str) for item in exclude
        ):
            raise ValueError("Field 'workspace.exclude' must be a list of strings")
        if resolver is not None and not isinstance(resolver, str):
            raise ValueError("Field 'workspace.resolver' must be a string")

        return WorkspaceConfig(
            members=list(members),
            exclude=list(exclude),
            resolver=resolver,
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
    "load_manifest_data",
    "Dependency",
    "BuildProfile",
    "PanicStrategy",
    "CrateType",
    "PackageMetadata",
    "BinaryTarget",
    "LibraryTarget",
    "WorkspaceConfig",
]
