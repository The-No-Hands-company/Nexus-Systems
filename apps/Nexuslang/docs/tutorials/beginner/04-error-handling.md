# Tutorial 4: Error Handling

**Time:** ~30 minutes  
**Prerequisites:** [Variables, Functions, and Control Flow](02-variables-functions-control-flow.md)

---

## Part 1 — Why Errors Happen

Programs fail for many reasons: a file might not exist, a user might type
unexpected input, a network might be unavailable.  NexusLang provides structured
mechanisms to detect these situations and handle them gracefully.

---

## Part 2 — try / catch

Wrap code that might fail in a `try` block.  If an error occurs, execution
jumps to the matching `catch` block.

```nexuslang
try
    set result to 10 divided by 0
    print text "Result: " plus convert result to string
catch error with message
    print text "Something went wrong: " plus message
```

Without the `try / catch`, a division by zero would crash the program.  With
it, the program continues running after the error.

### Catching File Errors

```nexuslang
try
    set content to read_file("data.txt")
    print text content
catch error with message
    print text "Could not read file: " plus message
```

### Catching Network Errors

```nexuslang
try
    set response to http_get("https://api.example.com/data")
    print text response
catch error with message
    print text "Request failed: " plus message
```

---

## Part 3 — Nested try/catch

You can nest `try/catch` blocks when different operations can fail with
different meanings:

```nexuslang
try
    set raw to read_file("config.txt")
    try
        set value to parse_integer(raw)
        print text "Config value: " plus convert value to string
    catch error with parse_message
        print text "Config file has invalid content: " plus parse_message
catch error with read_message
    print text "Config file missing: " plus read_message
```

---

## Part 4 — Raising Errors

Use `raise error with "message"` to signal an error from your own code:

```nexuslang
function divide with a as Float and b as Float returns Float
    if b equals 0.0
        raise error with "Cannot divide by zero"
    return a divided by b
end

try
    set result to divide with 10.0 and 0.0
catch error with message
    print text "Error caught: " plus message
```

---

## Part 5 — Validation Pattern

A common pattern is to validate input at the start of a function:

```nexuslang
function set_age with age as Integer
    if age is less than 0
        raise error with "Age cannot be negative"
    if age is greater than 150
        raise error with "Age value is unreasonably large"
    print text "Age set to " plus convert age to string
end

try
    set_age with -5
catch error with message
    print text "Validation failed: " plus message
```

---

## Part 6 — always Block

A block that runs whether or not an error occurred — useful for cleanup:

```nexuslang
try
    set file_handle to open_file("log.txt", "write")
    write_to_file with file_handle and "Processing started"
    set data to read_file("input.txt")
    write_to_file with file_handle and "Done"
catch error with message
    write_to_file with file_handle and "Error: " plus message
always
    close_file with file_handle
```

---

## Part 7 — Letting Errors Propagate

Not every function needs to handle every error.  If a function does not have
a `try/catch`, any error it encounters propagates up to the caller:

```nexuslang
function load_config returns String
    # If the file is missing, the error bubbles up to whoever calls load_config
    return read_file("config.txt")
end

try
    set cfg to load_config()
    print text "Loaded: " plus cfg
catch error with message
    print text "Startup failed: " plus message
```

---

## Summary

| Concept | Syntax |
|---------|--------|
| Handle errors | `try { … } catch error with msg { … }` |
| Cleanup code | `always { … }` |
| Signal an error | `raise error with "message"` |
| Validate input | Check conditions and raise at function start |
| Propagate errors | Omit `try/catch`; caller handles it |

**Next:** [Modules and Imports](05-modules-and-imports.md)
