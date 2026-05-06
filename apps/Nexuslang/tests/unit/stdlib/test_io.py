"""
Buffered I/O tests: BufferedReader, BufferedWriter, Pipe, MemoryMappedFile.
Split from test_session_features.py.
"""

import sys
import os
import tempfile
import pytest
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

class TestBufferedIO:
    def test_buffered_reader_import(self):
        from nexuslang.stdlib.io import BufferedReader
        assert BufferedReader is not None

    def test_buffered_writer_import(self):
        from nexuslang.stdlib.io import BufferedWriter
        assert BufferedWriter is not None

    def test_pipe_import(self):
        from nexuslang.stdlib.io import Pipe
        assert Pipe is not None

    def test_memory_mapped_file_import(self):
        from nexuslang.stdlib.io import MemoryMappedFile
        assert MemoryMappedFile is not None

    def test_pipe_write_read(self):
        from nexuslang.stdlib.io import Pipe
        p = Pipe()
        p.write(b"hello")
        data = p.read(5)
        assert data == b"hello"

    def test_buffered_writer_and_reader(self):
        from nexuslang.stdlib.io import BufferedWriter, BufferedReader
        with tempfile.NamedTemporaryFile(delete=False) as f:
            path = f.name
        try:
            w = BufferedWriter(path)
            w.write("test content")
            w.close()
            r = BufferedReader(path)
            content = r.read()
            r.close()
            assert "test content" in content
        finally:
            os.unlink(path)


# ============================================================
# Section 11 - Lexer tokens for test framework
# ============================================================

