"""
Interpreter for NexusLang.
Executes AST nodes and manages program state.
"""

import os
import copy
import queue
import re
import struct
from typing import Any, Dict, List, Optional, Pattern, Tuple

from nexuslang.errors import NxlNameError, NxlRuntimeError, NxlTypeError, get_close_matches
from nexuslang.parser.ast import (
    Program, VariableDeclaration, FunctionDefinition, Parameter,
    IfStatement, WhileLoop, ForLoop, MemoryAllocation, MemoryDeallocation,
    ClassDefinition, PropertyDeclaration, MethodDefinition,
    ObjectInstantiation,
    ConcurrentExecution, TryCatch, BinaryOperation, UnaryOperation,
    Literal, Identifier, FunctionCall, RepeatNTimesLoop,
    ReturnStatement, BreakStatement, ContinueStatement, Block, ConcurrentBlock, TryCatchBlock,
    # Module-related AST nodes
    ImportStatement, SelectiveImport, ModuleAccess, PrivateDeclaration,
    # FFI-related AST nodes
    ExternFunctionDeclaration, ExternVariableDeclaration, ExternTypeDeclaration,
    # Inline assembly AST node
    InlineAssembly,
    # Struct/Union AST nodes
    StructDefinition as ASTStructDefinition, 
    UnionDefinition as ASTUnionDefinition,
    StructField,
    # Pattern matching AST nodes
    MatchExpression, MatchCase,
    LiteralPattern, IdentifierPattern, WildcardPattern,
    OptionPattern, ResultPattern, VariantPattern,
    # Smart pointer / ownership AST nodes
    RcCreation, DowngradeExpression, UpgradeExpression,
    MoveExpression, BorrowExpression, DropBorrowStatement,
    # Allocator hints and parallel execution
    AllocatorHint, ParallelForLoop,
    # Lifetime-annotated borrow expressions
    BorrowExpressionWithLifetime,
    # Metaprogramming / compile-time AST nodes
    ComptimeExpression, ComptimeConst, ComptimeAssert,
)
import concurrent.futures as _cf
from nexuslang.runtime import Runtime
# Import CircularImportError for module loading
from nexuslang.modules.module_loader import CircularImportError
# Don't import ModuleLoader here to avoid circular imports
# from nexuslang.modules.module_loader import ModuleLoader
from nexuslang.runtime.structures import (
    StructDefinition as RuntimeStructDefinition,
    UnionDefinition as RuntimeUnionDefinition,
    StructureInstance
)
from nexuslang.typesystem.generics_system import TypeParameterInfo, TypeConstraint, GenericTypeInference



# Control flow exceptions for break/continue
class BreakException(Exception):
    """Raised when a break statement is encountered."""
    pass


class ContinueException(Exception):
    """Raised when a continue statement is encountered."""
    pass


class ReturnException(Exception):
    """Raised when a return statement is encountered."""
    def __init__(self, value=None):
        self.value = value


class FallthroughException(Exception):
    """Raised when a fallthrough statement is encountered in switch cases."""
    pass


class YieldException(Exception):
    """Raised when a yield expression is encountered in generators."""
    def __init__(self, value=None):
        self.value = value


class NLPLUserException(Exception):
    """Exception raised by NexusLang 'raise' statements."""
    def __init__(self, exception_type, message, line=None, column=None):
        self.exception_type = exception_type
        self.message = message
        self.line = line
        self.column = column
        super().__init__(f"{exception_type}: {message}" if message else exception_type)


class _Channel:
    """Simple runtime channel used by send/receive expressions."""

    def __init__(self):
        self._queue = queue.Queue()
        self._closed = False

    def send(self, value):
        if self._closed:
            raise NxlRuntimeError(
                message="Cannot send to a closed channel",
                error_type_key="invalid_operation",
            )
        # Snapshot mutable payloads to model transfer semantics in interpreter mode.
        try:
            transferred = copy.deepcopy(value)
        except Exception:
            transferred = value
        self._queue.put(transferred)

    def receive(self):
        try:
            return self._queue.get_nowait()
        except queue.Empty as exc:
            if self._closed:
                raise NxlRuntimeError(
                    message="Cannot receive from a closed and empty channel",
                    error_type_key="invalid_operation",
                ) from exc
            raise NxlRuntimeError(
                message="Cannot receive from an empty channel",
                error_type_key="invalid_operation",
            ) from exc

    def close(self):
        self._closed = True

    def __repr__(self):
        return "Channel()"


def _camel_to_snake(name: str) -> str:
    """Convert CamelCase to snake_case for dispatch table building."""
    return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()


class Interpreter:
    """Interprets and executes the AST."""

    # Class-level dispatch cache: maps node_type string -> bound method.
    # Built lazily on first Interpreter instantiation and shared across instances
    # (methods are looked up on self at call time via getattr, then cached per-instance).
    # This eliminates the per-call regex + hasattr + getattr chain in execute().
    _DISPATCH_TABLE: Dict[str, str] = {}  # node_type -> method_name (str)

    def __init__(self, runtime, enable_type_checking=False, source=None):
        """Initialize the interpreter.

        Args:
            runtime: The Runtime environment.
            enable_type_checking: Whether to run the static type checker before
                execution.  Defaults to False at the constructor level because
                the TypeChecker requires stdlib functions to be synced first
                (done automatically in run_program).  The CLI entry point
                (main.py) passes True by default so that end-user programs are
                always type-checked unless --no-type-check is given.
            source: Full source text (for error context).
        """
        self.runtime = runtime
        self.source = source  # Store full source for error context
        self.global_scope = {}
        self.current_scope = [self.global_scope]
        self.functions = {}
        self.classes = {}
        self.macros = {}  # Registry for macro definitions
        self.comptime_constants = {}  # Registry for compile-time constants
        self.type_aliases = {}  # Registry for type aliases
        self.attribute_definitions = {}  # Registry for declared attribute types
        self.derived_method_registry = {}  # class_name -> {method_name -> callable}
        self.last_exception = None  # To support re-raising
        self.module_loader = None  # Will be initialized lazily

        # Ownership / borrow tracking
        # Structure: { var_name: {"immutable_count": int, "is_mutable": bool} }
        self._borrow_tracker: dict = {}

        # Runtime type enforcement: parallel scope stack tracking declared type
        # annotations for variables.  When a variable is declared with a type
        # (e.g.  set x as Integer to 5), the annotation is stored here so that
        # subsequent reassignments (set x to "hello") are caught as type errors
        # even when the static type checker is disabled.
        self._type_annotations: list = [{}]  # parallel to current_scope

        # Unsafe FFI context depth (0 = safe mode, >0 = inside an 'unsafe' block).
        # When > 0, null-pointer guards, bounds checks, and ownership enforcement
        # are suppressed to allow raw C-level FFI operations.
        self._in_unsafe_context: int = 0

        # Type system components (lazy initialization)
        self.enable_type_checking = enable_type_checking
        self.type_checker = None
        self.type_inference = None
        if enable_type_checking:
            from ..typesystem.typechecker import TypeChecker
            from ..typesystem.type_inference import TypeInferenceEngine
            self.type_checker = TypeChecker()
            self.type_inference = TypeInferenceEngine()
        
        # Debugger integration (optional)
        self.debugger = None
        self.current_file = None
        self.current_line = None

        # Coverage collector (optional) — set by CoverageCollector.attach()
        self._coverage_collector = None

        # Inline assembly interpreter policy.
        # Supported values:
        # - "error": raise NxlRuntimeError when asm is encountered.
        # - "warn": print warning once and skip asm block.
        # - "skip": silently skip asm block.
        #
        # Default is strict when type checking is enabled (CLI default), and
        # warning mode otherwise for compatibility with non-strict test runs.
        runtime_policy = getattr(runtime, "inline_asm_policy", None)
        if runtime_policy is not None:
            self.inline_asm_policy = str(runtime_policy).strip().lower()
        else:
            self.inline_asm_policy = "error" if enable_type_checking else "warn"

        # Pre-call value map for old(expr) postcondition capture.
        # Keyed by id(OldExpression node) → pre-call evaluated value.
        # Populated before each user-function body and cleared after.
        self._old_values: Dict[int, Any] = {}

        # Per-instance dispatch cache: node_type -> bound method (avoids repeated getattr).
        # Built lazily on first call to execute() so that subclasses that override
        # execute_* methods are also picked up correctly.
        self._dispatch_cache: Dict[str, Any] = {}

        # Accumulated test results from all describe / test / it blocks run
        # during this interpreter session.  Each entry is a dict produced by
        # _run_test_body: {"name": str, "passed": bool, "error": str|None,
        # "duration": float, "suite": str}.  The test runner reads this list
        # to build its aggregate report.
        self._collected_test_results: list = []

        # Build the class-level string dispatch table once (shared, maps type name -> method name)
        if not Interpreter._DISPATCH_TABLE:
            Interpreter._build_dispatch_table()

        # Register metaprogramming introspection functions that need interpreter access
        self._register_metaprogramming_functions()
    
    @staticmethod
    def _build_dispatch_table():
        """Build the class-level node_type -> method_name mapping.

        Iterates over Interpreter's own execute_* methods once at class-init time
        and stores the camelCase node-type key alongside the method name string.
        This avoids the per-dispatch regex overhead at runtime.
        """
        table = {}
        for attr in dir(Interpreter):
            if not attr.startswith("execute_"):
                continue
            snake = attr[len("execute_"):]  # e.g. "binary_operation"
            # Derive CamelCase key (e.g. "BinaryOperation")
            camel = "".join(w.title() for w in snake.split("_"))
            table[camel] = attr
            # Also store the snake_case key directly for nodes that use
            # node_type in snake_case (rare, but keep backward compat)
            table[snake] = attr
        Interpreter._DISPATCH_TABLE = table

    def sync_runtime_functions_to_type_checker(self):
        """
        Sync runtime-registered functions to the type checker.
        
        This is critical for stdlib functions registered after interpreter initialization.
        Without this, the type checker won't know about graphics functions, math functions, etc.
        """
        if not self.enable_type_checking or not self.type_checker:
            return
        
        from ..typesystem.types import FunctionType, ANY_TYPE
        
        # Register all runtime functions with the type checker
        for func_name in self.runtime.functions.keys():
            if func_name not in self.type_checker.env.functions:
                # Create a permissive function type (accepts any args, returns any)
                # This allows type checking to pass while still catching undefined functions
                # We use a large param list to support functions with many arguments
                func_type = FunctionType(
                    param_types=[ANY_TYPE] * 20,  # Support up to 20 args
                    return_type=ANY_TYPE
                )
                # Mark it as variadic so argument count checking is skipped
                func_type.variadic = True
                self.type_checker.env.define_function(func_name, func_type)
        
    def create_function_wrapper(self, function_def):
        """Create a callable wrapper for a user-defined function.
        
        Supports both positional and keyword arguments, including:
        - Named/keyword parameters
        - Default values
        - Variadic parameters (*args)
        - Keyword-only parameters (after * separator)
        """
        def wrapper(*args, **kwargs):
            # Use _resolve_function_arguments to handle all parameter types
            resolved_args = self._resolve_function_arguments(
                function_def,
                list(args),
                kwargs,
                function_def.name
            )

            # Separate ensure postconditions from the main body so they can
            # be executed AFTER the body with `result` bound in scope.
            from ..parser.ast import EnsureStatement as _EnsureStmt
            main_body = [s for s in function_def.body
                         if not isinstance(s, _EnsureStmt)]
            ensure_stmts = [s for s in function_def.body
                            if isinstance(s, _EnsureStmt)]

            # Capture old() pre-call values before entering the new scope.
            saved_old_values = self._old_values
            self._old_values = {}
            if ensure_stmts:
                for _es in ensure_stmts:
                    for _old_node in self._collect_old_refs(_es.condition):
                        self._old_values[id(_old_node)] = self.execute(_old_node.expr)
                    if _es.message_expr is not None:
                        for _old_node in self._collect_old_refs(_es.message_expr):
                            self._old_values[id(_old_node)] = self.execute(_old_node.expr)

            # Create a new scope
            self.enter_scope()

            return_value = None
            try:
                # Bind parameters
                for param, value in zip(function_def.parameters, resolved_args):
                    self.set_variable(param.name, value)

                # Execute main body (excluding ensure statements)
                try:
                    for statement in main_body:
                        self.execute(statement)
                except ReturnException as ret:
                    return_value = ret.value

                # Bind `result` for postcondition evaluation.
                self.set_variable("result", return_value)

                # Execute ensure postconditions.
                for _es in ensure_stmts:
                    self.execute(_es)

            finally:
                self._old_values = saved_old_values
                self.exit_scope()

            return return_value
        return wrapper

    def _get_module_loader(self):
        """Lazy initialize the module loader to avoid circular imports"""
        if self.module_loader is None:
            # Import here to avoid circular imports
            from ..modules.module_loader import ModuleLoader
            search_paths = [os.getcwd()]
            # If the runtime knows the current file, add its directory to search
            # paths so bare-name imports like 'import utils' resolve from the
            # script's directory in addition to cwd.
            file_path = getattr(self.runtime, 'module_path', None)
            if file_path:
                file_dir = os.path.dirname(os.path.abspath(file_path))
                if file_dir not in search_paths:
                    search_paths.insert(0, file_dir)
            self.module_loader = ModuleLoader(self.runtime, search_paths)
        return self.module_loader

    def _register_metaprogramming_functions(self):
        """Register metaprogramming introspection functions on the runtime.

        These closures capture the interpreter instance so NexusLang programs can
        query macro and compile-time constant registries at runtime.
        """
        interpreter = self

        def meta_macro_names():
            """Return a list of all defined macro names."""
            return list(interpreter.macros.keys())

        def meta_macro_exists(name):
            """Return True if a macro with the given name is defined."""
            return name in interpreter.macros

        def meta_macro_arg_count(name):
            """Return the parameter count of a macro, or -1 if undefined."""
            macro = interpreter.macros.get(name)
            if macro is None:
                return -1
            return len(macro.parameters)

        def meta_comptime_const_names():
            """Return a list of all compile-time constant names."""
            return list(interpreter.comptime_constants.keys())

        def meta_comptime_const_value(name):
            """Return the value of a compile-time constant, or None."""
            return interpreter.comptime_constants.get(name)

        def meta_comptime_const_exists(name):
            """Return True if a compile-time constant with the given name exists."""
            return name in interpreter.comptime_constants

        for fn_name, fn in (
            ("meta_macro_names", meta_macro_names),
            ("meta_macro_exists", meta_macro_exists),
            ("meta_macro_arg_count", meta_macro_arg_count),
            ("meta_comptime_const_names", meta_comptime_const_names),
            ("meta_comptime_const_value", meta_comptime_const_value),
            ("meta_comptime_const_exists", meta_comptime_const_exists),
        ):
            self.runtime.register_function(fn_name, fn)

    def interpret(self, ast_or_source, optimization_level=0):
        """Interpret the AST and execute the program.

        Args:
            ast_or_source: Either an AST Program node or a source code string
            optimization_level: AST optimization level (0=none, 1=basic, 2=standard, 3=aggressive)

        Returns:
            The result of program execution
        """
        # Support both AST and source string for backward compatibility
        if isinstance(ast_or_source, str):
            from ..parser.lexer import Lexer
            from ..parser.parser import Parser

            lexer = Lexer(ast_or_source)
            tokens = lexer.tokenize()
            parser = Parser(tokens, source=ast_or_source)
            ast = parser.parse()
        else:
            ast = ast_or_source

        # Apply AST-level optimizations when level > 0
        if optimization_level > 0 and isinstance(ast, Program):
            from ..optimizer import create_optimization_pipeline, OptimizationLevel
            _level_map = {
                1: OptimizationLevel.O1,
                2: OptimizationLevel.O2,
                3: OptimizationLevel.O3,
            }
            opt_level = _level_map.get(optimization_level, OptimizationLevel.O1)
            pipeline = create_optimization_pipeline(opt_level, interpreter_mode=True)
            ast = pipeline.run(ast)

        # Run type checking if enabled
        if self.enable_type_checking and isinstance(ast, Program):
            # CRITICAL: Sync runtime functions to type checker first
            # This ensures stdlib functions (graphics, math, etc.) are known to the type system
            self.sync_runtime_functions_to_type_checker()
            
            errors = self.type_checker.check_program(ast)
            if errors:
                error_msg = "\n".join(errors)
                raise NxlTypeError(
                    f"Type checking failed:\n{error_msg}",
                    error_type_key="type_mismatch",
                    full_source=self.source,
                )
            
        try:
            if isinstance(ast, Program):
                result = None
                for statement in ast.statements:
                    result = self.execute(statement)
                return result
            else:
                return self.execute(ast)
        except Exception as e:
            # Debugger hook: trace exception
            if self.debugger:
                self.debugger.trace_exception(e)
            raise
            
    def execute(self, node):
        """Execute a node in the AST."""
        # Track execution position (coverage, debugger, and error reporting)
        if hasattr(node, 'line'):
            _exec_file = getattr(node, 'file', self.current_file or '<unknown>')
            _exec_line = getattr(node, 'line', self.current_line or 0)
            self.current_file = _exec_file
            self.current_line = _exec_line
            # Debugger hook
            if self.debugger:
                self.debugger.trace_line(_exec_file, _exec_line)
            # Coverage hook
            if self._coverage_collector is not None and _exec_line > 0:
                self._coverage_collector.record(_exec_file, _exec_line)

        # Get node type key (prefer node_type attr; fall back to class name)
        if hasattr(node, 'node_type'):
            node_type = node.node_type
        else:
            node_type = node.__class__.__name__

        # Fast path: check per-instance cache first (bound method lookup is O(1))
        handler = self._dispatch_cache.get(node_type)
        if handler is None:
            # Look up method name in the class-level table
            method_name = Interpreter._DISPATCH_TABLE.get(node_type)
            if method_name is None:
                # Fall back to on-the-fly camel->snake conversion (handles
                # subclasses or newly added node types not yet in the table)
                method_name = "execute_" + _camel_to_snake(node_type)
            if hasattr(self, method_name):
                handler = getattr(self, method_name)
                self._dispatch_cache[node_type] = handler
            else:
                raise NotImplementedError(
                    f"Execution of {node_type} nodes is not implemented "
                    f"(looked for method: {method_name})"
                )

        return handler(node)
            
    def get_variable_or_none(self, name):
        """
        Cheap speculative variable lookup — returns None on miss instead of raising.

        Use this for hot-path checks (e.g. 'is this name a callable variable?')
        where the absence of the name is expected and normal.  This avoids the
        expensive NxlNameError construction (difflib.get_close_matches over all
        known names) that get_variable() incurs on every miss.
        """
        for scope in reversed(self.current_scope):
            if name in scope:
                return scope[name]
        if hasattr(self.runtime, 'constants') and name in self.runtime.constants:
            return self.runtime.constants[name]
        return None

    def get_variable(self, name):
        """Get a variable from the current scope with enhanced error reporting."""
        # Search from innermost to outermost scope
        for scope in reversed(self.current_scope):
            if name in scope:
                value = scope[name]
                # Moved value: accessing a moved variable is a hard error
                try:
                    from ..stdlib.smart_pointers import MovedValue
                    if isinstance(value, MovedValue):
                        raise NxlRuntimeError(
                            f"use of moved value: '{name}' was moved out and cannot be accessed",
                            error_type_key="runtime_error",
                            full_source=self.source,
                        )
                except ImportError:
                    pass
                return value
        
        # Check runtime constants (PI, E, TAU, etc.)
        if hasattr(self.runtime, 'constants') and name in self.runtime.constants:
            return self.runtime.constants[name]

        # Check self.functions (user defined functions)
        if name in self.functions:
            raise NameError("Function found but not returned by get_variable - caught in execute_identifier")
        
        # Variable not found - provide helpful error with suggestions
        available_names = []
        for scope in self.current_scope:
            available_names.extend(scope.keys())
        
        # Also include runtime functions and constants as potential suggestions
        if hasattr(self.runtime, 'functions'):
            available_names.extend(self.runtime.functions.keys())
        if hasattr(self.runtime, 'constants'):
            available_names.extend(self.runtime.constants.keys())
        
        raise NxlNameError(
            name,
            available_names=available_names,
            error_type_key="undefined_variable",
            full_source=self.source
        )
        
    # ------------------------------------------------------------------
    # Runtime type map: NexusLang type annotation string -> acceptable Python types.
    # Used by _validate_runtime_type() to enforce declared types at execution.
    # ------------------------------------------------------------------
    _RUNTIME_TYPE_MAP: Dict[str, Any] = {
        "Integer": (int,),
        "int": (int,),
        "Float": (int, float),   # int coerces to float
        "float": (int, float),
        "String": (str,),
        "string": (str,),
        "Text": (str,),
        "Boolean": (bool,),
        "bool": (bool,),
        "List": (list,),
        "Array": (list,),
        "Dictionary": (dict,),
        "Dict": (dict,),
    }

    def _resolve_type_annotation(self, name: str) -> str | None:
        """Look up the declared type annotation for *name* across all scopes.

        Searches from innermost scope outward (matching variable resolution).
        Returns the annotation string, or None if un-typed.
        """
        for scope in reversed(self._type_annotations):
            if name in scope:
                return scope[name]
        return None

    def _validate_runtime_type(self, value, type_annotation, var_name, node=None):
        """Raise NxlTypeError when *value* does not match *type_annotation*.

        Only string annotations that appear in *_RUNTIME_TYPE_MAP* are enforced;
        complex/custom types (smart pointers, user classes, generics with args)
        are left to the static type checker.
        """
        if value is None:
            return  # None / null is accepted for any type (nullable by default)
        if not isinstance(type_annotation, str):
            return  # Non-string annotations (AST nodes) handled by static checker

        # Strip generic parameters for map lookup: "List<Integer>" -> "List"
        base_type = type_annotation.split("<")[0].split(" ")[0]

        expected = self._RUNTIME_TYPE_MAP.get(base_type)
        if expected is None:
            return  # Unknown / user-defined type -- skip runtime check

        # bool is a subclass of int in Python; guard against assigning True to Integer.
        if base_type in ("Integer", "int") and isinstance(value, bool):
            line = getattr(node, "line_number", getattr(node, "line", None))
            raise NxlTypeError(
                f"Type error: Cannot assign value of type 'Boolean' "
                f"to variable '{var_name}' declared as '{type_annotation}'",
                line=line,
                error_type_key="type_mismatch",
                full_source=self.source,
            )

        if not isinstance(value, expected):
            actual_type = type(value).__name__
            # Map Python type names back to NexusLang names for clearer messages
            _py_to_nxl = {"str": "String", "int": "Integer", "float": "Float",
                           "bool": "Boolean", "list": "List", "dict": "Dictionary"}
            nxl_actual = _py_to_nxl.get(actual_type, actual_type)
            line = getattr(node, "line_number", getattr(node, "line", None))
            raise NxlTypeError(
                f"Type error: Cannot assign value of type '{nxl_actual}' "
                f"to variable '{var_name}' declared as '{type_annotation}'",
                line=line,
                error_type_key="type_mismatch",
                full_source=self.source,
            )

    def set_variable(self, name, value):
        """Set a variable in the current scope.

        Raises if the variable is currently borrowed (immutably or mutably),
        since mutation of a borrowed variable violates the borrow contract.

        When the variable has a declared type annotation, validates that the
        new value matches the expected type (runtime type enforcement).
        """
        borrow = self._borrow_tracker.get(name)
        if borrow and (borrow["immutable_count"] > 0 or borrow["is_mutable"]):
            kind = "mutably" if borrow["is_mutable"] else "immutably"
            raise NxlRuntimeError(
                f"cannot assign to '{name}': it is currently borrowed {kind}; "
                f"call 'drop borrow {name}' first",
                error_type_key="runtime_error",
                full_source=self.source,
            )

        # Runtime type enforcement: check declared annotation if present
        declared = self._resolve_type_annotation(name)
        if declared is not None:
            self._validate_runtime_type(value, declared, name)

        self.current_scope[-1][name] = value
        return value
        
    def enter_scope(self):
        """Enter a new scope."""
        self.current_scope.append({})
        self._type_annotations.append({})
        
    def exit_scope(self):
        """Exit the current scope.

        Performs RAII-style cleanup in reverse declaration order:
        1. Custom Drop trait methods on user objects and structs
        2. Rc / Arc / Weak reference-count decrements

        Drop order is LIFO (last declared, first dropped) to match
        Rust semantics and guarantee deterministic resource release.
        """
        if len(self.current_scope) > 1:
            scope = self.current_scope[-1]

            # Clean up borrows registered for variables in this scope.
            for var_name in scope:
                if var_name in self._borrow_tracker:
                    del self._borrow_tracker[var_name]

            # RAII: iterate values in *reverse* insertion order (Python 3.7+ dicts
            # preserve insertion order) so the last-declared variable is dropped first.
            from ..runtime.runtime import Object as _Object
            from ..runtime.structures import StructureInstance as _StructInst
            try:
                from ..stdlib.smart_pointers import RcValue, ArcValue, WeakValue
                _sp_types = (RcValue, ArcValue, WeakValue)
            except Exception:
                _sp_types = ()

            for val in reversed(list(scope.values())):
                try:
                    # 1. Custom Drop: user-defined drop methods on Object / Struct
                    #    Null the callback first to prevent re-entrant drops when
                    #    the drop body creates an inner scope that also exits.
                    if isinstance(val, _Object) and val._drop_method is not None:
                        drop_fn = val._drop_method
                        val._drop_method = None
                        drop_fn(val)
                    elif isinstance(val, _StructInst) and getattr(val, '_drop_method', None) is not None:
                        drop_fn = val._drop_method
                        val._drop_method = None
                        drop_fn(val)
                    # 2. Smart-pointer RAII drop
                    if _sp_types and isinstance(val, _sp_types):
                        val.drop()
                except Exception:
                    pass  # Never let RAII / drop errors abort cleanup

            self.current_scope.pop()
            if len(self._type_annotations) > 1:
                self._type_annotations.pop()
            
    # Module-related execution methods
    
    def execute_import_statement(self, node):
        """Execute an import statement."""
        try:
            # Get the current module path for relative imports
            current_path = getattr(self.runtime, 'module_path', None)
            
            # Load the module
            module = self._get_module_loader().load_module(node.module_name, current_path)
            
            # Use the alias if provided, otherwise use the module name
            module_name = node.alias if node.alias else node.module_name
            
            # If the module name contains path separators, use the last part
            if '/' in module_name:
                module_name = module_name.split('/')[-1]
            if '\\' in module_name:
                module_name = module_name.split('\\')[-1]
                
            # If the module name has dots (package.module), use the last part
            if '.' in module_name:
                module_name = module_name.split('.')[-1]
                
            # Add the module to the current scope
            self.set_variable(module_name, module)
            
            return None
        except (ImportError, CircularImportError) as e:
            raise NxlRuntimeError(
                message=f"Import error: {str(e)}",
                line=getattr(node, 'line', None),
                column=getattr(node, 'column', None),
                error_type_key="module_not_found" if isinstance(e, ImportError) else "circular_import",
                full_source=self.source,
            )
            
    def execute_selective_import(self, node):
        """Execute a selective import statement."""
        try:
            # Get the current module path for relative imports
            current_path = getattr(self.runtime, 'module_path', None)
            
            # Load the module
            module = self._get_module_loader().load_module(node.module_name, current_path)
            
            # Import each specified name
            for name in node.imported_names:
                # Check if the name exists in the module
                if hasattr(module, name):
                    # Add the name to the current scope
                    self.set_variable(name, getattr(module, name))
                else:
                    # Get available attributes for suggestions
                    available = [attr for attr in dir(module) if not attr.startswith('_')]
                    suggestions = get_close_matches(name, available, n=3, cutoff=0.6)
                    raise NxlNameError(
                        message=f"Module '{node.module_name}' has no attribute '{name}'",
                        name=name,
                        suggestions=suggestions,
                        line=getattr(node, 'line', None),
                        column=getattr(node, 'column', None),
                        error_type_key="undefined_attribute",
                        full_source=self.source
                    )
                    
            return None
        except (ImportError, CircularImportError) as e:
            raise NxlRuntimeError(
                message=f"Import error: {str(e)}",
                line=getattr(node, 'line', None),
                column=getattr(node, 'column', None),
                error_type_key="module_not_found" if isinstance(e, ImportError) else "circular_import",
                full_source=self.source,
            )
            
    def execute_module_access(self, node):
        """Execute a module access expression."""
        # Get the module
        try:
            module = self.get_variable(node.module_name)
        except NxlNameError:
            # Re-raise the name error with module context
            raise
            
        # Get the member from the module
        if hasattr(module, node.member_name):
            return getattr(module, node.member_name)
        else:
            # Get available attributes for suggestions
            available = [attr for attr in dir(module) if not attr.startswith('_')]
            suggestions = get_close_matches(node.member_name, available, n=3, cutoff=0.6)
            raise NxlNameError(
                message=f"Module '{node.module_name}' has no attribute '{node.member_name}'",
                name=node.member_name,
                suggestions=suggestions,
                line=getattr(node, 'line', None),
                column=getattr(node, 'column', None),
                error_type_key="undefined_attribute",
                full_source=self.source
            )
            
    def execute_private_declaration(self, node):
        """Execute a private declaration."""
        # Execute the declaration normally
        # The privacy is enforced by the module system
        return self.execute(node.declaration)
            
    # Statement execution methods
    
    def execute_variable_declaration(self, node):
        """Execute a variable declaration."""
        value = None
        if node.value:
            value = self.execute(node.value)

        # Handle allocator hints on collection types:
        #   set items to [] as List of Integer with allocator arena_alloc
        # The AllocatorHint tells us to route collection construction through the
        # specified allocator so that the backing memory is tracked by it, and
        # wrap the collection in an AllocatorTrackedList / AllocatorTrackedDict so
        # subsequent mutations (append, insert, remove, …) are also tracked.
        if hasattr(node, 'type_annotation') and isinstance(node.type_annotation, AllocatorHint):
            alloc_name = node.type_annotation.allocator_name
            try:
                allocator = self.get_variable(alloc_name)
            except NxlNameError:
                raise NxlRuntimeError(
                    f"Allocator '{alloc_name}' is not defined in the current scope",
                    line=getattr(node, 'line_number', None),
                    error_type_key="undefined_variable",
                    full_source=self.source,
                )
            if allocator is not None:
                from ..stdlib.allocators import wrap_collection_with_allocator
                # wrap_collection_with_allocator handles both list and dict
                # and calls allocator.allocate() for the initial elements.
                value = wrap_collection_with_allocator(value if value is not None else [], allocator)

        # Store the type annotation for runtime enforcement on reassignment
        if hasattr(node, 'type_annotation') and node.type_annotation is not None:
            ann = node.type_annotation
            if isinstance(ann, AllocatorHint):
                # The base type inside the allocator hint (if present)
                ann = getattr(ann, 'base_type', None)
            if isinstance(ann, str):
                self._type_annotations[-1][node.name] = ann
                # Validate the initial value against the declared type
                self._validate_runtime_type(value, ann, node.name, node)

        self.set_variable(node.name, value)
        return value

    def execute_index_assignment(self, node):
        """Execute an index assignment: set array[index] to value."""
        target = node.target

        # Get the container (array, dict, etc.)
        container = self.execute(target.array_expr)

        # Get the index
        index = self.execute(target.index_expr)

        # Get the value to assign
        value = self.execute(node.value)

        # Perform the assignment
        try:
            container[index] = value
            return value
        except (IndexError, KeyError, TypeError) as e:
            if isinstance(e, IndexError):
                raise NxlRuntimeError(
                    message=f"Index {index} is out of range for assignment",
                    line=getattr(node, 'line', None),
                    column=getattr(node, 'column', None),
                    error_type_key="index_out_of_range",
                    full_source=self.source
                )
            elif isinstance(e, TypeError):
                raise NxlTypeError(
                    message=f"Cannot assign to index on type {type(container).__name__}",
                    expected_type="list or dict",
                    got_type=type(container).__name__,
                    line=getattr(node, 'line', None),
                    column=getattr(node, 'column', None),
                    error_type_key="invalid_operation",
                    full_source=self.source
                )
            else:
                raise NxlRuntimeError(
                    message=f"Failed to assign to index {index}: {e}",
                    line=getattr(node, 'line', None),
                    column=getattr(node, 'column', None),
                    error_type_key="key_not_found",
                    full_source=self.source
                )
        
    @staticmethod
    def _ast_kind_to_hkt(kind_node):
        """Convert an AST KindAnnotation node to an HKT Kind object.

        Returns a :class:`nlpl.typesystem.hkt.Kind` instance (StarKind or ArrowKind).
        """
        from nexuslang.parser.ast import StarKindAnnotation, ArrowKindAnnotation
        from nexuslang.typesystem.hkt import STAR, ArrowKind as HKTArrowKind

        if isinstance(kind_node, StarKindAnnotation):
            return STAR
        if isinstance(kind_node, ArrowKindAnnotation):
            left = Interpreter._ast_kind_to_hkt(kind_node.left)
            right = Interpreter._ast_kind_to_hkt(kind_node.right)
            return HKTArrowKind(left, right)
        return STAR  # Fallback

    def execute_function_definition(self, node):
        """Execute a function definition.
        
        Stores the function definition and creates a callable function value
        that can be used as a first-class value (assigned to variables, passed as arguments, etc.)
        """
        self.functions[node.name] = node
        
        # Create a callable wrapper and store it as a variable too
        # This enables first-class function support
        function_value = self.create_function_wrapper(node)
        
        # Apply decorators if any
        if hasattr(node, 'decorators') and node.decorators:
            for decorator in reversed(node.decorators):  # Apply decorators bottom-up
                function_value = self.apply_decorator(decorator, function_value, node)
        
        self.set_variable(node.name, function_value)
        
        return node.name
    
    def apply_decorator(self, decorator_node, function_value, function_def):
        """Apply a decorator to a function."""
        from nexuslang.decorators import get_decorator
        
        # Evaluate decorator arguments
        args = {}
        for arg_name, arg_expr in decorator_node.arguments.items():
            args[arg_name] = self.execute(arg_expr)
        
        # Get the decorator implementation (built-in first, then user-defined NexusLang function)
        decorator_func = get_decorator(decorator_node.name)
        if decorator_func is None:
            # Check if this is a declared attribute type
            if decorator_node.name in self.attribute_definitions:
                prop_vals = {}
                prop_defs = self.attribute_definitions[decorator_node.name]["properties"]
                if "_args" in decorator_node.arguments:
                    positional = [self.execute(a) for a in decorator_node.arguments["_args"]]
                    for i, val in enumerate(positional):
                        if i < len(prop_defs):
                            prop_vals[prop_defs[i][0]] = val
                for arg_name, arg_expr in decorator_node.arguments.items():
                    if arg_name != "_args":
                        prop_vals[arg_name] = self.execute(arg_expr)
                if not hasattr(function_value, '_applied_attributes'):
                    function_value._applied_attributes = {}
                function_value._applied_attributes[decorator_node.name] = prop_vals
                # Mirror to runtime for reflection
                fname = getattr(function_def, 'name', None) or getattr(function_value, 'name', None)
                if fname:
                    if fname not in self.runtime._function_attributes:
                        self.runtime._function_attributes[fname] = {}
                    self.runtime._function_attributes[fname][decorator_node.name] = prop_vals
                return function_value
            # Try user-defined NexusLang function as decorator
            nxl_func = self.functions.get(decorator_node.name)
            if nxl_func is None:
                try:
                    nxl_func = self.get_variable(decorator_node.name)
                except Exception:
                    nxl_func = None
            if nxl_func is not None and hasattr(nxl_func, 'body'):
                self.enter_scope()
                try:
                    if nxl_func.parameters:
                        self.set_variable(nxl_func.parameters[0].name, function_value)
                    result = None
                    try:
                        for stmt in nxl_func.body:
                            result = self.execute(stmt)
                    except ReturnException as ret:
                        result = ret.value
                    return result if result is not None else function_value
                finally:
                    self.exit_scope()
            raise NxlRuntimeError(
                f"Unknown decorator: @{decorator_node.name}",
                line=decorator_node.line,
                error_type_key="undefined_function",
                full_source=self.source,
            )
        
        # Apply decorator based on type
        # Decorators that take arguments need to be called first to get the actual decorator
        if args and decorator_node.name in ("deprecated", "retry", "validate_args"):
            # Call with arguments to get the actual decorator
            actual_decorator = decorator_func(**args)
            return actual_decorator(function_value)
        else:
            # Decorator without arguments or simple decorator
            return decorator_func(function_value)
    
    def execute_async_function_definition(self, node):
        """Execute an async function definition."""
        # Store async function similar to regular function
        # Mark it as async so we know to handle it differently when called
        self.functions[node.name] = node
        return node.name
    
    def execute_await_expression(self, node):
        """Execute an await expression."""
        import asyncio
        
        # Execute the expression (should be an async function call or coroutine)
        result = self.execute(node.expression)
        
        # If result is a coroutine, run it
        if asyncio.iscoroutine(result):
            # Get or create event loop
            try:
                loop = asyncio.get_running_loop()
            except RuntimeError:
                # No running loop, create one
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)
                result = loop.run_until_complete(result)
                loop.close()
            else:
                # Already in async context - this shouldn't happen in interpreter
                # but handle it gracefully
                result = asyncio.run(result)
        
        return result

    def execute_channel_creation(self, node):
        """Execute channel creation: create channel."""
        return _Channel()

    def execute_send_statement(self, node):
        """Execute sending a value to a channel."""
        value = self.execute(node.value)
        channel = self.execute(node.channel)

        if not isinstance(channel, _Channel):
            raise NxlTypeError(
                f"send target must be a channel, got {type(channel).__name__}",
                error_type_key="type_mismatch",
                full_source=self.source,
            )

        channel.send(value)
        return None

    def execute_close_statement(self, node):
        """Execute closing a channel."""
        channel = self.execute(node.channel)

        if not isinstance(channel, _Channel):
            raise NxlTypeError(
                f"close target must be a channel, got {type(channel).__name__}",
                error_type_key="type_mismatch",
                full_source=self.source,
            )

        channel.close()
        return None

    def execute_receive_expression(self, node):
        """Execute receiving a value from a channel."""
        channel = self.execute(node.channel)

        if not isinstance(channel, _Channel):
            raise NxlTypeError(
                f"receive source must be a channel, got {type(channel).__name__}",
                error_type_key="type_mismatch",
                full_source=self.source,
            )

        return channel.receive()
        
    def execute_if_statement(self, node):
        """Execute an if statement."""
        condition = self.execute(node.condition)
        
        # Don't create new scope - if statements should access outer scope variables
        if condition:
            result = None
            for statement in node.then_block:
                result = self.execute(statement)
            return result
        elif node.else_block:
            result = None
            for statement in node.else_block:
                result = self.execute(statement)
            return result
            
    def execute_while_loop(self, node):
        """Execute a while loop."""
        result = None
        
        # Don't create new scope - while loops should access outer scope variables
        try:
            while self.execute(node.condition):
                try:
                    for statement in node.body:
                        result = self.execute(statement)
                except ContinueException:
                    # Continue to next iteration
                    continue
        except BreakException:
            # Break out of loop
            pass
            
        return result
        
    def execute_for_loop(self, node):
        """Execute a for loop."""
        result = None
        iterable = self.execute(node.iterable)

        # Optional enumerate-style index variable from: for each item with index i in ...
        index_var = getattr(node, 'index_var', None)

        # Don't create new scope - for loops should access outer scope variables like while loops
        try:
            if index_var:
                for i, item in enumerate(iterable):
                    self.set_variable(node.iterator, item)
                    self.set_variable(index_var, i)

                    try:
                        for statement in node.body:
                            result = self.execute(statement)
                    except ContinueException:
                        continue
            else:
                for item in iterable:
                    # Set the iterator variable in the current scope
                    self.set_variable(node.iterator, item)

                    try:
                        for statement in node.body:
                            result = self.execute(statement)
                    except ContinueException:
                        # Continue to next iteration
                        continue
        except BreakException:
            # Break out of loop
            pass

        return result

    def execute_parallel_for_loop(self, node):
        """Execute a parallel for-each loop.

        Each iteration runs in its own thread from a thread pool.
        Iterations are isolated: each one receives a snapshot of the enclosing
        scope and any variables written inside an iteration are local to that
        iteration only (preventing data races by construction).

        Break/continue are not supported inside parallel loops.

        Example NexusLang::

            parallel for each item in data
                set result to process item
                print text result
            end
        """
        # Use self.__class__ to avoid a circular import of interpreter.py
        _Interp = self.__class__
        import copy

        iterable = self.execute(node.iterable)
        if not hasattr(iterable, '__iter__'):
            raise NxlRuntimeError(
                f"'parallel for each' requires an iterable collection, got {type(iterable).__name__}",
                line=getattr(node, 'line_number', None),
                error_type_key="type_mismatch",
                full_source=self.source,
            )
        items = list(iterable)
        if not items:
            return None

        # Build a shallow snapshot of the current scope for read-only access
        scope_snapshot = {}
        for scope_level in self.current_scope:
            scope_snapshot.update(scope_level)

        errors = []
        errors_lock = __import__("threading").Lock()

        cpu_count = __import__("multiprocessing").cpu_count()
        max_workers = max(1, min(cpu_count, len(items)))

        def run_iteration(item):
            """Execute the loop body for one item in its own interpreter child."""
            child = _Interp(self.runtime)
            # Populate child scope with parent snapshot (read-only bindings)
            child.enter_scope()
            for name, val in scope_snapshot.items():
                child.set_variable(name, val)
            # Set the loop variable for this iteration
            child.set_variable(node.var_name, item)
            # Copy function definitions so the body can call functions
            child.functions.update(self.functions)
            child.classes.update(self.classes)
            child.source = self.source
            try:
                for stmt in node.body:
                    child.execute(stmt)
            except Exception as exc:
                with errors_lock:
                    errors.append(exc)
            finally:
                child.exit_scope()

        with _cf.ThreadPoolExecutor(max_workers=max_workers) as pool:
            futures = [pool.submit(run_iteration, item) for item in items]
            _cf.wait(futures)

        if errors:
            raise NxlRuntimeError(
                f"Error in parallel for each iteration: {errors[0]}",
                line=getattr(node, 'line_number', None),
                error_type_key="runtime_error",
                full_source=self.source,
            )
        return None

    def execute_conditional_compilation_block(self, node):
        """Execute a compile-time conditional block.

        Evaluates the condition against the current host platform (or the
        CompileTarget set in the runtime, if any) and runs the matching branch.

        Even though this is called at *runtime* in interpreter mode, the
        semantics are identical to a compile-time branch: only the selected
        branch is executed; the other branch is silently skipped.

        Example NexusLang::

            when target os is "linux"
                print text "Running on Linux"
            otherwise
                print text "Not Linux"
            end
        """
        from nexuslang.compiler.preprocessor import evaluate_condition, host_target

        # Allow the runtime to override the default host target for
        # cross-compilation simulation: runtime.compile_target
        target = getattr(self.runtime, "compile_target", None) or host_target()

        condition_holds = evaluate_condition(
            node.condition_type,
            str(node.condition_value),
            target,
        )

        chosen_branch = node.body if condition_holds else (node.else_body or [])

        # Execute in the *current* scope (not a new inner scope).
        # Conditional compilation blocks are equivalent to C's #ifdef: variables
        # declared inside must be visible after the `end` keyword.
        result = None
        for stmt in chosen_branch:
            result = self.execute(stmt)
        return result

    def execute_repeat_n_times_loop(self, node):
        """Execute a repeat-n-times loop."""
        result = None

        # Evaluate the count expression to get number of iterations
        count = self.execute(node.count)

        # Ensure count is an integer
        if not isinstance(count, (int, float)):
            raise NxlTypeError(
                f"Repeat count must be a number, got {type(count).__name__}",
                expected_type="Integer or Float",
                got_type=type(count).__name__,
                line=getattr(node, 'line', None),
                error_type_key="type_mismatch",
                full_source=self.source,
            )
        
        count = int(count)
        
        # Validate count is non-negative
        if count < 0:
            raise NxlRuntimeError(
                f"Repeat count must be non-negative, got {count}",
                line=getattr(node, 'line', None),
                error_type_key="invalid_operation",
                full_source=self.source,
            )
        
        # Execute the loop body 'count' times
        # DO NOT create a new scope - loops should access outer scope variables
        try:
            for iteration in range(count):
                try:
                    for statement in node.body:
                        result = self.execute(statement)
                except ContinueException:
                    # Continue to next iteration
                    continue
        except BreakException:
            # Break out of loop
            pass
        
        return result
    
    def execute_repeat_while_loop(self, node):
        """Execute a repeat-while loop (natural language while loop)."""
        result = None
        loop_completed = False
        
        # Execute the loop while condition is true
        # DO NOT create a new scope - loops should access outer scope variables
        try:
            while self.execute(node.condition):
                try:
                    for statement in node.body:
                        result = self.execute(statement)
                except ContinueException:
                    # Continue to next iteration
                    continue
        except BreakException:
            # Break was called, loop did not complete normally
            pass
        else:
            # Loop completed without break
            loop_completed = True
        
        # Execute else block if loop completed without break
        if loop_completed and node.else_body:
            for statement in node.else_body:
                result = self.execute(statement)
        
        return result
        
    def execute_switch_statement(self, node):
        """Execute a switch statement for multi-way branching.

        Evaluates the switch expression once and compares it against each case value.
        Executes the matching case body.  When a case body ends with a ``fallthrough``
        statement, execution continues into the next case's body without re-evaluating
        the condition (C-style fallthrough semantics).  Without a ``fallthrough``,
        execution stops after the first matching case.  The default case runs when no
        case matches.
        """
        switch_value = self.execute(node.expression)

        # Find the first matching case index.
        matched_index = None
        for i, case in enumerate(node.cases):
            case_value = self.execute(case.value)
            if switch_value == case_value:
                matched_index = i
                break

        if matched_index is None:
            # No case matched — run default if present.
            if node.default_case:
                result = None
                for statement in node.default_case:
                    result = self.execute(statement)
                return result
            return None

        # Execute matched case and handle fallthrough into subsequent cases.
        result = None
        for case in node.cases[matched_index:]:
            fell_through = False
            try:
                for statement in case.body:
                    result = self.execute(statement)
            except FallthroughException:
                fell_through = True
            if not fell_through:
                return result
        # Fell through all remaining cases — run default if present.
        if node.default_case:
            try:
                for statement in node.default_case:
                    result = self.execute(statement)
            except FallthroughException:
                pass
        return result
    
    def execute_match_expression(self, node):
        """Execute a pattern matching expression.
        
        Evaluates the match expression and tries to match against each case pattern.
        When a pattern matches, binds any variables and executes the case body.
        Supports:
        - Literal patterns: case 42, case "hello"
        - Identifier patterns: case x (binds x to the value)
        - Wildcard pattern: case _ (matches anything)
        - Option patterns: case Some with value, case None
        - Result patterns: case Ok with value, case Err with error
        - Variant patterns: case Ok value, case Some x
        
        Returns the result of the matching case body, or None if no match.
        """
        # Evaluate the expression to match against
        match_value = self.execute(node.expression)
        
        # Try each case in order (first match wins)
        for case in node.cases:
            # Check if pattern matches and get any bindings
            matched, bindings = self._match_pattern(case.pattern, match_value)
            
            if matched:
                # Check guard condition if present
                if case.guard:
                    # Enter scope for bindings during guard evaluation
                    self.enter_scope()
                    try:
                        # Bind variables for guard evaluation
                        for var_name, var_value in bindings.items():
                            self.set_variable(var_name, var_value)
                        
                        # Evaluate guard condition
                        guard_result = self.execute(case.guard)
                        
                        # Guard must be truthy to proceed
                        if not guard_result:
                            continue
                    finally:
                        self.exit_scope()
                
                # Pattern matched (and guard passed if present)
                # Enter new scope and bind pattern variables
                self.enter_scope()
                try:
                    # Bind all pattern variables
                    for var_name, var_value in bindings.items():
                        self.set_variable(var_name, var_value)
                    
                    # Execute the case body
                    result = None
                    for statement in case.body:
                        result = self.execute(statement)
                    
                    return result
                finally:
                    self.exit_scope()
        
        # No pattern matched
        raise NxlRuntimeError(
            f"Non-exhaustive pattern match: no pattern matched the value {match_value!r}",
            line=node.line_number,
            error_type_key="invalid_operation",
            full_source=self.source
        )
    
    def _match_pattern(self, pattern, value):
        """Check if a pattern matches a value.
        
        Args:
            pattern: Pattern AST node
            value: Runtime value to match against
            
        Returns:
            Tuple of (matched: bool, bindings: dict)
            - matched: True if pattern matches value
            - bindings: Dict of variable names to values for identifier patterns
        """
        # Literal pattern: case 42, case "hello", case true
        if isinstance(pattern, LiteralPattern):
            # pattern.value is a Literal AST node - need to extract the actual value
            if hasattr(pattern.value, 'value'):
                pattern_value = pattern.value.value
            else:
                pattern_value = pattern.value
            
            # For string literals, strip surrounding quotes if present
            if isinstance(pattern_value, str) and len(pattern_value) >= 2:
                if (pattern_value[0] == '"' and pattern_value[-1] == '"') or \
                   (pattern_value[0] == "'" and pattern_value[-1] == "'"):
                    pattern_value = pattern_value[1:-1]
            
            matched = (pattern_value == value)
            return (matched, {})
        
        # Identifier pattern: case x (binds x to the value)
        elif isinstance(pattern, IdentifierPattern):
            # Always matches, binds the identifier to the value
            return (True, {pattern.name: value})
        
        # Wildcard pattern: case _ (matches anything, no binding)
        elif isinstance(pattern, WildcardPattern):
            return (True, {})
        
        # Option pattern: case Some with value, case None
        elif isinstance(pattern, OptionPattern):
            # Import Option class for type checking
            from nexuslang.stdlib.option_result import Option
            
            if not isinstance(value, Option):
                return (False, {})
            
            # Check variant type
            if pattern.variant == "Some":
                if value.is_some():
                    # Bind the contained value if binding specified
                    if pattern.binding:
                        return (True, {pattern.binding: value.unwrap()})
                    return (True, {})
                return (False, {})
            
            elif pattern.variant == "None":
                if value.is_none():
                    return (True, {})
                return (False, {})
        
        # Result pattern: case Ok with value, case Err with error
        elif isinstance(pattern, ResultPattern):
            # Import Result class for type checking
            from nexuslang.stdlib.option_result import Result
            
            if not isinstance(value, Result):
                return (False, {})
            
            # Check variant type
            if pattern.variant == "Ok":
                if value.is_ok():
                    # Bind the success value if binding specified
                    if pattern.binding:
                        return (True, {pattern.binding: value.unwrap()})
                    return (True, {})
                return (False, {})
            
            elif pattern.variant == "Err":
                if value.is_err():
                    # Bind the error value if binding specified
                    if pattern.binding:
                        return (True, {pattern.binding: value.unwrap_err()})
                    return (True, {})
                return (False, {})
        
        # Variant pattern: case Ok value, case Some x (generic variant matching)
        elif isinstance(pattern, VariantPattern):
            # Try to match against Option/Result types first
            from nexuslang.stdlib.option_result import Option, Result
            
            if isinstance(value, Option):
                # Match Option variants
                if pattern.variant_name == "Some" and value.is_some():
                    bindings = {}
                    if pattern.bindings:
                        # Bind the contained value to the first binding name
                        bindings[pattern.bindings[0]] = value.unwrap()
                    return (True, bindings)
                elif pattern.variant_name == "None" and value.is_none():
                    return (True, {})
                return (False, {})
            
            elif isinstance(value, Result):
                # Match Result variants
                if pattern.variant_name == "Ok" and value.is_ok():
                    bindings = {}
                    if pattern.bindings:
                        # Bind the success value to the first binding name
                        bindings[pattern.bindings[0]] = value.unwrap()
                    return (True, bindings)
                elif pattern.variant_name == "Err" and value.is_err():
                    bindings = {}
                    if pattern.bindings:
                        # Bind the error value to the first binding name
                        bindings[pattern.bindings[0]] = value.unwrap_err()
                    return (True, bindings)
                return (False, {})
            
            # Future: Support for custom variant types (enums, structs)
            # For now, just fail to match
            return (False, {})
        
        # Unknown pattern type
        else:
            raise NxlRuntimeError(
                f"Unknown pattern type: {type(pattern).__name__}",
                line=getattr(pattern, 'line_number', None),
                error_type_key="invalid_operation",
                full_source=self.source,
            )
    
    def execute_memory_allocation(self, node):
        """Execute memory allocation."""
        initial_value = None
        if node.initial_value:
            initial_value = self.execute(node.initial_value)
            
        # Simulate memory allocation using the runtime
        pointer = self.runtime.allocate_memory(node.var_type, initial_value)
        self.set_variable(node.identifier, pointer)
        
        return pointer
        
    def execute_memory_deallocation(self, node):
        """Execute memory deallocation."""
        pointer = self.get_variable(node.identifier)
        self.runtime.free_memory(pointer)
        return None
        
    def execute_class_definition(self, node):
        """Execute a class definition."""
        # Store the AST node. It contains generic_parameters if defined.
        self.classes[node.name] = node
        # Apply class-level decorators (e.g., @derive, @singleton)
        if hasattr(node, 'decorators') and node.decorators:
            for decorator in reversed(node.decorators):
                self._apply_class_decorator(decorator, node)
        return node.name

    def _apply_class_decorator(self, decorator_node, class_node):
        """Apply a decorator to a class definition."""
        name = decorator_node.name
        if name == "derive":
            self._apply_derive_to_class(decorator_node, class_node)
        elif name == "singleton":
            # Mark class for singleton instantiation
            class_node._singleton_instance = None
            class_node._is_singleton = True
        elif name in self.attribute_definitions:
            # Store attribute metadata on class node and mirror to runtime for reflection
            if not hasattr(class_node, '_applied_attributes'):
                class_node._applied_attributes = {}
            prop_vals = {}
            prop_defs = self.attribute_definitions[name]["properties"]
            if "_args" in decorator_node.arguments:
                positional = [self.execute(a) for a in decorator_node.arguments["_args"]]
                for i, val in enumerate(positional):
                    if i < len(prop_defs):
                        prop_vals[prop_defs[i][0]] = val
            for arg_name, arg_expr in decorator_node.arguments.items():
                if arg_name != "_args":
                    prop_vals[arg_name] = self.execute(arg_expr)
            class_node._applied_attributes[name] = prop_vals
            # Mirror to runtime for reflection queries (keyed by class name)
            if class_node.name not in self.runtime._class_attributes:
                self.runtime._class_attributes[class_node.name] = {}
            self.runtime._class_attributes[class_node.name][name] = prop_vals
        else:
            # Try user-defined class decorator function
            nxl_func = self.functions.get(name)
            if nxl_func is not None and hasattr(nxl_func, 'body'):
                # Pass the class node (as a string name) to the decorator
                self.enter_scope()
                try:
                    if nxl_func.parameters:
                        self.set_variable(nxl_func.parameters[0].name, class_node.name)
                    for stmt in nxl_func.body:
                        self.execute(stmt)
                finally:
                    self.exit_scope()

    def _apply_derive_to_class(self, decorator_node, class_node):
        """Generate methods for @derive(Trait1, Trait2, ...) on a class."""
        if not hasattr(class_node, '_derived_methods'):
            class_node._derived_methods = {}

        # Collect trait names from decorator arguments
        traits = []
        # Handle @derive(Trait1, Trait2) style - parser stores as {"_args": ["Trait1", "Trait2"]}
        if "_args" in decorator_node.arguments:
            for t in decorator_node.arguments["_args"]:
                if isinstance(t, str):
                    traits.append(t)
        else:
            # Legacy: @derive with TraitName value (keyword-style args)
            for arg_name, arg_expr in decorator_node.arguments.items():
                traits.append(arg_name)
        # Also handle positional args stored as list (in case parser uses positional)
        if hasattr(decorator_node, 'positional_args') and decorator_node.positional_args:
            for arg_expr in decorator_node.positional_args:
                try:
                    val = self.execute(arg_expr)
                    if isinstance(val, str):
                        traits.append(val)
                except Exception:
                    pass

        for trait in traits:
            if trait == "DebugPrint":
                def make_to_string(cn):
                    def to_string(obj, *args):
                        props = ", ".join(f"{k}={v!r}" for k, v in obj.properties.items())
                        return f"{cn}({props})"
                    return to_string
                def make_debug_print(cn):
                    def debug_print(obj, *args):
                        props = ", ".join(f"{k}={v!r}" for k, v in obj.properties.items())
                        print(f"{cn}({props})")
                        return None
                    return debug_print
                class_node._derived_methods["to_string"] = make_to_string(class_node.name)
                class_node._derived_methods["debug_print"] = make_debug_print(class_node.name)
            elif trait == "Equality":
                def equals(obj, other, *args):
                    if not hasattr(other, 'properties'):
                        return False
                    return obj.properties == other.properties
                class_node._derived_methods["equals"] = equals
            elif trait == "Clone":
                def clone(obj, *args):
                    import copy
                    return copy.deepcopy(obj)
                class_node._derived_methods["clone"] = clone
            elif trait == "Hash":
                def hash_code(obj, *args):
                    try:
                        return hash(tuple(sorted(obj.properties.items())))
                    except TypeError:
                        return id(obj)
                class_node._derived_methods["hash_code"] = hash_code
            elif trait == "Default":
                def default(obj, *args):
                    for key in obj.properties:
                        obj.properties[key] = None
                    return obj
                class_node._derived_methods["default"] = default
    
    def execute_interface_definition(self, node):
        """Execute an interface definition.
        
        Interfaces are stored for type checking purposes.
        They don't create runtime objects, just define contracts.
        """
        # Store the interface definition for type checking
        if not hasattr(self, 'interfaces'):
            self.interfaces = {}
        self.interfaces[node.name] = node
        return node.name
    
    def execute_abstract_class_definition(self, node):
        """Execute an abstract class definition.
        
        Abstract classes are like regular classes but cannot be instantiated directly.
        They can contain both abstract methods (no implementation) and concrete methods.
        """
        # Store like a regular class - the type system will enforce abstract rules
        self.classes[node.name] = node
        return node.name
    
    def execute_trait_definition(self, node):
        """Execute a trait definition.
        
        Traits define reusable behavior that can be mixed into classes.
        They contain method signatures (required_methods) and optionally 
        default implementations (provided_methods).
        
        Traits are similar to interfaces but can provide default implementations.
        They support composition and multiple trait usage in a single class.
        """
        # Store trait definition for type checking and class composition
        if not hasattr(self, 'traits'):
            self.traits = {}
        self.traits[node.name] = node
        return node.name
    
    def execute_struct_definition(self, node):
        """Execute a struct definition.
        
        Registers the struct as a type that can be instantiated.
        Structs are like classes but with explicit memory layout control.
        Supports packed structs and explicit alignment.
        """
        # Extract fields and their types from StructField nodes
        fields = []
        original_fields = []
        for field in node.fields:
            # field is StructField with name, type_annotation, bit_width
            type_name = field.type_annotation
            if not type_name:
                type_name = "Pointer"  # Default to pointer size if unknown

            fields.append((field.name, type_name, field.bit_width))
            original_fields.append((field.name, type_name))
            
        # Create runtime definition with packed and alignment support
        definition = RuntimeStructDefinition(
            node.name, 
            fields, 
            packed=node.packed if hasattr(node, 'packed') else False,
            alignment=node.alignment if hasattr(node, 'alignment') else None
        )
        
        # Store original fields for FFI marshalling
        definition._original_fields = original_fields
        definition._original_field_specs = fields
        
        self.classes[node.name] = definition
        
        # Also store methods if this is a C++-style struct with methods
        if hasattr(node, 'methods') and node.methods:
            # Store methods on the definition for later binding
            definition.methods = node.methods
        
        return node.name
    
    def execute_sizeof_expression(self, node):
        """Execute sizeof expression."""
        # Clean attribute access: try 'target' first, then 'type_name'
        target = getattr(node, 'target', getattr(node, 'type_name', None))
        
        # Resolve Identifier to string name if possible
        target_name = None
        if hasattr(target, 'name'):
            target_name = target.name
        elif isinstance(target, str):
            target_name = target
            
        # 1. Look up Type Name (e.g., "Point", "Integer")
        if target_name:
            # Check user-defined types
            if target_name in self.classes:
                defn = self.classes[target_name]
                if hasattr(defn, 'size'):
                    return defn.size
            
            # Check primitive types
            from ..runtime.structures import PrimitiveType
            if target_name in ("Integer", "Int"): return 8
            if target_name in ("Float", "Double"): return 8
            if target_name in ("Boolean", "Bool"): return 1
            if target_name == "Byte": return 1
            if target_name == "Char": return 1
            
        # 2. Evaluate Expression (e.g., variable instance)
        try:
            val = self.execute(target)
            from ..runtime.structures import StructureInstance
            if isinstance(val, StructureInstance):
                return val.definition.size
        except:
            pass
            
        return 8  # Default pointer size.
    
    def execute_union_definition(self, node):
        """Execute a union definition.
        
        Registers the union as a type. All fields share the same memory location.
        Size is determined by the largest field.
        """
        # Extract fields and their types from StructField nodes
        fields = []
        original_fields = []
        for field in node.fields:
            # field is StructField with name, type_annotation, bit_width
            type_name = field.type_annotation
            if not type_name:
                type_name = "Pointer"  # Default to pointer size if unknown
            fields.append((field.name, type_name, field.bit_width))
            original_fields.append((field.name, type_name))
            
        # Create runtime definition and store in classes map
        # Unions don't support packed/alignment (they're already minimal)
        definition = RuntimeUnionDefinition(node.name, fields)
        definition._original_fields = original_fields
        definition._original_field_specs = fields
        self.classes[node.name] = definition
        return node.name
    
    def execute_enum_definition(self, node):
        """Execute an enum definition.
        
        Creates an enum type with named constant values.
        The enum is accessible as a namespace with member values.
        Supports auto-numbering, explicit values, and string enums.
        
        Examples:
            enum Color
                Red
                Green
                Blue
            end
            # Auto-numbered: Red=0, Green=1, Blue=2
            
            enum Status
                Success = 0
                Error = 1
                Pending = 2
            end
            
            enum LogLevel
                Debug = "DEBUG"
                Info = "INFO"
                Error = "ERROR"
            end
            
            # Access: Color.Red, Status.Success, LogLevel.Debug
        """
        # Create an enum class-like object
        class EnumType:
            """Runtime representation of an enum type."""
            def __init__(self, name, members):
                self._name = name
                self._members = members
                self._value_to_name = {v: k for k, v in members.items()}
                # Add members as attributes
                for member_name, member_value in members.items():
                    setattr(self, member_name, member_value)
            
            def __repr__(self):
                return f"<Enum {self._name}>"
            
            def __getitem__(self, key):
                """Allow dict-like access: Color["Red"]"""
                return self._members[key]
            
            def __contains__(self, value):
                """Check if value is in enum: 0 in Color"""
                return value in self._value_to_name
            
            def get_name(self, value):
                """Get name for a value: Color.get_name(0) -> "Red" """
                return self._value_to_name.get(value)
            
            def values(self):
                """Get all enum values"""
                return list(self._members.values())
            
            def names(self):
                """Get all enum names"""
                return list(self._members.keys())
        
        # Build member dictionary with auto-numbering
        members = {}
        auto_value = 0
        
        for member in node.members:
            if hasattr(member, 'value') and member.value is not None:
                # Explicit value provided
                value = self.execute(member.value)
                # Update auto_value for next implicit member
                if isinstance(value, (int, float)):
                    auto_value = int(value) + 1
            else:
                # Auto-numbered value
                value = auto_value
                auto_value += 1
            
            members[member.name] = value
        
        # Create enum type instance
        enum_type = EnumType(node.name, members)
        
        # Store the enum type in classes (for type checking) and variables (for access)
        self.classes[node.name] = node
        self.set_variable(node.name, enum_type)
        
        return node.name
        
    def execute_concurrent_execution(self, node):
        """Execute concurrent tasks."""
        return self.runtime.run_concurrently([
            lambda task=task: self.execute(task) for task in node.tasks
        ])
        
    def execute_try_catch(self, node):
        """Execute a try-catch block."""
        try:
            # Execute all statements in try block
            result = None
            for statement in node.try_block:
                result = self.execute(statement)
            return result
        except NLPLUserException as e:
            # Check if exception type matches the catch block
            # If no type is specified, it matches all NexusLang user exceptions
            matches = True
            if node.exception_type:
                # Type name "Error" matches everything
                if node.exception_type != "Error" and node.exception_type != e.exception_type:
                    matches = False
            
            if matches:
                # Store for re-raising
                self.last_exception = e

                # Bind exception variable in the current scope (no inner scope, so
                # assignments inside the catch body update variables in the outer scope)
                exc_value = e.message if e.message else e.exception_type
                exc_var = node.exception_var if node.exception_var else "error"
                self.set_variable(exc_var, exc_value)

                # Bind exception properties if specified (e.g., "catch error with message")
                if hasattr(node, 'exception_properties') and node.exception_properties:
                    for prop in node.exception_properties:
                        if prop == 'message':
                            self.set_variable(prop, e.message if e.message else str(exc_value))
                        elif prop == 'type':
                            self.set_variable(prop, e.exception_type)
                        elif prop == 'code':
                            self.set_variable(prop, getattr(e, 'code', None))
                        else:
                            self.set_variable(prop, getattr(e, prop, None))

                result = None
                for statement in node.catch_block:
                    result = self.execute(statement)
                return result
            else:
                # Re-raise NLPLUserException if type doesn't match
                raise e
        except (NxlRuntimeError, NxlTypeError, NxlNameError) as e:
            # Handle runtime errors from the interpreter itself
            # Check if exception type matches based on nxl_type
            matches = True
            if node.exception_type and node.exception_type != "Error":
                if node.exception_type != getattr(e, 'nxl_type', None):
                    matches = False
            
            if matches:
                exc_message = str(e)
                exc_var = node.exception_var if node.exception_var else "error"
                self.set_variable(exc_var, exc_message)

                # Bind exception properties if specified
                if hasattr(node, 'exception_properties') and node.exception_properties:
                    for prop in node.exception_properties:
                        if prop == 'message':
                            self.set_variable(prop, exc_message)
                        elif prop == 'type':
                            self.set_variable(prop, getattr(e, 'nxl_type', type(e).__name__))
                        else:
                            self.set_variable(prop, getattr(e, prop, None))

                result = None
                for statement in node.catch_block:
                    result = self.execute(statement)
                return result
            else:
                raise e
        except (ReturnException, BreakException, ContinueException, YieldException):
            # Control flow exceptions must propagate -- never catch these
            raise
        except Exception as e:
            # Handle other Python exceptions (e.g., ZeroDivisionError)
            if node.exception_type and node.exception_type != "Error":
                raise e

            exc_message = str(e)
            exc_var = node.exception_var if node.exception_var else "error"
            self.set_variable(exc_var, exc_message)

            # Bind exception properties if specified (e.g., "catch error with message")
            if hasattr(node, 'exception_properties') and node.exception_properties:
                for prop in node.exception_properties:
                    if prop == 'message':
                        self.set_variable(prop, exc_message)
                    elif prop == 'type':
                        self.set_variable(prop, type(e).__name__)
                    else:
                        self.set_variable(prop, getattr(e, prop, None))

            result = None
            for statement in node.catch_block:
                result = self.execute(statement)
            return result

    def execute_raise_statement(self, node):
        """Execute a raise statement."""
        exception_type = node.exception_type
        message = None
        if node.message:
            message = self.execute(node.message)
            
        # If it's a re-raise: "raise Error" with no message
        if exception_type == "Error" and message is None:
            if self.last_exception:
                raise self.last_exception
            else:
                # Fallback to looking in scope if last_exception is lost
                try:
                    error_val = self.get_variable("error")
                    raise NLPLUserException("Error", str(error_val), getattr(node, 'line', None))
                except:
                    # No exception to re-raise
                    raise NxlRuntimeError(
                        message="Nothing to re-raise",
                        line=getattr(node, 'line', None),
                        error_type_key="invalid_operation",
                        full_source=self.source,
                    )
                
        raise NLPLUserException(exception_type, message, getattr(node, 'line', None))
    
    def _nxl_type_to_ctype(self, nxl_type):
        """Convert NexusLang type string to ctypes type."""
        import ctypes
        
        # Normalize type name
        type_name = str(nxl_type).lower().strip()
        
        # Map NexusLang types to ctypes types
        type_map = {
            'integer': ctypes.c_long,
            'int': ctypes.c_long,
            'i32': ctypes.c_int,
            'i64': ctypes.c_longlong,
            'float': ctypes.c_double,
            'double': ctypes.c_double,
            'f32': ctypes.c_float,
            'f64': ctypes.c_double,
            'boolean': ctypes.c_bool,
            'bool': ctypes.c_bool,
            'pointer': ctypes.c_void_p,
            'string': ctypes.c_char_p,
            'void': None,
        }
        
        if type_name in type_map:
            return type_map[type_name]
        
        # Default to void pointer for unknown types
        return ctypes.c_void_p
    
    def _struct_to_ctype(self, struct_def):
        """Convert NexusLang StructDefinition to ctypes.Structure class.
        
        Creates a dynamic ctypes.Structure class that matches the NexusLang struct layout.
        Handles nested structs, arrays, and proper field types.
        """
        import ctypes
        
        if not isinstance(struct_def, RuntimeStructDefinition):
            raise NxlTypeError(
                f"Expected RuntimeStructDefinition, got {type(struct_def)}",
                expected_type="RuntimeStructDefinition",
                got_type=type(struct_def).__name__,
                error_type_key="type_mismatch",
                full_source=self.source,
            )
        
        # Create fields list for ctypes.Structure
        # Format: [(field_name, ctype), ...]
        ctype_fields = []
        
        for field_name, field_obj in struct_def.fields.items():
            # Get the NexusLang type name from the original fields
            # We need to find the original type name from the definition
            type_name = None
            for fname, ftype in [(f[0], f[1]) for f in getattr(struct_def, '_original_fields', [])]:
                if fname == field_name:
                    type_name = ftype
                    break
            
            if not type_name:
                # Fallback: infer from struct format code
                code_to_type = {
                    'q': ctypes.c_longlong,  # INTEGER
                    'd': ctypes.c_double,     # FLOAT
                    '?': ctypes.c_bool,       # BOOLEAN
                    'b': ctypes.c_byte,       # BYTE
                    'c': ctypes.c_char,       # CHAR
                    'Q': ctypes.c_void_p,     # POINTER
                }
                ctype = code_to_type.get(field_obj.type_code, ctypes.c_void_p)
            else:
                # Check if this is a nested struct
                if type_name in self.classes and isinstance(self.classes[type_name], RuntimeStructDefinition):
                    # Recursive: convert nested struct
                    ctype = self._struct_to_ctype(self.classes[type_name])
                else:
                    # Regular type
                    ctype = self._nxl_type_to_ctype(type_name)
            
            ctype_fields.append((field_name, ctype))
        
        # Dynamically create ctypes.Structure subclass
        struct_class = type(
            struct_def.name,
            (ctypes.Structure,),
            {
                '_fields_': ctype_fields,
                '_pack_': 1 if struct_def.packed else None,
            }
        )
        
        # Handle explicit alignment
        if struct_def.explicit_alignment:
            struct_class._align_ = struct_def.explicit_alignment
        
        return struct_class
    
    def _structure_instance_to_cstruct(self, struct_instance, cstruct_class):
        """Convert NexusLang StructureInstance to ctypes.Structure instance.
        
        Copies all field values from NexusLang struct to C struct with proper type conversion.
        """
        import ctypes
        from nexuslang.runtime.structures import StructureInstance
        
        if not isinstance(struct_instance, StructureInstance):
            raise NxlTypeError(
                f"Expected StructureInstance, got {type(struct_instance)}",
                expected_type="StructureInstance",
                got_type=type(struct_instance).__name__,
                error_type_key="type_mismatch",
                full_source=self.source,
            )
        
        # Create C struct instance
        c_struct = cstruct_class()
        
        # Copy fields
        for field_name in struct_instance.definition.fields:
            nxl_value = struct_instance.get_field(field_name)
            
            # Handle nested structs
            if isinstance(nxl_value, StructureInstance):
                # Get the nested struct's ctype class
                nested_ctype = self._struct_to_ctype(nxl_value.definition)
                nested_c_struct = self._structure_instance_to_cstruct(nxl_value, nested_ctype)
                setattr(c_struct, field_name, nested_c_struct)
            else:
                # Regular field - direct assignment (ctypes handles conversion)
                setattr(c_struct, field_name, nxl_value)
        
        return c_struct
    
    def _cstruct_to_structure_instance(self, c_struct, struct_def):
        """Convert ctypes.Structure instance back to NexusLang StructureInstance.
        
        Creates a new NexusLang struct and populates it from the C struct.
        """
        from nexuslang.runtime.structures import StructureInstance
        
        # Create NexusLang struct instance
        nxl_struct = StructureInstance(struct_def)
        
        # Copy fields from C struct
        for field_name in struct_def.fields:
            c_value = getattr(c_struct, field_name)
            
            # Handle nested structs
            if hasattr(c_value, '_fields_'):  # It's a ctypes.Structure
                # Find the nested struct definition
                nested_struct_def = None
                for fname, ftype in getattr(struct_def, '_original_fields', []):
                    if fname == field_name and ftype in self.classes:
                        nested_struct_def = self.classes[ftype]
                        break
                
                if nested_struct_def:
                    nested_nxl = self._cstruct_to_structure_instance(c_value, nested_struct_def)
                    nxl_struct.set_field(field_name, nested_nxl)
                else:
                    # Can't convert, store as-is
                    nxl_struct.set_field(field_name, c_value)
            else:
                # Regular field
                nxl_struct.set_field(field_name, c_value)
        
        return nxl_struct
    
    def _python_to_ctype_value(self, value, ctype):
        """Convert Python value to ctypes-compatible value."""
        import ctypes
        from nexuslang.runtime.structures import StructureInstance
        
        if ctype is None or ctype == "void":
            return None
        
        # Handle NexusLang struct instances
        if isinstance(value, StructureInstance):
            # Convert to ctypes.Structure
            cstruct_class = self._struct_to_ctype(value.definition)
            return self._structure_instance_to_cstruct(value, cstruct_class)
        
        if ctype == ctypes.c_char_p:
            if isinstance(value, str):
                return value.encode('utf-8')
            elif isinstance(value, bytes):
                return value
            else:
                return str(value).encode('utf-8')
        
        # For numeric types, convert to appropriate type
        if ctype in [ctypes.c_long, ctypes.c_int, ctypes.c_longlong]:
            return int(value) if value is not None else 0
        elif ctype in [ctypes.c_float, ctypes.c_double]:
            return float(value) if value is not None else 0.0
        elif ctype == ctypes.c_bool:
            return bool(value)
        elif ctype == ctypes.c_void_p:
            # Handle pointer values
            if isinstance(value, int):
                return value
            elif hasattr(value, '_address'):
                return value._address
            else:
                return 0
        
        return value
    
    def _ctype_value_to_python(self, value, ctype):
        """Convert ctypes return value to Python value."""
        import ctypes
        
        if ctype is None or ctype == "void":
            return None
        
        # Handle ctypes.Structure instances (struct return values)
        if hasattr(value, '_fields_'):
            # This is a ctypes.Structure - need to find the corresponding NexusLang struct
            # Try to match by structure name
            struct_name = type(value).__name__
            if struct_name in self.classes:
                struct_def = self.classes[struct_name]
                return self._cstruct_to_structure_instance(value, struct_def)
            # Can't convert, return as-is
            return value
        
        if ctype == ctypes.c_char_p:
            if isinstance(value, bytes):
                return value.decode('utf-8', errors='replace')
            return value
        
        # Numeric types
        return value
    
    def execute_extern_function_declaration(self, node):
        """Execute an extern function declaration.
        
        Registers a C function binding so it can be called from NexusLang.
        
        Example:
            extern function printf with format as Pointer returns Integer from library "c"
        """
        import ctypes
        import platform
        
        func_name = node.name
        library_name = node.library
        calling_convention = getattr(node, 'calling_convention', 'cdecl')
        variadic = getattr(node, 'variadic', False)
        
        if not library_name:
            raise NxlRuntimeError(
                f"Extern function '{func_name}' must specify a library",
                line=getattr(node, 'line_number', None),
                error_type_key="invalid_operation",
                full_source=self.source,
            )
        
        try:
            system = platform.system()
            full_library_path = ctypes.util.find_library(library_name) or \
                self._resolve_library_path(library_name, system)
            library = self._load_ctypes_library(full_library_path, calling_convention, system)
            
            c_func = getattr(library, func_name)
            return_type = node.return_type if node.return_type else "Void"
            c_return_type = self._nxl_type_to_ctype(return_type)
            c_func.restype = c_return_type
            
            param_types = []
            param_names = []
            for param in node.parameters:
                param_names.append(param.name)
                param_types.append(self._nxl_type_to_ctype(param.type_annotation))
            
            if param_types:
                c_func.argtypes = param_types
            
            nxl_wrapper = self._build_extern_wrapper(
                func_name, c_func, param_types, variadic, c_return_type, node
            )
            
            self.runtime.register_function(func_name, nxl_wrapper)
            self.set_variable(func_name, nxl_wrapper)
            
            return func_name
            
        except OSError as e:
            raise NxlRuntimeError(
                f"Failed to load library '{library_name}' for extern function '{func_name}': {str(e)}",
                line=getattr(node, 'line_number', None),
                error_type_key="module_not_found",
                full_source=self.source,
            )
        except AttributeError as e:
            raise NxlRuntimeError(
                f"Function '{func_name}' not found in library '{library_name}': {str(e)}",
                line=getattr(node, 'line_number', None),
                error_type_key="undefined_function",
                full_source=self.source,
            )

    def _resolve_library_path(self, library_name: str, system: str) -> str:
        """Return a platform-appropriate shared-library file name for *library_name*."""
        if system == 'Windows':
            aliases = {'c': 'msvcrt.dll', 'm': 'msvcrt.dll'}
        elif system == 'Darwin':
            aliases = {'c': 'libc.dylib', 'm': 'libm.dylib'}
        else:  # Linux and other Unix-like
            aliases = {'c': 'libc.so.6', 'm': 'libm.so.6'}
        return aliases.get(library_name, library_name)

    def _load_ctypes_library(self, full_library_path: str, calling_convention: str, system: str):
        """Load and return a ctypes library object from *full_library_path*."""
        import ctypes
        if system == 'Windows' and calling_convention == 'stdcall':
            return ctypes.WinDLL(full_library_path)
        return ctypes.CDLL(full_library_path)

    def _build_extern_wrapper(self, func_name: str, c_func, param_types, variadic: bool, c_return_type, node):
        """Return a Python callable that converts NexusLang values to C values, calls *c_func*, and converts the result back."""
        import ctypes

        def nxl_wrapper(*args):
            c_args = []
            temp_refs = []  # keep alive to prevent GC during the call
            
            for i, arg in enumerate(args):
                if i < len(param_types):
                    ctype = param_types[i]
                else:
                    # Variadic: infer ctype from Python value
                    if isinstance(arg, bool):
                        ctype = ctypes.c_int
                    elif isinstance(arg, int):
                        ctype = ctypes.c_long
                    elif isinstance(arg, float):
                        ctype = ctypes.c_double
                    elif isinstance(arg, (str, bytes)):
                        ctype = ctypes.c_char_p
                    else:
                        ctype = ctypes.c_void_p
                
                if ctype in (ctypes.c_char_p, ctypes.c_void_p) and isinstance(arg, str):
                    encoded = arg.encode('utf-8')
                    char_ptr = ctypes.c_char_p(encoded)
                    temp_refs.append(char_ptr)
                    c_args.append(char_ptr)
                else:
                    converted = self._python_to_ctype_value(arg, ctype)
                    if i >= len(param_types) and variadic:
                        if ctype == ctypes.c_double and isinstance(converted, (int, float)):
                            converted = ctypes.c_double(float(converted))
                        elif ctype == ctypes.c_long and isinstance(converted, int):
                            converted = ctypes.c_long(converted)
                    c_args.append(converted)
            
            try:
                result = c_func(*c_args)
            except Exception as e:
                raise NxlRuntimeError(
                    f"Error calling C function '{func_name}': {str(e)}",
                    line=getattr(node, 'line_number', None),
                    error_type_key="function_call_error",
                    full_source=self.source,
                )
            
            return self._ctype_value_to_python(result, c_return_type)
        
        return nxl_wrapper

    def execute_unsafe_block(self, node):
        """Execute an unsafe FFI block, suppressing runtime safety checks.

        While _in_unsafe_context > 0, null-pointer guards, bounds checks,
        and borrow-checker enforcements are suppressed.  The depth counter
        supports nested 'unsafe' blocks correctly.
        """
        self._in_unsafe_context += 1
        try:
            result = None
            for stmt in node.body:
                result = self.execute(stmt)
            return result
        finally:
            self._in_unsafe_context -= 1

    # ---------------------------------------------------------------------------
    # Native test framework execution
    # ---------------------------------------------------------------------------

    def _run_test_body(self, name: str, body: list,
                       setup_stmts: list = None,
                       teardown_stmts: list = None) -> dict:
        """Execute one test case body and return a result dict.

        Parameters
        ----------
        name : str
            Human-readable test name for reporting.
        body : list
            Statement nodes forming the test body.
        setup_stmts : list, optional
            Statements to execute before the body (before-each).
        teardown_stmts : list, optional
            Statements to execute after the body regardless of outcome.

        Returns
        -------
        dict with keys: name, passed (bool), error (str or None), duration (float)
        """
        import time
        start = time.time()
        error_msg = None
        passed = False
        self.enter_scope()
        try:
            if setup_stmts:
                for stmt in setup_stmts:
                    self.execute(stmt)
            for stmt in body:
                self.execute(stmt)
            passed = True
        except AssertionError as exc:
            error_msg = str(exc)
        except Exception as exc:
            error_msg = f"{type(exc).__name__}: {exc}"
        finally:
            if teardown_stmts:
                try:
                    for stmt in teardown_stmts:
                        self.execute(stmt)
                except Exception as td_exc:
                    if error_msg is None:
                        error_msg = f"Teardown failed: {td_exc}"
            self.exit_scope()
        duration = time.time() - start
        return {"name": name, "passed": passed, "error": error_msg,
                "duration": duration}

    def _print_test_summary(self, results: list, suite_name: str = ""):
        """Print a test-run summary to stdout and accumulate into _collected_test_results."""
        total = len(results)
        passed = sum(1 for r in results if r["passed"])
        failed = total - passed
        marker = "=" * 60
        print(f"\n{marker}")
        if suite_name:
            print(f"Test suite: {suite_name}")
        for r in results:
            status = "PASS" if r["passed"] else "FAIL"
            print(f"  [{status}] {r['name']} ({r['duration']:.3f}s)")
            if r["error"]:
                print(f"         {r['error']}")
        print(f"{marker}")
        rate = (passed / total * 100) if total else 0.0
        print(f"Results: {passed}/{total} passed ({rate:.1f}%)")
        if failed:
            print(f"FAILURES: {failed} test(s) failed")
        print()

        # Accumulate into the interpreter's result list for the test runner
        for r in results:
            entry = dict(r)
            entry["suite"] = suite_name
            self._collected_test_results.append(entry)

        return {"total": total, "passed": passed, "failed": failed}

    def execute_test_block(self, node):
        """Execute a named test block.

        Syntax: test "name" do ... end
        """
        result = self._run_test_body(node.name, node.body)
        self._print_test_summary([result])
        return result

    def execute_describe_block(self, node):
        """Execute a describe (test suite) block.

        Runs 'before each' / 'after each' hooks around each nested test/it block.
        """
        from ..parser.ast import (
            BeforeEachBlock, AfterEachBlock, TestBlock, ItBlock,
            ParameterizedTestBlock,
        )

        # Collect lifecycle hooks and test nodes from the body
        setup_stmts: list = []
        teardown_stmts: list = []
        test_nodes: list = []

        for item in node.body:
            if isinstance(item, BeforeEachBlock):
                setup_stmts = item.body
            elif isinstance(item, AfterEachBlock):
                teardown_stmts = item.body
            elif isinstance(item, (TestBlock, ItBlock, ParameterizedTestBlock)):
                test_nodes.append(item)
            else:
                # Non-test statements (variable declarations etc.) are executed
                # as suite-level setup code.
                self.execute(item)

        results = []
        for test_node in test_nodes:
            if isinstance(test_node, ParameterizedTestBlock):
                # Delegate to parameterized handler for each case
                sub_results = self._run_parameterized_cases(
                    test_node, setup_stmts, teardown_stmts
                )
                results.extend(sub_results)
            else:
                r = self._run_test_body(
                    test_node.name, test_node.body, setup_stmts, teardown_stmts
                )
                results.append(r)

        self._print_test_summary(results, suite_name=node.name)
        return results

    def execute_it_block(self, node):
        """Execute a BDD-style 'it' specification block.

        Functionally identical to execute_test_block; the different keyword
        conveys intent only.
        """
        result = self._run_test_body(node.name, node.body)
        self._print_test_summary([result])
        return result

    def execute_before_each_block(self, node):
        """Execute a 'before each' block.

        When encountered at the top level (outside a describe block) it runs
        immediately as a plain statement sequence.  Inside a describe block it
        is collected and used as setup only.
        """
        for stmt in node.body:
            self.execute(stmt)
        return None

    def execute_after_each_block(self, node):
        """Execute an 'after each' block at the top level (immediate run)."""
        for stmt in node.body:
            self.execute(stmt)
        return None

    # ------------------------------------------------------------------
    # Assertion library (expect statements)
    # ------------------------------------------------------------------

    def execute_expect_statement(self, node):
        """Execute an expect assertion statement.

        Raises AssertionError on failure so that _run_test_body can catch it.
        When used outside a test block the AssertionError propagates to the
        caller (normal Python assertion semantics).
        """
        matcher = node.matcher
        negated = node.negated

        def _fail(msg: str):
            raise AssertionError(msg)

        # raise_error is special: must NOT pre-evaluate the expression so
        # that we can catch exceptions thrown by the expression itself.
        if matcher == "raise_error":
            return self._execute_expect_raise_error(node, negated, _fail)

        actual = self.execute(node.actual_expr)

        if matcher in ("be_true", "be_false", "be_null", "be_empty"):
            self._execute_expect_unary_matcher(matcher, negated, actual, _fail)
        elif matcher == "approximately_equal":
            self._execute_expect_approximate(negated, actual, node, _fail)
        elif matcher == "be_of_type":
            expected = self.execute(node.expected_expr)
            self._execute_expect_type_check(negated, actual, expected, _fail)
        elif matcher in (
            "equal", "greater_than", "less_than", "greater_than_or_equal_to",
            "less_than_or_equal_to", "contain", "have_length", "start_with", "end_with",
        ):
            expected = self.execute(node.expected_expr)
            self._execute_expect_comparison_matcher(matcher, negated, actual, expected, _fail)
        else:
            raise RuntimeError(f"Unknown expect matcher: {matcher!r}")

        return None

    def _execute_expect_raise_error(self, node, negated: bool, _fail) -> None:
        """Handle the 'raise_error' expect matcher."""
        raised = False
        raised_exc = None
        try:
            self.execute(node.actual_expr)
        except AssertionError:
            raised = True
            raised_exc = None  # assertion failures are not "raised errors"
        except Exception as exc:
            raised = True
            raised_exc = exc
        if negated:
            if raised:
                exc_info = f"{type(raised_exc).__name__}: {raised_exc}" if raised_exc else "AssertionError"
                _fail(f"Expected expression not to raise an error, but got: {exc_info}")
        else:
            if not raised:
                _fail("Expected expression to raise an error, but it did not raise")

    def _execute_expect_unary_matcher(self, matcher: str, negated: bool, actual, _fail) -> None:
        """Handle matchers that do not require an expected value."""
        if matcher == "be_true":
            passed = bool(actual)
        elif matcher == "be_false":
            passed = not bool(actual)
        elif matcher == "be_null":
            passed = actual is None
        elif matcher == "be_empty":
            try:
                passed = len(actual) == 0
            except TypeError:
                passed = False
        else:
            raise RuntimeError(f"Unknown unary matcher: {matcher!r}")

        if negated:
            passed = not passed
        if not passed:
            qual = "not " if negated else ""
            if matcher == "be_null":
                _fail(f"Expected value {qual}to be null, got {actual!r}")
            elif matcher == "be_empty":
                length = len(actual) if hasattr(actual, "__len__") else "?"
                _fail(
                    f"Expected value {qual}to be empty"
                    + (f", but it has {length} element(s)" if not negated else "")
                )
            else:
                _fail(f"Expected {actual!r} {qual}to be {matcher.replace('_', ' ')}")

    def _execute_expect_approximate(self, negated: bool, actual, node, _fail) -> None:
        """Handle the 'approximately_equal' expect matcher."""
        expected = self.execute(node.expected_expr)
        tolerance = self.execute(node.tolerance_expr) if node.tolerance_expr is not None else 1e-9
        try:
            passed = abs(actual - expected) <= tolerance
        except TypeError:
            passed = False
        if negated:
            passed = not passed
        if not passed:
            qual = "not " if negated else ""
            _fail(
                f"Expected {actual!r} {qual}to be approximately equal to "
                f"{expected!r} within {tolerance!r}"
            )

    def _execute_expect_type_check(self, negated: bool, actual, expected, _fail) -> None:
        """Handle the 'be_of_type' expect matcher."""
        type_name = str(expected).lower()
        _TYPE_MAP = {
            "integer": int, "int": int,
            "float": float, "number": (int, float),
            "string": str, "str": str,
            "boolean": bool, "bool": bool,
            "list": list, "array": list,
            "dict": dict, "dictionary": dict, "map": dict,
            "none": type(None), "null": type(None),
        }
        expected_type = _TYPE_MAP.get(type_name)
        if expected_type is not None:
            passed = isinstance(actual, expected_type)
        else:
            passed = type(actual).__name__.lower() == type_name
        if negated:
            passed = not passed
        if not passed:
            actual_type = type(actual).__name__
            qual = "not " if negated else ""
            _fail(
                f"Expected value of type {expected!r}, but got {actual_type!r}"
                if not negated
                else f"Expected value {qual}to be of type {expected!r}, but it is {actual_type!r}"
            )

    def _execute_expect_comparison_matcher(self, matcher: str, negated: bool, actual, expected, _fail) -> None:
        """Handle comparison expect matchers that require an expected value."""
        if matcher == "equal":
            passed = actual == expected
        elif matcher == "greater_than":
            passed = actual > expected
        elif matcher == "less_than":
            passed = actual < expected
        elif matcher == "greater_than_or_equal_to":
            passed = actual >= expected
        elif matcher == "less_than_or_equal_to":
            passed = actual <= expected
        elif matcher == "contain":
            try:
                passed = expected in actual
            except TypeError:
                passed = False
        elif matcher == "have_length":
            try:
                actual_len = len(actual)
                passed = actual_len == expected
            except TypeError:
                passed = False
                actual_len = None
        elif matcher == "start_with":
            try:
                if isinstance(actual, str):
                    passed = actual.startswith(str(expected))
                elif isinstance(actual, (list, tuple)):
                    passed = len(actual) > 0 and actual[0] == expected
                else:
                    passed = False
            except Exception:
                passed = False
        elif matcher == "end_with":
            try:
                if isinstance(actual, str):
                    passed = actual.endswith(str(expected))
                elif isinstance(actual, (list, tuple)):
                    passed = len(actual) > 0 and actual[-1] == expected
                else:
                    passed = False
            except Exception:
                passed = False
        else:
            raise RuntimeError(f"Unknown comparison matcher: {matcher!r}")

        if negated:
            passed = not passed
        if not passed:
            qual = "not " if negated else ""
            if matcher == "have_length":
                actual_len = len(actual) if hasattr(actual, "__len__") else None
                _fail(
                    f"Expected value {qual}to have length {expected!r}"
                    + (f", but it has length {actual_len!r}" if actual_len is not None else "")
                )
            elif matcher in ("equal",):
                if negated:
                    _fail(f"Expected {actual!r} not to equal {expected!r}")
                else:
                    _fail(f"Expected {actual!r} to equal {expected!r}")
            else:
                verb = {
                    "greater_than": "greater than",
                    "less_than": "less than",
                    "greater_than_or_equal_to": ">=",
                    "less_than_or_equal_to": "<=",
                    "contain": "contain",
                    "start_with": "start with",
                    "end_with": "end with",
                }.get(matcher, matcher)
                _fail(f"Expected {actual!r} {qual}to be {verb} {expected!r}")

    # ------------------------------------------------------------------
    # Contract programming (require / ensure / guarantee)
    # ------------------------------------------------------------------

    def execute_require_statement(self, node):
        """Execute a 'require' contract precondition.

        Raises NLPLContractError when the condition is False.
        """
        from ..errors import NxlContractError
        cond = self.execute(node.condition)
        if not cond:
            if node.message_expr is not None:
                msg = str(self.execute(node.message_expr))
            else:
                msg = "Precondition failed (require)"
            raise NxlContractError(msg, contract_kind="require")
        return None

    def execute_ensure_statement(self, node):
        """Execute an 'ensure' contract postcondition.

        Raises NLPLContractError when the condition is False.
        """
        from ..errors import NxlContractError
        cond = self.execute(node.condition)
        if not cond:
            if node.message_expr is not None:
                msg = str(self.execute(node.message_expr))
            else:
                msg = "Postcondition failed (ensure)"
            raise NxlContractError(msg, contract_kind="ensure")
        return None

    def execute_guarantee_statement(self, node):
        """Execute a 'guarantee' contract invariant assertion.

        Raises NLPLContractError when the condition is False.
        """
        from ..errors import NxlContractError
        cond = self.execute(node.condition)
        if not cond:
            if node.message_expr is not None:
                msg = str(self.execute(node.message_expr))
            else:
                msg = "Invariant violated (guarantee)"
            raise NxlContractError(msg, contract_kind="guarantee")
        return None

    def execute_invariant_statement(self, node):
        """Execute an 'invariant' contract assertion.

        Semantically identical to ``guarantee`` at runtime.  The
        ``nlpl-verify`` static verification tool distinguishes invariants
        from one-shot guarantees when generating verification conditions.

        Raises NLPLContractError (contract_kind="invariant") on failure.
        """
        from ..errors import NxlContractError
        cond = self.execute(node.condition)
        if not cond:
            if node.message_expr is not None:
                msg = str(self.execute(node.message_expr))
            else:
                msg = "Invariant violated"
            raise NxlContractError(msg, contract_kind="invariant")
        return None

    def execute_old_expression(self, node):
        """Return the pre-call captured value of an old(expr) expression.

        The value is pre-populated by the function-call executor before the
        function body runs.  Returns None gracefully when called outside a
        function context (e.g., in interactive mode or top-level ensures).
        """
        return self._old_values.get(id(node))

    def execute_spec_block(self, node):
        """Spec blocks are no-ops at runtime.

        They carry formal specification annotations consumed by the
        ``nlpl-verify`` static analysis tool.  Executing them during normal
        interpretation does nothing.
        """
        return None

    # ------------------------------------------------------------------
    # Formal-verification helpers
    # ------------------------------------------------------------------

    def _collect_old_refs(self, node):
        """Recursively collect all OldExpression nodes in an AST subtree."""
        from ..parser.ast import OldExpression
        results = []
        if isinstance(node, OldExpression):
            results.append(node)
        for attr in vars(node).values():
            if attr is node:
                continue
            if hasattr(attr, 'node_type'):
                results.extend(self._collect_old_refs(attr))
            elif isinstance(attr, list):
                for item in attr:
                    if hasattr(item, 'node_type'):
                        results.extend(self._collect_old_refs(item))
        return results

    def _run_parameterized_cases(self, node, setup_stmts: list,
                                  teardown_stmts: list) -> list:
        """Run each case of a parameterized test block and return result list."""
        results = []
        for idx, case_args in enumerate(node.cases):
            case_label = f"{node.name} (case {idx + 1})"
            evaluated_args = [self.execute(arg) for arg in case_args]

            def _body_with_bindings(args=evaluated_args, params=node.params,
                                    body=node.body):
                if params:
                    for name, value in zip(params, args):
                        self.current_scope[-1][name] = value
                for stmt in body:
                    self.execute(stmt)

            # Use _run_test_body mechanics manually so we keep scope handling
            import time
            start = time.time()
            error_msg = None
            passed = False
            self.enter_scope()
            try:
                if setup_stmts:
                    for stmt in setup_stmts:
                        self.execute(stmt)
                _body_with_bindings()
                passed = True
            except AssertionError as exc:
                error_msg = str(exc)
            except Exception as exc:
                error_msg = f"{type(exc).__name__}: {exc}"
            finally:
                if teardown_stmts:
                    try:
                        for stmt in teardown_stmts:
                            self.execute(stmt)
                    except Exception as td_exc:
                        if error_msg is None:
                            error_msg = f"Teardown failed: {td_exc}"
                self.exit_scope()
            results.append({
                "name": case_label,
                "passed": passed,
                "error": error_msg,
                "duration": time.time() - start,
            })
        return results

    def execute_parameterized_test_block(self, node):
        """Execute a parameterized test block (standalone, outside describe)."""
        results = self._run_parameterized_cases(node, [], [])
        self._print_test_summary(results, suite_name=node.name)
        return results


    def execute_extern_variable_declaration(self, node):
        """Execute an extern variable declaration.
        
        Accesses a global variable from a C library.
        
        Example:
            extern variable errno as Integer from library "c"
        """
        import ctypes
        import platform
        
        var_name = node.name
        library_name = node.library
        var_type = node.type_annotation
        
        if not library_name:
            raise NxlRuntimeError(
                f"Extern variable '{var_name}' must specify a library",
                line=getattr(node, 'line_number', None),
                error_type_key="invalid_operation",
                full_source=self.source,
            )
        
        try:
            # Load the library
            system = platform.system()
            if system == 'Windows':
                library = ctypes.CDLL(library_name)
            else:
                # Unix-like systems
                if library_name == 'c':
                    if system == 'Darwin':  # macOS
                        library_name = 'libc.dylib'
                    else:  # Linux
                        library_name = 'libc.so.6'
                
                library = ctypes.CDLL(library_name)
            
            # Get the variable address
            var_address = ctypes.addressof(ctypes.py_object(library))
            
            # Create a ctypes object to hold the variable
            ctype = self._nxl_type_to_ctype(var_type)
            if ctype is None:
                ctype = ctypes.c_void_p
            
            # Store the variable reference in the current scope
            # For now, we store the address and type info
            self.set_variable(var_name, {
                'address': var_address,
                'type': var_type,
                'library': library,
                'ctype': ctype,
                'c_var_name': var_name
            })
            
            return var_name
            
        except OSError as e:
            raise NxlRuntimeError(
                f"Failed to load library '{library_name}' for extern variable '{var_name}': {str(e)}",
                line=getattr(node, 'line_number', None),
                error_type_key="module_not_found",
                full_source=self.source,
            )
    
    def execute_inline_assembly(self, node):
        """Execute inline assembly block.

        Inline assembly is primarily supported in compiled mode. In the
        interpreter, behavior is controlled by `inline_asm_policy`:

        - "error": raise runtime error (strict mode)
        - "warn": print warning once, then skip block
        - "skip": silently skip block

        For limited interpreter support of hardcoded x86 instructions, use the
        execute_asm() stdlib function from nexuslang.stdlib.asm.
        """
        policy = getattr(self, "inline_asm_policy", "error")

        if policy not in {"error", "warn", "skip"}:
            raise NxlRuntimeError(
                f"Invalid inline_asm_policy '{policy}'. Expected one of: error, warn, skip",
                line=getattr(node, 'line_number', None),
                error_type_key="runtime_error",
                full_source=self.source,
            )

        if policy == "error":
            raise NxlRuntimeError(
                "Inline assembly is not executed in interpreter mode. "
                "Compile with 'nlplc' or set runtime.inline_asm_policy to 'warn' or 'skip'.",
                line=getattr(node, 'line_number', None),
                error_type_key="runtime_error",
                full_source=self.source,
            )

        if policy == "warn":
            if not hasattr(self, '_inline_asm_warned'):
                print("Warning: Inline assembly is only fully supported in compiled mode.")
                print("         Assembly blocks are skipped in interpreter mode.")
                print("         Compile with 'nlplc' to generate actual inline assembly.")
                self._inline_asm_warned = True

        # Skip execution - inline assembly requires compiled native code.
        return None
            
    def execute_print_statement(self, node):
        """Execute a print statement with optional type conversion."""
        value = self.execute(node.expression)
        
        # Apply type conversion if hint is provided
        if node.print_type == "text":
            value = str(value)
        elif node.print_type == "number":
            try:
                if isinstance(value, str):
                    if "." in value:
                        value = float(value)
                    else:
                        value = int(value)
                else:
                    value = float(value)
            except (ValueError, TypeError):
                # Fallback or error if conversion fails
                pass
                
        self.runtime.print(value)
        return value

    # Expression execution methods
    
    def execute_type_cast_expression(self, node):
        """Execute a type cast expression."""
        value = self.execute(node.expression)
        target = node.target_type.lower()
        
        if target == "string":
            return str(value)
        elif target == "integer":
            if isinstance(value, str):
                if "." in value:
                    return int(float(value))
                return int(value)
            return int(value)
        elif target == "float":
            return float(value)
        elif target == "uppercase":
            if isinstance(value, str):
                return value.upper()
            return str(value).upper()
        elif target == "lowercase":
            if isinstance(value, str):
                return value.lower()
            return str(value).lower()
        
        return value

    def execute_binary_operation(self, node):
        """Execute a binary operation."""
        left = self.execute(node.left)
        right = self.execute(node.right)
        
        # Perform the operation based on the operator
        from ..parser.lexer import TokenType
        
        # Get the operator type from the token
        op_type = node.operator.type if hasattr(node.operator, 'type') else node.operator

        # Operator overloading on NexusLang objects (runtime.Object and legacy dict-backed objects).
        _OP_METHOD_MAP = {
            TokenType.PLUS: '__op_add__',
            TokenType.MINUS: '__op_sub__',
            TokenType.TIMES: '__op_mul__',
            TokenType.DIVIDED_BY: '__op_div__',
            TokenType.MODULO: '__op_mod__',
            TokenType.POWER: '__op_pow__',
            TokenType.EQUAL_TO: '__op_eq__',
            TokenType.NOT_EQUAL_TO: '__op_ne__',
            TokenType.LESS_THAN: '__op_lt__',
            TokenType.GREATER_THAN: '__op_gt__',
            TokenType.LESS_THAN_OR_EQUAL_TO: '__op_le__',
            TokenType.GREATER_THAN_OR_EQUAL_TO: '__op_ge__',
        }
        overload_name = _OP_METHOD_MAP.get(op_type)
        if overload_name:
            from ..runtime.runtime import Object as RuntimeObject

            if isinstance(left, RuntimeObject) and overload_name in left.methods:
                return self._call_method_on_object(left, overload_name, [right])

            if isinstance(left, dict) and f"__method_{overload_name}__" in left:
                return self._call_method_on_object(left, overload_name, [right])
        
        if op_type == TokenType.PLUS:
            # Auto-coerce: if either operand is a string, convert the other to string.
            # This makes "value: " plus 42 work naturally without explicit to_string().
            if isinstance(left, str) or isinstance(right, str):
                return str(left) + str(right)
            return left + right
        elif op_type == TokenType.MINUS:
            return left - right
        elif op_type == TokenType.TIMES:
            return left * right
        elif op_type == TokenType.DIVIDED_BY:
            return left / right
        elif op_type == TokenType.MODULO:
            return left % right
        elif op_type == TokenType.POWER:
            return left ** right
        elif op_type == TokenType.FLOOR_DIVIDE:
            return left // right
        elif op_type == TokenType.CONCATENATE:
            # String concatenation - convert both operands to strings
            return str(left) + str(right)
        elif op_type == TokenType.EQUAL_TO:
            return left == right
        elif op_type == TokenType.NOT_EQUAL_TO:
            return left != right
        elif op_type == TokenType.LESS_THAN:
            return left < right
        elif op_type == TokenType.GREATER_THAN:
            return left > right
        elif op_type == TokenType.LESS_THAN_OR_EQUAL_TO:
            return left <= right
        elif op_type == TokenType.GREATER_THAN_OR_EQUAL_TO:
            return left >= right
        elif op_type == TokenType.AND:
            return left and right
        elif op_type == TokenType.OR:
            return left or right
        elif op_type == TokenType.BITWISE_AND:
            return left & right
        elif op_type == TokenType.BITWISE_OR:
            return left | right
        elif op_type == TokenType.BITWISE_XOR:
            return left ^ right
        elif op_type == TokenType.LEFT_SHIFT:
            return left << right
        elif op_type == TokenType.RIGHT_SHIFT:
            return left >> right
        elif op_type == TokenType.IN:
            return left in right
        else:
            supported = ['+', '-', '*', '/', '//', '%', '**', '==', '!=', '<', '>', '<=', '>=', 
                       'and', 'or', 'in', 'not in', 'is', 'is not', '&', '|', '^', '<<', '>>']
            raise NxlTypeError(
                f"Unsupported binary operator: '{node.operator}'.\n"
                f"  Supported operators: {', '.join(supported)}\n"
                f"  This is a language limitation - operator '{node.operator}' is not available.",
                error_type_key="invalid_operation",
                full_source=self.source,
            )
    
    def execute_unary_operation(self, node):
        """Execute a unary operation."""
        operand = self.execute(node.operand)
        
        # Perform the operation based on the operator
        from ..parser.lexer import TokenType
        
        op_type = node.operator.type if hasattr(node.operator, 'type') else node.operator

        if op_type == TokenType.PLUS:
            return +operand
        elif op_type == TokenType.MINUS:
            return -operand
        elif op_type == TokenType.NOT:
            return not operand
        elif op_type == TokenType.BITWISE_NOT:
            return ~operand
        else:
            supported = ['not', '-', '+', '~', 'address of', 'dereference']
            raise NxlTypeError(
                f"Unsupported unary operator: '{node.operator}'.\n"
                f"  Supported operators: {', '.join(supported)}\n"
                f"  This is a language limitation - operator '{node.operator}' is not available.",
                error_type_key="invalid_operation",
                full_source=self.source,
            )
            
    def execute_literal(self, node):
        """Execute a literal."""
        return node.value
    
    def execute_fstring_expression(self, node):
        """Execute f-string with interpolation and format specifiers."""
        result_parts = []
        for part_tuple in node.parts:
            # Handle both old (2-tuple) and new (3-tuple) formats
            if len(part_tuple) == 2:
                is_literal, content = part_tuple
                format_spec = None
            elif len(part_tuple) == 3:
                is_literal, content, format_spec = part_tuple
            else:
                continue
            
            if is_literal:
                # Literal string part
                result_parts.append(str(content))
            else:
                # Expression - evaluate it
                value = self.execute(content)
                
                # Apply format specifier if present
                if format_spec:
                    formatted = self._apply_format_spec(value, format_spec)
                    result_parts.append(formatted)
                else:
                    # Convert to string
                    result_parts.append(str(value))
        return ''.join(result_parts)
    
    def _apply_format_spec(self, value, format_spec):
        """Apply Python-style format specifier to a value.
        
        Supports:
        - .Nf for floats (precision)
        - 0Nd for integers (zero-padding)
        - >N, <N, ^N for alignment
        """
        try:
            # Use Python's format specification mini-language
            return format(value, format_spec)
        except (ValueError, TypeError) as e:
            # Fallback to string conversion if format fails
            return str(value)
    
    def execute_lambda_expression(self, node):
        """Execute a lambda expression, creating a closure."""
        # Create a closure that captures the current scope
        captured_scope = dict(self.current_scope[-1])  # Copy current scope
        
        # Store lambda metadata
        lambda_func = {
            'type': 'lambda',
            'parameters': node.parameters,
            'body': node.body,
            'return_type': node.return_type,
            'captured_scope': captured_scope,
            'interpreter': self  # Reference to interpreter for execution
        }
        
        return lambda_func
    
    def _create_closure(self, lambda_node):
        """Create a Python callable closure from a lambda/block AST node.
        
        This wraps the lambda/block execution in a proper Python callable
        that can be passed as an argument and called later.
        
        Args:
            lambda_node: LambdaExpression AST node representing the block
            
        Returns:
            Python callable that executes the block when called
        """
        # Capture current scope
        captured_scope = dict(self.current_scope[-1]) if self.current_scope else {}
        
        def closure(*args):
            """Closure that executes the block with the given arguments."""
            # Enter new scope with captured variables
            self.enter_scope()
            
            # Restore captured scope
            for var, val in captured_scope.items():
                self.set_variable(var, val)
            
            # Bind block parameters to arguments
            for i, param in enumerate(lambda_node.parameters):
                if i < len(args):
                    self.set_variable(param.name, args[i])
            
            # Execute block body
            result = None
            try:
                for stmt in lambda_node.body:
                    result = self.execute(stmt)
            except ReturnException as ret:
                # Block used explicit return
                result = ret.value
            finally:
                self.exit_scope()
            
            return result
        
        return closure
    
    def execute_list_expression(self, node):
        """Execute a list expression (array literal)."""
        # Evaluate all elements in the list
        return [self.execute(element) for element in node.elements]
    
    def execute_dict_expression(self, node):
        """Execute a dictionary expression (dict literal)."""
        # Evaluate all key-value pairs
        result = {}
        for key_expr, value_expr in node.entries:
            key = self.execute(key_expr)
            value = self.execute(value_expr)
            result[key] = value
        return result
    
    def execute_list_comprehension(self, node):
        """Execute a list comprehension."""
        # Syntax: [expr for var in iterable if condition]
        result = []
        iterable = self.execute(node.iterable)
        
        # Save current scope
        old_scope = self.current_scope.copy()
        
        for item in iterable:
            # Set loop variable
            self.set_variable(node.target.name, item)
            
            # Check condition if present
            if node.condition is None or self.execute(node.condition):
                # Evaluate expression and add to result
                value = self.execute(node.expr)
                result.append(value)
        
        # Restore scope (remove loop variable)
        self.current_scope = old_scope
        
        return result
    
    def execute_dict_comprehension(self, node):
        """Execute a dictionary comprehension."""
        # Syntax: {key: value for var in iterable if condition}
        result = {}
        iterable = self.execute(node.iterable)
        
        # Save current scope
        old_scope = self.current_scope.copy()
        
        for item in iterable:
            # Set loop variable
            self.set_variable(node.target.name, item)
            
            # Check condition if present
            if node.condition is None or self.execute(node.condition):
                # Evaluate key and value expressions
                key = self.execute(node.key)
                value = self.execute(node.value)
                result[key] = value
        
        # Restore scope (remove loop variable)
        self.current_scope = old_scope
        
        return result
    
    def execute_index_expression(self, node):
        """Execute an index expression (array access)."""
        # Evaluate the array and index
        array = self.execute(node.array_expr)
        index = self.execute(node.index_expr)
        
        # Return the element at the index
        try:
            return array[index]
        except IndexError as e:
            raise NxlRuntimeError(
                message=f"Index {index} is out of range for array of length {len(array)}",
                line=getattr(node, 'line', None),
                column=getattr(node, 'column', None),
                nxl_type="IndexError",
                error_type_key="index_out_of_range",
                full_source=self.source
            )
        except KeyError as e:
            # For dictionaries, suggest similar keys
            if isinstance(array, dict):
                available_keys = [str(k) for k in array.keys()]
                raise NxlNameError(
                    name=str(index),
                    available_names=available_keys,
                    line=getattr(node, 'line', None),
                    column=getattr(node, 'column', None),
                    source_line=getattr(node, 'source_line', None),
                    error_type_key="undefined_variable",
                    full_source=self.source
                )
            raise NxlRuntimeError(
                message=f"Key '{index}' not found",
                line=getattr(node, 'line', None),
                column=getattr(node, 'column', None),
                nxl_type="IndexError",
                error_type_key="key_not_found",
                full_source=self.source
            )
        except TypeError as e:
            raise NxlTypeError(
                message=f"Cannot use index on type {type(array).__name__}",
                expected_type="list, dict, or string",
                got_type=type(array).__name__,
                line=getattr(node, 'line', None),
                column=getattr(node, 'column', None),
                error_type_key="invalid_operation",
                full_source=self.source,
            )

    def execute_slice_expression(self, node):
        """Execute a slice expression (e.g., s[0:5] or s[1:10:2])."""
        seq = self.execute(node.expr)
        start = self.execute(node.start) if node.start is not None else None
        end = self.execute(node.end) if node.end is not None else None
        step = self.execute(node.step) if node.step is not None else None
        try:
            return seq[slice(start, end, step)]
        except (TypeError, AttributeError) as e:
            raise NxlRuntimeError(
                message=f"Cannot slice value of type {type(seq).__name__}",
                line=getattr(node, 'line', None),
                column=getattr(node, 'column', None),
                nxl_type="TypeError",
                error_type_key="invalid_operation",
                full_source=self.source,
            )

    def execute_identifier(self, node):
        """Execute an identifier (variable reference)."""
        try:
            return self.get_variable(node.name)
        except NameError:
            # Check if it's a function (first-class support)
            if node.name in self.functions:
                return self.create_function_wrapper(self.functions[node.name])
            
                
            # Check if it's a built-in function
            if node.name in self.runtime.functions:
                return self.runtime.functions[node.name]
                
            raise NameError(f"Variable or function '{node.name}' is not defined")
        
    def execute_function_call(self, node):
        """Execute a function call with positional and/or named arguments.
        
        Supports both:
        - Direct function calls: add with 5 and 10
        - Named arguments: add with x: 5 and y: 10
        - Mixed arguments: add with 5 and y: 10
        - Module function calls: module.function with args
        - Callable values: (function stored in variable or passed as argument)
        - Trailing blocks: func do ... end
        """
        function_name = node.name
        
        # Evaluate positional arguments
        positional_args = [self.execute(arg) for arg in node.arguments]
        
        # Evaluate named arguments
        named_args = {}
        if hasattr(node, 'named_arguments') and node.named_arguments:
            if isinstance(node.named_arguments, dict):
                for param_name, arg_expr in node.named_arguments.items():
                    named_args[param_name] = self.execute(arg_expr)
        
        # Handle trailing block - create closure and add as last positional argument
        if hasattr(node, 'trailing_block') and node.trailing_block:
            block_closure = self._create_closure(node.trailing_block)
            positional_args.append(block_closure)
        
        # If function_name is already a callable (function value), call it directly
        if callable(function_name):
            return function_name(*positional_args, **named_args)
        
        # Handle expressions that evaluate to callables (e.g., function pointers)
        if not isinstance(function_name, str):
            func_value = self.execute(function_name)
            if callable(func_value):
                return func_value(*positional_args, **named_args)
            else:
                raise NxlTypeError(
                    f"Cannot call non-function value: {type(func_value).__name__}",
                    error_type_key="function_call_error",
                    full_source=self.source,
                )
        
        # Check if function_name is a variable holding a callable
        _var_value = self.get_variable_or_none(function_name)
        if _var_value is not None and callable(_var_value):
            return _var_value(*positional_args, **named_args)
        
        # Handle module.function calls (function_name contains a dot)
        if '.' in function_name:
            result = self._execute_module_function_call(function_name, positional_args, named_args)
            if result is not None or '.' in function_name:
                return result
        
        # Check for built-in functions in the runtime
        if function_name in self.runtime.functions:
            return self._execute_builtin_function_call(function_name, positional_args, named_args)
        
        # Check for user-defined functions
        if function_name in self.functions:
            return self._execute_user_defined_function_call(
                function_name, self.functions[function_name], positional_args, named_args
            )
        
        # Try to get it as a variable (might be a function value stored after definition)
        _func_value = self.get_variable_or_none(function_name)
        if _func_value is not None and callable(_func_value):
            return _func_value(*positional_args, **named_args)
        
        raise NxlNameError(
            name=function_name,
            available_names=list(self.functions.keys()) + list(self.runtime.functions.keys()),
            error_type_key="undefined_function",
            full_source=self.source,
        )

    def _execute_module_function_call(self, function_name: str, positional_args, named_args):
        """Handle ``module.function`` style calls. Returns the call result."""
        parts = function_name.split('.')
        if len(parts) == 2:
            module_name, member_name = parts
            module = self.get_variable_or_none(module_name)
            if module is not None:
                if hasattr(module, member_name):
                    func = getattr(module, member_name)
                    if callable(func):
                        return func(*positional_args, **named_args)
                    else:
                        raise NxlTypeError(
                            f"{module_name}.{member_name} is not callable",
                            error_type_key="function_call_error",
                            full_source=self.source,
                        )
                else:
                    raise AttributeError(f"Module '{module_name}' has no attribute '{member_name}'")
        return None

    def _execute_builtin_function_call(self, function_name: str, positional_args, named_args):
        """Call a function registered in the runtime's built-in function table."""
        import inspect
        func = self.runtime.functions[function_name]
        sig = inspect.signature(func)
        params = list(sig.parameters.keys())
        
        if params and params[0] == 'runtime':
            positional_args = [self.runtime] + list(positional_args)
        
        if named_args:
            try:
                return func(*positional_args, **named_args)
            except TypeError:
                all_args = list(positional_args) + list(named_args.values())
                return func(*all_args)
        else:
            return func(*positional_args)

    def _execute_user_defined_function_call(self, function_name: str, function_def, positional_args, named_args):
        """Execute a user-defined NexusLang function including contract postconditions."""
        args = self._resolve_function_arguments(
            function_def,
            positional_args,
            named_args,
            function_name,
        )
        
        if self.debugger:
            local_vars = {param.name: args[i] if i < len(args) else None
                         for i, param in enumerate(function_def.parameters)}
            self.debugger.trace_call(
                function_name,
                getattr(function_def, 'file', self.current_file or '<unknown>'),
                getattr(function_def, 'line', self.current_line or 0),
                local_vars,
            )
        
        self.enter_scope()
        
        for i, param in enumerate(function_def.parameters):
            if i < len(args):
                self.set_variable(param.name, args[i])
            else:
                default_value = None
                if hasattr(param, 'default_value') and param.default_value:
                    default_value = self.execute(param.default_value)
                self.set_variable(param.name, default_value)
        
        from ..parser.ast import EnsureStatement as _EnsureStmt
        main_body = [s for s in function_def.body if not isinstance(s, _EnsureStmt)]
        ensure_stmts = [s for s in function_def.body if isinstance(s, _EnsureStmt)]
        
        saved_old_values = self._old_values
        self._old_values = {}
        for _es in ensure_stmts:
            for _old_node in self._collect_old_refs(_es.condition):
                self._old_values[id(_old_node)] = self.execute(_old_node.expr)
            if _es.message_expr is not None:
                for _old_node in self._collect_old_refs(_es.message_expr):
                    self._old_values[id(_old_node)] = self.execute(_old_node.expr)
        
        result = None
        try:
            try:
                for statement in main_body:
                    result = self.execute(statement)
            except ReturnException as ret:
                result = ret.value
            
            self.set_variable("result", result)
            for _es in ensure_stmts:
                self.execute(_es)
        finally:
            if self.debugger:
                self.debugger.trace_return(function_name, result)
            self._old_values = saved_old_values
            self.exit_scope()
        
        return result
    
    def _resolve_function_arguments(self, function_def, positional_args, named_args, function_name):
        """Resolve positional and named arguments into a single argument list.
        
        Handles:
        - Positional arguments
        - Named arguments (param: value)
        - Default parameter values
        - Variadic parameters (*args)
        - Keyword-only parameters (after * separator)
        
        Args:
            function_def: FunctionDefinition AST node
            positional_args: List of positional argument values
            named_args: Dict of parameter_name -> value
            function_name: Name of function (for error messages)
            
        Returns:
            List of argument values in parameter order
        """
        params = function_def.parameters
        
        # Find variadic parameter and keyword-only parameters
        variadic_param_index = None
        first_keyword_only_index = None
        for i, param in enumerate(params):
            if hasattr(param, 'is_variadic') and param.is_variadic:
                variadic_param_index = i
            if hasattr(param, 'keyword_only') and param.keyword_only and first_keyword_only_index is None:
                first_keyword_only_index = i
        
        # Validate positional args don't fill keyword-only parameters
        if first_keyword_only_index is not None and len(positional_args) > first_keyword_only_index:
            # Count how many non-variadic positional params before keyword-only
            non_kw_count = first_keyword_only_index
            raise NxlTypeError(
                f"Function '{function_name}' takes at most {non_kw_count} positional arguments "
                f"but {len(positional_args)} were given. Parameters after '*' must be passed by name.",
                error_type_key="wrong_argument_count",
                full_source=self.source,
            )
        
        resolved_args = [None] * len(params)
        
        # Fill in positional arguments first
        if variadic_param_index is not None:
            # If there's a variadic parameter, collect excess args into it
            for i, arg in enumerate(positional_args):
                if i < variadic_param_index:
                    # Regular parameter
                    resolved_args[i] = arg
                elif i == variadic_param_index:
                    # Start of variadic args - collect all remaining into a list
                    resolved_args[i] = positional_args[i:]
                    break
        else:
            # No variadic parameter - strict argument count
            for i, arg in enumerate(positional_args):
                if i >= len(params):
                    raise NxlTypeError(
                        f"Function '{function_name}' takes {len(params)} parameters but {len(positional_args)} positional arguments were given",
                        error_type_key="wrong_argument_count",
                        full_source=self.source,
                    )
                # Check if this parameter is keyword-only
                if hasattr(params[i], 'keyword_only') and params[i].keyword_only:
                    raise NxlTypeError(
                        f"Parameter '{params[i].name}' is keyword-only and cannot be passed positionally in '{function_name}'",
                        error_type_key="wrong_argument_count",
                        full_source=self.source,
                    )
                resolved_args[i] = arg
        
        # Fill in named arguments
        for param_name, value in named_args.items():
            # Find parameter index by name
            param_index = None
            for i, param in enumerate(params):
                if param.name == param_name:
                    param_index = i
                    break
            
            if param_index is None:
                raise NxlTypeError(
                    f"Function '{function_name}' has no parameter named '{param_name}'",
                    error_type_key="wrong_argument_count",
                    full_source=self.source,
                )
            
            # Check if this parameter was already filled by positional arg
            if resolved_args[param_index] is not None:
                raise NxlTypeError(
                    f"Function '{function_name}' got multiple values for parameter '{param_name}'",
                    error_type_key="wrong_argument_count",
                    full_source=self.source,
                )
            
            resolved_args[param_index] = value
        
        # Check that all keyword-only parameters without defaults were provided
        for i, param in enumerate(params):
            if hasattr(param, 'keyword_only') and param.keyword_only:
                if resolved_args[i] is None and not (hasattr(param, 'default_value') and param.default_value is not None):
                    raise NxlTypeError(
                        f"Missing required keyword-only parameter '{param.name}' in call to '{function_name}'",
                        error_type_key="wrong_argument_count",
                        full_source=self.source,
                    )
        
        # Fill in default values for missing parameters
        for i, (arg, param) in enumerate(zip(resolved_args, params)):
            if arg is None:
                # Check if this is a variadic parameter
                if hasattr(param, 'is_variadic') and param.is_variadic:
                    # Variadic parameter with no args gets empty list
                    resolved_args[i] = []
                # Check if parameter has a default value
                elif hasattr(param, 'default_value') and param.default_value is not None:
                    # Execute the default value expression to get the actual value
                    resolved_args[i] = self.execute(param.default_value)
                # If no default and no value, it remains None (which will cause an error later)
        
        return resolved_args
        
    def execute_return_statement(self, node):
        """Execute a return statement."""
        value = None
        if node.value:
            value = self.execute(node.value)
        raise ReturnException(value)
    
    def execute_break_statement(self, node):
        """Execute a break statement."""
        raise BreakException()
    
    def execute_continue_statement(self, node):
        """Execute a continue statement."""
        raise ContinueException()
        
    def execute_block(self, node):
        """Execute a block of statements."""
        result = None
        
        self.enter_scope()
        try:
            for statement in node.statements:
                result = self.execute(statement)
                if isinstance(statement, ReturnStatement):
                    break
        finally:
            self.exit_scope()
            
        return result
        
    def execute_concurrent_block(self, node):
        """Execute a block of statements concurrently."""
        futures = []
        
        for statement in node.statements:
            # Create a function that executes the statement in its own scope
            def execute_concurrent(stmt=statement):
                self.enter_scope()
                try:
                    return self.execute(stmt)
                finally:
                    self.exit_scope()
            
            # Submit the function to the runtime's executor
            future = self.runtime.run_concurrent(execute_concurrent)
            futures.append(future)
        
        # Wait for all futures to complete and return their results
        return self.runtime.wait_for_futures(futures)
        
    def execute_try_catch_block(self, node):
        """Execute a try-catch block."""
        try:
            return self.execute(node.try_block)
        except (ReturnException, BreakException, ContinueException, YieldException):
            # Control flow exceptions must propagate -- never catch these
            raise
        except Exception as e:
            # Enter the catch block with the exception bound to the variable
            self.enter_scope()
            try:
                if node.exception_var:
                    self.current_scope[-1][node.exception_var] = str(e)
                return self.execute(node.catch_block)
            finally:
                self.exit_scope()
    
    # Low-level pointer operation execution methods
    
    def execute_address_of_expression(self, node):
        """
        Execute address-of operation: get memory address of a variable.
        Returns a MemoryAddress object.
        """
        # Get the target (must be an identifier or indexable expression)
        if hasattr(node.target, 'name'):
            # Simple variable: address of x
            var_name = node.target.name
            value = self.get_variable(var_name)
            return self.runtime.memory_manager.get_address(var_name, value)
        else:
            # Expression - evaluate it first
            value = self.execute(node.target)
            # Generate a temporary name for the address
            temp_name = f"_temp_{id(value)}"
            return self.runtime.memory_manager.get_address(temp_name, value)
    
    def execute_dereference_expression(self, node):
        """
        Execute dereference operation: get value at pointer address.
        """
        # Evaluate the pointer expression
        pointer = self.execute(node.pointer)
        
        # Dereference the pointer
        from ..runtime.memory import MemoryAddress
        if not isinstance(pointer, MemoryAddress):
            raise NxlTypeError(
                message=f"Cannot dereference non-pointer type",
                expected_type="Pointer",
                got_type=type(pointer).__name__,
                line=getattr(node, 'line', None),
                column=getattr(node, 'column', None),
                error_type_key="type_mismatch",
                full_source=self.source,
            )
        
        return self.runtime.memory_manager.dereference(pointer)
    
    def execute_sizeof_expression(self, node):
        """
        Execute sizeof operation: get size of type or variable in bytes.
        """
        # Check if target is a type name (identifier) or a value
        if hasattr(node.target, 'name'):
            # Type name or variable name
            target_name = node.target.name
            # Check if it's a known type
            type_names = ["Integer", "Float", "String", "Boolean", "Pointer", "List", "Dictionary"]
            if target_name in type_names:
                return self.runtime.memory_manager.sizeof(target_name)
            # Check if it's a struct/union/class
            elif target_name in self.classes:
                # Calculate size based on fields (simplified: 8 bytes per field)
                struct_def = self.classes[target_name]
                if hasattr(struct_def, 'fields'):
                    return len(struct_def.fields) * 8
                else:
                    # Default size for classes without explicit fields
                    return 8
            else:
                # It's a variable - get its value and find size
                value = self.get_variable(target_name)
                return self.runtime.memory_manager.sizeof(value)
        else:
            # Expression - evaluate and get size
            value = self.execute(node.target)
            return self.runtime.memory_manager.sizeof(value)    
    def execute_offsetof_expression(self, node):
        """
        Execute offsetof operation: get byte offset of field in struct/union.
        
        Returns the memory offset of a field within a struct or union type.
        """
        struct_name = node.struct_type
        field_name = node.field_name
        
        # Get struct definition
        if struct_name not in self.classes:
            raise NxlRuntimeError(
                f"Unknown struct/union type: {struct_name}",
                line=getattr(node, 'line', None),
                error_type_key="undefined_class",
                full_source=self.source
            )
        
        struct_def = self.classes[struct_name]
        
        if field_name not in struct_def.fields:
            raise NxlRuntimeError(
                f"Field '{field_name}' not found in {struct_name}",
                line=getattr(node, 'line', None),
                error_type_key="undefined_attribute",
                full_source=self.source
            )
            
        return struct_def.fields[field_name].offset
    
    def execute_type_cast(self, node):
        """
        Execute type cast: (expression as TargetType).
        
        Converts an expression to the target type.
        """
        # Evaluate the expression
        value = self.execute(node.expression)
        target_type = node.target_type
        
        # Handle pointer casts specially
        if "Pointer" in str(target_type):
            # For pointer casts, create a MemoryAddress if value is an integer
            from ..runtime.memory import MemoryAddress
            if isinstance(value, int):
                # Cast integer to pointer (memory-mapped I/O, hardware registers)
                return MemoryAddress(value)
            elif isinstance(value, MemoryAddress):
                # Already a pointer, return as-is
                return value
            else:
                # For other types, treat as reinterpret_cast
                return value
        
        # Type conversions
        if "Integer" in str(target_type):
            return int(value)
        elif "Float" in str(target_type):
            return float(value)
        elif "String" in str(target_type):
            return str(value)
        elif "Boolean" in str(target_type):
            return bool(value)
        else:
            # For struct/class types, return as-is (structural typing)
            return value
    
    def execute_object_instantiation(self, node):
        """
        Execute object instantiation: new ClassName or new StructName.
        
        Creates a new instance of a class or struct with initialized fields.
        """
        class_name = node.class_name
        
        builtin = self._instantiate_builtin_type(class_name)
        if builtin is not None:
            return builtin
        
        class_def = self.classes.get(class_name)
        if not class_def:
            from ..errors import NxlNameError
            raise NxlNameError(
                name=class_name,
                line=node.line_number,
                available_names=list(self.classes.keys()),
                error_type_key="undefined_class",
                full_source=self.source,
            )
        
        type_args_map = self._build_generic_type_args_map(class_def, node, class_name)
        
        if isinstance(class_def, (RuntimeStructDefinition, RuntimeUnionDefinition)):
            return self._instantiate_struct_or_union(class_def)
        
        return self._instantiate_regular_class(class_name, class_def, type_args_map)

    def _instantiate_builtin_type(self, class_name: str):
        """Return a new instance of a built-in collection type, or None if not recognised."""
        if class_name == "List" or class_name.startswith("List ") or class_name.startswith("List<"):
            return []
        if class_name in ("Dictionary", "Map") or \
                class_name.startswith("Dictionary ") or class_name.startswith("Map ") or \
                class_name.startswith("Map<"):
            return {}
        if "Array of" in class_name:
            parts = class_name.split()
            if len(parts) >= 3:
                try:
                    size = int(parts[2])
                    if len(parts) >= 4:
                        element_type = parts[3]
                        if element_type in self.classes:
                            return [
                                self.execute(ObjectInstantiation(element_type, [], None))
                                for _ in range(size)
                            ]
                    return [None] * size
                except ValueError:
                    pass
            return []
        return None

    def _build_generic_type_args_map(self, class_def, node, class_name: str) -> dict:
        """Build a mapping from generic parameter names to concrete type names from *node*."""
        type_args_map = {}
        if not (hasattr(node, 'type_arguments') and node.type_arguments):
            return type_args_map
        
        from ..errors import NxlTypeError
        if not hasattr(class_def, 'generic_parameters') or not class_def.generic_parameters:
            raise NxlTypeError(
                f"Class '{class_name}' is not generic but was given type arguments",
                line=getattr(node, 'line', None),
                column=getattr(node, 'column', None),
                error_type_key="invalid_generic_args",
                full_source=self.source,
            )
        if len(node.type_arguments) != len(class_def.generic_parameters):
            raise NxlTypeError(
                f"Class '{class_name}' expects {len(class_def.generic_parameters)} type arguments, "
                f"got {len(node.type_arguments)}",
                line=getattr(node, 'line', None),
                column=getattr(node, 'column', None),
                error_type_key="invalid_generic_args",
                full_source=self.source,
            )
        
        for param, arg in zip(class_def.generic_parameters, node.type_arguments):
            param_name = param.name if hasattr(param, 'name') else str(param)
            arg_name = arg.name if hasattr(arg, 'name') else str(arg)
            type_args_map[param_name] = arg_name
        
        return type_args_map

    def _instantiate_struct_or_union(self, class_def):
        """Create and return a StructureInstance, wiring up the drop method if defined."""
        instance = StructureInstance(class_def)
        
        if isinstance(class_def, RuntimeStructDefinition) and hasattr(class_def, '_original_fields'):
            for field_name, type_name in class_def._original_fields:
                if type_name in self.classes and isinstance(self.classes[type_name], RuntimeStructDefinition):
                    nested_instance = StructureInstance(self.classes[type_name])
                    instance.set_field(field_name, nested_instance)
        
        if hasattr(class_def, 'methods'):
            for m in class_def.methods:
                if getattr(m, 'name', None) == "drop":
                    def _make_struct_drop(interp, drop_node):
                        def _drop_fn(obj):
                            interp.enter_scope()
                            try:
                                interp.set_variable("self", obj)
                                for stmt in (getattr(drop_node, 'body', None) or []):
                                    interp.execute(stmt)
                            finally:
                                interp.exit_scope()
                        return _drop_fn
                    instance._drop_method = _make_struct_drop(self, m)
                    break
        
        return instance

    def _instantiate_regular_class(self, class_name: str, class_def, type_args_map: dict):
        """Create and return a regular (non-struct) class instance."""
        instance = self.runtime.create_object(class_name, type_arguments=type_args_map)
        
        if hasattr(class_def, 'properties'):
            for prop in class_def.properties:
                instance.set_property(prop.name, None)
        
        if hasattr(class_def, 'methods'):
            for method in class_def.methods:
                instance.add_method(method.name, method)
        
        if hasattr(class_def, '_derived_methods'):
            for mname, mcallable in class_def._derived_methods.items():
                instance.add_method(mname, mcallable)
        
        if "drop" in instance.methods:
            drop_ast = instance.methods["drop"]
            def _make_drop(interp, drop_node):
                def _drop_fn(obj):
                    interp.enter_scope()
                    try:
                        interp.set_variable("self", obj)
                        for stmt in (getattr(drop_node, 'body', None) or []):
                            interp.execute(stmt)
                    finally:
                        interp.exit_scope()
                return _drop_fn
            instance._drop_method = _make_drop(self, drop_ast)
        
        if getattr(class_def, '_is_singleton', False):
            if class_def._singleton_instance is None:
                class_def._singleton_instance = instance
            return class_def._singleton_instance
        
        return instance
    
    def execute_member_access(self, node):
        """
        Execute member access: object.field or object.method().
        
        Accesses a field or calls a method on an object.
        """
        # Evaluate the object expression
        obj = self.execute(node.object_expr)
        member_name = node.member_name
        
        # Handle Struct/Union member access
        from ..runtime.structures import StructureInstance
        if isinstance(obj, StructureInstance):
            return obj.get_field(member_name)
            
        # Handle runtime.Object instances (official objects)
        from ..runtime.runtime import Object
        if isinstance(obj, Object):
            return self._execute_member_access_object(obj, node, member_name)
        if isinstance(obj, dict):
            return self._execute_member_access_dict(obj, node, member_name)
        elif hasattr(obj, member_name):
            attr = getattr(obj, member_name)
            if node.is_method_call:
                # Evaluate arguments
                args = [self.execute(arg) for arg in node.arguments]
                return attr(*args)
            else:
                return attr
        else:
            raise NxlRuntimeError(
                f"Object has no member: {member_name}",
                line=getattr(node, 'line', None),
                error_type_key="undefined_attribute",
                full_source=self.source
            )
    

    def _execute_member_access_object(self, obj, node, member_name):
        """Handle member access on runtime Object instances (classes/instances)."""
        if node.is_method_call:
            # Method call
            if member_name in obj.methods:
                method_def = obj.methods[member_name]

                # Handle native Python callables (e.g., methods generated by @derive)
                if callable(method_def) and not hasattr(method_def, 'node_type'):
                    call_args = []
                    if hasattr(node, 'arguments') and node.arguments:
                        call_args = [self.execute(arg) for arg in node.arguments]
                    return method_def(obj, *call_args)

                # Enter new scope for method execution
                self.enter_scope()
                    
                try:
                    # Bind instance to 'self'
                    self.set_variable("self", obj)

                    # Bind instance properties to scope (implicit self)
                    for key, value in obj.properties.items():
                        self.set_variable(key, value)
                        
                    # Bind method parameters
                    if hasattr(method_def, 'parameters') and method_def.parameters:
                        # Evaluate arguments
                        args = [self.execute(arg) for arg in node.arguments] if hasattr(node, 'arguments') and node.arguments else []
                            
                        for i, param in enumerate(method_def.parameters):
                            if i < len(args):
                                self.set_variable(param.name, args[i])
                            elif hasattr(param, 'default_value') and param.default_value:
                                self.set_variable(param.name, self.execute(param.default_value))
                            else:
                                self.set_variable(param.name, None)
                        
                    # Execute method body
                    result = None
                    if hasattr(method_def, 'body'):
                        from ..parser.ast import ReturnStatement
                        for statement in method_def.body:
                            result = self.execute(statement)
                            if isinstance(statement, ReturnStatement):
                                break
                        
                    # Update instance properties from scope
                    for key in obj.properties.keys():
                        try:
                            obj.set_property(key, self.get_variable(key))
                        except:
                            pass
                        
                    return result
                finally:
                    self.exit_scope()
            else:
                # Try to see if it's a property that is a callable
                if member_name in obj.properties:
                    prop = obj.get_property(member_name)
                    if callable(prop):
                        args = [self.execute(arg) for arg in node.arguments] if hasattr(node, 'arguments') and node.arguments else []
                        return prop(*args)
                    
                raise NxlNameError(
                    message=f"Object of type '{obj.class_name}' has no method '{member_name}'",
                    name=member_name,
                    line=getattr(node, 'line', None),
                    error_type_key="undefined_function",
                    full_source=self.source
                )
        else:
            # Property access
            try:
                return obj.get_property(member_name)
            except AttributeError:
                raise NxlNameError(
                    message=f"Object of type '{obj.class_name}' has no property '{member_name}'",
                    name=member_name,
                    line=getattr(node, 'line', None),
                    error_type_key="undefined_attribute",
                    full_source=self.source
                )

    # Handle dict-based objects (legacy/fallback)


    def _execute_member_access_dict(self, obj, node, member_name):
        """Handle member access on dict-based objects (legacy storage format)."""
        if node.is_method_call:
            # Evaluate arguments once for reuse below
            args = [self.execute(arg) for arg in node.arguments] if hasattr(node, 'arguments') and node.arguments else []

            # ── Native Python dict operations ────────────────────────────
            # These are mapped from natural NexusLang method names so that
            # keywords like 'has', 'set', 'add', 'contains', 'insert',
            # 'update' work correctly as method calls on dict objects.
            if member_name == 'has' or member_name == 'contains':
                # dict.has(key)  /  dict.contains(key) → key in dict
                if not args:
                    raise NxlRuntimeError("has() requires a key argument",
                                           line=getattr(node, 'line', None), full_source=self.source)
                return args[0] in obj

            elif member_name == 'get':
                # dict.get(key)  /  dict.get(key, default)
                if not args:
                    raise NxlRuntimeError("get() requires a key argument",
                                           line=getattr(node, 'line', None), full_source=self.source)
                return obj.get(args[0], args[1] if len(args) > 1 else None)

            elif member_name == 'set' or member_name == 'insert' or member_name == 'put':
                # dict.set(key, value)  /  dict.insert(key, value)  /  dict.put(key, value)
                if len(args) < 2:
                    raise NxlRuntimeError(f"{member_name}() requires key and value arguments",
                                           line=getattr(node, 'line', None), full_source=self.source)
                obj[args[0]] = args[1]
                return None

            elif member_name == 'remove' or member_name == 'delete':
                # dict.remove(key)  /  dict.delete(key)
                if not args:
                    raise NxlRuntimeError("remove() requires a key argument",
                                           line=getattr(node, 'line', None), full_source=self.source)
                obj.pop(args[0], None)
                return None

            elif member_name == 'pop':
                # dict.pop(key)  /  dict.pop(key, default)
                if not args:
                    raise NxlRuntimeError("pop() requires a key argument",
                                           line=getattr(node, 'line', None), full_source=self.source)
                return obj.pop(args[0]) if len(args) == 1 else obj.pop(args[0], args[1])

            elif member_name == 'keys':
                return list(obj.keys())

            elif member_name == 'values':
                return list(obj.values())

            elif member_name == 'items':
                return list(obj.items())

            elif member_name == 'update' or member_name == 'merge':
                # dict.update(other_dict)
                if args and isinstance(args[0], dict):
                    obj.update(args[0])
                return None

            elif member_name == 'clear':
                obj.clear()
                return None

            elif member_name == 'copy':
                return dict(obj)

            elif member_name == 'size' or member_name == 'length':
                return len(obj)

            elif member_name == 'is_empty':
                return len(obj) == 0

            # ── User-defined methods via __method_*__ ────────────────────
            method_key = f"__method_{member_name}__"
            if method_key in obj:
                # Execute the method with the instance as context
                method_def = obj[method_key]
                    
                # Enter new scope for method execution
                self.enter_scope()
                    
                try:
                    # Bind instance to 'self'
                    self.set_variable("self", obj)

                    # Bind instance fields to scope (implicit self)
                    # Fields are accessible directly as variables within the method
                    for key, value in obj.items():
                        if not key.startswith("__"):  # Skip internal keys
                            self.set_variable(key, value)
                        
                    # Bind method parameters
                    if hasattr(method_def, 'parameters') and method_def.parameters:
                        # Evaluate arguments
                        args = [self.execute(arg) for arg in node.arguments] if hasattr(node, 'arguments') and node.arguments else []
                            
                        # Bind parameters to arguments
                        for i, param in enumerate(method_def.parameters):
                            if i < len(args):
                                self.set_variable(param.name, args[i])
                            elif hasattr(param, 'default_value') and param.default_value:
                                self.set_variable(param.name, self.execute(param.default_value))
                            else:
                                self.set_variable(param.name, None)
                        
                    # Execute method body
                    result = None
                    if hasattr(method_def, 'body'):
                        from ..parser.ast import ReturnStatement
                        for statement in method_def.body:
                            result = self.execute(statement)
                            # Check if this was a return statement
                            if isinstance(statement, ReturnStatement):
                                break
                        
                    # Update instance fields from scope (fields may have been modified)
                    for key in obj.keys():
                        if not key.startswith("__"):
                            try:
                                obj[key] = self.get_variable(key)
                            except:
                                pass  # Field not modified
                        
                    return result
                finally:
                    self.exit_scope()
            else:
                raise NxlRuntimeError(
                    f"Unknown method: {member_name}",
                    line=getattr(node, 'line', None),
                    error_type_key="undefined_function",
                    full_source=self.source
                )
        else:
            # Property/field access
            if member_name in obj:
                return obj[member_name]
            else:
                raise NxlRuntimeError(
                    f"Unknown field: {member_name}",
                    line=getattr(node, 'line', None),
                    error_type_key="undefined_attribute",
                    full_source=self.source
                )
        
    # Handle regular Python objects

    def _call_method_on_object(self, obj, method_name: str, args: list):
        """Call a method on either runtime.Object or legacy dict-backed objects."""
        from ..runtime.runtime import Object as RuntimeObject

        if isinstance(obj, RuntimeObject):
            if method_name not in obj.methods:
                raise NxlNameError(
                    message=f"Object of type '{obj.class_name}' has no method '{method_name}'",
                    name=method_name,
                    error_type_key="undefined_function",
                    full_source=self.source,
                )

            method_def = obj.methods[method_name]

            if callable(method_def) and not hasattr(method_def, 'node_type'):
                return method_def(obj, *args)

            self.enter_scope()
            try:
                self.set_variable("self", obj)

                for key, value in obj.properties.items():
                    self.set_variable(key, value)

                if hasattr(method_def, 'parameters') and method_def.parameters:
                    for i, param in enumerate(method_def.parameters):
                        if i < len(args):
                            self.set_variable(param.name, args[i])
                        elif hasattr(param, 'default_value') and param.default_value:
                            self.set_variable(param.name, self.execute(param.default_value))
                        else:
                            self.set_variable(param.name, None)

                result = None
                if hasattr(method_def, 'body'):
                    try:
                        for statement in method_def.body:
                            result = self.execute(statement)
                    except ReturnException as ret:
                        result = ret.value

                for key in obj.properties.keys():
                    try:
                        obj.set_property(key, self.get_variable(key))
                    except Exception:
                        pass

                return result
            finally:
                self.exit_scope()

        if isinstance(obj, dict):
            method_key = f"__method_{method_name}__"
            if method_key not in obj:
                raise NxlRuntimeError(
                    f"Unknown method: {method_name}",
                    error_type_key="undefined_function",
                    full_source=self.source,
                )

            method_def = obj[method_key]

            self.enter_scope()
            try:
                self.set_variable("self", obj)

                for key, value in obj.items():
                    if not key.startswith("__"):
                        self.set_variable(key, value)

                if hasattr(method_def, 'parameters') and method_def.parameters:
                    for i, param in enumerate(method_def.parameters):
                        if i < len(args):
                            self.set_variable(param.name, args[i])
                        elif hasattr(param, 'default_value') and param.default_value:
                            self.set_variable(param.name, self.execute(param.default_value))
                        else:
                            self.set_variable(param.name, None)

                result = None
                if hasattr(method_def, 'body'):
                    try:
                        for statement in method_def.body:
                            result = self.execute(statement)
                    except ReturnException as ret:
                        result = ret.value

                for key in obj.keys():
                    if not key.startswith("__"):
                        try:
                            obj[key] = self.get_variable(key)
                        except Exception:
                            pass

                return result
            finally:
                self.exit_scope()

        raise NxlTypeError(
            f"Cannot call method '{method_name}' on value of type {type(obj).__name__}",
            error_type_key="invalid_operation",
            full_source=self.source,
        )

    def execute_member_assignment(self, node):
        """
        Execute member assignment: set object.field to value.
        
        Assigns a value to a struct/class field.
        """
        # Get the member access target
        member_access = node.target
        
        # Evaluate the object
        target_obj = self.execute(member_access.object_expr)
        member_name = member_access.member_name
        
        # Evaluate the value to assign
        value = self.execute(node.value)
        
        # Handle Struct/Union member assignment
        from ..runtime.structures import StructureInstance
        if isinstance(target_obj, StructureInstance):
             target_obj.set_field(member_name, value)
             return value

        # Handle regular object assignment
        if isinstance(target_obj, dict):
            target_obj[member_name] = value
            return value
            
        if hasattr(target_obj, 'set_property'):
            target_obj.set_property(member_name, value)
            return value
            
        if hasattr(target_obj, member_name):
            setattr(target_obj, member_name, value)
            return value
            
        raise NxlRuntimeError(
            f"Cannot assign to member '{member_name}' on object of type {type(target_obj)}",
            line=getattr(node, 'line', None),
            error_type_key="invalid_operation",
            full_source=self.source
        )
    
    def execute_generic_type_instantiation(self, node):
        """
        Execute generic type instantiation.
        Syntax: create list of Integer, create dict of String to Integer
        
        Creates typed collections using the GenericTypeRegistry.
        """
        from collections import deque
        
        generic_name = node.generic_name.lower()
        type_args = node.type_args
        
        # Create typed Python collections based on generic name
        # Integrates with GenericTypeRegistry for type checking
        
        if generic_name == "list":
            # Create a list with optional initial value
            if node.initial_value:
                initial = self.execute(node.initial_value)
                if isinstance(initial, list):
                    return initial.copy()
                else:
                    return [initial]
            else:
                return []
        
        elif generic_name in ("dict", "dictionary", "map"):
            # Create a dictionary/map
            if node.initial_value:
                initial = self.execute(node.initial_value)
                if isinstance(initial, dict):
                    return initial.copy()
                else:
                    raise NxlTypeError(
                        "Dictionary initial value must be a dictionary",
                        error_type_key="type_mismatch",
                        full_source=self.source,
                    )
            else:
                return {}
        
        elif generic_name == "set":
            # Create a set with optional initial values
            if node.initial_value:
                initial = self.execute(node.initial_value)
                if isinstance(initial, (list, tuple, set)):
                    return set(initial)
                else:
                    return {initial}
            else:
                return set()
        
        elif generic_name == "queue":
            # Create a queue (deque) with optional initial values
            if node.initial_value:
                initial = self.execute(node.initial_value)
                if isinstance(initial, (list, tuple)):
                    return deque(initial)
                else:
                    return deque([initial])
            else:
                return deque()
        
        elif generic_name == "stack":
            # Create a stack (list used as stack) with optional initial values
            if node.initial_value:
                initial = self.execute(node.initial_value)
                if isinstance(initial, (list, tuple)):
                    return list(initial)
                else:
                    return [initial]
            else:
                return []
        
        elif generic_name == "tuple":
            # Create an immutable tuple
            if node.initial_value:
                initial = self.execute(node.initial_value)
                if isinstance(initial, (list, tuple)):
                    return tuple(initial)
                else:
                    return (initial,)
            else:
                return ()
        
        elif generic_name == "optional":
            # Create an Optional value (can be None or a value)
            if node.initial_value:
                return self.execute(node.initial_value)
            else:
                return None
        
        else:
            # Unknown generic type - provide helpful error message
            supported = ["List", "Dict", "Set", "Queue", "Stack", "Tuple", "Optional", "Map"]
            raise NxlTypeError(
                f"Unsupported generic type: '{generic_name}'.\n"
                f"  Supported generic types: {', '.join(supported)}\n"
                f"  Note: This is a language limitation, not incomplete implementation.",
                line=getattr(node, 'line', None),
                error_type_key="invalid_generic_args",
                full_source=self.source
            )

    def execute_try_expression(self, node):
        """Execute ? operator for error propagation."""
        from ..stdlib.option_result import Result
        
        result = self.execute(node.expression)
        
        if not isinstance(result, Result):
            raise NxlRuntimeError(
                f'? operator requires Result type, got {type(result).__name__}',
                line=getattr(node, 'line', None),
                error_type_key="type_mismatch",
                full_source=self.source
            )
        
        if result.is_ok():
            return result.unwrap()
        else:
            self.return_value = result
            return None
    
    def execute_macro_definition(self, node):
        """Execute a macro definition - store it in registry."""
        self.macros[node.name] = node
        return None

    # ------------------------------------------------------------------
    # Compile-time evaluation
    # ------------------------------------------------------------------

    def execute_comptime_expression(self, node):
        """Execute a comptime eval expression - evaluate at runtime (current implementation)."""
        return self.execute(node.expr)

    def execute_comptime_const(self, node):
        """Execute a comptime const declaration - store as global compile-time constant."""
        value = self.execute(node.expr)
        self.comptime_constants[node.name] = value
        # Also register in global scope so NexusLang code can reference it
        self.current_scope[0][node.name] = value
        return value

    def execute_comptime_assert(self, node):
        """Execute a comptime assert - raise at 'compile time' (early in execution)."""
        condition = self.execute(node.condition)
        if not condition:
            message = "Compile-time assertion failed"
            if node.message_expr is not None:
                try:
                    msg_val = self.execute(node.message_expr)
                    message = f"Compile-time assertion failed: {msg_val}"
                except Exception:
                    pass
            raise NxlRuntimeError(
                message,
                line=node.line_number,
                error_type_key="runtime_error",
                full_source=self.source,
            )
        return condition

    def execute_attribute_declaration(self, node):
        """Register an attribute type definition in the attribute registry."""
        self.attribute_definitions[node.name] = {
            "properties": node.properties,  # list of (prop_name, type_str) tuples
        }
        return node.name

    def execute_macro_expansion(self, node):
        """Execute a macro expansion - substitute parameters and execute body."""
        # Get macro definition
        _macro_line = getattr(node, 'line_number', getattr(node, 'line', None))
        if node.name not in self.macros:
            raise NxlRuntimeError(
                f"Undefined macro: {node.name}",
                line=_macro_line,
                error_type_key="undefined_function",
                full_source=self.source,
            )
        
        macro_def = self.macros[node.name]
        
        # Evaluate all argument expressions
        evaluated_args = {}
        for arg_name, arg_expr in node.arguments.items():
            evaluated_args[arg_name] = self.execute(arg_expr)
        
        # Hygienic scope: save outer scope, run macro in isolation
        saved_scope = self.current_scope
        # Start from globals only so macro cannot see caller's local variables
        self.current_scope = [self.current_scope[0]]
        self.enter_scope()
        
        try:
            # Bind macro parameters to argument values
            for param_name in macro_def.parameters:
                if param_name in evaluated_args:
                    self.set_variable(param_name, evaluated_args[param_name])
                else:
                    raise NxlRuntimeError(
                        f"Macro {node.name} missing argument: {param_name}",
                        line=_macro_line,
                        error_type_key="wrong_argument_count",
                        full_source=self.source,
                    )
            
            # Execute macro body
            result = None
            for stmt in macro_def.body:
                result = self.execute(stmt)
        finally:
            # Restore original scope (macro variables cannot leak to caller)
            self.exit_scope()
            self.current_scope = saved_scope
        
        return result

    # ------------------------------------------------------------------
    # Ownership / borrow execution methods
    # ------------------------------------------------------------------

    def execute_move_expression(self, node):
        """Execute 'move x' — transfer ownership, leaving source as MovedValue.

        After a move:
        - The moved-to binding holds the original value.
        - Accessing the source variable raises a 'use of moved value' error.

        Syntax:  set dest to move source
        """
        from ..stdlib.smart_pointers import MovedValue

        var_name = node.var_name
        # Check the variable is not currently borrowed
        borrow = self._borrow_tracker.get(var_name)
        if borrow and (borrow["immutable_count"] > 0 or borrow["is_mutable"]):
            kind = "mutably" if borrow["is_mutable"] else "immutably"
            raise NxlRuntimeError(
                f"cannot move '{var_name}': it is currently borrowed {kind}; "
                f"call 'drop borrow {var_name}' before moving",
                line=getattr(node, 'line_number', None),
                error_type_key="runtime_error",
                full_source=self.source,
            )

        # Read the current value (will raise on MovedValue automatically)
        value = self.get_variable(var_name)

        # Invalidate the source by replacing with a MovedValue sentinel
        for scope in reversed(self.current_scope):
            if var_name in scope:
                scope[var_name] = MovedValue(var_name)
                break

        return value

    def execute_borrow_expression(self, node):
        """Execute 'borrow x' / 'borrow mutable x' — register a borrow.

        Immutable borrow:  multiple simultaneous immutable borrows are allowed;
                           the variable cannot be written while any borrow holds.
        Mutable borrow:    only one mutable borrow at a time;
                           no other borrow (immutable or mutable) may exist;
                           the variable cannot be written or read-borrowed while
                           the mutable borrow holds.

        Returns the current value of the variable (a snapshot, not a reference).

        Syntax:
            set b to borrow x
            set m to borrow mutable x
        """
        var_name = node.var_name
        mutable = node.mutable
        line = getattr(node, 'line_number', None)

        borrow = self._borrow_tracker.get(var_name, {"immutable_count": 0, "is_mutable": False})

        if mutable:
            # Mutable borrow: nothing else may be borrowed
            if borrow["immutable_count"] > 0:
                raise NxlRuntimeError(
                    f"cannot borrow '{var_name}' as mutable: it is already borrowed "
                    f"immutably ({borrow['immutable_count']} active borrow(s))",
                    line=line,
                    error_type_key="runtime_error",
                    full_source=self.source,
                )
            if borrow["is_mutable"]:
                raise NxlRuntimeError(
                    f"cannot borrow '{var_name}' as mutable: already mutably borrowed",
                    line=line,
                    error_type_key="runtime_error",
                    full_source=self.source,
                )
            self._borrow_tracker[var_name] = {"immutable_count": 0, "is_mutable": True}
        else:
            # Immutable borrow: not allowed if mutably borrowed
            if borrow["is_mutable"]:
                raise NxlRuntimeError(
                    f"cannot borrow '{var_name}' immutably: it is already mutably borrowed",
                    line=line,
                    error_type_key="runtime_error",
                    full_source=self.source,
                )
            self._borrow_tracker[var_name] = {
                "immutable_count": borrow["immutable_count"] + 1,
                "is_mutable": False,
            }

        return self.get_variable(var_name)

    def execute_drop_borrow_statement(self, node):
        """Execute 'drop borrow x' / 'drop borrow mutable x' — release a borrow.

        Decrements the borrow count.  The variable becomes writable again once
        all borrows have been dropped.
        """
        var_name = node.var_name
        mutable = node.mutable
        line = getattr(node, 'line_number', None)

        borrow = self._borrow_tracker.get(var_name)

        if mutable:
            if not borrow or not borrow["is_mutable"]:
                raise NxlRuntimeError(
                    f"no active mutable borrow of '{var_name}' to drop",
                    line=line,
                    error_type_key="runtime_error",
                    full_source=self.source,
                )
            self._borrow_tracker[var_name] = {"immutable_count": 0, "is_mutable": False}
        else:
            if not borrow or borrow["immutable_count"] <= 0:
                raise NxlRuntimeError(
                    f"no active immutable borrow of '{var_name}' to drop",
                    line=line,
                    error_type_key="runtime_error",
                    full_source=self.source,
                )
            new_count = borrow["immutable_count"] - 1
            if new_count == 0 and not borrow["is_mutable"]:
                del self._borrow_tracker[var_name]
            else:
                self._borrow_tracker[var_name] = {"immutable_count": new_count, "is_mutable": borrow["is_mutable"]}

        return None

    def execute_borrow_expression_with_lifetime(self, node):
        """Execute a borrow expression that carries a lifetime annotation.

        At runtime the lifetime label is purely a static analysis artifact and
        has no effect on program execution.  This handler therefore delegates
        directly to the standard borrow logic, which enforces the runtime borrow
        rules (no double-mutable borrow, no borrow while moved, etc.).

        Syntax::

            set ref to borrow x with lifetime outer
            set mut_ref to borrow mutable y with lifetime inner
        """
        # Delegate to the standard borrow handler.  The BorrowExpressionWithLifetime
        # node has the same var_name / mutable attributes, so it is duck-type
        # compatible here.
        return self.execute_borrow_expression(node)

    # ------------------------------------------------------------------
    # Smart pointer execution methods
    # ------------------------------------------------------------------

    def execute_rc_creation(self, node):
        """Execute Rc/Arc/Weak creation.

        Syntax (NexusLang):
            set ptr to Rc of Integer with 42
            set shared to Arc of String with "hello"
            set w to Weak of Integer with 0   # rarely used directly; prefer downgrade
        """
        from ..stdlib.smart_pointers import RcValue, ArcValue, WeakValue

        value = self.execute(node.value) if node.value is not None else None
        kind = getattr(node, 'rc_kind', 'rc')

        if kind == 'rc':
            return RcValue(value)
        elif kind == 'arc':
            return ArcValue(value)
        elif kind == 'weak':
            # A standalone Weak creation wraps a freshly-allocated inner that
            # immediately has strong_count=0 (the Weak is the only reference).
            # The idiomatic way to create a Weak is via 'downgrade ptr', not
            # via 'Weak of T with val'.  Support the literal form anyway.
            rc_temp = RcValue(value)   # strong=1
            weak = rc_temp.downgrade() # weak=1
            rc_temp.drop()             # strong -> 0 (data kept alive by weak ref)
            return weak
        else:
            raise NxlRuntimeError(
                f"Unknown smart pointer kind: {kind!r}",
                line=getattr(node, 'line_number', None),
                error_type_key="internal_error",
                full_source=self.source,
            )

    def execute_downgrade_expression(self, node):
        """Execute 'downgrade rc_value' — convert Rc<T>/Arc<T> to Weak<T>."""
        from ..stdlib.smart_pointers import RcValue, ArcValue

        rc = self.execute(node.rc_expr)
        if isinstance(rc, (RcValue, ArcValue)):
            return rc.downgrade()
        raise NxlRuntimeError(
            f"downgrade requires an Rc or Arc value, got {type(rc).__name__}",
            line=getattr(node, 'line_number', None),
            error_type_key="type_error",
            full_source=self.source,
        )

    def execute_upgrade_expression(self, node):
        """Execute 'upgrade weak_value' — attempt to recover a strong Rc/Arc.

        Returns the Rc/Arc value on success, or None if the data has been
        dropped (all strong counts reached zero).
        """
        from ..stdlib.smart_pointers import WeakValue

        weak = self.execute(node.weak_expr)
        if isinstance(weak, WeakValue):
            return weak.upgrade()   # returns RcValue / ArcValue or None
        raise NxlRuntimeError(
            f"upgrade requires a Weak value, got {type(weak).__name__}",
            line=getattr(node, 'line_number', None),
            error_type_key="type_error",
            full_source=self.source,
        )

    # ------------------------------------------------------------------
    # Missing dispatch methods — complete coverage for all AST nodes
    # ------------------------------------------------------------------

    def execute_string_literal(self, node):
        """Execute a string literal — return its value directly."""
        return node.value

    def execute_ternary_expression(self, node):
        """Execute a ternary expression: condition ? true_expr : false_expr."""
        condition = self.execute(node.condition)
        if condition:
            return self.execute(node.true_expr)
        else:
            return self.execute(node.false_expr)

    def execute_null_coalesce_expression(self, node):
        """Execute: value otherwise default — returns value if not None, else default."""
        value = self.execute(node.value)
        if value is None:
            return self.execute(node.default)
        return value

    def execute_panic_statement(self, node):
        """Execute a panic statement — raise an unrecoverable error."""
        message = self.execute(node.message) if node.message else "explicit panic"
        raise NxlRuntimeError(
            message=f"panic: {message}",
            line=getattr(node, 'line_number', None),
            error_type_key="panic",
            full_source=self.source,
        )

    def execute_fallthrough_statement(self, node):
        """Execute a fallthrough statement in a switch case."""
        raise FallthroughException()

    def execute_yield_expression(self, node):
        """Execute a yield expression — signal a generator yield."""
        value = self.execute(node.value) if node.value else None
        raise YieldException(value)

    def execute_generator_expression(self, node):
        """Execute a generator expression — create a lazy generator."""
        iterable = self.execute(node.iterable)
        results = []
        for item in iterable:
            self.set_variable(node.target.name, item)
            if node.condition is None or self.execute(node.condition):
                results.append(self.execute(node.expr))
        return iter(results)

    def execute_async_expression(self, node):
        """Execute an async expression — schedule for concurrent execution."""
        import concurrent.futures
        def run():
            return self.execute(node.expr)
        future = self.runtime.run_concurrent(run)
        return future

    def execute_method_definition(self, node):
        """Execute a standalone method definition (outside class context).
        
        When encountered at top level, register as a regular function.
        """
        self.functions[node.name] = node
        return None

    def execute_property_declaration(self, node):
        """Execute a standalone property declaration (outside class context).
        
        Store the default value in the current scope if provided.
        """
        value = None
        if node.default_value:
            value = self.execute(node.default_value)
        self.set_variable(node.name, value)
        return value

    def execute_abstract_method_definition(self, node):
        """Execute an abstract method definition — register as a method stub."""
        self.functions[node.name] = node
        return None

    def execute_export_statement(self, node):
        """Execute an export statement — mark names as exported from module."""
        if not hasattr(self, '_module_exports'):
            self._module_exports = set()
        for name in node.names:
            self._module_exports.add(name)
        return None

    def execute_module_definition(self, node):
        """Execute a module definition — register module namespace."""
        module_dict = {'__name__': node.name}
        for export_name in node.exports:
            val = self.get_variable_or_none(export_name)
            if val is not None:
                module_dict[export_name] = val
        self.set_variable(node.name, module_dict)
        return None

    def execute_decorator(self, node):
        """Execute a decorator — return the decorator metadata.
        
        Decorators are typically consumed by function/class definitions,
        not executed standalone. When dispatched alone, return metadata.
        """
        return {'__decorator__': node.name, 'arguments': node.arguments}

    def execute_type_alias(self, node):
        """Execute a type alias — register the alias in the type registry."""
        if hasattr(self, 'type_aliases'):
            self.type_aliases[node.name] = node.target_type
        else:
            self.type_aliases = {node.name: node.target_type}
        return None

    def execute_type_alias_definition(self, node):
        """Execute a type alias definition — register in type system."""
        if not hasattr(self, 'type_aliases'):
            self.type_aliases = {}
        self.type_aliases[node.name] = node.target_type
        return None

    def execute_rc_type(self, node):
        """Execute Rc<T> type annotation — return type metadata."""
        return {'__smart_pointer__': 'Rc', 'inner_type': node.inner_type}

    def execute_weak_type(self, node):
        """Execute Weak<T> type annotation — return type metadata."""
        return {'__smart_pointer__': 'Weak', 'inner_type': node.inner_type}

    def execute_arc_type(self, node):
        """Execute Arc<T> type annotation — return type metadata."""
        return {'__smart_pointer__': 'Arc', 'inner_type': node.inner_type}

    def execute_foreign_library_load(self, node):
        """Execute a foreign library load statement — load shared library via ctypes."""
        import ctypes
        try:
            lib = ctypes.CDLL(node.library_path)
            self.set_variable(node.alias, lib)
            return lib
        except OSError as e:
            raise NxlRuntimeError(
                message=f"Failed to load foreign library '{node.library_path}': {e}",
                line=getattr(node, 'line_number', None),
                error_type_key="ffi_error",
                full_source=self.source,
            )

    def execute_extern_type_declaration(self, node):
        """Execute an extern type declaration — register FFI type metadata."""
        type_info = {
            '__extern_type__': True,
            'name': node.name,
            'base_type': node.base_type,
            'is_opaque': node.is_opaque,
            'is_function_pointer': node.is_function_pointer,
            'function_signature': node.function_signature,
        }
        self.set_variable(node.name, type_info)
        return None

    def execute_callback_reference(self, node):
        """Execute a callback reference — resolve function name to callable."""
        func_name = node.function_name
        if func_name in self.functions:
            func_def = self.functions[func_name]
            def callback_wrapper(*args):
                self.enter_scope()
                try:
                    for i, param in enumerate(func_def.parameters):
                        if i < len(args):
                            self.set_variable(param.name, args[i])
                    result = None
                    for stmt in func_def.body:
                        result = self.execute(stmt)
                    return result
                except ReturnException as ret:
                    return ret.value
                finally:
                    self.exit_scope()
            return callback_wrapper
        elif func_name in self.runtime.functions:
            return self.runtime.functions[func_name]
        raise NxlNameError(
            name=func_name,
            available_names=list(self.functions.keys()),
            error_type_key="undefined_function",
            full_source=self.source,
        )

    def execute_allocator_hint(self, node):
        """Execute an allocator hint — return metadata for custom allocation."""
        return {
            '__allocator_hint__': True,
            'base_type': node.base_type,
            'allocator_name': node.allocator_name,
        }

