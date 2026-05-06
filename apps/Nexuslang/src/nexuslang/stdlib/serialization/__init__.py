"""
Serialization utilities for NexusLang.

Provides:
- Pickle serialization (Python objects)
- JSON serialization (already in json_utils)
- MessagePack serialization (binary JSON)
- YAML serialization
- TOML serialization
- Protocol Buffers serialization (google.protobuf Struct encoding)
- CBOR serialization (Concise Binary Object Representation — RFC 7049/8949)
"""

import pickle
import json
from typing import Any
import os

# Optional imports with fallbacks
try:
    import msgpack
    HAS_MSGPACK = True
except ImportError:
    HAS_MSGPACK = False

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False

try:
    import toml
    HAS_TOML = True
except ImportError:
    HAS_TOML = False

try:
    from google.protobuf.struct_pb2 import Struct as _PbStruct
    from google.protobuf.json_format import MessageToDict as _pb_to_dict, ParseDict as _pb_parse_dict
    HAS_PROTOBUF = True
except ImportError:
    HAS_PROTOBUF = False

try:
    import cbor2
    HAS_CBOR = True
except ImportError:
    HAS_CBOR = False


def pickle_dumps(obj: Any) -> bytes:
    """Serialize object to bytes using pickle.
    
    Args:
        obj: Object to serialize
        
    Returns:
        Serialized bytes
    """
    return pickle.dumps(obj)


def pickle_loads(data: bytes) -> Any:
    """Deserialize object from bytes using pickle.
    
    Args:
        data: Serialized bytes
        
    Returns:
        Deserialized object
    """
    return pickle.loads(data)


def pickle_dump_file(obj: Any, filepath: str) -> bool:
    """Serialize object to file using pickle.
    
    Args:
        obj: Object to serialize
        filepath: Path to output file
        
    Returns:
        True if successful
    """
    try:
        # Create directory if it doesn't exist
        os.makedirs(os.path.dirname(os.path.abspath(filepath)), exist_ok=True)
        
        with open(filepath, 'wb') as f:
            pickle.dump(obj, f)
        return True
    except Exception:
        return False


def pickle_load_file(filepath: str) -> Any:
    """Deserialize object from file using pickle.
    
    Args:
        filepath: Path to input file
        
    Returns:
        Deserialized object
    """
    with open(filepath, 'rb') as f:
        return pickle.load(f)


def msgpack_dumps(obj: Any) -> bytes:
    """Serialize object to bytes using MessagePack.
    
    Args:
        obj: Object to serialize
        
    Returns:
        Serialized bytes
    """
    if not HAS_MSGPACK:
        raise ImportError("msgpack is not installed. Install with: pip install msgpack")
    
    return msgpack.packb(obj, use_bin_type=True)


def msgpack_loads(data: bytes) -> Any:
    """Deserialize object from bytes using MessagePack.
    
    Args:
        data: Serialized bytes
        
    Returns:
        Deserialized object
    """
    if not HAS_MSGPACK:
        raise ImportError("msgpack is not installed. Install with: pip install msgpack")
    
    return msgpack.unpackb(data, raw=False)


def msgpack_dump_file(obj: Any, filepath: str) -> bool:
    """Serialize object to file using MessagePack.
    
    Args:
        obj: Object to serialize
        filepath: Path to output file
        
    Returns:
        True if successful
    """
    if not HAS_MSGPACK:
        raise ImportError("msgpack is not installed. Install with: pip install msgpack")
    
    try:
        # Create directory if it doesn't exist
        os.makedirs(os.path.dirname(os.path.abspath(filepath)), exist_ok=True)
        
        with open(filepath, 'wb') as f:
            msgpack.pack(obj, f, use_bin_type=True)
        return True
    except Exception:
        return False


def msgpack_load_file(filepath: str) -> Any:
    """Deserialize object from file using MessagePack.
    
    Args:
        filepath: Path to input file
        
    Returns:
        Deserialized object
    """
    if not HAS_MSGPACK:
        raise ImportError("msgpack is not installed. Install with: pip install msgpack")
    
    with open(filepath, 'rb') as f:
        return msgpack.unpack(f, raw=False)


def yaml_dumps(obj: Any) -> str:
    """Serialize object to YAML string.
    
    Args:
        obj: Object to serialize
        
    Returns:
        YAML string
    """
    if not HAS_YAML:
        raise ImportError("PyYAML is not installed. Install with: pip install pyyaml")
    
    return yaml.dump(obj, default_flow_style=False)


def yaml_loads(data: str) -> Any:
    """Deserialize object from YAML string.
    
    Args:
        data: YAML string
        
    Returns:
        Deserialized object
    """
    if not HAS_YAML:
        raise ImportError("PyYAML is not installed. Install with: pip install pyyaml")
    
    return yaml.safe_load(data)


def yaml_dump_file(obj: Any, filepath: str) -> bool:
    """Serialize object to YAML file.
    
    Args:
        obj: Object to serialize
        filepath: Path to output file
        
    Returns:
        True if successful
    """
    if not HAS_YAML:
        raise ImportError("PyYAML is not installed. Install with: pip install pyyaml")
    
    try:
        # Create directory if it doesn't exist
        os.makedirs(os.path.dirname(os.path.abspath(filepath)), exist_ok=True)
        
        with open(filepath, 'w') as f:
            yaml.dump(obj, f, default_flow_style=False)
        return True
    except Exception:
        return False


def yaml_load_file(filepath: str) -> Any:
    """Deserialize object from YAML file.
    
    Args:
        filepath: Path to input file
        
    Returns:
        Deserialized object
    """
    if not HAS_YAML:
        raise ImportError("PyYAML is not installed. Install with: pip install pyyaml")
    
    with open(filepath, 'r') as f:
        return yaml.safe_load(f)


def toml_dumps(obj: dict) -> str:
    """Serialize dictionary to TOML string.
    
    Args:
        obj: Dictionary to serialize
        
    Returns:
        TOML string
    """
    if not HAS_TOML:
        raise ImportError("toml is not installed. Install with: pip install toml")
    
    return toml.dumps(obj)


def toml_loads(data: str) -> dict:
    """Deserialize dictionary from TOML string.
    
    Args:
        data: TOML string
        
    Returns:
        Deserialized dictionary
    """
    if not HAS_TOML:
        raise ImportError("toml is not installed. Install with: pip install toml")
    
    return toml.loads(data)


def toml_dump_file(obj: dict, filepath: str) -> bool:
    """Serialize dictionary to TOML file.
    
    Args:
        obj: Dictionary to serialize
        filepath: Path to output file
        
    Returns:
        True if successful
    """
    if not HAS_TOML:
        raise ImportError("toml is not installed. Install with: pip install toml")
    
    try:
        # Create directory if it doesn't exist
        os.makedirs(os.path.dirname(os.path.abspath(filepath)), exist_ok=True)
        
        with open(filepath, 'w') as f:
            toml.dump(obj, f)
        return True
    except Exception:
        return False


def toml_load_file(filepath: str) -> dict:
    """Deserialize dictionary from TOML file.
    
    Args:
        filepath: Path to input file
        
    Returns:
        Deserialized dictionary
    """
    if not HAS_TOML:
        raise ImportError("toml is not installed. Install with: pip install toml")
    
    with open(filepath, 'r') as f:
        return toml.load(f)


def protobuf_dumps(obj: dict) -> bytes:
    """Serialize dictionary to Protocol Buffers bytes using Struct encoding.

    Args:
        obj: Dictionary to serialize (values must be JSON-compatible types)

    Returns:
        Serialized bytes
    """
    if not HAS_PROTOBUF:
        raise ImportError("protobuf is not installed. Install with: pip install protobuf")
    s = _PbStruct()
    _pb_parse_dict(obj, s)
    return s.SerializeToString()


def protobuf_loads(data: bytes) -> dict:
    """Deserialize dictionary from Protocol Buffers bytes.

    Args:
        data: Serialized bytes produced by protobuf_dumps

    Returns:
        Deserialized dictionary
    """
    if not HAS_PROTOBUF:
        raise ImportError("protobuf is not installed. Install with: pip install protobuf")
    s = _PbStruct()
    s.ParseFromString(data)
    return _pb_to_dict(s)


def protobuf_dump_file(obj: dict, filepath: str) -> bool:
    """Serialize dictionary to Protocol Buffers binary file.

    Args:
        obj: Dictionary to serialize
        filepath: Path to output file

    Returns:
        True on success, False on failure
    """
    if not HAS_PROTOBUF:
        raise ImportError("protobuf is not installed. Install with: pip install protobuf")
    try:
        os.makedirs(os.path.dirname(os.path.abspath(filepath)), exist_ok=True)
        with open(filepath, 'wb') as f:
            f.write(protobuf_dumps(obj))
        return True
    except Exception:
        return False


def protobuf_load_file(filepath: str) -> dict:
    """Deserialize dictionary from Protocol Buffers binary file.

    Args:
        filepath: Path to input file

    Returns:
        Deserialized dictionary
    """
    if not HAS_PROTOBUF:
        raise ImportError("protobuf is not installed. Install with: pip install protobuf")
    with open(filepath, 'rb') as f:
        return protobuf_loads(f.read())


# ---------------------------------------------------------------------------
# CBOR serialization (Concise Binary Object Representation — RFC 7049 / 8949)
# cbor2 supports: None, bool, int, float, str, bytes, list, dict, datetime,
#                 Decimal, UUID, sets, and all other CBOR major types.
# ---------------------------------------------------------------------------

def cbor_dumps(obj: Any) -> bytes:
    """Serialize a value to CBOR bytes (RFC 8949).

    Args:
        obj: Any CBOR-serializable value (None, bool, int, float, str,
             bytes, list, dict, datetime, Decimal, UUID, set, …).

    Returns:
        CBOR-encoded bytes.

    Raises:
        ImportError: If the cbor2 package is not installed.
    """
    if not HAS_CBOR:
        raise ImportError("cbor2 is not installed. Install with: pip install cbor2")
    return cbor2.dumps(obj)


def cbor_loads(data: bytes) -> Any:
    """Deserialize a value from CBOR bytes.

    Args:
        data: CBOR-encoded bytes produced by cbor_dumps or any CBOR encoder.

    Returns:
        Decoded Python value.

    Raises:
        ImportError: If the cbor2 package is not installed.
        cbor2.CBORDecodeError: If the input is malformed.
    """
    if not HAS_CBOR:
        raise ImportError("cbor2 is not installed. Install with: pip install cbor2")
    return cbor2.loads(data)


def cbor_dump_file(obj: Any, filepath: str) -> bool:
    """Serialize a value and write CBOR bytes to a file.

    Args:
        obj: Value to serialize.
        filepath: Destination file path.

    Returns:
        True on success.

    Raises:
        ImportError: If the cbor2 package is not installed.
        OSError: If the file cannot be written.
    """
    if not HAS_CBOR:
        raise ImportError("cbor2 is not installed. Install with: pip install cbor2")
    try:
        os.makedirs(os.path.dirname(os.path.abspath(filepath)), exist_ok=True)
        with open(filepath, 'wb') as f:
            cbor2.dump(obj, f)
        return True
    except OSError as exc:
        raise OSError(f"cbor_dump_file: cannot write '{filepath}': {exc}") from exc


def cbor_load_file(filepath: str) -> Any:
    """Read CBOR bytes from a file and deserialize.

    Args:
        filepath: Path to a CBOR-encoded file.

    Returns:
        Decoded Python value.

    Raises:
        ImportError: If the cbor2 package is not installed.
        OSError: If the file cannot be read.
        cbor2.CBORDecodeError: If the file contents are malformed.
    """
    if not HAS_CBOR:
        raise ImportError("cbor2 is not installed. Install with: pip install cbor2")
    with open(filepath, 'rb') as f:
        return cbor2.load(f)


def register_serialization_functions(runtime):
    """Register serialization functions with the NexusLang runtime."""
    # Pickle serialization
    runtime.register_function("pickle_dumps", pickle_dumps)
    runtime.register_function("pickle_loads", pickle_loads)
    runtime.register_function("pickle_dump_file", pickle_dump_file)
    runtime.register_function("pickle_load_file", pickle_load_file)

    # MessagePack serialization (if available)
    if HAS_MSGPACK:
        runtime.register_function("msgpack_dumps", msgpack_dumps)
        runtime.register_function("msgpack_loads", msgpack_loads)
        runtime.register_function("msgpack_dump_file", msgpack_dump_file)
        runtime.register_function("msgpack_load_file", msgpack_load_file)

    # YAML serialization (if available)
    if HAS_YAML:
        runtime.register_function("yaml_dumps", yaml_dumps)
        runtime.register_function("yaml_loads", yaml_loads)
        runtime.register_function("yaml_dump_file", yaml_dump_file)
        runtime.register_function("yaml_load_file", yaml_load_file)

    # TOML serialization (if available)
    if HAS_TOML:
        runtime.register_function("toml_dumps", toml_dumps)
        runtime.register_function("toml_loads", toml_loads)
        runtime.register_function("toml_dump_file", toml_dump_file)
        runtime.register_function("toml_load_file", toml_load_file)

    # Protocol Buffers serialization (if available)
    if HAS_PROTOBUF:
        runtime.register_function("protobuf_dumps", protobuf_dumps)
        runtime.register_function("protobuf_loads", protobuf_loads)
        runtime.register_function("protobuf_dump_file", protobuf_dump_file)
        runtime.register_function("protobuf_load_file", protobuf_load_file)

    # CBOR serialization (if available)
    if HAS_CBOR:
        runtime.register_function("cbor_dumps", cbor_dumps)
        runtime.register_function("cbor_loads", cbor_loads)
        runtime.register_function("cbor_dump_file", cbor_dump_file)
        runtime.register_function("cbor_load_file", cbor_load_file)
