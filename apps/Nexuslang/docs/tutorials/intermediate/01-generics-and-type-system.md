# Tutorial 6: Generics and the Type System

**Time:** ~60 minutes  
**Prerequisites:** [Objects and Classes](../beginner/03-objects-and-classes.md)

---

## Part 1 — Why Generics?

Without generics you need a separate function for every type:

```nexuslang
function max_integer with a as Integer and b as Integer returns Integer
    if a is greater than b  return a
    return b
end

function max_float with a as Float and b as Float returns Float
    if a is greater than b  return a
    return b
end
```

With generics you write it once with a **type parameter** `T`:

```nexuslang
function max<T> that takes a as T and b as T returns T
    if a is greater than b  return a
    return b
end

set i to max with 5 and 3           # Integer
set f to max with 3.14 and 2.71     # Float
```

---

## Part 2 — Generic Functions

### Identity and Pass-Through

```nexuslang
function identity<T> that takes value as T returns T
    return value
end

print text convert (identity with 42) to string        # 42
print text identity with "hello"                       # hello
```

### Swapping Two Values

```nexuslang
function swap<T> that takes first as T and second as T returns List
    return [second, first]
end

set pair to swap with "alpha" and "beta"
# pair is ["beta", "alpha"]
```

### Pair Creation

```nexuslang
function make_pair<T, U> that takes first as T and second as U returns List
    return [first, second]
end

set kv to make_pair with "port" and 8080
```

---

## Part 3 — Generic Classes

```nexuslang
class Box<T>
    private set contents to T

    public function initialize with value as T
        set this.contents to value

    public function get returns T
        return this.contents

    public function set with value as T
        set this.contents to value

    public function is_empty returns Boolean
        return false
```

```nexuslang
set int_box to new Box<Integer>
set int_box.contents to 100
print text convert (int_box.get()) to string    # 100

set str_box to new Box<String>
set str_box.contents to "message"
print text str_box.get()
```

### Generic Stack

```nexuslang
class Stack<T>
    private set items to List of T

    public function initialize
        create this.items as empty List of T

    public function push with item as T
        append item to this.items

    public function pop returns T
        if length(this.items) equals 0
            raise error with "Stack is empty"
        set top to this.items[length(this.items) minus 1]
        remove last from this.items
        return top

    public function peek returns T
        if length(this.items) equals 0
            raise error with "Stack is empty"
        return this.items[length(this.items) minus 1]

    public function is_empty returns Boolean
        return length(this.items) equals 0

    public function size returns Integer
        return length(this.items)
```

---

## Part 4 — Type Annotations

NLPL infers types from usage, but you can annotate variables and function
signatures explicitly for clarity and early error detection:

```nexuslang
set count as Integer to 0
set label as String to "items"
set ratio as Float to 0.5

function compute with values as List of Float returns Float
    set total as Float to 0.0
    for each v in values
        set total to total plus v
    return total divided by length(values)
end
```

---

## Part 5 — Optional Types

An optional holds either a value or nothing (`null`).  Use `Optional of T`:

```nexuslang
function find_user with id as Integer returns Optional of String
    if id equals 1
        return "Alice"
    if id equals 2
        return "Bob"
    return null
end

set user to find_user with 1
if user is not null
    print text "Found: " plus user
else
    print text "User not found"
```

---

## Part 6 — Type Constraints

Constrain a generic parameter to types that support certain operations:

```nexuslang
function sum_list<T: Numeric> that takes items as List of T returns T
    set total to 0
    for each item in items
        set total to total plus item
    return total
end

print text convert (sum_list with [1, 2, 3, 4, 5]) to string        # 15
print text convert (sum_list with [1.1, 2.2, 3.3]) to string        # 6.6
```

---

## Summary

| Concept | Syntax |
|---------|--------|
| Generic function | `function f<T> that takes v as T returns T` |
| Generic class | `class C<T>` |
| Instantiate generic | `new Container<Integer>` |
| Optional value | `Optional of T`, check with `is not null` |
| Type annotation | `set x as Integer to 0` |
| Type constraint | `<T: Numeric>` |

**Next:** [Concurrency Basics](02-concurrency-basics.md)
