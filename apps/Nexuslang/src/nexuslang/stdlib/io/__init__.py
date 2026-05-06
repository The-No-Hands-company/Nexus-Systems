"""
IO module for the NexusLang standard library.
This module provides input/output functions for file operations.
"""

import os
import json
import shutil
from ...runtime.runtime import Runtime

def register_io_functions(runtime: Runtime) -> None:
    """Register IO functions with the runtime."""
    # File operations
    runtime.register_function("read_file", read_file)
    runtime.register_function("write_file", write_file)
    runtime.register_function("append_file", append_file)
    runtime.register_function("file_exists", file_exists)
    runtime.register_function("delete_file", delete_file)
    runtime.register_function("copy_file", copy_file)
    runtime.register_function("move_file", move_file)
    runtime.register_function("get_file_size", get_file_size)
    runtime.register_function("read_lines", read_lines)
    runtime.register_function("write_lines", write_lines)
    runtime.register_function("read_bytes", read_bytes)
    runtime.register_function("write_bytes", write_bytes)
    
    # Directory operations
    runtime.register_function("list_directory", list_directory)
    runtime.register_function("create_directory", create_directory)
    runtime.register_function("directory_exists", directory_exists)
    runtime.register_function("delete_directory", delete_directory)
    runtime.register_function("walk_directory", walk_directory)
    
    # Path operations
    runtime.register_function("join_path", join_path)
    runtime.register_function("get_basename", get_basename)
    runtime.register_function("get_dirname", get_dirname)
    runtime.register_function("get_extension", get_extension)
    runtime.register_function("absolute_path", absolute_path)
    runtime.register_function("normalize_path", normalize_path)
    
    # JSON operations
    runtime.register_function("parse_json", parse_json)
    runtime.register_function("stringify_json", stringify_json)
    
    # Console operations
    runtime.register_function("print", print_to_console)
    runtime.register_function("println", print_to_console)  # Alias
    runtime.register_function("input", get_input)
    runtime.register_function("read_input", get_input)  # Alias

    # Buffered I/O
    runtime.register_function("BufferedReader", BufferedReader)
    runtime.register_function("BufferedWriter", BufferedWriter)
    runtime.register_function("Pipe", Pipe)
    runtime.register_function("MemoryMappedFile", MemoryMappedFile)

# File operations
def read_file(path, encoding="utf-8"):
    """Read the contents of a file."""
    if not os.path.isfile(path):
        raise FileNotFoundError(f"File not found: {path}")
    
    with open(path, "r", encoding=encoding) as f:
        return f.read()

def write_file(path, content, encoding="utf-8"):
    """Write content to a file, overwriting any existing content."""
    with open(path, "w", encoding=encoding) as f:
        f.write(content)
    return True

def append_file(path, content, encoding="utf-8"):
    """Append content to a file."""
    with open(path, "a", encoding=encoding) as f:
        f.write(content)
    return True

def file_exists(path):
    """Check if a file exists."""
    return os.path.isfile(path)

def delete_file(path):
    """Delete a file."""
    if not os.path.isfile(path):
        raise FileNotFoundError(f"File not found: {path}")
    
    os.remove(path)
    return True

# Directory operations
def list_directory(path="."):
    """List the contents of a directory."""
    if not os.path.isdir(path):
        raise NotADirectoryError(f"Not a directory: {path}")
    
    return os.listdir(path)

def create_directory(path, exist_ok=True):
    """Create a directory."""
    os.makedirs(path, exist_ok=exist_ok)
    return True

def directory_exists(path):
    """Check if a directory exists."""
    return os.path.isdir(path)

def delete_directory(path, recursive=True):
    """Delete a directory."""
    if not os.path.isdir(path):
        raise NotADirectoryError(f"Not a directory: {path}")
    
    if recursive:
        shutil.rmtree(path)
    else:
        os.rmdir(path)
    
    return True

# Path operations
def join_path(*paths):
    """Join path components."""
    return os.path.join(*paths)

def get_basename(path):
    """Get the base name of a path."""
    return os.path.basename(path)

def get_dirname(path):
    """Get the directory name of a path."""
    return os.path.dirname(path)

def get_extension(path):
    """Get the file extension of a path."""
    return os.path.splitext(path)[1]

# JSON operations
def parse_json(json_str):
    """Parse a JSON string into an object."""
    return json.loads(json_str)

def stringify_json(obj, indent=None):
    """Convert an object to a JSON string."""
    return json.dumps(obj, indent=indent)

# Console operations
def print_to_console(*args, sep=" ", end="\n"):
    """Print to the console."""
    print(*args, sep=sep, end=end)
    return None

def get_input(prompt=""):
    """Get input from the user."""
    return input(prompt)

# Additional file operations

def copy_file(source, destination):
    """Copy a file from source to destination."""
    try:
        shutil.copy2(source, destination)
        return True
    except Exception as e:
        raise RuntimeError(f"Error copying file from '{source}' to '{destination}': {str(e)}")

def move_file(source, destination):
    """Move/rename a file from source to destination."""
    try:
        shutil.move(source, destination)
        return True
    except Exception as e:
        raise RuntimeError(f"Error moving file from '{source}' to '{destination}': {str(e)}")

def get_file_size(file_path):
    """Get the size of a file in bytes."""
    try:
        return os.path.getsize(file_path)
    except Exception as e:
        raise RuntimeError(f"Error getting file size for '{file_path}': {str(e)}")

def read_lines(file_path, encoding="utf-8"):
    """Read file contents as a list of lines."""
    try:
        with open(file_path, "r", encoding=encoding) as file:
            return file.readlines()
    except Exception as e:
        raise RuntimeError(f"Error reading lines from '{file_path}': {str(e)}")

def write_lines(file_path, lines, encoding="utf-8"):
    """Write a list of lines to a file."""
    try:
        with open(file_path, "w", encoding=encoding) as file:
            file.writelines(lines)
        return True
    except Exception as e:
        raise RuntimeError(f"Error writing lines to '{file_path}': {str(e)}")

def read_bytes(file_path):
    """Read file contents as bytes."""
    try:
        with open(file_path, "rb") as file:
            return file.read()
    except Exception as e:
        raise RuntimeError(f"Error reading bytes from '{file_path}': {str(e)}")

def write_bytes(file_path, data):
    """Write bytes to a file."""
    try:
        with open(file_path, "wb") as file:
            file.write(data)
        return True
    except Exception as e:
        raise RuntimeError(f"Error writing bytes to '{file_path}': {str(e)}")

def walk_directory(directory_path):
    """Walk through directory tree, returning (dirpath, dirnames, filenames) tuples."""
    try:
        results = []
        for dirpath, dirnames, filenames in os.walk(directory_path):
            results.append({
                "path": dirpath,
                "directories": dirnames,
                "files": filenames
            })
        return results
    except Exception as e:
        raise RuntimeError(f"Error walking directory '{directory_path}': {str(e)}")

def absolute_path(path):
    """Get the absolute path of a file or directory."""
    return os.path.abspath(path)

def normalize_path(path):
    """Normalize a path (resolve .. and . components)."""
    return os.path.normpath(path)


# ---------------------------------------------------------------------------
# Buffered I/O
# ---------------------------------------------------------------------------

class BufferedReader:
    """
    Buffered file reader.  Reads chunks into an internal buffer to minimise
    system-call overhead when reading data line-by-line or in small chunks.
    """

    def __init__(self, path: str, buffer_size: int = 8192, encoding: str = "utf-8") -> None:
        self._path = path
        self._buffer_size = buffer_size
        self._encoding = encoding
        self._file = None
        self._buffer = b""
        self._pos = 0
        self._eof = False

    def open(self) -> None:
        self._file = open(self._path, "rb")
        self._buffer = b""
        self._pos = 0
        self._eof = False

    def close(self) -> None:
        if self._file is not None:
            self._file.close()
            self._file = None

    def _fill(self) -> None:
        if self._file is None:
            raise IOError("BufferedReader: file not open")
        chunk = self._file.read(self._buffer_size)
        if not chunk:
            self._eof = True
        self._buffer = self._buffer[self._pos:] + chunk
        self._pos = 0

    def read(self, n: int = -1) -> str:
        if self._file is None:
            self.open()
        if n < 0:
            return self._file.read().decode(self._encoding)
        data = b""
        while len(data) < n and not self._eof:
            available = self._buffer[self._pos:]
            need = n - len(data)
            data += available[:need]
            self._pos += min(need, len(available))
            if self._pos >= len(self._buffer):
                self._fill()
        return data.decode(self._encoding)

    def read_line(self) -> str:
        if self._file is None:
            self.open()
        while True:
            chunk = self._buffer[self._pos:]
            idx = chunk.find(b"\n")
            if idx >= 0:
                line = chunk[:idx + 1]
                self._pos += idx + 1
                return line.decode(self._encoding)
            if self._eof:
                self._pos = len(self._buffer)
                return chunk.decode(self._encoding)
            self._fill()

    def read_lines(self) -> list:
        lines = []
        while True:
            line = self.read_line()
            if not line:
                break
            lines.append(line)
        return lines

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *_):
        self.close()

    def __repr__(self) -> str:
        return f"BufferedReader(path={self._path!r}, buffer_size={self._buffer_size})"


class BufferedWriter:
    """
    Buffered file writer.  Accumulates writes in an internal buffer and
    flushes to disk when the buffer is full or explicitly flushed/closed.
    """

    def __init__(self, path: str, buffer_size: int = 8192,
                 encoding: str = "utf-8", append: bool = False) -> None:
        self._path = path
        self._buffer_size = buffer_size
        self._encoding = encoding
        self._mode = "ab" if append else "wb"
        self._file = None
        self._buffer = bytearray()

    def open(self) -> None:
        self._file = open(self._path, self._mode)
        self._buffer = bytearray()

    def write(self, data: str) -> int:
        if self._file is None:
            self.open()
        encoded = data.encode(self._encoding)
        self._buffer.extend(encoded)
        if len(self._buffer) >= self._buffer_size:
            self.flush()
        return len(data)

    def write_line(self, line: str) -> None:
        self.write(line if line.endswith("\n") else line + "\n")

    def flush(self) -> None:
        if self._file is not None and self._buffer:
            self._file.write(bytes(self._buffer))
            self._file.flush()
            self._buffer = bytearray()

    def close(self) -> None:
        self.flush()
        if self._file is not None:
            self._file.close()
            self._file = None

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *_):
        self.close()

    def __repr__(self) -> str:
        return f"BufferedWriter(path={self._path!r}, buffered={len(self._buffer)}b)"


# ---------------------------------------------------------------------------
# Pipe — in-process byte/string pipe
# ---------------------------------------------------------------------------

import io as _io
import threading as _threading


class Pipe:
    """
    In-process pipe: one thread writes, another reads.
    Backed by a thread-safe queue for unbounded buffering.
    """

    def __init__(self) -> None:
        import queue as _q
        self._queue = _q.Queue()
        self._closed = False

    def write(self, data: str) -> int:
        if self._closed:
            raise IOError("Pipe: write to closed pipe")
        self._queue.put(data)
        return len(data)

    def write_bytes(self, data: bytes) -> int:
        if self._closed:
            raise IOError("Pipe: write to closed pipe")
        self._queue.put(data)
        return len(data)

    def read(self, timeout: float = None) -> str:
        """
        Read one item from the pipe.

        Args:
            timeout: Seconds to wait. None = block forever, 0 = non-blocking.

        Returns:
            Data str/bytes, or None if timed out / pipe empty (non-blocking).
        """
        import queue as _q
        try:
            if timeout == 0:
                return self._queue.get_nowait()
            return self._queue.get(timeout=timeout)
        except _q.Empty:
            return None

    def read_all(self) -> list:
        """Drain all currently available items without blocking."""
        items = []
        while True:
            item = self.read(timeout=0)
            if item is None:
                break
            items.append(item)
        return items

    def close(self) -> None:
        self._closed = True

    def is_empty(self) -> bool:
        return self._queue.empty()

    def __repr__(self) -> str:
        return f"Pipe(closed={self._closed}, size~={self._queue.qsize()})"


# ---------------------------------------------------------------------------
# MemoryMappedFile
# ---------------------------------------------------------------------------

import mmap as _mmap


class MemoryMappedFile:
    """
    Memory-mapped file access for high-performance random reads/writes
    on large files without loading them entirely into RAM.
    """

    def __init__(self, path: str, writable: bool = False) -> None:
        self._path = path
        self._writable = writable
        self._file = None
        self._mmap = None

    def open(self) -> None:
        mode = "r+b" if self._writable else "rb"
        self._file = open(self._path, mode)
        access = _mmap.ACCESS_WRITE if self._writable else _mmap.ACCESS_READ
        self._mmap = _mmap.mmap(self._file.fileno(), 0, access=access)

    def close(self) -> None:
        if self._mmap is not None:
            self._mmap.close()
            self._mmap = None
        if self._file is not None:
            self._file.close()
            self._file = None

    def read_at(self, offset: int, length: int) -> bytes:
        if self._mmap is None:
            raise IOError("MemoryMappedFile: not open")
        self._mmap.seek(offset)
        return self._mmap.read(length)

    def read_str_at(self, offset: int, length: int, encoding: str = "utf-8") -> str:
        return self.read_at(offset, length).decode(encoding)

    def write_at(self, offset: int, data: bytes) -> None:
        if not self._writable:
            raise IOError("MemoryMappedFile: opened read-only")
        if self._mmap is None:
            raise IOError("MemoryMappedFile: not open")
        self._mmap.seek(offset)
        self._mmap.write(data)

    def size(self) -> int:
        if self._mmap is None:
            raise IOError("MemoryMappedFile: not open")
        return self._mmap.size()

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *_):
        self.close()

    def __repr__(self) -> str:
        return f"MemoryMappedFile(path={self._path!r}, writable={self._writable})"
