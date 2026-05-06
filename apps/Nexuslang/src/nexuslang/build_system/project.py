"""
Project Configuration
======================

Handles nlpl.toml project files and project structure.
"""

import os
import toml
from pathlib import Path
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Any


@dataclass
class Dependency:
    """Represents a project dependency."""
    name: str
    version: str
    source: str = "registry"  # registry, git, path
    path: Optional[str] = None
    git_url: Optional[str] = None
    git_branch: Optional[str] = None
    
    def __str__(self):
        if self.source == "path":
            return f"{self.name} (path: {self.path})"
        elif self.source == "git":
            return f"{self.name} @ {self.git_url}#{self.git_branch or 'main'}"
        else:
            return f"{self.name} {self.version}"


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
        with open(toml_path, 'r') as f:
            data = toml.load(f)
        
        # Parse package info
        package = data.get('package', {})
        config = cls(
            name=package.get('name', 'unnamed'),
            version=package.get('version', '0.1.0'),
            authors=package.get('authors', []),
            description=package.get('description', ''),
            license=package.get('license', ''),
        )
        
        # Parse build settings
        build = data.get('build', {})
        config.source_dir = build.get('source_dir', 'src')
        config.build_dir = build.get('build_dir', 'build')
        config.output_dir = build.get('output_dir', 'bin')
        
        # Parse dependencies
        deps = data.get('dependencies', {})
        for name, spec in deps.items():
            if isinstance(spec, str):
                config.dependencies[name] = Dependency(name=name, version=spec)
            elif isinstance(spec, dict):
                dep = Dependency(
                    name=name,
                    version=spec.get('version', '*'),
                    source=spec.get('source', 'registry'),
                    path=spec.get('path'),
                    git_url=spec.get('git'),
                    git_branch=spec.get('branch'),
                )
                config.dependencies[name] = dep
        
        # Parse dev dependencies
        dev_deps = data.get('dev-dependencies', {})
        for name, spec in dev_deps.items():
            if isinstance(spec, str):
                config.dev_dependencies[name] = Dependency(name=name, version=spec)
            elif isinstance(spec, dict):
                dep = Dependency(
                    name=name,
                    version=spec.get('version', '*'),
                    source=spec.get('source', 'registry'),
                    path=spec.get('path'),
                    git_url=spec.get('git'),
                    git_branch=spec.get('branch'),
                )
                config.dev_dependencies[name] = dep
        
        # Parse targets
        targets = data.get('target', {})
        for name, spec in targets.items():
            target = Target(
                name=name,
                source=spec.get('source', f'src/{name}.nxl'),
                target_type=spec.get('type', 'executable'),
                dependencies=spec.get('dependencies', []),
                optimization=spec.get('optimization', 0),
                debug=spec.get('debug', False),
            )
            config.targets[name] = target
        
        # Parse build profiles
        config.profiles = data.get('profile', {})
        
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
        
        with open(toml_path, 'w') as f:
            toml.dump(data, f)


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
    
    def __str__(self):
        return f"Project({self.config.name} v{self.config.version})"
