# Domain Recipes

Practical solutions organised by application domain.

---

## Data Processing

### Parse and Filter a CSV

```nlpl
import io

function load_csv with path as String returns List
    set lines to io.read_lines with path
    set header to io.split with lines[0] and delimiter: ","
    set records to []

    set i to 1
    while i is less than length(lines)
        if length(lines[i]) is greater than 0
            set values to io.split with lines[i] and delimiter: ","
            set record to {}
            set j to 0
            while j is less than length(header)
                set record[header[j]] to values[j]
                set j to j plus 1
            append record to records
        set i to i plus 1
    return records
end

set rows to load_csv with "sales.csv"
set filtered to []
for each row in rows
    if convert row["amount"] to float is greater than 1000.0
        append row to filtered

print text "High-value sales: " plus convert length(filtered) to string
```

### Aggregate Totals from CSV

```nlpl
import io

set lines to io.read_lines with "transactions.csv"
set totals to {}

set i to 1
while i is less than length(lines)
    set fields to io.split with lines[i] and delimiter: ","
    set category to fields[0]
    set amount   to convert fields[1] to float

    if totals[category] is null
        set totals[category] to 0.0
    set totals[category] to totals[category] plus amount
    set i to i plus 1

for each category in totals
    print text category plus ": " plus convert totals[category] to string
```

### Transform a JSON Array

```nlpl
import io

set json_text to io.read_file with "products.json"
set products to io.parse_json with json_text

set discounted to []
for each product in products
    set price to product["price"]
    set new_product to {
        "name": product["name"],
        "original_price": price,
        "sale_price": price times 0.9
    }
    append new_product to discounted

io.write_file with "discounted.json" and (io.to_json_pretty with discounted)
```

---

## Web Scraping

### Fetch and Extract Links

```nlpl
import network, regex

async function scrape_links with url as String returns List of String
    set html to await network.async_http_get with url
    set pattern to "href=\"(https?://[^\"]+)\""
    return regex.find_all with pattern: pattern and text: html
end

set links to await scrape_links with "https://example.com"
for each link in links
    print text link
```

### Paginated API

```nlpl
import network, io

async function fetch_all_pages with base_url as String returns List
    set all_items to []
    set page to 1
    set has_more to true

    while has_more
        set url to base_url plus "?page=" plus convert page to string plus "&per_page=100"
        set response to await network.async_http_get with url
        set data to io.parse_json with response
        set items to data["items"]

        for each item in items
            append item to all_items

        set has_more to data["has_next_page"]
        set page to page plus 1

    return all_items
end

set results to await fetch_all_pages with "https://api.example.com/records"
print text "Total records: " plus convert length(results) to string
```

---

## System Automation

### Run a Shell Command Safely

```nlpl
from nexuslang.security import safe_execute

set output to safe_execute with program: "ls" and args: ["-la", "/tmp"] and allowed_programs: ["ls"]
print text output
```

### Process Every File in a Directory

```nlpl
import io

function process_file with path as String returns Boolean
    set content to io.read_file with path
    # ... transform content ...
    set new_content to convert content to uppercase   # placeholder
    io.write_file with path plus ".processed" and new_content
    return true
end

set files to io.list_directory with "./inbox"
for each filename in files
    if io.file_extension with filename equals ".txt"
        set ok to process_file with "./inbox/" plus filename
        if ok
            print text "Processed: " plus filename
```

### Watch a Directory for Changes

```nlpl
import fs_watch

set watcher to fs_watch.watch with path: "./data" and recursive: true
fs_watch.on_created with watcher and function new_file_handler with event
    print text "New file: " plus event["path"]
end
fs_watch.on_modified with watcher and function changed_handler with event
    print text "Changed: " plus event["path"]
end
fs_watch.start with watcher
# Runs until process exits
```

### Send an Email Notification

```nlpl
import email_utils

set mail to email_utils.new_message()
set mail["to"]      to "admin@example.com"
set mail["subject"] to "Build Completed"
set mail["body"]    to "The nightly build finished without errors."

email_utils.send_smtp with message: mail and host: "smtp.example.com" and port: 587 and user: "ci@example.com" and password: read_env with "SMTP_PASSWORD"
```

---

## Scientific Computing

### Numerical Integration (Trapezoidal Rule)

```nlpl
function integrate with f as Function and a as Float and b as Float and steps as Integer returns Float
    set h to (b minus a) divided by convert steps to float
    set total to (f with a) plus (f with b)

    set i to 1
    while i is less than steps
        set x to a plus (convert i to float times h)
        set total to total plus (2.0 times (f with x))
        set i to i plus 1

    return total times h divided by 2.0
end

function f with x as Float returns Float
    return x times x
end

set area to integrate with f and 0.0 and 1.0 and 1000
print text "Integral of x^2 from 0 to 1: " plus convert area to string    # ~0.333
```

### Matrix Multiplication

```nlpl
function mat_mul with a as List and b as List returns List
    set rows_a to length(a)
    set cols_a to length(a[0])
    set cols_b to length(b[0])

    set result to []
    set i to 0
    while i is less than rows_a
        set row to []
        set j to 0
        while j is less than cols_b
            set total to 0.0
            set k to 0
            while k is less than cols_a
                set total to total plus (a[i][k] times b[k][j])
                set k to k plus 1
            append total to row
            set j to j plus 1
        append row to result
        set i to i plus 1
    return result
end

set a to [[1.0, 2.0], [3.0, 4.0]]
set b to [[5.0, 6.0], [7.0, 8.0]]
set c to mat_mul with a and b
print text convert c[0][0] to string    # 19.0
```

### Sieve of Eratosthenes

```nlpl
function primes_up_to with limit as Integer returns List of Integer
    set sieve to []
    set i to 0
    while i is less than or equal to limit
        append true to sieve
        set i to i plus 1

    set sieve[0] to false
    set sieve[1] to false

    set p to 2
    while (p times p) is less than or equal to limit
        if sieve[p]
            set multiple to p times p
            while multiple is less than or equal to limit
                set sieve[multiple] to false
                set multiple to multiple plus p
        set p to p plus 1

    set result to []
    set n to 2
    while n is less than or equal to limit
        if sieve[n]
            append n to result
        set n to n plus 1
    return result
end

set primes to primes_up_to with 100
print text "Primes up to 100: " plus convert length(primes) to string
```

---

## Business Applications

### Invoice Total Calculation

```nlpl
class LineItem
    public set description to String
    public set quantity to Integer
    public set unit_price to Float

    public function initialize with desc as String and qty as Integer and price as Float
        set this.description to desc
        set this.quantity to qty
        set this.unit_price to price

    public function subtotal returns Float
        return convert this.quantity to float times this.unit_price

function calculate_invoice with items as List and tax_rate as Float returns Float
    set subtotal to 0.0
    for each item in items
        set subtotal to subtotal plus item.subtotal()
    return subtotal times (1.0 plus tax_rate)

set items to [
    create LineItem with "Widget A" and 3 and 25.00,
    create LineItem with "Widget B" and 1 and 150.00,
    create LineItem with "Widget C" and 2 and 45.00
]

set total to calculate_invoice with items and 0.08
print text "Invoice total: $" plus convert total to string
```

### Inventory Report

```nlpl
import io, collections

set inventory to io.parse_json with (io.read_file with "stock.json")

set low_stock    to []
set out_of_stock to []

for each item in inventory
    if item["quantity"] equals 0
        append item to out_of_stock
    else if item["quantity"] is less than 10
        append item to low_stock

print text "Out of stock (" plus convert length(out_of_stock) to string plus "):"
for each item in out_of_stock
    print text "  " plus item["name"]

print text "Low stock (" plus convert length(low_stock) to string plus "):"
for each item in low_stock
    print text "  " plus item["name"] plus " (qty " plus convert item["quantity"] to string plus ")"
```

### Simple REST API Server

```nlpl
import network

set router to network.create_router()

network.route with router and method: "GET" and path: "/health" and handler: function health_handler with req
    return {"status": "ok", "version": "1.0"}
end

network.route with router and method: "GET" and path: "/users/:id" and handler: function get_user with req
    set id to req["params"]["id"]
    # ... look up user ...
    return {"id": id, "name": "Alice"}
end

network.route with router and method: "POST" and path: "/users" and handler: function create_user with req
    set body to req["body"]
    # ... persist user ...
    return {"created": true, "id": "42"}
end

network.serve with router and port: 8080
print text "Server running on port 8080"
```

---

## Graphics and Multimedia

### Generate a Simple Image

```nlpl
import image_utils

set img to image_utils.new_image with width: 256 and height: 256

set y to 0
while y is less than 256
    set x to 0
    while x is less than 256
        image_utils.set_pixel with image: img and x: x and y: y and r: x and g: y and b: 128
        set x to x plus 1
    set y to y plus 1

image_utils.save with image: img and path: "gradient.png"
```

---

## Network Services

### TCP Echo Server

```nlpl
import network

set server to network.create_server with "0.0.0.0" and 9000
print text "Echo server on port 9000"

while true
    set client to network.accept_connection with server
    set data to network.receive with client
    print text "Echo: " plus data
    network.send with client and data
    network.close with client
```
