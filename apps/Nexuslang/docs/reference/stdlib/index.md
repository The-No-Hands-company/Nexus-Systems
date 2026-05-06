# NexusLang Standard Library API Reference

**Total Modules:** 62  
**Status:** Production-ready

---

## Overview

The NexusLang standard library provides comprehensive functionality organized into logical categories. All modules are fully implemented and tested.

**Design Principles:**
- Zero dependencies beyond Python stdlib
- Production-ready implementations (no shortcuts!)
- Natural language function names with short aliases
- Consistent error handling
- Full type annotations

**Import Syntax:**
```nlpl
import module_name
from module_name import function_name
import module_name as alias
```

---

## Table of Contents

### Core Modules
1. [math](#math) - Mathematical operations and constants
2. [string](#string) - String manipulation
3. [io](#io) - Input/output operations
4. [system](#system) - System interactions
5. [collections](#collections) - Data structures (Vec, HashMap, Set)
6. [network](#network) - Networking utilities

### Graphics & Rendering
7. [graphics](#graphics) - 2D/3D graphics primitives
8. [vulkan](#vulkan) - Vulkan API for GPU programming
9. [rendering](#rendering) - Rendering pipeline
10. [shaders](#shaders) - Shader compilation and management
11. [camera](#camera) - Camera controls
12. [image_utils](#image_utils) - Image processing
13. [mesh_loader](#mesh_loader) - 3D mesh loading
14. [scene](#scene) - Scene graph management
15. [math3d](#math3d) - 3D mathematics

### Data Formats
16. [json_utils](#json_utils) - JSON parsing/serialization
17. [csv_utils](#csv_utils) - CSV file handling
18. [xml_utils](#xml_utils) - XML parsing
19. [yaml](#yaml) - YAML support (via serialization module)
20. [toml](#toml) - TOML configuration (via serialization module)
21. [pdf_utils](#pdf_utils) - PDF generation

### Databases
22. [databases](#databases) - Generic database interface
23. [sqlite](#sqlite) - SQLite embedded database

### Web & Network
24. [http](#http) - HTTP client/server
25. [websocket_utils](#websocket_utils) - WebSocket support
26. [email_utils](#email_utils) - Email sending

### System Programming
27. [filesystem](#filesystem) - File system operations
28. [path_utils](#path_utils) - Path manipulation
29. [subprocess_utils](#subprocess_utils) - Process management
30. [threading_utils](#threading_utils) - Threading support
31. [asyncio_utils](#asyncio_utils) - Async I/O
32. [signal_utils](#signal_utils) - Signal handling
33. [env](#env) - Environment variables
34. [errno](#errno) - Error numbers
35. [limits](#limits) - System limits
36. [interrupts](#interrupts) - Interrupt handling

### Low-Level
37. [asm](#asm) - Inline assembly utilities
38. [ffi](#ffi) - Foreign function interface
39. [ctype](#ctype) - C type conversions
40. [bit_ops](#bit_ops) - Bit manipulation
41. [simd](#simd) - SIMD operations

### Utilities
42. [datetime_utils](#datetime_utils) - Date/time operations
43. [uuid_utils](#uuid_utils) - UUID generation
44. [validation](#validation) - Input validation
45. [templates](#templates) - Template engine
46. [logging_utils](#logging_utils) - Logging framework
47. [testing](#testing) - Unit testing
48. [algorithms](#algorithms) - Common algorithms
49. [iterators](#iterators) - Iterator utilities
50. [cache](#cache) - Caching mechanisms
51. [random_utils](#random_utils) - Random numbers
52. [statistics](#statistics) - Statistical functions
53. [config](#config) - Configuration management
54. [argparse_utils](#argparse_utils) - Command-line parsing

### Advanced Types
55. [option_result](#option_result) - Option<T> and Result<T,E>
56. [types](#types) - Type utilities
57. [type_traits](#type_traits) - Type introspection
58. [modules](#modules) - Module system utilities

### Data Processing
59. [compression](#compression) - Compression (gzip, zip)
60. [serialization](#serialization) - Object serialization
61. [regex](#regex) - Regular expressions
62. [stringbuilder](#stringbuilder) - Efficient string building
63. [crypto](#crypto) - Cryptographic operations

---

## Core Modules

### math

Mathematical operations and constants.

**Constants:**
- `PI` - Pi (3.14159...)
- `E` - Euler's number (2.71828...)
- `TAU` - Tau (2π)
- `INF` - Infinity
- `NAN` - Not a Number

**Basic Operations:**
```nlpl
import math

# Absolute value
set x to absolute of -5  # x = 5
set x to abs of -5       # Short alias

# Square root
set x to square_root of 16  # x = 4.0
set x to sqrt of 16         # Short alias

# Power
set x to power of 2, 8  # x = 256
set x to pow of 2, 8    # Short alias

# Rounding
set x to floor of 3.7      # x = 3
set x to ceiling of 3.2    # x = 4
set x to ceil of 3.2       # Short alias
set x to round of 3.5      # x = 4
set x to truncate of 3.9   # x = 3
set x to trunc of 3.9      # Short alias

# Sign
set x to sign of -5  # x = -1
set x to sign of 5   # x = 1
set x to sign of 0   # x = 0

# Number theory
set x to gcd of 12, 8    # x = 4 (greatest common divisor)
set x to lcm of 4, 6     # x = 12 (least common multiple)
set x to factorial of 5  # x = 120
```

**Trigonometric Functions:**
```nlpl
# Basic trig
set x to sine of 1.57      # sin(π/2) ≈ 1.0
set x to sin of 1.57       # Short alias
set x to cosine of 0       # cos(0) = 1.0
set x to cos of 0          # Short alias
set x to tangent of 0.785  # tan(π/4) ≈ 1.0
set x to tan of 0.785      # Short alias

# Inverse trig
set x to arcsine of 1      # asin(1) = π/2
set x to asin of 1         # Short alias
set x to arccosine of 0    # acos(0) = π/2
set x to acos of 0         # Short alias
set x to arctangent of 1   # atan(1) = π/4
set x to atan of 1         # Short alias
set x to arctangent2 of 1, 1  # atan2(1,1) = π/4
set x to atan2 of 1, 1     # Short alias

# Hyperbolic trig
set x to sinh of 1  # Hyperbolic sine
set x to cosh of 1  # Hyperbolic cosine
set x to tanh of 1  # Hyperbolic tangent
```

**Exponential & Logarithmic:**
```nlpl
set x to exponential of 1     # e^1
set x to exp of 1             # Short alias
set x to logarithm of 10      # ln(10)
set x to log of 10            # Short alias
set x to log10 of 100         # log₁₀(100) = 2
set x to log2 of 8            # log₂(8) = 3
```

**Min/Max:**
```nlpl
set x to minimum of 5, 3  # x = 3
set x to min of 5, 3      # Short alias
set x to maximum of 5, 3  # x = 5
set x to max of 5, 3      # Short alias
```

---

### string

String manipulation and validation.

**Basic Operations:**
```nlpl
import string

# Length
set len to length of "hello"  # len = 5
set len to len of "hello"     # Short alias

# Case conversion
set upper to upper of "hello"      # "HELLO"
set lower to lower of "HELLO"      # "hello"
set cap to capitalize of "hello"   # "Hello"
set title to title of "hello world"  # "Hello World"

# Trimming
set trimmed to trim of "  hello  "         # "hello"
set left to trim_left of "  hello"         # "hello"
set right to trim_right of "hello  "       # "hello"
set custom to trim_chars of "xxhelloxx", "x"  # "hello"

# Searching
if contains "hello", "ell"  # true
if starts_with "hello", "hel"  # true
if ends_with "hello", "lo"     # true
set idx to index_of "hello", "l"  # idx = 2 (first occurrence)
set idx to last_index_of "hello", "l"  # idx = 3

# Character access
set char to char_at "hello", 1  # char = "e"
```

**Splitting & Joining:**
```nlpl
# Split string
set parts to split "a,b,c", ","  # ["a", "b", "c"]
set lines to split_lines "line1\nline2\nline3"  # ["line1", "line2", "line3"]

# Join strings
set result to join ["a", "b", "c"], ","  # "a,b,c"
set result to join_with_separator ["a", "b"], " - "  # "a - b"
```

**Replacement:**
```nlpl
set result to replace "hello world", "world", "there"  # "hello there"
set result to replace_all "aaa", "a", "b"  # "bbb"
set result to replace_first "aaa", "a", "b"  # "baa"
```

**Validation:**
```nlpl
if is_numeric "123"        # true
if is_alphabetic "abc"     # true
if is_alphanumeric "abc123"  # true
if is_lowercase "hello"    # true
if is_uppercase "HELLO"    # true
if is_empty ""             # true
```

**Padding:**
```nlpl
set result to pad_left "5", 3, "0"   # "005"
set result to pad_right "5", 3, "0"  # "500"
set result to center "hi", 6, " "    # "  hi  "
```

**Regular Expressions:**
```nlpl
# Match pattern
set matches to match "test123", "\\d+"  # ["123"]

# Replace with pattern
set result to replace_regex "test123", "\\d+", "456"  # "test456"
```

---

### io

Input/output operations.

**File Operations:**
```nlpl
import io

# Read entire file
set content to read_file "data.txt"

# Write to file
call write_file with "output.txt", "Hello, world!"

# Append to file
call append_to_file with "log.txt", "New entry\n"

# Read lines
set lines to read_lines "data.txt"  # Returns list of lines

# Check if file exists
if file_exists "data.txt"
  print text "File found"
end
```

**Console I/O:**
```nlpl
# Print to stdout
call print with "Hello"
call println with "Hello"  # With newline

# Read from stdin
set input to read_line
set input to input with "Enter name: "  # With prompt

# Error output
call print_error with "Error message"
```

**Binary I/O:**
```nlpl
# Read binary file
set data to read_binary "image.png"

# Write binary file
call write_binary with "output.bin", data
```

---

### system

System interactions and OS operations.

**Process Management:**
```nlpl
import system

# Execute command
set result to execute "ls -la"
set output to execute_with_output "pwd"

# Get exit code
set code to exit_code of command_result

# Environment
set home to get_env "HOME"
call set_env with "MY_VAR", "value"
set all_vars to environment_variables
```

**System Information:**
```nlpl
set os to operating_system  # "Linux", "Windows", "Darwin"
set platform to platform_name
set arch to architecture  # "x86_64", "aarch64"
set hostname to get_hostname
set username to get_username
```

**Time Operations:**
```nlpl
set timestamp to current_timestamp
set time to current_time
call sleep with 1  # Sleep 1 second
call sleep_ms with 500  # Sleep 500 milliseconds
```

**Exit:**
```nlpl
call exit with 0  # Exit with code 0
call abort  # Abnormal termination
```

---

### collections

Data structures: Vec<T>, HashMap<K,V>, Set<T>.

**Vec (Dynamic Array):**
```nlpl
import collections

# Create vector
set vec to new Vec

# Add elements
call vec.push with 1
call vec.push with 2
call vec.push with 3

# Access elements
set first to vec.get with 0  # Returns Option<T>
set value to vec[0]  # Direct access

# Remove elements
set last to vec.pop  # Returns Option<T>

# Properties
set len to vec.length
set is_empty to vec.is_empty

# Iteration
for each item in vec
  print text item
end
```

**HashMap (Key-Value Store):**
```nlpl
# Create hashmap
set map to new HashMap

# Insert
call map.insert with "name", "Alice"
call map.insert with "age", 25

# Get value
set name to map.get with "name"  # Returns Option<V>

# Check existence
if map.contains_key with "name"
  print text "Key exists"
end

# Remove
call map.remove with "name"

# Iteration
for each key in map.keys
  set value to map.get with key
  print text key plus ": " plus value
end
```

**Set (Unique Elements):**
```nlpl
# Create set
set set to new Set

# Add elements
call set.insert with 1
call set.insert with 2
call set.insert with 1  # Duplicate ignored

# Check membership
if set.contains with 1
  print text "Found"
end

# Remove
call set.remove with 1

# Set operations
set union to set1.union with set2
set intersection to set1.intersection with set2
set difference to set1.difference with set2
```

---

### network

Network operations and protocols.

**HTTP Client:**
```nlpl
import network

# GET request
set response to http_get "https://api.example.com/data"
print text response.body
print text response.status_code

# POST request
set body to {"key": "value"}
set response to http_post "https://api.example.com/data", body

# With headers
set headers to {"Authorization": "Bearer token"}
set response to http_get_with_headers "https://api.example.com", headers
```

**Sockets:**
```nlpl
# TCP client
set socket to tcp_connect "localhost", 8080
call socket.send with "Hello"
set data to socket.receive
call socket.close

# TCP server
set server to tcp_listen "0.0.0.0", 8080
set client to server.accept
set data to client.receive
call client.send with "Response"
call client.close
call server.close
```

**URL Parsing:**
```nlpl
set url to parse_url "https://example.com:8080/path?key=value"
print text url.scheme    # "https"
print text url.host      # "example.com"
print text url.port      # 8080
print text url.path      # "/path"
print text url.query     # {"key": "value"}
```

---

## Graphics & Rendering

### graphics

2D and 3D graphics primitives.

**Basic Drawing:**
```nlpl
import graphics

# Initialize graphics context
set ctx to create_context with 800, 600

# Draw shapes
call draw_rectangle with ctx, 10, 10, 100, 50
call draw_circle with ctx, 200, 200, 50
call draw_line with ctx, 0, 0, 100, 100

# Colors
call set_color with ctx, 255, 0, 0  # Red
call set_color_rgb with ctx, 0.5, 0.5, 1.0  # Light blue

# Text
call draw_text with ctx, "Hello", 50, 50
call set_font with ctx, "Arial", 24

# Clear screen
call clear with ctx
call clear_color with ctx, 0, 0, 0  # Black
```

**Transformations:**
```nlpl
# Translation
call translate with ctx, 50, 50

# Rotation
call rotate with ctx, 45  # Degrees

# Scaling
call scale with ctx, 2.0, 2.0

# Reset transform
call reset_transform with ctx
```

---

### vulkan

Vulkan API bindings for GPU programming.

**Initialization:**
```nlpl
import vulkan

# Create instance
set instance to create_vulkan_instance

# Select physical device
set devices to enumerate_physical_devices with instance
set device to devices[0]

# Create logical device
set logical_device to create_device with device

# Create swapchain
set swapchain to create_swapchain with logical_device, width, height
```

**Rendering:**
```nlpl
# Create render pass
set render_pass to create_render_pass with logical_device

# Create pipeline
set pipeline to create_graphics_pipeline with logical_device, render_pass

# Command buffer
set cmd_buffer to create_command_buffer with logical_device
call begin_command_buffer with cmd_buffer
call bind_pipeline with cmd_buffer, pipeline
call draw with cmd_buffer, vertex_count
call end_command_buffer with cmd_buffer

# Submit to queue
call submit_to_queue with queue, cmd_buffer
```

---

## Data Formats

### json_utils

JSON parsing and serialization.

**Parsing:**
```nlpl
import json_utils

# Parse JSON string
set data to parse_json '{"name": "Alice", "age": 25}'
print text data["name"]  # "Alice"

# Parse from file
set data to parse_json_file "data.json"
```

**Serialization:**
```nlpl
# Convert to JSON
set obj to {"name": "Alice", "age": 25, "active": true}
set json_str to to_json obj
# Result: '{"name":"Alice","age":25,"active":true}'

# Pretty print
set json_str to to_json_pretty obj
# Result with indentation

# Write to file
call write_json_file with "output.json", obj
```

**JSON Path:**
```nlpl
# Query nested data
set name to json_path data, "$.user.name"
set emails to json_path data, "$.users[*].email"
```

---

### csv_utils

CSV file handling.

**Reading:**
```nlpl
import csv_utils

# Read CSV
set rows to read_csv "data.csv"
for each row in rows
  print text row[0]  # First column
end

# Read with headers
set data to read_csv_with_headers "data.csv"
for each row in data
  print text row["name"]
end
```

**Writing:**
```nlpl
# Write CSV
set rows to [["Alice", "25"], ["Bob", "30"]]
call write_csv with "output.csv", rows

# Write with headers
set headers to ["Name", "Age"]
set data to [["Alice", "25"], ["Bob", "30"]]
call write_csv_with_headers with "output.csv", headers, data
```

---

### xml_utils

XML parsing and generation.

**Parsing:**
```nlpl
import xml_utils

# Parse XML
set doc to parse_xml "<root><item>value</item></root>"
set root to doc.root
set items to root.find_all "item"
```

**Generation:**
```nlpl
# Create XML document
set doc to create_xml_document "root"
set child to doc.add_element "item"
call child.set_attribute with "id", "1"
call child.set_text with "Value"

# Convert to string
set xml_str to doc.to_string
```

---

## Databases

### sqlite

SQLite embedded database.

**Connection:**
```nlpl
import sqlite

# Open database
set db to sqlite_open "data.db"

# Create table
call db.execute "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)"

# Insert data
call db.execute "INSERT INTO users (name) VALUES ('Alice')"

# Query
set rows to db.query "SELECT * FROM users"
for each row in rows
  print text row["name"]
end

# Close
call db.close
```

**Prepared Statements:**
```nlpl
set stmt to db.prepare "INSERT INTO users (name) VALUES (?)"
call stmt.bind with 1, "Bob"
call stmt.execute
```

---

## System Programming

### filesystem

File system operations.

**Directory Operations:**
```nlpl
import filesystem

# Create directory
call create_directory "mydir"
call create_directories "path/to/nested/dir"

# Remove directory
call remove_directory "mydir"
call remove_directory_recursive "mydir"  # With contents

# List directory
set entries to list_directory "."
for each entry in entries
  print text entry.name
  if entry.is_directory
    print text "  (directory)"
  end
end
```

**File Operations:**
```nlpl
# Copy file
call copy_file "source.txt", "dest.txt"

# Move file
call move_file "old.txt", "new.txt"

# Delete file
call delete_file "temp.txt"

# File info
set info to file_info "data.txt"
print text info.size
print text info.modified_time
print text info.permissions
```

**Path Operations:**
```nlpl
# Check existence
if exists "file.txt"
if is_file "file.txt"
if is_directory "mydir"

# Get current directory
set cwd to current_directory

# Change directory
call change_directory "newdir"
```

---

### subprocess_utils

Process spawning and management.

**Execute Commands:**
```nlpl
import subprocess_utils

# Run command
set result to run_command "ls -la"
print text result.stdout
print text result.stderr
print text result.exit_code

# Run with input
set result to run_command_with_input "cat", "Hello"

# Run asynchronously
set process to spawn_process "long_running_command"
# Do other work...
call process.wait
```

**Process Management:**
```nlpl
# Get process ID
set pid to get_pid

# Kill process
call kill_process pid

# Check if process running
if is_process_running pid
  print text "Still running"
end
```

---

### threading_utils

Threading support.

**Creating Threads:**
```nlpl
import threading_utils

# Create thread
function worker
  print text "Thread running"
end

set thread to create_thread worker
call thread.start
call thread.join  # Wait for completion
```

**Thread Synchronization:**
```nlpl
# Mutex
set mutex to create_mutex
call mutex.lock
# Critical section
call mutex.unlock

# Lock guard (automatic unlock)
with_lock mutex
  # Critical section
end
```

**Thread Pool:**
```nlpl
set pool to create_thread_pool 4  # 4 worker threads
call pool.submit worker_function
call pool.shutdown
call pool.wait
```

---

## Low-Level Modules

### asm

Inline assembly utilities (see [inline_assembly.md](../3_core_concepts/inline_assembly.md)).

**Helper Functions:**
```nlpl
import asm

# CPU feature detection
if has_sse42
  # Use SSE4.2 instructions
end

if has_avx2
  # Use AVX2 instructions
end

# Register operations
set rax to read_register "rax"
call write_register with "rax", 42

# Memory barriers
call memory_fence
call load_fence
call store_fence
```

---

### ffi

Foreign function interface (see [ffi.md](../3_core_concepts/ffi.md)).

**Helper Functions:**
```nlpl
import ffi

# Load library
set lib to load_library "libmylib.so"

# Get function
set func to get_function lib, "my_function"

# Call function
set result to call_function func, arg1, arg2

# Type conversion
set c_string to to_c_string "Hello"
set nxl_string to from_c_string c_ptr
```

---

### bit_ops

Bit manipulation utilities.

**Bitwise Operations:**
```nlpl
import bit_ops

# Basic operations
set result to bitwise_and 0b1010, 0b1100  # 0b1000
set result to bitwise_or 0b1010, 0b0101   # 0b1111
set result to bitwise_xor 0b1010, 0b1100  # 0b0110
set result to bitwise_not 0b1010          # Complement

# Shifts
set result to shift_left 0b0001, 3   # 0b1000
set result to shift_right 0b1000, 2  # 0b0010
set result to rotate_left 0b1001, 1  # 0b0011
set result to rotate_right 0b1001, 1 # 0b1100

# Bit manipulation
set result to set_bit 0b0000, 2      # Set bit 2: 0b0100
set result to clear_bit 0b0111, 1    # Clear bit 1: 0b0101
set result to toggle_bit 0b0101, 0   # Toggle bit 0: 0b0100
if test_bit 0b0101, 2                # Test bit 2: true

# Bit counting
set count to count_bits 0b1011  # Count 1s: 3
set count to count_zeros 0b1011 # Count 0s: 29 (in 32-bit)
set pos to find_first_set 0b1010  # Position of first 1: 1
```

---

## Utilities

### datetime_utils

Date and time operations.

**Current Time:**
```nlpl
import datetime_utils

set now to now  # Current datetime
set today to today  # Current date
set time to current_time  # Current time
```

**Formatting:**
```nlpl
set dt to now
set formatted to format_datetime dt, "%Y-%m-%d %H:%M:%S"
# "2026-02-03 14:30:00"

set parsed to parse_datetime "2026-02-03", "%Y-%m-%d"
```

**Date Arithmetic:**
```nlpl
set tomorrow to add_days today, 1
set next_week to add_days today, 7
set last_month to subtract_months today, 1

# Time difference
set diff to difference_in_days date1, date2
set diff to difference_in_seconds time1, time2
```

---

### uuid_utils

UUID generation.

```nlpl
import uuid_utils

# Generate UUID v4 (random)
set id to generate_uuid4
# "550e8400-e29b-41d4-a716-446655440000"

# Generate UUID v1 (time-based)
set id to generate_uuid1

# Parse UUID
set uuid to parse_uuid "550e8400-e29b-41d4-a716-446655440000"
```

---

### validation

Input validation.

**Type Validation:**
```nlpl
import validation

# Validate email
if is_valid_email "user@example.com"
  print text "Valid email"
end

# Validate URL
if is_valid_url "https://example.com"
  print text "Valid URL"
end

# Validate IP address
if is_valid_ipv4 "192.168.1.1"
  print text "Valid IPv4"
end
```

**Range Validation:**
```nlpl
if in_range 5, 1, 10  # Check if 5 is between 1 and 10
if is_positive 5
if is_negative -5
if is_even 4
if is_odd 5
```

---

### logging_utils

Logging framework.

**Basic Logging:**
```nlpl
import logging_utils

# Log levels
call log_debug "Debug message"
call log_info "Info message"
call log_warning "Warning message"
call log_error "Error message"
call log_critical "Critical message"
```

**Logger Configuration:**
```nlpl
# Create logger
set logger to create_logger "myapp"
call logger.set_level "INFO"

# Add file handler
call logger.add_file_handler "app.log"

# Custom format
call logger.set_format "[%level%] %time% - %message%"
```

---

### testing

Unit testing framework.

**Writing Tests:**
```nlpl
import testing

function test_addition
  assert_equal 2 plus 2, 4
  assert_not_equal 2 plus 2, 5
end

function test_strings
  assert_true "hello" starts_with "hel"
  assert_false "hello" ends_with "x"
end
```

**Running Tests:**
```nlpl
set suite to create_test_suite "Math Tests"
call suite.add_test test_addition
call suite.add_test test_subtraction
call suite.run

# Output:
# Running Math Tests...
# test_addition: PASS
# test_subtraction: PASS
# 2 tests, 2 passed, 0 failed
```

---

### option_result

Rust-style Option<T> and Result<T,E> types.

**Option:**
```nlpl
import option_result

# Create Option
set maybe to Some(42)
set nothing to None

# Pattern matching
match maybe with
  case Some(value) then print text value
  case None then print text "No value"
end

# Methods
set doubled to maybe.map lambda x returns x times 2
set filtered to maybe.filter lambda x returns x is greater than 0
set unwrapped to maybe.unwrap_or 0
```

**Result:**
```nlpl
# Create Result
set success to Ok(100)
set failure to Err("Error message")

# Pattern matching
match result with
  case Ok(value) then print text "Success: " plus value
  case Err(msg) then print text "Error: " plus msg
end

# Methods
set mapped to result.map lambda x returns x times 2
set chained to result.and_then lambda x returns Ok(x plus 1)
```

---

## Quick Function Reference

### Common Operations Cheat Sheet

```nlpl
# Math
abs(-5)              # 5
sqrt(16)             # 4.0
pow(2, 8)            # 256
sin(1.57)            # ~1.0
min(5, 3)            # 3
max(5, 3)            # 5

# String
len("hello")                    # 5
upper("hello")                  # "HELLO"
lower("HELLO")                  # "hello"
trim("  hi  ")                  # "hi"
split("a,b,c", ",")            # ["a", "b", "c"]
join(["a", "b"], ",")          # "a,b"
contains("hello", "ell")       # true
replace("hi world", "world", "there")  # "hi there"

# Collections
vec.push(42)
vec.pop()
vec.get(0)
map.insert("key", "value")
map.get("key")
set.insert(1)
set.contains(1)

# File I/O
read_file("data.txt")
write_file("out.txt", "content")
file_exists("test.txt")

# System
execute("ls -la")
get_env("HOME")
sleep(1)
exit(0)
```

---

## Module Loading

All stdlib modules are automatically registered at runtime. Simply import and use:

```nlpl
import math
set result to sqrt of 16

from string import upper, lower
set text to upper of "hello"

import collections as col
set vec to new col.Vec
```

---

## Summary

The NexusLang standard library provides:

✅ **62 modules** covering all common needs  
✅ **Production-ready** implementations  
✅ **Natural language** function names  
✅ **Comprehensive coverage** from math to graphics  
✅ **Zero dependencies** beyond Python stdlib  
✅ **Full documentation** with examples  

**For detailed guides, see:**
- Core concepts: [docs/guide/](../../guide/)
- Pattern matching: [pattern-matching.md](../../guide/pattern-matching.md)
- FFI: [ffi.md](ffi.md)
- Inline assembly: [inline-assembly.md](../../guide/inline-assembly.md)

**Status:** All modules are fully implemented and ready for production use!
