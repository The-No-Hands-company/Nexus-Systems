"""
NLPL Standard Library - File I/O Module
Comprehensive file and directory operations
"""

import os
import shutil
import tempfile
from pathlib import Path
from typing import Any, Dict, List


def register_file_io_functions(runtime: Any) -> None:
    """Register file I/O functions with the runtime."""
    
    # File reading
    runtime.register_function("read_file", read_file)
    runtime.register_function("read_text", read_file)  # Alias
    runtime.register_function("read_lines", read_lines)
    runtime.register_function("read_bytes", read_bytes)
    
    # File writing
    runtime.register_function("write_file", write_file)
    runtime.register_function("write_text", write_file)  # Alias
    runtime.register_function("append_file", append_file)
    runtime.register_function("write_lines", write_lines)
    runtime.register_function("write_bytes", write_bytes)
    
    # File operations
    runtime.register_function("file_exists", file_exists)
    runtime.register_function("file_size", file_size)
    runtime.register_function("delete_file", delete_file)
    runtime.register_function("remove_file", delete_file)  # Alias
    runtime.register_function("copy_file", copy_file)
    runtime.register_function("move_file", move_file)
    runtime.register_function("rename_file", rename_file)
    
    # Directory operations
    runtime.register_function("create_directory", create_directory)
    runtime.register_function("mkdir", create_directory)  # Alias
    runtime.register_function("create_directories", create_directories)
    runtime.register_function("mkdirs", create_directories)  # Alias
    runtime.register_function("directory_exists", directory_exists)
    runtime.register_function("list_directory", list_directory)
    runtime.register_function("listdir", list_directory)  # Alias
    runtime.register_function("walk_directory", walk_directory)
    runtime.register_function("delete_directory", delete_directory)
    runtime.register_function("rmdir", delete_directory)  # Alias
    runtime.register_function("remove_directory_recursive", remove_directory_recursive)
    
    # Temporary files
    runtime.register_function("create_temp_file", create_temp_file)
    runtime.register_function("create_temp_directory", create_temp_directory)
    
    # File info
    runtime.register_function("get_file_info", get_file_info)
    runtime.register_function("is_file", is_file)
    runtime.register_function("is_directory", is_directory)
    runtime.register_function("is_symlink", is_symlink)


# =======================
# File Reading
# =======================

def read_file(filepath: str, encoding: str = "utf-8") -> str:
    """Read entire file contents as text."""
    try:
        with open(filepath, 'r', encoding=encoding) as f:
            return f.read()
    except FileNotFoundError:
        raise RuntimeError(f"File not found: {filepath}")
    except PermissionError:
        raise RuntimeError(f"Permission denied: {filepath}")
    except UnicodeDecodeError:
        raise RuntimeError(f"Cannot decode file as {encoding}: {filepath}")
    except Exception as e:
        raise RuntimeError(f"Error reading file {filepath}: {e}")


def read_lines(filepath: str, encoding: str = "utf-8") -> List[str]:
    """Read file as list of lines (with newlines stripped)."""
    try:
        with open(filepath, 'r', encoding=encoding) as f:
            return [line.rstrip('\n\r') for line in f]
    except FileNotFoundError:
        raise RuntimeError(f"File not found: {filepath}")
    except Exception as e:
        raise RuntimeError(f"Error reading file {filepath}: {e}")


def read_bytes(filepath: str) -> bytes:
    """Read file as raw bytes."""
    try:
        with open(filepath, 'rb') as f:
            return f.read()
    except FileNotFoundError:
        raise RuntimeError(f"File not found: {filepath}")
    except Exception as e:
        raise RuntimeError(f"Error reading file {filepath}: {e}")


# =======================
# File Writing
# =======================

def write_file(filepath: str, content: str, encoding: str = "utf-8") -> bool:
    """Write text content to file (overwrites existing)."""
    try:
        # Create parent directories if needed
        parent = Path(filepath).parent
        if parent and not parent.exists():
            parent.mkdir(parents=True, exist_ok=True)
        
        with open(filepath, 'w', encoding=encoding) as f:
            f.write(content)
        return True
    except PermissionError:
        raise RuntimeError(f"Permission denied: {filepath}")
    except Exception as e:
        raise RuntimeError(f"Error writing file {filepath}: {e}")


def append_file(filepath: str, content: str, encoding: str = "utf-8") -> bool:
    """Append text content to file."""
    try:
        with open(filepath, 'a', encoding=encoding) as f:
            f.write(content)
        return True
    except Exception as e:
        raise RuntimeError(f"Error appending to file {filepath}: {e}")


def write_lines(filepath: str, lines: List[str], encoding: str = "utf-8") -> bool:
    """Write list of lines to file (adds newlines)."""
    try:
        parent = Path(filepath).parent
        if parent and not parent.exists():
            parent.mkdir(parents=True, exist_ok=True)
        
        with open(filepath, 'w', encoding=encoding) as f:
            for line in lines:
                f.write(str(line) + '\n')
        return True
    except Exception as e:
        raise RuntimeError(f"Error writing lines to {filepath}: {e}")


def write_bytes(filepath: str, data: bytes) -> bool:
    """Write raw bytes to file."""
    try:
        parent = Path(filepath).parent
        if parent and not parent.exists():
            parent.mkdir(parents=True, exist_ok=True)
        
        with open(filepath, 'wb') as f:
            f.write(data)
        return True
    except Exception as e:
        raise RuntimeError(f"Error writing bytes to {filepath}: {e}")


# =======================
# File Operations
# =======================

def file_exists(filepath: str) -> bool:
    """Check if file exists."""
    return os.path.isfile(filepath)


def file_size(filepath: str) -> int:
    """Get file size in bytes."""
    try:
        return os.path.getsize(filepath)
    except FileNotFoundError:
        raise RuntimeError(f"File not found: {filepath}")
    except Exception as e:
        raise RuntimeError(f"Error getting file size {filepath}: {e}")


def delete_file(filepath: str) -> bool:
    """Delete a file."""
    try:
        os.remove(filepath)
        return True
    except FileNotFoundError:
        return False
    except Exception as e:
        raise RuntimeError(f"Error deleting file {filepath}: {e}")


def copy_file(source: str, destination: str) -> bool:
    """Copy file from source to destination."""
    try:
        shutil.copy2(source, destination)
        return True
    except FileNotFoundError:
        raise RuntimeError(f"Source file not found: {source}")
    except Exception as e:
        raise RuntimeError(f"Error copying {source} to {destination}: {e}")


def move_file(source: str, destination: str) -> bool:
    """Move/rename file from source to destination."""
    try:
        shutil.move(source, destination)
        return True
    except FileNotFoundError:
        raise RuntimeError(f"Source file not found: {source}")
    except Exception as e:
        raise RuntimeError(f"Error moving {source} to {destination}: {e}")


def rename_file(old_path: str, new_path: str) -> bool:
    """Rename a file."""
    try:
        os.rename(old_path, new_path)
        return True
    except FileNotFoundError:
        raise RuntimeError(f"File not found: {old_path}")
    except Exception as e:
        raise RuntimeError(f"Error renaming {old_path} to {new_path}: {e}")


# =======================
# Directory Operations
# =======================

def create_directory(dirpath: str) -> bool:
    """Create a single directory (fails if parent doesn't exist)."""
    try:
        os.mkdir(dirpath)
        return True
    except FileExistsError:
        return False
    except Exception as e:
        raise RuntimeError(f"Error creating directory {dirpath}: {e}")


def create_directories(dirpath: str) -> bool:
    """Create directory and all parent directories."""
    try:
        os.makedirs(dirpath, exist_ok=True)
        return True
    except Exception as e:
        raise RuntimeError(f"Error creating directories {dirpath}: {e}")


def directory_exists(dirpath: str) -> bool:
    """Check if directory exists."""
    return os.path.isdir(dirpath)


def list_directory(dirpath: str) -> List[str]:
    """List contents of directory."""
    try:
        return os.listdir(dirpath)
    except FileNotFoundError:
        raise RuntimeError(f"Directory not found: {dirpath}")
    except NotADirectoryError:
        raise RuntimeError(f"Not a directory: {dirpath}")
    except Exception as e:
        raise RuntimeError(f"Error listing directory {dirpath}: {e}")


def walk_directory(dirpath: str) -> List[Dict[str, Any]]:
    """
    Walk directory tree recursively.
    Returns list of dicts with 'path', 'root', 'dirs', 'files' keys.
    """
    try:
        result = []
        for root, dirs, files in os.walk(dirpath):
            result.append({
                'path': root,
                'root': root,
                'dirs': dirs,
                'directories': dirs,
                'files': files
            })
        return result
    except Exception as e:
        raise RuntimeError(f"Error walking directory {dirpath}: {e}")


def delete_directory(dirpath: str) -> bool:
    """Delete a directory (including non-empty directories)."""
    try:
        shutil.rmtree(dirpath)
        return True
    except FileNotFoundError:
        return False
    except Exception as e:
        raise RuntimeError(f"Error deleting directory {dirpath}: {e}")


def remove_directory_recursive(dirpath: str) -> bool:
    """Recursively delete directory and all contents."""
    try:
        shutil.rmtree(dirpath)
        return True
    except FileNotFoundError:
        return False
    except Exception as e:
        raise RuntimeError(f"Error removing directory tree {dirpath}: {e}")


# =======================
# Temporary Files
# =======================

def create_temp_file(suffix: str = "", prefix: str = "tmp", text: bool = True) -> str:
    """
    Create temporary file and return its path.
    File is NOT automatically deleted.
    """
    try:
        fd, path = tempfile.mkstemp(suffix=suffix, prefix=prefix, text=text)
        os.close(fd)  # Close file descriptor
        return path
    except Exception as e:
        raise RuntimeError(f"Error creating temp file: {e}")


def create_temp_directory(suffix: str = "", prefix: str = "tmp") -> str:
    """
    Create temporary directory and return its path.
    Directory is NOT automatically deleted.
    """
    try:
        return tempfile.mkdtemp(suffix=suffix, prefix=prefix)
    except Exception as e:
        raise RuntimeError(f"Error creating temp directory: {e}")


# =======================
# File Info
# =======================

def get_file_info(filepath: str) -> Dict[str, Any]:
    """Get detailed file information."""
    try:
        stat = os.stat(filepath)
        path = Path(filepath)
        
        return {
            'path': str(path.absolute()),
            'name': path.name,
            'parent': str(path.parent),
            'size': stat.st_size,
            'is_file': path.is_file(),
            'is_directory': path.is_dir(),
            'is_symlink': path.is_symlink(),
            'created': stat.st_ctime,
            'modified': stat.st_mtime,
            'accessed': stat.st_atime,
            'permissions': oct(stat.st_mode)[-3:]
        }
    except FileNotFoundError:
        raise RuntimeError(f"Path not found: {filepath}")
    except Exception as e:
        raise RuntimeError(f"Error getting file info for {filepath}: {e}")


def is_file(filepath: str) -> bool:
    """Check if path is a file."""
    return os.path.isfile(filepath)


def is_directory(dirpath: str) -> bool:
    """Check if path is a directory."""
    return os.path.isdir(dirpath)


def is_symlink(filepath: str) -> bool:
    """Check if path is a symbolic link."""
    return os.path.islink(filepath)
