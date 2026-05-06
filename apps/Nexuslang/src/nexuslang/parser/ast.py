"""
Abstract Syntax Tree (AST) for the NexusLang (NexusLang).
This module defines the nodes that represent the structure of programs written in our language.
"""

class ASTNode:
    """Base class for all AST nodes."""
    def __init__(self, node_type, line_number=None):
        self.node_type = node_type
        self.line_number = line_number
        
    def __str__(self):
        return f"{self.node_type} (line {self.line_number})"

class Program:
    """The root node of the AST."""
    def __init__(self, statements):
        self.statements = statements if statements else []
        
    def __str__(self):
        return f"Program with {len(self.statements)} statements"

class VariableDeclaration:
    """A variable declaration node."""
    def __init__(self, name, value, type_annotation=None, allocator_hint=None, line_number=None):
        self.name = name
        self.value = value
        self.type_annotation = type_annotation
        self.allocator_hint = allocator_hint  # Optional allocator name (str)
        self.line_number = line_number  # Source line (1-indexed, for LSP/debugger)

    def __str__(self):
        return f"Variable {self.name} = {self.value}"

class IndexAssignment:
    """An index assignment node: set array[index] to value."""
    def __init__(self, target, value):
        self.target = target  # IndexExpression node (array[index])
        self.value = value    # Expression to assign
        
    def __str__(self):
        return f"IndexAssignment {self.target} = {self.value}"

class MemberAssignment:
    """A member assignment node: set object.field to value."""
    def __init__(self, target, value):
        self.target = target  # MemberAccess node (object.field)
        self.value = value    # Expression to assign
        
    def __str__(self):
        return f"MemberAssignment {self.target} = {self.value}"

class DereferenceAssignment:
    """A pointer dereference assignment node: set (value at ptr) to value."""
    def __init__(self, target, value):
        self.target = target  # DereferenceExpression node (value at ptr)
        self.value = value    # Expression to assign
    
    def __str__(self):
        return f"DereferenceAssignment (value at {self.target}) = {self.value}"

class Literal:
    """A literal value node."""
    def __init__(self, type, value):
        self.type = type
        self.value = value
        
    def __str__(self):
        return str(self.value)

class FunctionDefinition(ASTNode):
    """Represents a function definition."""
    def __init__(self, name, parameters, body=None, return_type=None, type_parameters=None, type_constraints=None, variadic=False, is_exported=False, decorators=None, type_param_kinds=None, line_number=None):
        super().__init__("function_definition", line_number)
        self.name = name
        self.parameters = parameters or []
        self.body = body or []
        self.return_type = return_type  # Optional return type annotation
        self.type_parameters = type_parameters or []  # Generic type parameters like <T, R>
        self.type_constraints = type_constraints or []  # Constraints like "where T is Comparable"
        self.variadic = variadic  # True if function accepts variable arguments (...)
        self.is_exported = is_exported
        self.decorators = decorators or []  # List of decorators applied to this function
        self.type_param_kinds = type_param_kinds or {}  # Dict: param_name -> KindAnnotation (HKT)
class Decorator(ASTNode):
    """Represents a decorator (@decorator_name or @decorator_name with args)."""
    def __init__(self, name, arguments=None, line_number=None):
        super().__init__("decorator", line_number)
        self.name = name  # Decorator name (e.g., "memoize", "trace")
        self.arguments = arguments or {}  # Dict of argument name -> value

class MacroDefinition(ASTNode):
    """Represents a macro definition."""
    def __init__(self, name, parameters, body, line_number=None):
        super().__init__("macro_definition", line_number)
        self.name = name
        self.parameters = parameters or []  # List of parameter names
        self.body = body or []  # List of statements in macro body

class MacroExpansion(ASTNode):
    """Represents a macro expansion/invocation."""
    def __init__(self, name, arguments, line_number=None):
        super().__init__("macro_expansion", line_number)
        self.name = name
        self.arguments = arguments or {}  # Dict of parameter name -> value expression

class ComptimeExpression(ASTNode):
    """comptime eval EXPR -- evaluates the expression at load time."""
    def __init__(self, expr, line_number=None):
        super().__init__("comptime_expression", line_number)
        self.expr = expr

class ComptimeConst(ASTNode):
    """comptime const NAME is EXPR -- defines an immutable compile-time constant."""
    def __init__(self, name, expr, line_number=None):
        super().__init__("comptime_const", line_number)
        self.name = name
        self.expr = expr


class AttributeDeclaration(ASTNode):
    """attribute Name [with prop1 as Type1, prop2 as Type2] -- declares a custom attribute type."""
    def __init__(self, name, properties=None, line_number=None):
        super().__init__("attribute_declaration", line_number)
        self.name = name
        self.properties = properties or []  # list of (prop_name, type_str) tuples

class ComptimeAssert(ASTNode):
    """comptime assert COND [message MSG] -- assertion checked at load time."""
    def __init__(self, condition, message_expr=None, line_number=None):
        super().__init__("comptime_assert", line_number)
        self.condition = condition
        self.message_expr = message_expr

class TypeAlias(ASTNode):
    """A type alias definition node."""
    def __init__(self, name, target_type, type_parameters=None, constraints=None, line_number=None):
        super().__init__("type_alias", line_number)
        self.name = name
        self.type_parameters = type_parameters or []  # List of type parameter names
        self.target_type = target_type  # The type being aliased
        self.constraints = constraints or {}  # Dict of parameter -> constraint
    
    def __str__(self):
        return f"TypeAlias {self.name}"

class RcType(ASTNode):
    """Reference counted smart pointer type.
    
    Example:
        Rc of Integer
        Rc of List of String
        Rc of MyStruct
    """
    def __init__(self, inner_type, line_number=None):
        super().__init__("rc_type", line_number)
        self.inner_type = inner_type  # The type being wrapped in Rc
    
    def __str__(self):
        return f"Rc<{self.inner_type}>"

class WeakType(ASTNode):
    """Weak reference type for breaking reference cycles.
    
    Example:
        Weak of Node
    """
    def __init__(self, inner_type, line_number=None):
        super().__init__("weak_type", line_number)
        self.inner_type = inner_type
    
    def __str__(self):
        return f"Weak<{self.inner_type}>"

class ArcType(ASTNode):
    """Atomic reference counted pointer (thread-safe).
    
    Example:
        Arc of Integer
    """
    def __init__(self, inner_type, line_number=None):
        super().__init__("arc_type", line_number)
        self.inner_type = inner_type
    
    def __str__(self):
        return f"Arc<{self.inner_type}>"

class RcCreation(ASTNode):
    """Node for creating Rc<T>, Weak<T>, or Arc<T> values.
    
    Syntax: Rc of Integer with 42
            Weak of String with "hello"
            Arc of Float with 3.14
    """
    def __init__(self, rc_kind, inner_type, value, line_number=None):
        super().__init__("rc_creation", line_number)
        self.rc_kind = rc_kind  # 'rc', 'weak', or 'arc'
        self.inner_type = inner_type  # Inner type as string
        self.value = value  # Initial value expression
    
    def __str__(self):
        return f"{self.rc_kind.capitalize()} of {self.inner_type} with {self.value}"

class AsyncFunctionDefinition(ASTNode):
    """Represents an async function definition.
    
    Example:
        async function fetch_data with url as String returns Dictionary
            set response to await http_get(url)
            return response
        end
    """
    def __init__(self, name, parameters, body=None, return_type=None, type_parameters=None, line_number=None):
        super().__init__("async_function_definition", line_number)
        self.name = name
        self.parameters = parameters or []
        self.body = body or []
        self.return_type = return_type
        self.type_parameters = type_parameters or []

class AwaitExpression(ASTNode):
    """Represents an await expression.
    
    Example:
        set result to await async_function()
    """
    def __init__(self, expression, line_number=None):
        super().__init__("await_expression", line_number)
        self.expression = expression

class Parameter(ASTNode):
    """Represents a function parameter."""
    def __init__(self, name, type_annotation=None, size_param=None, default_value=None, is_variadic=False, keyword_only=False, line_number=None):
        super().__init__("parameter", line_number)
        self.name = name
        self.type_annotation = type_annotation  # Optional type annotation
        self.size_param = size_param  # Optional size parameter name for array bounds
        self.default_value = default_value  # Optional default value expression
        self.is_variadic = is_variadic  # True if this is a *args style parameter
        self.keyword_only = keyword_only  # True if parameter must be passed by name (comes after * separator)

class IfStatement(ASTNode):
    """Represents an if statement."""
    def __init__(self, condition, then_block, else_block=None, line_number=None):
        super().__init__("if_statement", line_number)
        self.condition = condition
        self.then_block = then_block
        self.else_block = else_block

class WhileLoop(ASTNode):
    """Represents a while loop."""
    def __init__(self, condition, body, line_number=None, else_body=None, label=None):
        super().__init__("while_loop", line_number)
        self.condition = condition
        self.body = body
        self.else_body = else_body  # Optional else block (executed if loop completes without break)
        self.label = label  # Optional label for labeled break/continue

class ForLoop(ASTNode):
    """Represents a for loop (both for-each and range-based).
    
    For-each loop: for each item in collection
        - iterator: loop variable name
        - iterable: collection expression
        - body: list of statements
        - start, end, step: None
    
    Range-based loop: for i from start to end [by step]
        - iterator: loop variable name  
        - start: start expression
        - end: end expression
        - step: step expression (default 1)
        - body: list of statements
        - iterable: None
    """
    def __init__(self, iterator, iterable=None, body=None, start=None, end=None, step=None, line_number=None, else_body=None, label=None, index_var=None):
        super().__init__("for_loop", line_number)
        self.iterator = iterator
        self.iterable = iterable  # For for-each loops
        self.start = start  # For range loops
        self.end = end  # For range loops
        self.step = step  # For range loops
        self.body = body or []
        self.else_body = else_body  # Optional else block (executed if loop completes without break)
        self.label = label  # Optional label for labeled break/continue
        self.index_var = index_var  # Optional index variable for enumerate-style iteration

class MatchExpression(ASTNode):
    """Represents a pattern matching expression.
    
    Example:
        match result with
            case Ok value
                print text value
            case Error message
                print text message
            case _
                print text "Unknown"
    """
    def __init__(self, expression, cases, line_number=None):
        super().__init__("match_expression", line_number)
        self.expression = expression  # Expression to match against
        self.cases = cases or []  # List of MatchCase nodes

class MatchCase(ASTNode):
    """Represents a single case in a match expression.
    
    Patterns can be:
    - Literal: case 42
    - Identifier (binding): case value
    - Variant: case Ok value (for Result, Option types)
    - Wildcard: case _
    - Guard: case value if value > 0
    """
    def __init__(self, pattern, body, guard=None, line_number=None):
        super().__init__("match_case", line_number)
        self.pattern = pattern  # Pattern node
        self.body = body  # List of statements to execute
        self.guard = guard  # Optional condition (if clause)

class Pattern(ASTNode):
    """Base class for patterns in match expressions."""
    def __init__(self, pattern_type, line_number=None):
        super().__init__(f"pattern_{pattern_type}", line_number)

class LiteralPattern(Pattern):
    """Matches a literal value: case 42, case "hello", case true"""
    def __init__(self, value, line_number=None):
        super().__init__("literal", line_number)
        self.value = value

class IdentifierPattern(Pattern):
    """Binds matched value to a variable: case value"""
    def __init__(self, name, line_number=None):
        super().__init__("identifier", line_number)
        self.name = name

class WildcardPattern(Pattern):
    """Matches any value: case _"""
    def __init__(self, line_number=None):
        super().__init__("wildcard", line_number)
    
    def __repr__(self):
        return "WildcardPattern(_)"


class OptionPattern(Pattern):
    """Pattern for matching Option<T> values.
    
    Production-ready pattern matching for Some/None variants.
    Supports destructuring and variable binding.
    
    Examples:
        case Some with value  # Matches Some and binds value
        case None             # Matches None
    """
    
    def __init__(self, variant: str, binding: str = None, line_number: int = None):
        """Initialize Option pattern.
        
        Args:
            variant: "Some" or "None"
            binding: Variable name to bind the value (for Some)
            line_number: Source line number
        """
        super().__init__("option", line_number)
        if variant not in ("Some", "None"):
            raise ValueError(f"Invalid Option variant: {variant}")
        
        self.variant = variant
        self.binding = binding
    
    def __repr__(self):
        if self.binding:
            return f"OptionPattern({self.variant} with {self.binding})"
        return f"OptionPattern({self.variant})"


class ResultPattern(Pattern):
    """Pattern for matching Result<T,E> values.
    
    Production-ready pattern matching for Ok/Err variants.
    Supports destructuring and variable binding.
    
    Examples:
        case Ok with value    # Matches Ok and binds value
        case Err with error   # Matches Err and binds error
    """
    
    def __init__(self, variant: str, binding: str = None, line_number: int = None):
        """Initialize Result pattern.
        
        Args:
            variant: "Ok" or "Err"
            binding: Variable name to bind the value/error
            line_number: Source line number
        """
        super().__init__("result", line_number)
        if variant not in ("Ok", "Err"):
            raise ValueError(f"Invalid Result variant: {variant}")
        
        self.variant = variant
        self.binding = binding
    
    def __repr__(self):
        if self.binding:
            return f"ResultPattern({self.variant} with {self.binding})"
        return f"ResultPattern({self.variant})"


class VariantPattern(Pattern):
    """Matches a variant with optional binding: case Ok value, case Error message"""
    def __init__(self, variant_name, bindings=None, line_number=None):
        super().__init__("variant", line_number)
        self.variant_name = variant_name  # "Ok", "Error", "Some", "None"
        self.bindings = bindings or []  # List of identifiers to bind

class TuplePattern(Pattern):
    """Matches a tuple: case (x, y)"""
    def __init__(self, patterns, line_number=None):
        super().__init__("tuple", line_number)
        self.patterns = patterns or []  # List of nested patterns

class ListPattern(Pattern):
    """Matches a list: case [first, second, ...rest]"""
    def __init__(self, patterns, rest_binding=None, line_number=None):
        super().__init__("list", line_number)
        self.patterns = patterns or []  # List of patterns
        self.rest_binding = rest_binding  # Optional ...rest binding

class SwitchStatement(ASTNode):
    """Represents a switch statement for multi-way branching.
    
    Example:
        switch value
            case 1
                print text "One"
            case 2
                print text "Two"
            default
                print text "Other"
    """
    def __init__(self, expression, cases, default_case=None, line_number=None):
        super().__init__("switch_statement", line_number)
        self.expression = expression  # Expression to switch on
        self.cases = cases or []  # List of SwitchCase nodes
        self.default_case = default_case  # Optional default case body (list of statements)

class SwitchCase(ASTNode):
    """Represents a single case in a switch statement.
    
    Example:
        case 42
            print text "Found it!"
    """
    def __init__(self, value, body, line_number=None):
        super().__init__("switch_case", line_number)
        self.value = value  # Expression to match (literal, identifier, etc.)
        self.body = body  # List of statements to execute

class MemoryAllocation(ASTNode):
    """Represents memory allocation (like C++ new)."""
    def __init__(self, var_type, identifier, initial_value=None, line_number=None):
        super().__init__("memory_allocation", line_number)
        self.var_type = var_type
        self.identifier = identifier
        self.initial_value = initial_value

class MemoryDeallocation(ASTNode):
    """Represents memory deallocation (like C++ delete)."""
    def __init__(self, identifier, line_number=None):
        super().__init__("memory_deallocation", line_number)
        self.identifier = identifier

class ClassDefinition(ASTNode):
    """Represents a class definition with inheritance and interface implementation."""
    def __init__(self, name, properties=None, methods=None, parent_classes=None,
                 implemented_interfaces=None, generic_parameters=None, is_exported=False,
                 invariants=None, decorators=None, type_param_kinds=None, line_number=None):
        super().__init__("class_definition", line_number)
        self.name = name
        self.properties = properties or []
        self.methods = methods or []
        self.parent_classes = parent_classes or []
        self.implemented_interfaces = implemented_interfaces or []
        self.generic_parameters = generic_parameters or []
        self.is_exported = is_exported
        self.invariants = invariants or []  # List of InvariantStatement nodes
        self.decorators = decorators or []  # List of Decorator nodes
        self.type_param_kinds = type_param_kinds or {}  # Dict: param_name -> KindAnnotation (HKT)

class StructDefinition(ASTNode):
    """Represents a C-style struct definition with memory layout control.
    
    Supports C++-style structs with methods for hybrid data/behavior structures.
    """
    def __init__(self, name, fields=None, methods=None, packed=False, alignment=None, line_number=None):
        super().__init__("struct_definition", line_number)
        self.name = name
        self.fields = fields or []  # List of StructField nodes
        self.methods = methods or []  # List of MethodDefinition nodes (C++-style)
        self.packed = packed  # Whether struct is packed (no padding)
        self.alignment = alignment  # Explicit alignment requirement

class StructField(ASTNode):
    """Represents a field in a struct or union."""
    def __init__(self, name, type_annotation, bit_width=None, line_number=None):
        super().__init__("struct_field", line_number)
        self.name = name
        self.type_annotation = type_annotation
        self.bit_width = bit_width  # For bit fields (e.g., 3 bits)

class UnionDefinition(ASTNode):
    """Represents a C-style union where all fields share the same memory."""
    def __init__(self, name, fields=None, line_number=None):
        super().__init__("union_definition", line_number)
        self.name = name
        self.fields = fields or []  # List of StructField nodes

class EnumDefinition(ASTNode):
    """Represents an enumeration type definition.
    
    Enums can have auto-numbered values or explicit integer/string values.
    Examples:
        enum Color { Red, Green, Blue }  # Auto-numbered: 0, 1, 2
        enum Status { Success = 0, Error = 1, Pending = 2 }
        enum LogLevel { Debug = "DEBUG", Info = "INFO", Error = "ERROR" }
    """
    def __init__(self, name, members=None, line_number=None):
        super().__init__("enum_definition", line_number)
        self.name = name
        self.members = members or []  # List of EnumMember nodes

class EnumMember(ASTNode):
    """Represents a member of an enum."""
    def __init__(self, name, value=None, line_number=None):
        super().__init__("enum_member", line_number)
        self.name = name
        self.value = value  # None for auto-numbered, or explicit value (int/string)

class PropertyDeclaration(ASTNode):
    """Represents a class property declaration."""
    def __init__(self, name, type_annotation=None, default_value=None, access_modifier='public', line_number=None):
        super().__init__("property_declaration", line_number)
        self.name = name
        self.type_annotation = type_annotation  # Optional type annotation
        self.default_value = default_value  # Optional default value expression
        self.access_modifier = access_modifier  # 'public', 'private', or 'protected'

class MethodDefinition(ASTNode):
    """Represents a class method definition."""
    """Represents a class method definition."""
    def __init__(self, name, parameters, body=None, return_type=None, is_static=False, access_modifier='public', line_number=None, is_operator=False, operator_symbol=None):
        super().__init__("method_definition", line_number)
        self.name = name
        self.parameters = parameters or []
        self.body = body or []
        self.return_type = return_type  # Optional return type annotation
        self.is_static = is_static
        self.access_modifier = access_modifier  # 'public', 'private', or 'protected'
        self.is_operator = is_operator          # True if this is an operator overload
        self.operator_symbol = operator_symbol  # e.g. '+', '-', '==', etc.

class ObjectInstantiation(ASTNode):
    """Represents object creation with 'new ClassName' or 'new ClassName<T>'."""
    def __init__(self, class_name, arguments=None, type_arguments=None, line_number=None):
        super().__init__("object_instantiation", line_number)
        self.class_name = class_name
        self.arguments = arguments or []  # Constructor arguments
        self.type_arguments = type_arguments or []  # Generic type arguments like <Integer, String>

class MemberAccess(ASTNode):
    """Represents member access: object.property or object.method()."""
    def __init__(self, object_expr, member_name, is_method_call=False, arguments=None, line_number=None):
        super().__init__("member_access", line_number)
        self.object_expr = object_expr  # Expression evaluating to an object
        self.member_name = member_name  # Property or method name
        self.is_method_call = is_method_call  # True if method call, False if property access
        self.arguments = arguments or []  # Method arguments if is_method_call

class ConcurrentExecution(ASTNode):
    """Represents concurrent execution of tasks."""
    def __init__(self, tasks, line_number=None):
        super().__init__("concurrent_execution", line_number)
        self.tasks = tasks or []

class TryCatch(ASTNode):
    """Represents a try-catch block."""
    def __init__(self, try_block, catch_block, exception_var=None, exception_type=None, exception_properties=None, line_number=None):
        super().__init__("try_catch", line_number)
        self.try_block = try_block
        self.catch_block = catch_block
        self.exception_var = exception_var  # Optional exception variable name
        self.exception_type = exception_type  # Optional exception type (e.g., 'ValueError', 'RuntimeError')
        self.exception_properties = exception_properties or []  # Properties to extract (e.g., ['message', 'code'])

class RaiseStatement(ASTNode):
    """Represents a raise/throw statement.
    
    Syntax:
        raise error with message "error text"
        raise ValueError with message "invalid value"
        raise error  # Re-raise current exception
    """
    def __init__(self, exception_type=None, message=None, line_number=None):
        super().__init__("raise_statement", line_number)
        self.exception_type = exception_type or "Error"  # Default to generic Error
        self.message = message  # Expression that evaluates to error message

class Expression(ASTNode):
    """Base class for expressions."""
    def __init__(self, expr_type, line_number=None):
        super().__init__(expr_type, line_number)

class BinaryOperation(Expression):
    """Represents a binary operation (e.g., addition, subtraction)."""
    def __init__(self, left, operator, right, line_number=None):
        super().__init__("binary_operation", line_number)
        self.left = left
        self.operator = operator
        self.right = right

class UnaryOperation(Expression):
    """Represents a unary operation (e.g., negation)."""
    def __init__(self, operator, operand, line_number=None):
        super().__init__("unary_operation", line_number)
        self.operator = operator
        self.operand = operand

class Identifier(Expression):
    """Represents an identifier (variable or function name)."""
    def __init__(self, name, line_number=None):
        super().__init__("identifier", line_number)
        self.name = name

class FunctionCall(Expression):
    """Represents a function call with positional and/or named arguments, optionally with a trailing block."""
    def __init__(self, name, arguments=None, type_arguments=None, named_arguments=None, trailing_block=None, line_number=None):
        super().__init__("function_call", line_number)
        self.name = name
        self.arguments = arguments or []  # Positional arguments (list)
        self.named_arguments = named_arguments or {}  # Named arguments (dict: param_name -> value)
        self.type_arguments = type_arguments or []  # Generic type arguments like <Integer, String>
        self.trailing_block = trailing_block  # Optional trailing block (LambdaExpression)

class RepeatNTimesLoop(ASTNode):
    """Represents a repeat-n-times loop."""
    def __init__(self, count, body=None, line_number=None):
        super().__init__("repeat_n_times_loop", line_number)
        self.count = count
        self.body = body or []

class RepeatWhileLoop(ASTNode):
    """Represents a repeat-while loop (natural language while loop)."""
    def __init__(self, condition, body, line_number=None, else_body=None, label=None):
        super().__init__("repeat_while_loop", line_number)
        self.condition = condition
        self.body = body
        self.else_body = else_body  # Optional else block (executed if loop completes without break)
        self.label = label  # Optional label for labeled break/continue

class Block(ASTNode):
    """Represents a block of statements."""
    def __init__(self, statements=None, line_number=None):
        super().__init__("block", line_number)
        self.statements = statements or []

class ReturnStatement(ASTNode):
    """Represents a return statement."""
    def __init__(self, value=None, line_number=None):
        super().__init__("return_statement", line_number)
        self.value = value

class StringLiteral(ASTNode):
    """Represents a string literal."""
    def __init__(self, value, line_number=None):
        super().__init__("string_literal", line_number)
        self.value = value

class FStringExpression(ASTNode):
    """Represents an f-string with interpolation: f"Hello, {name}!" """
    def __init__(self, parts, line_number=None):
        super().__init__("fstring_expression", line_number)
        self.parts = parts  # List of (is_literal, content, format_spec) tuples
        self.line_number = line_number
    
    def __repr__(self):
        return f"FStringExpression({len(self.parts)} parts)"


class TryExpression(ASTNode):
    """Represents the ? operator for error propagation.
    
    Production-ready implementation for Result<T,E> unwrapping.
    Automatically propagates errors by early returning Err values.
    
    Example:
        let file = open_file(path)?  # Unwraps Ok, returns Err
        let content = read_all(file)?
    
    Semantics:
        - If expression evaluates to Ok(value), unwraps to value
        - If expression evaluates to Err(error), returns Err(error) from function
        - Can only be used in functions returning Result<T,E>
    """
    
    def __init__(self, expression, line_number=None):
        """Initialize try expression.
        
        Args:
            expression: Expression that should evaluate to Result<T,E>
            line_number: Source line number
        """
        super().__init__("try_expression", line_number)
        self.expression = expression
    
    def __repr__(self):
        return f"TryExpression({self.expression}?)"


class BreakStatement(ASTNode):
    """Represents a break statement."""
    def __init__(self, line_number=None, label=None):
        super().__init__("break_statement", line_number)
        self.label = label  # Optional label for breaking out of nested loops

class ContinueStatement(ASTNode):
    """Represents a continue statement."""
    def __init__(self, line_number=None, label=None):
        super().__init__("continue_statement", line_number)
        self.label = label  # Optional label for continuing specific nested loop
        self.label = label  # Optional label for continuing specific loop

class FallthroughStatement(ASTNode):
    """Represents a fallthrough statement in switch cases."""
    def __init__(self, line_number=None):
        super().__init__("fallthrough_statement", line_number)

class PanicStatement(ASTNode):
    """Represents a panic statement: panic with "message"."""
    def __init__(self, message, line_number=None):
        super().__init__("panic_statement", line_number)
        self.message = message

class SendStatement(ASTNode):
    """Represents sending a value to a channel: send value to channel."""
    def __init__(self, value, channel, line_number=None):
        super().__init__("send_statement", line_number)
        self.value = value
        self.channel = channel


class CloseStatement(ASTNode):
    """Represents closing a channel: close channel."""
    def __init__(self, channel, line_number=None):
        super().__init__("close_statement", line_number)
        self.channel = channel

class ConcurrentBlock(ASTNode):
    """Represents a concurrent block of statements."""
    def __init__(self, statements=None, line_number=None):
        super().__init__("concurrent_block", line_number)
        self.statements = statements or []

class TryCatchBlock(ASTNode):
    """Represents a try-catch block."""
    def __init__(self, try_block, catch_block, exception_var=None, exception_type=None, line_number=None):
        super().__init__("try_catch_block", line_number)
        self.try_block = try_block
        self.catch_block = catch_block
        self.exception_var = exception_var  # Optional exception variable name
        self.exception_type = exception_type  # Optional exception type (e.g., 'ValueError', 'RuntimeError')

class PrintStatement(ASTNode):
    """Represents a print statement with optional type hints.
    
    Syntax:
        print "hello"
        print text value
        print number 42
    """
    def __init__(self, expression, print_type=None, line_number=None):
        super().__init__("print_statement", line_number)
        self.expression = expression
        self.print_type = print_type  # "text", "number", or None
        
# Module-related AST nodes
class ImportStatement(ASTNode):
    """Represents an import statement."""
    def __init__(self, module_name, alias=None, line_number=None):
        super().__init__("import_statement", line_number)
        self.module_name = module_name
        self.alias = alias  # Optional alias for the module

class SelectiveImport(ASTNode):
    """Represents a selective import statement."""
    def __init__(self, module_name, imported_names, line_number=None):
        super().__init__("selective_import", line_number)
        self.module_name = module_name
        self.imported_names = imported_names  # List of names to import

class ModuleAccess(Expression):
    """Represents access to a module member (e.g., module.function)."""
    def __init__(self, module_name, member_name, line_number=None):
        super().__init__("module_access", line_number)
        self.module_name = module_name
        self.member_name = member_name

class PrivateDeclaration(ASTNode):
    """Represents a private declaration modifier."""
    def __init__(self, declaration, line_number=None):
        super().__init__("private_declaration", line_number)
        self.declaration = declaration  # The declaration being marked as private

class ModuleDefinition(ASTNode):
    """Represents a module definition."""
    def __init__(self, name, exports=None, line_number=None):
        super().__init__("module_definition", line_number)
        self.name = name
        self.exports = exports or []  # List of exported names (functions, classes, etc.)

class ExportStatement(ASTNode):
    """Represents an export statement."""
    def __init__(self, names, line_number=None):
        super().__init__("export_statement", line_number)
        self.names = names  # List of names to export

# Add InterfaceDefinition class
class InterfaceDefinition(ASTNode):
    """Represents an interface definition."""
    def __init__(self, name, methods=None, generic_parameters=None, line_number=None):
        super().__init__("interface_definition", line_number)
        self.name = name
        self.methods = methods or []
        self.generic_parameters = generic_parameters or []

class AbstractClassDefinition(ASTNode):
    """Represents an abstract class definition."""
    def __init__(self, name, abstract_methods=None, concrete_methods=None, properties=None, 
                 parent_classes=None, implemented_interfaces=None, generic_parameters=None, line_number=None):
        super().__init__("abstract_class_definition", line_number)
        self.name = name
        self.abstract_methods = abstract_methods or []
        self.concrete_methods = concrete_methods or []
        self.properties = properties or []
        self.parent_classes = parent_classes or []
        self.implemented_interfaces = implemented_interfaces or []
        self.generic_parameters = generic_parameters or []

class AbstractMethodDefinition(ASTNode):
    """Represents an abstract method definition (without implementation)."""
    def __init__(self, name, parameters=None, return_type=None, line_number=None):
        super().__init__("abstract_method_definition", line_number)
        self.name = name
        self.parameters = parameters or []
        self.return_type = return_type  # Optional return type

class TraitDefinition(ASTNode):
    """Represents a trait definition."""
    def __init__(self, name, required_methods=None, provided_methods=None, 
                 required_traits=None, generic_parameters=None, line_number=None):
        super().__init__("trait_definition", line_number)
        self.name = name
        self.required_methods = required_methods or []
        self.provided_methods = provided_methods or []
        self.required_traits = required_traits or []
        self.generic_parameters = generic_parameters or []

class TypeAliasDefinition(ASTNode):
    """Represents a type alias definition."""
    def __init__(self, name, target_type, generic_parameters=None, line_number=None):
        super().__init__("type_alias_definition", line_number)
        self.name = name
        self.target_type = target_type
        self.generic_parameters = generic_parameters or []

class TypeParameter(ASTNode):
    """Represents a generic type parameter.

    For ground types (kind ``*``), *kind* is ``None``.
    For higher-kinded parameters, *kind* is a :class:`KindAnnotation` subtree.
    """
    def __init__(self, name, bounds=None, variance=None, kind=None, line_number=None):
        super().__init__("type_parameter", line_number)
        self.name = name
        self.bounds = bounds or []  # List of types that bound this parameter
        self.variance = variance    # 'covariant', 'contravariant', or None for invariant
        self.kind = kind            # Optional KindAnnotation for HKT

class KindAnnotation(ASTNode):
    """Base class for kind annotations in higher-kinded type parameters."""
    def __init__(self, node_type, line_number=None):
        super().__init__(node_type, line_number)

class StarKindAnnotation(KindAnnotation):
    """Represents the ground kind ``*`` (fully applied type)."""
    def __init__(self, line_number=None):
        super().__init__("star_kind", line_number)

    def __repr__(self):
        return "*"

class ArrowKindAnnotation(KindAnnotation):
    """Represents a kind arrow ``left -> right`` (type constructor)."""
    def __init__(self, left, right, line_number=None):
        super().__init__("arrow_kind", line_number)
        self.left = left    # KindAnnotation
        self.right = right  # KindAnnotation

    def __repr__(self):
        left_str = f"({repr(self.left)})" if isinstance(self.left, ArrowKindAnnotation) else repr(self.left)
        return f"{left_str} -> {repr(self.right)}"

class TypeConstraint(ASTNode):
    """Represents a type constraint."""
    def __init__(self, type_parameter, constraint_type, line_number=None):
        super().__init__("type_constraint", line_number)
        self.type_parameter = type_parameter
        self.constraint_type = constraint_type

class TypeGuard(ASTNode):
    """Represents a type guard in an if statement."""
    def __init__(self, condition, type_name, body, line_number=None):
        super().__init__("type_guard", line_number)
        self.condition = condition
        self.type_name = type_name
        self.body = body

class ListExpression(Expression):
    """Represents a list literal expression."""
    def __init__(self, elements, line_number=None):
        super().__init__("list_expression", line_number)
        self.elements = elements

class DictExpression(Expression):
    """Represents a dictionary literal expression."""
    def __init__(self, entries, line_number=None):
        super().__init__("dict_expression", line_number)
        self.entries = entries

class SliceExpression(Expression):
    """Represents a slice expression."""
    def __init__(self, expr, start, end, step, line_number=None):
        super().__init__("slice_expression", line_number)
        self.expr = expr
        self.start = start
        self.end = end
        self.step = step

class IndexExpression(Expression):
    """Represents array/list indexing: array[index]."""
    def __init__(self, array_expr, index_expr, line_number=None):
        super().__init__("index_expression", line_number)
        self.array_expr = array_expr  # Expression evaluating to array/list
        self.index_expr = index_expr  # Expression evaluating to index

class ListComprehension(Expression):
    """Represents a list comprehension."""
    def __init__(self, expr, target, iterable, condition, line_number=None):
        super().__init__("list_comprehension", line_number)
        self.expr = expr
        self.target = target
        self.iterable = iterable
        self.condition = condition

class DictComprehension(Expression):
    """Represents a dictionary comprehension."""
    def __init__(self, key, value, target, iterable, condition, line_number=None):
        super().__init__("dict_comprehension", line_number)
        self.key = key
        self.value = value
        self.target = target
        self.iterable = iterable
        self.condition = condition

class TernaryExpression(Expression):
    """Represents a ternary expression (condition ? true_expr : false_expr)."""
    def __init__(self, condition, true_expr, false_expr, line_number=None):
        super().__init__("ternary_expression", line_number)
        self.condition = condition
        self.true_expr = true_expr
        self.false_expr = false_expr

class NullCoalesceExpression(Expression):
    """Represents a null coalescing expression: value otherwise default.

    Evaluates to *value* if it is not None/null, otherwise evaluates and
    returns *default*.  Unlike ``or``, this only triggers on null/None, not
    on any falsy value (e.g. 0, False, or "").
    """
    def __init__(self, value, default, line_number=None):
        super().__init__("null_coalesce_expression", line_number)
        self.value = value
        self.default = default

class LambdaExpression(Expression):
    """Represents a lambda expression."""
    def __init__(self, parameters, body, line_number=None, return_type=None):
        super().__init__("lambda_expression", line_number)
        self.parameters = parameters
        self.body = body
        self.return_type = return_type

class AsyncExpression(Expression):
    """Represents an async expression."""
    def __init__(self, expr, line_number=None):
        super().__init__("async_expression", line_number)
        self.expr = expr

class AwaitExpression(Expression):
    """Represents an await expression."""
    def __init__(self, expr, line_number=None):
        super().__init__("await_expression", line_number)
        self.expr = expr

class YieldExpression(Expression):
    """Represents a yield expression."""
    def __init__(self, value, line_number=None):
        super().__init__("yield_expression", line_number)
        self.value = value

class AddressOfExpression(Expression):
    """Represents taking the address of a variable: address of variable or &variable."""
    def __init__(self, target, line_number=None):
        super().__init__("address_of_expression", line_number)
        self.target = target  # Variable or expression to get address of

class DereferenceExpression(Expression):
    """Represents dereferencing a pointer: dereference pointer or *pointer or value at pointer."""
    def __init__(self, pointer, line_number=None):
        super().__init__("dereference_expression", line_number)
        self.pointer = pointer  # Pointer expression to dereference

class SizeofExpression(Expression):
    """Represents sizeof operator: sizeof Type or size of variable."""
    def __init__(self, target, line_number=None):
        super().__init__("sizeof_expression", line_number)
        self.target = target  # Type name or variable to get size of

class DowngradeExpression(Expression):
    """Represents downgrading Rc to Weak: downgrade rc_value (break reference cycles)."""
    def __init__(self, rc_expr, line_number=None):
        super().__init__("downgrade_expression", line_number)
        self.rc_expr = rc_expr  # Rc expression to downgrade to Weak

class UpgradeExpression(Expression):
    """Represents upgrading Weak to Rc: upgrade weak_value (may return null if deallocated)."""
    def __init__(self, weak_expr, line_number=None):
        super().__init__("upgrade_expression", line_number)
        self.weak_expr = weak_expr  # Weak expression to upgrade to Rc

class MoveExpression(Expression):
    """Transfer ownership: move x — evaluates x, marks x as moved (inaccessible after this).

    Syntax:  move <identifier>
    """
    def __init__(self, var_name, line_number=None):
        super().__init__("move_expression", line_number)
        self.var_name = var_name  # name of the variable being moved

class BorrowExpression(Expression):
    """Temporarily borrow a variable: borrow x / borrow mutable x.

    An immutable borrow prevents the variable from being written while borrowed.
    A mutable borrow additionally prevents other borrows of the variable.

    Syntax:
        borrow <identifier>           # immutable borrow
        borrow mutable <identifier>   # mutable borrow
    """
    def __init__(self, var_name, mutable=False, line_number=None):
        super().__init__("borrow_expression", line_number)
        self.var_name = var_name    # name of the variable being borrowed
        self.mutable = mutable      # True = mutable borrow

class DropBorrowStatement(ASTNode):
    """Release a previously registered borrow: drop borrow x / drop borrow mutable x.

    Syntax:
        drop borrow <identifier>          # release one immutable borrow
        drop borrow mutable <identifier>  # release the mutable borrow
    """
    def __init__(self, var_name, mutable=False, line_number=None):
        super().__init__("drop_borrow_statement", line_number)
        self.var_name = var_name    # name of the variable whose borrow is released
        self.mutable = mutable      # True = releasing a mutable borrow

class LifetimeAnnotation(ASTNode):
    """A named lifetime label used in borrow / function-signature annotations.

    Syntax::

        borrow x with lifetime outer
        borrow mutable x with lifetime inner

        function first with x as borrow String with lifetime a
                         and y as borrow String with lifetime b
                         returns borrow String with lifetime a
            return borrow x
        end

    The ``label`` is a plain identifier chosen by the programmer.  Lifetime
    labels on borrow expressions inside a function body must match the labels
    declared on the function parameters or return type so the
    :class:`~nlpl.typesystem.lifetime_checker.LifetimeChecker` can verify that
    returned references actually outlive the call.
    """

    def __init__(self, label: str, line_number=None):
        super().__init__("lifetime_annotation", line_number)
        self.label = label  # The programmer-chosen identifier (e.g. 'a', 'outer')

    def __repr__(self) -> str:
        return f"LifetimeAnnotation(label={self.label!r})"


class BorrowExpressionWithLifetime(ASTNode):
    """A borrow expression that carries an explicit lifetime annotation.

    This is produced by the parser when a ``borrow`` expression is followed by
    ``with lifetime <label>``.  The :class:`~nlpl.typesystem.borrow_checker.BorrowChecker`
    and :class:`~nlpl.typesystem.lifetime_checker.LifetimeChecker` both inspect
    this node.

    Syntax::

        set ref to borrow x with lifetime outer
        set mut_ref to borrow mutable y with lifetime inner
    """

    def __init__(self, var_name: str, mutable: bool = False,
                 lifetime: "LifetimeAnnotation | None" = None,
                 line_number=None):
        super().__init__("borrow_expression_with_lifetime", line_number)
        self.var_name = var_name
        self.mutable = mutable
        self.lifetime = lifetime  # LifetimeAnnotation or None

    def __repr__(self) -> str:
        mut = " mutable" if self.mutable else ""
        lt = f" with lifetime {self.lifetime.label}" if self.lifetime else ""
        return f"BorrowExpressionWithLifetime(borrow{mut} {self.var_name}{lt})"


class ParameterWithLifetime(ASTNode):
    """A function parameter with an explicit borrow-lifetime annotation.

    Produced by the parser when a parameter type is ``borrow <Type> with lifetime <label>``.

    Syntax::

        function f with x as borrow String with lifetime a returns borrow String with lifetime a
    """

    def __init__(self, name: str, type_annotation=None,
                 borrow_mutable: bool = False,
                 lifetime: "LifetimeAnnotation | None" = None,
                 default_value=None,
                 is_variadic: bool = False,
                 keyword_only: bool = False,
                 line_number=None):
        super().__init__("parameter_with_lifetime", line_number)
        self.name = name
        self.type_annotation = type_annotation
        self.borrow_mutable = borrow_mutable   # True if the parameter is a mutable borrow
        self.lifetime = lifetime               # LifetimeAnnotation or None
        self.default_value = default_value
        self.is_variadic = is_variadic
        self.keyword_only = keyword_only

    def __repr__(self) -> str:
        lt = f" lifetime {self.lifetime.label}" if self.lifetime else ""
        mut = " mutable" if self.borrow_mutable else ""
        return f"ParameterWithLifetime({self.name}: borrow{mut} {self.type_annotation}{lt})"


class ReturnTypeWithLifetime(ASTNode):
    """A function return type annotated with a borrow lifetime.

    Produced by the parser when a ``returns borrow <Type> with lifetime <label>``
    clause appears in a function definition.

    Syntax::

        returns borrow String with lifetime a
    """

    def __init__(self, base_type, borrow_mutable: bool = False,
                 lifetime: "LifetimeAnnotation | None" = None,
                 line_number=None):
        super().__init__("return_type_with_lifetime", line_number)
        self.base_type = base_type          # The plain type (e.g. 'String')
        self.borrow_mutable = borrow_mutable
        self.lifetime = lifetime            # LifetimeAnnotation or None

    def __repr__(self) -> str:
        lt = f" lifetime {self.lifetime.label}" if self.lifetime else ""
        mut = " mutable" if self.borrow_mutable else ""
        return f"ReturnTypeWithLifetime(borrow{mut} {self.base_type}{lt})"


class OffsetofExpression(Expression):
    """Represents offsetof operator: offset of field in StructName."""
    def __init__(self, struct_type, field_name, line_number=None):
        super().__init__("offsetof_expression", line_number)
        self.struct_type = struct_type  # Struct/Union type name
        self.field_name = field_name    # Field name within the struct

class TypeCastExpression(Expression):
    """Represents type casting: (expression as TargetType)."""
    def __init__(self, expression, target_type, line_number=None):
        super().__init__("type_cast", line_number)
        self.expression = expression      # Expression to cast
        self.target_type = target_type    # Target type to cast to

class PointerType:
    """Represents a pointer type annotation: Pointer to Integer."""
    def __init__(self, base_type):
        self.base_type = base_type  # The type this pointer points to
        
    def __str__(self):
        return f"Pointer to {self.base_type}"

class GeneratorExpression(Expression):
    """Represents a generator expression."""
    def __init__(self, expr, target, iterable, condition, line_number=None):
        super().__init__("generator_expression", line_number)
        self.expr = expr
        self.target = target
        self.iterable = iterable
        self.condition = condition

class GenericTypeInstantiation(Expression):
    """Represents a generic type instantiation: create list of Integer, Dict of String to Integer."""
    def __init__(self, generic_name, type_args, initial_value=None, line_number=None):
        super().__init__("generic_type_instantiation", line_number)
        self.generic_name = generic_name  # "list", "dict", etc.
        self.type_args = type_args        # List of type names: ["Integer"], ["String", "Integer"]
        self.initial_value = initial_value  # Optional initial value (e.g., list elements)
    
    def __str__(self):
        return f"GenericType({self.generic_name}<{', '.join(self.type_args)}>)"

class ChannelCreation(Expression):
    """Represents channel creation: create channel."""
    def __init__(self, line_number=None):
        super().__init__("channel_creation", line_number)

class ReceiveExpression(Expression):
    """Represents receiving a value from a channel: receive from channel."""
    def __init__(self, channel, line_number=None):
        super().__init__("receive_expression", line_number)
        self.channel = channel

class InlineAssembly(ASTNode):
    """Represents inline assembly code.

    The optional *arch* attribute is an architecture guard: when set the
    compiler only emits the block for the matching target architecture
    (e.g. ``"riscv64"``, ``"arm"``, ``"mips"``).  In the interpreter the
    block is always skipped regardless of arch.
    """
    def __init__(self, asm_code, inputs=None, outputs=None, clobbers=None,
                 line_number=None, arch=None):
        super().__init__("inline_assembly", line_number)
        self.asm_code = asm_code          # Assembly code string(s)
        self.inputs = inputs or []        # Input operands: [(constraint, expression), ...]
        self.outputs = outputs or []      # Output operands: [(constraint, variable), ...]
        self.clobbers = clobbers or []    # Clobbered registers
        self.arch = arch                  # Optional architecture guard, e.g. "riscv64"

    def __str__(self):
        arch_part = f" arch={self.arch}" if self.arch else ""
        return f"InlineAssembly({len(self.asm_code)} instructions{arch_part})"

    def __repr__(self):
        return (
            f"InlineAssembly(code={len(self.asm_code)}, "
            f"inputs={len(self.inputs)}, outputs={len(self.outputs)}, "
            f"clobbers={len(self.clobbers)}, arch={self.arch!r})"
        )

class ExternFunctionDeclaration(ASTNode):
    """Represents an external function declaration for FFI.
    
    Example:
        extern function printf with format as Pointer, ... returns Integer from library "c"
        foreign function malloc with size as Integer returns Pointer from library "c" calling convention cdecl
    """
    def __init__(self, name, parameters, return_type, library=None, calling_convention="cdecl", variadic=False, line_number=None):
        super().__init__("extern_function_declaration", line_number)
        self.name = name
        self.parameters = parameters or []
        self.return_type = return_type
        self.library = library  # Library name (e.g., "c", "m", "pthread")
        self.calling_convention = calling_convention  # cdecl, stdcall, etc.
        self.variadic = variadic  # True if function accepts variable arguments (...)
        # Buffer size annotations: list of (param_index, size_param_index_or_literal)
        # e.g. [(0, 1)] means param 0 has its size in param 1
        # Used to emit __attribute__((access(...))) and runtime buffer checks.
        self.buffer_size_annotations: list = []
    
    def __str__(self):
        return f"ExternFunction({self.name} from {self.library})"

class ExternVariableDeclaration(ASTNode):
    """Represents an external variable declaration for FFI.
    
    Example:
        extern variable errno as Integer from library "c"
    """
    def __init__(self, name, type_annotation, library=None, line_number=None):
        super().__init__("extern_variable_declaration", line_number)
        self.name = name
        self.type_annotation = type_annotation
        self.library = library
    
    def __str__(self):
        return f"ExternVariable({self.name} from {self.library})"

class ExternTypeDeclaration(ASTNode):
    """Represents an external type declaration for FFI.
    
    Examples:
        extern type FILE as opaque pointer
        extern type CompareFunc as function with Integer, Integer returns Integer
        extern type pthread_t as opaque pointer
        extern type sockaddr as opaque struct
    """
    def __init__(self, name, base_type, is_opaque=False, is_function_pointer=False, 
                 function_signature=None, line_number=None):
        super().__init__("extern_type_declaration", line_number)
        self.name = name
        self.base_type = base_type  # "pointer", "struct", "function", etc.
        self.is_opaque = is_opaque
        self.is_function_pointer = is_function_pointer
        self.function_signature = function_signature  # For function pointer types: (param_types, return_type)
    
    def __str__(self):
        if self.is_opaque:
            return f"ExternType({self.name} as opaque {self.base_type})"
        elif self.is_function_pointer:
            return f"ExternType({self.name} as function pointer)"
        else:
            return f"ExternType({self.name} as {self.base_type})"

class ForeignLibraryLoad(ASTNode):
    """Represents loading a foreign library.
    
    Example:
        load foreign library "libmath.so" as math_lib
    """
    def __init__(self, library_path, alias=None, line_number=None):
        super().__init__("foreign_library_load", line_number)
        self.library_path = library_path
        self.alias = alias or library_path
    
    def __str__(self):
        return f"ForeignLibraryLoad({self.library_path} as {self.alias})"


class UnsafeBlock(ASTNode):
    """Marks a block of FFI code as explicitly unsafe.

    Unsafe blocks suppress NLPL's automatic safety checks (null-pointer guards,
    bounds checks, ownership enforcement) for the statements they contain.
    Only use when calling C functions that cannot be wrapped safely.

    Example::

        unsafe do
            set raw_ptr to malloc with 1024
            free with raw_ptr
        end
    """

    def __init__(self, body, line_number=None):
        super().__init__("unsafe_block", line_number)
        self.body = body or []

    def __str__(self):
        return f"UnsafeBlock({len(self.body)} statements)"

    def __repr__(self):
        return f"UnsafeBlock(body={len(self.body)} stmts)"

class CallbackReference(Expression):
    """Represents a reference to a callback function.
    
    This is used when passing NexusLang functions as callbacks to C functions.
    
    Example:
        callback compare_ints
        callback my_handler
    """
    def __init__(self, function_name, line_number=None):
        super().__init__("callback_reference", line_number)
        self.function_name = function_name
    
    def __str__(self):
        return f"Callback({self.function_name})"


class AllocatorHint(ASTNode):
    """
    Wraps a type annotation with an allocator hint.

    Produced when the programmer writes:
        set items to List of Integer with allocator arena_alloc

    At declaration time the interpreter will route the initial collection
    creation through the named allocator so that the backing memory comes from
    the specified source.

    Fields:
        base_type  - the inner type string, e.g. 'List of Integer'
        allocator_name - identifier naming the allocator variable
    """
    def __init__(self, base_type, allocator_name, line_number=None):
        super().__init__("allocator_hint", line_number)
        self.base_type = base_type
        self.allocator_name = allocator_name

    def __str__(self):
        return f"AllocatorHint({self.base_type} with allocator {self.allocator_name})"

    def __repr__(self):
        return self.__str__()


class ParallelForLoop(ASTNode):
    """
    Parallel for-each loop.

    Produced when the programmer writes:
        parallel for each item in collection
            ...body...
        end

    The interpreter executes the body concurrently for each element using
    a thread pool.  Each iteration runs in its own scope and sees the
    enclosing scope's variables as read-only bindings; writes are
    iteration-local (safe by construction).

    Fields:
        var_name   - loop variable identifier string
        iterable   - expression that yields the collection
        body       - list of statement AST nodes
    """
    def __init__(self, var_name, iterable, body, line_number=None):
        super().__init__("parallel_for_loop", line_number)
        self.var_name = var_name
        self.iterable = iterable
        self.body = body

    def __str__(self):
        return f"ParallelForLoop({self.var_name} in {self.iterable})"


class ConditionalCompilationBlock(ASTNode):
    """
    Compile-time / target-conditional block.

    Syntax variants:
        when target os is "linux"
            ...body...
        end

        when target arch is "x86_64"
            ...body...
        otherwise
            ...else_body...
        end

        when feature "networking"
            ...body...
        end

    The interpreter evaluates the condition against the current runtime
    platform (os.name, platform.system, platform.machine) and executes
    the matching branch.  The else/otherwise branch is optional.

    Fields:
        condition_type  - "target_os" | "target_arch" | "target_pointer_width"
                          | "target_endian" | "feature" | "platform"
        condition_value - the string value to match against
        body            - list of statements for the true branch
        else_body       - list of statements for the false branch (or None)
    """
    def __init__(self, condition_type, condition_value,
                 body, else_body=None, line_number=None):
        super().__init__("conditional_compilation_block", line_number)
        self.condition_type = condition_type
        self.condition_value = condition_value
        self.body = body
        self.else_body = else_body

    def __str__(self):
        return (f"ConditionalCompilationBlock("
                f"when {self.condition_type} is {self.condition_value!r})")


# ---------------------------------------------------------------------------
# Test framework nodes
# ---------------------------------------------------------------------------

class TestBlock(ASTNode):
    """
    A named test block.

    Syntax:
        test "name" do
            ...
        end

    Attributes:
        name        - string label for the test
        body        - list of statements to execute as the test
        line_number - source line
    """
    __test__ = False

    def __init__(self, name: str, body: list, line_number=None):
        super().__init__("test_block", line_number)
        self.name = name
        self.body = body or []

    def __str__(self):
        return f"TestBlock({self.name!r}, {len(self.body)} statements)"


class DescribeBlock(ASTNode):
    """
    A describe (test suite) block that groups related tests.

    Syntax:
        describe "SuiteName" do
            ...
        end

    Attributes:
        name        - suite label
        body        - list of TestBlock / ItBlock / before_each / after_each nodes
        line_number - source line
    """
    def __init__(self, name: str, body: list, line_number=None):
        super().__init__("describe_block", line_number)
        self.name = name
        self.body = body or []

    def __str__(self):
        return f"DescribeBlock({self.name!r}, {len(self.body)} items)"


class ItBlock(ASTNode):
    """
    An `it` behaviour block (alias for test, BDD-style).

    Syntax:
        it "should do something" do
            ...
        end

    Attributes:
        name        - behaviour description
        body        - list of statements
        line_number - source line
    """
    def __init__(self, name: str, body: list, line_number=None):
        super().__init__("it_block", line_number)
        self.name = name
        self.body = body or []

    def __str__(self):
        return f"ItBlock({self.name!r}, {len(self.body)} statements)"


class ParameterizedTestBlock(ASTNode):
    """
    A parameterized test that runs once for each set of inputs.

    Syntax:
        test "name" with cases
            case (1, 2, 3)
            case (4, 5, 9)
        do
            ...
        end

    Attributes:
        name      - test label
        params    - list of parameter name strings
        cases     - list of case argument lists (each is a list of expressions)
        body      - list of statements (can reference param names)
        line_number
    """
    def __init__(self, name: str, params: list, cases: list, body: list,
                 line_number=None):
        super().__init__("parameterized_test_block", line_number)
        self.name = name
        self.params = params or []
        self.cases = cases or []
        self.body = body or []

    def __str__(self):
        return (f"ParameterizedTestBlock({self.name!r}, "
                f"{len(self.cases)} cases, params={self.params})")


class BeforeEachBlock(ASTNode):
    """
    Setup block run before each test in the enclosing describe block.

    Syntax:
        before each do
            ...
        end
    """
    def __init__(self, body: list, line_number=None):
        super().__init__("before_each_block", line_number)
        self.body = body or []

    def __str__(self):
        return f"BeforeEachBlock({len(self.body)} statements)"


class AfterEachBlock(ASTNode):
    """
    Teardown block run after each test in the enclosing describe block.

    Syntax:
        after each do
            ...
        end
    """
    def __init__(self, body: list, line_number=None):
        super().__init__("after_each_block", line_number)
        self.body = body or []

    def __str__(self):
        return f"AfterEachBlock({len(self.body)} statements)"


class ExpectStatement(ASTNode):
    """
    Assertion statement for the NexusLang test framework.

    Syntax examples:
        expect x to equal 5
        expect x to not equal 5
        expect x to be greater than 3
        expect x to be less than 10
        expect x to be greater than or equal to 0
        expect x to be less than or equal to 100
        expect x to contain "hello"
        expect x to be true
        expect x to be false
        expect x to be null
        expect x to not be null
        expect x to be approximately equal to 3.14 within 0.01

    Attributes
    ----------
    actual_expr : ASTNode
        Expression under test.
    matcher : str
        One of: "equal", "greater_than", "less_than",
        "greater_than_or_equal_to", "less_than_or_equal_to",
        "contain", "be_true", "be_false", "be_null", "approximately_equal".
    expected_expr : ASTNode or None
        The expected value (None for "be_true", "be_false", "be_null").
    negated : bool
        True when "not" appears before the matcher (e.g. "not equal").
    tolerance_expr : ASTNode or None
        Tolerance for "approximately_equal" matcher (the WITHIN value).
    """
    def __init__(self, actual_expr, matcher: str, expected_expr=None,
                 negated: bool = False, tolerance_expr=None, line_number=None):
        super().__init__("expect_statement", line_number)
        self.actual_expr = actual_expr
        self.matcher = matcher
        self.expected_expr = expected_expr
        self.negated = negated
        self.tolerance_expr = tolerance_expr

    def __str__(self):
        neg = " not" if self.negated else ""
        return f"ExpectStatement(<expr>{neg} {self.matcher} <expected>)"


class RequireStatement(ASTNode):
    """
    Contract precondition statement.

    Syntax:
        require <condition>
        require <condition> message "explanation"

    Checked at the beginning of a function.  Raises ContractError when
    the condition evaluates to False.
    """
    def __init__(self, condition, message_expr=None, line_number=None):
        super().__init__("require_statement", line_number)
        self.condition = condition
        self.message_expr = message_expr

    def __str__(self):
        return f"RequireStatement(<condition>)"


class EnsureStatement(ASTNode):
    """
    Contract postcondition statement.

    Syntax:
        ensure <condition>
        ensure <condition> message "explanation"

    Checked at the end of a function (or at the ensure keyword position
    for inline use).  Raises ContractError when the condition evaluates
    to False.
    """
    def __init__(self, condition, message_expr=None, line_number=None):
        super().__init__("ensure_statement", line_number)
        self.condition = condition
        self.message_expr = message_expr

    def __str__(self):
        return f"EnsureStatement(<condition>)"


class GuaranteeStatement(ASTNode):
    """
    Contract invariant assertion statement.

    Syntax:
        guarantee <condition>
        guarantee <condition> message "explanation"

    Raises ContractError immediately when the condition is False.
    """
    def __init__(self, condition, message_expr=None, line_number=None):
        super().__init__("guarantee_statement", line_number)
        self.condition = condition
        self.message_expr = message_expr

    def __str__(self):
        return f"GuaranteeStatement(<condition>)"


class InvariantStatement(ASTNode):
    """
    Class or scope invariant assertion.

    Syntax:
        invariant <condition>
        invariant <condition> message "explanation"

    When used inside a class body, invariants are collected and checked
    automatically after every method call.  In any other scope the
    behaviour is identical to ``guarantee``: checked immediately.

    Raises ContractError (contract_kind="invariant") when the condition
    evaluates to False.
    """
    def __init__(self, condition, message_expr=None, line_number=None):
        super().__init__("invariant_statement", line_number)
        self.condition = condition
        self.message_expr = message_expr

    def __str__(self):
        return "InvariantStatement(<condition>)"


class OldExpression(ASTNode):
    """
    Pre-call value capture for use in postconditions.

    Syntax:
        old(expr)

    The interpreter evaluates ``expr`` once *before* the function body
    executes and stores the result.  When the postcondition (``ensure``)
    is later evaluated, ``old(expr)`` returns the stored pre-call value.

    Example::

        function increment with counter as Integer returns Integer
            ensure result equals old(counter) plus 1
            return counter plus 1
        end
    """
    def __init__(self, expr, line_number=None):
        super().__init__("old_expression", line_number)
        self.expr = expr

    def __str__(self):
        return "OldExpression(<expr>)"


class SpecAnnotation(ASTNode):
    """
    A single annotation line inside a ``spec`` block.

    Syntax:
        requires <condition>
        ensures  <condition>
        invariant <condition>
        decreases <expression>

    Attributes:
        kind      (str): "requires", "ensures", "invariant", or "decreases".
        condition (ASTNode): The condition/expression to annotate.
        label     (str|None): Optional human-readable label for the VC.
    """
    def __init__(self, kind, condition, label=None, line_number=None):
        super().__init__("spec_annotation", line_number)
        self.kind = kind
        self.condition = condition
        self.label = label

    def __str__(self):
        return f"SpecAnnotation({self.kind})"


class SpecBlock(ASTNode):
    """
    Formal specification block grouping multiple spec annotations.

    Syntax:
        spec "optional name"
            requires  <cond>
            ensures   <cond>
            invariant <cond>
            decreases <expr>
        end spec

    Spec blocks are consumed by the ``nlpl-verify`` tool for static
    verification.  At runtime they are treated as no-ops so that
    programs with formal annotations still execute normally.
    """
    def __init__(self, name=None, annotations=None, line_number=None):
        super().__init__("spec_block", line_number)
        self.name = name           # Optional label string (e.g. "sort_postcond")
        self.annotations = annotations or []

    def __str__(self):
        return f"SpecBlock({self.name!r})"
