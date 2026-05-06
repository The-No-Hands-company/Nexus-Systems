"""
Type checker for the NexusLang language.
This module provides type checking functionality for NexusLang programs.
"""

from typing import Dict, List, Optional, Set, Union, Any, Tuple, Type
import re
from ..parser.ast import (
    Program, VariableDeclaration, FunctionDefinition, Parameter,
    IfStatement, WhileLoop, ForLoop, MemoryAllocation, MemoryDeallocation,
    ClassDefinition, PropertyDeclaration, MethodDefinition,
    ConcurrentExecution, TryCatch, BinaryOperation, UnaryOperation,
    Literal, Identifier, FunctionCall, RepeatNTimesLoop, RepeatWhileLoop,
    ReturnStatement, Block, ConcurrentBlock, TryCatchBlock,
    InterfaceDefinition, AbstractClassDefinition, TraitDefinition,
    TypeAliasDefinition, AbstractMethodDefinition,
    TypeParameter, TypeConstraint, TypeGuard,
    PrintStatement,  # Add print statement
    TypeCastExpression,  # Add type cast
    RaiseStatement,  # Add raise statement
    ParallelForLoop,
    ExpectStatement, RequireStatement, EnsureStatement,
    GuaranteeStatement, InvariantStatement,
    ComptimeExpression, ComptimeConst, ComptimeAssert,
    MacroDefinition, MacroExpansion,
    # Low-level constructs
    StructDefinition, UnionDefinition, ObjectInstantiation, MemberAssignment,
    SizeofExpression, AddressOfExpression, DereferenceExpression,
    # Allocator hint
    AllocatorHint,
    # Switch statement
    SwitchStatement,
    # Mutation/assignment nodes (used for contract side-effect detection)
    IndexAssignment, DereferenceAssignment,
)
from ..typesystem.types import (
    Type, PrimitiveType, ListType, DictionaryType, ClassType, 
    FunctionType, UnionType, AnyType, 
    INTEGER_TYPE, FLOAT_TYPE, STRING_TYPE, BOOLEAN_TYPE, NULL_TYPE, ANY_TYPE,
    get_type_by_name, infer_type, GenericType, GenericParameter, ChannelType
)
from ..typesystem.type_inference import TypeInferenceEngine
from ..typesystem.generic_types import GenericTypeRegistry, GenericTypeContext
from ..typesystem.generics_system import (
    GenericContext, GenericTypeInference, Monomorphizer,
    TypeConstraint as GenericConstraint, TypeParameterInfo
)
from ..typesystem.hkt import (
    GLOBAL_HKT_REGISTRY, HigherKindedType, TypeConstructorParam,
    TypeApplication, STAR, STAR_TO_STAR,
)

class TypeCheckError(Exception):
    """Exception raised for type checking errors."""
    pass

class TypeEnvironment:
    """Environment for type checking, containing variable and function types."""
    
    def __init__(self, parent: Optional['TypeEnvironment'] = None):
        self.parent = parent
        self.variables: Dict[str, Type] = {}
        self.functions: Dict[str, FunctionType] = {}
        self.return_type: Optional[Type] = None
        self.is_generator_function: bool = False
        self.expected_yield_type: Optional[Type] = None
        self.yielded_types: List[Type] = []
        
        # Generic support
        self.generic_context: Optional[GenericContext] = None
        self.type_parameters: Dict[str, TypeParameterInfo] = {}
    
    def enter_generic_scope(self, type_params: List[TypeParameterInfo]) -> None:
        """Enter a new generic scope with type parameters."""
        self.generic_context = GenericContext(parent=self.generic_context)
        for param in type_params:
            self.generic_context.add_type_parameter(param)
            self.type_parameters[param.name] = param
    
    def exit_generic_scope(self) -> None:
        """Exit the current generic scope."""
        if self.generic_context and self.generic_context.parent:
            self.generic_context = self.generic_context.parent
        else:
            self.generic_context = None
    
    def is_type_parameter(self, name: str) -> bool:
        """Check if a name is a type parameter."""
        return name in self.type_parameters
    
    def resolve_type(self, type_: Type) -> Type:
        """Resolve a type by applying generic substitutions."""
        if self.generic_context:
            return self.generic_context.resolve_type(type_)
        return type_
    
    def define_variable(self, name: str, type_: Type) -> None:
        """Define a variable with a type in the current scope."""
        self.variables[name] = type_

    def assign_variable_type(self, name: str, type_: Type) -> bool:
        """Assign an existing variable's type in the nearest enclosing scope."""
        if name in self.variables:
            self.variables[name] = type_
            return True
        if self.parent:
            return self.parent.assign_variable_type(name, type_)
        return False
    
    def define_function(self, name: str, type_: FunctionType) -> None:
        """Define a function with a type in the current scope."""
        self.functions[name] = type_
    
    def get_variable_type(self, name: str) -> Type:
        """Get the type of a variable from the environment."""
        if name in self.variables:
            return self.variables[name]
        
        if self.parent:
            return self.parent.get_variable_type(name)
        
        raise TypeCheckError(f"Undefined variable: {name}")
    
    def get_function_type(self, name: str) -> FunctionType:
        """Get the type of a function from the environment."""
        if name in self.functions:
            return self.functions[name]
        
        if self.parent:
            return self.parent.get_function_type(name)
        
        raise TypeCheckError(f"Undefined function: {name}")
    
    def set_return_type(self, type_: Type) -> None:
        """Set the expected return type for the current function scope."""
        self.return_type = type_
    
    def get_return_type(self) -> Optional[Type]:
        """Get the expected return type for the current function scope."""
        if self.return_type:
            return self.return_type
        
        if self.parent:
            return self.parent.get_return_type()
        
        return None

    def get_generator_context(self) -> Optional['TypeEnvironment']:
        """Get the nearest enclosing generator-function scope, if any."""
        if self.is_generator_function:
            return self
        if self.parent:
            return self.parent.get_generator_context()
        return None

class TypeRegistry(dict):
    """Registry for user-defined types (classes, interfaces, traits, aliases).

    Extends ``dict`` so callers can use the standard ``reg[name]``,
    ``name in reg``, and ``reg[name] = t`` patterns while also providing
    the domain-specific helpers expected by TypeChecker.
    """

    def __init__(self) -> None:
        super().__init__()
        # Map interface name -> frozenset of required method names.
        self._interfaces: Dict[str, Set[str]] = {}

    # ------------------------------------------------------------------
    # Helpers used by TypeChecker
    # ------------------------------------------------------------------

    def create_class_type(
        self,
        name: str,
        properties: Dict[str, Any],
        methods: Dict[str, Any],
        parent_classes: Optional[List[str]] = None,
    ) -> "ClassType":
        """Create a ClassType, register it, and return it."""
        class_type = ClassType(name, properties, methods, parent_classes)
        self[name] = class_type
        return class_type

    def register_interface(self, name: str, required_methods: List[str]) -> None:
        """Record which methods an interface mandates."""
        self._interfaces[name] = set(required_methods)

    def check_interface_implementation(
        self, class_name: str, interface_name: str
    ) -> List[str]:
        """Return the list of methods required by *interface_name* that
        *class_name* has not yet implemented.  Returns ``[]`` when the
        interface is unknown (forward reference) or fully satisfied.
        """
        required = self._interfaces.get(interface_name, set())
        if not required:
            return []
        class_type = self.get(class_name)
        implemented: Set[str] = set()
        if class_type is not None and hasattr(class_type, "methods"):
            implemented = set(class_type.methods.keys())
        return sorted(required - implemented)


class TypeChecker:
    """Type checker for NexusLang programs."""
    
    def __init__(self):
        self.env = TypeEnvironment()
        self.errors: List[str] = []
        # Use the type inference engine for improved inference
        self.type_inference = TypeInferenceEngine()
        self.type_inference_engine = self.type_inference  # Alias for pattern matching
        # Use the user-defined type registry
        self.type_registry = TypeRegistry()
        self.generic_registry = GenericTypeRegistry()
        self.current_class: Optional[str] = None
        self.current_trait: Optional[str] = None
        self.current_interface: Optional[str] = None
        self.abstract_methods: Dict[str, Set[str]] = {}
        self.trait_methods: Dict[str, Set[str]] = {}
        
        # Generic system components
        self.monomorphizer = Monomorphizer()
        self.generic_inference = GenericTypeInference()
        self.generic_functions: Dict[str, FunctionDefinition] = {}  # Track generic function templates
        self.macro_definitions: Dict[str, MacroDefinition] = {}

        # HKT registry: used to validate higher-kinded type constraints
        self.hkt_registry = GLOBAL_HKT_REGISTRY
    
    def check_program(self, program: Program) -> List[str]:
        """Check the types in a program and return any errors."""
        self.errors = []
        
        for statement in program.statements:
            self.check_statement(statement, self.env)
        
        return self.errors
    
    def check_statement(self, statement: Any, env: TypeEnvironment) -> Type:
        """Check the type of a statement."""
        # Standard AST nodes with dedicated check methods
        if isinstance(statement, VariableDeclaration):
            return self.check_variable_declaration(statement, env)
        elif isinstance(statement, FunctionDefinition):
            return self.check_function_definition(statement, env)
        elif isinstance(statement, ClassDefinition):
            return self.check_class_definition(statement, env)
        elif isinstance(statement, InterfaceDefinition):
            return self.check_interface_definition(statement, env)
        elif isinstance(statement, IfStatement):
            return self.check_if_statement(statement, env)
        elif isinstance(statement, WhileLoop):
            return self.check_while_loop(statement, env)
        elif isinstance(statement, ForLoop):
            return self.check_for_loop(statement, env)
        elif isinstance(statement, ParallelForLoop):
            return self.check_parallel_for_loop(statement, env)
        elif isinstance(statement, SwitchStatement):
            return self.check_switch_statement(statement, env)
        elif isinstance(statement, ExpectStatement):
            return self.check_expect_statement(statement, env)
        elif isinstance(statement, RequireStatement):
            return self.check_require_statement(statement, env)
        elif isinstance(statement, EnsureStatement):
            return self.check_ensure_statement(statement, env)
        elif isinstance(statement, GuaranteeStatement):
            return self.check_guarantee_statement(statement, env)
        elif isinstance(statement, InvariantStatement):
            return self.check_invariant_statement(statement, env)
        elif isinstance(statement, ComptimeExpression):
            return self.check_comptime_expression(statement, env)
        elif isinstance(statement, ComptimeConst):
            return self.check_comptime_const(statement, env)
        elif isinstance(statement, ComptimeAssert):
            return self.check_comptime_assert(statement, env)
        elif isinstance(statement, MacroDefinition):
            return self.check_macro_definition(statement, env)
        elif isinstance(statement, MacroExpansion):
            return self.check_macro_expansion(statement, env)
        elif isinstance(statement, MemoryAllocation):
            return self.check_memory_allocation(statement, env)
        elif isinstance(statement, MemoryDeallocation):
            return self.check_memory_deallocation(statement, env)
        elif isinstance(statement, RepeatNTimesLoop):
            return self.check_repeat_n_times_loop(statement, env)
        elif isinstance(statement, RepeatWhileLoop):
            return self.check_repeat_while_loop(statement, env)
        elif isinstance(statement, ReturnStatement):
            return self.check_return_statement(statement, env)
        elif statement.__class__.__name__ in ('BreakStatement', 'ContinueStatement', 'FallthroughStatement'):
            return ANY_TYPE
        elif isinstance(statement, Block):
            return self.check_block(statement, env)
        elif isinstance(statement, ConcurrentBlock):
            return self.check_concurrent_block(statement, env)
        elif isinstance(statement, TryCatchBlock):
            return self.check_try_catch_block(statement, env)
        elif isinstance(statement, TryCatch):
            return self.check_try_catch(statement, env)
        elif isinstance(statement, FunctionCall):
            return self.check_function_call(statement, env)
        elif isinstance(statement, BinaryOperation):
            return self.check_binary_operation(statement, env)
        elif isinstance(statement, UnaryOperation):
            return self.check_unary_operation(statement, env)
        elif isinstance(statement, Literal):
            return self.check_literal(statement, env)
        elif isinstance(statement, Identifier):
            return self.check_identifier(statement, env)
        elif isinstance(statement, AbstractClassDefinition):
            return self.check_abstract_class_definition(statement)
        elif isinstance(statement, TraitDefinition):
            return self.check_trait_definition(statement)
        elif isinstance(statement, TypeAliasDefinition):
            return self.check_type_alias_definition(statement)
        elif isinstance(statement, PrintStatement):
            return self.check_print_statement(statement, env)
        # Delegate remaining groups to focused helpers
        handled, result = self._check_import_statement(statement, env)
        if handled:
            return result
        handled, result = self._check_data_structure_statement(statement, env)
        if handled:
            return result
        handled, result = self._check_collection_expression(statement, env)
        if handled:
            return result
        handled, result = self._check_ffi_statement(statement, env)
        if handled:
            return result
        handled, result = self._check_inline_assembly_statement(statement, env)
        if handled:
            return result
        handled, result = self._check_match_expression_statement(statement, env)
        if handled:
            return result
        handled, result = self._check_ownership_statement(statement, env)
        if handled:
            return result
        raise TypeCheckError(f"Unsupported statement type: {statement.__class__.__name__}")

    def _check_import_statement(self, statement: Any, env: TypeEnvironment) -> Tuple[bool, Any]:
        """Handle import-related statement nodes. Returns (handled, type)."""
        cls = statement.__class__.__name__
        if cls == 'ImportStatement':
            if hasattr(statement, 'module_name'):
                if hasattr(statement, 'alias') and statement.alias:
                    module_name = statement.alias
                else:
                    module_name = statement.module_name
                    if '.' in module_name:
                        module_name = module_name.split('.')[-1]
                env.define_variable(module_name, ANY_TYPE)
            return True, ANY_TYPE
        if cls == 'SelectiveImport':
            if hasattr(statement, 'imported_names'):
                for name in statement.imported_names:
                    env.define_variable(name, ANY_TYPE)
            return True, ANY_TYPE
        if cls == 'ModuleAccess':
            return True, ANY_TYPE
        return False, None

    def _check_data_structure_statement(self, statement: Any, env: TypeEnvironment) -> Tuple[bool, Any]:
        """Handle struct/union/object/memory statement nodes. Returns (handled, type)."""
        cls = statement.__class__.__name__
        if cls == 'SendStatement':
            self._check_send_statement(statement, env)
            return True, ANY_TYPE
        if cls == 'CloseStatement':
            self._check_close_statement(statement, env)
            return True, ANY_TYPE
        if isinstance(statement, (StructDefinition, UnionDefinition, ObjectInstantiation, MemberAssignment)):
            return True, ANY_TYPE
        if cls == 'IndexAssignment':
            if hasattr(statement, 'target'):
                self.check_expression(statement.target, env)
            if hasattr(statement, 'value'):
                return True, self.check_expression(statement.value, env)
            return True, ANY_TYPE
        if isinstance(statement, SizeofExpression):
            return True, INTEGER_TYPE
        if isinstance(statement, (AddressOfExpression, DereferenceExpression)):
            return True, ANY_TYPE
        if isinstance(statement, TypeCastExpression):
            return True, self.check_type_cast(statement, env)
        if cls == 'LambdaExpression':
            return True, self.check_lambda_expression(statement, env)
        if isinstance(statement, RaiseStatement):
            if statement.message:
                message_type = self.check_statement(statement.message, env)
                if message_type != STRING_TYPE and message_type != ANY_TYPE:
                    self.errors.append(
                        f"Line {getattr(statement, 'line_number', '?')}: Raise message should be String, got {message_type}"
                    )
            return True, ANY_TYPE
        if hasattr(statement, 'node_type') and getattr(statement, 'node_type', None) == 'conditional_compilation_block':
            branch_env = env.create_child_scope() if hasattr(env, 'create_child_scope') else env
            for stmt in (statement.body or []):
                self.check_statement(stmt, branch_env)
            if statement.else_body:
                for stmt in statement.else_body:
                    self.check_statement(stmt, branch_env)
            return True, ANY_TYPE
        return False, None

    def _check_collection_expression(self, statement: Any, env: TypeEnvironment) -> Tuple[bool, Any]:
        """Handle collection literal and comprehension nodes. Returns (handled, type)."""
        cls = statement.__class__.__name__
        if cls == 'ChannelCreation':
            return True, ChannelType(ANY_TYPE)
        if cls == 'ReceiveExpression':
            return True, self._check_receive_expression(statement, env)
        if cls == 'ListExpression':
            return True, self.check_list_expression(statement, env)
        if cls == 'DictExpression':
            return True, self.check_dict_expression(statement, env)
        if cls == 'ListComprehension':
            return True, self.check_list_comprehension(statement, env)
        if cls == 'DictComprehension':
            return True, self.check_dict_comprehension(statement, env)
        if cls == 'GeneratorExpression':
            return True, self.check_generator_expression(statement, env)
        if cls == 'YieldExpression':
            return True, self.check_yield_expression(statement, env)
        if cls == 'MemberAccess':
            return True, self.check_member_access(statement, env)
        if cls == 'IndexExpression':
            return True, self.check_index_expression(statement, env)
        if cls == 'GenericTypeInstantiation':
            return True, self.check_generic_type_instantiation(statement, env)
        return False, None

    def _check_send_statement(self, statement: Any, env: TypeEnvironment) -> None:
        """Type check sending a value to a channel."""
        value_type = ANY_TYPE
        channel_type: Type = ANY_TYPE

        if hasattr(statement, 'value'):
            value_type = self.check_expression(statement.value, env)
        if hasattr(statement, 'channel'):
            channel_type = self.check_expression(statement.channel, env)

        if isinstance(channel_type, AnyType):
            return

        if not isinstance(channel_type, ChannelType):
            self.errors.append(
                f"Line {getattr(statement, 'line_number', '?')}: "
                f"Send target must be a channel, got '{self._type_name(channel_type)}'"
            )
            return

        payload_type = channel_type.payload_type

        # First observed payload type on an untyped channel refines the channel.
        if isinstance(payload_type, AnyType) and not isinstance(value_type, AnyType):
            self._refine_channel_identifier_type(getattr(statement, 'channel', None), value_type, env)
            return

        if not value_type.is_compatible_with(payload_type):
            self.errors.append(
                f"Line {getattr(statement, 'line_number', '?')}: "
                f"Cannot send value of type '{self._type_name(value_type)}' to channel of "
                f"'{self._type_name(payload_type)}'"
            )

    def _check_receive_expression(self, statement: Any, env: TypeEnvironment) -> Type:
        """Type check receiving a value from a channel."""
        if not hasattr(statement, 'channel'):
            return ANY_TYPE

        channel_type = self.check_expression(statement.channel, env)

        if isinstance(channel_type, AnyType):
            return ANY_TYPE

        if not isinstance(channel_type, ChannelType):
            self.errors.append(
                f"Line {getattr(statement, 'line_number', '?')}: "
                f"Receive target must be a channel, got '{self._type_name(channel_type)}'"
            )
            return ANY_TYPE

        return channel_type.payload_type

    def _check_close_statement(self, statement: Any, env: TypeEnvironment) -> None:
        """Type check closing a channel."""
        if not hasattr(statement, 'channel'):
            return

        channel_type = self.check_expression(statement.channel, env)

        if isinstance(channel_type, AnyType):
            return

        if not isinstance(channel_type, ChannelType):
            self.errors.append(
                f"Line {getattr(statement, 'line_number', '?')}: "
                f"Close target must be a channel, got '{self._type_name(channel_type)}'"
            )

    def _refine_channel_identifier_type(self, channel_expr: Any, payload_type: Type, env: TypeEnvironment) -> None:
        """Refine an identifier channel from Channel[Any] to Channel[payload_type]."""
        if not isinstance(channel_expr, Identifier):
            return

        try:
            existing = env.get_variable_type(channel_expr.name)
        except TypeCheckError:
            return

        if not isinstance(existing, ChannelType):
            return

        if not isinstance(existing.payload_type, AnyType):
            return

        refined = ChannelType(payload_type)
        if not env.assign_variable_type(channel_expr.name, refined):
            env.define_variable(channel_expr.name, refined)

    def _check_ffi_statement(self, statement: Any, env: TypeEnvironment) -> Tuple[bool, Any]:
        """Handle FFI extern declaration nodes. Returns (handled, type)."""
        cls = statement.__class__.__name__
        if cls == 'ExternFunctionDeclaration':
            if hasattr(statement, 'name'):
                param_types = []
                if hasattr(statement, 'parameters'):
                    for param in statement.parameters:
                        param_type = get_type_by_name(param.type_annotation) if hasattr(param, 'type_annotation') else ANY_TYPE
                        param_types.append(param_type)
                return_type = get_type_by_name(statement.return_type) if hasattr(statement, 'return_type') else ANY_TYPE
                func_type = FunctionType(param_types, return_type)
                if hasattr(statement, 'variadic') and statement.variadic:
                    func_type.variadic = True
                env.define_function(statement.name, func_type)
            return True, ANY_TYPE
        if cls == 'ExternVariableDeclaration':
            if hasattr(statement, 'name') and hasattr(statement, 'type_annotation'):
                var_type = get_type_by_name(statement.type_annotation)
                env.define_variable(statement.name, var_type)
            return True, ANY_TYPE
        if cls == 'ExternTypeDeclaration':
            return True, ANY_TYPE
        return False, None

    def _check_inline_assembly_statement(self, statement: Any, env: TypeEnvironment) -> Tuple[bool, Any]:
        """Handle inline assembly nodes. Returns (handled, type)."""
        if statement.__class__.__name__ != 'InlineAssembly':
            return False, None
        line = getattr(statement, 'line_number', '?')

        if hasattr(statement, 'inputs'):
            for constraint, expr in statement.inputs:
                normalized = self._normalize_asm_token(constraint)
                if not self._is_valid_asm_constraint(normalized, is_output=False):
                    self.errors.append(
                        f"Line {line}: Invalid inline assembly input constraint '{normalized}'"
                    )
                self.check_expression(expr, env)

        if hasattr(statement, 'outputs'):
            for constraint, target in statement.outputs:
                normalized = self._normalize_asm_token(constraint)
                if not self._is_valid_asm_constraint(normalized, is_output=True):
                    self.errors.append(
                        f"Line {line}: Invalid inline assembly output constraint '{normalized}'"
                    )
                if not isinstance(target, Identifier):
                    self.errors.append(
                        f"Line {line}: Inline assembly output operand must be an identifier"
                    )
                else:
                    try:
                        env.get_variable_type(target.name)
                    except TypeCheckError:
                        self.errors.append(
                            f"Line {line}: Undefined output variable in inline assembly: {target.name}"
                        )

        seen_clobbers: Set[str] = set()
        if hasattr(statement, 'clobbers'):
            for clobber in statement.clobbers:
                normalized = self._normalize_asm_token(clobber)
                if not self._is_valid_asm_clobber(normalized):
                    self.errors.append(
                        f"Line {line}: Invalid inline assembly clobber '{normalized}'"
                    )
                    continue
                if normalized in seen_clobbers:
                    self.errors.append(
                        f"Line {line}: Duplicate inline assembly clobber '{normalized}'"
                    )
                seen_clobbers.add(normalized)

        return True, INTEGER_TYPE

    def _normalize_asm_token(self, token: Any) -> str:
        """Normalize parser-provided asm token strings (strip quotes/whitespace)."""
        text = str(token).strip()
        if len(text) >= 2 and text[0] == '"' and text[-1] == '"':
            text = text[1:-1]
        return text.strip()

    def _is_valid_asm_constraint(self, constraint: str, is_output: bool) -> bool:
        """Validate inline asm operand constraints using common GCC-style forms."""
        if not constraint or any(ch.isspace() for ch in constraint):
            return False

        # Output constraints must include write marker ('=' or '+').
        if is_output and ('=' not in constraint and '+' not in constraint):
            return False

        # Common token grammar: optional modifiers + body (register/memory/immediate classes).
        if not re.fullmatch(r'[=+&%]*[A-Za-z0-9_{}(),|<>\-]+', constraint):
            return False

        return True

    def _is_valid_asm_clobber(self, clobber: str) -> bool:
        """Validate inline asm clobber names."""
        if not clobber or any(ch.isspace() for ch in clobber):
            return False
        if clobber in ('memory', 'cc'):
            return True
        return re.fullmatch(r'%?[A-Za-z][A-Za-z0-9_]*', clobber) is not None

    def _check_match_expression_statement(self, statement: Any, env: TypeEnvironment) -> Tuple[bool, Any]:
        """Handle match/case expression nodes. Returns (handled, type)."""
        if statement.__class__.__name__ != 'MatchExpression':
            return False, None
        match_expr_type = self.check_expression(statement.expression, env)
        result_types = []
        for case in statement.cases:
            case_env = TypeEnvironment(parent=env)
            self._bind_pattern_types(case.pattern, match_expr_type, case_env)
            if case.guard:
                guard_type = self.check_expression(case.guard, case_env)
                if guard_type != BOOLEAN_TYPE and guard_type != ANY_TYPE:
                    self.errors.append(
                        f"Line {getattr(case, 'line_number', '?')}: Guard condition must be boolean, "
                        f"got {guard_type}"
                    )
            body_type = ANY_TYPE
            for stmt in case.body:
                body_type = self.check_statement(stmt, case_env)
            result_types.append(body_type)
        return True, self._unify_result_types(result_types)

    def _bind_pattern_types(self, pattern: Any, match_expr_type: Type, case_env: TypeEnvironment) -> None:
        """Bind pattern variable names to their inferred types in *case_env*."""
        if hasattr(self, 'type_inference_engine'):
            bindings = self.type_inference_engine.infer_pattern_binding_type(pattern, match_expr_type)
            for var_name, var_type in bindings.items():
                case_env.define_variable(var_name, var_type)
            return
        if hasattr(pattern, 'name'):
            case_env.define_variable(pattern.name, match_expr_type)
        elif hasattr(pattern, 'binding') and pattern.binding:
            inner_type = ANY_TYPE
            if hasattr(match_expr_type, 'type_parameters') and match_expr_type.type_parameters:
                inner_type = match_expr_type.type_parameters[0]
            case_env.define_variable(pattern.binding, inner_type)
        elif hasattr(pattern, 'bindings'):
            for binding in pattern.bindings:
                case_env.define_variable(binding, ANY_TYPE)

    def _unify_result_types(self, result_types: List[Type]) -> Type:
        """Return the unified type of *result_types*, or ANY_TYPE if unification fails."""
        if not result_types:
            return ANY_TYPE
        if all(t == result_types[0] for t in result_types):
            return result_types[0]
        if hasattr(self, 'type_inference_engine'):
            unified_type = result_types[0]
            for rt in result_types[1:]:
                unified = self.type_inference_engine.unify_types(unified_type, rt)
                if unified:
                    unified_type = unified
                else:
                    return ANY_TYPE
            return unified_type
        return ANY_TYPE

    def _check_ownership_statement(self, statement: Any, env: TypeEnvironment) -> Tuple[bool, Any]:
        """Handle ownership/smart-pointer nodes. Returns (handled, type)."""
        cls = statement.__class__.__name__
        if cls == 'RcCreation':
            if hasattr(statement, 'value') and statement.value is not None:
                self.check_statement(statement.value, env)
            return True, ANY_TYPE
        if cls == 'DowngradeExpression':
            if hasattr(statement, 'rc_expr') and statement.rc_expr is not None:
                self.check_statement(statement.rc_expr, env)
            return True, ANY_TYPE
        if cls == 'UpgradeExpression':
            if hasattr(statement, 'weak_expr') and statement.weak_expr is not None:
                self.check_statement(statement.weak_expr, env)
            return True, ANY_TYPE
        if cls == 'MoveExpression':
            var_name = getattr(statement, 'var_name', None)
            if var_name is not None:
                try:
                    return True, env.get_variable_type(var_name)
                except Exception:
                    pass
            return True, ANY_TYPE
        if cls in ('BorrowExpression', 'BorrowExpressionWithLifetime'):
            var_name = getattr(statement, 'var_name', None)
            if var_name is not None:
                try:
                    return True, env.get_variable_type(var_name)
                except Exception:
                    pass
            return True, ANY_TYPE
        if cls in ('DropBorrowStatement', 'LifetimeAnnotation', 'MovedValue'):
            return True, ANY_TYPE
        return False, None
    
    def check_expression(self, expression: Any, env: TypeEnvironment) -> Type:
        """Check the type of an expression. Alias for check_statement for compatibility."""
        return self.check_statement(expression, env)
    
    def check_variable_declaration(self, declaration: VariableDeclaration, env: TypeEnvironment) -> Type:
        """Check the type of a variable declaration with bidirectional inference."""
        # If there's a type annotation, use bidirectional inference
        if declaration.type_annotation:
            # AllocatorHint wraps a base type with an allocator name.
            # For type checking purposes we use the base type and also validate
            # that the allocator name resolves to an allocator in scope.
            raw_annotation = declaration.type_annotation
            if isinstance(raw_annotation, AllocatorHint):
                alloc_name = raw_annotation.allocator_name
                if alloc_name not in env.variables:
                    self.errors.append(
                        f"Line {getattr(declaration, 'line_number', '?')}: "
                        f"Allocator '{alloc_name}' is not defined in the current scope"
                    )
                effective_annotation = raw_annotation.base_type
            else:
                effective_annotation = raw_annotation

            declared_type = get_type_by_name(effective_annotation)
            
            # Use bidirectional inference: expected type guides value type inference
            value_type = self.type_inference.infer_with_expected_type(
                declaration.value, declared_type, env.variables
            )
            
            if not value_type.is_compatible_with(declared_type):
                _line = getattr(declaration, 'line_number', getattr(declaration, 'line', '?'))
                self.errors.append(
                    f"Line {_line}: Type error: Cannot assign value of type "
                    f"'{value_type}' to variable '{declaration.name}' of type '{declared_type}'"
                )
                # Define the variable with the declared type anyway (for error recovery)
                env.define_variable(declaration.name, declared_type)
                return declared_type
            
            # Define the variable with the declared type
            env.define_variable(declaration.name, declared_type)
            return declared_type
        
        # If there's no type annotation, infer without expected type
        value_type = self.check_expression(declaration.value, env)
        inferred_type = self.type_inference.infer_variable_declaration(declaration, env.variables)
        if inferred_type != value_type and inferred_type != ANY_TYPE:
            # If inference came up with a more specific type, use it
            value_type = inferred_type
        
        # If value is a function reference, get its type
        if hasattr(declaration.value, '__class__') and declaration.value.__class__.__name__ == 'Identifier':
            try:
                func_type = env.get_function_type(declaration.value.name)
                value_type = func_type
            except TypeCheckError:
                pass  # Not a function, use inferred type
        
        # Define the variable with the inferred type
        env.define_variable(declaration.name, value_type)
        return value_type
    
    def check_function_definition(self, definition: FunctionDefinition, env: TypeEnvironment) -> Type:
        """Check the type of a function definition (with generic support)."""
        # Check if this is a generic function
        is_generic = bool(definition.type_parameters)
        contains_yield = any(self._statement_contains_yield(stmt) for stmt in definition.body)
        
        if is_generic:
            # Store generic function template for later instantiation
            self.generic_functions[definition.name] = definition
            
            # Build type parameter info
            type_param_info = []
            for param_name in definition.type_parameters:
                # Find constraints for this parameter
                constraints = []
                if hasattr(definition, 'type_constraints'):
                    # New format: dict mapping parameter name to list of trait names
                    if isinstance(definition.type_constraints, dict):
                        trait_names = definition.type_constraints.get(param_name, [])
                        for trait_name in trait_names:
                            # For now, store trait names directly
                            # Validation will happen at instantiation time
                            constraints.append(trait_name)
                    # Old format: list of constraint objects (for backward compatibility)
                    elif isinstance(definition.type_constraints, list):
                        for constraint in definition.type_constraints:
                            if hasattr(constraint, 'type_parameter') and constraint.type_parameter == param_name:
                                # Convert AST constraint to type system constraint
                                constraint_type = get_type_by_name(constraint.constraint_type)
                                constraints.append(constraint_type)
                
                # Store parameter info with constraints
                if constraints:
                    # For now, we'll just track that constraints exist
                    # Full validation happens during type checking
                    pass
                
                type_param_info.append({
                    'name': param_name,
                    'constraints': constraints
                })
            
            # Note: We don't enter a generic scope here anymore
            # Just track that this is a generic function
        
        # Create a new environment for the function scope
        function_env = TypeEnvironment(env)
        if is_generic:
            function_env.generic_context = env.generic_context
            function_env.type_parameters = env.type_parameters
        
        # Process parameters
        param_types = []
        min_params = 0  # Count of required parameters (without defaults)
        has_defaults = False
        has_variadic = False
        variadic_index = -1
        
        for i, param in enumerate(definition.parameters):
            param_type = ANY_TYPE
            
            # Check if this is a variadic parameter
            is_variadic_param = hasattr(param, 'is_variadic') and param.is_variadic
            if is_variadic_param:
                has_variadic = True
                variadic_index = i
            
            if param.type_annotation:
                # Check if this is a type parameter
                if env.is_type_parameter(param.type_annotation):
                    param_type = GenericParameter(param.type_annotation)
                else:
                    param_type = get_type_by_name(param.type_annotation)
                
                # Wrap variadic parameter type in ListType
                if is_variadic_param:
                    param_type = ListType(param_type)
            elif is_variadic_param:
                # Variadic parameter with no type annotation defaults to List of Any
                param_type = ListType(ANY_TYPE)
            
            param_types.append(param_type)
            function_env.define_variable(param.name, param_type)
            
            # Track if parameter has a default value
            if hasattr(param, 'default_value') and param.default_value is not None:
                has_defaults = True
            elif not is_variadic_param:
                # If no default and not variadic, this parameter is required
                if not has_defaults and not has_variadic:
                    min_params += 1
        
        # Set return type
        declared_return_type = ANY_TYPE
        has_explicit_return_type = bool(definition.return_type)
        if definition.return_type:
            if env.is_type_parameter(definition.return_type):
                declared_return_type = GenericParameter(definition.return_type)
            else:
                declared_return_type = get_type_by_name(definition.return_type)
        else:
            inferred_return_type = self.type_inference.infer_function_return_type(definition, env.variables)
            if inferred_return_type != ANY_TYPE:
                declared_return_type = inferred_return_type

        return_type = declared_return_type
        function_env.is_generator_function = contains_yield
        if contains_yield:
            if has_explicit_return_type:
                if isinstance(declared_return_type, ListType):
                    function_env.expected_yield_type = declared_return_type.element_type
                    return_type = declared_return_type
                else:
                    self.errors.append(
                        f"Type error: Function '{definition.name}' is a generator function and must declare a list-like return type, got '{self._type_name(declared_return_type)}'"
                    )
                    function_env.expected_yield_type = ANY_TYPE
                    return_type = ListType(ANY_TYPE)
            else:
                function_env.expected_yield_type = ANY_TYPE
                return_type = ListType(ANY_TYPE)
        
        function_env.set_return_type(return_type)
        
        # Create function type with default parameter info
        function_type = FunctionType(param_types, return_type)
        function_type.has_defaults = has_defaults
        function_type.min_params = min_params
        function_type.variadic = has_variadic
        function_type.variadic_index = variadic_index
        
        # Define the function in the parent environment
        env.define_function(definition.name, function_type)
        
        # Check the function body
        for statement in definition.body:
            self.check_statement(statement, function_env)

        if contains_yield:
            inferred_yield_type = self._infer_generator_yield_type(function_env.yielded_types, definition.name)
            if not has_explicit_return_type:
                return_type = ListType(inferred_yield_type)
            elif isinstance(declared_return_type, ListType):
                return_type = declared_return_type

            function_env.set_return_type(return_type)
        
        if is_generic:
            # Exit generic scope
            env.exit_generic_scope()

        function_type.return_type = return_type
        env.define_function(definition.name, function_type)
        
        return function_type
    
    def check_if_statement(self, statement: IfStatement, env: TypeEnvironment) -> Type:
        """Check an if statement."""
        # Check the condition
        condition_type = self.check_statement(statement.condition, env)
        
        # Allow ANY_TYPE (unknown) and FunctionType (likely a method call whose
        # return type could not be statically resolved) to pass through without
        # an error. Only hard-fail on concrete non-boolean types.
        if (not isinstance(condition_type, (AnyType, FunctionType))
                and not condition_type.is_compatible_with(BOOLEAN_TYPE)):
            self.errors.append(
                f"Type error: If condition must be a boolean, got '{condition_type}'"
            )
        
        # Check the then branch
        then_env = TypeEnvironment(env)
        then_type = NULL_TYPE
        for stmt in statement.then_block:
            then_type = self.check_statement(stmt, then_env)
        
        # Check the else branch if it exists
        else_type = NULL_TYPE
        if statement.else_block:
            else_env = TypeEnvironment(env)
            for stmt in statement.else_block:
                else_type = self.check_statement(stmt, else_env)
        
        # The type of an if statement is the union of the then and else branch types
        return UnionType([then_type, else_type])
    
    def check_while_loop(self, loop: WhileLoop, env: TypeEnvironment) -> Type:
        """Check a while loop."""
        # Check the condition
        condition_type = self.check_statement(loop.condition, env)
        
        # Allow ANY_TYPE (unknown) and FunctionType (likely a method call whose
        # return type could not be statically resolved) to pass through without
        # an error. Only hard-fail on concrete non-boolean types.
        if (not isinstance(condition_type, (AnyType, FunctionType))
                and not condition_type.is_compatible_with(BOOLEAN_TYPE)):
            self.errors.append(
                f"Type error: While condition must be a boolean, got '{condition_type}'"
            )
        
        # Check the body
        loop_env = TypeEnvironment(env)
        result_type = NULL_TYPE
        for stmt in loop.body:
            result_type = self.check_statement(stmt, loop_env)
        
        return result_type
    
    def check_for_loop(self, loop: ForLoop, env: TypeEnvironment) -> Type:
        """Check a for loop."""
        # Check the iterable
        iterable_type = self.check_statement(loop.iterable, env)
        
        # Create a new environment for the loop body
        loop_env = TypeEnvironment(env)
        
        # Infer the element type from the iterable type.
        # AnyType is allowed (runtime-registered stdlib functions return ANY_TYPE).
        if isinstance(iterable_type, ListType):
            element_type = iterable_type.element_type
        elif isinstance(iterable_type, AnyType):
            element_type = ANY_TYPE  # Dynamic iterable — skip type error
        else:
            element_type = ANY_TYPE
            self.errors.append(
                f"Type error: For loop iterable must be a list, got '{iterable_type}'"
            )
        
        # Define the iterator variable in the loop environment
        loop_env.define_variable(loop.iterator, element_type)
        
        # Check the loop body
        result_type = NULL_TYPE
        for stmt in loop.body:
            result_type = self.check_statement(stmt, loop_env)
        
        return result_type

    def check_parallel_for_loop(self, loop: ParallelForLoop, env: TypeEnvironment) -> Type:
        """Check a parallel for-each loop."""
        iterable_type = self.check_statement(loop.iterable, env)

        loop_env = TypeEnvironment(env)
        if isinstance(iterable_type, ListType):
            element_type = iterable_type.element_type
        elif isinstance(iterable_type, AnyType):
            element_type = ANY_TYPE
        else:
            element_type = ANY_TYPE
            self.errors.append(
                f"Type error: Parallel for iterable must be a list, got '{self._type_name(iterable_type)}'"
            )

        loop_env.define_variable(loop.var_name, element_type)

        result_type = NULL_TYPE
        for stmt in loop.body:
            result_type = self.check_statement(stmt, loop_env)

        # Detect loop-carried dependencies and unsupported reduction-style
        # accumulator updates inside the parallel region.
        outer_mutations = self._collect_outer_mutations(
            loop.body,
            env,
            excluded_names={loop.var_name},
        )
        for mutation in outer_mutations:
            if mutation["kind"] == "reduction":
                self.errors.append(
                    f"Type error: parallel for loop uses outer variable '{mutation['name']}' as a reduction variable "
                    f"inside the parallel region — potential data race (loop-carried dependency); "
                    f"use parallel_reduce or aggregate results after the loop"
                )
                continue

            self.errors.append(
                f"Type error: parallel for loop {mutation['detail']} '{mutation['name']}' — "
                f"potential data race (loop-carried dependency)"
            )

        return result_type

    def _collect_outer_mutations(
            self,
            body: list,
            outer_env: TypeEnvironment,
            excluded_names: Optional[Set[str]] = None) -> List[Dict[str, str]]:
        """Return structured outer-scope write hazards found inside a parallel region.

        Hazards include:
        - writes to an outer variable
        - mutations of outer object members
        - mutations of outer indexed collections
        - reduction-style writes where an outer variable is reassigned using its
          own previous value (for example: `set total to total plus x`)
        """
        mutated: List[Dict[str, str]] = []
        excluded = excluded_names or set()
        non_executed_scope_nodes = {
            "FunctionDefinition",
            "MethodDefinition",
            "ClassDefinition",
            "InterfaceDefinition",
            "AbstractClassDefinition",
            "TraitDefinition",
        }

        def _outer_name_exists(name: str) -> bool:
            try:
                outer_env.get_variable_type(name)
                return True
            except TypeCheckError:
                return False

        def _record(name: str, kind: str, detail: str) -> None:
            if name in excluded:
                return
            if not _outer_name_exists(name):
                return
            entry = {"name": name, "kind": kind, "detail": detail}
            if entry not in mutated:
                mutated.append(entry)

        def _expression_mentions_identifier(node: Any, identifier: str) -> bool:
            if node is None:
                return False
            if isinstance(node, Identifier):
                return node.name == identifier

            for attr in (
                    "left", "right", "operand", "value", "condition",
                    "object_expr", "target", "iterable", "index"):
                child = getattr(node, attr, None)
                if child is not None and _expression_mentions_identifier(child, identifier):
                    return True

            for attr in ("arguments", "elements", "body"):
                children = getattr(node, attr, None)
                if isinstance(children, list):
                    for child in children:
                        if _expression_mentions_identifier(child, identifier):
                            return True

            return False

        def _walk(nodes):
            for node in nodes:
                if node.__class__.__name__ in non_executed_scope_nodes:
                    # Nested declarations are not immediately executed as loop body writes.
                    continue

                if isinstance(node, VariableDeclaration):
                    if _expression_mentions_identifier(getattr(node, "value", None), node.name):
                        _record(
                            node.name,
                            "reduction",
                            "uses outer variable as a reduction variable",
                        )
                    else:
                        _record(node.name, "variable-write", "writes to outer variable")

                if isinstance(node, MemberAssignment):
                    target = getattr(node, "target", None)
                    base_name = getattr(getattr(target, "object_expr", None), "name", None)
                    if isinstance(base_name, str):
                        _record(base_name, "member-write", "mutates outer object")

                if isinstance(node, IndexAssignment):
                    target = getattr(node, "target", None)
                    base_name = getattr(getattr(target, "array_expr", None), "name", None)
                    if base_name is None:
                        base_name = getattr(getattr(target, "array", None), "name", None)
                    if isinstance(base_name, str):
                        _record(base_name, "index-write", "mutates outer collection")

                # Recurse into compound statements
                for attr in ("body", "then_body", "else_body", "cases", "default_case"):
                    child = getattr(node, attr, None)
                    if child is None:
                        continue
                    if isinstance(child, list):
                        _walk(child)

        _walk(body)
        return mutated

    def check_switch_statement(self, statement: SwitchStatement, env: TypeEnvironment) -> Type:
        """Check a switch statement for multi-way branching.

        Verifies:
        - The switch expression has a valid type.
        - Each case value type is compatible with the switch expression type.
        - All case bodies are well-typed.
        - Flags unreachable duplicate cases (same constant value appearing twice).
        - Fallthrough statements are accepted without error (they are typed as ANY_TYPE
          via the FallthroughStatement dispatch in check_statement).

        Returns the union of all branch body types (including the default branch).
        """
        switch_type = self.check_expression(statement.expression, env)

        case_types: List[Type] = []
        seen_case_values: List[Any] = []

        for case in statement.cases:
            case_val_type = self.check_expression(case.value, env)

            # Warn when a case value type is clearly incompatible with the switch
            # expression type (e.g. switching on Integer but case is a String literal).
            if (
                not isinstance(switch_type, AnyType)
                and not isinstance(case_val_type, AnyType)
                and not case_val_type.is_compatible_with(switch_type)
                and not switch_type.is_compatible_with(case_val_type)
            ):
                self.errors.append(
                    f"Type error: case value type '{self._type_name(case_val_type)}' is "
                    f"incompatible with switch expression type '{self._type_name(switch_type)}'"
                )

            # Duplicate constant detection (best-effort for Literal nodes).
            literal_value = None
            if hasattr(case.value, 'value'):
                literal_value = case.value.value
            if literal_value is not None:
                if literal_value in seen_case_values:
                    self.errors.append(
                        f"Type error: duplicate case value {literal_value!r} in switch statement"
                    )
                else:
                    seen_case_values.append(literal_value)

            # Type-check the case body in a fresh child environment.
            case_env = TypeEnvironment(env)
            body_type = NULL_TYPE
            for stmt in case.body:
                body_type = self.check_statement(stmt, case_env)
            case_types.append(body_type)

        # Type-check the default case body (if present).
        if statement.default_case:
            default_env = TypeEnvironment(env)
            default_type = NULL_TYPE
            for stmt in statement.default_case:
                default_type = self.check_statement(stmt, default_env)
            case_types.append(default_type)

        if not case_types:
            return NULL_TYPE
        return UnionType(case_types)

    def _has_side_effects(self, node: Any) -> bool:
        """Return True if an AST node contains side-effecting sub-expressions.

        Side effects forbidden in contract conditions: variable assignments,
        index assignments, member assignments, and dereference assignments.
        Function calls are allowed because pure observation calls are common
        in contracts (e.g. require list_length(items) > 0).
        """
        if node is None:
            return False
        if isinstance(node, (VariableDeclaration, IndexAssignment,
                             MemberAssignment, DereferenceAssignment)):
            return True
        # Recurse into common compound node fields
        for attr in ('left', 'right', 'operand', 'value', 'condition',
                     'actual_expr', 'expected_expr'):
            child = getattr(node, attr, None)
            if child is not None and self._has_side_effects(child):
                return True
        # Recurse into list-valued fields (e.g. arguments)
        for attr in ('arguments', 'body', 'elements'):
            children = getattr(node, attr, None)
            if isinstance(children, list):
                for child in children:
                    if self._has_side_effects(child):
                        return True
        return False

    def _check_contract_condition(self, condition: Any, env: TypeEnvironment, kind: str) -> Type:
        """Validate a contract/assertion condition expression type."""
        if self._has_side_effects(condition):
            self.errors.append(
                f"Type error: {kind} condition must not contain assignments or mutations"
            )
        condition_type = self.check_statement(condition, env)
        if (not isinstance(condition_type, AnyType)
                and not condition_type.is_compatible_with(BOOLEAN_TYPE)):
            self.errors.append(
                f"Type error: {kind} condition must be a boolean, got '{self._type_name(condition_type)}'"
            )
        return condition_type

    def _check_contract_message(self, message_expr: Any, env: TypeEnvironment, kind: str) -> None:
        """Validate optional contract message expression type."""
        if message_expr is None:
            return
        msg_type = self.check_statement(message_expr, env)
        if not msg_type.is_compatible_with(STRING_TYPE) and not isinstance(msg_type, AnyType):
            self.errors.append(
                f"Type error: {kind} message must be a string, got '{self._type_name(msg_type)}'"
            )

    def check_require_statement(self, node: RequireStatement, env: TypeEnvironment) -> Type:
        self._check_contract_condition(node.condition, env, "Require")
        self._check_contract_message(getattr(node, 'message_expr', None), env, "Require")
        return BOOLEAN_TYPE

    def check_ensure_statement(self, node: EnsureStatement, env: TypeEnvironment) -> Type:
        self._check_contract_condition(node.condition, env, "Ensure")
        self._check_contract_message(getattr(node, 'message_expr', None), env, "Ensure")
        return BOOLEAN_TYPE

    def check_guarantee_statement(self, node: GuaranteeStatement, env: TypeEnvironment) -> Type:
        self._check_contract_condition(node.condition, env, "Guarantee")
        self._check_contract_message(getattr(node, 'message_expr', None), env, "Guarantee")
        return BOOLEAN_TYPE

    def check_invariant_statement(self, node: InvariantStatement, env: TypeEnvironment) -> Type:
        self._check_contract_condition(node.condition, env, "Invariant")
        self._check_contract_message(getattr(node, 'message_expr', None), env, "Invariant")
        return BOOLEAN_TYPE

    def check_comptime_expression(self, node: ComptimeExpression, env: TypeEnvironment) -> Type:
        """Type-check a comptime eval expression."""
        return self.check_statement(node.expr, env)

    def check_comptime_const(self, node: ComptimeConst, env: TypeEnvironment) -> Type:
        """Type-check and bind a comptime constant."""
        expr_type = self.check_statement(node.expr, env)
        env.define_variable(node.name, expr_type)
        return expr_type

    def check_comptime_assert(self, node: ComptimeAssert, env: TypeEnvironment) -> Type:
        """Type-check a comptime assertion."""
        condition_type = self.check_statement(node.condition, env)
        if not isinstance(condition_type, AnyType) and not condition_type.is_compatible_with(BOOLEAN_TYPE):
            self.errors.append(
                f"Type error: comptime assert condition must be boolean, got '{self._type_name(condition_type)}'"
            )

        self._check_contract_message(getattr(node, 'message_expr', None), env, "Comptime assert")
        return BOOLEAN_TYPE

    def check_macro_definition(self, node: MacroDefinition, env: TypeEnvironment) -> Type:
        """Register macro definitions for later expansion checks."""
        self.macro_definitions[node.name] = node
        return ANY_TYPE

    def check_macro_expansion(self, node: MacroExpansion, env: TypeEnvironment) -> Type:
        """Type-check macro expansion arguments and validate macro existence."""
        if node.name not in self.macro_definitions:
            self.errors.append(f"Undefined macro: {node.name}")
            return ANY_TYPE

        for arg_expr in (node.arguments or {}).values():
            self.check_statement(arg_expr, env)

        return ANY_TYPE

    def check_expect_statement(self, node: ExpectStatement, env: TypeEnvironment) -> Type:
        """Type-check an expect assertion statement."""
        actual_type = self.check_statement(node.actual_expr, env)
        matcher = getattr(node, 'matcher', '')
        expected_expr = getattr(node, 'expected_expr', None)

        if matcher in (
            'equal', 'greater_than', 'less_than',
            'greater_than_or_equal_to', 'less_than_or_equal_to'
        ):
            if expected_expr is not None:
                expected_type = self.check_statement(expected_expr, env)
                if matcher == 'equal':
                    if (not actual_type.is_compatible_with(expected_type)
                            and not expected_type.is_compatible_with(actual_type)
                            and not isinstance(actual_type, AnyType)
                            and not isinstance(expected_type, AnyType)):
                        self.errors.append(
                            f"Type error: expect equal compares incompatible types "
                            f"'{self._type_name(actual_type)}' and '{self._type_name(expected_type)}'"
                        )
                else:
                    for t in (actual_type, expected_type):
                        if (not isinstance(t, AnyType)
                                and not t.is_compatible_with(INTEGER_TYPE)
                                and not t.is_compatible_with(FLOAT_TYPE)):
                            self.errors.append(
                                f"Type error: expect comparison matcher requires numeric operands, got '{self._type_name(t)}'"
                            )
        elif matcher in ('be_true', 'be_false'):
            if (not isinstance(actual_type, AnyType)
                    and not actual_type.is_compatible_with(BOOLEAN_TYPE)):
                self.errors.append(
                    f"Type error: expect {matcher} requires boolean actual value, got '{self._type_name(actual_type)}'"
                )
        elif matcher == 'be_null':
            # Any type can be compared against null semantics.
            pass
        elif matcher == 'approximately_equal':
            if expected_expr is not None:
                expected_type = self.check_statement(expected_expr, env)
                for t in (actual_type, expected_type):
                    if (not isinstance(t, AnyType)
                            and not t.is_compatible_with(INTEGER_TYPE)
                            and not t.is_compatible_with(FLOAT_TYPE)):
                        self.errors.append(
                            f"Type error: approximately_equal requires numeric operands, got '{self._type_name(t)}'"
                        )
            tolerance_expr = getattr(node, 'tolerance_expr', None)
            if tolerance_expr is not None:
                tol_type = self.check_statement(tolerance_expr, env)
                if (not isinstance(tol_type, AnyType)
                        and not tol_type.is_compatible_with(INTEGER_TYPE)
                        and not tol_type.is_compatible_with(FLOAT_TYPE)):
                    self.errors.append(
                        f"Type error: expect approximately_equal tolerance must be numeric, got '{self._type_name(tol_type)}'"
                    )
        else:
            if expected_expr is not None:
                self.check_statement(expected_expr, env)
            tolerance_expr = getattr(node, 'tolerance_expr', None)
            if tolerance_expr is not None:
                self.check_statement(tolerance_expr, env)

        return BOOLEAN_TYPE
    
    def check_repeat_n_times_loop(self, loop: RepeatNTimesLoop, env: TypeEnvironment) -> Type:
        """Check a repeat-n-times loop."""
        # Check the count
        count_type = self.check_statement(loop.count, env)
        
        if not count_type.is_compatible_with(INTEGER_TYPE):
            self.errors.append(
                f"Type error: Repeat count must be an integer, got '{count_type}'"
            )
        
        # Check the body
        loop_env = TypeEnvironment(env)
        result_type = NULL_TYPE
        for stmt in loop.body:
            result_type = self.check_statement(stmt, loop_env)
        
        return result_type
    
    def check_repeat_while_loop(self, loop: RepeatWhileLoop, env: TypeEnvironment) -> Type:
        """Check a repeat-while loop (natural language while loop)."""
        # Check the condition
        condition_type = self.check_statement(loop.condition, env)
        
        # Condition should be boolean-compatible (any type can be truthy/falsy)
        # No strict type checking needed for conditions in NexusLang
        
        # Check the body
        loop_env = TypeEnvironment(env)
        result_type = NULL_TYPE
        for stmt in loop.body:
            result_type = self.check_statement(stmt, loop_env)
        
        # Check else body if present
        if loop.else_body:
            for stmt in loop.else_body:
                result_type = self.check_statement(stmt, loop_env)
        
        return result_type
    
    def check_return_statement(self, statement: ReturnStatement, env: TypeEnvironment) -> Type:
        """Check a return statement."""
        generator_env = env.get_generator_context()
        if generator_env is not None:
            if statement.value is not None:
                value_type = self.check_statement(statement.value, env)
                self.errors.append(
                    f"Type error: Generator function cannot return a value of type '{self._type_name(value_type)}'; use bare return or yield"
                )
            return NULL_TYPE

        if statement.value:
            value_type = self.check_statement(statement.value, env)
        else:
            value_type = NULL_TYPE
        
        # Check compatibility with the expected return type
        expected_return_type = env.get_return_type()
        if expected_return_type and not value_type.is_compatible_with(expected_return_type):
            self.errors.append(
                f"Type error: Return value of type '{value_type}' is not compatible with expected return type '{expected_return_type}'"
            )
        
        return value_type
    
    def _is_terminal_statement(self, stmt: Any) -> bool:
        """Return True if stmt unconditionally transfers control (raise, return)."""
        return isinstance(stmt, RaiseStatement) or isinstance(stmt, ReturnStatement)

    def _check_statements_for_unreachable(self, statements: list, env: TypeEnvironment) -> Type:
        """Type-check a statement list and emit an error for unreachable code after raise/return."""
        result_type = NULL_TYPE
        for i, stmt in enumerate(statements):
            result_type = self.check_statement(stmt, env)
            if self._is_terminal_statement(stmt) and i < len(statements) - 1:
                self.errors.append(
                    f"Line {getattr(statements[i + 1], 'line_number', '?')}: "
                    f"Unreachable code after '{stmt.__class__.__name__}'"
                )
                break
        return result_type

    def check_block(self, block: Block, env: TypeEnvironment) -> Type:
        """Check a block of statements."""
        block_env = TypeEnvironment(env)
        result_type = NULL_TYPE
        for stmt in block.statements:
            result_type = self.check_statement(stmt, block_env)
        return result_type
    
    def check_concurrent_block(self, block: ConcurrentBlock, env: TypeEnvironment) -> Type:
        """Check a concurrent block of statements."""
        block_env = TypeEnvironment(env)
        result_types = []
        
        for stmt in block.statements:
            result_types.append(self.check_statement(stmt, block_env))
        
        # The type of a concurrent block is a list of the result types
        return ListType(UnionType(result_types))
    
    def check_try_catch_block(self, block: TryCatchBlock, env: TypeEnvironment) -> Type:
        """Check a try-catch block."""
        try_env = TypeEnvironment(env)
        try_type = self.check_block(block.try_block, try_env)

        catch_env = TypeEnvironment(env)
        if block.exception_var:
            # Use declared exception type annotation when available, fall back to string
            exc_type: Type
            if block.exception_type:
                exc_type = ClassType(block.exception_type, {}, {})
            else:
                exc_type = STRING_TYPE
            catch_env.define_variable(block.exception_var, exc_type)

        catch_type = self.check_block(block.catch_block, catch_env)
        return UnionType([try_type, catch_type])
    
    def check_try_catch(self, node: TryCatch, env: TypeEnvironment) -> Type:
        """Check a TryCatch node (alternative try-catch AST form)."""
        def check_body(body, local_env):
            if body is None:
                return ANY_TYPE
            if isinstance(body, list):
                return self._check_statements_for_unreachable(body, local_env)
            return self.check_block(body, local_env)

        try_env = TypeEnvironment(env)
        try_type = check_body(node.try_block, try_env)
        catch_env = TypeEnvironment(env)
        if node.exception_var:
            exc_type: Type
            if node.exception_type:
                exc_type = ClassType(node.exception_type, {}, {})
            else:
                exc_type = STRING_TYPE
            catch_env.define_variable(node.exception_var, exc_type)
        catch_type = check_body(node.catch_block, catch_env)
        return UnionType([try_type, catch_type])

    def check_function_call(self, call: FunctionCall, env: TypeEnvironment) -> Type:
        """Check a function call with bidirectional type inference."""
        # Handle module.function calls (function name contains a dot)
        if isinstance(call.name, str) and '.' in call.name:
            # Module member access - type check arguments but return ANY_TYPE
            # since we don't track module member types yet
            arg_types = [self.check_statement(arg, env) for arg in call.arguments]
            return ANY_TYPE
        
        try:
            # Get the function type
            function_type = env.get_function_type(call.name)
            
            # Count total arguments (positional + named)
            total_args = len(call.arguments)
            if hasattr(call, 'named_arguments') and call.named_arguments:
                # Defensive: ensure named_arguments is actually a dict/list, not an int
                if isinstance(call.named_arguments, (dict, list)):
                    total_args += len(call.named_arguments)
            
            # Check argument count (skip for variadic functions)
            has_variadic = getattr(function_type, 'variadic', False)
            if not has_variadic:
                # Count required parameters (those without defaults)
                required_params = len(function_type.param_types)
                if hasattr(function_type, 'has_defaults') and function_type.has_defaults:
                    # If function has defaults, allow fewer arguments
                    required_params = function_type.min_params if hasattr(function_type, 'min_params') else 0
                
                # Allow arguments between min and max
                max_params = len(function_type.param_types)
                if total_args < required_params or total_args > max_params:
                    if required_params == max_params:
                        self.errors.append(
                            f"Type error: Function '{call.name}' expects {required_params} arguments, got {total_args}"
                        )
                    else:
                        self.errors.append(
                            f"Type error: Function '{call.name}' expects {required_params}-{max_params} arguments, got {total_args}"
                        )
                    return function_type.return_type
            else:
                # Variadic function - check we have at least min_params
                min_required = function_type.min_params if hasattr(function_type, 'min_params') else 0
                variadic_index = function_type.variadic_index if hasattr(function_type, 'variadic_index') else len(function_type.param_types) - 1
                
                # Need at least enough args for non-variadic parameters
                if total_args < min_required:
                    self.errors.append(
                        f"Type error: Function '{call.name}' expects at least {min_required} arguments, got {total_args}"
                    )
                    return function_type.return_type
            
            # Type check positional arguments
            arg_types = self.type_inference.infer_argument_types_from_function(
                function_type, call.arguments, env.variables
            )
            
            # Type check named arguments (just ensure expressions are valid, not param matching)
            # Note: We can't match named args to param types here because FunctionType
            # doesn't include parameter names, only types
            if hasattr(call, 'named_arguments') and call.named_arguments:
                for param_name, arg_expr in call.named_arguments.items():
                    # Just check that the expression is valid
                    self.check_statement(arg_expr, env)
            
            # Only check positional argument types against parameter types
            # Named arguments will be validated at runtime
            # For variadic functions, stop type checking before the variadic parameter
            if has_variadic and hasattr(function_type, 'variadic_index'):
                # Only check non-variadic parameters
                check_count = min(len(call.arguments), function_type.variadic_index)
                positional_param_types = function_type.param_types[:check_count]
                arg_types_to_check = arg_types[:check_count]
            else:
                positional_param_types = function_type.param_types[:len(call.arguments)]
                arg_types_to_check = arg_types
            
            for i, (arg_type, param_type) in enumerate(zip(arg_types_to_check, positional_param_types)):
                if param_type != ANY_TYPE and not arg_type.is_compatible_with(param_type):
                    self.errors.append(
                        f"Type error: Function '{call.name}' argument {i+1} expects type '{self._type_name(param_type)}', got '{self._type_name(arg_type)}'"
                    )
            
            return function_type.return_type
        except TypeCheckError:
            # If the function is not defined, assume it's a runtime-registered function (stdlib)
            # Check arguments without expected types to ensure they're valid expressions
            arg_types = [self.check_statement(arg, env) for arg in call.arguments]
            # Also check named arguments
            if hasattr(call, 'named_arguments') and call.named_arguments:
                for param_name, arg_expr in call.named_arguments.items():
                    self.check_statement(arg_expr, env)
            # Return ANY_TYPE to allow runtime resolution
            # NOTE: The function MUST exist at runtime or it will fail there
            return ANY_TYPE
    
    def _type_name(self, type_: Type) -> str:
        """Get a human-readable name for a type."""
        if hasattr(type_, 'name'):
            return type_.name
        return type_.__class__.__name__

    
    def check_binary_operation(self, operation: BinaryOperation, env: TypeEnvironment) -> Type:
        """Check a binary operation."""
        left_type = self.check_statement(operation.left, env)
        right_type = self.check_statement(operation.right, env)
        line_no = getattr(operation, 'line_number', None)
        if line_no is None:
            line_no = getattr(operation.operator, 'line', '?')
        
        # Get the operator (handle both Token objects and strings)
        if hasattr(operation.operator, 'lexeme'):
            op = operation.operator.lexeme
        else:
            op = str(operation.operator)

        # Normalize type-prefixed natural-language operators like "integer divided by"
        # The parser sometimes emits operators with a type qualifier prefix.
        _type_prefixes = ('integer ', 'float ', 'number ', 'string ', 'boolean ',
                          'text ', 'real ', 'int ')
        for _pfx in _type_prefixes:
            if op.startswith(_pfx):
                op = op[len(_pfx):]
                break

        # Arithmetic operators (both symbolic and natural language)
        arithmetic_ops = ['+', '-', '*', '/', '%', '//', 'plus', 'minus', 'times', 'divided by', 'modulo', 'power', 'to the power of', '**', 'integer divided by']
        if op in arithmetic_ops:
            if op in ['+', 'plus'] and (left_type == STRING_TYPE or right_type == STRING_TYPE):
                # String concatenation
                return STRING_TYPE
            
            # AnyType and FunctionType are allowed (dynamic/runtime calls — skip numeric check)
            _numeric = (INTEGER_TYPE, FLOAT_TYPE)
            _dynamic_types = (AnyType,)
            _skip_left = isinstance(left_type, _dynamic_types) or \
                         type(left_type).__name__ == 'FunctionType' or \
                         any(left_type.is_compatible_with(t) for t in _numeric)
            _skip_right = isinstance(right_type, _dynamic_types) or \
                          type(right_type).__name__ == 'FunctionType' or \
                          any(right_type.is_compatible_with(t) for t in _numeric)
            if not _skip_left:
                self.errors.append(
                    f"Line {line_no}: Type error: Left operand of '{op}' must be a number, got '{self._type_name(left_type)}'"
                )
            if not _skip_right:
                self.errors.append(
                    f"Line {line_no}: Type error: Right operand of '{op}' must be a number, got '{self._type_name(right_type)}'"
                )
            
            # Division and power always return float
            if op in ['/', 'divided by', 'power', 'to the power of', '**']:
                return FLOAT_TYPE

            # Floor division returns integer
            if op in ['//', 'integer divided by']:
                return INTEGER_TYPE
            
            # If either operand is a float, the result is a float
            if left_type == FLOAT_TYPE or right_type == FLOAT_TYPE:
                return FLOAT_TYPE
            
            return INTEGER_TYPE

        # Explicit string concatenation operator.
        if op == 'concatenate':
            if not left_type.is_compatible_with(STRING_TYPE):
                self.errors.append(
                    f"Line {line_no}: Type error: Left operand of '{op}' must be a string, got '{self._type_name(left_type)}'"
                )
            if not right_type.is_compatible_with(STRING_TYPE):
                self.errors.append(
                    f"Line {line_no}: Type error: Right operand of '{op}' must be a string, got '{self._type_name(right_type)}'"
                )
            return STRING_TYPE
        
        # Comparison operators (both symbolic and natural language)
        comparison_ops = ['==', '!=', '<', '>', '<=', '>=', 
                         'equals', 'equal to', 'is equal to', 
                         'not equal to', 'is not equal to', 
                         'greater than', 'is greater than', 
                         'less than', 'is less than',
                         'greater than or equal to', 'is greater than or equal to', 
                         'less than or equal to', 'is less than or equal to']
        if op in comparison_ops:
            # For equality operators, any types can be compared
            if op in ['==', '!=', 'equals', 'equal to', 'is equal to', 'not equal to', 'is not equal to']:
                return BOOLEAN_TYPE
            
            # For other comparison operators, operands must be comparable
            if not (
                (left_type.is_compatible_with(INTEGER_TYPE) or left_type.is_compatible_with(FLOAT_TYPE)) and
                (right_type.is_compatible_with(INTEGER_TYPE) or right_type.is_compatible_with(FLOAT_TYPE))
            ):
                self.errors.append(
                    f"Line {line_no}: Type error: Operands of '{op}' must be numbers, got '{self._type_name(left_type)}' and '{self._type_name(right_type)}'"
                )
            
            return BOOLEAN_TYPE
        
        # Logical operators
        logical_ops = ['and', 'or']
        if op in logical_ops:
            if not left_type.is_compatible_with(BOOLEAN_TYPE):
                self.errors.append(
                    f"Line {line_no}: Type error: Left operand of '{op}' must be a boolean, got '{self._type_name(left_type)}'"
                )
            
            if not right_type.is_compatible_with(BOOLEAN_TYPE):
                self.errors.append(
                    f"Line {line_no}: Type error: Right operand of '{op}' must be a boolean, got '{self._type_name(right_type)}'"
                )
            
            return BOOLEAN_TYPE
        
        # Bitwise operators (require integers)
        bitwise_ops = ['bitwise and', 'bitwise or', 'bitwise xor', '&', '|', '^', '<<', '>>', 'shift left', 'shift right']
        if op in bitwise_ops:
            if not left_type.is_compatible_with(INTEGER_TYPE):
                self.errors.append(
                    f"Line {line_no}: Type error: Left operand of '{op}' must be an integer, got '{self._type_name(left_type)}'"
                )
            
            if not right_type.is_compatible_with(INTEGER_TYPE):
                self.errors.append(
                    f"Line {line_no}: Type error: Right operand of '{op}' must be an integer, got '{self._type_name(right_type)}'"
                )
            
            return INTEGER_TYPE
        
        else:
            self.errors.append(f"Unsupported binary operator: {op}")
            return ANY_TYPE

    
    def check_unary_operation(self, operation: UnaryOperation, env: TypeEnvironment) -> Type:
        """Check a unary operation."""
        operand_type = self.check_statement(operation.operand, env)
        line_no = getattr(operation, 'line_number', None)
        if line_no is None:
            line_no = getattr(operation.operator, 'line', '?')
        
        # Get the operator (handle both Token objects and strings)
        if hasattr(operation.operator, 'lexeme'):
            op = operation.operator.lexeme
        else:
            op = str(operation.operator)
        
        if op in ['-', 'minus', 'negative']:
            if not (operand_type.is_compatible_with(INTEGER_TYPE) or operand_type.is_compatible_with(FLOAT_TYPE)):
                self.errors.append(
                    f"Line {line_no}: Type error: Operand of unary '{op}' must be a number, got '{self._type_name(operand_type)}'"
                )
            
            return operand_type
        
        elif op == 'not':
            if not operand_type.is_compatible_with(BOOLEAN_TYPE):
                self.errors.append(
                    f"Line {line_no}: Type error: Operand of 'not' must be a boolean, got '{self._type_name(operand_type)}'"
                )
            
            return BOOLEAN_TYPE
        
        elif op in ['~', 'bitwise not']:
            if not operand_type.is_compatible_with(INTEGER_TYPE):
                self.errors.append(
                    f"Line {line_no}: Type error: Operand of '{op}' must be an integer, got '{self._type_name(operand_type)}'"
                )
            
            return INTEGER_TYPE
        
        else:
            self.errors.append(f"Unsupported unary operator: {op}")
            return ANY_TYPE

    def check_memory_allocation(self, allocation: MemoryAllocation, env: TypeEnvironment) -> Type:
        """Type-check memory allocation statements."""
        var_type_name = allocation.var_type if isinstance(allocation.var_type, str) else str(allocation.var_type)
        allocated_type = get_type_by_name(var_type_name)

        if allocation.initial_value is not None:
            initial_type = self.check_statement(allocation.initial_value, env)
            if not initial_type.is_compatible_with(allocated_type):
                line_num = getattr(allocation, 'line_number', '?')
                self.errors.append(
                    f"Line {line_num}: Type error: Cannot initialize allocated '{allocation.identifier}' "
                    f"of type '{allocated_type}' with value of type '{initial_type}'"
                )

        env.define_variable(allocation.identifier, allocated_type)
        return allocated_type

    def check_memory_deallocation(self, deallocation: MemoryDeallocation, env: TypeEnvironment) -> Type:
        """Type-check memory deallocation statements."""
        try:
            env.get_variable_type(deallocation.identifier)
        except TypeCheckError:
            line_num = getattr(deallocation, 'line_number', '?')
            self.errors.append(
                f"Line {line_num}: Type error: Cannot free undefined memory identifier '{deallocation.identifier}'"
            )
        return ANY_TYPE
    
    def check_literal(self, literal: Literal, env: TypeEnvironment) -> Type:
        """Check a literal value."""
        return infer_type(literal.value)
    
    def check_identifier(self, identifier: Identifier, env: TypeEnvironment) -> Type:
        """Check an identifier (variable or function reference)."""
        try:
            # First try to get it as a variable
            return env.get_variable_type(identifier.name)
        except TypeCheckError:
            # If not a variable, try to get it as a function
            try:
                return env.get_function_type(identifier.name)
            except TypeCheckError as e:
                self.errors.append(str(e))
                return ANY_TYPE
    
    def check_list_expression(self, list_expr, env: TypeEnvironment) -> Type:
        """Check a list literal: [1, 2, 3]."""
        from ..typesystem.types import ListType
        
        if not list_expr.elements:
            # Empty list - return generic List type
            return ListType(ANY_TYPE)
        
        # Check all elements and infer common type
        element_types = [self.check_statement(elem, env) for elem in list_expr.elements]
        
        # Find common type (simplified - use first element's type)
        common_type = element_types[0]
        for elem_type in element_types[1:]:
            if not elem_type.is_compatible_with(common_type):
                # Types don't match - use ANY_TYPE
                common_type = ANY_TYPE
                break
        
        return ListType(common_type)
    
    def check_dict_expression(self, dict_expr, env: TypeEnvironment) -> Type:
        """Check a dictionary literal: {"key": "value"}."""
        from ..typesystem.types import DictionaryType
        
        if not dict_expr.entries:
            # Empty dict - return generic Dict type
            return DictionaryType(ANY_TYPE, ANY_TYPE)
        
        # Check all key-value pairs
        key_types = []
        value_types = []
        for key_expr, value_expr in dict_expr.entries:
            key_types.append(self.check_statement(key_expr, env))
            value_types.append(self.check_statement(value_expr, env))
        
        # Find common key and value types
        common_key_type = key_types[0]
        common_value_type = value_types[0]
        
        for key_type in key_types[1:]:
            if not key_type.is_compatible_with(common_key_type):
                common_key_type = ANY_TYPE
                break
        
        for value_type in value_types[1:]:
            if not value_type.is_compatible_with(common_value_type):
                common_value_type = ANY_TYPE
                break
        
        return DictionaryType(common_key_type, common_value_type)
    
    def check_list_comprehension(self, comp_expr, env: TypeEnvironment) -> Type:
        """Check a list comprehension: [x for x in range(10)]."""
        from ..typesystem.types import ListType
        
        # Create new scope for comprehension variable
        comp_env = TypeEnvironment(env)
        
        # Check iterable type
        iterable_type = self.check_statement(comp_expr.iterable, env)
        
        # Define loop variable in comprehension scope
        # For now, use ANY_TYPE for the loop variable
        comp_env.define_variable(comp_expr.target.name, ANY_TYPE)
        
        # Check condition if present
        if comp_expr.condition:
            self.check_statement(comp_expr.condition, comp_env)
        
        # Check element expression
        element_type = self.check_statement(comp_expr.element, comp_env)
        
        return ListType(element_type)
    
    def check_dict_comprehension(self, comp_expr, env: TypeEnvironment) -> Type:
        """Check a dict comprehension: {k: v for k, v in items}."""
        from ..typesystem.types import DictionaryType
        
        # Create new scope for comprehension variable
        comp_env = TypeEnvironment(env)
        
        # Check iterable type
        iterable_type = self.check_statement(comp_expr.iterable, env)
        
        # Define loop variable in comprehension scope
        comp_env.define_variable(comp_expr.target.name, ANY_TYPE)
        
        # Check condition if present
        if comp_expr.condition:
            self.check_statement(comp_expr.condition, comp_env)
        
        # Check key and value expressions
        key_type = self.check_statement(comp_expr.key, comp_env)
        value_type = self.check_statement(comp_expr.value, comp_env)
        
        return DictionaryType(key_type, value_type)

    def check_generator_expression(self, gen_expr, env: TypeEnvironment) -> Type:
        """Check a generator expression: (x for x in iterable if condition)."""
        comp_env = TypeEnvironment(env)

        iterable_type = self.check_statement(gen_expr.iterable, env)
        if isinstance(iterable_type, ListType):
            element_type = iterable_type.element_type
        elif isinstance(iterable_type, AnyType):
            element_type = ANY_TYPE
        else:
            element_type = ANY_TYPE
            self.errors.append(
                f"Type error: Generator iterable must be a list, got '{self._type_name(iterable_type)}'"
            )

        if hasattr(gen_expr, 'target') and isinstance(gen_expr.target, Identifier):
            comp_env.define_variable(gen_expr.target.name, element_type)

        if getattr(gen_expr, 'condition', None):
            cond_type = self.check_statement(gen_expr.condition, comp_env)
            if (not isinstance(cond_type, AnyType)
                    and not cond_type.is_compatible_with(BOOLEAN_TYPE)):
                self.errors.append(
                    f"Type error: Generator condition must be boolean, got '{self._type_name(cond_type)}'"
                )

        produced_type = self.check_statement(gen_expr.expr, comp_env)
        # Generator expressions currently lower to list-like semantics in the type checker.
        return ListType(produced_type)

    def check_yield_expression(self, yield_expr, env: TypeEnvironment) -> Type:
        """Check a yield expression with function-context compatibility checks."""
        yielded_type = NULL_TYPE
        if getattr(yield_expr, 'value', None) is not None:
            yielded_type = self.check_statement(yield_expr.value, env)

        generator_env = env.get_generator_context()
        if generator_env is None:
            self.errors.append("Type error: 'yield' can only be used inside a function")
            return yielded_type

        generator_env.yielded_types.append(yielded_type)

        expected_yield_type = generator_env.expected_yield_type
        if expected_yield_type is None:
            return yielded_type

        if not yielded_type.is_compatible_with(expected_yield_type):
            self.errors.append(
                f"Type error: Yield value of type '{self._type_name(yielded_type)}' is not compatible "
                f"with generator element type '{self._type_name(expected_yield_type)}'"
            )

        return yielded_type

    def _statement_contains_yield(self, node: Any) -> bool:
        """Return True when *node* contains a yield in the current function scope."""
        if node is None:
            return False

        node_type = type(node).__name__
        if node_type == 'YieldExpression':
            return True
        if node_type in {'FunctionDefinition', 'AsyncFunctionDefinition', 'LambdaExpression', 'ClassDefinition', 'MethodDefinition'}:
            return False

        if isinstance(node, list):
            return any(self._statement_contains_yield(item) for item in node)

        if not hasattr(node, '__dict__'):
            return False

        for value in vars(node).values():
            if isinstance(value, list):
                if any(self._statement_contains_yield(item) for item in value):
                    return True
            elif hasattr(value, '__dict__') and self._statement_contains_yield(value):
                return True

        return False

    def _infer_generator_yield_type(self, yielded_types: List[Type], function_name: str) -> Type:
        """Infer a stable generator element type or report incompatible yields."""
        if not yielded_types:
            return ANY_TYPE

        current_type = yielded_types[0]
        for next_type in yielded_types[1:]:
            if isinstance(current_type, AnyType) or isinstance(next_type, AnyType):
                current_type = ANY_TYPE
                continue

            common_type = current_type.get_common_supertype(next_type)
            compatible = current_type.is_compatible_with(next_type) or next_type.is_compatible_with(current_type)
            if isinstance(common_type, AnyType) and not compatible:
                self.errors.append(
                    f"Type error: Function '{function_name}' has incompatible yield types '{self._type_name(current_type)}' and '{self._type_name(next_type)}'"
                )
                return ANY_TYPE
            current_type = common_type

        return current_type

    def check_class_definition(self, definition: ClassDefinition, env: TypeEnvironment) -> Type:
        """Check the type of a class definition."""
        # Check if class is already defined in the type registry
        if definition.name in self.type_registry:
            class_type = self.type_registry[definition.name]
        else:
            # Extract properties and their types
            properties = {}
            for prop in definition.properties:
                if isinstance(prop, PropertyDeclaration):
                    prop_type = ANY_TYPE
                    if prop.type_annotation:
                        prop_type = get_type_by_name(prop.type_annotation)
                    properties[prop.name] = prop_type
            
            # Extract methods and their types
            methods = {}
            for method in definition.methods:
                if isinstance(method, MethodDefinition):
                    # Create a new environment for method scope
                    method_env = TypeEnvironment(env)
                    
                    # Process parameters
                    param_types = []
                    for param in method.parameters:
                        param_type = ANY_TYPE
                        if param.type_annotation:
                            param_type = get_type_by_name(param.type_annotation)
                        param_types.append(param_type)
                        method_env.define_variable(param.name, param_type)
                    
                    # Set return type
                    return_type = ANY_TYPE
                    if method.return_type:
                        return_type = get_type_by_name(method.return_type)
                    
                    method_env.set_return_type(return_type)
                    
                    # Create method type
                    method_type = FunctionType(param_types, return_type)
                    methods[method.name] = method_type
                    
                    # Check method body
                    for statement in method.body:
                        self.check_statement(statement, method_env)
            
            # Process inheritance
            parent_classes = None
            if hasattr(definition, 'parent_classes') and definition.parent_classes:
                parent_classes = definition.parent_classes
                
                # Check that parent classes exist
                for parent in parent_classes:
                    if parent not in self.type_registry:
                        self.errors.append(
                            f"Line {definition.line_number}: Type error: Parent class '{parent}' not defined"
                        )
            
            # Process interface implementations
            if hasattr(definition, 'implemented_interfaces') and definition.implemented_interfaces:
                for interface in definition.implemented_interfaces:
                    if interface not in self.type_registry:
                        self.errors.append(
                            f"Line {definition.line_number}: Type error: Interface '{interface}' not defined"
                        )
                    else:
                        # Check that all required methods are implemented
                        missing_methods = self.type_registry.check_interface_implementation(definition.name, interface)
                        if missing_methods:
                            methods_str = ", ".join(missing_methods)
                            self.errors.append(
                                f"Line {definition.line_number}: Type error: Class '{definition.name}' "
                                f"does not implement methods required by interface '{interface}': {methods_str}"
                            )

                    # If this interface is a known HKT trait, validate HKT method requirements.
                    if self.hkt_registry.get(interface) is not None:
                        self.check_hkt_implementation(
                            definition.name, interface, definition.line_number
                        )
            
            # Create or retrieve class type
            class_type = self.type_registry.create_class_type(
                definition.name,
                properties,
                methods,
                parent_classes
            )
        
        # Define the class in the environment
        env.define_variable(definition.name, class_type)
        
        return class_type

    def check_interface_definition(self, definition: InterfaceDefinition, env: TypeEnvironment) -> Type:
        """Check the type of an interface definition."""
        # Register the interface in the type registry with required methods
        required_methods = []
        
        for method in definition.methods:
            if isinstance(method, MethodDefinition):
                required_methods.append(method.name)
        
        self.type_registry.register_interface(definition.name, required_methods)
        
        # Process methods and their types
        methods = {}
        for method in definition.methods:
            if isinstance(method, MethodDefinition):
                # Create a new environment for method scope
                method_env = TypeEnvironment(env)
                
                # Process parameters
                param_types = []
                for param in method.parameters:
                    param_type = ANY_TYPE
                    if param.type_annotation:
                        param_type = get_type_by_name(param.type_annotation)
                    param_types.append(param_type)
                    method_env.define_variable(param.name, param_type)
                
                # Set return type
                return_type = ANY_TYPE
                if method.return_type:
                    return_type = get_type_by_name(method.return_type)
                
                method_env.set_return_type(return_type)
                
                # Create method type
                method_type = FunctionType(param_types, return_type)
                methods[method.name] = method_type
        
        # Create interface type (as a special kind of class type)
        interface_type = ClassType(definition.name, {}, methods)
        
        # Define the interface in the environment
        env.define_variable(definition.name, interface_type)
        
        return interface_type

    def check_abstract_class_definition(self, definition: AbstractClassDefinition) -> Type:
        """Check the type of an abstract class definition."""
        # Create a new type context for the class
        self.current_class = definition.name
        
        # Register the class type
        class_type = ClassType(
            name=definition.name,
            properties={},
            methods={},
            parent_classes=definition.parent_classes,
            is_abstract=True
        )
        self.type_registry[definition.name] = class_type
        
        # Check type parameters
        if definition.type_parameters:
            generic_context = self.generic_registry.create_context(definition.name)
            for param in definition.type_parameters:
                generic_context.add_type_parameter(param)
        
        # Check properties
        for prop in definition.properties:
            self.check_property_definition(prop)
        
        # Check methods
        abstract_methods = set()
        for method in definition.methods:
            if isinstance(method, AbstractMethodDefinition):
                abstract_methods.add(method.name)
            self.check_method_definition(method)
        
        # Store abstract methods
        self.abstract_methods[definition.name] = abstract_methods
        
        # Check interface implementation
        for interface in definition.implemented_interfaces:
            self.check_interface_implementation(definition.name, interface)
        
        self.current_class = None
        
        return class_type

    def check_trait_definition(self, definition: TraitDefinition) -> Type:
        """Check the type of a trait definition."""
        # Create a new type context for the trait
        self.current_trait = definition.name
        
        # Register the trait type
        trait_type = ClassType(
            name=definition.name,
            properties={},
            methods={},
            parent_classes=[],
            is_trait=True
        )
        self.type_registry[definition.name] = trait_type
        
        # Check type parameters
        if definition.type_parameters:
            generic_context = self.generic_registry.create_context(definition.name)
            for param in definition.type_parameters:
                generic_context.add_type_parameter(param)
        
        # Check methods
        trait_methods = set()
        for method in definition.methods:
            if isinstance(method, AbstractMethodDefinition):
                trait_methods.add(method.name)
            self.check_method_definition(method)
        
        # Store trait methods
        self.trait_methods[definition.name] = trait_methods
        
        self.current_trait = None
        
        return trait_type

    def check_type_alias_definition(self, definition: TypeAliasDefinition) -> Type:
        """Check the type of a type alias definition."""
        # Resolve the target type
        target_type = self.resolve_type(definition.target_type)
        
        # Register the type alias
        self.type_registry[definition.name] = target_type
        
        return target_type

    def check_interface_implementation(self, class_name: str, interface_name: str) -> None:
        """Check if a class properly implements an interface."""
        if interface_name not in self.type_registry:
            raise TypeError(f"Interface {interface_name} not found")
        
        interface_type = self.type_registry[interface_name]
        class_type = self.type_registry[class_name]
        
        # Check if all interface methods are implemented
        for method_name, method_type in interface_type.methods.items():
            if method_name not in class_type.methods:
                raise TypeError(f"Class {class_name} does not implement method {method_name} from interface {interface_name}")
            
            # Check method signature compatibility
            class_method = class_type.methods[method_name]
            if not self.types_compatible(class_method, method_type):
                raise TypeError(f"Method {method_name} in class {class_name} is not compatible with interface {interface_name}")
    
    def check_abstract_method_implementation(self, class_name: str) -> None:
        """Check if all abstract methods are implemented."""
        if class_name not in self.abstract_methods:
            return
        
        class_type = self.type_registry[class_name]
        abstract_methods = self.abstract_methods[class_name]
        
        for method_name in abstract_methods:
            if method_name not in class_type.methods:
                raise TypeError(f"Class {class_name} does not implement abstract method {method_name}")
    
    def check_trait_method_implementation(self, class_name: str, trait_name: str) -> None:
        """Check if all trait methods are implemented."""
        if trait_name not in self.trait_methods:
            return
        
        class_type = self.type_registry[class_name]
        trait_methods = self.trait_methods[trait_name]
        
        for method_name in trait_methods:
            if method_name not in class_type.methods:
                raise TypeError(f"Class {class_name} does not implement trait method {method_name} from trait {trait_name}")

    # ------------------------------------------------------------------
    # Higher-Kinded Type (HKT) constraint checking
    # ------------------------------------------------------------------

    def check_hkt_implementation(
        self,
        class_name: str,
        hkt_name: str,
        line_number: Optional[int] = None,
    ) -> None:
        """Verify that *class_name* supplies every method required by the HKT trait *hkt_name*.

        When a class declares e.g. ``implements Functor``, this method checks that
        the class provides a ``map`` method.  Errors are appended to ``self.errors``
        rather than raised so the full program can be type-checked in one pass.

        Also recursively checks parent HKTs (e.g. Monad extends Applicative extends
        Functor), providing a single clear error per missing method.
        """
        hkt = self.hkt_registry.get(hkt_name)
        if hkt is None:
            # Not a known HKT — nothing to check.
            return

        prefix = f"Line {line_number}: " if line_number is not None else ""

        # Collect expected method names from this HKT and its parents.
        expected_methods: Dict[str, str] = {}  # method_name -> source HKT name
        queue = [hkt]
        visited: set = set()
        while queue:
            current = queue.pop(0)
            if current.name in visited:
                continue
            visited.add(current.name)
            for method_name in current.methods:
                if method_name not in expected_methods:
                    expected_methods[method_name] = current.name
            queue.extend(current.parent_hkt)

        # Look up the class type.
        class_type = None
        if hasattr(self.type_registry, 'get_class_type'):
            class_type = self.type_registry.get_class_type(class_name)
        elif class_name in self.type_registry:
            class_type = self.type_registry[class_name]

        if class_type is None:
            # Class not registered yet — skip silently.
            return

        class_methods = getattr(class_type, 'methods', {}) or {}

        for method_name, source_hkt in expected_methods.items():
            if method_name not in class_methods:
                self.errors.append(
                    f"{prefix}Type error: Class '{class_name}' implements "
                    f"HKT trait '{hkt_name}' but is missing required method "
                    f"'{method_name}' (required by '{source_hkt}')"
                )

        # Register the implementation so other code can query it later.
        self.hkt_registry.register_implementation(class_name, hkt_name)

    def check_hkt_constraint(self, constructor_name: str, hkt_name: str) -> bool:
        """Return True if *constructor_name* is known to implement *hkt_name*.

        This queries the global HKT registry.  Returns False (without emitting an
        error) if the HKT is unknown, leaving the caller to decide whether to raise.
        """
        if self.hkt_registry.get(hkt_name) is None:
            return False
        return self.hkt_registry.implements(constructor_name, hkt_name)

    def get_implemented_hkts(self, type_name: str) -> List[str]:
        """Return the list of HKT trait names that *type_name* has been registered to implement."""
        return self.hkt_registry.get_implementations(type_name)

    def resolve_type(self, type_name: str) -> Type:
        """Resolve a type name to its actual type."""
        if type_name in self.type_registry:
            return self.type_registry[type_name]
        
        # Handle generic types
        if '<' in type_name and type_name.endswith('>'):
            base_name, param_str = type_name.split('<', 1)
            param_str = param_str[:-1]  # Remove the closing '>'
            
            if base_name in self.type_registry:
                base_type = self.type_registry[base_name]
                if isinstance(base_type, GenericType):
                    type_args = [self.resolve_type(arg.strip()) for arg in param_str.split(',')]
                    return self.generic_registry.instantiate_type(base_type, type_args)
        
        raise TypeError(f"Type {type_name} not found")
    
    def types_compatible(self, type1: Type, type2: Type) -> bool:
        """Check if two types are compatible."""
        # Handle generic types
        if isinstance(type1, GenericType) and isinstance(type2, GenericType):
            if type1.name != type2.name:
                return False
            
            # Check type parameter compatibility
            for param1, param2 in zip(type1.type_parameters, type2.type_parameters):
                if not self.types_compatible(param1, param2):
                    return False
            
            return self.types_compatible(type1.base_type, type2.base_type)
        
        # Handle class types
        if isinstance(type1, ClassType) and isinstance(type2, ClassType):
            # Check inheritance
            if type1.name in type2.parent_classes:
                return True
            
        # Default: use Type's is_compatible_with method
        return type1.is_compatible_with(type2)
    
    def check_print_statement(self, statement: PrintStatement, env: TypeEnvironment) -> Type:
        """Check a print statement. Print can handle any type."""
        # Type check the expression being printed
        if hasattr(statement, 'value') and statement.value:
            self.check_statement(statement.value, env)
        return ANY_TYPE
    
    def check_lambda_expression(self, lambda_expr, env: TypeEnvironment) -> FunctionType:
        """
        Check a lambda expression and infer its type.
        
        Uses type inference to determine parameter and return types.
        Supports bidirectional type inference when expected type is available.
        """
        # Use type inference to get the lambda's function type
        # The type inference engine handles bidirectional inference
        lambda_type = self.type_inference.infer_lambda_types(
            lambda_expr, 
            None,  # No expected type (context-free checking)
            env.variables
        )
        
        return lambda_type
    
    def check_type_cast(self, expr: TypeCastExpression, env: TypeEnvironment) -> Type:
        """Check a type cast expression and return the target type."""
        # Check the expression being cast
        source_type = self.check_statement(expr.expression, env)
        
        # Convert target_type to Type object if it's a string
        if isinstance(expr.target_type, str):
            type_name = expr.target_type.lower()
            if type_name == "integer" or type_name == "int":
                return INTEGER_TYPE
            elif type_name == "float":
                return FLOAT_TYPE
            elif type_name == "string":
                return STRING_TYPE
            elif type_name == "boolean" or type_name == "bool":
                return BOOLEAN_TYPE
            else:
                # For other types, return ANY_TYPE
                return ANY_TYPE
        
        # Return target type if already a Type object
        return expr.target_type if isinstance(expr.target_type, Type) else ANY_TYPE
    
    def check_member_access(self, expr: Any, env: TypeEnvironment) -> Type:
        """
        Check a member access expression: object.member
        
        Uses type inference engine for proper type propagation through
        member access chains (e.g., obj.method().property.another_method()).
        """
        # Use type inference engine for comprehensive member access handling
        inferred_type = self.type_inference.infer_member_access_type(expr, env.variables)
        
        # If inference succeeded, return the inferred type
        if inferred_type != ANY_TYPE:
            # When this is a method call (has arguments) and inference returned
            # FunctionType, we should return the function's return type, not the
            # FunctionType itself. This handles native dict/list method calls such
            # as d.has("key"), d.size(), d.get("k") used directly in if-conditions.
            if isinstance(inferred_type, FunctionType) and getattr(expr, 'is_method_call', False):
                rt = getattr(inferred_type, 'return_type', None)
                return rt if rt is not None else ANY_TYPE
            return inferred_type
        
        # Fallback: Check the object expression directly
        obj_type = self.check_statement(expr.object_expr, env)
        
        # If object type is a class type, check if member exists
        if isinstance(obj_type, ClassType):
            member_name = expr.member_name
            
            # Check properties
            if member_name in obj_type.properties:
                return obj_type.properties[member_name]
            
            # Check methods
            if member_name in obj_type.methods:
                method_type = obj_type.methods[member_name]
                
                # If this is a method call, return the return type
                if hasattr(expr, 'is_method_call') and expr.is_method_call:
                    if isinstance(method_type, FunctionType):
                        return method_type.return_type
                return method_type
            
            # Member not found - return ANY for error recovery
            self.errors.append(
                f"Member '{member_name}' not found in class '{obj_type.name}'"
            )
            return ANY_TYPE
        
        # For other types, return ANY_TYPE
        return ANY_TYPE
    
    def check_index_expression(self, expr: Any, env: TypeEnvironment) -> Type:
        """
        Check an index expression: array[index] or dict[key]
        
        Validates that:
        - The indexed object is a collection type (list, dict, string)
        - The index type matches the expected type (integer for lists, key type for dicts)
        """
        # Use type inference engine for proper index type checking
        inferred_type = self.type_inference.infer_index_expression_type(expr, env.variables)
        
        # Check the array/collection type
        array_type = self.check_statement(expr.array_expr, env)
        
        # Check the index type
        index_type = self.check_statement(expr.index_expr, env)
        
        # Validate based on collection type
        if isinstance(array_type, ListType):
            # List indexing requires integer index
            if not index_type.is_compatible_with(INTEGER_TYPE) and index_type != ANY_TYPE:
                self.errors.append(
                    f"List index must be Integer, got {index_type}"
                )
            return array_type.element_type
        
        elif isinstance(array_type, DictionaryType):
            # Dictionary access requires key type
            if not index_type.is_compatible_with(array_type.key_type) and index_type != ANY_TYPE:
                self.errors.append(
                    f"Dictionary key must be {array_type.key_type}, got {index_type}"
                )
            return array_type.value_type
        
        elif array_type == STRING_TYPE:
            # String indexing requires integer index, returns string (character)
            if not index_type.is_compatible_with(INTEGER_TYPE) and index_type != ANY_TYPE:
                self.errors.append(
                    f"String index must be Integer, got {index_type}"
                )
            return STRING_TYPE
        
        elif array_type != ANY_TYPE:
            # Type error - trying to index non-indexable type
            self.errors.append(
                f"Cannot index into type {array_type}"
            )
        
        return inferred_type

    def check_generic_type_instantiation(self, expr: Any, env: TypeEnvironment) -> Type:
        """
        Check a generic type instantiation: create list, create list of Integer
        
        Returns the appropriate generic type based on the instantiation:
        - create list of Integer -> ListType(INTEGER_TYPE)
        - create dict of String to Integer -> DictionaryType(STRING_TYPE, INTEGER_TYPE)
        - create list (no type args) -> ListType(ANY_TYPE) with type inference
        """
        from nexuslang.typesystem.types import ListType, DictionaryType, SetType, TupleType
        
        generic_name = expr.generic_name.lower()
        
        # Handle list types
        if generic_name == "list":
            if expr.type_args and len(expr.type_args) > 0:
                # Explicit type: create list of Integer
                element_type = get_type_by_name(expr.type_args[0])
                return ListType(element_type)
            else:
                # Type inference: create list (will infer from usage)
                # Check if there's an initial value to infer from
                if expr.initial_value:
                    # Infer element type from initial value
                    init_type = self.check_statement(expr.initial_value, env)
                    if isinstance(init_type, ListType):
                        return init_type  # Already a list with inferred type
                    else:
                        return ListType(init_type)  # Single element
                return ListType(ANY_TYPE)  # Will infer later
        
        # Handle dict/dictionary/map types
        elif generic_name in ("dict", "dictionary", "map"):
            if expr.type_args and len(expr.type_args) >= 2:
                # Explicit types: create dict of String to Integer
                key_type = get_type_by_name(expr.type_args[0])
                value_type = get_type_by_name(expr.type_args[1])
                return DictionaryType(key_type, value_type)
            else:
                # Type inference: create dict
                if expr.initial_value:
                    init_type = self.check_statement(expr.initial_value, env)
                    if isinstance(init_type, DictionaryType):
                        return init_type
                return DictionaryType(ANY_TYPE, ANY_TYPE)
        
        # Handle set types
        elif generic_name == "set":
            if expr.type_args and len(expr.type_args) > 0:
                element_type = get_type_by_name(expr.type_args[0])
                return SetType(element_type)
            else:
                if expr.initial_value:
                    init_type = self.check_statement(expr.initial_value, env)
                    if isinstance(init_type, SetType):
                        return init_type
                    else:
                        return SetType(init_type)
                return SetType(ANY_TYPE)
        
        # Handle tuple types
        elif generic_name == "tuple":
            if expr.type_args and len(expr.type_args) > 0:
                element_types = [get_type_by_name(t) for t in expr.type_args]
                return TupleType(element_types)
            else:
                if expr.initial_value:
                    # Infer from initial values
                    init_type = self.check_statement(expr.initial_value, env)
                    if isinstance(init_type, TupleType):
                        return init_type
                return TupleType([ANY_TYPE])
        
        # Handle queue and stack (simplified as List)
        elif generic_name in ("queue", "stack"):
            if expr.type_args and len(expr.type_args) > 0:
                element_type = get_type_by_name(expr.type_args[0])
                return ListType(element_type)
            else:
                return ListType(ANY_TYPE)
        
        # Unknown generic type
        else:
            self.errors.append(f"Unknown generic type: {generic_name}")
            return ANY_TYPE

    def check_generic_constraints(self, type_params: list, type_args: list, 
                                   constraints: dict, context: str = "") -> bool:
        """
        Check if type arguments satisfy generic type parameter constraints.
        
        Args:
            type_params: List of type parameter names (e.g., ['T', 'R'])
            type_args: List of concrete types being substituted
            constraints: Dict mapping parameter name to list of trait names
            context: Context string for error messages (e.g., "function sum")
            
        Returns:
            True if all constraints are satisfied, False otherwise
        """
        from nexuslang.typesystem.types import TraitType, COMPARABLE_TRAIT, EQUATABLE_TRAIT, PRINTABLE_TRAIT
        from nexuslang.typesystem.generic_types import GenericTypeConstraint
        
        if len(type_params) != len(type_args):
            self.errors.append(
                f"Type error in {context}: Expected {len(type_params)} type arguments, got {len(type_args)}"
            )
            return False
        
        # Map of predefined traits
        TRAIT_MAP = {
            'Comparable': COMPARABLE_TRAIT,
            'Equatable': EQUATABLE_TRAIT,
            'Printable': PRINTABLE_TRAIT,
            # Add more traits as they're defined
        }
        
        all_satisfied = True
        
        for param_name, type_arg in zip(type_params, type_args):
            # Check if this parameter has constraints
            if param_name not in constraints:
                continue
            
            trait_names = constraints[param_name]
            
            for trait_name in trait_names:
                # First check: is this a Higher-Kinded Type constraint?
                if self.hkt_registry.get(trait_name) is not None:
                    constructor_name = self._type_name(type_arg)
                    if not self.check_hkt_constraint(constructor_name, trait_name):
                        self.errors.append(
                            f"Type error in {context}: Type constructor '{constructor_name}' "
                            f"does not implement HKT trait '{trait_name}'. "
                            f"Register it via HKTRegistry.register_implementation() or ensure "
                            f"the class declares and implements all required methods."
                        )
                        all_satisfied = False
                    continue

                # Get the trait type from the standard trait map
                trait = TRAIT_MAP.get(trait_name)
                
                if trait is None:
                    self.errors.append(
                        f"Type error in {context}: Unknown trait '{trait_name}'"
                    )
                    all_satisfied = False
                    continue
                
                # Check if the type argument implements the trait
                if not trait.is_implemented_by(type_arg):
                    self.errors.append(
                        f"Type error in {context}: Type '{self._type_name(type_arg)}' does not implement trait '{trait_name}'"
                    )
                    all_satisfied = False
        
        return all_satisfied
    
    def validate_generic_function_call(self, func_name: str, func_def, type_args: list) -> bool:
        """
        Validate that a generic function call satisfies trait bounds.
        
        Args:
            func_name: Name of the function being called
            func_def: FunctionDefinition AST node
            type_args: List of concrete type arguments
            
        Returns:
            True if valid, False otherwise
        """
        if not hasattr(func_def, 'type_parameters') or not func_def.type_parameters:
            return True  # Not a generic function
        
        # Get constraints from function definition
        constraints = {}
        if hasattr(func_def, 'type_constraints') and isinstance(func_def.type_constraints, dict):
            constraints = func_def.type_constraints
        
        return self.check_generic_constraints(
            func_def.type_parameters,
            type_args,
            constraints,
            context=f"function '{func_name}'"
        )
 