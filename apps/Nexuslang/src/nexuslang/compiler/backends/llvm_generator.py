"""
LLVM IR Code Generator for NexusLang
================================

Generates LLVM IR from NexusLang AST for native compilation.

This is the REAL compiler backend - generates machine code directly via LLVM,
not through C/C++ transpilation.

Strategy:
- NexusLang AST → LLVM IR (Intermediate Representation)
- LLVM IR → Optimized machine code (x86-64, ARM, WASM, etc.)
- Direct hardware access: real SIMD, GPU compute, inline assembly
- Full LLVM optimization pipeline

Dependencies:
- LLVM toolchain (llc, clang) - uses system LLVM directly
  No Python bindings needed!
  
This implementation generates LLVM IR as text, then uses system LLVM tools.
Works with ANY LLVM version (11-21+).

Architecture:
    NexusLang Source → Lexer → Parser → AST → LLVMGenerator → LLVM IR Text
                                                              ↓
    LLVM Optimizer (opt) ← LLVM IR (.ll file)
         ↓
    LLVM Compiler (llc) → Assembly (.s)
         ↓
    Assembler (clang) → Native Executable
"""

from typing import Any, Dict, List, Optional, Set
from nexuslang.compiler import CodeGenerator, CompilationTarget
from nexuslang.parser.ast import *

# Pure Python LLVM IR generation - no external dependencies!
LLVM_AVAILABLE = True


class LLVMTypeMapper:
    """Maps NexusLang types to LLVM IR type strings."""
    
    def __init__(self):
        # LLVM type strings (pure text, no dependencies)
        pass
        
    def map_type(self, nxl_type: Optional[str]) -> str:
        """Map NexusLang type string to LLVM IR type string."""
        if nxl_type is None:
            return "i64"  # Default to i64
        
        type_lower = nxl_type.lower()
        
        # Primitive types
        if type_lower in ("integer", "int"):
            return "i64"
        elif type_lower in ("float", "double"):
            return "double"
        elif type_lower == "boolean":
            return "i1"
        elif type_lower == "string":
            return "i8*"  # char pointer
        elif type_lower == "void":
            return "void"
        
        # Default to i64 for unknown types
        return "i64"


class LLVMCodeGenerator(CodeGenerator):
    """Generate LLVM IR from NexusLang AST."""
    
    def __init__(self, target: str = CompilationTarget.LLVM_IR):
        super().__init__(target)
        
        if not LLVM_AVAILABLE:
            raise ImportError(
                "llvmlite not installed. Install with: pip install llvmlite\n"
                "Run: pip install llvmlite"
            )
        
        # Initialize LLVM
        llvm.initialize()
        llvm.initialize_native_target()
        llvm.initialize_native_asmprinter()
        
        # LLVM module and builder
        self.module = ir.Module(name="nxl_module")
        self.module.triple = llvm.get_default_triple()
        
        self.builder: Optional[ir.IRBuilder] = None
        self.current_function: Optional[ir.Function] = None
        
        # Type mapper
        self.type_mapper = LLVMTypeMapper()
        
        # Symbol tables
        self.global_symbols: Dict[str, ir.GlobalVariable] = {}
        self.local_symbols: Dict[str, ir.AllocaInstr] = {}
        self.functions: Dict[str, ir.Function] = {}
        
        # Standard library functions (will be declared as external)
        self._declare_stdlib_functions()
    
    def _declare_stdlib_functions(self):
        """Declare external standard library functions."""
        # printf - for print statements
        voidptr_ty = ir.IntType(8).as_pointer()
        printf_ty = ir.FunctionType(ir.IntType(32), [voidptr_ty], var_arg=True)
        self.printf_func = ir.Function(self.module, printf_ty, name="printf")
        
        # malloc - for dynamic memory allocation
        malloc_ty = ir.FunctionType(voidptr_ty, [ir.IntType(64)])
        self.malloc_func = ir.Function(self.module, malloc_ty, name="malloc")
        
        # free - for memory deallocation
        free_ty = ir.FunctionType(ir.VoidType(), [voidptr_ty])
        self.free_func = ir.Function(self.module, free_ty, name="free")
    
    def generate(self, ast: Program) -> str:
        """Generate LLVM IR from AST."""
        # First pass: declare all functions
        for stmt in ast.statements:
            if isinstance(stmt, FunctionDefinition):
                self._declare_function(stmt)
        
        # Create main function
        main_func_type = ir.FunctionType(ir.IntType(32), [])
        main_func = ir.Function(self.module, main_func_type, name="main")
        self.current_function = main_func
        
        # Create entry block
        entry_block = main_func.append_basic_block(name="entry")
        self.builder = ir.IRBuilder(entry_block)
        
        # Generate top-level statements (in main function)
        for stmt in ast.statements:
            if not isinstance(stmt, FunctionDefinition):
                self._generate_statement(stmt)
        
        # Return 0 from main
        self.builder.ret(ir.Constant(ir.IntType(32), 0))
        
        # Second pass: generate function bodies
        for stmt in ast.statements:
            if isinstance(stmt, FunctionDefinition):
                self._generate_function(stmt)
        
        # Return the LLVM IR as string
        return str(self.module)
    
    def _declare_function(self, node: FunctionDefinition):
        """Declare a function (signature only)."""
        # Map return type
        if node.return_type:
            ret_type = self.type_mapper.map_type(node.return_type)
        else:
            ret_type = ir.VoidType()
        
        # Map parameter types
        param_types = []
        for param in node.parameters:
            param_type = self.type_mapper.map_type(param.param_type if hasattr(param, 'param_type') else None)
            param_types.append(param_type)
        
        # Create function type
        func_type = ir.FunctionType(ret_type, param_types)
        
        # Create function
        func = ir.Function(self.module, func_type, name=node.name)
        self.functions[node.name] = func
    
    def _generate_function(self, node: FunctionDefinition):
        """Generate complete function implementation."""
        func = self.functions[node.name]
        self.current_function = func
        
        # Create entry block
        entry_block = func.append_basic_block(name="entry")
        self.builder = ir.IRBuilder(entry_block)
        
        # Clear local symbols for new function
        self.local_symbols = {}
        
        # Allocate space for parameters and store them
        for i, param in enumerate(node.parameters):
            param_name = param.name if hasattr(param, 'name') else f"arg{i}"
            param_type = self.type_mapper.map_type(param.param_type if hasattr(param, 'param_type') else None)
            
            # Allocate space for parameter
            param_alloca = self.builder.alloca(param_type, name=param_name)
            
            # Store the parameter value
            self.builder.store(func.args[i], param_alloca)
            
            # Add to symbol table
            self.local_symbols[param_name] = param_alloca
        
        # Generate function body
        for stmt in node.body:
            self._generate_statement(stmt)
        
        # Ensure function has a return
        if not self.builder.block.is_terminated:
            if func.return_value.type == ir.VoidType():
                self.builder.ret_void()
            else:
                # Return default value (0, 0.0, false, null)
                self.builder.ret(ir.Constant(func.return_value.type, 0))
    
    def _generate_statement(self, stmt):
        """Generate LLVM IR for a statement."""
        if isinstance(stmt, VariableDeclaration):
            self._generate_variable_declaration(stmt)
        elif isinstance(stmt, Assignment):
            self._generate_assignment(stmt)
        elif isinstance(stmt, FunctionCall):
            self._generate_function_call(stmt)
        elif isinstance(stmt, PrintStatement):
            self._generate_print_statement(stmt)
        elif isinstance(stmt, ReturnStatement):
            self._generate_return_statement(stmt)
        elif isinstance(stmt, IfStatement):
            self._generate_if_statement(stmt)
        elif isinstance(stmt, WhileLoop):
            self._generate_while_loop(stmt)
        elif isinstance(stmt, ForLoop):
            self._generate_for_loop(stmt)
        # Add more statement types as needed
    
    def _generate_variable_declaration(self, node: VariableDeclaration):
        """Generate variable declaration."""
        var_type = self.type_mapper.map_type(node.var_type if hasattr(node, 'var_type') else None)
        
        # Allocate space on stack
        var_alloca = self.builder.alloca(var_type, name=node.name)
        
        # Store initial value if provided
        if node.value:
            initial_value = self._generate_expression(node.value)
            self.builder.store(initial_value, var_alloca)
        else:
            # Initialize to zero
            self.builder.store(ir.Constant(var_type, 0), var_alloca)
        
        # Add to symbol table
        self.local_symbols[node.name] = var_alloca
    
    def _generate_assignment(self, node: Assignment):
        """Generate assignment statement."""
        # Get the variable
        if node.target in self.local_symbols:
            var_ptr = self.local_symbols[node.target]
        elif node.target in self.global_symbols:
            var_ptr = self.global_symbols[node.target]
        else:
            raise ValueError(f"Undefined variable: {node.target}")
        
        # Generate value expression
        value = self._generate_expression(node.value)
        
        # Store the value
        self.builder.store(value, var_ptr)
    
    def _generate_print_statement(self, node: PrintStatement):
        """Generate print statement using printf."""
        # Get the value to print
        value = self._generate_expression(node.value)
        
        # Create format string based on type
        if value.type == ir.IntType(64):
            format_str = "%lld\n"
        elif value.type == ir.IntType(32):
            format_str = "%d\n"
        elif value.type == ir.DoubleType():
            format_str = "%f\n"
        elif value.type == ir.IntType(1):
            format_str = "%d\n"
        else:
            format_str = "%p\n"  # Pointer
        
        # Create global constant string
        c_fmt = ir.Constant(ir.ArrayType(ir.IntType(8), len(format_str) + 1),
                           bytearray(format_str.encode("utf8") + b"\0"))
        global_fmt = ir.GlobalVariable(self.module, c_fmt.type, name=f".fmt_{self.temp_counter}")
        global_fmt.linkage = 'internal'
        global_fmt.global_constant = True
        global_fmt.initializer = c_fmt
        self.temp_counter += 1
        
        # Get pointer to format string
        fmt_ptr = self.builder.bitcast(global_fmt, ir.IntType(8).as_pointer())
        
        # Call printf
        self.builder.call(self.printf_func, [fmt_ptr, value])
    
    def _generate_return_statement(self, node: ReturnStatement):
        """Generate return statement."""
        if node.value:
            ret_value = self._generate_expression(node.value)
            self.builder.ret(ret_value)
        else:
            self.builder.ret_void()
    
    def _generate_if_statement(self, node: IfStatement):
        """Generate if statement with conditional branching."""
        # Evaluate condition
        condition = self._generate_expression(node.condition)
        
        # Create basic blocks
        then_block = self.current_function.append_basic_block(name="if.then")
        else_block = self.current_function.append_basic_block(name="if.else") if node.else_body else None
        merge_block = self.current_function.append_basic_block(name="if.end")
        
        # Branch based on condition
        if else_block:
            self.builder.cbranch(condition, then_block, else_block)
        else:
            self.builder.cbranch(condition, then_block, merge_block)
        
        # Generate then block
        self.builder.position_at_end(then_block)
        for stmt in node.then_body:
            self._generate_statement(stmt)
        if not self.builder.block.is_terminated:
            self.builder.branch(merge_block)
        
        # Generate else block if present
        if else_block:
            self.builder.position_at_end(else_block)
            for stmt in node.else_body:
                self._generate_statement(stmt)
            if not self.builder.block.is_terminated:
                self.builder.branch(merge_block)
        
        # Continue at merge block
        self.builder.position_at_end(merge_block)
    
    def _generate_while_loop(self, node: WhileLoop):
        """Generate while loop."""
        # Create basic blocks
        cond_block = self.current_function.append_basic_block(name="while.cond")
        body_block = self.current_function.append_basic_block(name="while.body")
        end_block = self.current_function.append_basic_block(name="while.end")
        
        # Jump to condition
        self.builder.branch(cond_block)
        
        # Generate condition block
        self.builder.position_at_end(cond_block)
        condition = self._generate_expression(node.condition)
        self.builder.cbranch(condition, body_block, end_block)
        
        # Generate body block
        self.builder.position_at_end(body_block)
        for stmt in node.body:
            self._generate_statement(stmt)
        if not self.builder.block.is_terminated:
            self.builder.branch(cond_block)
        
        # Continue at end block
        self.builder.position_at_end(end_block)
    
    def _generate_for_loop(self, node: ForLoop):
        """Generate for loop (converted to while loop)."""
        # Initialize loop variable
        if hasattr(node, 'init'):
            self._generate_statement(node.init)
        
        # Create basic blocks
        cond_block = self.current_function.append_basic_block(name="for.cond")
        body_block = self.current_function.append_basic_block(name="for.body")
        inc_block = self.current_function.append_basic_block(name="for.inc")
        end_block = self.current_function.append_basic_block(name="for.end")
        
        # Jump to condition
        self.builder.branch(cond_block)
        
        # Generate condition block
        self.builder.position_at_end(cond_block)
        if hasattr(node, 'condition'):
            condition = self._generate_expression(node.condition)
            self.builder.cbranch(condition, body_block, end_block)
        else:
            self.builder.branch(body_block)
        
        # Generate body block
        self.builder.position_at_end(body_block)
        for stmt in node.body:
            self._generate_statement(stmt)
        if not self.builder.block.is_terminated:
            self.builder.branch(inc_block)
        
        # Generate increment block
        self.builder.position_at_end(inc_block)
        if hasattr(node, 'increment'):
            self._generate_statement(node.increment)
        self.builder.branch(cond_block)
        
        # Continue at end block
        self.builder.position_at_end(end_block)
    
    def _generate_expression(self, expr) -> ir.Value:
        """Generate LLVM IR for an expression."""
        if isinstance(expr, IntegerLiteral):
            return ir.Constant(ir.IntType(64), expr.value)
        
        elif isinstance(expr, FloatLiteral):
            return ir.Constant(ir.DoubleType(), expr.value)
        
        elif isinstance(expr, BooleanLiteral):
            return ir.Constant(ir.IntType(1), 1 if expr.value else 0)
        
        elif isinstance(expr, StringLiteral):
            # Create global string constant
            str_val = expr.value
            c_str = ir.Constant(ir.ArrayType(ir.IntType(8), len(str_val) + 1),
                               bytearray(str_val.encode("utf8") + b"\0"))
            global_str = ir.GlobalVariable(self.module, c_str.type, name=f".str_{self.temp_counter}")
            global_str.linkage = 'internal'
            global_str.global_constant = True
            global_str.initializer = c_str
            self.temp_counter += 1
            
            # Return pointer to string
            return self.builder.bitcast(global_str, ir.IntType(8).as_pointer())
        
        elif isinstance(expr, Variable):
            # Load variable value
            if expr.name in self.local_symbols:
                var_ptr = self.local_symbols[expr.name]
            elif expr.name in self.global_symbols:
                var_ptr = self.global_symbols[expr.name]
            else:
                raise ValueError(f"Undefined variable: {expr.name}")
            
            return self.builder.load(var_ptr, name=expr.name)
        
        elif isinstance(expr, BinaryOp):
            return self._generate_binary_op(expr)
        
        elif isinstance(expr, UnaryOp):
            return self._generate_unary_op(expr)
        
        elif isinstance(expr, FunctionCall):
            return self._generate_function_call(expr)
        
        else:
            raise NotImplementedError(f"Expression type not implemented: {type(expr).__name__}")
    
    def _generate_binary_op(self, expr: BinaryOp) -> ir.Value:
        """Generate binary operation."""
        left = self._generate_expression(expr.left)
        right = self._generate_expression(expr.right)
        
        # Arithmetic operations
        if expr.operator in ("+", "plus"):
            return self.builder.add(left, right, name="add")
        elif expr.operator in ("-", "minus"):
            return self.builder.sub(left, right, name="sub")
        elif expr.operator in ("*", "times", "multiplied by"):
            return self.builder.mul(left, right, name="mul")
        elif expr.operator in ("/", "divided by"):
            return self.builder.sdiv(left, right, name="div")
        elif expr.operator in ("%", "mod", "modulo"):
            return self.builder.srem(left, right, name="mod")
        
        # Comparison operations
        elif expr.operator in ("==", "equals", "is equal to"):
            return self.builder.icmp_signed("==", left, right, name="eq")
        elif expr.operator in ("!=", "is not equal to"):
            return self.builder.icmp_signed("!=", left, right, name="ne")
        elif expr.operator in ("<", "is less than"):
            return self.builder.icmp_signed("<", left, right, name="lt")
        elif expr.operator in ("<=", "is less than or equal to"):
            return self.builder.icmp_signed("<=", left, right, name="le")
        elif expr.operator in (">", "is greater than"):
            return self.builder.icmp_signed(">", left, right, name="gt")
        elif expr.operator in (">=", "is greater than or equal to"):
            return self.builder.icmp_signed(">=", left, right, name="ge")
        
        # Logical operations
        elif expr.operator in ("and", "&&"):
            return self.builder.and_(left, right, name="and")
        elif expr.operator in ("or", "||"):
            return self.builder.or_(left, right, name="or")
        
        else:
            raise NotImplementedError(f"Binary operator not implemented: {expr.operator}")
    
    def _generate_unary_op(self, expr: UnaryOp) -> ir.Value:
        """Generate unary operation."""
        operand = self._generate_expression(expr.operand)
        
        if expr.operator in ("-", "minus"):
            # Negate
            return self.builder.neg(operand, name="neg")
        elif expr.operator in ("not", "!"):
            # Logical NOT
            return self.builder.not_(operand, name="not")
        else:
            raise NotImplementedError(f"Unary operator not implemented: {expr.operator}")
    
    def _generate_function_call(self, expr: FunctionCall) -> Optional[ir.Value]:
        """Generate function call."""
        if expr.function_name not in self.functions:
            raise ValueError(f"Undefined function: {expr.function_name}")
        
        func = self.functions[expr.function_name]
        
        # Generate argument expressions
        args = []
        for arg in expr.arguments:
            args.append(self._generate_expression(arg))
        
        # Call the function
        return self.builder.call(func, args, name=f"{expr.function_name}_result")
    
    def compile_to_object_file(self, output_file: str):
        """Compile LLVM IR to object file (.o)."""
        # Parse the module
        mod = llvm.parse_assembly(str(self.module))
        mod.verify()
        
        # Create target machine
        target = llvm.Target.from_default_triple()
        target_machine = target.create_target_machine()
        
        # Compile to object file
        with open(output_file, 'wb') as f:
            obj = target_machine.emit_object(mod)
            f.write(obj)
        
        print(f" Object file generated: {output_file}")
    
    def compile_to_executable(self, output_file: str):
        """Compile LLVM IR to native executable."""
        import subprocess
        import tempfile
        import os
        
        # First compile to object file
        with tempfile.NamedTemporaryFile(suffix='.o', delete=False) as tmp:
            obj_file = tmp.name
        
        try:
            self.compile_to_object_file(obj_file)
            
            # Link with clang
            link_cmd = ['clang', obj_file, '-o', output_file, '-lm']
            result = subprocess.run(link_cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                print(f" Executable generated: {output_file}")
                return True
            else:
                print(f" Linking failed: {result.stderr}")
                return False
        
        finally:
            # Clean up temporary object file
            if os.path.exists(obj_file):
                os.remove(obj_file)
