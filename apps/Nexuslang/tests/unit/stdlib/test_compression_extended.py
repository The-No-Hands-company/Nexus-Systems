"""
Tests for extended compression functions: lzma (stdlib), lz4, zstd.
lzma is always available; lz4 and zstd require optional third-party packages.
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.stdlib.compression import (
    # lzma (always available)
    lzma_compress,
    lzma_decompress,
    lzma_compress_file,
    lzma_decompress_file,
    # lz4 (optional)
    lz4_compress,
    lz4_decompress,
    lz4_compress_file,
    lz4_decompress_file,
    # zstd (optional)
    zstd_compress,
    zstd_decompress,
    zstd_compress_file,
    zstd_decompress_file,
    # availability flags
    HAS_LZ4,
    HAS_ZSTD,
)
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib.compression import register_compression_functions

SAMPLE_ASCII = "The quick brown fox jumps over the lazy dog"
SAMPLE_UNICODE = "Unicode: \u4e2d\u6587 \u0420\u0443\u0441\u0441\u043a\u0438\u0439 Deutsch"
LARGE_DATA = "NLPL compression test data. " * 500


# ---------------------------------------------------------------------------
# LZMA (XZ) — always available, no skip needed
# ---------------------------------------------------------------------------


class TestLzma:
    def test_compress_returns_bytes(self):
        result = lzma_compress(SAMPLE_ASCII)
        assert isinstance(result, bytes)

    def test_round_trip_ascii(self):
        compressed = lzma_compress(SAMPLE_ASCII)
        assert lzma_decompress(compressed) == SAMPLE_ASCII

    def test_round_trip_unicode(self):
        compressed = lzma_compress(SAMPLE_UNICODE)
        assert lzma_decompress(compressed) == SAMPLE_UNICODE

    def test_round_trip_empty_string(self):
        compressed = lzma_compress("")
        assert lzma_decompress(compressed) == ""

    def test_compresses_repetitive_data(self):
        compressed = lzma_compress(LARGE_DATA)
        assert len(compressed) < len(LARGE_DATA.encode())

    def test_custom_encoding(self):
        text = "Hello latin-1"
        compressed = lzma_compress(text, encoding='latin-1')
        assert lzma_decompress(compressed, encoding='latin-1') == text


class TestLzmaFile:
    def test_compress_file_returns_path(self, tmp_path):
        src = tmp_path / "data.txt"
        src.write_text(SAMPLE_ASCII)
        out = lzma_compress_file(str(src))
        assert out is not None
        assert os.path.exists(out)

    def test_compress_file_default_extension(self, tmp_path):
        src = tmp_path / "data.txt"
        src.write_text(SAMPLE_ASCII)
        out = lzma_compress_file(str(src))
        assert out.endswith('.xz')

    def test_decompress_file_round_trip(self, tmp_path):
        src = tmp_path / "data.txt"
        src.write_text(SAMPLE_ASCII)
        compressed_path = lzma_compress_file(str(src))
        restored_path = tmp_path / "restored.txt"
        result = lzma_decompress_file(compressed_path, str(restored_path))
        assert result is True
        assert restored_path.read_text() == SAMPLE_ASCII

    def test_decompress_missing_file_returns_false(self, tmp_path):
        result = lzma_decompress_file("/nonexistent/path.xz", str(tmp_path / "out.txt"))
        assert result is False


# ---------------------------------------------------------------------------
# LZ4 — optional, skip if not installed
# ---------------------------------------------------------------------------

lz4_mark = pytest.mark.skipif(not HAS_LZ4, reason="lz4 package not installed")


@lz4_mark
class TestLz4:
    def test_compress_returns_bytes(self):
        result = lz4_compress(SAMPLE_ASCII)
        assert isinstance(result, bytes)

    def test_round_trip(self):
        assert lz4_decompress(lz4_compress(SAMPLE_ASCII)) == SAMPLE_ASCII

    def test_round_trip_unicode(self):
        assert lz4_decompress(lz4_compress(SAMPLE_UNICODE)) == SAMPLE_UNICODE

    def test_round_trip_empty_string(self):
        assert lz4_decompress(lz4_compress("")) == ""

    def test_round_trip_large_data(self):
        assert lz4_decompress(lz4_compress(LARGE_DATA)) == LARGE_DATA

    def test_compression_level_zero_fastest(self):
        compressed = lz4_compress(SAMPLE_ASCII, level=0)
        assert isinstance(compressed, bytes)
        assert lz4_decompress(compressed) == SAMPLE_ASCII

    def test_compression_level_9_slowest(self):
        compressed = lz4_compress(LARGE_DATA, level=9)
        assert isinstance(compressed, bytes)
        assert lz4_decompress(compressed) == LARGE_DATA

    def test_no_lz4_compress_raises(self, monkeypatch):
        import src.nlpl.stdlib.compression as mod
        monkeypatch.setattr(mod, 'HAS_LZ4', False)
        with pytest.raises(ImportError, match="lz4 package required"):
            mod.lz4_compress(SAMPLE_ASCII)

    def test_no_lz4_decompress_raises(self, monkeypatch):
        import src.nlpl.stdlib.compression as mod
        monkeypatch.setattr(mod, 'HAS_LZ4', False)
        with pytest.raises(ImportError, match="lz4 package required"):
            mod.lz4_decompress(b"\x04\x22\x4d\x18")


@lz4_mark
class TestLz4File:
    def test_compress_file_returns_path(self, tmp_path):
        src = tmp_path / "data.txt"
        src.write_text(LARGE_DATA)
        out = lz4_compress_file(str(src))
        assert os.path.exists(out)

    def test_compress_file_default_extension(self, tmp_path):
        src = tmp_path / "data.txt"
        src.write_text(SAMPLE_ASCII)
        out = lz4_compress_file(str(src))
        assert out.endswith('.lz4')

    def test_decompress_file_round_trip(self, tmp_path):
        src = tmp_path / "data.txt"
        src.write_text(LARGE_DATA)
        compressed_path = lz4_compress_file(str(src))
        restored_path = tmp_path / "restored.txt"
        result = lz4_decompress_file(compressed_path, str(restored_path))
        assert result is True
        assert restored_path.read_text() == LARGE_DATA

    def test_no_lz4_compress_file_raises(self, monkeypatch):
        import src.nlpl.stdlib.compression as mod
        monkeypatch.setattr(mod, 'HAS_LZ4', False)
        with pytest.raises(ImportError, match="lz4 package required"):
            mod.lz4_compress_file("/tmp/nonexistent.txt")


# ---------------------------------------------------------------------------
# ZSTD — optional, skip if not installed
# ---------------------------------------------------------------------------

zstd_mark = pytest.mark.skipif(not HAS_ZSTD, reason="zstandard package not installed")


@zstd_mark
class TestZstd:
    def test_compress_returns_bytes(self):
        result = zstd_compress(SAMPLE_ASCII)
        assert isinstance(result, bytes)

    def test_round_trip(self):
        assert zstd_decompress(zstd_compress(SAMPLE_ASCII)) == SAMPLE_ASCII

    def test_round_trip_unicode(self):
        assert zstd_decompress(zstd_compress(SAMPLE_UNICODE)) == SAMPLE_UNICODE

    def test_round_trip_empty_string(self):
        assert zstd_decompress(zstd_compress("")) == ""

    def test_round_trip_large_data(self):
        assert zstd_decompress(zstd_compress(LARGE_DATA)) == LARGE_DATA

    def test_compresses_repetitive_data(self):
        compressed = zstd_compress(LARGE_DATA)
        assert len(compressed) < len(LARGE_DATA.encode())

    def test_compression_level_1_fastest(self):
        compressed = zstd_compress(SAMPLE_ASCII, level=1)
        assert isinstance(compressed, bytes)
        assert zstd_decompress(compressed) == SAMPLE_ASCII

    def test_compression_level_22_best(self):
        compressed = zstd_compress(LARGE_DATA, level=22)
        assert isinstance(compressed, bytes)
        assert zstd_decompress(compressed) == LARGE_DATA

    def test_higher_level_smaller_or_equal(self):
        low = zstd_compress(LARGE_DATA, level=1)
        high = zstd_compress(LARGE_DATA, level=22)
        assert len(high) <= len(low)

    def test_no_zstd_compress_raises(self, monkeypatch):
        import src.nlpl.stdlib.compression as mod
        monkeypatch.setattr(mod, 'HAS_ZSTD', False)
        with pytest.raises(ImportError, match="zstandard package required"):
            mod.zstd_compress(SAMPLE_ASCII)

    def test_no_zstd_decompress_raises(self, monkeypatch):
        import src.nlpl.stdlib.compression as mod
        monkeypatch.setattr(mod, 'HAS_ZSTD', False)
        with pytest.raises(ImportError, match="zstandard package required"):
            mod.zstd_decompress(b"\x28\xb5\x2f\xfd")


@zstd_mark
class TestZstdFile:
    def test_compress_file_returns_path(self, tmp_path):
        src = tmp_path / "data.txt"
        src.write_text(LARGE_DATA)
        out = zstd_compress_file(str(src))
        assert os.path.exists(out)

    def test_compress_file_default_extension(self, tmp_path):
        src = tmp_path / "data.txt"
        src.write_text(SAMPLE_ASCII)
        out = zstd_compress_file(str(src))
        assert out.endswith('.zst')

    def test_decompress_file_round_trip(self, tmp_path):
        src = tmp_path / "data.txt"
        src.write_text(LARGE_DATA)
        compressed_path = zstd_compress_file(str(src))
        restored_path = tmp_path / "restored.txt"
        result = zstd_decompress_file(compressed_path, str(restored_path))
        assert result is True
        assert restored_path.read_text() == LARGE_DATA

    def test_compress_file_custom_level(self, tmp_path):
        src = tmp_path / "data.txt"
        src.write_text(LARGE_DATA)
        out = zstd_compress_file(str(src), level=10)
        assert os.path.exists(out)
        assert os.path.getsize(out) > 0

    def test_no_zstd_compress_file_raises(self, monkeypatch):
        import src.nlpl.stdlib.compression as mod
        monkeypatch.setattr(mod, 'HAS_ZSTD', False)
        with pytest.raises(ImportError, match="zstandard package required"):
            mod.zstd_compress_file("/tmp/nonexistent.txt")


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------


class TestCompressionExtendedRegistration:
    def _make_runtime(self):
        rt = Runtime()
        register_compression_functions(rt)
        return rt

    def test_lzma_functions_registered(self):
        rt = self._make_runtime()
        for name in ["lzma_compress", "lzma_decompress", "lzma_compress_file", "lzma_decompress_file"]:
            assert name in rt.functions, f"Missing: {name}"

    @pytest.mark.skipif(not HAS_LZ4, reason="lz4 package not installed")
    def test_lz4_functions_registered(self):
        rt = self._make_runtime()
        for name in ["lz4_compress", "lz4_decompress", "lz4_compress_file", "lz4_decompress_file"]:
            assert name in rt.functions, f"Missing: {name}"

    @pytest.mark.skipif(not HAS_ZSTD, reason="zstandard package not installed")
    def test_zstd_functions_registered(self):
        rt = self._make_runtime()
        for name in ["zstd_compress", "zstd_decompress", "zstd_compress_file", "zstd_decompress_file"]:
            assert name in rt.functions, f"Missing: {name}"
