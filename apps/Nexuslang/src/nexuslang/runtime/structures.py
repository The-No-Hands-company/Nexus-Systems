"""
Runtime support for low-level Structs and Unions in NexusLang.

This module implements:
- Memory layout calculation (size, alignment, padding)
- Byte-array backed storage for Structs and Unions
- Type conversion to/from bytes using Python's `struct` module
"""

import struct
from typing import Dict, List, Any, Tuple, Optional, Union
from enum import Enum

class PrimitiveType(Enum):
    INTEGER = 'q'  # 8 bytes (int64)
    FLOAT = 'd'    # 8 bytes (double)
    BOOLEAN = '?'  # 1 byte
    BYTE = 'b'     # 1 byte
    CHAR = 'c'     # 1 byte
    POINTER = 'Q'  # 8 bytes (unsigned int64 for IDs/addresses)

    @property
    def size(self) -> int:
        return struct.calcsize(self.value)

class Field:
    def __init__(
        self,
        name: str,
        type_code: str,
        offset: int,
        size: int,
        bit_width: Optional[int] = None,
        bit_offset: int = 0,
    ):
        self.name = name
        self.type_code = type_code
        self.offset = offset
        self.size = size
        self.bit_width = bit_width
        self.bit_offset = bit_offset

class StructureDefinition:
    """Base class for Struct and Union definitions."""
    def __init__(
        self,
        name: str,
        fields: List[Union[Tuple[str, str], Tuple[str, str, Optional[int]]]],
        packed: bool = False,
        alignment: Optional[int] = None,
    ):
        self.name = name
        self.fields: Dict[str, Field] = {}
        self.size = 0
        self.alignment = 1
        self.packed = packed  # Whether to disable padding
        self.explicit_alignment = alignment  # Explicit alignment override
        self._calculate_layout(fields)

    def _get_type_info(self, type_name: str) -> Tuple[str, int, int]:
        """Returns (struct_format_char, size, alignment) for a given type name."""
        type_name = type_name.lower()
        if type_name in ('integer', 'int'):
            return PrimitiveType.INTEGER.value, 8, 8
        elif type_name in ('float', 'double'):
            return PrimitiveType.FLOAT.value, 8, 8
        elif type_name in ('boolean', 'bool'):
            return PrimitiveType.BOOLEAN.value, 1, 1
        elif type_name == 'byte':
            return PrimitiveType.BYTE.value, 1, 1
        elif type_name == 'char':
            return PrimitiveType.CHAR.value, 1, 1
        else:
            # Treat everything else as a pointer/reference (Object ID)
            return PrimitiveType.POINTER.value, 8, 8

    def _calculate_layout(self, fields: List[Union[Tuple[str, str], Tuple[str, str, Optional[int]]]]):
        raise NotImplementedError

class StructDefinition(StructureDefinition):
    """
    Defines a C-style struct layout including padding.
    Supports packed structs (no padding) and explicit alignment.
    """
    def _calculate_layout(self, fields: List[Union[Tuple[str, str], Tuple[str, str, Optional[int]]]]):
        current_offset = 0
        max_alignment = 1

        pending_bit_unit_offset: Optional[int] = None
        pending_bit_unit_size = 0
        pending_bit_unit_align = 1
        pending_bit_type_code: Optional[str] = None
        pending_used_bits = 0

        def flush_pending_bit_unit() -> None:
            nonlocal current_offset
            nonlocal pending_bit_unit_offset, pending_bit_unit_size
            nonlocal pending_bit_unit_align, pending_bit_type_code, pending_used_bits

            if pending_bit_unit_offset is None:
                return

            current_offset = pending_bit_unit_offset + pending_bit_unit_size
            pending_bit_unit_offset = None
            pending_bit_unit_size = 0
            pending_bit_unit_align = 1
            pending_bit_type_code = None
            pending_used_bits = 0

        def parse_field_spec(field_spec):
            if len(field_spec) == 2:
                name, type_name = field_spec
                return name, type_name, None
            if len(field_spec) == 3:
                name, type_name, bit_width = field_spec
                return name, type_name, bit_width
            raise ValueError(f"Invalid field specification: {field_spec}")

        def validate_bitfield_type(type_code: str, type_name: str, name: str) -> None:
            if type_code in ('d', 'c', '?'):
                raise TypeError(
                    f"Bit field '{name}' uses unsupported base type '{type_name}'"
                )

        for field_spec in fields:
            name, type_name, bit_width = parse_field_spec(field_spec)
            fmt, size, align = self._get_type_info(type_name)

            if bit_width is not None:
                validate_bitfield_type(fmt, type_name, name)
                if bit_width <= 0:
                    raise ValueError(f"Bit width for field '{name}' must be positive")

                unit_bits = size * 8
                if bit_width > unit_bits:
                    raise ValueError(
                        f"Bit width {bit_width} for field '{name}' exceeds {unit_bits}-bit storage unit"
                    )

                must_start_new_unit = (
                    pending_bit_unit_offset is None
                    or pending_bit_type_code != fmt
                    or (pending_used_bits + bit_width) > unit_bits
                )

                if must_start_new_unit:
                    flush_pending_bit_unit()

                    if not self.packed:
                        padding = (align - (current_offset % align)) % align
                        current_offset += padding

                    pending_bit_unit_offset = current_offset
                    pending_bit_unit_size = size
                    pending_bit_unit_align = align
                    pending_bit_type_code = fmt
                    pending_used_bits = 0

                self.fields[name] = Field(
                    name,
                    fmt,
                    pending_bit_unit_offset,
                    size,
                    bit_width=bit_width,
                    bit_offset=pending_used_bits,
                )

                pending_used_bits += bit_width
                max_alignment = max(max_alignment, pending_bit_unit_align)

                if pending_used_bits == unit_bits:
                    flush_pending_bit_unit()

                continue

            flush_pending_bit_unit()
            
            # Add padding for alignment (unless packed)
            if not self.packed:
                padding = (align - (current_offset % align)) % align
                current_offset += padding
            
            self.fields[name] = Field(name, fmt, current_offset, size)
            current_offset += size
            
            max_alignment = max(max_alignment, align)

        flush_pending_bit_unit()

        # Structure end padding (unless packed)
        if not self.packed:
            padding = (max_alignment - (current_offset % max_alignment)) % max_alignment
            self.size = current_offset + padding
        else:
            self.size = current_offset
            
        # Apply explicit alignment if specified
        if self.explicit_alignment:
            # Ensure size is at least alignment bytes
            if self.size < self.explicit_alignment:
                self.size = self.explicit_alignment
            # Ensure size is multiple of alignment
            padding = (self.explicit_alignment - (self.size % self.explicit_alignment)) % self.explicit_alignment
            self.size += padding
            max_alignment = self.explicit_alignment
            
        self.alignment = max_alignment

class UnionDefinition(StructureDefinition):
    """
    Defines a C-style union layout (all fields at offset 0).
    """
    def _calculate_layout(self, fields: List[Union[Tuple[str, str], Tuple[str, str, Optional[int]]]]):
        max_size = 0
        max_alignment = 1

        for field_spec in fields:
            if len(field_spec) == 2:
                name, type_name = field_spec
                bit_width = None
            elif len(field_spec) == 3:
                name, type_name, bit_width = field_spec
            else:
                raise ValueError(f"Invalid field specification: {field_spec}")

            fmt, size, align = self._get_type_info(type_name)

            if bit_width is not None:
                if fmt in ('d', 'c', '?'):
                    raise TypeError(
                        f"Bit field '{name}' uses unsupported base type '{type_name}'"
                    )
                unit_bits = size * 8
                if bit_width <= 0:
                    raise ValueError(f"Bit width for field '{name}' must be positive")
                if bit_width > unit_bits:
                    raise ValueError(
                        f"Bit width {bit_width} for field '{name}' exceeds {unit_bits}-bit storage unit"
                    )

            # All fields at offset 0
            self.fields[name] = Field(name, fmt, 0, size, bit_width=bit_width, bit_offset=0)
            
            max_size = max(max_size, size)
            max_alignment = max(max_alignment, align)

        # Union size is size of largest member, aligned
        padding = (max_alignment - (max_size % max_alignment)) % max_alignment
        self.size = max_size + padding
        self.alignment = max_alignment

class StructureInstance:
    """Instance of a Struct or Union, backed by a bytearray."""
    def __init__(self, definition: StructureDefinition):
        self.definition = definition
        self.memory = bytearray(definition.size)
        # Keep references to objects stored as pointers to prevent GC
        self.references: Dict[int, Any] = {}

    def get_field(self, name: str) -> Any:
        field = self.definition.fields.get(name)
        if not field:
            raise AttributeError(f"'{self.definition.name}' has no field '{name}'")

        if field.bit_width is not None:
            try:
                container = struct.unpack_from(field.type_code, self.memory, field.offset)[0]
                if field.type_code == '?':
                    container = 1 if container else 0

                unit_bits = field.size * 8
                unsigned_val = int(container) & ((1 << unit_bits) - 1)
                mask = (1 << field.bit_width) - 1
                return (unsigned_val >> field.bit_offset) & mask
            except struct.error as e:
                raise RuntimeError(f"Memory corruption reading bit field '{name}': {e}")
        
        # Unpack from memory
        try:
            val = struct.unpack_from(field.type_code, self.memory, field.offset)[0]
            
            # Convert bytes to char if needed
            if field.type_code == 'c':
                return val.decode('ascii')
                
            # If it's a pointer type and we have a stored reference, return the object
            if field.type_code == 'Q' and field.offset in self.references:
                return self.references[field.offset]
                
            return val
        except struct.error as e:
            raise RuntimeError(f"Memory corruption reading field '{name}': {e}")

    def set_field(self, name: str, value: Any) -> None:
        field = self.definition.fields.get(name)
        if not field:
            raise AttributeError(f"'{self.definition.name}' has no field '{name}'")

        if field.bit_width is not None:
            if not isinstance(value, int):
                raise TypeError(f"Bit field '{name}' requires integer value")

            mask = (1 << field.bit_width) - 1
            if value < 0 or value > mask:
                raise ValueError(
                    f"Bit field '{name}' value {value} does not fit in {field.bit_width} bits"
                )

            try:
                container = struct.unpack_from(field.type_code, self.memory, field.offset)[0]
                if field.type_code == '?':
                    container = 1 if container else 0

                unit_bits = field.size * 8
                full_mask = (1 << unit_bits) - 1
                unsigned_val = int(container) & full_mask

                bit_mask = mask << field.bit_offset
                unsigned_val = (unsigned_val & ~bit_mask) | ((value & mask) << field.bit_offset)

                if field.type_code in ('q', 'b', 'h', 'i', 'l'):
                    sign_bit = 1 << (unit_bits - 1)
                    if unsigned_val & sign_bit:
                        packed_val = unsigned_val - (1 << unit_bits)
                    else:
                        packed_val = unsigned_val
                elif field.type_code == '?':
                    packed_val = bool(unsigned_val & 1)
                else:
                    packed_val = unsigned_val

                struct.pack_into(field.type_code, self.memory, field.offset, packed_val)
                return
            except struct.error as e:
                raise TypeError(f"Invalid bit field assignment for '{name}': {e}")
        
        # Pack to memory
        try:
            # Handle char encoding
            if field.type_code == 'c' and isinstance(value, str):
                value = value.encode('ascii')
            
            # Handle StructureInstance assignment to pointer field (nested structs)
            if field.type_code == 'Q' and isinstance(value, StructureInstance):
                # Store reference to prevent GC and allow retrieval
                self.references[field.offset] = value
                # Use Python's id() as a simulated pointer address
                value = id(value)
            elif field.type_code == 'Q' and field.offset in self.references:
                # If overwriting a pointer with a raw integer/address, remove the reference
                del self.references[field.offset]
                
            struct.pack_into(field.type_code, self.memory, field.offset, value)
        except struct.error as e:
            raise TypeError(f"Invalid value for field '{name}' (type {field.type_code}): {e}")

    def __repr__(self):
        fields_str = ", ".join(f"{name}={self.get_field(name)}" for name in self.definition.fields)
        return f"{self.definition.name}({fields_str})"
