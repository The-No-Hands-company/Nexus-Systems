"""
Tests for the NexusLang stdlib compression module.

All compression algorithms use Python stdlib only (gzip, zlib, bz2,
tarfile, zipfile) — no external dependencies, so every test runs
unconditionally.

Covers: gzip, zlib, bz2 (bytes roundtrip + file operations),
        zip (create/extract/list/add), tar (create/extract/list/add).
"""

import os
import sys
import pytest

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from nexuslang.stdlib.compression import (
    gzip_compress,
    gzip_decompress,
    gzip_compress_file,
    gzip_decompress_file,
    zlib_compress,
    zlib_decompress,
    bz2_compress,
    bz2_decompress,
    bz2_compress_file,
    bz2_decompress_file,
    zip_create,
    zip_extract,
    zip_list,
    zip_add,
    tar_create,
    tar_extract,
    tar_list,
    tar_add,
)

_TEST_TEXT = "Hello, compression! " * 100  # 2000 chars — good compression ratio


# ===========================================================================
# gzip (bytes API)
# ===========================================================================


class TestGzipBytes:
    def test_compress_returns_bytes(self):
        result = gzip_compress(_TEST_TEXT)
        assert isinstance(result, bytes)

    def test_decompress_returns_str(self):
        compressed = gzip_compress(_TEST_TEXT)
        assert isinstance(gzip_decompress(compressed), str)

    def test_roundtrip(self):
        assert gzip_decompress(gzip_compress(_TEST_TEXT)) == _TEST_TEXT

    def test_compressed_smaller_than_original(self):
        compressed = gzip_compress(_TEST_TEXT)
        assert len(compressed) < len(_TEST_TEXT.encode())

    def test_empty_string_roundtrip(self):
        assert gzip_decompress(gzip_compress("")) == ""

    def test_unicode_roundtrip(self):
        text = "Unicode: \u00e9\u00e0\u00fc\u4e2d\u6587"
        assert gzip_decompress(gzip_compress(text)) == text


# ===========================================================================
# gzip (file API)
# ===========================================================================


class TestGzipFile:
    def test_compress_file_returns_path(self, tmp_path):
        src = tmp_path / "source.txt"
        src.write_text(_TEST_TEXT)
        out = gzip_compress_file(str(src))
        assert isinstance(out, str) and os.path.exists(out)

    def test_compress_file_custom_output(self, tmp_path):
        src = tmp_path / "source.txt"
        src.write_text(_TEST_TEXT)
        out = tmp_path / "custom.gz"
        result = gzip_compress_file(str(src), str(out))
        assert result == str(out) and out.exists()

    def test_decompress_file_roundtrip(self, tmp_path):
        src = tmp_path / "source.txt"
        src.write_text(_TEST_TEXT)
        gz = gzip_compress_file(str(src))
        out = tmp_path / "restored.txt"
        gzip_decompress_file(gz, str(out))
        assert out.read_text() == _TEST_TEXT

    def test_compressed_file_smaller(self, tmp_path):
        src = tmp_path / "source.txt"
        src.write_text(_TEST_TEXT)
        gz = gzip_compress_file(str(src))
        assert os.path.getsize(gz) < os.path.getsize(str(src))


# ===========================================================================
# zlib (bytes API)
# ===========================================================================


class TestZlib:
    def test_compress_returns_bytes(self):
        assert isinstance(zlib_compress(_TEST_TEXT), bytes)

    def test_roundtrip(self):
        assert zlib_decompress(zlib_compress(_TEST_TEXT)) == _TEST_TEXT

    def test_empty_roundtrip(self):
        assert zlib_decompress(zlib_compress("")) == ""

    def test_compression_levels(self):
        for level in (1, 6, 9):
            assert zlib_decompress(zlib_compress(_TEST_TEXT, level=level)) == _TEST_TEXT

    def test_compressed_smaller(self):
        assert len(zlib_compress(_TEST_TEXT)) < len(_TEST_TEXT.encode())


# ===========================================================================
# bz2 (bytes API)
# ===========================================================================


class TestBz2Bytes:
    def test_compress_returns_bytes(self):
        assert isinstance(bz2_compress(_TEST_TEXT), bytes)

    def test_roundtrip(self):
        assert bz2_decompress(bz2_compress(_TEST_TEXT)) == _TEST_TEXT

    def test_empty_roundtrip(self):
        assert bz2_decompress(bz2_compress("")) == ""

    def test_compression_levels(self):
        for level in (1, 5, 9):
            assert bz2_decompress(bz2_compress(_TEST_TEXT, level=level)) == _TEST_TEXT

    def test_compressed_smaller(self):
        assert len(bz2_compress(_TEST_TEXT)) < len(_TEST_TEXT.encode())


# ===========================================================================
# bz2 (file API)
# ===========================================================================


class TestBz2File:
    def test_compress_file_returns_path(self, tmp_path):
        src = tmp_path / "source.txt"
        src.write_text(_TEST_TEXT)
        out = bz2_compress_file(str(src))
        assert isinstance(out, str) and os.path.exists(out)

    def test_decompress_file_roundtrip(self, tmp_path):
        src = tmp_path / "source.txt"
        src.write_text(_TEST_TEXT)
        bz2_path = bz2_compress_file(str(src))
        out = tmp_path / "restored.txt"
        bz2_decompress_file(bz2_path, str(out))
        assert out.read_text() == _TEST_TEXT


# ===========================================================================
# ZIP archives
# ===========================================================================


class TestZip:
    def _make_files(self, tmp_path, count=3):
        files = []
        for i in range(count):
            p = tmp_path / f"file{i}.txt"
            p.write_text(f"content of file {i} — {_TEST_TEXT[:50]}")
            files.append(str(p))
        return files

    def test_create_returns_true(self, tmp_path):
        files = self._make_files(tmp_path)
        archive = str(tmp_path / "test.zip")
        assert zip_create(archive, files) is True

    def test_archive_exists(self, tmp_path):
        files = self._make_files(tmp_path)
        archive = str(tmp_path / "test.zip")
        zip_create(archive, files)
        assert os.path.exists(archive)

    def test_list_returns_filenames(self, tmp_path):
        files = self._make_files(tmp_path)
        archive = str(tmp_path / "test.zip")
        zip_create(archive, files)
        names = zip_list(archive)
        assert isinstance(names, list) and len(names) == len(files)

    def test_extract_restores_files(self, tmp_path):
        files = self._make_files(tmp_path)
        archive = str(tmp_path / "test.zip")
        zip_create(archive, files)
        out_dir = tmp_path / "extracted"
        out_dir.mkdir()
        assert zip_extract(archive, str(out_dir)) is True

    def test_add_increases_count(self, tmp_path):
        files = self._make_files(tmp_path, count=2)
        archive = str(tmp_path / "test.zip")
        zip_create(archive, files)
        extra = tmp_path / "extra.txt"
        extra.write_text("extra file content")
        zip_add(archive, str(extra))
        assert len(zip_list(archive)) == 3


# ===========================================================================
# TAR archives
# ===========================================================================


class TestTar:
    def _make_files(self, tmp_path, count=3):
        files = []
        for i in range(count):
            p = tmp_path / f"item{i}.txt"
            p.write_text(f"tar content {i}")
            files.append(str(p))
        return files

    def test_create_returns_true(self, tmp_path):
        files = self._make_files(tmp_path)
        archive = str(tmp_path / "test.tar")
        assert tar_create(archive, files) is True

    def test_archive_exists(self, tmp_path):
        files = self._make_files(tmp_path)
        archive = str(tmp_path / "test.tar")
        tar_create(archive, files)
        assert os.path.exists(archive)

    def test_list_returns_filenames(self, tmp_path):
        files = self._make_files(tmp_path)
        archive = str(tmp_path / "test.tar")
        tar_create(archive, files)
        names = tar_list(archive)
        assert isinstance(names, list) and len(names) == len(files)

    def test_extract_restores_files(self, tmp_path):
        files = self._make_files(tmp_path)
        archive = str(tmp_path / "test.tar")
        tar_create(archive, files)
        out_dir = tmp_path / "extracted"
        out_dir.mkdir()
        assert tar_extract(archive, str(out_dir)) is True

    def test_create_with_gzip_compression(self, tmp_path):
        files = self._make_files(tmp_path)
        archive = str(tmp_path / "test.tar.gz")
        assert tar_create(archive, files, compression="gz") is True
        assert os.path.exists(archive)

    def test_add_increases_count(self, tmp_path):
        files = self._make_files(tmp_path, count=2)
        archive = str(tmp_path / "test.tar")
        tar_create(archive, files)
        extra = tmp_path / "extra.txt"
        extra.write_text("extra tar content")
        tar_add(archive, str(extra))
        assert len(tar_list(archive)) == 3
