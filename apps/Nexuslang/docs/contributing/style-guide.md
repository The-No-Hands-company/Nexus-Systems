# NexusLang Style Guide

## Introduction

This style guide provides guidelines for writing clean, readable, and maintainable code in the NexusLang (NexusLang). The goal is to make code that reads like natural English while maintaining programming precision and clarity.

## General Principles

1. **Readability First**: Code should be self-documenting and read like natural English prose.
2. **Consistency**: Follow consistent patterns and naming conventions throughout the codebase.
3. **Clarity**: Use clear, descriptive names and avoid ambiguity.
4. **Simplicity**: Prefer simple, straightforward constructs over complex ones.

## Naming Conventions

### Classes

- Use PascalCase for class names
- Use descriptive nouns or noun phrases
- Examples:

  ```nlpl
  class BankAccount
  class StudentRecord
  class DatabaseConnection
  ```

### Functions and Methods

- Use snake_case for function and method names
- Use descriptive verbs or verb phrases
- Examples:

  ```nlpl
  function calculate_average
  function process_payment
  function validate_input
  ```

### Variables

- Use snake_case for variable names
- Use descriptive nouns or noun phrases
- Examples:

  ```nlpl
  set user_name to "John"
  set account_balance to 1000.00
  set is_valid to true
  ```

### Constants

- Use UPPER_SNAKE_CASE for constant names
- Use descriptive nouns or noun phrases
- Examples:

  ```nlpl
  set MAX_RETRY_ATTEMPTS to 3
  set DEFAULT_TIMEOUT to 30
  set API_VERSION to "1.0"
  ```

## Code Structure

### Class Definitions

```nlpl
class Student
    private set name to String
    private set age to Integer
    private set grades to List of Float
    
    public function initialize with name as String and age as Integer
        set this.name to name
        set this.age to age
        create this.grades as empty List of Float
    
    public function add grade with score as Float
        append score to this.grades
    
    public function calculate average returns Float
        if this.grades is empty
            return 0.0
        
        set total to 0.0
        for each grade in this.grades
            set total to total plus grade
        
        return total divided by length of this.grades
```

### Function Definitions

```nlpl
function process payment with amount as Float and account as BankAccount returns Boolean
    if amount is less than or equal to 0
        raise error with message "Invalid payment amount"
    
    if account.balance is less than amount
        return false
    
    set account.balance to account.balance minus amount
    return true
```

### Control Flow

#### If Statements

```nlpl
if user.age is greater than or equal to 18
    print text "User is an adult"
else if user.age is greater than or equal to 13
    print text "User is a teenager"
else
    print text "User is a child"
```

#### Loops

##### For Loops

```nlpl
for each item in items
    process item
```

##### While Loops

```nlpl
while attempts is less than MAX_RETRY_ATTEMPTS
    try
        connect to server
        break
    catch error
        set attempts to attempts plus 1
        wait for 1 second
```

### Error Handling

```nlpl
try
    open file "data.txt" for reading
    set content to read from file
    close file
catch error with message
    print text "Error reading file: " plus message
```

### File Operations

```nlpl
if file "config.json" exists
    open file "config.json" for reading
    set config to read from file
    close file
else
    create new file "config.json"
    write default config to file
    close file
```

### Database Operations

```nlpl
connect to database "users.db"
try
    execute query "SELECT * FROM users WHERE age > ?" with [18]
    set results to read results
finally
    disconnect from database
```

### Network Operations

```nlpl
connect to websocket "ws://example.com"
try
    send message "Hello, server!"
    set response to receive message
finally
    disconnect from websocket
```

## Best Practices

1. **Indentation**
   - Use 4 spaces for indentation
   - Indent nested blocks consistently

2. **Line Length**
   - Keep lines under 100 characters
   - Break long lines at logical points

3. **Comments**
   - Use comments sparingly - code should be self-documenting
   - When needed, use full sentences ending with a period
   - Examples:

     ```nlpl
     # Calculate the total cost including tax and shipping.
     set total to base_price plus tax plus shipping_cost
     ```

4. **Whitespace**
   - Use blank lines to separate logical sections
   - Use spaces around operators
   - No trailing whitespace

5. **Type Annotations**
   - Always specify types for function parameters and return values
   - Use descriptive type names
   - Examples:

     ```nlpl
     function calculate total with prices as List of Float returns Float
     function validate user with credentials as Dictionary of String returns Boolean
     ```

6. **Error Handling**
   - Use try-catch blocks for operations that might fail
   - Provide meaningful error messages
   - Clean up resources in finally blocks

7. **Resource Management**
   - Always close files, database connections, and network connections
   - Use try-finally blocks for cleanup
   - Consider using with blocks for automatic resource management

8. **Testing**
   - Write unit tests for all functions and classes
   - Use descriptive test names
   - Test both success and error cases
   - Example:

     ```nlpl
     function test calculate average
         create student as new Student with "John" and 20
         add grade 85.5 to student
         add grade 90.0 to student
         assert student.calculate average is equal to 87.75
     ```

## Common Patterns

### Singleton Pattern

```nlpl
class DatabaseConnection
    private static set instance to null
    
    public static function get instance returns DatabaseConnection
        if instance is null
            create instance as new DatabaseConnection
        return instance
```

### Factory Pattern

```nlpl
class PaymentProcessor
    public static function create with type as String returns PaymentProcessor
        switch type
            case "credit"
                return new CreditCardProcessor
            case "paypal"
                return new PayPalProcessor
            default
                raise error with message "Unsupported payment type"
```

### Observer Pattern

```nlpl
class EventEmitter
    private set listeners to Dictionary of List of Function
    
    public function on with event as String and callback as Function
        if event not in listeners
            create listeners[event] as empty List of Function
        append callback to listeners[event]
    
    public function emit with event as String and data as Any
        if event in listeners
            for each callback in listeners[event]
                call callback with data
```

## Conclusion

This style guide provides a foundation for writing clean, readable, and maintainable NexusLang code. Remember that the goal is to make code that reads like natural English while maintaining programming precision and clarity. Follow these guidelines consistently, but also use common sense and adapt when necessary for specific situations.
