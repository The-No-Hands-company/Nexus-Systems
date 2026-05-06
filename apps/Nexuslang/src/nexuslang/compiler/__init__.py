"""
NexusLang Compiler - Code Generation to Multiple Targets
========================================================

This module provides the compiler infrastructure to convert NexusLang AST
into various target formats for standalone execution.

Compiler Pipeline:
    NexusLang Source → Lexer → Parser → AST → Optimizer → Code Generator → Target Output

Supported Targets:
    1. C/C++ (compile with GCC/Clang to native binary)
    2. x86-64 Assembly (direct machine code generation)
    3. JavaScript/TypeScript (web platform)
    4. WebAssembly (WASM - universal execution)
    5. LLVM IR (future: leverage LLVM optimization passes)

Philosophy: No compromises - NexusLang compiles to true native code, not bytecode.
"""

from typing import Any, Dict, List, Optional, Set, Tuple
from abc import ABC, abstractmethod
from nexuslang.parser.ast import *
from nexuslang.optimizer import OptimizationLevel, OptimizationPipeline, create_optimization_pipeline


class CompilationTarget:
    """Enumeration of compilation targets."""
    C = "c"
    CPP = "cpp"
    ASM_X86_64 = "asm_x86_64"
    ASM_ARM = "asm_arm"
    JAVASCRIPT = "javascript"
    TYPESCRIPT = "typescript"
    WASM = "wasm"
    LLVM_IR = "llvm_ir"


class CodeGenerator(ABC):
    """Base class for all code generators."""
    
    def __init__(self, target: str):
        self.target = target
        self.output_buffer = []
        self.indent_level = 0
        self.temp_counter = 0
        self.required_libraries = set()
        
    @abstractmethod
    def generate(self, ast: Program) -> str:
        """Generate code from AST."""
        pass
    
    def emit(self, code: str) -> None:
        """Emit a line of code with proper indentation."""
        indent = "    " * self.indent_level
        self.output_buffer.append(f"{indent}{code}")
    
    def emit_raw(self, code: str) -> None:
        """Emit code without indentation."""
        self.output_buffer.append(code)
    
    def indent(self) -> None:
        """Increase indentation level."""
        self.indent_level += 1
    
    def dedent(self) -> None:
        """Decrease indentation level."""
        if self.indent_level > 0:
            self.indent_level -= 1
    
    def new_temp(self) -> str:
        """Generate a new temporary variable name."""
        self.temp_counter += 1
        return f"_t{self.temp_counter}"
    
    def get_output(self) -> str:
        """Get the generated code."""
        return "\n".join(self.output_buffer)
    
    def reset(self) -> None:
        """Reset the generator state."""
        self.output_buffer = []
        self.indent_level = 0
        self.temp_counter = 0


class CompilerOptions:
    """Compilation options and flags."""
    
    def __init__(self):
        self.optimization_level = 0  # 0 = none, 1 = basic, 2 = aggressive, 3 = maximum
        self.debug_info = False
        self.inline_functions = False
        self.loop_unrolling = False
        self.dead_code_elimination = True
        self.constant_folding = True
        self.target_arch = "x86_64"  # x86_64, arm64, etc.
        self.output_format = "executable"  # executable, library, object
        self.link_stdlib = True
        self.strip_symbols = False
        self.library_search_paths = []  # List of paths to search for libraries (-L)
        self.generate_header = False     # Whether to generate a C header file

        # Runtime pointer / memory validation (FFI safety)
        self.sanitize_address = False      # Enable AddressSanitizer (-fsanitize=address)
        self.sanitize_undefined = False    # Enable UndefinedBehaviorSanitizer (-fsanitize=undefined)
        self.enable_valgrind = False       # Emit Valgrind client-request macros in generated C
        
    def apply_optimization_preset(self, level: int) -> None:
        """Apply optimization preset based on level."""
        self.optimization_level = level
        
        if level >= 1:
            self.constant_folding = True
            self.dead_code_elimination = True
        
        if level >= 2:
            self.inline_functions = True
            self.loop_unrolling = True
        
        if level >= 3:
            # Aggressive optimizations
            self.strip_symbols = True


class Compiler:
    """Main NexusLang compiler orchestrator."""
    
    def __init__(self, options: Optional[CompilerOptions] = None):
        self.options = options or CompilerOptions()
        self.generators = {}
        self._register_generators()
    
    def _register_generators(self) -> None:
        """Register available code generators."""
        # Import generators here to avoid circular imports
        from .backends.c_generator import CCodeGenerator
        from .backends.cpp_generator import CppCodeGenerator
        from .backends.llvm_ir_generator import LLVMIRGenerator
        
        self.generators[CompilationTarget.C] = CCodeGenerator
        self.generators[CompilationTarget.CPP] = CppCodeGenerator
        self.generators[CompilationTarget.LLVM_IR] = LLVMIRGenerator
        # More generators will be registered as implemented
    
    def _map_optimization_level(self) -> OptimizationLevel:
        """Map CompilerOptions.optimization_level to OptimizationLevel enum."""
        level_map = {
            0: OptimizationLevel.O0,
            1: OptimizationLevel.O1,
            2: OptimizationLevel.O2,
            3: OptimizationLevel.O3,
        }
        return level_map.get(self.options.optimization_level, OptimizationLevel.O0)
    
    def _apply_ast_optimizations(self, ast: Program) -> Program:
        """
        Apply AST-level optimizations before code generation.
        
        Runs the optimization pipeline based on the compiler's optimization level:
            - O0: No optimizations
            - O1: Basic constant folding and dead code elimination
            - O2: O1 + function inlining, more aggressive DCE
            - O3: Maximum optimizations, aggressive inlining
        
        Args:
            ast: The original program AST
            
        Returns:
            Optimized AST (may be modified in place or a new tree)
        """
        opt_level = self._map_optimization_level()
        
        # Skip optimization if O0
        if opt_level == OptimizationLevel.O0:
            return ast
        
        # Create and run optimization pipeline
        verbose = self.options.debug_info  # Show optimization details in debug mode
        pipeline = create_optimization_pipeline(opt_level, verbose=verbose)
        
        if verbose:
            print(f"[Optimizer] Running optimization pipeline at level {opt_level.name}")
        
        optimized_ast = pipeline.run(ast)
        
        if verbose:
            pipeline.print_stats()
        
        return optimized_ast

    def compile(self, ast: Program, target: str, output_file: str) -> bool:
        """
        Compile AST to target format.
        
        Args:
            ast: The program AST to compile
            target: Compilation target (c, cpp, asm, js, wasm, etc.)
            output_file: Output file path
            
        Returns:
            Tuple[bool, Set[str]]: (Success, Required libraries)
        """
        if target not in self.generators:
            raise ValueError(f"Unsupported compilation target: {target}")
        
        # Apply AST-level optimizations based on optimization level
        optimized_ast = self._apply_ast_optimizations(ast)
        
        # Get appropriate generator
        generator_class = self.generators[target]
        generator = generator_class(target)
        
        # Generate code
        try:
            code = generator.generate(optimized_ast)
            
            # Ensure output directory exists
            import os
            output_dir = os.path.dirname(output_file)
            if output_dir and not os.path.exists(output_dir):
                os.makedirs(output_dir, exist_ok=True)
            
            # Write to output file
            with open(output_file, 'w') as f:
                f.write(code)
            
            print(f" Compilation successful: {output_file}")
            
            # Generate C header if requested and target is C/CPP
            if self.options.generate_header and target in [CompilationTarget.C, CompilationTarget.CPP]:
                from .codegen.header_generator import CHeaderGenerator
                header_gen = CHeaderGenerator()
                # Use same base name but .h extension
                import os
                base_name = os.path.splitext(output_file)[0]
                header_file = f"{base_name}.h"
                module_name = os.path.basename(base_name)
                
                header_code = header_gen.generate(ast, module_name)
                with open(header_file, 'w') as f:
                    f.write(header_code)
                print(f" Header generation successful: {header_file}")
            
            return True, generator.required_libraries
            
        except Exception as e:
            print(f" Compilation failed: {e}")
            import traceback
            traceback.print_exc()
            return False, set()
    
    def compile_and_link(self, ast: Program, target: str, output_file: str) -> bool:
        """
        Compile AST and link to create executable.
        
        For C/C++ targets, this generates the source and then invokes
        the system compiler (GCC/Clang) to create the final binary.
        """
        # First compile to intermediate format
        # Place intermediate files in same directory as output to avoid root clutter
        import os
        output_dir = os.path.dirname(output_file) or "."
        output_base = os.path.basename(output_file)
        intermediate_file = os.path.join(output_dir, f"{output_base}.generated.{target}")
        
        success, libraries = self.compile(ast, target, intermediate_file)
        if not success:
            return False
        
        # For C/C++, invoke system compiler
        if target in [CompilationTarget.C, CompilationTarget.CPP]:
            return self._link_with_system_compiler(intermediate_file, output_file, target, libraries)
        
        # For assembly, use assembler and linker
        elif target in [CompilationTarget.ASM_X86_64, CompilationTarget.ASM_ARM]:
            return self._assemble_and_link(intermediate_file, output_file)
        
        # Other targets don't need linking
        return True
    
    def _link_with_system_compiler(self, source_file: str, output_file: str, target: str, libraries: Set[str] = None) -> bool:
        """Link using system C/C++ compiler."""
        import subprocess
        import shutil
        
        # Choose compiler
        if target == CompilationTarget.CPP:
            compilers = ['g++', 'clang++']
        else:
            compilers = ['gcc', 'clang']
        
        compiler = None
        for c in compilers:
            if shutil.which(c):
                compiler = c
                break
        
        if not compiler:
            print(f" No C/C++ compiler found. Please install GCC or Clang.")
            return False
        
        # Build compiler command
        cmd = [compiler, source_file, '-o', output_file]
        
        # Add library search paths
        for path in self.options.library_search_paths:
            cmd.append(f'-L{path}')

        # Add required libraries from extern declarations
        if libraries:
            for lib in libraries:
                if lib and lib != 'c':  # libc is linked by default
                    cmd.append(f'-l{lib}')
        
        # Add optimization flags
        if self.options.optimization_level > 0:
            cmd.append(f'-O{self.options.optimization_level}')
        
        if self.options.debug_info:
            cmd.append('-g')

        if self.options.sanitize_address:
            cmd.extend(['-fsanitize=address', '-fno-omit-frame-pointer'])
        if self.options.sanitize_undefined:
            cmd.append('-fsanitize=undefined')

        if self.options.strip_symbols and self.options.optimization_level >= 3:
            cmd.append('-s')
        
        # Execute compiler
        try:
            print(f"Linking with {compiler}...")
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                print(f" Linking successful: {output_file}")
                return True
            else:
                print(f" Linking failed:")
                print(result.stderr)
                return False
                
        except Exception as e:
            print(f" Linking error: {e}")
            return False
    
    def _assemble_and_link(self, asm_file: str, output_file: str) -> bool:
        """Assemble and link assembly code."""
        import subprocess
        import shutil
        
        # Check for assembler (nasm for x86-64)
        if not shutil.which('nasm'):
            print(" NASM assembler not found. Please install NASM.")
            return False
        
        # Assemble
        obj_file = asm_file.replace('.asm', '.o')
        asm_cmd = ['nasm', '-f', 'elf64', asm_file, '-o', obj_file]
        
        try:
            result = subprocess.run(asm_cmd, capture_output=True, text=True)
            if result.returncode != 0:
                print(f" Assembly failed: {result.stderr}")
                return False
        except Exception as e:
            print(f" Assembly error: {e}")
            return False
        
        # Link
        link_cmd = ['ld', obj_file, '-o', output_file]
        
        try:
            result = subprocess.run(link_cmd, capture_output=True, text=True)
            if result.returncode != 0:
                print(f" Linking failed: {result.stderr}")
                return False
            
            print(f" Assembly and linking successful: {output_file}")
            return True
            
        except Exception as e:
            print(f" Linking error: {e}")
            return False


def create_compiler(optimization_level: int = 0, debug: bool = False) -> Compiler:
    """
    Create a compiler with specified options.
    
    Args:
        optimization_level: 0=none, 1=basic, 2=aggressive, 3=maximum
        debug: Include debug information
        
    Returns:
        Configured Compiler instance
    """
    options = CompilerOptions()
    options.apply_optimization_preset(optimization_level)
    options.debug_info = debug
    
    return Compiler(options)
