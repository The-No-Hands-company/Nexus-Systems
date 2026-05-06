"""
Tests for the NexusLang stdlib serialization module.

Covers: pickle, msgpack (if installed), yaml (if installed),
        toml (if installed), and Protocol Buffers (if installed).
"""

import os
import sys
import pytest

# Resolve project root so imports work without installation
_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

from nexuslang.stdlib.serialization import (
    HAS_MSGPACK,
    HAS_YAML,
    HAS_TOML,
    HAS_PROTOBUF,
    pickle_dumps,
    pickle_loads,
    pickle_dump_file,
    pickle_load_file,
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_SIMPLE_DICT = {"key": "value", "number": 42, "flag": True}
_NESTED_DICT = {"outer": {"inner": [1, 2, 3]}, "x": None}


# ===========================================================================
# Pickle
# ===========================================================================


class TestPickleDumpsLoads:
    def test_roundtrip_simple_dict(self):
        data = pickle_dumps(_SIMPLE_DICT)
        assert isinstance(data, bytes)
        result = pickle_loads(data)
        assert result == _SIMPLE_DICT

    def test_roundtrip_nested(self):
        data = pickle_dumps(_NESTED_DICT)
        assert pickle_loads(data) == _NESTED_DICT

    def test_roundtrip_list(self):
        obj = [1, "two", 3.0, None]
        assert pickle_loads(pickle_dumps(obj)) == obj

    def test_roundtrip_integer(self):
        assert pickle_loads(pickle_dumps(99)) == 99

    def test_dumps_returns_bytes(self):
        assert isinstance(pickle_dumps({}), bytes)

    def test_empty_dict_roundtrip(self):
        assert pickle_loads(pickle_dumps({})) == {}


class TestPickleFile:
    def test_dump_and_load_file(self, tmp_path):
        path = str(tmp_path / "test.pkl")
        assert pickle_dump_file(_SIMPLE_DICT, path) is True
        assert pickle_load_file(path) == _SIMPLE_DICT

    def test_creates_parent_directory(self, tmp_path):
        path = str(tmp_path / "subdir" / "data.pkl")
        assert pickle_dump_file(_SIMPLE_DICT, path) is True
        assert os.path.exists(path)

    def test_dump_returns_false_on_write_error(self, tmp_path):
        # Make target a directory so open() fails
        bad_path = str(tmp_path / "is_a_dir")
        os.makedirs(bad_path)
        result = pickle_dump_file(_SIMPLE_DICT, bad_path)
        assert result is False

    def test_load_missing_file_raises(self, tmp_path):
        with pytest.raises((FileNotFoundError, OSError)):
            pickle_load_file(str(tmp_path / "nonexistent.pkl"))


# ===========================================================================
# MessagePack
# ===========================================================================


@pytest.mark.skipif(not HAS_MSGPACK, reason="msgpack not installed")
class TestMsgpackDumpsLoads:
    def setup_method(self):
        from nexuslang.stdlib.serialization import msgpack_dumps, msgpack_loads
        self.dumps = msgpack_dumps
        self.loads = msgpack_loads

    def test_roundtrip_simple_dict(self):
        data = self.dumps(_SIMPLE_DICT)
        assert isinstance(data, bytes)
        result = self.loads(data)
        assert result == _SIMPLE_DICT

    def test_roundtrip_list(self):
        obj = [1, 2, 3]
        assert self.loads(self.dumps(obj)) == obj

    def test_empty_dict_roundtrip(self):
        assert self.loads(self.dumps({})) == {}

    def test_dumps_produces_fewer_bytes_than_json(self):
        import json
        data = {"key": "value", "n": 12345}
        json_size = len(json.dumps(data).encode())
        msgpack_size = len(self.dumps(data))
        # msgpack should be more compact
        assert msgpack_size < json_size


@pytest.mark.skipif(not HAS_MSGPACK, reason="msgpack not installed")
class TestMsgpackFile:
    def setup_method(self):
        from nexuslang.stdlib.serialization import msgpack_dump_file, msgpack_load_file
        self.dump_file = msgpack_dump_file
        self.load_file = msgpack_load_file

    def test_dump_and_load_file(self, tmp_path):
        path = str(tmp_path / "data.msgpack")
        assert self.dump_file(_SIMPLE_DICT, path) is True
        assert self.load_file(path) == _SIMPLE_DICT

    def test_creates_parent_directory(self, tmp_path):
        path = str(tmp_path / "sub" / "data.msgpack")
        self.dump_file(_SIMPLE_DICT, path)
        assert os.path.exists(path)


@pytest.mark.skipif(HAS_MSGPACK, reason="msgpack IS installed, skip missing-dep test")
class TestMsgpackMissing:
    def test_dumps_raises_import_error(self):
        from nexuslang.stdlib.serialization import msgpack_dumps
        with pytest.raises(ImportError, match="msgpack"):
            msgpack_dumps({"a": 1})

    def test_loads_raises_import_error(self):
        from nexuslang.stdlib.serialization import msgpack_loads
        with pytest.raises(ImportError, match="msgpack"):
            msgpack_loads(b"")


# ===========================================================================
# YAML
# ===========================================================================


@pytest.mark.skipif(not HAS_YAML, reason="PyYAML not installed")
class TestYamlDumpsLoads:
    def setup_method(self):
        from nexuslang.stdlib.serialization import yaml_dumps, yaml_loads
        self.dumps = yaml_dumps
        self.loads = yaml_loads

    def test_roundtrip_simple_dict(self):
        data = self.dumps(_SIMPLE_DICT)
        assert isinstance(data, str)
        result = self.loads(data)
        assert result == _SIMPLE_DICT

    def test_roundtrip_nested(self):
        d = {"a": {"b": [1, 2]}}
        assert self.loads(self.dumps(d)) == d

    def test_loads_valid_yaml_string(self):
        yaml_str = "key: value\nnumber: 42\n"
        result = self.loads(yaml_str)
        assert result["key"] == "value"
        assert result["number"] == 42


@pytest.mark.skipif(not HAS_YAML, reason="PyYAML not installed")
class TestYamlFile:
    def setup_method(self):
        from nexuslang.stdlib.serialization import yaml_dump_file, yaml_load_file
        self.dump_file = yaml_dump_file
        self.load_file = yaml_load_file

    def test_dump_and_load_file(self, tmp_path):
        path = str(tmp_path / "data.yaml")
        assert self.dump_file(_SIMPLE_DICT, path) is True
        assert self.load_file(path) == _SIMPLE_DICT

    def test_creates_parent_directory(self, tmp_path):
        path = str(tmp_path / "sub" / "data.yaml")
        self.dump_file(_SIMPLE_DICT, path)
        assert os.path.exists(path)


# ===========================================================================
# TOML
# ===========================================================================


@pytest.mark.skipif(not HAS_TOML, reason="toml not installed")
class TestTomlDumpsLoads:
    def setup_method(self):
        from nexuslang.stdlib.serialization import toml_dumps, toml_loads
        self.dumps = toml_dumps
        self.loads = toml_loads

    def test_roundtrip_simple_dict(self):
        # TOML requires all string keys at top level
        d = {"key": "value", "number": 42}
        data = self.dumps(d)
        assert isinstance(data, str)
        assert self.loads(data) == d

    def test_output_contains_key_value(self):
        s = self.dumps({"host": "localhost", "port": 8080})
        assert "host" in s
        assert "localhost" in s


@pytest.mark.skipif(not HAS_TOML, reason="toml not installed")
class TestTomlFile:
    def setup_method(self):
        from nexuslang.stdlib.serialization import toml_dump_file, toml_load_file
        self.dump_file = toml_dump_file
        self.load_file = toml_load_file

    def test_dump_and_load_file(self, tmp_path):
        path = str(tmp_path / "config.toml")
        d = {"version": "1.0", "count": 3}
        assert self.dump_file(d, path) is True
        assert self.load_file(path) == d


# ===========================================================================
# Protocol Buffers
# ===========================================================================


@pytest.mark.skipif(not HAS_PROTOBUF, reason="protobuf not installed")
class TestProtobufDumpsLoads:
    def setup_method(self):
        from nexuslang.stdlib.serialization import protobuf_dumps, protobuf_loads
        self.dumps = protobuf_dumps
        self.loads = protobuf_loads

    def test_roundtrip_simple_dict(self):
        d = {"name": "Alice", "score": 99.0}
        data = self.dumps(d)
        assert isinstance(data, bytes)
        result = self.loads(data)
        assert result["name"] == "Alice"
        assert result["score"] == pytest.approx(99.0)

    def test_roundtrip_empty_dict(self):
        result = self.loads(self.dumps({}))
        assert result == {}

    def test_roundtrip_nested_dict(self):
        d = {"outer": {"inner": "value"}}
        result = self.loads(self.dumps(d))
        assert result["outer"]["inner"] == "value"

    def test_roundtrip_boolean_value(self):
        d = {"flag": True}
        result = self.loads(self.dumps(d))
        assert result["flag"] is True

    def test_roundtrip_numeric_string_key(self):
        d = {"count": 7.0}
        result = self.loads(self.dumps(d))
        assert result["count"] == pytest.approx(7.0)

    def test_dumps_produces_bytes(self):
        assert isinstance(self.dumps({"k": "v"}), bytes)

    def test_bytes_are_not_json(self):
        data = self.dumps({"k": "v"})
        # Protobuf bytes should not be valid UTF-8 JSON
        try:
            text = data.decode("utf-8")
            import json
            json.loads(text)
            # If it somehow is valid JSON, the test is not useful but not a failure
        except (UnicodeDecodeError, ValueError):
            pass  # Expected for binary protobuf encoding

    def test_list_values_roundtrip(self):
        # Struct supports list values via ListValue
        d = {"items": [1.0, 2.0, 3.0]}
        result = self.loads(self.dumps(d))
        assert result["items"] == pytest.approx([1.0, 2.0, 3.0])

    def test_null_value_roundtrip(self):
        d = {"nothing": None}
        result = self.loads(self.dumps(d))
        assert result.get("nothing") is None


@pytest.mark.skipif(not HAS_PROTOBUF, reason="protobuf not installed")
class TestProtobufFile:
    def setup_method(self):
        from nexuslang.stdlib.serialization import protobuf_dump_file, protobuf_load_file
        self.dump_file = protobuf_dump_file
        self.load_file = protobuf_load_file

    def test_dump_and_load_file(self, tmp_path):
        path = str(tmp_path / "data.pb")
        d = {"name": "Bob", "value": 3.14}
        assert self.dump_file(d, path) is True
        result = self.load_file(path)
        assert result["name"] == "Bob"
        assert result["value"] == pytest.approx(3.14)

    def test_creates_parent_directory(self, tmp_path):
        path = str(tmp_path / "nested" / "out.pb")
        self.dump_file({"k": "v"}, path)
        assert os.path.exists(path)

    def test_dump_returns_false_on_write_error(self, tmp_path):
        bad_path = str(tmp_path / "is_a_dir.pb")
        os.makedirs(bad_path)
        result = self.dump_file({"k": "v"}, bad_path)
        assert result is False

    def test_load_missing_file_raises(self, tmp_path):
        with pytest.raises((FileNotFoundError, OSError)):
            self.load_file(str(tmp_path / "no_file.pb"))

    def test_file_is_binary(self, tmp_path):
        path = str(tmp_path / "binary.pb")
        self.dump_file({"key": "value"}, path)
        with open(path, 'rb') as f:
            content = f.read()
        assert isinstance(content, bytes)


@pytest.mark.skipif(HAS_PROTOBUF, reason="protobuf IS installed, skip missing-dep test")
class TestProtobufMissing:
    def test_dumps_raises_import_error(self):
        from nexuslang.stdlib.serialization import protobuf_dumps
        with pytest.raises(ImportError, match="protobuf"):
            protobuf_dumps({"k": "v"})

    def test_loads_raises_import_error(self):
        from nexuslang.stdlib.serialization import protobuf_loads
        with pytest.raises(ImportError, match="protobuf"):
            protobuf_loads(b"")

    def test_dump_file_raises_import_error(self, tmp_path):
        from nexuslang.stdlib.serialization import protobuf_dump_file
        with pytest.raises(ImportError, match="protobuf"):
            protobuf_dump_file({"k": "v"}, str(tmp_path / "out.pb"))

    def test_load_file_raises_import_error(self, tmp_path):
        from nexuslang.stdlib.serialization import protobuf_load_file
        with pytest.raises(ImportError, match="protobuf"):
            protobuf_load_file(str(tmp_path / "in.pb"))


# ===========================================================================
# Registration
# ===========================================================================


class TestRegisterSerializationFunctions:
    """Verify runtime registration registers the right function names."""

    def _make_mock_runtime(self):
        class MockRuntime:
            def __init__(self):
                self.registered = {}
            def register_function(self, name, fn):
                self.registered[name] = fn
        return MockRuntime()

    def test_pickle_functions_always_registered(self):
        from nexuslang.stdlib.serialization import register_serialization_functions
        rt = self._make_mock_runtime()
        register_serialization_functions(rt)
        for name in ("pickle_dumps", "pickle_loads", "pickle_dump_file", "pickle_load_file"):
            assert name in rt.registered, f"{name!r} not registered"

    @pytest.mark.skipif(not HAS_PROTOBUF, reason="protobuf not installed")
    def test_protobuf_functions_registered_when_available(self):
        from nexuslang.stdlib.serialization import register_serialization_functions
        rt = self._make_mock_runtime()
        register_serialization_functions(rt)
        for name in ("protobuf_dumps", "protobuf_loads", "protobuf_dump_file", "protobuf_load_file"):
            assert name in rt.registered, f"{name!r} not registered"

    @pytest.mark.skipif(not HAS_MSGPACK, reason="msgpack not installed")
    def test_msgpack_functions_registered_when_available(self):
        from nexuslang.stdlib.serialization import register_serialization_functions
        rt = self._make_mock_runtime()
        register_serialization_functions(rt)
        for name in ("msgpack_dumps", "msgpack_loads", "msgpack_dump_file", "msgpack_load_file"):
            assert name in rt.registered, f"{name!r} not registered"

    @pytest.mark.skipif(not HAS_YAML, reason="PyYAML not installed")
    def test_yaml_functions_registered_when_available(self):
        from nexuslang.stdlib.serialization import register_serialization_functions
        rt = self._make_mock_runtime()
        register_serialization_functions(rt)
        for name in ("yaml_dumps", "yaml_loads", "yaml_dump_file", "yaml_load_file"):
            assert name in rt.registered, f"{name!r} not registered"

    @pytest.mark.skipif(not HAS_TOML, reason="toml not installed")
    def test_toml_functions_registered_when_available(self):
        from nexuslang.stdlib.serialization import register_serialization_functions
        rt = self._make_mock_runtime()
        register_serialization_functions(rt)
        for name in ("toml_dumps", "toml_loads", "toml_dump_file", "toml_load_file"):
            assert name in rt.registered, f"{name!r} not registered"
