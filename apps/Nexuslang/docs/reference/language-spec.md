# NexusLang (NexusLang) Specification

## 1. Introduction

The NexusLang (NexusLang) is a general-purpose programming language designed to bridge the gap between human language and computer code. It features an English-like syntax while maintaining the power and expressiveness of traditional programming languages like C++.

This document provides a formal specification of the NexusLang syntax and semantics, serving as a reference for language implementers and users.

## 2. Language Design Goals

NLPL was designed with the following goals in mind:

1. **Natural English Syntax**: Commands should read like English sentences, making the code more accessible to non-programmers.
2. **C++-Level Power**: The language should support low-level features like manual memory management alongside high-level abstractions.
3. **General Purpose**: The language should support a wide range of programming paradigms and applications.
4. **Unambiguous Parsing**: Despite using natural language, the grammar should be structured enough for reliable parsing.
5. **Advanced Features**: The language should include features that go beyond C++, such as built-in concurrency and more natural error handling.

## 3. Lexical Structure

### 3.1 Keywords

NLPL uses English words as keywords. The following are reserved keywords in the language:

```
Create, Define, Function, Class, If, Otherwise, While, Repeat, For, Each, In,
Allocate, Free, Memory, Run, These, Tasks, At, Same, Time, Try, To, But, Fails,
And, Or, Not, With, Called, That, Takes, Returns, Has, Property, Method, End
```

Keywords are case-insensitive in NexusLang.

### 3.2 Identifiers

Identifiers in NexusLang follow standard programming language conventions:

- Must start with a letter
- Can contain letters, digits, and underscores
- Cannot be a keyword

Examples of valid identifiers:
```
counter
firstName
data_processor
Calculator
```

### 3.3 Literals

NLPL supports the following types of literals:

#### 3.3.1 Number Literals

Integer and floating-point numbers:
```
42
3.14
-10
```

#### 3.3.2 String Literals

Text enclosed in double quotes:
```
"Hello, world!"
"NLPL is an English-like language"
```

#### 3.3.3 Boolean Literals

Truth values:
```
true
false
```

## 4. Syntax and Semantics

### 4.1 Program Structure

An NexusLang program consists of a sequence of statements. Each statement typically ends with a period, though some compound statements (like function definitions) have an explicit `End` marker.

### 4.2 Variable Declaration and Assignment

Variables are declared using the `Create` keyword, followed by the type and identifier:

```
Create an integer called counter and set it to 0.
Create a string called message.
```

Equivalent C++ code:
```cpp
int counter = 0;
std::string message;
```

### 4.3 Functions

Functions are defined using the `Define` keyword:

```
Define a function called add that takes integer a and integer b and returns integer.
    Create an integer called result and set it to a plus b.
    Return result.
End
```

Equivalent C++ code:
```cpp
int add(int a, int b) {
    int result = a + b;
    return result;
}
```

### 4.4 Control Flow

#### 4.4.1 Conditional Statements

If statements use natural language conditions:

```
If counter is greater than 10 then
    Print "Counter is large".
Otherwise
    Print "Counter is small".
End
```

Equivalent C++ code:
```cpp
if (counter > 10) {
    std::cout << "Counter is large" << std::endl;
} else {
    std::cout << "Counter is small" << std::endl;
}
```

#### 4.4.2 Loops

While loops:

```
While counter is less than 10
    Print counter.
    Set counter to counter plus 1.
End
```

For-each loops:

```
Repeat for each item in items
    Print item.
End
```

Equivalent C++ code:
```cpp
while (counter < 10) {
    std::cout << counter << std::endl;
    counter = counter + 1;
}

for (auto& item : items) {
    std::cout << item << std::endl;
}
```

### 4.5 Memory Management

NLPL provides explicit memory management similar to C++:

```
Allocate a new integer in memory with value 42 and name it ptr.
Free the memory at ptr.
```

Equivalent C++ code:
```cpp
int* ptr = new int(42);
delete ptr;
```

### 4.6 Classes and Objects

Classes are defined with properties and methods:

```
Define a class called Calculator.
    It has a float property called result.
    
    Define a method called add that takes float a and float b and returns float.
        Set result to a plus b.
        Return result.
    End
End

Create a Calculator called calc.
Call calc.add with 5, 3.
Print calc.result.
```

Equivalent C++ code:
```cpp
class Calculator {
public:
    float result;
    
    float add(float a, float b) {
        result = a + b;
        return result;
    }
};

Calculator calc;
calc.add(5, 3);
std::cout << calc.result << std::endl;
```

### 4.7 Concurrency

NLPL provides built-in concurrency:

```
Run these tasks at the same time:
    Call process_data with dataset1.
    Call process_data with dataset2.
End
```

Equivalent C++ code (using std::thread):
```cpp
std::thread t1([&]() { process_data(dataset1); });
std::thread t2([&]() { process_data(dataset2); });
t1.join();
t2.join();
```

### 4.8 Error Handling

NLPL uses try-catch blocks with natural language:

```
Try to
    Call divide with 10, 0.
But if it fails
    Print "Division by zero error".
End
```

Equivalent C++ code:
```cpp
try {
    divide(10, 0);
} catch (std::exception& e) {
    std::cout << "Division by zero error" << std::endl;
}
```

## 5. Formal Grammar

The complete BNF grammar for NexusLang can be found in the `src/parser/bnf_grammar.txt` file. Here's a simplified overview:

```
<program> ::= <statement-list>
<statement> ::= <variable-declaration> | <function-definition> | ...
<variable-declaration> ::= "Create" ["a" | "an"] <type> "called" <identifier> ["and" "set" "it" "to" <expression>]
...
```

## 6. Examples

### 6.1 Simple Calculator

```
# Simple Calculator Program in NexusLang

# Define the main calculator class
Define a class called Calculator

    # Properties
    It has a float property called result

    # Addition method
    Define a method called add that takes float a and float b and returns float
        Create a float called sum and set it to a plus b
        Set result to sum
        Return result
    End

    # Subtraction method
    Define a method called subtract that takes float a and float b and returns float
        Create a float called difference and set it to a minus b
        Set result to difference
        Return result
    End

    # Multiplication method
    Define a method called multiply that takes float a and float b and returns float
        Create a float called product and set it to a times b
        Set result to product
        Return result
    End

    # Division method
    Define a method called divide that takes float a and float b and returns float
        # Error handling for division by zero
        Try to
            Create a float called quotient and set it to a divided by b
            Set result to quotient
            Return result
        But if it fails
            Print "Error: Cannot divide by zero"
            Return 0
        End
    End

End

# Main program
Create a Calculator called calc

# Perform calculations
Print "Calculator Example"
Print "----------------"

Print "Addition: 5 + 3 = "
Call calc.add with 5, 3
Print calc.result
```

### 6.2 Concurrent Processing

```
# Concurrent Processing Example in NexusLang

Define a class called DataProcessor
    It has a list property called data
    It has a list property called results
    It has an integer property called thread_count
    
    Define a method called process_concurrent that takes nothing and returns list
        Create a list called local_results with size equal to length of data
        
        Print "Starting concurrent processing with " + thread_count + " threads..."
        
        # Define the worker function
        Define a function called worker that takes integer start_idx and integer end_idx and returns nothing
            Repeat for each idx from start_idx to end_idx
                Create an integer called item and set it to data at idx
                Call process_item with item
                Set local_results at idx to the result
            End
        End
        
        # Launch concurrent tasks
        Run these tasks at the same time:
            Repeat for each t from 0 to thread_count
                Call worker with t times chunk_size, (t plus 1) times chunk_size minus 1
            End
        End
        
        Return local_results
    End
End
```

## 7. Implementation Notes

The NexusLang implementation consists of several components:

1. **Lexer**: Tokenizes the natural language input
2. **Parser**: Converts tokens into an Abstract Syntax Tree (AST)
3. **Interpreter**: Executes the AST
4. **Runtime**: Provides the execution environment

The implementation is designed to be modular and extensible, allowing for future enhancements and optimizations.

## 8. Future Directions

Potential future enhancements to NexusLang include:

1. **Type Inference**: Reducing the need for explicit type declarations
2. **Module System**: Supporting code organization and reuse
3. **Metaprogramming**: Allowing code that generates code
4. **Parallel Computing**: Enhanced support for GPU and distributed computing
5. **Natural Language Understanding**: More flexible parsing of English-like constructs

## 9. Conclusion

NLPL demonstrates that it's possible to create a programming language with natural English syntax while maintaining the power and expressiveness of traditional languages. By bridging the gap between human language and computer code, NexusLang aims to make programming more accessible to a wider audience. 