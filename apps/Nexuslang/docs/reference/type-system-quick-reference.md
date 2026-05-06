# Type System Quick Reference

## Type Annotations

### Variable Declarations
```nexuslang
set x to 42 # Inferred as Integer
set y as Float to 3.14 # Explicit type annotation
set name as String to "Alice" # Explicit String type
```

### Function Signatures
```nexuslang
# Function with typed parameters and return type
function add with a as Integer, b as Integer returns Integer
 return a plus b
end

# Function without return type annotation (inferred)
function greet with name as String
 return "Hello, " plus name
end
```

### Class Properties
```nexuslang
class Person
 name as String
 age as Integer
 email as String
 
 method get_info returns String
 return this.name plus " (" plus this.age as String plus ")"
 end
end
```

## Type Inference

### Automatic Inference
```nexuslang
# Literals
set count to 0 # Integer
set price to 9.99 # Float
set message to "Hello" # String
set active to true # Boolean

# Collections
set numbers to [1, 2, 3] # List<Integer>
set names to ["Alice", "Bob"] # List<String>
set scores to {"A": 95, "B": 87} # Dictionary<String, Integer>

# Expressions
set sum to 10 plus 20 # Integer (10) + Integer (20) = Integer
set result to 5 times 2.5 # Integer (5) * Float (2.5) = Float
```

### Bidirectional Inference
```nexuslang
# Lambda parameter types inferred from context
function map with items as List of Integer, fn as Function returns List of Integer
 set result to empty List of Integer
 for each item in items
 append fn(item) to result
 end
 return result
end

set doubled to map([1, 2, 3], lambda x => x times 2)
# x inferred as Integer from map's parameter type
```

### Expected Type Context
```nexuslang
# Empty collection with expected type
function create_numbers returns List of Integer
 set nums to empty List of Integer # Type inferred from return type
 append 1 to nums
 append 2 to nums
 return nums
end

# Function call with expected return type
function get_score returns Integer
 return 95 # Integer literal matches expected return type
end
```

## Generic Types

### Generic Functions
```nexuslang
# Generic function with type parameter T
function identity<T> with value as T returns T
 return value
end

# Type argument inferred from usage
set x to identity(42) # T = Integer
set y to identity("hello") # T = String
```

### Multiple Type Parameters
```nexuslang
# Generic function with T and R
function map<T, R> with items as List<T>, fn as Function returns List<R>
 set result to empty List<R>
 for each item in items
 append fn(item) to result
 end
 return result
end

# Both T and R inferred from arguments
set strings to map([1, 2, 3], lambda x => "Number " plus x as String)
# T = Integer, R = String
```

### Generic Collections
```nexuslang
# List with element type
set numbers to empty List of Integer
append 1 to numbers
append 2 to numbers

# Dictionary with key and value types
set scores to empty Dictionary
set scores["Alice"] to 95 # Key: String, Value: Integer
set scores["Bob"] to 87
```

### Generic Constraints
```nexuslang
# Generic function with constraint
function max<T: Comparable> with a as T, b as T returns T
 if a is greater than b
 return a
 else
 return b
 end
end

set max_num to max(42, 17) # T = Integer (Comparable)
set max_str to max("apple", "banana") # T = String (Comparable)
```

## User-Defined Types

### Basic Class
```nexuslang
class Point
 x as Integer
 y as Integer
 
 method distance_from_origin returns Float
 set x_squared to this.x times this.x
 set y_squared to this.y times this.y
 return sqrt(x_squared plus y_squared)
 end
end

set p to Point with x as 3, y as 4
set distance to p.distance_from_origin()
```

### Inheritance
```nexuslang
class Animal
 name as String
 age as Integer
 
 method make_sound returns String
 return "Some sound"
 end
end

class Dog extends Animal
 breed as String
 
 method make_sound returns String
 return "Woof!"
 end
 
 method bark
 print text this.make_sound()
 end
end

set dog to Dog with name as "Buddy", age as 3, breed as "Golden Retriever"
dog.bark()
```

### Polymorphism
```nexuslang
# Function accepting supertype
function greet_animal with animal as Animal returns String
 return "Hello, " plus animal.name plus "! " plus animal.make_sound()
end

set dog to Dog with name as "Max", age as 2, breed as "Labrador"
set cat to Cat with name as "Whiskers", age as 1

# Both work - Dog and Cat are subtypes of Animal
set greeting1 to greet_animal(dog)
set greeting2 to greet_animal(cat)
```

## Type Compatibility

### Numeric Widening
```nexuslang
# Integer automatically widens to Float
set int_val to 10 # Integer
set float_val to 3.14 # Float
set result to int_val plus float_val # Float (widening)

# Function parameter widening
function process with value as Float returns Float
 return value times 2.0
end

set doubled to process(5) # Integer 5 widens to Float
```

### String Concatenation
```nexuslang
# String + Any (with conversion)
set name to "Alice"
set age to 30
set message to name plus " is " plus age as String plus " years old"
```

### List Compatibility
```nexuslang
# Mixed numeric types unify to Float
set mixed to [1, 2.5, 3] # List<Float> (Integer widens to Float)

# Element type compatibility
function sum_integers with numbers as List of Integer returns Integer
 set total to 0
 for each num in numbers
 set total to total plus num
 end
 return total
end

set nums to [1, 2, 3, 4, 5]
set sum to sum_integers(nums) # List<Integer> matches parameter type
```

### Subtype Compatibility
```nexuslang
# Subtype can be used where supertype is expected
class Vehicle
 speed as Integer
end

class Car extends Vehicle
 brand as String
end

function show_speed with vehicle as Vehicle
 print text "Speed: " plus vehicle.speed as String
end

set car to Car with speed as 120, brand as "Toyota"
show_speed(car) # Car compatible with Vehicle (subtype)
```

## Common Patterns

### Optional Types (Nullable)
```nexuslang
# Function that may return null
function find_user with id as Integer returns String
 if id is greater than 0
 return "User" plus id as String
 else
 return "" # Empty string represents "not found"
 end
end
```

### Union Types
```nexuslang
# Function with multiple return types
function get_value with flag as Boolean returns Integer
 if flag
 return 42
 else
 return 0
 end
end
# Return type unified to Integer
```

### Lambda Types
```nexuslang
# Lambda with inferred types
set doubler to lambda x => x times 2
# Type inferred from usage: Integer -> Integer

# Lambda with explicit types
set adder to lambda x as Integer, y as Integer => x plus y
# Type: (Integer, Integer) -> Integer

# Lambda as function parameter
function apply with value as Integer, fn as Function returns Integer
 return fn(value)
end

set result to apply(10, lambda x => x plus 5)
```

### Generic Collections
```nexuslang
# Creating typed collections
set int_list to empty List of Integer
set str_dict to empty Dictionary of String, Integer

# Type-safe operations
append 1 to int_list # OK: Integer matches element type
set str_dict["key"] to 42 # OK: String key, Integer value
```

## Type System Commands

### Enable Type Checking
```bash
# Run with type checking enabled
python src/main.py program.nlpl --type-check

# Disable type checking (default)
python src/main.py program.nlpl --no-type-check
```

### Debug Type Inference
```bash
# Show inferred types (debug mode)
python src/main.py program.nlpl --debug --type-check
```

## Best Practices

1. **Use explicit type annotations** for public APIs (functions, classes)
2. **Rely on inference** for local variables and internal code
3. **Leverage bidirectional inference** for lambdas
4. **Use generic types** for reusable, type-safe functions
5. **Define clear class hierarchies** for polymorphism
6. **Enable type checking** during development to catch errors early
7. **Write tests** with expected types to verify type system behavior

## Type System Features

| Feature | Syntax Example | Status |
|---------|----------------|--------|
| Basic inference | `set x to 42` | |
| Type annotations | `set x as Integer to 42` | |
| Function types | `function f with x as Integer returns Integer` | |
| Generic functions | `function f<T> with x as T returns T` | |
| Generic types | `List<Integer>`, `Dictionary<String, Float>` | |
| Class types | `class Person ... end` | |
| Inheritance | `class Dog extends Animal` | |
| Lambda inference | `lambda x => x plus 1` | |
| Bidirectional inference | Context-guided lambda types | |
| Subtyping | Dog Animal | |
| Numeric widening | Integer Float | |
| Union types | Multiple return types | |
| Constraints | `<T: Comparable>` | |
| Variance | `out T`, `in T` | |
| Traits | `trait Printable` | |

 = Fully implemented
 = Partially implemented
 = Not yet implemented
