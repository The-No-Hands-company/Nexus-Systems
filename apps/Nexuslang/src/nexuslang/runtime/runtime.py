"""
Runtime module for the NexusLang interpreter.
Provides the execution environment for NexusLang programs.
"""

import time
import threading
import concurrent.futures
import uuid
from typing import Any, Dict, List, Callable, Optional, Set, Union
from nexuslang.runtime.memory import MemoryManager, MemoryAddress

class MemoryPointer:
    """Represents a pointer to memory."""
    
    def __init__(self, address: str):
        self.address = address
    
    def __str__(self):
        return f"MemoryPointer({self.address})"
    
    def __repr__(self):
        return self.__str__()

class Object:
    """Represents an object instance."""
    
    def __init__(self, class_name: str, properties: Dict[str, Any] = None, type_arguments: Dict[str, Any] = None):
        self.class_name = class_name
        self.properties = properties or {}
        self.type_arguments = type_arguments or {}
        self.methods = {}
        self._drop_method: Optional[Callable] = None
    
    def get_property(self, name: str) -> Any:
        """Get a property value."""
        if name not in self.properties:
            raise AttributeError(f"Object of type '{self.class_name}' has no property '{name}'")
        return self.properties[name]
    
    def set_property(self, name: str, value: Any) -> None:
        """Set a property value."""
        self.properties[name] = value
    
    def add_method(self, name: str, func: Callable) -> None:
        """Add a method to the object."""
        self.methods[name] = func
    
    def invoke_method(self, name: str, *args, **kwargs) -> Any:
        """Invoke a method on the object."""
        if name not in self.methods:
            raise AttributeError(f"Object of type '{self.class_name}' has no method '{name}'")
        return self.methods[name](self, *args, **kwargs)
    
    def __str__(self):
        return f"{self.class_name}({', '.join(f'{k}={v}' for k, v in self.properties.items())})"
    
    def __repr__(self):
        return self.__str__()

class Runtime:
    """Provides the execution environment for NexusLang programs."""
    
    def __init__(self):
        self.memory = {}
        self.functions = {}
        self.constants = {}  # Store global constants (PI, E, etc.)
        self.modules = {}
        self.registered_modules = set()
        self.executor = concurrent.futures.ThreadPoolExecutor()
        # Low-level memory manager for pointer operations
        self.memory_manager = MemoryManager()
        # Attribute metadata storage: class_name -> {attr_name -> {prop_name: value}}
        self._class_attributes: dict = {}
        # Function attribute metadata: func_name -> {attr_name -> {prop_name: value}}
        self._function_attributes: dict = {}
    
    def allocate(self, value: Any) -> MemoryPointer:
        """Allocate memory for a value and return a pointer to it."""
        address = str(uuid.uuid4())
        self.memory[address] = value
        return MemoryPointer(address)
    
    def free(self, pointer: MemoryPointer) -> bool:
        """Free memory associated with a pointer."""
        if pointer.address in self.memory:
            del self.memory[pointer.address]
            return True
        return False
    
    def get(self, pointer: MemoryPointer) -> Any:
        """Get the value associated with a pointer."""
        if pointer.address not in self.memory:
            raise RuntimeError(f"Invalid memory access: {pointer.address}")
        return self.memory[pointer.address]
    
    def set(self, pointer: MemoryPointer, value: Any) -> None:
        """Set the value associated with a pointer."""
        if pointer.address not in self.memory:
            raise RuntimeError(f"Invalid memory access: {pointer.address}")
        self.memory[pointer.address] = value
    
    def create_object(self, class_name: str, properties: Dict[str, Any] = None, type_arguments: Dict[str, Any] = None) -> Object:
        """Create a new object instance."""
        return Object(class_name, properties, type_arguments)
    
    def register_function(self, name: str, func: Callable) -> None:
        """Register a function with the runtime."""
        self.functions[name] = func

    def get_function(self, name: str):
        """Retrieve a registered function by name. Returns None if not found."""
        return self.functions.get(name)
    
    def register_global(self, name: str, value: Any) -> None:
        """Register a global constant or variable with the runtime."""
        self.functions[name] = lambda: value
    
    def register_constant(self, name: str, value: Any) -> None:
        """Register a constant with the runtime."""
        self.constants[name] = value
    
    def register_module(self, name: str) -> None:
        """Register a module name for importing."""
        self.registered_modules.add(name)
    
    def import_module(self, name: str) -> Dict[str, Callable]:
        """Import a module and return its functions."""
        if name not in self.registered_modules:
            raise ImportError(f"No module named '{name}'")
        
        # If the module is already imported, return it
        if name in self.modules:
            return self.modules[name]
        
        # Create a new module namespace
        module = {}
        
        # Find all functions that belong to this module
        prefix = f"{name}_"
        for func_name, func in self.functions.items():
            # If the function is directly in the module (like math.sin)
            if func_name.startswith(prefix):
                module_func_name = func_name[len(prefix):]
                module[module_func_name] = func
        
        # Store the module for future imports
        self.modules[name] = module
        return module
    
    def invoke_function(self, name: str, *args, **kwargs) -> Any:
        """Invoke a registered function."""
        if name not in self.functions:
            raise RuntimeError(f"Function '{name}' is not defined")
        return self.functions[name](*args, **kwargs)
    
    def invoke_method(self, obj: Object, method_name: str, *args, **kwargs) -> Any:
        """Invoke a method on an object."""
        return obj.invoke_method(method_name, *args, **kwargs)
    
    def run_concurrent(self, func: Callable, *args, **kwargs) -> concurrent.futures.Future:
        """Run a function concurrently and return a Future."""
        return self.executor.submit(func, *args, **kwargs)
    
    def wait_for_futures(self, futures: List[concurrent.futures.Future]) -> List[Any]:
        """Wait for all futures to complete and return their results."""
        return [future.result() for future in concurrent.futures.as_completed(futures)]
    
    # Built-in functions
    
    def print(self, *args, sep=' ', end='\n'):
        """Print values to the console."""
        print(*args, sep=sep, end=end)
    
    def current_time_millis(self):
        """Get the current time in milliseconds."""
        return int(time.time() * 1000)
    
    def sleep(self, milliseconds):
        """Sleep for the specified number of milliseconds."""
        time.sleep(milliseconds / 1000)
    
    def to_int(self, value):
        """Convert a value to an integer."""
        return int(value)
    
    def to_float(self, value):
        """Convert a value to a float."""
        return float(value)
    
    def to_string(self, value):
        """Convert a value to a string."""
        return str(value)
    
    def to_bool(self, value):
        """Convert a value to a boolean."""
        return bool(value)
    
    def random(self):
        """Generate a random number between 0 and 1."""
        import random
        return random.random()
    
    def random_int(self, min_val, max_val):
        """Generate a random integer between min_val and max_val (inclusive)."""
        import random
        return random.randint(min_val, max_val)