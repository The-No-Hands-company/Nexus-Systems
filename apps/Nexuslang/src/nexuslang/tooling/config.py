
import os
from dataclasses import dataclass, field
from typing import List, Optional, Dict, Any, Union

from nexuslang.build.manifest import Dependency as ManifestDependency
from nexuslang.build.manifest import Manifest, load_manifest_data

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
    optimization: Union[int, str] = 0       # 0-3 or size-focused symbolic levels
    debug_info: bool = True
    debug_assertions: bool = True
    lto: Union[bool, str] = False            # Link-time optimisation
    incremental: bool = True
    strip: Union[bool, str] = False          # Strip symbols on release


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


def _dependency_to_spec(dep: ManifestDependency) -> Any:
    """Convert canonical manifest dependency model to tooling config shape."""
    is_plain_version = (
        dep.version_req is not None
        and dep.path is None
        and dep.git is None
        and dep.branch is None
        and dep.tag is None
        and dep.rev is None
        and not dep.features
        and dep.default_features
        and not dep.optional
    )
    if is_plain_version:
        return dep.version_req

    spec: Dict[str, Any] = {}
    if dep.version_req is not None:
        spec["version"] = dep.version_req
    if dep.path is not None:
        spec["path"] = dep.path
    if dep.git is not None:
        spec["git"] = dep.git
    if dep.branch is not None:
        spec["branch"] = dep.branch
    if dep.tag is not None:
        spec["tag"] = dep.tag
    if dep.rev is not None:
        spec["rev"] = dep.rev
    if dep.features:
        spec["features"] = list(dep.features)
    if not dep.default_features:
        spec["default-features"] = False
    if dep.optional:
        spec["optional"] = True

    return spec


class ConfigLoader:
    """Loads and validates nlpl.toml configuration."""

    @staticmethod
    def _expect_table(value: Any, section: str) -> Dict[str, Any]:
        if not isinstance(value, dict):
            raise ValueError(f"Section '{section}' must be a table")
        return value

    @staticmethod
    def _expect_string(value: Any, field: str) -> str:
        if not isinstance(value, str):
            raise ValueError(f"Invalid field '{field}': expected string")
        return value

    @staticmethod
    def _expect_optional_string(value: Any, field: str) -> Optional[str]:
        if value is None:
            return None
        if not isinstance(value, str):
            raise ValueError(f"Invalid field '{field}': expected string or null")
        return value

    @staticmethod
    def _expect_bool(value: Any, field: str) -> bool:
        if not isinstance(value, bool):
            raise ValueError(f"Invalid field '{field}': expected boolean")
        return value

    @staticmethod
    def _expect_int(value: Any, field: str, *, min_value: Optional[int] = None, max_value: Optional[int] = None) -> int:
        if isinstance(value, bool) or not isinstance(value, int):
            raise ValueError(f"Invalid field '{field}': expected integer")
        if min_value is not None and value < min_value:
            raise ValueError(f"Invalid field '{field}': expected integer >= {min_value}")
        if max_value is not None and value > max_value:
            raise ValueError(f"Invalid field '{field}': expected integer <= {max_value}")
        return value

    @staticmethod
    def _expect_string_list(value: Any, field: str) -> List[str]:
        if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
            raise ValueError(f"Invalid field '{field}': expected list of strings")
        return list(value)

    @staticmethod
    def load(path: str = "nexuslang.toml") -> ProjectConfig:
        if not os.path.exists(path):
            raise FileNotFoundError(f"Configuration file not found: {path}")

        data = load_manifest_data(path)
        manifest = Manifest.from_data(data, path=path)

        # Parse canonical manifest sections via nexuslang.build.manifest.
        if manifest.package is not None:
            package = PackageConfig(
                name=manifest.package.name,
                version=manifest.package.version,
                authors=list(manifest.package.authors),
                description=manifest.package.description or "",
            )
        else:
            # Workspace-only manifests can omit [package].
            pkg_data = data.get("package", {})
            package = PackageConfig(
                name=pkg_data.get("name", "untitled"),
                version=pkg_data.get("version", "0.1.0"),
                authors=list(pkg_data.get("authors", [])),
                description=pkg_data.get("description", ""),
            )

        # Parse [build]
        build_data = ConfigLoader._expect_table(data.get("build", {}), "build")

        source_dir = ConfigLoader._expect_string(build_data.get("source_dir", "src"), "build.source_dir")
        output_dir = ConfigLoader._expect_string(build_data.get("output_dir", "build"), "build.output_dir")
        target = ConfigLoader._expect_string(build_data.get("target", "c"), "build.target")
        optimization = ConfigLoader._expect_int(
            build_data.get("optimization", 0),
            "build.optimization",
            min_value=0,
            max_value=3,
        )
        debug_info = ConfigLoader._expect_bool(build_data.get("debug_info", True), "build.debug_info")
        headers = ConfigLoader._expect_bool(build_data.get("headers", False), "build.headers")
        features = ConfigLoader._expect_string_list(build_data.get("features", []), "build.features")
        profile = ConfigLoader._expect_string(build_data.get("profile", "dev"), "build.profile")
        target_triple = ConfigLoader._expect_optional_string(
            build_data.get("target_triple"),
            "build.target_triple",
        )

        jobs_raw = build_data.get("jobs")
        if jobs_raw is None:
            jobs = None
        else:
            jobs = ConfigLoader._expect_int(jobs_raw, "build.jobs", min_value=1)

        warnings_as_errors = ConfigLoader._expect_bool(
            build_data.get("warnings_as_errors", False),
            "build.warnings_as_errors",
        )
        build_script = ConfigLoader._expect_optional_string(
            build_data.get("build_script", None),
            "build.build_script",
        )
        lint_on_build = ConfigLoader._expect_bool(
            build_data.get("lint_on_build", False),
            "build.lint_on_build",
        )
        lint_strict = ConfigLoader._expect_bool(
            build_data.get("lint_strict", False),
            "build.lint_strict",
        )
        lint_errors_only = ConfigLoader._expect_bool(
            build_data.get("lint_errors_only", False),
            "build.lint_errors_only",
        )
        lint_fail_on_warnings = ConfigLoader._expect_bool(
            build_data.get("lint_fail_on_warnings", False),
            "build.lint_fail_on_warnings",
        )

        parallel_jobs = ConfigLoader._expect_int(
            build_data.get("parallel_jobs", 0),
            "build.parallel_jobs",
            min_value=0,
        )
        lto = ConfigLoader._expect_string(build_data.get("lto", "disabled"), "build.lto")
        if lto not in {"disabled", "thin", "full"}:
            raise ValueError("Invalid field 'build.lto': expected one of ['disabled', 'full', 'thin']")
        sysroot = ConfigLoader._expect_optional_string(
            build_data.get("sysroot"),
            "build.sysroot",
        )

        build = BuildConfig(
            source_dir=source_dir,
            output_dir=output_dir,
            target=target,
            optimization=optimization,
            debug_info=debug_info,
            headers=headers,
            features=features,
            profile=profile,
            target_triple=target_triple,
            jobs=jobs,
            parallel_jobs=parallel_jobs,
            lto=lto,
            sysroot=sysroot,
            warnings_as_errors=warnings_as_errors,
            # None preserves auto-detect; absent key → None.  Empty string → disabled.
            build_script=build_script,
            lint_on_build=lint_on_build,
            lint_strict=lint_strict,
            lint_errors_only=lint_errors_only,
            lint_fail_on_warnings=lint_fail_on_warnings,
        )

        features_raw = dict(manifest.features)
        default_features = features_raw.pop("default", [])
        features_config = FeaturesConfig(
            definitions=features_raw,
            default=list(default_features),
        )

        # Keep only user-defined profiles in config.profiles.
        profiles: Dict[str, ProfileConfig] = {}
        for profile_name in data.get("profile", {}):
            parsed = manifest.profiles.get(profile_name)
            if parsed is None:
                continue
            profiles[profile_name] = ProfileConfig(
                name=profile_name,
                optimization=parsed.opt_level,
                debug_info=parsed.debug,
                debug_assertions=parsed.debug_assertions,
                lto=parsed.lto,
                incremental=parsed.incremental,
                strip=parsed.strip,
            )

        dependencies = {
            name: _dependency_to_spec(dep)
            for name, dep in manifest.dependencies.items()
        }
        dev_dependencies = {
            name: _dependency_to_spec(dep)
            for name, dep in manifest.dev_dependencies.items()
        }
        build_dependencies = {
            name: _dependency_to_spec(dep)
            for name, dep in manifest.build_dependencies.items()
        }

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

