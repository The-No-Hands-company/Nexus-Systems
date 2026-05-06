"""
UUID generation for NexusLang.
"""

import uuid
from ...runtime.runtime import Runtime


def uuid4() -> str:
    """Generate random UUID (version 4)."""
    return str(uuid.uuid4())


def uuid1() -> str:
    """Generate UUID based on host and time (version 1)."""
    return str(uuid.uuid1())


def uuid3(namespace: str, name: str) -> str:
    """Generate UUID using MD5 hash (version 3)."""
    ns = uuid.NAMESPACE_DNS if namespace == "dns" else uuid.UUID(namespace)
    return str(uuid.uuid3(ns, name))


def uuid5(namespace: str, name: str) -> str:
    """Generate UUID using SHA-1 hash (version 5)."""
    ns = uuid.NAMESPACE_DNS if namespace == "dns" else uuid.UUID(namespace)
    return str(uuid.uuid5(ns, name))


def is_valid_uuid(uuid_string: str) -> bool:
    """Check if string is valid UUID."""
    try:
        uuid.UUID(uuid_string)
        return True
    except (ValueError, AttributeError):
        return False


def register_uuid_functions(runtime: Runtime) -> None:
    """Register UUID functions with the runtime."""
    runtime.register_function("uuid4", uuid4)
    runtime.register_function("uuid1", uuid1)
    runtime.register_function("uuid3", uuid3)
    runtime.register_function("uuid5", uuid5)
    runtime.register_function("is_valid_uuid", is_valid_uuid)
    
    # Aliases
    runtime.register_function("generate_uuid", uuid4)
    runtime.register_function("new_uuid", uuid4)
