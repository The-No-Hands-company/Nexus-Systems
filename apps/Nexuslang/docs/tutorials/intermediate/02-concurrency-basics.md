# Tutorial 7: Concurrency Basics

**Time:** ~60 minutes  
**Prerequisites:** [Errors](../beginner/04-error-handling.md), [Modules](../beginner/05-modules-and-imports.md)

---

## Part 1 — Why Concurrency?

Some tasks — downloading files, querying databases, waiting for user input —
spend most of their time waiting rather than computing.  Without concurrency
your program idles during that wait.  With concurrency it can start other
work during the wait, finishing the overall job faster.

NLPL supports two concurrency models:

| Model | When to use |
|-------|-------------|
| `async / await` | I/O-bound work (network, disk) |
| Threads (`native_thread`) | CPU-bound parallel computation |

---

## Part 2 — async Functions

Mark a function `async` to allow it to `await` other async operations:

```nexuslang
async function fetch_page with url as String returns String
    print text "Fetching " plus url plus "..."
    set response to await http_get with url
    return response
end
```

Call an async function with `await`:

```nexuslang
set html to await fetch_page with "https://example.com"
print text html
```

### Async Error Handling

`await` can raise errors — handle them normally:

```nexuslang
async function safe_fetch with url as String returns String
    try
        return await http_get with url
    catch error with message
        print text "Fetch failed: " plus message
        return ""
end
```

---

## Part 3 — Running Multiple Async Tasks

### Sequential (slower)

```nexuslang
set a to await fetch_page with "https://example.com/page1"
set b to await fetch_page with "https://example.com/page2"
set c to await fetch_page with "https://example.com/page3"
```

Each request waits for the previous one to complete.

### Concurrent (faster)

```nexuslang
set tasks to [
    fetch_page with "https://example.com/page1",
    fetch_page with "https://example.com/page2",
    fetch_page with "https://example.com/page3"
]
set results to await gather with tasks
```

`gather` starts all tasks at once and waits for all of them to finish,
returning a list of results in the same order.

---

## Part 4 — Async in a Loop

```nexuslang
set urls to [
    "https://api.example.com/users/1",
    "https://api.example.com/users/2",
    "https://api.example.com/users/3"
]

async function fetch_all with urls as List of String returns List of String
    set tasks to []
    for each url in urls
        append (http_get with url) to tasks
    return await gather with tasks
end

set responses to await fetch_all with urls
for each r in responses
    print text r
```

---

## Part 5 — Threads for CPU-Bound Work

For parallel number crunching, use native threads:

```nexuslang
import system

function compute_chunk with data as List of Integer returns Integer
    set total to 0
    for each n in data
        set total to total plus (n times n)
    return total
end

set work = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
set half = length(work) divided by 2
set chunk1 = work[0 to half]
set chunk2 = work[half to length(work)]

set t1 to system.spawn_thread with compute_chunk and chunk1
set t2 to system.spawn_thread with compute_chunk and chunk2

set r1 to system.join_thread with t1
set r2 to system.join_thread with t2

print text convert (r1 plus r2) to string
```

---

## Part 6 — Shared State and Synchronisation

When threads share mutable data, use a mutex to prevent races:

```nexuslang
import system

set counter to 0
set lock to system.create_mutex()

function increment_counter
    system.acquire_mutex with lock
    set counter to counter plus 1
    system.release_mutex with lock
end

set threads to []
set i to 0
while i is less than 10
    append (system.spawn_thread with increment_counter) to threads
    set i to i plus 1

for each t in threads
    system.join_thread with t

print text "Counter: " plus convert counter to string    # Counter: 10
```

---

## Part 7 — Event-Driven Programming

Attach handlers to events:

```nexuslang
import system

function on_data with payload as String
    print text "Received: " plus payload
end

function on_error with message as String
    print text "Error: " plus message
end

set bus to system.create_event_bus()
system.on with bus and "data" and on_data
system.on with bus and "error" and on_error

system.emit with bus and "data" and "Hello from the event bus"
```

---

## Summary

| Concept | Syntax / API |
|---------|-------------|
| Async function | `async function f … end` |
| Await single | `set x to await async_call with arg` |
| Await multiple | `set results to await gather with task_list` |
| Spawn thread | `system.spawn_thread with func` |
| Join thread | `system.join_thread with handle` |
| Mutex lock/unlock | `system.acquire_mutex`, `system.release_mutex` |

**Next:** [File I/O and Networking](03-file-io-networking.md)
