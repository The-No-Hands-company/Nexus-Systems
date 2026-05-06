# NexusLang Overview

## What is NLPL?

NLPL (NexusLang) is a general-purpose programming language that reads like English. You write code using words like `set`, `if`, `for each`, `function`, and `return` — the same words you use to explain logic to another person.

NLPL compiles to native code via LLVM. It has a comprehensive standard library, a full type system with generics, pattern matching, FFI, inline assembly, and a production-quality tooling ecosystem.

## Design goals

- **Readable by default** — code should be understandable without language expertise
- **No compromises on capability** — full OOP, generics, pattern matching, FFI, systems programming
- **Strong tooling** — LSP, debugger, build system, formatter, REPL all work today
- **Type safety** — optional static typing with inference; dynamic mode available

## What NexusLang looks like

```nlpl
# Variables
set name to "Alice"
set age as Integer to 30

# Functions
function greet with person as String returns String
    if person is not empty
        return "Hello, " plus person plus "!"
    else
        return "Hello!"
    end
end

# Classes
class Counter
    set count as Integer to 0

    function increment
        set count to count plus 1
    end

    function value returns Integer
        return count
    end
end

# Using the class
set c to create Counter
call c.increment
call c.increment
print number c.value()

# Collections and loops
set numbers to [1, 2, 3, 4, 5]
set total to 0
for each n in numbers
    set total to total plus n
end
print text "Sum: " plus total

# Error handling
try
    set data to read_file("config.json")
catch error as e
    print text "Could not read config: " plus e
end
```

## Current state

NLPL is pre-v1.0 and under active development. The interpreter is production-quality for real programs. The LLVM compiler backend works for core language constructs. See [README.md](../../README.md) for a full honest status breakdown.
