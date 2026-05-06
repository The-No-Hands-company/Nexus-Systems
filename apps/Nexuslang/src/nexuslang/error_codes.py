"""
NLPL Error Codes Registry
=========================

Comprehensive error code system with categories, descriptions, and fixes.
Inspired by Rust's error codes.
"""

from typing import Dict, Optional, List, Type
from dataclasses import dataclass


@dataclass
class ErrorInfo:
    """Information about a specific error code."""
    code: str
    category: str
    title: str
    description: str
    common_causes: List[str]
    fixes: List[str]
    doc_link: Optional[str] = None
    
    def format_help(self) -> str:
        """Format complete help text for this error."""
        lines = [
            f"Error {self.code}: {self.title}",
            "=" * (len(f"Error {self.code}: {self.title}")),
            "",
            self.description,
            "",
        ]
        
        if self.common_causes:
            lines.append("Common causes:")
            for cause in self.common_causes:
                lines.append(f"  • {cause}")
            lines.append("")
        
        if self.fixes:
            lines.append("How to fix:")
            for fix in self.fixes:
                lines.append(f"  • {fix}")
            lines.append("")
        
        if self.doc_link:
            lines.append(f"Documentation: {self.doc_link}")
        
        return "\n".join(lines)


# Error code registry - organized by category
ERROR_CODES: Dict[str, ErrorInfo] = {
    # Syntax Errors (E001-E099)
    "E001": ErrorInfo(
        code="E001",
        category="syntax",
        title="Unexpected token",
        description="The parser encountered a token it didn't expect at this location.",
        common_causes=[
            "Missing keyword (like 'to' in 'set x to value')",
            "Extra or misplaced punctuation",
            "Typo in a keyword",
        ],
        fixes=[
            "Check the syntax for this statement type",
            "Look for missing or extra words around the error location",
            "Compare with examples in the documentation",
        ],
        doc_link="https://nlpl.dev/docs/syntax"
    ),
    
    "E002": ErrorInfo(
        code="E002",
        category="syntax",
        title="Missing 'end' keyword",
        description="A block (function, class, if, while, for) was not closed with 'end'.",
        common_causes=[
            "Forgot to add 'end' after a block",
            "Mismatched 'end' keywords",
            "Extra 'end' closing the wrong block",
        ],
        fixes=[
            "Add 'end' at the end of each block",
            "Check indentation to see which block is unclosed",
            "Use a code editor with bracket matching",
        ],
        doc_link="https://nlpl.dev/docs/syntax/blocks"
    ),
    
    "E003": ErrorInfo(
        code="E003",
        category="syntax",
        title="Invalid function definition",
        description="Function definition syntax is incorrect.",
        common_causes=[
            "Missing 'function' keyword",
            "Missing 'called' or 'with' after function name",
            "Invalid parameter syntax",
        ],
        fixes=[
            "Use: function name with param1 as Type, param2 as Type returns Type",
            "Or: function name called with no parameters returns Type",
            "Check parameter syntax: 'name as Type'",
        ],
        doc_link="https://nlpl.dev/docs/functions"
    ),
    
    "E004": ErrorInfo(
        code="E004",
        category="syntax",
        title="Invalid class definition",
        description="Class definition syntax is incorrect.",
        common_causes=[
            "Missing 'class' keyword",
            "Invalid property or method syntax",
            "Missing 'end' after class body",
        ],
        fixes=[
            "Use: class ClassName ... end",
            "Properties: property name as Type",
            "Methods: method name with params returns Type ... end",
        ],
        doc_link="https://nlpl.dev/docs/classes"
    ),
    
    "E005": ErrorInfo(
        code="E005",
        category="syntax",
        title="Invalid expression",
        description="The expression syntax is not valid.",
        common_causes=[
            "Missing operand in binary operation",
            "Invalid operator usage",
            "Unmatched parentheses or brackets",
        ],
        fixes=[
            "Check that all operators have both operands",
            "Ensure parentheses and brackets are balanced",
            "Verify operator precedence",
        ],
        doc_link="https://nlpl.dev/docs/expressions"
    ),
    
    # Name/Variable Errors (E100-E199)
    "E100": ErrorInfo(
        code="E100",
        category="name",
        title="Undefined variable",
        description="Attempted to use a variable that hasn't been defined.",
        common_causes=[
            "Typo in variable name",
            "Variable not declared with 'set'",
            "Variable out of scope",
        ],
        fixes=[
            "Declare variable first: set name to value",
            "Check spelling of variable name",
            "Ensure variable is in scope (not inside closed block)",
        ],
        doc_link="https://nlpl.dev/docs/variables"
    ),
    
    "E101": ErrorInfo(
        code="E101",
        category="name",
        title="Undefined function",
        description="Attempted to call a function that doesn't exist.",
        common_causes=[
            "Typo in function name",
            "Function not imported",
            "Function not defined yet",
        ],
        fixes=[
            "Define function before calling it",
            "Import function from module if needed",
            "Check function name spelling",
        ],
        doc_link="https://nlpl.dev/docs/functions"
    ),
    
    "E102": ErrorInfo(
        code="E102",
        category="name",
        title="Undefined class",
        description="Attempted to use a class that doesn't exist.",
        common_causes=[
            "Typo in class name",
            "Class not imported",
            "Class not defined yet",
        ],
        fixes=[
            "Define class before using it",
            "Import class from module if needed",
            "Check class name spelling (classes should be PascalCase)",
        ],
        doc_link="https://nlpl.dev/docs/classes"
    ),
    
    "E103": ErrorInfo(
        code="E103",
        category="name",
        title="Undefined attribute",
        description="Attempted to access an attribute that doesn't exist on this object.",
        common_causes=[
            "Typo in attribute name",
            "Attribute not defined in class",
            "Accessing wrong object",
        ],
        fixes=[
            "Check attribute name spelling",
            "Verify the object has this attribute",
            "Define the property/method in the class",
        ],
        doc_link="https://nlpl.dev/docs/classes"
    ),

    "E150": ErrorInfo(
        code="E150",
        category="name",
        title="Unused variable",
        description="Variable was declared but never used.",
        common_causes=[
            "Variable declared but program logic never reads it",
            "Variable intended for later use but forgotten",
            "Debugging leftover that was not cleaned up",
        ],
        fixes=[
            "Remove the unused variable declaration",
            "Use the variable in an expression or print statement",
        ],
        doc_link="https://nlpl.dev/docs/variables"
    ),

    # Type Errors (E200-E299)
    "E200": ErrorInfo(
        code="E200",
        category="type",
        title="Type mismatch",
        description="Operation attempted on incompatible types.",
        common_causes=[
            "Adding string to number",
            "Using wrong type in comparison",
            "Passing wrong type to function",
        ],
        fixes=[
            "Convert types explicitly if needed",
            "Check function parameter types",
            "Use type-compatible operations",
        ],
        doc_link="https://nlpl.dev/docs/types"
    ),
    
    "E201": ErrorInfo(
        code="E201",
        category="type",
        title="Invalid operation for type",
        description="This operation cannot be performed on this type.",
        common_causes=[
            "Arithmetic on non-numeric types",
            "Indexing a non-indexable type",
            "Calling non-callable object",
        ],
        fixes=[
            "Check the type supports this operation",
            "Use appropriate type conversion",
            "Use correct operators for the type",
        ],
        doc_link="https://nlpl.dev/docs/types/operations"
    ),
    
    "E202": ErrorInfo(
        code="E202",
        category="type",
        title="Wrong number of arguments",
        description="Function called with incorrect number of arguments.",
        common_causes=[
            "Missing required arguments",
            "Too many arguments provided",
            "Misunderstanding function signature",
        ],
        fixes=[
            "Check function definition for required parameters",
            "Provide all required arguments",
            "Remove extra arguments",
        ],
        doc_link="https://nlpl.dev/docs/functions/calls"
    ),
    
    # Runtime Errors (E300-E399)
    "E300": ErrorInfo(
        code="E300",
        category="runtime",
        title="Division by zero",
        description="Attempted to divide by zero.",
        common_causes=[
            "Divisor evaluates to zero",
            "Loop counter error",
            "Uninitialized variable used as divisor",
        ],
        fixes=[
            "Check divisor is not zero before dividing",
            "Add conditional: if divisor is not 0",
            "Initialize variables properly",
        ],
        doc_link="https://nlpl.dev/docs/arithmetic"
    ),
    
    "E301": ErrorInfo(
        code="E301",
        category="runtime",
        title="Index out of range",
        description="Attempted to access list/array element at invalid index.",
        common_causes=[
            "Index larger than list size",
            "Negative index on empty list",
            "Off-by-one error in loop",
        ],
        fixes=[
            "Check list length before indexing",
            "Use: if index < length of list",
            "Remember lists are 0-indexed",
        ],
        doc_link="https://nlpl.dev/docs/collections/lists"
    ),
    
    "E302": ErrorInfo(
        code="E302",
        category="runtime",
        title="Key not found in dictionary",
        description="Attempted to access dictionary key that doesn't exist.",
        common_causes=[
            "Typo in key name",
            "Key not added to dictionary yet",
            "Wrong dictionary being accessed",
        ],
        fixes=[
            "Check key exists: if key in dictionary",
            "Add key before accessing it",
            "Use get with default: dictionary.get(key, default)",
        ],
        doc_link="https://nlpl.dev/docs/collections/dictionaries"
    ),
    
    "E303": ErrorInfo(
        code="E303",
        category="runtime",
        title="Null pointer dereference",
        description="Attempted to dereference a null pointer.",
        common_causes=[
            "Pointer not initialized",
            "Freed memory accessed",
            "Null value dereferenced",
        ],
        fixes=[
            "Check pointer is not null before dereferencing",
            "Initialize pointers properly",
            "Don't access freed memory",
        ],
        doc_link="https://nlpl.dev/docs/pointers"
    ),
    
    # Import/Module Errors (E400-E499)
    "E400": ErrorInfo(
        code="E400",
        category="module",
        title="Module not found",
        description="Attempted to import a module that doesn't exist.",
        common_causes=[
            "Typo in module name",
            "Module file doesn't exist",
            "Wrong import path",
        ],
        fixes=[
            "Check module name spelling",
            "Verify module file exists",
            "Check import path is correct",
        ],
        doc_link="https://nlpl.dev/docs/modules"
    ),
    
    "E401": ErrorInfo(
        code="E401",
        category="module",
        title="Circular import",
        description="Modules import each other creating a circular dependency.",
        common_causes=[
            "Module A imports B, B imports A",
            "Indirect circular dependency chain",
            "Import at module level instead of inside function",
        ],
        fixes=[
            "Restructure code to avoid circular dependency",
            "Move import inside function if possible",
            "Extract shared code to separate module",
        ],
        doc_link="https://nlpl.dev/docs/modules/circular"
    ),
    
    "E402": ErrorInfo(
        code="E402",
        category="module",
        title="Import name not found",
        description="The name being imported doesn't exist in the module.",
        common_causes=[
            "Typo in imported name",
            "Name not exported from module",
            "Wrong module imported",
        ],
        fixes=[
            "Check the name exists in the module",
            "Verify spelling of imported name",
            "Check module documentation for available names",
        ],
        doc_link="https://nlpl.dev/docs/modules/imports"
    ),
    
    # Additional Runtime Errors (E304-E320)
    "E304": ErrorInfo(
        code="E304",
        category="runtime",
        title="Object has no attribute",
        description="Attempted to access an attribute that doesn't exist on the object.",
        common_causes=[
            "Typo in attribute name",
            "Attribute not defined in class",
            "Wrong object type",
        ],
        fixes=[
            "Check spelling of attribute name",
            "Verify the attribute is defined in the class",
            "Use hasattr() to check if attribute exists",
        ],
        doc_link="https://nlpl.dev/docs/objects/attributes"
    ),
    
    "E305": ErrorInfo(
        code="E305",
        category="runtime",
        title="Function call error",
        description="Error occurred while calling a function.",
        common_causes=[
            "Wrong number of arguments",
            "Argument type mismatch",
            "Function not callable",
        ],
        fixes=[
            "Check function signature for required parameters",
            "Verify argument types match function definition",
            "Ensure the object is actually a function",
        ],
        doc_link="https://nlpl.dev/docs/functions"
    ),
    
    "E306": ErrorInfo(
        code="E306",
        category="runtime",
        title="Invalid cast",
        description="Type conversion/cast failed.",
        common_causes=[
            "Incompatible types for conversion",
            "Value out of range for target type",
            "Invalid format for parsing",
        ],
        fixes=[
            "Check if conversion is valid for these types",
            "Validate value before conversion",
            "Use try-catch for risky conversions",
        ],
        doc_link="https://nlpl.dev/docs/types/conversion"
    ),
    
    "E307": ErrorInfo(
        code="E307",
        category="runtime",
        title="Memory allocation failed",
        description="Failed to allocate memory.",
        common_causes=[
            "Requested size too large",
            "System out of memory",
            "Invalid size parameter",
        ],
        fixes=[
            "Check available memory",
            "Reduce allocation size",
            "Free unused memory first",
        ],
        doc_link="https://nlpl.dev/docs/memory"
    ),
    
    "E308": ErrorInfo(
        code="E308",
        category="runtime",
        title="Invalid memory operation",
        description="Memory operation is invalid (accessing freed memory, invalid pointer, etc.).",
        common_causes=[
            "Using freed memory",
            "Null pointer dereference",
            "Invalid pointer arithmetic",
        ],
        fixes=[
            "Check pointer is not null before use",
            "Don't use memory after freeing",
            "Validate pointer operations",
        ],
        doc_link="https://nlpl.dev/docs/memory/safety"
    ),
    
    # Additional Type Errors (E203-E210)
    "E203": ErrorInfo(
        code="E203",
        category="type",
        title="Invalid generic type arguments",
        description="Generic type arguments are invalid.",
        common_causes=[
            "Wrong number of type arguments",
            "Type constraints not satisfied",
            "Invalid type parameter",
        ],
        fixes=[
            "Check the number of type parameters required",
            "Verify type constraints are met",
            "Use valid concrete types for generics",
        ],
        doc_link="https://nlpl.dev/docs/generics"
    ),
    
    "E204": ErrorInfo(
        code="E204",
        category="type",
        title="Type annotation error",
        description="Type annotation is invalid or inconsistent.",
        common_causes=[
            "Undefined type in annotation",
            "Inconsistent type annotations",
            "Invalid type syntax",
        ],
        fixes=[
            "Check type name is defined",
            "Ensure annotations are consistent",
            "Use correct type annotation syntax",
        ],
        doc_link="https://nlpl.dev/docs/types/annotations"
    ),

    "E205": ErrorInfo(
        code="E205",
        category="type",
        title="Data schema type mismatch",
        description="Structured data does not match the expected field or record types.",
        common_causes=[
            "CSV/JSON field contains incompatible value",
            "Record field type differs from declared schema",
            "Transform step changed type unexpectedly",
        ],
        fixes=[
            "Validate incoming data against schema before processing",
            "Normalize field types during import/transformation",
            "Update schema or conversion logic to match actual data",
        ],
        doc_link="https://nlpl.dev/docs/data-processing"
    ),

    "E206": ErrorInfo(
        code="E206",
        category="type",
        title="Numeric domain error",
        description="A numeric operation used a value outside the valid mathematical domain.",
        common_causes=[
            "Square root of a negative value",
            "Logarithm of a non-positive value",
            "Invalid numeric parameter range",
        ],
        fixes=[
            "Validate numeric ranges before computation",
            "Clamp or transform invalid input values",
            "Use domain-safe formulas or conditional handling",
        ],
        doc_link="https://nlpl.dev/docs/scientific"
    ),

    "E309": ErrorInfo(
        code="E309",
        category="runtime",
        title="General runtime error",
        description="An operation failed during execution without a more specific runtime category.",
        common_causes=[
            "Unexpected runtime state",
            "Unhandled edge case in operation",
            "External resource failure",
        ],
        fixes=[
            "Inspect the failing line and nearby operations",
            "Enable debug mode for additional context",
            "Add explicit validation before this operation",
        ],
        doc_link="https://nlpl.dev/docs/runtime"
    ),

    "E410": ErrorInfo(
        code="E410",
        category="module",
        title="Network request failed",
        description="A network call failed before receiving a usable response.",
        common_causes=[
            "Target service is unavailable",
            "Connection timeout",
            "Invalid endpoint or DNS resolution issue",
        ],
        fixes=[
            "Check endpoint URL and network connectivity",
            "Retry with timeout/backoff strategy",
            "Handle transient failures with fallback logic",
        ],
        doc_link="https://nlpl.dev/docs/network"
    ),

    "E411": ErrorInfo(
        code="E411",
        category="module",
        title="Invalid HTTP response",
        description="An HTTP response was received but could not be processed as expected.",
        common_causes=[
            "Unexpected status code",
            "Malformed response body",
            "Mismatched content type",
        ],
        fixes=[
            "Validate status code before parsing response",
            "Check content type and response schema",
            "Add defensive parsing with clear fallbacks",
        ],
        doc_link="https://nlpl.dev/docs/web-services"
    ),

    "E412": ErrorInfo(
        code="E412",
        category="module",
        title="Database connection failed",
        description="Could not establish or maintain a database connection.",
        common_causes=[
            "Invalid connection string or credentials",
            "Database server unavailable",
            "Connection pool exhaustion",
        ],
        fixes=[
            "Verify credentials and connection settings",
            "Check database server health and network access",
            "Tune connection pool limits and retry policy",
        ],
        doc_link="https://nlpl.dev/docs/business-applications"
    ),

    "E413": ErrorInfo(
        code="E413",
        category="module",
        title="Transaction conflict",
        description="Concurrent operations produced a transaction conflict.",
        common_causes=[
            "Simultaneous writes to same record",
            "Isolation level mismatch",
            "Long-running transaction contention",
        ],
        fixes=[
            "Retry conflicted transaction with backoff",
            "Reduce transaction scope and duration",
            "Use appropriate isolation/locking strategy",
        ],
        doc_link="https://nlpl.dev/docs/business-applications"
    ),
}


def get_error_info(code: str) -> Optional[ErrorInfo]:
    """Get error information by code."""
    return ERROR_CODES.get(code)


def get_error_code_for_type(error_type: str, context: dict = None) -> Optional[str]:
    """
    Map error type to error code.
    
    Args:
        error_type: Type of error (e.g., "undefined_variable")
        context: Optional context for disambiguation
    
    Returns:
        Error code or None
    """
    mapping = {
        # Syntax
        "unexpected_token": "E001",
        "missing_end": "E002",
        "invalid_function": "E003",
        "invalid_class": "E004",
        "invalid_expression": "E005",
        
        # Names
        "undefined_variable": "E100",
        "undefined_function": "E101",
        "undefined_class": "E102",
        "undefined_attribute": "E103",
        "unused_variable": "E150",
        
        # Types
        "type_mismatch": "E200",
        "invalid_operation": "E201",
        "wrong_argument_count": "E202",
        "invalid_generic_args": "E203",
        "type_annotation_error": "E204",
        "data_schema_mismatch": "E205",
        "numeric_domain_error": "E206",
        
        # Runtime
        "division_by_zero": "E300",
        "index_out_of_range": "E301",
        "key_not_found": "E302",
        "null_pointer": "E303",
        "no_attribute": "E304",
        "function_call_error": "E305",
        "invalid_cast": "E306",
        "memory_allocation_failed": "E307",
        "invalid_memory_operation": "E308",
        "runtime_error": "E309",
        
        # Modules
        "module_not_found": "E400",
        "circular_import": "E401",
        "import_name_not_found": "E402",
        "network_request_failed": "E410",
        "invalid_http_response": "E411",
        "database_connection_failed": "E412",
        "transaction_conflict": "E413",
    }
    
    return mapping.get(error_type)


def list_all_error_codes() -> List[str]:
    """Get list of all error codes."""
    return sorted(ERROR_CODES.keys())


def search_errors(query: str) -> List[ErrorInfo]:
    """Search for errors matching query."""
    query = query.lower()
    results = []
    
    for error_info in ERROR_CODES.values():
        if (query in error_info.title.lower() or 
            query in error_info.description.lower() or
            query in error_info.code.lower()):
            results.append(error_info)
    
    return results
