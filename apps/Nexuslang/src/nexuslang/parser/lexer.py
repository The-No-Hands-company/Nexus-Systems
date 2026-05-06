"""
Lexer for NexusLang.
Converts source code into tokens using natural English-like patterns.
"""

from enum import Enum, auto
from dataclasses import dataclass
from typing import Dict, Optional, List, Any

class TokenType(Enum):
    """Token types for NexusLang with natural English-like keywords."""
    # Keywords
    CLASS = auto()
    FUNCTION = auto()
    PRIVATE = auto()
    PUBLIC = auto()
    PROTECTED = auto()
    RETURNS = auto()
    RETURN = auto()
    YIELD = auto()
    NOTHING = auto()
    THIS = auto()
    NEW = auto()
    CREATE = auto()
    SET = auto()
    TO = auto()
    DEFINE = auto()
    CALLED = auto()
    THAT = auto()
    HAS = auto()
    TAKES = auto()
    METHOD = auto()
    PROPERTY = auto()
    END = auto()
    THE = auto()
    A = auto()
    AN = auto()
    PROPERTIES = auto()
    METHODS = auto()
    END_CLASS = auto()
    END_THE_CLASS = auto()
    END_METHOD = auto()
    END_THE_METHOD = auto()
    END_TRAIT = auto()
    END_THE_TRAIT = auto()
    END_INTERFACE = auto()
    END_THE_INTERFACE = auto()
    EXPORT = auto()
    END_IF = auto()
    END_WHILE = auto()
    END_LOOP = auto()
    END_CONCURRENT = auto()
    END_TRY = auto()
    REPEAT = auto()
    ALLOCATE = auto()
    DEALLOCATE = auto()
    FREE = auto()
    MEMORY = auto()
    RUN = auto()
    IMPORT = auto()
    TRAIT = auto()
    TYPE = auto()
    
    # FFI (Foreign Function Interface)
    EXTERN = auto()
    FOREIGN = auto()
    LIBRARY = auto()
    CDECL = auto()
    STDCALL = auto()
    CALLBACK = auto()
    LAMBDA = auto()
    UNSAFE = auto()    # Explicit unsafe FFI block marker
    
    # Macros
    MACRO = auto()
    EXPAND = auto()
    
    # Struct/Union types (low-level data structures)
    STRUCT = auto()
    UNION = auto()
    ENUM = auto()
    PACKED = auto()
    ALIGN = auto()
    OFFSETOF = auto()
    OPAQUE = auto()  # For opaque pointer types
    
    # Generic types and type construction
    OF = auto()  # Used in "create list of Integer", "array of 10 bytes"
    
    # Smart pointers and memory management
    RC = auto()  # Reference counted smart pointer (Rc<T>)
    WEAK = auto()  # Weak reference (Weak<T>)
    ARC = auto()  # Atomic reference counted (Arc<T>) - for threading
    
    # Natural language operators
    PLUS = auto()
    MINUS = auto()
    TIMES = auto()
    DIVIDED_BY = auto()
    MODULO = auto()
    POWER = auto()  # ** or 'to the power of'
    FLOOR_DIVIDE = auto()  # // or 'divided by (rounded down)'
    CONCATENATE = auto()  # text concatenate
    ADD = auto()  # add X to Y
    IS = auto()
    GREATER_THAN = auto()
    LESS_THAN = auto()
    EQUAL_TO = auto()
    NOT_EQUAL_TO = auto()
    GREATER_THAN_OR_EQUAL_TO = auto()
    LESS_THAN_OR_EQUAL_TO = auto()
    AND = auto()
    OR = auto()
    NOT = auto()
    
    # Bitwise operators
    BITWISE_AND = auto()  # & or 'bitwise and'
    BITWISE_OR = auto()   # | or 'bitwise or'
    BITWISE_XOR = auto()  # ^ or 'bitwise xor'
    BITWISE_NOT = auto()  # ~ or 'bitwise not'
    LEFT_SHIFT = auto()   # << or 'shift left'
    RIGHT_SHIFT = auto()  # >> or 'shift right'
    
    # Pointer operators (low-level memory operations)
    ADDRESS_OF = auto()   # & or 'address of'
    DEREFERENCE = auto()  # * or 'dereference' or 'value at'
    ARROW_OP = auto()     # -> for pointer member access
    SIZEOF = auto()       # sizeof operator for memory size
    DOWNGRADE = auto()    # downgrade Rc to Weak (break cycles)
    UPGRADE = auto()      # upgrade Weak to Rc (safe access)
    MOVE = auto()         # move semantics: transfer ownership
    BORROW = auto()       # borrow semantics: temporary reference
    DROP = auto()         # drop a borrow or value explicitly
    LIFETIME = auto()     # lifetime annotation keyword ('with lifetime <name>')
    ALLOCATOR = auto()    # allocator hint keyword ('with allocator <name>')
    PARALLEL = auto()     # parallel execution keyword ('parallel for each ...')

    # Control flow
    IF = auto()
    ELSE = auto()
    ELSE_IF = auto()
    WHILE = auto()
    FOR = auto()
    FOR_EACH = auto()
    IN = auto()
    RANGE = auto()
    FROM = auto()
    DO = auto()
    BREAK = auto()
    CONTINUE = auto()
    SWITCH = auto()
    CASE = auto()
    DEFAULT = auto()
    FALLTHROUGH = auto()  # Switch case fallthrough
    MATCH = auto()  # Pattern matching
    WHEN = auto()   # Guard condition in pattern matching
    LABEL = auto()  # Loop label declaration
    
    # Error handling
    TRY = auto()
    CATCH = auto()
    RAISE = auto()
    WITH = auto()
    MESSAGE = auto()
    PANIC = auto()
    
    # Async/Await (concurrency)
    ASYNC = auto()
    AWAIT = auto()
    
    # I/O and data manipulation
    PRINT = auto()
    TEXT = auto()
    NUMBER = auto()
    CONVERT = auto()
    AS = auto()
    INTO = auto()
    READ = auto()
    WRITE = auto()
    APPEND = auto()
    JOIN = auto()
    LENGTH = auto()
    EMPTY = auto()
    CONTAINS = auto()
    
    # Database operations
    DATABASE = auto()
    CONNECT = auto()
    DISCONNECT = auto()
    QUERY = auto()
    EXECUTE = auto()
    INSERT = auto()
    UPDATE = auto()
    DELETE = auto()
    SELECT = auto()
    WHERE = auto()
    
    # Network operations
    NETWORK = auto()
    CHANNEL = auto()
    SEND = auto()
    RECEIVE = auto()
    REQUEST = auto()
    RESPONSE = auto()
    HTTP = auto()
    WEBSOCKET = auto()
    CONNECT_TO = auto()
    DISCONNECT_FROM = auto()
    
    # File operations
    FILE = auto()
    OPEN = auto()
    CLOSE = auto()
    EXISTS = auto()
    DIRECTORY = auto()
    PATH = auto()
    
    # Types
    INTEGER = auto()
    FLOAT = auto()
    STRING = auto()
    BOOLEAN = auto()
    NULL = auto()
    LIST = auto()
    DICTIONARY = auto()
    ARRAY = auto()
    OBJECT = auto()
    POINTER = auto()
    REFERENCE = auto()
    
    # Range operators
    RANGE_INCLUSIVE = auto()  # ..=
    
    # Error propagation
    QUESTION = auto()  # ? operator for Result unwrapping
    
    # Decorators
    AT = auto()  # @ symbol for decorators

    # Test framework
    TEST = auto()        # `test "name" do ... end`
    DESCRIBE = auto()   # `describe "suite" do ... end`
    IT = auto()          # `it "behaviour" do ... end`
    EXPECT = auto()     # `expect value to equal ...`
    BEFORE_EACH = auto()  # `before each do ... end`
    AFTER_EACH = auto()   # `after each do ... end`

    # Contract programming
    REQUIRE = auto()     # `require condition`
    ENSURE = auto()      # `ensure condition`
    GUARANTEE = auto()   # `guarantee condition`
    INVARIANT = auto()   # `invariant condition` (class/scope invariant)
    OLD = auto()         # `old(expr)` (pre-call value capture in postconditions)
    SPEC = auto()        # `spec` block (formal specification annotation)
    COMPTIME = auto()    # `comptime` compile-time evaluation keyword
    ATTRIBUTE = auto()   # `attribute` declaration keyword
    OTHERWISE = auto()   # `otherwise` null-coalescing operator
    OPERATOR = auto()    # `operator` method definition keyword

    # Literals
    IDENTIFIER = auto()
    INTEGER_LITERAL = auto()
    FLOAT_LITERAL = auto()
    STRING_LITERAL = auto()
    FSTRING_LITERAL = auto()  # f"..." strings with interpolation
    TRUE = auto()
    FALSE = auto()
    
    # Operators and punctuation
    LEFT_PAREN = auto()
    RIGHT_PAREN = auto()
    LEFT_BRACE = auto()
    RIGHT_BRACE = auto()
    LEFT_BRACKET = auto()
    RIGHT_BRACKET = auto()
    COMMA = auto()
    DOT = auto()
    COLON = auto()
    DOUBLE_COLON = auto()  # :: for kind annotations (HKT)
    SEMICOLON = auto()
    ARROW = auto()
    EQUALS = auto()  # = for enum values and assignments
    
    # Special tokens
    EOF = auto()
    ERROR = auto()
    INDENT = auto()  # Indentation increase
    DEDENT = auto()  # Indentation decrease
    NEWLINE = auto()  # Significant newline (not just whitespace)
    
    # Contextual keywords (can be used as variable names)
    VALUE = auto()
    NAME = auto()
    DATA = auto()
    INFO = auto()
    STATUS = auto()
    
    # New token types
    EXTENDS = auto()
    INHERITS = auto()
    IMPLEMENTS = auto()
    INTERFACE = auto()
    GENERIC = auto()
    
    # Inline assembly
    INLINE = auto()
    ASSEMBLY = auto()
    ASM = auto()  # Short form
    
    # Variadic functions
    ELLIPSIS = auto()  # ... for variadic parameters

    # Documentation
    DOC_COMMENT = auto()  # ## doc comment (## text after the double-hash)

    # Backward-compatibility aliases (used by older tests / external code)
    INTEGER_TYPE = INTEGER        # type: ignore[assignment]
    FLOAT_TYPE = FLOAT            # type: ignore[assignment]
    STRING_TYPE = STRING          # type: ignore[assignment]
    BOOLEAN_TYPE = BOOLEAN        # type: ignore[assignment]
    NULL_TYPE = NULL              # type: ignore[assignment]
    LIST_TYPE = LIST              # type: ignore[assignment]
    DICTIONARY_TYPE = DICTIONARY  # type: ignore[assignment]
    MULTIPLY = TIMES              # type: ignore[assignment]
    EQUALS_EQUALS = EQUAL_TO      # type: ignore[assignment]
    NOT_EQUALS = NOT_EQUAL_TO     # type: ignore[assignment]
    LESS_THAN_EQUALS = LESS_THAN_OR_EQUAL_TO     # type: ignore[assignment]
    GREATER_THAN_EQUALS = GREATER_THAN_OR_EQUAL_TO  # type: ignore[assignment]
    LPAREN = LEFT_PAREN          # type: ignore[assignment]
    RPAREN = RIGHT_PAREN         # type: ignore[assignment]

@dataclass
class Token:
    """Represents a token in the source code."""
    type: TokenType
    lexeme: str
    literal: Optional[Any]
    line: int
    column: int
    source_line: Optional[str] = None  # Store the full source line for error reporting

    @property
    def value(self) -> Any:
        """Return the meaningful value: literal for literal tokens, lexeme otherwise."""
        return self.literal if self.literal is not None else self.lexeme

class Lexer:
    """Lexer for NexusLang that recognizes natural English-like patterns."""

    def _build_keywords(self) -> Dict[str, TokenType]:
        """Return the complete keywords mapping for NexusLang."""
        return {
            # Class and function related
            "class": TokenType.CLASS,
            "function": TokenType.FUNCTION,
            "private": TokenType.PRIVATE,
            "public": TokenType.PUBLIC,
            "protected": TokenType.PROTECTED,
            "returns": TokenType.RETURNS,
            "return": TokenType.RETURN,
            "yield": TokenType.YIELD,
            "nothing": TokenType.NOTHING,
            "this": TokenType.THIS,
            "new": TokenType.NEW,
            "create": TokenType.CREATE,
            "set": TokenType.SET,
            "to": TokenType.TO,
            "define": TokenType.DEFINE,
            "called": TokenType.CALLED,
            "that": TokenType.THAT,
            "has": TokenType.HAS,
            "takes": TokenType.TAKES,
            "method": TokenType.METHOD,
            "property": TokenType.PROPERTY,
            "end": TokenType.END,
            "the": TokenType.THE,
            "a": TokenType.A,
            "an": TokenType.AN,
            "properties": TokenType.PROPERTIES,
            "methods": TokenType.METHODS,
            "end class": TokenType.END_CLASS,
            "end the class": TokenType.END_THE_CLASS,
            "end method": TokenType.END_METHOD,
            "end the method": TokenType.END_THE_METHOD,
            "end trait": TokenType.END_TRAIT,
            "end the trait": TokenType.END_THE_TRAIT,
            "end interface": TokenType.END_INTERFACE,
            "end the interface": TokenType.END_THE_INTERFACE,
            "end if": TokenType.END_IF,
            "end while": TokenType.END_WHILE,
            "end loop": TokenType.END_LOOP,
            "end concurrent": TokenType.END_CONCURRENT,
            "end try": TokenType.END_TRY,
            "repeat": TokenType.REPEAT,
            "allocate": TokenType.ALLOCATE,
            "deallocate": TokenType.DEALLOCATE,
            "free": TokenType.FREE,
            "memory": TokenType.MEMORY,
            "run": TokenType.RUN,
            "import": TokenType.IMPORT,
            "trait": TokenType.TRAIT,
            "type": TokenType.TYPE,

            # FFI keywords
            "extern": TokenType.EXTERN,
            "external": TokenType.EXTERN,
            "foreign": TokenType.FOREIGN,
            "library": TokenType.LIBRARY,
            "cdecl": TokenType.CDECL,
            "stdcall": TokenType.STDCALL,
            "callback": TokenType.CALLBACK,
            "lambda": TokenType.LAMBDA,
            "unsafe": TokenType.UNSAFE,

            # Macros
            "macro": TokenType.MACRO,
            "expand": TokenType.EXPAND,

            # Struct/Union/Enum keywords
            "struct": TokenType.STRUCT,
            "structure": TokenType.STRUCT,
            "union": TokenType.UNION,
            "enum": TokenType.ENUM,
            "enumeration": TokenType.ENUM,
            "packed": TokenType.PACKED,
            "align": TokenType.ALIGN,
            "aligned": TokenType.ALIGN,
            "offsetof": TokenType.OFFSETOF,
            "offset of": TokenType.OFFSETOF,
            "opaque": TokenType.OPAQUE,

            # Smart pointers
            "rc": TokenType.RC,
            "weak": TokenType.WEAK,
            "arc": TokenType.ARC,

            # Natural language operators
            "plus": TokenType.PLUS,
            "minus": TokenType.MINUS,
            "negative": TokenType.MINUS,  # natural language unary minus alias
            "times": TokenType.TIMES,
            "divided by": TokenType.DIVIDED_BY,
            "integer divided by": TokenType.FLOOR_DIVIDE,
            "modulo": TokenType.MODULO,
            "to the power of": TokenType.POWER,
            "power": TokenType.POWER,
            "concatenate": TokenType.CONCATENATE,
            "add": TokenType.ADD,
            "is": TokenType.IS,
            "equals": TokenType.EQUAL_TO,  # Single-word comparison
            "greater than": TokenType.GREATER_THAN,
            "less than": TokenType.LESS_THAN,
            "equal to": TokenType.EQUAL_TO,
            "not equal to": TokenType.NOT_EQUAL_TO,
            "greater than or equal to": TokenType.GREATER_THAN_OR_EQUAL_TO,
            "less than or equal to": TokenType.LESS_THAN_OR_EQUAL_TO,
            "and": TokenType.AND,
            "or": TokenType.OR,
            "not": TokenType.NOT,

            # Bitwise operators (natural language)
            "bitwise and": TokenType.BITWISE_AND,
            "bitwise or": TokenType.BITWISE_OR,
            "bitwise xor": TokenType.BITWISE_XOR,
            "bitwise not": TokenType.BITWISE_NOT,
            "shift left": TokenType.LEFT_SHIFT,
            "shift right": TokenType.RIGHT_SHIFT,

            # Pointer operations (natural language)
            "address of": TokenType.ADDRESS_OF,
            "dereference": TokenType.DEREFERENCE,
            "value at": TokenType.DEREFERENCE,
            "sizeof": TokenType.SIZEOF,
            "size of": TokenType.SIZEOF,

            # Smart pointer operations (Rc/Weak)
            "downgrade": TokenType.DOWNGRADE,
            "upgrade": TokenType.UPGRADE,

            # Ownership / borrow keywords
            "move": TokenType.MOVE,
            "borrow": TokenType.BORROW,
            "drop": TokenType.DROP,
            "lifetime": TokenType.LIFETIME,
            "allocator": TokenType.ALLOCATOR,
            "parallel": TokenType.PARALLEL,

            # Control flow
            "if": TokenType.IF,
            "else": TokenType.ELSE,
            "else if": TokenType.ELSE_IF,
            "while": TokenType.WHILE,
            "for": TokenType.FOR,
            "for each": TokenType.FOR_EACH,
            "in": TokenType.IN,
            "range": TokenType.RANGE,
            "from": TokenType.FROM,
            "to": TokenType.TO,
            "do": TokenType.DO,
            "break": TokenType.BREAK,
            "continue": TokenType.CONTINUE,
            "switch": TokenType.SWITCH,
            "case": TokenType.CASE,
            "default": TokenType.DEFAULT,
            "fallthrough": TokenType.FALLTHROUGH,
            "fall through": TokenType.FALLTHROUGH,
            "match": TokenType.MATCH,
            "when": TokenType.WHEN,  # Guard condition in pattern matching
            "label": TokenType.LABEL,

            # Error handling
            "try": TokenType.TRY,
            "catch": TokenType.CATCH,
            "raise": TokenType.RAISE,
            "error": TokenType.ERROR,
            "with": TokenType.WITH,
            "message": TokenType.MESSAGE,
            "panic": TokenType.PANIC,
            "of": TokenType.OF,  # For "create list of Integer", "array of 10 bytes"

            # Async/Await
            "async": TokenType.ASYNC,
            "asynchronous": TokenType.ASYNC,
            "await": TokenType.AWAIT,
            "wait": TokenType.AWAIT,  # More natural alias

            # Contextual keywords (can be used as variable names in certain contexts)
            "value": TokenType.VALUE,
            "name": TokenType.NAME,
            "data": TokenType.DATA,
            "info": TokenType.INFO,
            "status": TokenType.STATUS,

            # I/O and data manipulation
            "print": TokenType.PRINT,
            "text": TokenType.TEXT,
            "number": TokenType.NUMBER,
            "convert": TokenType.CONVERT,
            "as": TokenType.AS,
            "from": TokenType.FROM,
            "into": TokenType.INTO,
            "read": TokenType.READ,
            "write": TokenType.WRITE,
            "append": TokenType.APPEND,
            "join": TokenType.JOIN,
            "length": TokenType.LENGTH,
            "empty": TokenType.EMPTY,
            "contains": TokenType.CONTAINS,

            # Database operations
            "export": TokenType.EXPORT,
            "database": TokenType.DATABASE,
            "connect": TokenType.CONNECT,
            "disconnect": TokenType.DISCONNECT,
            "query": TokenType.QUERY,
            "execute": TokenType.EXECUTE,
            "insert": TokenType.INSERT,
            "update": TokenType.UPDATE,
            "delete": TokenType.DELETE,
            "select": TokenType.SELECT,
            "where": TokenType.WHERE,

            # Network operations
            "network": TokenType.NETWORK,
            "channel": TokenType.CHANNEL,
            "send": TokenType.SEND,
            "receive": TokenType.RECEIVE,
            "request": TokenType.REQUEST,
            "response": TokenType.RESPONSE,
            "http": TokenType.HTTP,
            "websocket": TokenType.WEBSOCKET,
            "connect to": TokenType.CONNECT_TO,
            "disconnect from": TokenType.DISCONNECT_FROM,

            # File operations
            "file": TokenType.FILE,
            "open": TokenType.OPEN,
            "close": TokenType.CLOSE,
            "exists": TokenType.EXISTS,
            "directory": TokenType.DIRECTORY,
            "path": TokenType.PATH,

            # Types
            "integer": TokenType.INTEGER,
            "float": TokenType.FLOAT,
            "string": TokenType.STRING,
            "boolean": TokenType.BOOLEAN,
            "null": TokenType.NULL,
            "list": TokenType.LIST,
            "dictionary": TokenType.DICTIONARY,
            "array": TokenType.ARRAY,
            "object": TokenType.OBJECT,
            "pointer": TokenType.POINTER,
            "reference": TokenType.REFERENCE,

            # Literals
            "true": TokenType.TRUE,
            "false": TokenType.FALSE,

            # New keywords
            "extends": TokenType.EXTENDS,
            "inherits": TokenType.INHERITS,
            "implements": TokenType.IMPLEMENTS,
            "interface": TokenType.INTERFACE,
            "generic": TokenType.GENERIC,

            # Inline assembly
            "inline": TokenType.INLINE,
            "assembly": TokenType.ASSEMBLY,
            "asm": TokenType.ASM,
            "inline assembly": TokenType.INLINE,

            # Test framework
            "test": TokenType.TEST,
            "describe": TokenType.DESCRIBE,
            "it": TokenType.IT,
            "expect": TokenType.EXPECT,
            "before each": TokenType.BEFORE_EACH,
            "after each": TokenType.AFTER_EACH,

            # Contract programming
            "require": TokenType.REQUIRE,
            "ensure": TokenType.ENSURE,
            "guarantee": TokenType.GUARANTEE,
            "invariant": TokenType.INVARIANT,
            "old": TokenType.OLD,
            "spec": TokenType.SPEC,
            "comptime": TokenType.COMPTIME,
            "attribute": TokenType.ATTRIBUTE,
            "otherwise": TokenType.OTHERWISE,
            "operator": TokenType.OPERATOR,
        }

    def __init__(self, source: str):
        self.source = source
        self.tokens: List[Token] = []
        self.start = 0
        self.current = 0
        self.line = 1
        self.column = 1

        # Split source into lines for error reporting
        self.source_lines = source.split('\n')

        # Indentation tracking for Python-style blocks
        self.indent_stack: List[int] = [0]  # Stack of indentation levels
        self.at_line_start = True  # Track if we're at the start of a line
        self.pending_dedents = 0  # Number of DEDENT tokens to emit

        # Keywords mapping with natural English-like patterns
        self.keywords: Dict[str, TokenType] = self._build_keywords()
    
    def scan_tokens(self) -> List[Token]:
        """Scan the source code and return a list of tokens with indentation handling."""
        while not self.is_at_end():
            self.start = self.current
            self.scan_token()
        
        # Emit any remaining DEDENTs at end of file
        while len(self.indent_stack) > 1:
            self.indent_stack.pop()
            self.tokens.append(Token(TokenType.DEDENT, "", None, self.line, self.column))
        
        self.tokens.append(Token(TokenType.EOF, "", None, self.line, self.column))
        return self.tokens
    
    def tokenize(self) -> List[Token]:
        """Alias for scan_tokens to maintain compatibility with tests."""
        return self.scan_tokens()

    def get_next_token(self) -> 'Token':
        """Return the next meaningful token (skips INDENT/DEDENT), for streaming API.

        On first call, tokenizes the entire source. Subsequent calls return
        successive tokens. Returns EOF when exhausted.
        """
        # Tokenize lazily on first call
        if not self.tokens:
            self.scan_tokens()

        # Initialise streaming index if not present
        if not hasattr(self, '_stream_idx'):
            self._stream_idx = 0

        # Skip INDENT / DEDENT / NEWLINE tokens (structural, not content)
        skip_types = {TokenType.INDENT, TokenType.DEDENT, TokenType.NEWLINE}
        while self._stream_idx < len(self.tokens) and self.tokens[self._stream_idx].type in skip_types:
            self._stream_idx += 1

        if self._stream_idx >= len(self.tokens):
            return self.tokens[-1]  # Return EOF

        token = self.tokens[self._stream_idx]
        self._stream_idx += 1
        return token

    def scan_token(self) -> None:
        """Scan a single token with indentation tracking."""
        if self._handle_line_start():
            return
        
        c = self.advance()
        
        if self._handle_whitespace_and_newlines(c):
            return
        
        if self._handle_comments(c):
            return
        
        if self._handle_literals(c):
            return
        
        self._handle_operators_and_punctuation(c)

    def _handle_line_start(self) -> bool:
        """Handle indentation at the start of a line. Returns True if we should return early."""
        if not self.at_line_start:
            return False
        
        self.handle_indentation()
        self.at_line_start = False
        self.start = self.current
        
        return self.is_at_end() or self.peek() == '\n'

    def _handle_whitespace_and_newlines(self, c: str) -> bool:
        """Handle newlines and whitespace. Returns True if handled."""
        if c == '\n':
            if self.tokens and self.tokens[-1].type not in (
                TokenType.NEWLINE, TokenType.INDENT, TokenType.DEDENT
            ):
                self.add_token(TokenType.NEWLINE)
            self.line += 1
            self.column = 1
            self.at_line_start = True
            return True
        
        if c.isspace():
            self.column += 1
            return True
        
        return False

    def _handle_comments(self, c: str) -> bool:
        """Handle comments (both doc comments and regular). Returns True if handled."""
        if c != '#':
            return False
        
        if self.peek() == '#':
            # Documentation comment (##)
            self.advance()  # consume the second '#'
            if self.peek() == ' ':
                self.advance()
            doc_start = self.current
            while self.peek() != '\n' and not self.is_at_end():
                self.advance()
            doc_text = self.source[doc_start:self.current]
            self.add_token(TokenType.DOC_COMMENT, doc_text)
        else:
            # Regular comment
            while self.peek() != '\n' and not self.is_at_end():
                self.advance()
        return True

    def _handle_literals(self, c: str) -> bool:
        """Handle literals (identifiers, numbers, strings). Returns True if handled."""
        if c.isalpha() or c == '_':
            if c == 'f' and self.peek() == '"':
                self.advance()  # consume the '"'
                self.fstring()
            else:
                self.identifier()
            return True
        
        if c.isdigit():
            self.number()
            return True
        
        if c == '"':
            self.string()
            return True
        
        if c == "'":
            self.single_quote_string()
            return True
        
        return False

    def _handle_operators_and_punctuation(self, c: str) -> None:
        """Handle operators and punctuation characters."""
        # Parentheses and brackets
        if c == '(':
            self.add_token(TokenType.LEFT_PAREN)
        elif c == ')':
            self.add_token(TokenType.RIGHT_PAREN)
        elif c == '{':
            self.add_token(TokenType.LEFT_BRACE)
        elif c == '}':
            self.add_token(TokenType.RIGHT_BRACE)
        elif c == '[':
            self.add_token(TokenType.LEFT_BRACKET)
        elif c == ']':
            self.add_token(TokenType.RIGHT_BRACKET)
        
        # Basic punctuation
        elif c == ',':
            self.add_token(TokenType.COMMA)
        elif c == ';':
            self.add_token(TokenType.SEMICOLON)
        elif c == '@':
            self.add_token(TokenType.AT)
        
        # Dot and ellipsis
        elif c == '.':
            if self.peek() == '.' and self.peek_next() == '.':
                self.advance()
                self.advance()
                self.add_token(TokenType.ELLIPSIS)
            else:
                self.add_token(TokenType.DOT)
        
        # Colon
        elif c == ':':
            if self.match(':'):
                self.add_token(TokenType.DOUBLE_COLON)
            else:
                self.add_token(TokenType.COLON)
        
        # Arithmetic operators
        elif c == '+':
            self.add_token(TokenType.PLUS)
        elif c == '-':
            if self.match('>'):
                self.add_token(TokenType.ARROW)
            else:
                self.add_token(TokenType.MINUS)
        elif c == '*':
            if self.match('*'):
                self.add_token(TokenType.POWER)
            else:
                self.add_token(TokenType.TIMES)
        elif c == '/':
            if self.match('/'):
                self.add_token(TokenType.FLOOR_DIVIDE)
            else:
                self.add_token(TokenType.DIVIDED_BY)
        elif c == '%':
            self.add_token(TokenType.MODULO)
        
        # Bitwise operators
        elif c == '&':
            self.add_token(TokenType.BITWISE_AND)
        elif c == '|':
            self.add_token(TokenType.BITWISE_OR)
        elif c == '^':
            self.add_token(TokenType.BITWISE_XOR)
        elif c == '~':
            self.add_token(TokenType.BITWISE_NOT)
        
        # Comparison operators
        elif c == '<':
            if self.match('<'):
                self.add_token(TokenType.LEFT_SHIFT)
            elif self.match('='):
                self.add_token(TokenType.LESS_THAN_OR_EQUAL_TO)
            else:
                self.add_token(TokenType.LESS_THAN)
        elif c == '>':
            if self.match('>'):
                self.add_token(TokenType.RIGHT_SHIFT)
            elif self.match('='):
                self.add_token(TokenType.GREATER_THAN_OR_EQUAL_TO)
            else:
                self.add_token(TokenType.GREATER_THAN)
        elif c == '=':
            if self.match('='):
                self.add_token(TokenType.EQUAL_TO)
            else:
                self.add_token(TokenType.EQUALS)
        elif c == '!':
            if self.match('='):
                self.add_token(TokenType.NOT_EQUAL_TO)
            else:
                self.add_token(TokenType.NOT)
        
        else:
            self.error(f"Unexpected character '{c}'")
    
    def handle_indentation(self) -> None:
        """Handle indentation at the start of a line."""
        # Count leading whitespace
        indent_count = 0
        while not self.is_at_end() and self.peek() in ' \t':
            if self.peek() == ' ':
                indent_count += 1
            elif self.peek() == '\t':
                indent_count += 4  # Treat tab as 4 spaces
            self.advance()
        
        # Skip blank lines and lines with only comments
        if self.is_at_end() or self.peek() == '\n' or self.peek() == '#':
            return
        
        current_indent = self.indent_stack[-1]
        
        if indent_count > current_indent:
            # Indentation increased - emit INDENT
            self.indent_stack.append(indent_count)
            self.tokens.append(Token(TokenType.INDENT, "", None, self.line, 1))
        elif indent_count < current_indent:
            # Indentation decreased - emit DEDENT(s)
            while len(self.indent_stack) > 1 and self.indent_stack[-1] > indent_count:
                self.indent_stack.pop()
                self.tokens.append(Token(TokenType.DEDENT, "", None, self.line, 1))
            
            # Check for indentation error (indent doesn't match any previous level)
            if self.indent_stack[-1] != indent_count:
                self.error(f"Indentation error: inconsistent indentation level")
    
    def identifier(self) -> None:
        """Scan an identifier, including multi-word keywords."""
        # Collect all characters that can be part of an identifier
        start_column = self.column
        while not self.is_at_end() and (self.peek().isalnum() or self.peek() == '_'):
            self.advance()
            
        # Get the first word
        text = self.source[self.start:self.current].strip().lower()
        
        # Try to match multi-word keywords by lookahead - find LONGEST match
        saved_current = self.current
        saved_column = self.column
        
        # Build a potential multi-word keyword by looking ahead
        longest_match = text if text in self.keywords else None
        longest_position = self.current
        longest_column = self.column
        lookahead_text = text
        
        # Greedy matching: keep extending as long as we might find a match
        while True:
            # Skip whitespace
            word_start_current = self.current
            word_start_column = self.column
            while not self.is_at_end() and self.peek().isspace() and self.peek() != '\n':
                self.advance()
            
            # If we hit a newline or non-alphabetic character, stop
            if self.is_at_end() or self.peek() == '\n' or not self.peek().isalpha():
                break
            
            # Read the next word
            next_word = ""
            while not self.is_at_end() and (self.peek().isalnum() or self.peek() == '_'):
                next_word += self.advance()
            
            # Try the extended keyword
            candidate = lookahead_text + " " + next_word.lower()
            
            # Check if this candidate matches a keyword
            if candidate in self.keywords:
                # Found a match - update longest match
                longest_match = candidate
                longest_position = self.current
                longest_column = self.column
                lookahead_text = candidate
            else:
                # No exact match yet, but check if any keyword STARTS with this candidate
                # This allows us to continue looking for longer matches like "greater than or equal to"
                has_potential_match = any(key.startswith(candidate + " ") for key in self.keywords)
                
                if has_potential_match:
                    # Keep going - there might be a longer match
                    lookahead_text = candidate
                else:
                    # No potential matches - restore position to before this word and break
                    self.current = word_start_current
                    self.column = word_start_column
                    break
        
        # Use the longest match found
        if longest_match:
            self.current = longest_position
            self.column = longest_column
            self.add_token(self.keywords[longest_match])
        elif text.upper() in TokenType.__members__:
            # Restore to single-word position
            self.current = saved_current
            self.column = saved_column
            text = self.source[self.start:self.current].strip()
            self.add_token(TokenType[text.upper()])
        else:
            # Regular identifier - restore to single-word position
            self.current = saved_current
            self.column = saved_column
            text = self.source[self.start:self.current].strip()
            self.add_token(TokenType.IDENTIFIER, text)
    
    def number(self) -> None:
        """Scan a number literal (decimal, hexadecimal)."""
        # Check for hexadecimal: 0x...
        # Note: self.current is already past the first digit when this is called
        if self.source[self.start] == '0' and self.peek() in ('x', 'X'):
            self.advance()  # consume 'x' or 'X'
            
            # Scan hex digits
            if not self.peek().isdigit() and self.peek().lower() not in 'abcdef':
                self.error("Expected hex digits after '0x'")
                return
            
            while self.peek().isdigit() or self.peek().lower() in 'abcdef':
                self.advance()
            
            hex_str = self.source[self.start:self.current]
            hex_value = int(hex_str, 16)
            self.add_token(TokenType.INTEGER_LITERAL, hex_value)
            return
        
        # Scan decimal number
        while self.peek().isdigit():
            self.advance()
        
        if self.peek() == '.' and self.peek_next().isdigit():
            self.advance()  # consume the '.'
            while self.peek().isdigit():
                self.advance()
            self.add_token(TokenType.FLOAT_LITERAL, float(self.source[self.start:self.current]))
        else:
            self.add_token(TokenType.INTEGER_LITERAL, int(self.source[self.start:self.current]))
    
    def single_quote_string(self) -> None:
        """Scan a single-quoted string literal."""
        chars = []
        while not self.is_at_end() and self.peek() != "'":
            if self.peek() == '\\':
                self.advance()  # consume backslash
                if self.is_at_end():
                    break
                escape_char = self.peek()
                escape_map = {'n': '\n', 't': '\t', 'r': '\r', "'": "'", '\\': '\\'}
                chars.append(escape_map.get(escape_char, escape_char))
            elif self.peek() == '\n':
                self.line += 1
                self.column = 1
                chars.append(self.peek())
            else:
                chars.append(self.peek())
            self.advance()

        if self.is_at_end():
            self.error("Unterminated string")
            return

        self.advance()  # consume closing "'"
        self.add_token(TokenType.STRING_LITERAL, ''.join(chars))

    def string(self) -> None:
        """Scan a string literal with escape sequence support."""
        chars = []
        
        while self.peek() != '"' and not self.is_at_end():
            if self.peek() == '\\' and self.peek_next() is not None:
                # Handle escape sequences
                self.advance()  # consume backslash
                escape_char = self.peek()
                
                if escape_char == 'n':
                    chars.append('\n')
                elif escape_char == 't':
                    chars.append('\t')
                elif escape_char == 'r':
                    chars.append('\r')
                elif escape_char == '\\':
                    chars.append('\\')
                elif escape_char == '"':
                    chars.append('"')
                elif escape_char == "'":
                    chars.append("'")
                elif escape_char == '0':
                    chars.append('\0')
                elif escape_char == 'b':
                    chars.append('\b')
                elif escape_char == 'f':
                    chars.append('\f')
                elif escape_char == 'v':
                    chars.append('\v')
                else:
                    # Unknown escape sequence - keep the backslash
                    chars.append('\\')
                    chars.append(escape_char)
                
                self.advance()  # consume the escape character
            elif self.peek() == '\n':
                self.line += 1
                self.column = 1
                chars.append(self.peek())
                self.advance()
            else:
                chars.append(self.peek())
                self.advance()
        
        if self.is_at_end():
            self.error("Unterminated string")
            return
        
        self.advance()  # consume the closing '"'
        value = ''.join(chars)
        self.add_token(TokenType.STRING_LITERAL, value)
    
    def fstring(self) -> None:
        """Scan an f-string with interpolation: f"Hello, {name}!" 
        
        Supports format specifiers: f"{pi:.2f}"
        """
        parts = []  # List of (type, content, format_spec)
        current_literal = []
        
        while self.peek() != '"' and not self.is_at_end():
            if self.peek() == '{':
                # Save current literal part
                if current_literal:
                    parts.append(('literal', ''.join(current_literal), None))
                    current_literal = []
                
                self.advance()  # consume '{'
                
                # Check for escaped brace {{
                if self.peek() == '{':
                    current_literal.append('{')
                    self.advance()
                    continue
                
                # Collect expression until ':' or '}'
                expr_chars = []
                format_spec = None
                brace_depth = 1
                
                while brace_depth > 0 and not self.is_at_end():
                    if self.peek() == '{':
                        brace_depth += 1
                        expr_chars.append(self.peek())
                        self.advance()
                    elif self.peek() == ':' and brace_depth == 1:
                        # Format specifier found
                        self.advance()  # consume ':'
                        format_chars = []
                        while self.peek() != '}' and not self.is_at_end():
                            format_chars.append(self.peek())
                            self.advance()
                        format_spec = ''.join(format_chars)
                        # Now we should be at '}'
                        if self.peek() == '}':
                            brace_depth = 0
                            self.advance()
                    elif self.peek() == '}':
                        brace_depth -= 1
                        if brace_depth > 0:
                            expr_chars.append(self.peek())
                        self.advance()
                    else:
                        expr_chars.append(self.peek())
                        self.advance()
                
                # Add expression part with optional format spec
                expr_str = ''.join(expr_chars).strip()
                if expr_str:
                    parts.append(('expr', expr_str, format_spec))
            
            elif self.peek() == '}':
                # Check for escaped brace }}
                self.advance()
                if self.peek() == '}':
                    current_literal.append('}')
                    self.advance()
                else:
                    self.error("Unmatched '}' in f-string")
                    return
            
            elif self.peek() == '\\' and self.peek_next() is not None:
                # Handle escape sequences
                self.advance()
                escape_char = self.peek()
                
                if escape_char == 'n':
                    current_literal.append('\n')
                elif escape_char == 't':
                    current_literal.append('\t')
                elif escape_char == 'r':
                    current_literal.append('\r')
                elif escape_char == '\\':
                    current_literal.append('\\')
                elif escape_char == '"':
                    current_literal.append('"')
                else:
                    current_literal.append('\\')
                    current_literal.append(escape_char)
                
                self.advance()
            
            elif self.peek() == '\n':
                self.line += 1
                self.column = 1
                current_literal.append(self.peek())
                self.advance()
            
            else:
                current_literal.append(self.peek())
                self.advance()
        
        if self.is_at_end():
            self.error("Unterminated f-string")
            return
        
        # Save final literal part
        if current_literal:
            parts.append(('literal', ''.join(current_literal), None))
        
        self.advance()  # consume the closing '"'
        self.add_token(TokenType.FSTRING_LITERAL, parts)
    
    def match(self, expected: str) -> bool:
        """Match and consume an expected character."""
        if self.is_at_end() or self.source[self.current] != expected:
            return False
        self.current += 1
        self.column += 1
        return True
    
    def peek(self) -> str:
        """Look at the next character without consuming it."""
        if self.is_at_end():
            return '\0'
        return self.source[self.current]
    
    def peek_next(self) -> str:
        """Look at the character after the next one without consuming it."""
        if self.current + 1 >= len(self.source):
            return '\0'
        return self.source[self.current + 1]
    
    def advance(self) -> str:
        """Consume and return the next character."""
        c = self.source[self.current]
        self.current += 1
        self.column += 1
        return c
    
    def add_token(self, type: TokenType, literal: Optional[Any] = None) -> None:
        """Add a token to the list."""
        text = self.source[self.start:self.current]
        # Get the source line for error reporting (1-indexed line number)
        source_line = self.source_lines[self.line - 1] if 0 <= self.line - 1 < len(self.source_lines) else None
        self.tokens.append(Token(type, text, literal, self.line, self.column, source_line))
    
    def is_at_end(self) -> bool:
        """Check if we've reached the end of the source."""
        return self.current >= len(self.source)
    
    def error(self, message: str) -> None:
        """Report an error with source context."""
        from ..errors import NxlSyntaxError
        
        # Get the source line for context
        source_line = self.source_lines[self.line - 1] if 0 <= self.line - 1 < len(self.source_lines) else None
        
        raise NxlSyntaxError(
            message,
            line=self.line,
            column=self.column,
            source_line=source_line
        ) 