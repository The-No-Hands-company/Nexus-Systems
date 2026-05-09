"""
Dependency Resolution
=====================

Legacy dependency-resolution compatibility surface.

Maintains the older graph-shaped API while delegating actual resolution to the
maintained lockfile and registry tooling where possible.
"""

from dataclasses import dataclass, field
from typing import Dict, List, Set, Optional, Tuple, Any
from pathlib import Path

from .project import Dependency
from nexuslang.tooling.lockfile import (
    LockedPackage,
    resolve_git_dependency as maintained_resolve_git_dependency,
    resolve_path_dependency as maintained_resolve_path_dependency,
    resolve_registry_dependency as maintained_resolve_registry_dependency,
)


class VersionConstraint:
    """Represents a version constraint (e.g., ^1.2.3, ~2.0, >=1.0.0)."""
    
    def __init__(self, constraint: str):
        self.constraint = constraint
        self.operator, self.version = self._parse_constraint(constraint)
    
    def _parse_constraint(self, constraint: str) -> Tuple[str, Tuple[int, ...]]:
        """Parse version constraint."""
        # Handle special operators
        if constraint.startswith('^'):
            return ('^', self._parse_version(constraint[1:]))
        elif constraint.startswith('~'):
            return ('~', self._parse_version(constraint[1:]))
        elif constraint.startswith('>='):
            return ('>=', self._parse_version(constraint[2:]))
        elif constraint.startswith('<='):
            return ('<=', self._parse_version(constraint[2:]))
        elif constraint.startswith('>'):
            return ('>', self._parse_version(constraint[1:]))
        elif constraint.startswith('<'):
            return ('<', self._parse_version(constraint[1:]))
        elif constraint.startswith('='):
            return ('=', self._parse_version(constraint[1:]))
        elif constraint == '*':
            return ('*', (0, 0, 0))
        else:
            return ('=', self._parse_version(constraint))
    
    def _parse_version(self, version: str) -> Tuple[int, ...]:
        """Parse version string into tuple."""
        parts = version.split('.')
        return tuple(int(p) for p in parts)
    
    def matches(self, version: str) -> bool:
        """Check if version satisfies constraint."""
        ver = self._parse_version(version)
        
        if self.operator == '*':
            return True
        elif self.operator == '=':
            return ver == self.version
        elif self.operator == '^':
            # Compatible with version (same major version)
            if len(ver) < len(self.version):
                return False
            return ver[0] == self.version[0] and ver >= self.version
        elif self.operator == '~':
            # Approximately equivalent (same major and minor)
            if len(ver) < len(self.version):
                return False
            return ver[0] == self.version[0] and ver[1] == self.version[1] and ver >= self.version
        elif self.operator == '>=':
            return ver >= self.version
        elif self.operator == '<=':
            return ver <= self.version
        elif self.operator == '>':
            return ver > self.version
        elif self.operator == '<':
            return ver < self.version
        else:
            return False
    
    def __str__(self):
        return self.constraint


@dataclass
class ResolvedDependency:
    """A resolved dependency with a specific version."""
    name: str
    version: str
    source: str
    path: Optional[Path] = None
    git_url: Optional[str] = None
    git_commit: Optional[str] = None
    dependencies: List['ResolvedDependency'] = field(default_factory=list)


class DependencyGraph:
    """Represents the dependency graph."""
    
    def __init__(self):
        self.nodes: Dict[str, ResolvedDependency] = {}
        self.edges: Dict[str, Set[str]] = {}
    
    def add_dependency(self, dep: ResolvedDependency):
        """Add a dependency to the graph."""
        self.nodes[dep.name] = dep
        if dep.name not in self.edges:
            self.edges[dep.name] = set()
        
        for sub_dep in dep.dependencies:
            self.edges[dep.name].add(sub_dep.name)
    
    def get_build_order(self) -> List[str]:
        """Get topological sort for build order."""
        # Kahn's algorithm
        in_degree = {node: 0 for node in self.nodes}
        for node in self.edges:
            for neighbor in self.edges[node]:
                in_degree[neighbor] = in_degree.get(neighbor, 0) + 1
        
        queue = [node for node in self.nodes if in_degree[node] == 0]
        result = []
        
        while queue:
            node = queue.pop(0)
            result.append(node)
            
            for neighbor in self.edges.get(node, []):
                in_degree[neighbor] -= 1
                if in_degree[neighbor] == 0:
                    queue.append(neighbor)
        
        if len(result) != len(self.nodes):
            raise ValueError("Circular dependency detected")
        
        return result
    
    def detect_cycles(self) -> List[List[str]]:
        """Detect circular dependencies."""
        visited = set()
        rec_stack = set()
        cycles = []
        
        def dfs(node, path):
            visited.add(node)
            rec_stack.add(node)
            path.append(node)
            
            for neighbor in self.edges.get(node, []):
                if neighbor not in visited:
                    dfs(neighbor, path.copy())
                elif neighbor in rec_stack:
                    # Found a cycle
                    cycle_start = path.index(neighbor)
                    cycle = path[cycle_start:] + [neighbor]
                    cycles.append(cycle)
            
            rec_stack.remove(node)
        
        for node in self.nodes:
            if node not in visited:
                dfs(node, [])
        
        return cycles


class DependencyResolver:
    """Resolves project dependencies."""
    
    def __init__(self, package_registry: Optional[Dict[str, Dict[str, Any]]] = None, project_root: Optional[Path] = None):
        """
        Initialize dependency resolver.
        
        Args:
            package_registry: Mock registry for testing (name -> versions -> metadata)
        """
        self.package_registry = package_registry or {}
        self.project_root = Path(project_root) if project_root is not None else Path.cwd()
        self.resolved: Dict[str, ResolvedDependency] = {}

    def _dependency_to_spec(self, dep: Dependency) -> Dict[str, Any]:
        spec: Dict[str, Any] = {}
        if dep.version:
            spec["version"] = dep.version
        if dep.path:
            spec["path"] = dep.path
        if dep.git_url:
            spec["git"] = dep.git_url
        if getattr(dep, "git_rev", None):
            spec["rev"] = dep.git_rev
        elif getattr(dep, "git_tag", None):
            spec["tag"] = dep.git_tag
        elif dep.git_branch:
            spec["branch"] = dep.git_branch
        return spec

    @staticmethod
    def _resolved_dependency_from_locked(pkg: LockedPackage) -> ResolvedDependency:
        return ResolvedDependency(
            name=pkg.name,
            version=pkg.version,
            source=pkg.source,
            path=Path(pkg.resolved_path) if pkg.resolved_path else None,
            git_url=pkg.git_url,
            git_commit=pkg.git_commit,
        )
    
    def resolve(self, dependencies: Dict[str, Dependency]) -> DependencyGraph:
        """
        Resolve all dependencies.
        
        Returns:
            DependencyGraph with all resolved dependencies
        """
        graph = DependencyGraph()
        
        for name, dep in dependencies.items():
            resolved = self._resolve_dependency(dep)
            if resolved:
                graph.add_dependency(resolved)
        
        # Check for circular dependencies
        cycles = graph.detect_cycles()
        if cycles:
            raise ValueError(f"Circular dependencies detected: {cycles}")
        
        return graph
    
    def _resolve_dependency(self, dep: Dependency) -> Optional[ResolvedDependency]:
        """Resolve a single dependency."""
        # Check if already resolved
        if dep.name in self.resolved:
            return self.resolved[dep.name]
        
        # Resolve based on source
        if dep.source == "path":
            return self._resolve_path_dependency(dep)
        elif dep.source == "git":
            return self._resolve_git_dependency(dep)
        else:
            return self._resolve_registry_dependency(dep)
    
    def _resolve_path_dependency(self, dep: Dependency) -> Optional[ResolvedDependency]:
        """Resolve a local path dependency via the maintained lockfile layer."""
        try:
            locked = maintained_resolve_path_dependency(
                dep.name,
                self._dependency_to_spec(dep),
                self.project_root,
            )
        except Exception as exc:
            print(f"Error: {exc}")
            return None

        resolved = self._resolved_dependency_from_locked(locked)
        self.resolved[dep.name] = resolved
        return resolved
    
    def _resolve_git_dependency(self, dep: Dependency) -> Optional[ResolvedDependency]:
        """Resolve a git dependency via the maintained lockfile layer."""
        try:
            locked = maintained_resolve_git_dependency(
                dep.name,
                self._dependency_to_spec(dep),
                self.project_root,
            )
        except Exception as exc:
            print(f"Error: {exc}")
            return None

        resolved = self._resolved_dependency_from_locked(locked)
        self.resolved[dep.name] = resolved
        return resolved
    
    def _resolve_registry_dependency(self, dep: Dependency) -> Optional[ResolvedDependency]:
        """Resolve a registry dependency."""
        if not self.package_registry:
            try:
                locked = maintained_resolve_registry_dependency(
                    dep.name,
                    dep.version,
                    self.project_root,
                )
            except Exception as exc:
                print(f"Error: {exc}")
                return None

            resolved = self._resolved_dependency_from_locked(locked)
            self.resolved[dep.name] = resolved
            return resolved

        # Check if package exists in registry
        if dep.name not in self.package_registry:
            print(f"Error: Package '{dep.name}' not found in registry")
            return None
        
        # Find compatible version
        constraint = VersionConstraint(dep.version)
        available_versions = self.package_registry[dep.name].get('versions', {})
        
        compatible_versions = [
            v for v in available_versions.keys()
            if constraint.matches(v)
        ]
        
        if not compatible_versions:
            print(f"Error: No compatible version found for {dep.name} {dep.version}")
            return None
        
        # Pick the latest compatible version
        latest_version = max(compatible_versions, key=lambda v: tuple(map(int, v.split('.'))))
        
        resolved = ResolvedDependency(
            name=dep.name,
            version=latest_version,
            source="registry",
        )
        
        self.resolved[dep.name] = resolved
        return resolved
    
    def download_dependencies(self, graph: DependencyGraph, install_dir: Path):
        """Fail fast for deprecated dependency installation behavior."""
        install_dir.mkdir(parents=True, exist_ok=True)
        
        for dep_name in graph.get_build_order():
            dep = graph.nodes[dep_name]
            
            if dep.source == "path":
                continue
            raise NotImplementedError(
                "Legacy build_system.DependencyResolver.download_dependencies is no "
                "longer maintained. Use the maintained tooling registry and lockfile "
                "workflow for git/registry dependencies."
            )
