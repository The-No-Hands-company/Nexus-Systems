# Tutorial 2: Variables, Functions, and Control Flow

**Time:** ~60 minutes  
**Prerequisites:** [Hello World](01-hello-world.md)

---

## Part 1 — Variables

### Declaring and Assigning Variables

```nexuslang
set name to "Alice"
set age to 30
set height to 1.68
set is_active to true
```

`set … to …` is the universal assignment statement.  There are no separate
declaration keywords — the type is inferred from the first assigned value.

### Changing a Variable's Value

```nexuslang
set counter to 0
set counter to counter plus 1
set counter to counter plus 1
print text convert counter to string   # prints 2
```

### Variable Types

| Type | Example literals |
|------|-----------------|
| Integer | `0`, `42`, `-7` |
| Float | `3.14`, `-0.5`, `1.0` |
| String | `"hello"`, `"world"` |
| Boolean | `true`, `false` |
| List | `[1, 2, 3]`, `["a", "b"]` |

### Arithmetic

```nexuslang
set a to 10
set b to 3

set sum       to a plus b          # 13
set diff      to a minus b         # 7
set product   to a times b         # 30
set quotient  to a divided by b    # 3.333...
set remainder to a modulo b        # 1
```

### String Operations

```nexuslang
set first to "Hello"
set second to "World"
set joined to first plus ", " plus second plus "!"   # "Hello, World!"
set loud   to convert first to uppercase             # "HELLO"
set quiet  to convert first to lowercase             # "hello"
set chars  to length(first)                          # 5
```

### List Operations

```nexuslang
set fruits to ["apple", "banana", "cherry"]
set first_fruit to fruits[0]                       # "apple"
set count       to length(fruits)                  # 3
append "date" to fruits                            # now 4 items
```

---

## Part 2 — Functions

### Defining a Function

```nexuslang
function greet with name as String
    print text "Hello, " plus name plus "!"
end
```

Call it with:

```nexuslang
greet with "Alice"
greet with "Bob"
```

### Returning a Value

```nexuslang
function square with n as Integer returns Integer
    return n times n
end

set result to square with 7
print text convert result to string    # 49
```

### Multiple Parameters

```nexuslang
function add with x as Integer and y as Integer returns Integer
    return x plus y
end

set total to add with 5 and 3
print text convert total to string     # 8
```

### Named Parameters

When a function has many parameters, using names at the call site makes the
code easier to read:

```nexuslang
function create_user with username as String and email as String and age as Integer
    print text "Creating user: " plus username
    print text "Email: " plus email
    print text "Age: " plus convert age to string
end

create_user with username: "alice" and email: "alice@example.com" and age: 28
```

### Default Parameters

```nexuslang
function greet_formal with name as String and title as String default to "Ms."
    print text "Good day, " plus title plus " " plus name plus "."
end

greet_formal with "Smith"             # uses default title "Ms."
greet_formal with "Jones" and title: "Dr."
```

### Functions Must Be Defined Before Use

NLPL evaluates the file top-to-bottom. Define functions above the code that calls them.

---

## Part 3 — Control Flow

### if / else if / else

```nexuslang
set score to 75

if score is greater than or equal to 90
    print text "Grade: A"
else if score is greater than or equal to 80
    print text "Grade: B"
else if score is greater than or equal to 70
    print text "Grade: C"
else
    print text "Grade: F"
```

### Comparison Operators

| Expression | Meaning |
|-----------|---------|
| `a equals b` | equal |
| `a is not equal to b` | not equal |
| `a is greater than b` | greater than |
| `a is less than b` | less than |
| `a is greater than or equal to b` | >= |
| `a is less than or equal to b` | <= |

### Boolean Operators

```nexuslang
if age is greater than or equal to 18 and has_id equals true
    print text "Access granted"

if is_student equals true or is_teacher equals true
    print text "Educational discount applies"

if not is_banned
    print text "Welcome back"
```

### for each Loop

```nexuslang
set numbers to [1, 2, 3, 4, 5]
for each n in numbers
    print text convert n to string
```

### while Loop

```nexuslang
set count to 1
while count is less than or equal to 5
    print text "Count: " plus convert count to string
    set count to count plus 1
```

### Nested Loops

```nexuslang
for each i in [1, 2, 3]
    for each j in [1, 2, 3]
        print text convert i to string plus " x " plus convert j to string plus " = " plus convert (i times j) to string
```

---

## Putting It Together

Here is a short program that uses everything from this tutorial:

```nexuslang
# Fizz Buzz

function fizzbuzz with n as Integer returns String
    if n modulo 15 equals 0
        return "FizzBuzz"
    else if n modulo 3 equals 0
        return "Fizz"
    else if n modulo 5 equals 0
        return "Buzz"
    else
        return convert n to string
end

set i to 1
while i is less than or equal to 20
    print text fizzbuzz with i
    set i to i plus 1
```

---

## Summary

| Concept | Syntax |
|---------|--------|
| Variable | `set name to value` |
| Arithmetic | `plus`, `minus`, `times`, `divided by`, `modulo` |
| Function | `function f with p as T returns R … end` |
| Named call | `f with p: value and q: value` |
| Conditional | `if … else if … else` |
| For loop | `for each item in list` |
| While loop | `while condition` |

**Next:** [Objects and Classes](03-objects-and-classes.md)
