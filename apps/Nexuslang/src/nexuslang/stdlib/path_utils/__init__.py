"""
Path manipulation utilities for NexusLang.
Extended path operations beyond basic filesystem module.
"""

from pathlib import Path
from typing import List, Optional
from ...runtime.runtime import Runtime


def path_join(*parts: str) -> str:
    """Join path components."""
    return str(Path(*parts))


def path_split(filepath: str) -> dict:
    """Split path into directory and filename."""
    p = Path(filepath)
    return {
        'directory': str(p.parent),
        'filename': p.name,
        'stem': p.stem,
        'extension': p.suffix
    }


def path_absolute(filepath: str) -> str:
    """Get absolute path."""
    return str(Path(filepath).resolve())


def path_relative(filepath: str, start: str) -> str:
    """Get relative path from start."""
    return str(Path(filepath).relative_to(start))


def path_normalize(filepath: str) -> str:
    """Normalize path (resolve .., ., etc.)."""
    return str(Path(filepath).resolve())


def path_exists(filepath: str) -> bool:
    """Check if path exists."""
    return Path(filepath).exists()


def path_is_file(filepath: str) -> bool:
    """Check if path is a file."""
    return Path(filepath).is_file()


def path_is_dir(filepath: str) -> bool:
    """Check if path is a directory."""
    return Path(filepath).is_dir()


def path_is_absolute(filepath: str) -> bool:
    """Check if path is absolute."""
    return Path(filepath).is_absolute()


def path_parent(filepath: str, level: int = 1) -> str:
    """Get parent directory (level=1 is immediate parent)."""
    p = Path(filepath)
    for _ in range(level):
        p = p.parent
    return str(p)


def path_parents(filepath: str) -> List[str]:
    """Get all parent directories."""
    return [str(p) for p in Path(filepath).parents]


def path_stem(filepath: str) -> str:
    """Get filename without extension."""
    return Path(filepath).stem


def path_suffix(filepath: str) -> str:
    """Get file extension (with dot)."""
    return Path(filepath).suffix


def path_suffixes(filepath: str) -> List[str]:
    """Get all file extensions (for .tar.gz etc.)."""
    return Path(filepath).suffixes


def path_with_suffix(filepath: str, suffix: str) -> str:
    """Replace file extension."""
    return str(Path(filepath).with_suffix(suffix))


def path_with_stem(filepath: str, stem: str) -> str:
    """Replace filename stem (keep extension)."""
    return str(Path(filepath).with_stem(stem))


def path_glob(directory: str, pattern: str) -> List[str]:
    """Find files matching pattern."""
    return [str(p) for p in Path(directory).glob(pattern)]


def path_rglob(directory: str, pattern: str) -> List[str]:
    """Recursively find files matching pattern."""
    return [str(p) for p in Path(directory).rglob(pattern)]


def path_match(filepath: str, pattern: str) -> bool:
    """Check if path matches pattern."""
    return Path(filepath).match(pattern)


def path_expanduser(filepath: str) -> str:
    """Expand ~ to user home directory."""
    return str(Path(filepath).expanduser())


def path_as_uri(filepath: str) -> str:
    """Convert path to file:// URI."""
    return Path(filepath).as_uri()


def register_path_functions(runtime: Runtime) -> None:
    """Register path functions with the runtime."""
    
    # Path operations
    runtime.register_function("path_join", path_join)
    runtime.register_function("path_split", path_split)
    runtime.register_function("path_absolute", path_absolute)
    runtime.register_function("path_relative", path_relative)
    runtime.register_function("path_normalize", path_normalize)
    
    # Path checks
    runtime.register_function("path_exists", path_exists)
    runtime.register_function("path_is_file", path_is_file)
    runtime.register_function("path_is_dir", path_is_dir)
    runtime.register_function("path_is_absolute", path_is_absolute)
    
    # Path components
    runtime.register_function("path_parent", path_parent)
    runtime.register_function("path_parents", path_parents)
    runtime.register_function("path_stem", path_stem)
    runtime.register_function("path_suffix", path_suffix)
    runtime.register_function("path_suffixes", path_suffixes)
    runtime.register_function("path_with_suffix", path_with_suffix)
    runtime.register_function("path_with_stem", path_with_stem)
    
    # Pattern matching
    runtime.register_function("path_glob", path_glob)
    runtime.register_function("path_rglob", path_rglob)
    runtime.register_function("path_match", path_match)
    
    # Utilities
    runtime.register_function("path_expanduser", path_expanduser)
    runtime.register_function("path_as_uri", path_as_uri)
