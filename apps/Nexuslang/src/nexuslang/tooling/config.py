
import os
import sys
from dataclasses import dataclass, field
from typing import List, Optional, Dict, Any

# Try to import tomllib (Python 3.11+) or tomli
try:
    import tomllib
except ImportError:
    try:
        import tomli as tomllib
    except ImportError:
        # Fallback for older python without tomli installed
        # This is a very basic parser for bootstrapping
        class SimpleToml:
            def load(self, f):
                data = {}
                current_section = data
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#'): continue
                    if line.startswith('[') and line.endswith(']'):
                        section = line[1:-1]
                        current_section = {}
                        data[section] = current_section
                    elif '=' in line:
                        key, value = line.split('=', 1)
                        key = key.strip()
                        value = value.strip().strip('"').strip("'")
                        current_section[key] = value
                return data
        tomllib = SimpleToml()

@dataclass
class PackageConfig:
    name: str
    version: str
    authors: List[str] = field(default_factory=list)
    description: str = ""

@dataclass
class ProfileConfig:
    """Build profile (dev, release, or custom)."""
    name: str
    optimization: int = 0       # 0-3
    debug_info: bool = True
    debug_assertions: bool = True
    lto: bool = False            # Link-time optimisation
    incremental: bool = True
    strip: bool = False          # Strip symbols on release


# Built-in profiles
_PROFILE_DEV = ProfileConfig(
    name="dev",
    optimization=0,
    debug_info=True,
    debug_assertions=True,
    lto=False,
    incremental=True,
    strip=False,
)

_PROFILE_RELEASE = ProfileConfig(
    name="release",
    optimization=3,
    debug_info=False,
    debug_assertions=False,
    lto=True,
    incremental=False,
    strip=True,
)


@dataclass
class BuildConfig:
    source_dir: str = "src"
    output_dir: str = "build"
    target: str = "c"  # c, cpp, llvm_ir, etc.
    optimization: int = 0
    debug_info: bool = True
    headers: bool = False
    # Feature flags enabled by the user or manifest
    features: List[str] = field(default_factory=list)
    # Current active profile name ("dev" or "release" or custom)
    profile: str = "dev"
    # Target triple for cross-compilation (e.g. "aarch64-unknown-linux-gnu")
    target_triple: Optional[str] = None
    # Number of parallel compilation jobs (None = cpu_count)
    jobs: Optional[int] = None
    # Parallel compilation jobs: 0 = auto-detect, >0 = exact count
    parallel_jobs: int = 0
    # Link-time optimisation mode: "disabled" | "thin" | "full"
    lto: str = "disabled"
    # Sysroot for cross-compilation (e.g. "/opt/aarch64-sysroot")
    sysroot: Optional[str] = None
    # Treat warnings as errors
    warnings_as_errors: bool = False
    # Path to the pre-build hook script (relative to manifest_dir).
    # None  = auto-detect "build.nxl" in the project root.
    # ""    = explicitly disabled (never run any build script).
    # "path" = explicit path to a specific script.
    build_script: Optional[str] = None
    # Run static analyzer (nlpllint) as part of build.
    lint_on_build: bool = False
    # Use strict analyzer profile (enables style checks).
    lint_strict: bool = False
    # Emit only lint errors (suppress warnings).
    lint_errors_only: bool = False
    # Treat lint warnings as build-breaking.
    lint_fail_on_warnings: bool = False


@dataclass
class FeaturesConfig:
    """[features] section — each feature can enable a list of other features/deps."""
    definitions: Dict[str, List[str]] = field(default_factory=dict)
    default: List[str] = field(default_factory=list)  # features enabled by default


@dataclass
class ProjectConfig:
    package: PackageConfig
    build: BuildConfig
    dependencies: Dict[str, Any] = field(default_factory=dict)
    dev_dependencies: Dict[str, Any] = field(default_factory=dict)
    build_dependencies: Dict[str, Any] = field(default_factory=dict)
    features_config: FeaturesConfig = field(default_factory=FeaturesConfig)
    profiles: Dict[str, ProfileConfig] = field(default_factory=dict)
    # Absolute path to the directory that contains nlpl.toml.  Set by
    # ConfigLoader.load(); empty string means "use the current directory".
    manifest_dir: str = ""

    def get_profile(self, name: Optional[str] = None) -> ProfileConfig:
        """Return the profile config for *name* (defaults to build.profile)."""
        profile_name = name or self.build.profile
        if profile_name in self.profiles:
            return self.profiles[profile_name]
        if profile_name == "release":
            return _PROFILE_RELEASE
        return _PROFILE_DEV

    def effective_features(self) -> List[str]:
        """Resolve the full set of active features, including defaults."""
        active = set(self.features_config.default)
        active.update(self.build.features)
        # Expand transitive feature dependencies
        changed = True
        while changed:
            changed = False
            for feat in list(active):
                for dep in self.features_config.definitions.get(feat, []):
                    if dep.startswith("dep:"):
                        continue  # Dependency activation, not a feature name
                    if dep not in active:
                        active.add(dep)
                        changed = True
        return sorted(active)


class ConfigLoader:
    """Loads and validates nlpl.toml configuration."""

    @staticmethod
    def load(path: str = "nexuslang.toml") -> ProjectConfig:
        if not os.path.exists(path):
            raise FileNotFoundError(f"Configuration file not found: {path}")

        with open(path, "rb") as f:
            data = tomllib.load(f)

        # Parse [package]
        pkg_data = data.get("package", {})
        package = PackageConfig(
            name=pkg_data.get("name", "untitled"),
            version=pkg_data.get("version", "0.1.0"),
            authors=pkg_data.get("authors", []),
            description=pkg_data.get("description", "")
        )

        # Parse [build]
        build_data = data.get("build", {})
        build = BuildConfig(
            source_dir=build_data.get("source_dir", "src"),
            output_dir=build_data.get("output_dir", "build"),
            target=build_data.get("target", "c"),
            optimization=int(build_data.get("optimization", 0)),
            debug_info=bool(build_data.get("debug_info", True)),
            headers=bool(build_data.get("headers", False)),
            features=list(build_data.get("features", [])),
            profile=build_data.get("profile", "dev"),
            target_triple=build_data.get("target_triple"),
            jobs=build_data.get("jobs"),
            warnings_as_errors=bool(build_data.get("warnings_as_errors", False)),
            # None preserves auto-detect; absent key → None.  Empty string → disabled.
            build_script=build_data.get("build_script", None),
            lint_on_build=bool(build_data.get("lint_on_build", False)),
            lint_strict=bool(build_data.get("lint_strict", False)),
            lint_errors_only=bool(build_data.get("lint_errors_only", False)),
            lint_fail_on_warnings=bool(build_data.get("lint_fail_on_warnings", False)),
        )

        # Parse [features]
        features_raw = data.get("features", {})
        default_features = features_raw.pop("default", []) if isinstance(features_raw, dict) else []
        features_config = FeaturesConfig(
            definitions=dict(features_raw) if isinstance(features_raw, dict) else {},
            default=list(default_features),
        )

        # Parse [profile.NAME] sections
        profiles: Dict[str, ProfileConfig] = {}
        for profile_name, profile_data in data.get("profile", {}).items():
            inherits = profile_data.get("inherits", profile_name)
            if inherits == "release":
                base = _PROFILE_RELEASE
            else:
                base = _PROFILE_DEV
            profiles[profile_name] = ProfileConfig(
                name=profile_name,
                optimization=profile_data.get("opt-level", base.optimization),
                debug_info=profile_data.get("debug", base.debug_info),
                debug_assertions=profile_data.get("debug-assertions", base.debug_assertions),
                lto=profile_data.get("lto", base.lto),
                incremental=profile_data.get("incremental", base.incremental),
                strip=profile_data.get("strip", base.strip),
            )

        # Parse dependency sections
        dependencies = data.get("dependencies", {})
        dev_dependencies = data.get("dev-dependencies", {})
        build_dependencies = data.get("build-dependencies", {})

        return ProjectConfig(
            package=package,
            build=build,
            dependencies=dependencies,
            dev_dependencies=dev_dependencies,
            build_dependencies=build_dependencies,
            features_config=features_config,
            profiles=profiles,
            manifest_dir=os.path.dirname(os.path.abspath(path)),
        )

