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

Error handling uses natural language constructs:

```
Try to
    Open the file "data.txt".
    Read the first line from the file.
If something goes wrong, then
    Display "Error: " followed by the error message.
Finally
    Close the file if it is open.
End try.
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

## Compiler Directives

```
Tell the compiler to optimize for speed.
Tell the compiler to include the file "helpers.h".
Tell the compiler to use the C calling convention for this function.
```

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
