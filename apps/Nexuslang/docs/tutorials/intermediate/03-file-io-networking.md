# Tutorial 8: File I/O and Networking

**Time:** ~60 minutes  
**Prerequisites:** [Error Handling](../beginner/04-error-handling.md)

---

## Part 1 — Reading Files

```nexuslang
import io

# Read an entire file as a string
set content to io.read_file with "data.txt"
print text content
```

If the file does not exist, `read_file` raises an error — handle it:

```nexuslang
try
    set content to io.read_file with "config.txt"
    print text content
catch error with message
    print text "File not found: " plus message
```

### Reading Line by Line

```nexuslang
import io

set lines to io.read_lines with "report.csv"
for each line in lines
    print text line
```

---

## Part 2 — Writing Files

```nexuslang
import io

io.write_file with "output.txt" and "Hello, file!"

# Append to an existing file
io.append_file with "log.txt" and "Entry: something happened\n"
```

### Writing Multiple Lines

```nexuslang
import io

set rows to ["Alice,30,Engineer", "Bob,25,Designer", "Carol,35,Manager"]
set content to ""
for each row in rows
    set content to content plus row plus "\n"

io.write_file with "people.csv" and content
```

---

## Part 3 — File System Operations

```nexuslang
import io

# Check existence
if io.file_exists with "settings.txt"
    print text "Settings found"

# List directory
set files to io.list_directory with "./data"
for each filename in files
    print text filename

# Delete
io.delete_file with "temp.txt"

# Copy / move
io.copy_file with "source.txt" and "backup.txt"
io.move_file with "old_name.txt" and "new_name.txt"

# Create directory
io.create_directory with "output/reports"
```

---

## Part 4 — HTTP Requests

```nexuslang
import network

# GET request
set response to network.http_get with "https://api.example.com/users"
print text response

# POST request with JSON body
set body to "{\"name\": \"Alice\", \"email\": \"alice@example.com\"}"
set result to network.http_post with "https://api.example.com/users" and body
print text result
```

### GET with Headers

```nexuslang
import network

set headers to {"Authorization": "Bearer my_token", "Accept": "application/json"}
set response to network.http_get_with_headers with "https://api.example.com/profile" and headers
print text response
```

### Async HTTP (Recommended for Real Programs)

```nexuslang
import network

async function get_user with id as Integer returns String
    set url to "https://api.example.com/users/" plus convert id to string
    return await network.async_http_get with url
end

set user_json to await get_user with 42
print text user_json
```

---

## Part 5 — Parsing JSON

```nexuslang
import io

set json_text to "{\"name\": \"Alice\", \"age\": 30}"
set data to io.parse_json with json_text

set name to data["name"]
set age  to data["age"]
print text name plus " is " plus convert age to string plus " years old."
```

### Writing JSON

```nexuslang
import io

set record to {"product": "Widget", "price": 9.99, "in_stock": true}
set json_out to io.to_json with record
io.write_file with "product.json" and json_out
```

---

## Part 6 — CSV Processing

```nexuslang
import io

set lines to io.read_lines with "sales.csv"
# Skip header
set header to lines[0]
set rows to lines[1 to length(lines)]

set total_sales to 0.0
for each row in rows
    set fields to io.split with row and ","
    set amount to convert fields[2] to float
    set total_sales to total_sales plus amount

print text "Total: $" plus convert total_sales to string
```

---

## Part 7 — TCP Sockets (Low-Level Networking)

```nexuslang
import network

# Server side
set server to network.create_server with "0.0.0.0" and 8080
print text "Listening on port 8080..."
set client to network.accept_connection with server
set message to network.receive with client
print text "Received: " plus message
network.send with client and "ACK: " plus message
network.close with client
network.close with server
```

```nexuslang
# Client side
import network

set conn to network.connect with "127.0.0.1" and 8080
network.send with conn and "Hello from client"
set reply to network.receive with conn
print text "Server replied: " plus reply
network.close with conn
```

---

## Summary

| Operation | Call |
|-----------|------|
| Read file | `io.read_file with "path"` |
| Read lines | `io.read_lines with "path"` |
| Write file | `io.write_file with "path" and "content"` |
| Append file | `io.append_file with "path" and "text"` |
| HTTP GET | `network.http_get with url` |
| HTTP POST | `network.http_post with url and body` |
| Parse JSON | `io.parse_json with text` |
| Write JSON | `io.to_json with dict` |

**Next:** [FFI and C Libraries](04-ffi-and-c-libraries.md)
