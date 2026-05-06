# Common Tasks

## File Operations

### Read a Whole File

```nlpl
import io
set content to io.read_file with "notes.txt"
print text content
```

### Read a File Line by Line

```nlpl
import io
set lines to io.read_lines with "data.txt"
for each line in lines
    print text line
```

### Write to a File

```nlpl
import io
io.write_file with "output.txt" and "Hello, file!"
```

### Append to a File

```nlpl
import io
io.append_file with "log.txt" and "New entry\n"
```

### Check if a File Exists

```nlpl
import io
if io.file_exists with "config.txt"
    print text "Config found"
else
    print text "Config missing"
```

### Copy / Move / Delete

```nlpl
import io
io.copy_file   with "original.txt" and "backup.txt"
io.move_file   with "old_name.txt" and "new_name.txt"
io.delete_file with "temp.txt"
```

### List a Directory

```nlpl
import io
set files to io.list_directory with "./src"
for each filename in files
    print text filename
```

---

## JSON

### Parse JSON

```nlpl
import io
set text to io.read_file with "data.json"
set obj to io.parse_json with text
print text obj["name"]
```

### Write JSON

```nlpl
import io
set record to {"name": "Alice", "age": 30, "active": true}
io.write_file with "user.json" and (io.to_json with record)
```

### Pretty-Print JSON

```nlpl
import io
set record to {"x": 1, "y": 2}
print text (io.to_json_pretty with record)
```

---

## CSV

### Read CSV

```nlpl
import io

set lines to io.read_lines with "people.csv"
set header to lines[0]
set rows to lines[1 to length(lines)]

for each row in rows
    set fields to io.split with row and ","
    print text "Name: " plus fields[0] plus ", Age: " plus fields[1]
```

### Write CSV

```nlpl
import io

set header to "name,age,email"
set rows to [
    "Alice,30,alice@example.com",
    "Bob,25,bob@example.com"
]
set lines to [header]
for each r in rows
    append r to lines

io.write_file with "output.csv" and (io.join with lines and separator: "\n")
```

---

## HTTP Requests

### GET

```nlpl
import network
set response to network.http_get with "https://api.example.com/items"
print text response
```

### POST JSON

```nlpl
import network, io
set payload to {"name": "Widget", "price": 9.99}
set body to io.to_json with payload
set headers to {"Content-Type": "application/json"}
set response to network.http_post with "https://api.example.com/items" and body and headers
print text response
```

### Async GET (Recommended)

```nlpl
import network

async function fetch with url as String returns String
    return await network.async_http_get with url
end

set data to await fetch with "https://api.example.com/data"
```

### Multiple Concurrent Requests

```nlpl
import network

set urls to [
    "https://api.example.com/users/1",
    "https://api.example.com/users/2",
    "https://api.example.com/users/3"
]

async function fetch_all_users returns List of String
    set tasks to []
    for each url in urls
        append (network.async_http_get with url) to tasks
    return await gather with tasks
end

set responses to await fetch_all_users()
```

---

## CLI Argument Parsing

```nlpl
import argparse_utils

set parser to argparse_utils.new_parser with description: "My NexusLang tool"
argparse_utils.add_argument with parser and "--input"  and type: "string" and required: true
argparse_utils.add_argument with parser and "--output" and type: "string" and default: "out.txt"
argparse_utils.add_argument with parser and "--verbose" and type: "flag"

set args to argparse_utils.parse with parser
set input_path  to args["input"]
set output_path to args["output"]
set verbose     to args["verbose"]

if verbose
    print text "Input: "  plus input_path
    print text "Output: " plus output_path
```

---

## String Manipulation

### Split and Join

```nlpl
import string

set csv_line to "Alice,30,Engineer"
set parts to string.split with csv_line and delimiter: ","
# parts = ["Alice", "30", "Engineer"]

set rejoined to string.join with parts and separator: " | "
print text rejoined    # Alice | 30 | Engineer
```

### Trim Whitespace

```nlpl
import string
set raw to "  hello world  "
print text string.trim with raw       # "hello world"
print text string.trim_left with raw  # "hello world  "
```

### Replace and Contains

```nlpl
import string
set text to "Hello, World!"
set updated to string.replace with text and "World" and "NexusLang"
print text updated                         # Hello, NLPL!
print text string.contains with text and "World"  # true
```

### Regular Expressions

```nlpl
import regex

set email_pattern to "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}"
set text to "Contact us at support@example.com or info@company.org"

set matches to regex.find_all with pattern: email_pattern and text: text
for each match in matches
    print text match
```

---

## Collections

### Stack (LIFO)

```nlpl
import collections

set stack to collections.new_stack()
collections.push with stack and "first"
collections.push with stack and "second"
collections.push with stack and "third"

while not collections.stack_empty with stack
    print text collections.pop with stack
# prints: third, second, first
```

### Queue (FIFO)

```nlpl
import collections

set q to collections.new_queue()
collections.enqueue with q and "task1"
collections.enqueue with q and "task2"

while not collections.queue_empty with q
    print text collections.dequeue with q
# prints: task1, task2
```

### Set (Unique Values)

```nlpl
import collections

set seen to collections.new_set()
set data to [1, 2, 2, 3, 3, 3, 4]
set unique to []

for each item in data
    if not collections.set_contains with seen and item
        collections.set_add with seen and item
        append item to unique

# unique = [1, 2, 3, 4]
```

---

## Multithreading

### Run Tasks in Parallel

```nlpl
import system

function heavy_task with id as Integer returns Integer
    set result to 0
    set i to 0
    while i is less than 1000000
        set result to result plus i
        set i to i plus 1
    return result
end

set handles to []
set i to 0
while i is less than 4
    append (system.spawn_thread with heavy_task and i) to handles
    set i to i plus 1

set results to []
for each h in handles
    append (system.join_thread with h) to results

for each r in results
    print text convert r to string
```

---

## Error Handling Patterns

### Validate Input

```nlpl
function require_positive with n as Integer returns Integer
    if n is less than or equal to 0
        raise error with "Expected positive integer, got " plus convert n to string
    return n
end
```

### Default on Failure

```nlpl
function read_config_or_default with path as String returns String
    try
        return io.read_file with path
    catch error with message
        return "{}"   # empty JSON object as default
end
```

### Retry with Backoff

```nlpl
import system

async function fetch_with_retry with url as String and max_attempts as Integer returns String
    set attempt to 0
    while attempt is less than max_attempts
        try
            return await network.async_http_get with url
        catch error with message
            set attempt to attempt plus 1
            if attempt equals max_attempts
                raise error with "All " plus convert max_attempts to string plus " attempts failed: " plus message
            system.sleep_ms with (attempt times 500)
    return ""
end
```

---

## Date and Time

```nlpl
import datetime_utils

set now to datetime_utils.now()
set formatted to datetime_utils.format with now and "YYYY-MM-DD HH:mm:ss"
print text "Current time: " plus formatted

set timestamp to datetime_utils.timestamp with now   # Unix seconds
print text convert timestamp to string

set tomorrow to datetime_utils.add_days with now and 1
print text datetime_utils.format with tomorrow and "YYYY-MM-DD"
```

---

## Logging

```nlpl
import logging_utils

set log to logging_utils.create_logger with name: "my_app" and level: "INFO"
logging_utils.log_info  with log and "Application started"
logging_utils.log_warn  with log and "Low memory"
logging_utils.log_error with log and "Failed to connect"
logging_utils.set_file  with log and "app.log"
```

---

## UUID Generation

```nlpl
import uuid_utils

set id to uuid_utils.new_uuid_v4()
print text id     # e.g. "550e8400-e29b-41d4-a716-446655440000"
```

---

## Hashing and Cryptography

```nlpl
import crypto

set hash to crypto.sha256 with "Hello, World!"
print text hash    # hex digest

set key to crypto.random_bytes with 32
set iv  to crypto.random_bytes with 16
set ciphertext to crypto.aes256_encrypt with plaintext: "secret" and key: key and iv: iv
set plaintext  to crypto.aes256_decrypt with ciphertext: ciphertext and key: key and iv: iv
```
