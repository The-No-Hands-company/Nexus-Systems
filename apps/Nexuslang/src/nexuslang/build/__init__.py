"""nexuslang.build — build pipeline helpers for NexusLang.

Submodules
----------
lto       Link-Time Optimisation driver (ThinLTO / Full LTO via llvm-lto or lld).
manifest  Project manifest parser (nexuslang.toml).
"""

from .lto import LTOConfig, LTOLinker, LTOMode, LTOResult
from .manifest import (
    Manifest,
    load_manifest,
    Dependency,
    BuildProfile,
    PanicStrategy,
    CrateType,
    PackageMetadata,
    BinaryTarget,
    LibraryTarget,
    WorkspaceConfig,
)
from .parallel import CompilationTask, TaskResult, ParallelCompiler, build_tasks_from_sources
from .cross import (
    TargetArch,
    TargetOS,
    TargetABI,
    TargetTriple,
    ToolchainInfo,
    ToolchainDetector,
    CrossCompiler,
    CrossCompileResult,
    KNOWN_TARGETS,
    get_cross_compiler,
)
from .project import BuildConfig as BuildProjectConfig, Project
from .builder import Builder
from .incremental import (
    FileMetadata,
    BuildArtifact,
    DependencyGraph,
    BuildCache,
    extract_imports_from_source,
)

__all__ = [
    "LTOConfig", "LTOLinker", "LTOMode", "LTOResult",
    "Manifest", "load_manifest", "Dependency", "BuildProfile",
    "PanicStrategy", "CrateType", "PackageMetadata",
    "BinaryTarget", "LibraryTarget", "WorkspaceConfig",
    "CompilationTask", "TaskResult", "ParallelCompiler", "build_tasks_from_sources",
    "TargetArch", "TargetOS", "TargetABI", "TargetTriple",
    "ToolchainInfo", "ToolchainDetector", "CrossCompiler", "CrossCompileResult",
    "KNOWN_TARGETS", "get_cross_compiler",
    "BuildProjectConfig", "Project", "Builder",
    "FileMetadata", "BuildArtifact", "DependencyGraph", "BuildCache",
    "extract_imports_from_source",
]
