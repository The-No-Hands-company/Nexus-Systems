"""Tests for CBOR serialization functions in nlpl.stdlib.serialization."""

import os
import math
import tempfile
import pytest

from nexuslang.stdlib.serialization import (
    cbor_dumps,
    cbor_loads,
    cbor_dump_file,
    cbor_load_file,
    HAS_CBOR,
)

# Skip every test in this module when cbor2 is not installed.
pytestmark = pytest.mark.skipif(not HAS_CBOR, reason="cbor2 not installed")


# ---------------------------------------------------------------------------
# cbor_dumps / cbor_loads — in-memory round-trips
# ---------------------------------------------------------------------------

class TestCborDumpsLoads:
    def test_dumps_returns_bytes(self):
        result = cbor_dumps({"key": "value"})
        assert isinstance(result, bytes)

    def test_loads_returns_original_type(self):
        assert isinstance(cbor_loads(cbor_dumps({})), dict)

    def test_roundtrip_dict(self):
        obj = {"name": "Alice", "age": 30, "active": True}
        assert cbor_loads(cbor_dumps(obj)) == obj

    def test_roundtrip_list(self):
        obj = [1, 2, 3, "hello", None, False]
        assert cbor_loads(cbor_dumps(obj)) == obj

    def test_roundtrip_nested_structure(self):
        obj = {"users": [{"id": 1, "tags": ["admin", "dev"]}, {"id": 2, "tags": []}]}
        assert cbor_loads(cbor_dumps(obj)) == obj

    def test_roundtrip_integer(self):
        for n in (0, 1, -1, 255, 256, 2**31 - 1, -(2**31)):
            assert cbor_loads(cbor_dumps(n)) == n

    def test_roundtrip_float(self):
        for v in (0.0, 1.5, -3.14, 1e100, -1e-100):
            result = cbor_loads(cbor_dumps(v))
            assert math.isclose(result, v, rel_tol=1e-9)

    def test_roundtrip_none(self):
        assert cbor_loads(cbor_dumps(None)) is None

    def test_roundtrip_bool_true(self):
        assert cbor_loads(cbor_dumps(True)) is True

    def test_roundtrip_bool_false(self):
        assert cbor_loads(cbor_dumps(False)) is False

    def test_roundtrip_empty_string(self):
        assert cbor_loads(cbor_dumps("")) == ""

    def test_roundtrip_unicode_string(self):
        obj = "Hello, World! Cześć Monde"
        assert cbor_loads(cbor_dumps(obj)) == obj

    def test_roundtrip_bytes(self):
        obj = b"\x00\x01\x02\xff"
        assert cbor_loads(cbor_dumps(obj)) == obj

    def test_roundtrip_empty_dict(self):
        assert cbor_loads(cbor_dumps({})) == {}

    def test_roundtrip_empty_list(self):
        assert cbor_loads(cbor_dumps([])) == []

    def test_cbor_is_more_compact_than_json(self):
        import json
        obj = {"items": list(range(50)), "label": "benchmark"}
        cbor_size = len(cbor_dumps(obj))
        json_size = len(json.dumps(obj).encode())
        assert cbor_size < json_size, (
            f"Expected CBOR ({cbor_size} B) to be smaller than JSON ({json_size} B)"
        )

    def test_dumps_error_on_missing_cbor2(self, monkeypatch):
        import nexuslang.stdlib.serialization as mod
        monkeypatch.setattr(mod, "HAS_CBOR", False)
        with pytest.raises(ImportError, match="cbor2"):
            mod.cbor_dumps({"a": 1})

    def test_loads_error_on_missing_cbor2(self, monkeypatch):
        import nexuslang.stdlib.serialization as mod
        monkeypatch.setattr(mod, "HAS_CBOR", False)
        with pytest.raises(ImportError, match="cbor2"):
            mod.cbor_loads(b"\xa1")

    def test_loads_invalid_bytes_raises(self):
        import cbor2
        # Empty bytes are not valid CBOR and must raise
        with pytest.raises((cbor2.CBORDecodeError, Exception)):
            cbor_loads(b"")


# ---------------------------------------------------------------------------
# cbor_dump_file / cbor_load_file — file I/O round-trips
# ---------------------------------------------------------------------------

class TestCborFileIO:
    def test_dump_and_load_dict(self, tmp_path):
        obj = {"project": "NexusLang", "version": 1}
        path = str(tmp_path / "test.cbor")
        assert cbor_dump_file(obj, path) is True
        assert cbor_load_file(path) == obj

    def test_dump_creates_file(self, tmp_path):
        path = str(tmp_path / "out.cbor")
        cbor_dump_file([1, 2, 3], path)
        assert os.path.isfile(path)

    def test_dump_file_size_nonzero(self, tmp_path):
        path = str(tmp_path / "data.cbor")
        cbor_dump_file({"key": "value"}, path)
        assert os.path.getsize(path) > 0

    def test_dump_and_load_nested(self, tmp_path):
        obj = {"matrix": [[1, 2], [3, 4]], "meta": {"rows": 2, "cols": 2}}
        path = str(tmp_path / "matrix.cbor")
        cbor_dump_file(obj, path)
        assert cbor_load_file(path) == obj

    def test_dump_and_load_bytes(self, tmp_path):
        obj = b"\xde\xad\xbe\xef"
        path = str(tmp_path / "bytes.cbor")
        cbor_dump_file(obj, path)
        assert cbor_load_file(path) == obj

    def test_dump_creates_parent_dirs(self, tmp_path):
        path = str(tmp_path / "deep" / "nested" / "file.cbor")
        cbor_dump_file({"a": 1}, path)
        assert os.path.isfile(path)

    def test_load_nonexistent_file_raises(self):
        with pytest.raises((FileNotFoundError, OSError)):
            cbor_load_file("/nonexistent/path/file.cbor")

    def test_dump_error_on_missing_cbor2(self, tmp_path, monkeypatch):
        import nexuslang.stdlib.serialization as mod
        monkeypatch.setattr(mod, "HAS_CBOR", False)
        with pytest.raises(ImportError, match="cbor2"):
            mod.cbor_dump_file({"a": 1}, str(tmp_path / "f.cbor"))

    def test_load_error_on_missing_cbor2(self, tmp_path, monkeypatch):
        import nexuslang.stdlib.serialization as mod
        # Write a real file first so missing-library is the only issue
        path = str(tmp_path / "f.cbor")
        cbor_dump_file({"a": 1}, path)
        monkeypatch.setattr(mod, "HAS_CBOR", False)
        with pytest.raises(ImportError, match="cbor2"):
            mod.cbor_load_file(path)


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

class TestCborRegistration:
    def test_functions_registered_in_runtime(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib.serialization import register_serialization_functions

        rt = Runtime()
        register_serialization_functions(rt)

        for name in ("cbor_dumps", "cbor_loads", "cbor_dump_file", "cbor_load_file"):
            assert name in rt.functions, f"{name} not registered"
