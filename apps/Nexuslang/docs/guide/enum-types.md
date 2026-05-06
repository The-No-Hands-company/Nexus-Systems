# Enum Types in NexusLang

## Overview

Enums (enumerations) in NexusLang provide a way to define named constant sets with automatic or explicit value assignment. They make code more readable and maintainable by replacing magic numbers with meaningful names.

## Syntax

### Basic Auto-Numbered Enum

```nexuslang
enum EnumName
    MemberOne
    MemberTwo
    MemberThree
```

Auto-numbered enums start at 0 and increment by 1 for each member.

### Enum with Explicit Values

```nexuslang
enum StatusCode
    Success = 0
    Warning = 100
    Error = 200
    Critical = 300
```

### Mixed Auto and Explicit Values

```nexuslang
enum Priority
    Low        # = 0 (auto)
    Medium     # = 1 (auto)
    High = 10  # = 10 (explicit)
    Critical   # = 11 (continues from 10)
```

## Basic Usage

### Declaration and Access

```nexuslang
enum Color
    Red
    Green
    Blue

set my_color to Color.Red
print number my_color  # Prints: 0
```

### Accessing Enum Members

Enum members are accessed using dot notation: `EnumName.MemberName`

```nexuslang
enum DayOfWeek
    Monday
    Tuesday
    Wednesday
    Thursday
    Friday
    Saturday
    Sunday

set today to DayOfWeek.Wednesday  # Value: 2
```

## Examples

### Traffic Light System

```nexuslang
enum TrafficLight
    Red = 0
    Yellow = 1
    Green = 2

set light to TrafficLight.Green

switch light
    case 0
        print text "STOP"
    case 1
        print text "CAUTION"
    case 2
        print text "GO"
```

**Output:** `GO`

### HTTP Status Codes

```nexuslang
enum HttpStatus
    OK = 200
    Created = 201
    BadRequest = 400
    NotFound = 404
    ServerError = 500

set status to HttpStatus.NotFound
print text "Status: "
print number status  # Prints: 404

if status is equal to 404
    print text "Page not found"
```

### Log Levels

```nexuslang
enum LogLevel
    DEBUG
    INFO
    WARNING
    ERROR
    CRITICAL

set current_level to LogLevel.WARNING

switch current_level
    case 0
        print text "Debug"
    case 1
        print text "Info"
    case 2
        print text "Warning"
    case 3
        print text "Error"
    case 4
        print text "Critical"
```

**Output:** `Warning`

### User Roles

```nexuslang
enum UserRole
    Guest = 0
    User = 10
    Moderator = 50
    Admin = 100

set role to UserRole.Moderator
print number role  # Prints: 50

if role is greater than 40
    print text "Has moderation privileges"
```

### Game States

```nexuslang
enum GameState
    MainMenu
    Playing
    Paused
    GameOver

set current_state to GameState.Playing

if current_state is equal to GameState.Playing
    print text "Game is active"
```

## Using Enums with Switch Statements

Enums work perfectly with switch statements for clean, readable code:

```nexuslang
enum Direction
    North
    South
    East
    West

set heading to Direction.East

switch heading
    case 0
        print text "Going North"
    case 1
        print text "Going South"
    case 2
        print text "Going East"
    case 3
        print text "Going West"
```

## Comparisons

Enum values can be compared like integers:

```nexuslang
enum ErrorLevel
    None = 0
    Low = 1
    Medium = 5
    High = 10

set level to ErrorLevel.Medium

if level is greater than ErrorLevel.Low
    print text "Significant error detected"

if level is less than ErrorLevel.High
    print text "Not critical yet"

if level is equal to 5
    print text "Level is Medium"  # Direct numeric comparison
```

## Multiple Enums

You can define multiple enums in the same program:

```nexuslang
enum Color
    Red
    Green
    Blue

enum Size
    Small = 1
    Medium = 2
    Large = 3

enum Priority
    Low
    High

set item_color to Color.Red
set item_size to Size.Large
set task_priority to Priority.High
```

## Implementation Details

### Compilation

Enums are compiled to integer constants at compile time. Each enum member becomes a global read-only constant:

**NLPL Code:**
```nexuslang
enum Status
    Active = 1
    Inactive = 0
```

**Generated LLVM IR:**
```llvm
@Status.Active = private unnamed_addr constant i64 1, align 8
@Status.Inactive = private unnamed_addr constant i64 0, align 8
```

### Member Access

When you write `Status.Active`, the compiler:
1. Checks if `Status` is a defined enum
2. Looks up `Active` in that enum's members
3. Returns the integer value directly

This means enum access has **zero runtime overhead** - it's as fast as using a literal number.

### Auto-Numbering

Auto-numbered enums start at 0 and increment by 1:

```nexuslang
enum Example
    First   # = 0
    Second  # = 1
    Third   # = 2
```

If you specify an explicit value, auto-numbering continues from that value:

```nexuslang
enum Example
    First      # = 0
    Second     # = 1
    Third = 10 # = 10
    Fourth     # = 11
```

### Type

Enums are represented as 64-bit signed integers (`i64` in LLVM IR). This allows:
- Values from -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
- Negative enum values
- Large explicit values

## Best Practices

### 1. Use Descriptive Names

```nexuslang
# Good
enum LogLevel
    DEBUG
    INFO
    ERROR

# Bad
enum LL
    D
    I
    E
```

### 2. Group Related Constants

```nexuslang
# Good - related HTTP status codes
enum HttpStatus
    OK = 200
    NotFound = 404
    ServerError = 500

# Bad - unrelated constants
enum MixedValues
    MaxUsers = 100
    ServerPort = 8080
    ErrorCode = 404
```

### 3. Use Explicit Values for Important Constants

```nexuslang
# Good - explicit values for external API compatibility
enum ApiStatus
    Success = 0
    InvalidToken = 401
    RateLimited = 429

# Less ideal - auto-numbering for external APIs
enum ApiStatus
    Success      # What if order changes?
    InvalidToken
    RateLimited
```

### 4. Prefix Enum Members Consistently

Some prefer prefixing enum members for clarity:

```nexuslang
enum Color
    ColorRed
    ColorGreen
    ColorBlue

# Or use the enum name in context
set my_color to Color.Red  # Clear from context
```

## Common Patterns

### State Machines

```nexuslang
enum ConnectionState
    Disconnected
    Connecting
    Connected
    Error

set state to ConnectionState.Disconnected

# State transitions...
```

### Configuration Flags

```nexuslang
enum Feature
    FeatureA = 1
    FeatureB = 2
    FeatureC = 4
    FeatureD = 8

# Could be used with bitwise operations (when implemented)
```

### Error Codes

```nexuslang
enum ErrorCode
    NoError = 0
    FileNotFound = 1
    PermissionDenied = 2
    NetworkError = 3
    InvalidInput = 4

function handle_error with code as Integer
    switch code
        case 0
            print text "Success"
        case 1
            print text "File not found"
        case 2
            print text "Permission denied"
        default
            print text "Unknown error"
```

## Limitations

1. **Integer values only**: Enum values must be integers (no strings, floats, etc.)
   ```nexuslang
   # Not supported
   enum Message
       Hello = "hello"
       Goodbye = "goodbye"
   ```

2. **No method definitions**: Enums are simple constants, not full types
   ```nexuslang
   # Not supported
   enum Color
       Red
       function to_string returns String
           # ...
   ```

3. **No automatic string conversion**: Converting enum to string requires manual mapping
   ```nexuslang
   # Need to manually map values to strings
   ```

## Comparison with Other Languages

### C/C++ Style

```c
// C/C++
enum Color {
    RED,
    GREEN,
    BLUE
};
```

```nexuslang
# NexusLang
enum Color
    Red
    Green
    Blue
```

### Python Style

```python
# Python
from enum import Enum
class Color(Enum):
    RED = 0
    GREEN = 1
    BLUE = 2
```

```nexuslang
# NexusLang
enum Color
    Red
    Green
    Blue
```

### Rust Style

```rust
// Rust
enum Color {
    Red,
    Green,
    Blue,
}
```

```nexuslang
# NexusLang
enum Color
    Red
    Green
    Blue
```

## Testing

Comprehensive test suite in `test_programs/compiler/test_enum_types.nlpl`:
- Basic auto-numbered enums
- Enums with explicit values
- Enums in switch statements
- Enum comparisons
- Multiple enums
- Log levels example
- Enums with gaps in values

Run tests:
```bash
./nlplc test_programs/compiler/test_enum_types.nlpl --run
```

## See Also

- Switch Statement (for using enums in multi-way branching)
- Constants (for simple named values)
- Type System Documentation
