# NexusLang (NexusLang) Syntax Design

## Core Design Philosophy

The NexusLang syntax is designed to balance the natural feel of English with the precision and power required for a C++-level programming language. The design follows these guiding principles:

1. **Natural readability**: Code should be readable by non-programmers while maintaining precision
2. **Flexible expression**: Multiple ways to express the same operation, accommodating different writing styles
3. **Progressive complexity**: Simple operations should be simple to express, complex operations can use more technical syntax
4. **Unambiguous interpretation**: Despite flexibility, the meaning must be clear to the compiler
5. **C++-level power**: Support for all major programming paradigms and low-level operations
6. **Minimal boilerplate**: Reduce ceremonial code while maintaining clarity

## Core Syntax Rules

### 1. Statement Structure

Statements in NexusLang follow natural English sentence structure with a few consistent patterns:

#### Basic Statement Pattern

```
[Action Verb] [Subject/Object] [Preposition] [Value/Target] [Modifiers].
```

Examples:

- `Set x to 10.`
- `Add 5 to counter.`
- `Create a function called calculateArea.`

#### Alternative Phrasings

The language supports multiple ways to express the same operation:

```
Set x to 10.
Let x be 10.
Make x equal to 10.
x equals 10.
```

All of these statements compile to the same operation.

### 2. Program Structure

Programs are organized into logical sections without requiring explicit boilerplate:

```
Start the program.
    [Main program code]
End the program.
```

The `Start the program` and `End the program` statements are optional. If omitted, all code is assumed to be in the main program.

### 3. Variables and Data Types

#### Variable Declaration and Assignment

Variables are implicitly declared upon first use:

```
Set counter to 0.
```

Explicit type declaration is optional but available:

```
Create an integer called counter with value 0.
Create a string called name with value "John".
```

#### Type Inference

The language uses type inference to determine variable types:

```
Set x to 10.          // Inferred as integer
Set y to 10.5.        // Inferred as float/double
Set name to "John".   // Inferred as string
```

#### Strong Typing with Flexible Conversion

The language is strongly typed but provides flexible conversion when unambiguous:

```
Set x to 5.
Set y to "10".
Set z to x + y.       // y is converted to integer, z becomes 15
```

### 4. Control Structures

#### Conditionals

Conditionals use natural language constructs:

```
If x is greater than 10, then
    Display "x is large".
Otherwise if x is greater than 5, then
    Display "x is medium".
Otherwise
    Display "x is small".
End if.
```

Alternative forms:

```
When x exceeds 10, do
    Display "x is large".
End when.

Unless x is less than 5, do
    Display "x is not small".
End unless.
```

#### Loops

Loops use natural language with clear scope:

```
For each number i from 1 to 10, do
    Display i.
End for.

While x is less than 100, do
    Multiply x by 2.
End while.

Repeat until x exceeds 100
    Multiply x by 2.
End repeat.

Repeat 5 times
    Display "Hello".
End repeat.
```

#### Switch, Labels, and Fallthrough

The parser supports indentation-scoped switch blocks and optional loop labels.

```nexuslang
label outer: while counter is less than 10
    switch counter
        case 0
            set counter to counter plus 1
            continue outer
        case 1
            set counter to counter plus 1
            fallthrough
        case 2
            break outer
        default
            break
end
```

Canonical forms:

```ebnf
whileLoop      ::= ["label" IDENTIFIER ":"] "while" expression block "end"
forEachLoop    ::= ["label" IDENTIFIER ":"] "for" "each" IDENTIFIER ["with" "index" IDENTIFIER] "in" expression block "end"
parallelForLoop::= "parallel" ("for" | "for" "each") IDENTIFIER "in" expression block "end"
switchStmt     ::= "switch" expression ("case" expression block)+ ["default" block]
breakStmt      ::= "break" [IDENTIFIER]
continueStmt   ::= "continue" [IDENTIFIER]
fallthroughStmt::= "fallthrough"
```

Notes:

1. The handwritten parser accepts `parallel for x in items` and `parallel for each x in items`.
2. `parallel for` currently does not support `with index` or chunk-size/options clauses in parser syntax.
3. Higher-level batching and reduction should use standard-library concurrency helpers such as `parallel_for_each` and `parallel_reduce` until dedicated loop clauses are implemented.

### 5. Functions and Procedures

Functions are defined using natural language:

```
Define a function called add that takes a and b.
    Return a plus b.
End function.
```

Functions can specify return types:

```
Define a function called calculateArea that takes length and width and returns a number.
    Return length times width.
End function.
```

Calling functions:

```
Set result to add(5, 10).
Calculate the area using length 5 and width 10.  // Alternative syntax for calculateArea(5, 10)
```

### 6. Object-Oriented Programming

#### Class Definition

```
Create a class called Person.
    Add a property called name.
    Add a property called age with default value 0.
    
    Define a method called introduce that takes no parameters.
        Display "My name is " followed by this person's name.
        Display "I am " followed by this person's age followed by " years old".
    End method.
    
    Define a constructor that takes name and age.
        Set this person's name to name.
        Set this person's age to age.
    End constructor.
End class.
```

Canonical forms:

```ebnf
classDef            ::= "class" IDENTIFIER [genericParams] [("extends" | "inherits") typeList] ["implements" typeList] classBody "end"
classBody           ::= classMember*
classMember         ::= classMemberModifier* (propertyDecl | methodDef | operatorMethodDef | constructorDef)
classMemberModifier ::= "public" | "private" | "protected"
propertyDecl        ::= ["property"] IDENTIFIER "as" typeAnnotation ["default" "to" expression]
                     | "set" IDENTIFIER "to" expression
methodDef           ::= functionDef
                     | "static" functionDef
                     | "abstract" "function" IDENTIFIER [genericParams] ["with" parameterList] ["returns" typeAnnotation]
```

Notes:

- Current parser accepts both `extends` and `inherits` for class inheritance clauses.
- `static` and `abstract` are parsed as contextual class-member forms (for methods), not general-purpose class-member modifiers.
- Access modifiers (`public`, `private`, `protected`) can prefix `set` property forms and method declarations in class bodies.

#### Object Creation and Usage

```
Create a Person called john with name "John" and age 30.
Tell john to introduce.
Set john's age to 31.
```

### 7. Memory Management and Pointers

The language supports both automatic memory management and manual control:

#### Automatic Memory Management

```
Create a list of integers called numbers.
Add 1, 2, and 3 to numbers.
```

#### Manual Memory Management

```
Allocate memory for an integer called x.
Set the value at x to 10.
Free the memory used by x.
```

#### Pointers and References

```
Create a pointer to integer called ptr.
Point ptr to x.
Set the value at ptr to 20.

Create a reference to x called xRef.
Set xRef to 30.  // This changes x's value
```

### 8. Error Handling

Error handling uses structured try/catch blocks with optional exception bindings and typed raises:

```
try
    read config_file
catch err with message, code as ConfigError
    print text message
finally
    print text "cleanup"
end

raise ValueError with message "invalid input"
throw error with message last_error
raise
```

Canonical forms:

```ebnf
tryCatch       ::= "try" block "catch" IDENTIFIER ["with" IDENTIFIER ("," IDENTIFIER)*] ["as" IDENTIFIER] block ["finally" block] "end"
raiseStmt      ::= ("raise" | "throw") [IDENTIFIER ["with" IDENTIFIER expression]]
```

### 9. Modules and Namespaces

```
Import the math module.
Use the graphics namespace.

Create a module called utilities.
    Define a function called formatDate that takes a date.
        [Function implementation]
    End function.
End module.

Export the utilities module.
```

### 9.1 Generators and Yield (Parser-Aligned)

NexusLang generator functions are declared using normal function syntax. A function is treated as a generator when its body contains one or more `yield` expressions.

```nexuslang
function sequence with n as Integer returns List of Integer
    set i to 0
    while i is less than n
        yield i
        set i to i plus 1
    end
end
```

Canonical forms:

```ebnf
functionDef          ::= ["async"] "function" IDENTIFIER [genericParams]
                         ["with" parameterList] [whereClause] ["returns" typeAnnotation]
                         statement* "end"

genericParams        ::= "<" genericParam ("," genericParam)* ">"
genericParam         ::= IDENTIFIER [":" typeConstraintList]
                       | IDENTIFIER "::" kindAnnotation
whereClause          ::= "where" whereConstraint ("," whereConstraint)*
whereConstraint      ::= IDENTIFIER "is" typeConstraintList
typeConstraintList   ::= IDENTIFIER (("+" | "and") IDENTIFIER)*

yieldExpression      ::= "yield" [expression]
generatorExpression  ::= "(" expression "for" expression "in" expression ["if" expression] ")"
```

Notes:

1. Generator declaration is semantic (body contains `yield`), not a separate keyword form.

### 9.2 Macros and Comptime (Parser-Aligned)

NexusLang supports parser-level metaprogramming statements for macro definition/expansion and compile-time evaluation.

```nexuslang
macro SCALE_BY with value, factor
    set scaled to value times factor
    print text scaled
end

comptime const BASE is 7
comptime assert BASE is greater than 0 message "BASE must stay positive"

function main returns Integer
    expand SCALE_BY with value BASE, factor 3
    return 0
end
```

Canonical forms:

```ebnf
macroDefinition  ::= "macro" IDENTIFIER ["with" IDENTIFIER ("," IDENTIFIER)*] statement* "end"
macroExpansion   ::= "expand" IDENTIFIER ["with" IDENTIFIER expression ("," IDENTIFIER expression)*]

comptimeStmt     ::= "comptime" "eval" expression
                   | "comptime" "const" IDENTIFIER ("is" | "to") expression
                   | "comptime" "assert" expression ["message" expression]
                   | "comptime" expression
```

Notes:

1. Macro parameters are plain identifiers in parser syntax (`with p1, p2, ...`); type annotations are not part of the current parser form.
2. Macro expansion arguments use named-value pairs (`with name expr`) and comma separators.
3. Hygiene behavior is currently enforced in compiler expansion logic via scoped renaming of macro-local declarations, rather than through additional grammar markers.
4. `comptime` supports both explicit forms (`eval`, `const`, `assert`) and a bare expression fallback (`comptime EXPR`).
2. Parser-level generator expression uses `for ... in ...` inside parentheses.
3. `yield` can be bare (`yield`) or carry a value (`yield expr`).
4. Generic bounds in angle-bracket params use `:` and support trait composition with `+`.
5. `where` constraints support one or more entries, separated by commas.

### 10. Comments and Documentation

Comments use natural language markers:

```
// This is a single-line comment

/* This is a 
   multi-line comment */

Note that this function calculates the area of a rectangle.
Define a function called calculateArea that takes length and width.
    Return length times width.
End function.
```

## Syntax for Low-Level Operations

### Bit Manipulation

```
Perform bitwise AND on x and y, store in result.
Shift x left by 2 bits.
Set the third bit of flags to 1.
```

### Memory Operations

```
Copy 10 bytes from source to destination.
Set memory at address 0x1000 to zero for 100 bytes.
Get the address of variable x.
```

### Inline Assembly

```
Execute assembly {
    mov eax, 1
    mov ebx, 0
    int 0x80
}.
```

Parser-aligned canonical forms:

```ebnf
inlineAssembly  ::= "asm" ["for" "arch" STRING] asmSection* ["end"]
asmSection      ::= "code" STRING+
                  | "inputs" asmOperandList
                  | "outputs" asmOperandList
                  | "clobbers" asmClobberList
asmOperandList  ::= asmOperand ("," asmOperand)*
asmOperand      ::= STRING ":" expression
asmClobberList  ::= STRING ("," STRING)*
```

```nexuslang
asm for arch "x86_64"
    code "mov rax, 1" "ret"
    inputs "r": value
    outputs "=r": out_reg
    clobbers "rax", "rcx"
end
```

Notes:

1. The parser accepts an indented `code` section containing one or more string literals.
2. Operand sections preserve source order and allow repeated constraints.
3. `end` is accepted but not required by the reference grammar for inline assembly blocks.

### Channels (Parser-Aligned)

```ebnf
sendStatement   ::= "send" expression "to" expression
closeStatement  ::= "close" ["with"] expression
receiveExpr     ::= "receive" ["from"] expression
```

```nexuslang
send payload to jobs
set next to receive from jobs
set fallback to receive jobs
close with jobs
close jobs
```

Notes:

1. `receive` supports both `receive from ch` and `receive ch`.
2. `close` supports both `close with ch` and `close ch`.
3. Channel pipeline operator forms are not part of the current parser-aligned reference grammar.

### Async and Await (Parser-Aligned)

```ebnf
asyncFunctionDef  ::= "async" "function" IDENTIFIER [genericParams]
                      ["with" parameterList] ["returns" typeAnnotation]
                      statement* "end"

awaitExpression   ::= "await" expression
```

```nexuslang
async function fetch_data with endpoint as String returns String
    return "ok"
end

set result to await fetch_data with "/status"
```

Notes:

1. `async` is parser-level syntax only for function declarations (`async function ...`).
2. `await` is parsed as an expression and can be nested inside larger expressions.
3. `spawn` is currently tooling/runtime-facing and not a parser-level keyword form in the reference grammar.

### Contracts and Assertions (Parser-Aligned)

```ebnf
contractStatement ::= requireStatement
                    | ensureStatement
                    | guaranteeStatement
                    | invariantStatement

requireStatement   ::= "require" expression [("message" | "with") expression]
ensureStatement    ::= "ensure" expression [("message" | "with") expression]
guaranteeStatement ::= "guarantee" expression [("message" | "with") expression]
invariantStatement ::= "invariant" expression [("message" | "with") expression]
```

```nexuslang
require amount is greater than 0 message "amount must be positive"
ensure result is greater than or equal to 0
guarantee cache_ready message "cache must be initialized"
invariant size is greater than or equal to 0

function divide with a as Integer and b as Integer returns Integer
    require b is not equal to 0 message "division by zero"
    if a is greater than 0
        invariant b is not equal to 0
    end
    return a divided by b
end
```

Notes:

1. Contract statements are regular statements in the parser and can appear at top-level or inside nested blocks.
2. Nested forms are valid in function bodies and control-flow blocks (`if`, `while`, `for`, `try`, etc.).
3. `message` payload is optional and accepts any expression (tooling/typechecker may apply stricter semantic checks).
4. Parser compatibility also accepts `with <expr>` as a message introducer in contract statements.

### FFI and Unsafe (Parser-Aligned)

```ebnf
externFunction  ::= "extern" "function" IDENTIFIER
                    ["with" externParameterList]
                    ["returns" typeAnnotation]
                    ["from" "library" STRING]
                    ["calling" "convention" ("cdecl"|"stdcall"|IDENTIFIER)]

externVariable  ::= "extern" "variable" IDENTIFIER "as" typeAnnotation
                    ["from" "library" STRING]

externType      ::= "extern" "type" IDENTIFIER "as" ["opaque"] typeAnnotation
                  | "extern" "type" IDENTIFIER "as" "function"
                    ["with" typeAnnotation ("," typeAnnotation)*]
                    ["returns" typeAnnotation]

foreignFunction ::= "foreign" "function" IDENTIFIER
                    ["with" externParameterList]
                    ["returns" typeAnnotation]
                    ["from" "library" STRING]
                    ["calling" "convention" ("cdecl"|"stdcall"|IDENTIFIER)]

unsafeBlock     ::= "unsafe" ["do"] statement* "end"
```

```nexuslang
extern function puts with s as String returns Integer from library "libc" calling convention cdecl
foreign function puts with s as String returns Integer from library "libc"
extern variable errno as Integer from library "libc"
extern type FILE as opaque pointer
extern type Callback as function with Integer, Integer returns Integer

unsafe do
    set ptr to address of buffer
    # raw pointer operations
end

unsafe
    set ptr to address of buffer
end
```

Notes:

1. `foreign` is parser-level syntax only for function declarations.
2. `unsafe` accepts both `unsafe do ... end` and `unsafe ... end`.
3. Extern function parameters are comma-separated in the reference grammar.

## Compiler Directives

```
Tell the compiler to optimize for speed.
Tell the compiler to include the file "helpers.h".
Tell the compiler to use the C calling convention for this function.
```

## Type System
## Ownership / Borrow / Lifetime

NexusLang implements a borrow-checker-enforced ownership model that prevents
data races and use-after-free errors at compile time.

### Move semantics

Transfer ownership of a variable to a new binding.
After a move the source name is invalid.

```nexuslang
set y to move x          # x is now invalid; y owns the value
```

Grammar:
```
moveExpression : MOVE IDENTIFIER ;
```

### Borrow semantics

Create a temporary shared (immutable) or exclusive (mutable) reference.
Multiple immutable borrows are allowed simultaneously.
At most one mutable borrow may be active at a time.

```nexuslang
set r  to borrow x               # immutable borrow
set rw to borrow mutable x       # mutable borrow
```

Grammar:
```
borrowExpression : BORROW MUTABLE? IDENTIFIER lifetimeAnnotation? ;
```

### Lifetime annotations

Bind a borrow to an explicit lifetime label so the compiler can verify
that the reference does not outlive its source.

```nexuslang
set r to borrow x with lifetime a
```

Grammar:
```
lifetimeAnnotation : WITH LIFETIME IDENTIFIER ;
```

### Explicit borrow release

Borrows are normally released when they fall out of scope.
`drop borrow` releases a borrow early, allowing subsequent mutations.

```nexuslang
drop borrow x
drop borrow mutable x
```

Grammar:
```
dropBorrowStatement : DROP BORROW MUTABLE? IDENTIFIER ;
```

### Reference-counted smart pointers

`rc` creates a single-threaded reference-counted pointer (`Rc<T>`).
`arc` creates an atomically reference-counted pointer for concurrent use (`Arc<T>`).

```nexuslang
set p  to rc Integer : 42        # Rc<Integer>
set ap to arc String : "hello"   # Arc<String>
```

Grammar:
```
rcCreation  : RC  typeAnnotation ':' expression | RC  expression ;
arcCreation : ARC typeAnnotation ':' expression | ARC expression ;
```

### Weak references (cycle breaking)

`downgrade` converts an `Rc`/`Arc` to a non-owning `Weak` reference.
`upgrade` converts a `Weak` back to an `Rc`/`Arc`, returning `None` if the
original has already been dropped.

```nexuslang
set w to downgrade p
set q to upgrade w       # q is None if p was dropped
```

Grammar:
```
downgradeExpression : DOWNGRADE expression ;
upgradeExpression   : UPGRADE   expression ;
```

Notes:

1. The typechecker enforces borrow rules via `BorrowChecker` + `LifetimeChecker` passes.
2. Ownership diagnostics are surfaced in LSP as `Ownership error: ...` with
    `relatedInformation` ranges pointing at conflicting borrow, move, or drop sites.
3. `drop borrow` is a statement; all other ownership forms are expressions usable
    inside variable declarations and function arguments.

## Type System

The NexusLang type system includes:

1. **Basic types**: integer, float, double, boolean, character, string
2. **Compound types**: array, list, dictionary, set, tuple
3. **User-defined types**: class, struct, enum
4. **Function types**: function pointers, lambdas
5. **Generic types**: templates, parameterized types

## Ambiguity Resolution

When the language encounters potentially ambiguous constructs, it uses these strategies:

1. **Contextual analysis**: Using surrounding code to infer intent
2. **Type-based disambiguation**: Using type information to resolve ambiguity
3. **Precedence rules**: Clear rules for operator precedence
4. **Explicit markers**: Keywords that clarify intent in complex cases

For example:

```
Set result to a + b * c.  // Uses standard operator precedence

Set result to (a + b) * c.  // Explicit grouping

Set result to the sum of a and b, multiplied by c.  // Natural language with clear intent
```

## Conclusion

This syntax design provides a foundation for a NexusLang that combines the readability and accessibility of English with the power and precision of C++. The flexible syntax allows programmers to express ideas in a way that feels natural while maintaining the technical capabilities required for systems programming.

The next steps involve designing the compiler architecture that can translate this natural language syntax into efficient machine code, focusing on parsing strategies, semantic analysis, and code generation techniques.
