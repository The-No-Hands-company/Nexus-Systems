# Inline Assembly Design for NexusLang

## Syntax Options

### Option 1: Natural language style
```nexuslang
inline assembly
    "mov rax, rbx"
    "add rax, 1"
    "ret"
end
```

### Option 2: Block with metadata
```nexuslang
asm
    code "mov rax, rbx"
    code "add rax, 1"
    outputs result as Integer in "=r"(rax)
    inputs value as Integer in "r"(rbx)
    clobbers "rax", "rbx"
end
```

### Option 3: Single-line (GCC-style)
```nexuslang
asm "mov %1, %0" with outputs "=r"(result) and inputs "r"(value)
```

### Option 4: Multi-line with template
```nexuslang
assembly """
    mov rax, {input1}
    add rax, {input2}
    mov {output}, rax
""" with inputs input1, input2 and output result
```

## Recommended Approach: Option 2 (Block with Metadata)

This provides the most flexibility and safety while remaining readable.

## Token Requirements
- ASM or ASSEMBLY keyword
- CODE keyword for assembly instructions
- OUTPUTS, INPUTS, CLOBBERS keywords
- IN keyword for register constraints
- Support for constraint strings ("=r", "r", "m", etc.)

## AST Node Structure
```python
class InlineAssemblyStatement:
    instructions: List[str]  # Assembly instructions
    outputs: List[Tuple[str, str, str]]  # (variable, constraint, register)
    inputs: List[Tuple[str, str, str]]  # (variable, constraint, register)
    clobbers: List[str]  # Clobbered registers
    dialect: str  # 'att' or 'intel'
```

## Safety Considerations
1. Inline assembly cannot be executed by interpreter (compile-only feature)
2. Must validate that outputs/inputs match assembly template
3. Warn about clobbered registers
4. Platform-specific (x86, ARM, etc.)
