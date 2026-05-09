"""
Project Configuration
======================

Handles nlpl.toml project files and project structure.
"""

import os
from pathlib import Path
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Any

from nexuslang.build.manifest import Manifest
from nexuslang.tooling import dependency_manager as maintained_dependency_manager
from nexuslang.tooling.config import ConfigLoader

try:
    import tomllib as _toml_loader
except ImportError:
    try:
        import tomli as _toml_loader
    except ImportError:
        _toml_loader = None

try:
    import toml as _toml_writer
except ImportError:
    _toml_writer = None


def _toml_load(file_obj):
    """Load TOML using available stdlib or third-party parser."""
    if _toml_writer is not None:
        content = file_obj.read()
        if isinstance(content, bytes):
            content = content.decode('utf-8')
        return _toml_writer.loads(content)
    if _toml_loader is None:
        raise ModuleNotFoundError(
            "TOML support requires Python 3.11+ or an installed tomli/toml package"
        )
    return _toml_loader.load(file_obj)


def _format_toml_value(value: Any) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return str(value)
    if isinstance(value, list):
        return "[" + ", ".join(_format_toml_value(item) for item in value) + "]"
    escaped = str(value).replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def _write_toml_table(lines: List[str], prefix: List[str], values: Dict[str, Any]) -> None:
    scalar_items = []
    nested_items = []
    for key, value in values.items():
        if isinstance(value, dict):
            nested_items.append((key, value))
        else:
            scalar_items.append((key, value))

    if prefix:
        lines.append(f"[{'/'.join(prefix).replace('/', '.')}]")
    for key, value in scalar_items:
        lines.append(f"{key} = {_format_toml_value(value)}")
    if prefix and (scalar_items or nested_items):
        lines.append("")
    for key, value in nested_items:
        _write_toml_table(lines, [*prefix, key], value)


def _toml_dump(data: Dict[str, Any], file_obj) -> None:
    """Write TOML without requiring the third-party toml package."""
    if _toml_writer is not None:
        _toml_writer.dump(data, file_obj)
        return

    lines: List[str] = []
    for key, value in data.items():
        if isinstance(value, dict):
            _write_toml_table(lines, [key], value)
        else:
            lines.append(f"{key} = {_format_toml_value(value)}")
    file_obj.write("\n".join(lines).rstrip() + "\n")


@dataclass
class Dependency:
    """Represents a project dependency."""
    name: str
    version: str
    source: str = "registry"  # registry, git, path
    path: Optional[str] = None
    git_url: Optional[str] = None
    git_branch: Optional[str] = None
    git_tag: Optional[str] = None
    git_rev: Optional[str] = None
    
    def __str__(self):
        if self.source == "path":
            return f"{self.name} (path: {self.path})"
        elif self.source == "git":
            git_ref = self.git_rev or self.git_tag or self.git_branch or 'main'
            return f"{self.name} @ {self.git_url}#{git_ref}"
        else:
            return f"{self.name} {self.version}"


def _dependency_from_spec(name: str, spec: Any) -> Dependency:
    """Convert maintained manifest/config dependency specs to legacy objects."""
    if isinstance(spec, str):
        return Dependency(name=name, version=spec)

    if isinstance(spec, dict):
        if "path" in spec:
            source = "path"
        elif "git" in spec:
            source = "git"
        else:
            source = "registry"

        return Dependency(
            name=name,
            version=spec.get("version", "*"),
            source=source,
            path=spec.get("path"),
            git_url=spec.get("git"),
            git_branch=spec.get("branch"),
            git_tag=spec.get("tag"),
            git_rev=spec.get("rev"),
        )

    raise ValueError(f"Invalid dependency specification for {name!r}")


def _load_legacy_targets(toml_path: Path, source_dir: str, package_name: str) -> Dict[str, "Target"]:
    """Adapt maintained manifest target shapes to the legacy target map."""
    with open(toml_path, 'rb') as f:
        data = _toml_load(f)

    targets: Dict[str, Target] = {}
    raw_targets = data.get('target', {})
    for name, spec in raw_targets.items():
        targets[name] = Target(
            name=name,
            source=spec.get('source', f'src/{name}.nxl'),
            target_type=spec.get('type', 'executable'),
            dependencies=spec.get('dependencies', []),
            optimization=spec.get('optimization', 0),
            debug=spec.get('debug', False),
        )

    if targets:
        return targets

    manifest = Manifest(toml_path)
    for binary in manifest.binary_targets:
        targets[binary.name] = Target(
            name=binary.name,
            source=binary.path,
            target_type='executable',
        )

    if manifest.library_target is not None:
        library_name = manifest.library_target.name or package_name
        targets[library_name] = Target(
            name=library_name,
            source=manifest.library_target.path,
            target_type='library',
        )

    default_main = f'{source_dir}/main.nxl'
    if not targets and (toml_path.parent / default_main).exists():
        targets['main'] = Target(
            name='main',
            source=default_main,
            target_type='executable',
        )

    return targets


@dataclass
class Target:
    """Represents a build target."""
    name: str
    source: str
    target_type: str = "executable"  # executable, library, module
    dependencies: List[str] = field(default_factory=list)
    optimization: int = 0
    debug: bool = False
    
    def __str__(self):
        return f"{self.name} ({self.target_type})"


@dataclass
class ProjectConfig:
    """Project configuration from nexuslang.toml."""
    name: str
    version: str
    authors: List[str] = field(default_factory=list)
    description: str = ""
    license: str = ""
    
    # Build settings
    source_dir: str = "src"
    build_dir: str = "build"
    output_dir: str = "bin"
    
    # Dependencies
    dependencies: Dict[str, Dependency] = field(default_factory=dict)
    dev_dependencies: Dict[str, Dependency] = field(default_factory=dict)
    
    # Targets
    targets: Dict[str, Target] = field(default_factory=dict)
    
    # Build profiles
    profiles: Dict[str, Dict[str, Any]] = field(default_factory=dict)
    
    @classmethod
    def from_toml(cls, toml_path: Path) -> 'ProjectConfig':
        """Load project config from nexuslang.toml."""
        maintained_config = ConfigLoader.load(str(toml_path))
        with open(toml_path, 'rb') as f:
            data = _toml_load(f)

        build = data.get('build', {})
        config = cls(
            name=maintained_config.package.name,
            version=maintained_config.package.version,
            authors=maintained_config.package.authors,
            description=maintained_config.package.description,
            license=data.get('package', {}).get('license', ''),
            source_dir=maintained_config.build.source_dir,
            build_dir=build.get('build_dir', 'build'),
            output_dir=maintained_config.build.output_dir,
            dependencies={
                name: _dependency_from_spec(name, spec)
                for name, spec in maintained_config.dependencies.items()
            },
            dev_dependencies={
                name: _dependency_from_spec(name, spec)
                for name, spec in maintained_config.dev_dependencies.items()
            },
            profiles=data.get('profile', {}),
        )
        config.targets = _load_legacy_targets(
            toml_path,
            source_dir=config.source_dir,
            package_name=config.name,
        )
        return config
    
    def to_toml(self, toml_path: Path):
        """Save project config to nlpl.toml."""
        data = {
            'package': {
                'name': self.name,
                'version': self.version,
                'authors': self.authors,
                'description': self.description,
                'license': self.license,
            },
            'build': {
                'source_dir': self.source_dir,
                'build_dir': self.build_dir,
                'output_dir': self.output_dir,
            },
        }
        
        # Add dependencies
        if self.dependencies:
            data['dependencies'] = {}
            for name, dep in self.dependencies.items():
                if dep.source == "registry":
                    data['dependencies'][name] = dep.version
                else:
                    dep_data = {'version': dep.version, 'source': dep.source}
                    if dep.path:
                        dep_data['path'] = dep.path
                    if dep.git_url:
                        dep_data['git'] = dep.git_url
                    if dep.git_branch:
                        dep_data['branch'] = dep.git_branch
                    if dep.git_tag:
                        dep_data['tag'] = dep.git_tag
                    if dep.git_rev:
                        dep_data['rev'] = dep.git_rev
                    data['dependencies'][name] = dep_data
        
        # Add dev dependencies
        if self.dev_dependencies:
            data['dev-dependencies'] = {}
            for name, dep in self.dev_dependencies.items():
                if dep.source == "registry":
                    data['dev-dependencies'][name] = dep.version
                else:
                    dep_data = {'version': dep.version, 'source': dep.source}
                    if dep.path:
                        dep_data['path'] = dep.path
                    if dep.git_url:
                        dep_data['git'] = dep.git_url
                    if dep.git_branch:
                        dep_data['branch'] = dep.git_branch
                    if dep.git_tag:
                        dep_data['tag'] = dep.git_tag
                    if dep.git_rev:
                        dep_data['rev'] = dep.git_rev
                    data['dev-dependencies'][name] = dep_data
        
        # Add targets
        if self.targets:
            data['target'] = {}
            for name, target in self.targets.items():
                data['target'][name] = {
                    'source': target.source,
                    'type': target.target_type,
                    'dependencies': target.dependencies,
                    'optimization': target.optimization,
                    'debug': target.debug,
                }
        
        # Add profiles
        if self.profiles:
            data['profile'] = self.profiles
        
        with open(toml_path, 'w', encoding='utf-8') as f:
            _toml_dump(data, f)


class Project:
    """Represents an NexusLang project."""
    
    def __init__(self, root_path: Path):
        self.root_path = root_path
        self.config_path = root_path / "nexuslang.toml"
        
        if self.config_path.exists():
            self.config = ProjectConfig.from_toml(self.config_path)
        else:
            raise FileNotFoundError(f"No nlpl.toml found in {root_path}")
    
    @classmethod
    def init(cls, root_path: Path, name: str, **kwargs) -> 'Project':
        """Initialize a new NexusLang project."""
        root_path.mkdir(parents=True, exist_ok=True)
        
        # Create project structure
        (root_path / "src").mkdir(exist_ok=True)
        (root_path / "build").mkdir(exist_ok=True)
        (root_path / "bin").mkdir(exist_ok=True)
        (root_path / "tests").mkdir(exist_ok=True)
        
        # Create default config
        config = ProjectConfig(
            name=name,
            version=kwargs.get('version', '0.1.0'),
            authors=kwargs.get('authors', []),
            description=kwargs.get('description', ''),
            license=kwargs.get('license', 'MIT'),
        )
        
        # Add default main target
        config.targets['main'] = Target(
            name='main',
            source='src/main.nxl',
            target_type='executable',
        )
        
        # Add default build profiles
        config.profiles = {
            'dev': {
                'optimization': 0,
                'debug': True,
            },
            'release': {
                'optimization': 3,
                'debug': False,
            },
        }
        
        # Save config
        config.to_toml(root_path / "nexuslang.toml")
        
        # Create main.nlpl
        main_path = root_path / "src" / "main.nxl"
        with open(main_path, 'w') as f:
            f.write(f'''# {name}
# Created by NexusLang Build System

print text "Hello from {name}!"
''')
        
        # Create .gitignore
        gitignore_path = root_path / ".gitignore"
        with open(gitignore_path, 'w') as f:
            f.write('''# NexusLang build artifacts
build/
bin/
*.o
*.ll
*.bc
*.so
*.a
*.exe

# Dependencies
.nlpl/

# IDE
.vscode/
.idea/
*.swp
*.swo
*~

# OS
.DS_Store
Thumbs.db
''')
        
        return cls(root_path)
    
    def get_source_dir(self) -> Path:
        """Get absolute path to source directory."""
        return self.root_path / self.config.source_dir
    
    def get_build_dir(self) -> Path:
        """Get absolute path to build directory."""
        return self.root_path / self.config.build_dir
    
    def get_output_dir(self) -> Path:
        """Get absolute path to output directory."""
        return self.root_path / self.config.output_dir
    
    def get_target(self, name: str) -> Optional[Target]:
        """Get target by name."""
        return self.config.targets.get(name)
    
    def get_profile(self, name: str) -> Dict[str, Any]:
        """Get build profile by name."""
        return self.config.profiles.get(name, {})

    def add_dependency(self, package_spec: str, **kwargs) -> None:
        """Compatibility wrapper over the maintained dependency manager."""
        maintained_dependency_manager.add_dependency(self.root_path, package_spec, **kwargs)
        self.config = ProjectConfig.from_toml(self.config_path)

    def remove_dependency(self, package_name: str, *, dev: bool = False) -> None:
        """Compatibility wrapper over the maintained dependency manager."""
        maintained_dependency_manager.remove_dependency(self.root_path, package_name, dev=dev)
        self.config = ProjectConfig.from_toml(self.config_path)

    def update_lockfile(self, *, offline: bool = False) -> None:
        """Compatibility wrapper over the maintained dependency manager."""
        maintained_dependency_manager.update_lockfile(self.root_path, offline=offline)

    def list_dependencies(self) -> None:
        """Compatibility wrapper over the maintained dependency manager."""
        maintained_dependency_manager.list_dependencies(self.root_path)
    
    def __str__(self):
        return f"Project({self.config.name} v{self.config.version})"
