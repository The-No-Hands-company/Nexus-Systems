"""Incremental build cache and dependency tracking for NexusLang."""

from __future__ import annotations

import hashlib
import json
import re
import threading
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


@dataclass
class FileMetadata:
    """Cached metadata about a source file."""

    path: str
    mtime: float
    size: int
    hash: str
    imports: List[str] = field(default_factory=list)

    def has_changed(self, mtime: float, size: int) -> bool:
        return self.mtime != mtime or self.size != size

    def to_dict(self) -> dict:
        return {
            "path": self.path,
            "mtime": self.mtime,
            "size": self.size,
            "hash": self.hash,
            "imports": list(self.imports),
        }

    @classmethod
    def from_dict(cls, data: dict) -> "FileMetadata":
        return cls(
            path=data["path"],
            mtime=float(data["mtime"]),
            size=int(data["size"]),
            hash=data["hash"],
            imports=list(data.get("imports", [])),
        )


@dataclass
class BuildArtifact:
    """Recorded build output for a source/profile/features combination."""

    source_file: str
    output_file: str
    build_time: float
    profile: str
    features: List[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "source_file": self.source_file,
            "output_file": self.output_file,
            "build_time": self.build_time,
            "profile": self.profile,
            "features": list(self.features),
        }

    @classmethod
    def from_dict(cls, data: dict) -> "BuildArtifact":
        return cls(
            source_file=data["source_file"],
            output_file=data["output_file"],
            build_time=float(data["build_time"]),
            profile=data["profile"],
            features=list(data.get("features", [])),
        )


class DependencyGraph:
    """Tracks source-file dependency and dependent relationships."""

    def __init__(self) -> None:
        self._deps: Dict[str, Set[str]] = {}
        self._dependents: Dict[str, Set[str]] = {}

    def add_dependency(self, source: str, dependency: str) -> None:
        self._deps.setdefault(source, set()).add(dependency)
        self._dependents.setdefault(dependency, set()).add(source)
        self._deps.setdefault(dependency, set())
        self._dependents.setdefault(source, set())

    def set_dependencies(self, source: str, dependencies: List[str]) -> None:
        old = self._deps.get(source, set()).copy()
        for dep in old:
            depset = self._dependents.get(dep)
            if depset is not None:
                depset.discard(source)
        self._deps[source] = set()
        self._dependents.setdefault(source, set())
        for dep in dependencies:
            self.add_dependency(source, dep)

    def get_dependencies(self, source: str) -> Set[str]:
        return set(self._deps.get(source, set()))

    def get_dependents(self, dependency: str) -> Set[str]:
        return set(self._dependents.get(dependency, set()))

    def get_transitive_dependencies(self, source: str) -> Set[str]:
        if source not in self._deps:
            return set()

        visited: Set[str] = set()
        stack = list(self._deps.get(source, set()))

        while stack:
            node = stack.pop()
            if node in visited or node == source:
                continue
            visited.add(node)
            stack.extend(self._deps.get(node, set()))

        return visited

    def get_transitive_dependents(self, dependency: str) -> Set[str]:
        if dependency not in self._dependents:
            return set()

        visited: Set[str] = set()
        stack = list(self._dependents.get(dependency, set()))

        while stack:
            node = stack.pop()
            if node in visited or node == dependency:
                continue
            visited.add(node)
            stack.extend(self._dependents.get(node, set()))

        return visited

    def to_dict(self) -> dict:
        return {
            "dependencies": {k: sorted(v) for k, v in self._deps.items()},
        }

    @classmethod
    def from_dict(cls, data: dict) -> "DependencyGraph":
        g = cls()
        deps = data.get("dependencies", {})
        for source, dep_list in deps.items():
            g.set_dependencies(source, list(dep_list))
        return g


def extract_imports_from_source(source: str) -> List[str]:
    """Extract top-level import names from source text."""
    imports: List[str] = []
    pattern = re.compile(r"^\s*import\s+([A-Za-z_][A-Za-z0-9_\.]*)\s*$")
    for line in source.splitlines():
        match = pattern.match(line)
        if match:
            imports.append(match.group(1))
    return imports


class BuildCache:
    """JSON-backed incremental build cache."""

    CACHE_VERSION = 1

    def __init__(self, cache_dir: Path):
        self.cache_dir = Path(cache_dir)
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.cache_file = self.cache_dir / "build_cache.json"

        self.file_metadata: Dict[str, FileMetadata] = {}
        self.build_artifacts: Dict[str, BuildArtifact] = {}
        self.dependency_graph = DependencyGraph()
        self._lock = threading.RLock()

        self._load()

    @staticmethod
    def _compute_hash(path: Path) -> str:
        digest = hashlib.sha256()
        with open(path, "rb") as handle:
            for chunk in iter(lambda: handle.read(8192), b""):
                digest.update(chunk)
        return digest.hexdigest()

    @staticmethod
    def _artifact_key(source: Path, profile: str, features: Optional[List[str]]) -> str:
        features = list(features or [])
        return f"{source}:{profile}:{','.join(features)}"

    def _load(self) -> None:
        if not self.cache_file.exists():
            return

        try:
            raw = self.cache_file.read_text(encoding="utf-8").strip()
            if not raw:
                return
            data = json.loads(raw)
        except Exception:
            return

        try:
            self.file_metadata = {
                p: FileMetadata.from_dict(meta)
                for p, meta in data.get("file_metadata", {}).items()
            }
            self.build_artifacts = {
                k: BuildArtifact.from_dict(artifact)
                for k, artifact in data.get("build_artifacts", {}).items()
            }
            self.dependency_graph = DependencyGraph.from_dict(
                data.get("dependency_graph", {})
            )
        except Exception:
            self.file_metadata = {}
            self.build_artifacts = {}
            self.dependency_graph = DependencyGraph()

    def save(self) -> None:
        payload = {
            "version": self.CACHE_VERSION,
            "file_metadata": {k: v.to_dict() for k, v in self.file_metadata.items()},
            "dependency_graph": self.dependency_graph.to_dict(),
            "build_artifacts": {k: v.to_dict() for k, v in self.build_artifacts.items()},
        }
        with self._lock:
            self.cache_file.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    def update_file_metadata(self, source_file: Path, imports: Optional[List[str]] = None) -> None:
        source_file = Path(source_file)
        stat = source_file.stat()
        source_str = str(source_file)
        with self._lock:
            self.file_metadata[source_str] = FileMetadata(
                path=source_str,
                mtime=stat.st_mtime,
                size=stat.st_size,
                hash=self._compute_hash(source_file),
                imports=list(imports or []),
            )
            if imports is not None:
                self.dependency_graph.set_dependencies(source_str, list(imports))

    def is_file_changed(self, source_file: Path) -> bool:
        source_file = Path(source_file)
        source_str = str(source_file)
        if not source_file.exists():
            return True

        with self._lock:
            meta = self.file_metadata.get(source_str)
        if meta is None:
            return True

        stat = source_file.stat()
        return meta.has_changed(stat.st_mtime, stat.st_size)

    def is_file_hash_changed(self, source_file: Path) -> bool:
        source_file = Path(source_file)
        source_str = str(source_file)
        if not source_file.exists():
            return True

        with self._lock:
            meta = self.file_metadata.get(source_str)
        if meta is None:
            return True

        return self._compute_hash(source_file) != meta.hash

    def record_build(
        self,
        source_file: Path,
        output_file: Path,
        profile: str,
        features: Optional[List[str]] = None,
    ) -> None:
        source_file = Path(source_file)
        output_file = Path(output_file)
        key = self._artifact_key(source_file, profile, features)
        artifact = BuildArtifact(
            source_file=str(source_file),
            output_file=str(output_file),
            build_time=output_file.stat().st_mtime if output_file.exists() else 0.0,
            profile=profile,
            features=list(features or []),
        )
        with self._lock:
            self.build_artifacts[key] = artifact

    def needs_rebuild(
        self,
        source_file: Path,
        profile: str = "dev",
        features: Optional[List[str]] = None,
    ) -> Tuple[bool, str]:
        source_file = Path(source_file)
        if self.is_file_changed(source_file):
            return True, "Source changed or missing metadata"

        key = self._artifact_key(source_file, profile, features)
        with self._lock:
            artifact = self.build_artifacts.get(key)

        if artifact is None:
            return True, "No previous build artifact for profile/features"

        output = Path(artifact.output_file)
        if not output.exists():
            return True, "Output artifact is missing"

        source_str = str(source_file)
        with self._lock:
            deps = self.dependency_graph.get_dependencies(source_str)

        for dep in deps:
            dep_path = Path(dep)
            if self.is_file_changed(dep_path) or self.is_file_hash_changed(dep_path):
                return True, f"Dependency changed: {dep}"

        return False, "Up to date"

    def get_files_to_rebuild(
        self,
        source_files: List[Path],
        profile: str = "dev",
        features: Optional[List[str]] = None,
    ) -> List[Tuple[Path, str]]:
        result: List[Tuple[Path, str]] = []
        for source in source_files:
            needs, reason = self.needs_rebuild(source, profile=profile, features=features)
            if needs:
                result.append((Path(source), reason))
        return result

    def invalidate_file(self, source_file: Path) -> Set[Path]:
        source_file = Path(source_file)
        source_str = str(source_file)
        with self._lock:
            invalidated = {source_str}
            invalidated.update(self.dependency_graph.get_transitive_dependents(source_str))

            for file_str in invalidated:
                for key in list(self.build_artifacts.keys()):
                    if key.startswith(f"{file_str}:"):
                        self.build_artifacts.pop(key, None)

        return {Path(p) for p in invalidated}

    def clear(self) -> None:
        with self._lock:
            self.file_metadata.clear()
            self.build_artifacts.clear()
            self.dependency_graph = DependencyGraph()
            if self.cache_file.exists():
                self.cache_file.unlink()
