"""
NLPL Build System
==================

Modern package management and build system for NexusLang.

Features:
- Project configuration (nlpl.toml)
- Dependency management
- Build configurations (debug/release)
- Incremental compilation
- Build caching
- Multiple targets

Usage:
    nlplbuild init                 # Initialize new project
    nlplbuild build                # Build project
    nlplbuild run                  # Build and run
    nlplbuild clean                # Clean build artifacts
    nlplbuild test                 # Run tests
"""

from .project import Project, ProjectConfig, Target, Dependency
from .builder import Builder, BuildConfig, BuildCache
from .dependency_resolver import DependencyResolver, DependencyGraph

__all__ = [
    'Project',
    'ProjectConfig',
    'Target',
    'Dependency',
    'Builder',
    'BuildConfig',
    'BuildCache',
    'DependencyResolver',
    'DependencyGraph',
]
