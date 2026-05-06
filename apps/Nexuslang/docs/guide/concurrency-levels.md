# NexusLang Concurrency: Syntax Design for All Levels

**Date:** January 2, 2026 
**Status:** Design Specification

---

## Overview

NLPL provides **five levels of concurrency abstractions**, allowing developers to choose the right model for their needs - from manual thread management to natural language parallelism.

---

## Level 1: Manual Threading (Assembly-Level)

**Philosophy:** Maximum control, zero overhead, explicit everything

### Syntax

#### Thread Creation via FFI
```nexuslang
# Direct system calls
extern function pthread_create with 
 thread as Pointer to Integer,
 attr as Pointer,
 start_routine as FunctionPointer,
 arg as Pointer
returns Integer from library "pthread"

extern function pthread_join with
 thread as Integer,
 retval as Pointer
returns Integer from library "pthread"

function worker_thread with arg as Pointer returns Pointer
 # Thread work here
 return null
end

# Create thread manually
set thread_id as Integer
set result to call pthread_create with
 address of thread_id,
 null,
 address of worker_thread,
 null

# Wait for completion
call pthread_join with thread_id, null
```

#### Atomic Operations
```nexuslang
# Direct atomic instructions via inline assembly
function atomic_increment with ptr as Pointer to Integer
 inline assembly
 lock inc dword ptr [ptr]
 end
end

function compare_and_swap with ptr as Pointer to Integer, old_val as Integer, new_val as Integer returns Boolean
 inline assembly
 mov rax, [old_val]
 mov rbx, [new_val]
 lock cmpxchg [ptr], rbx
 setz al
 movzx rax, al
 end
end
```

#### Spin Locks
```nexuslang
# Manual spinlock implementation
struct SpinLock
 locked as Integer # 0 = unlocked, 1 = locked
end

function acquire with lock as Pointer to SpinLock
 inline assembly
 spin_wait:
 lock bts dword ptr [lock], 0
 jc spin_wait
 end
end

function release with lock as Pointer to SpinLock
 set lock.locked to 0
end
```

**Use Cases:**
- OS kernel synchronization
- Real-time systems
- Device drivers
- Lock-free data structures

---

## Level 2: Explicit Threading (Systems Programming)

**Philosophy:** RAII-style thread management, manual but safe

### Syntax

#### Thread Objects
```nexuslang
# Import threading module
import from stdlib/threading

# Create thread with RAII cleanup
function process_data_threaded with data as List of Integer
 set thread1 to create thread with lambda: process_chunk with data, 0, 1000
 set thread2 to create thread with lambda: process_chunk with data, 1000, 2000
 
 # Threads automatically joined when going out of scope
 # Or explicitly:
 thread1.join
 thread2.join
end
```

#### Mutexes and Condition Variables
```nexuslang
import from stdlib/sync

# Mutex-protected data
struct SharedCounter
 property value as Integer
 property mutex as Mutex
end

function increment_counter with counter as Pointer to SharedCounter
 # Lock scope guard
 using lock as counter.mutex.lock
 set counter.value to counter.value plus 1
 end # Automatically unlocks
end

# Condition variables
struct WorkQueue
 property items as List of WorkItem
 property mutex as Mutex
 property cond_var as ConditionVariable
end

function producer with queue as Pointer to WorkQueue
 while true
 set item to create_work_item
 
 using lock as queue.mutex.lock
 queue.items.add with item
 queue.cond_var.notify_one
 end
 end
end

function consumer with queue as Pointer to WorkQueue
 while true
 using lock as queue.mutex.lock
 while queue.items.is_empty
 queue.cond_var.wait with lock
 end
 
 set item to queue.items.remove_first
 end
 
 call process with item
 end
end
```

#### Thread Pools
```nexuslang
import from stdlib/threading

# Create fixed-size thread pool
set pool to create thread pool with 8 threads

# Submit work
for each item in work_items
 pool.submit with lambda: process with item
end

# Wait for all work to complete
pool.wait_all

# Cleanup
pool.shutdown
```

**Use Cases:**
- High-performance servers
- Parallel algorithms
- Embedded systems with multiple cores
- Real-time processing

---

## Level 3: Structured Concurrency (Application Programming)

**Philosophy:** Explicit concurrency boundaries, automatic cleanup

### Syntax

#### Concurrent Blocks
```nexuslang
# Explicit parallel execution
concurrent
 set result1 to fetch_data with "url1"
 set result2 to fetch_data with "url2"
 set result3 to fetch_data with "url3"
end
# All three complete before continuing

# Use results
print text result1, result2, result3
```

#### Async Functions (Optional)
```nexuslang
# For developers who prefer async/await style
async function fetch_user_data with user_id as Integer returns eventually UserData
 set user to await fetch_user with user_id
 set posts to await fetch_posts with user_id
 set comments to await fetch_comments with user_id
 
 return UserData with user, posts, comments
end

# Call async function
set future to fetch_user_data with 42
# Do other work...
set data to await future
```

#### Parallel Loops
```nexuslang
# Process items in parallel automatically
parallel for each image in images
 call resize with image, 800, 600
 call apply_filter with image
 call save with image
end

# Chunk-size clauses for parallel loops are not parser-supported yet.
# Use library helpers when explicit partitioning is required.
parallel_for_each with process_batch and batches
```

#### Futures and Promises
```nexuslang
import from stdlib/async

# Create promise
set promise to create promise of Integer

# Pass to another thread
spawn
 # Do work
 set result to expensive_calculation
 promise.set_value with result
end

# Get future and wait for result
set future to promise.get_future
set value to future.get # Blocks until ready
```

**Use Cases:**
- Desktop applications
- CLI tools with parallel processing
- Game engines
- Data processing pipelines

---

## Level 4: Goroutines (High-Level Abstractions)

**Philosophy:** Lightweight concurrency, implicit scheduling, CSP model

### Syntax

#### Spawning Goroutines
```nexuslang
# Spawn lightweight concurrent task
spawn
 set data to fetch_from_network with url
 call process with data
 call save_results
end

# Spawn with named function
spawn call handle_client with connection

# Spawn in loop (handles 100,000+ concurrent connections)
for each connection in server.connections
 spawn
 set request to receive from connection
 set response to handle_request with request
 send response to connection
 end
end
```

#### Channels
```nexuslang
# Create typed channels
set jobs to create channel of WorkItem
set results to create channel of Result

# Buffered channel
set messages to create channel of String with capacity 100

# Send and receive
send work_item to jobs # Blocks if full
set item to receive from jobs # Blocks if empty

# Non-blocking operations
if try send item to jobs
 print text "Sent successfully"
otherwise
 print text "Channel full"
end

set value to try receive from results or default_value

# Close channel
close jobs

# Range over channel (receives until closed)
for each item from jobs
 call process with item
end
```

#### Select Statement
```nexuslang
# Wait on multiple channels
select
 case message from channel1
 print text "From channel1: ", message
 
 case result from channel2
 print text "From channel2: ", result
 
 case timeout after 5 seconds
 print text "Timed out"
 
 case default
 print text "Nothing ready"
end
```

#### Worker Pools with Channels
```nexuslang
# Create worker pool pattern
set jobs to create channel of Job with capacity 100
set results to create channel of Result

# Start workers
for set i to 0 while i is less than num_workers
 spawn
 for each job from jobs
 set result to process with job
 send result to results
 end
 end
end

# Send work
for each job in all_jobs
 send job to jobs
end
close jobs

# Collect results
for set i to 0 while i is less than all_jobs.length
 set result to receive from results
 print text "Got result: ", result
end
```

#### Context and Cancellation
```nexuslang
# Create cancellable context
set ctx to create context with timeout 30 seconds

# Spawn with context
spawn with ctx
 while not ctx.is_cancelled
 set data to fetch_data
 call process with data
 
 # Check cancellation
 if ctx.is_cancelled
 break
 end
 end
end

# Cancel all spawned tasks
ctx.cancel
```

**Use Cases:**
- Web servers
- Microservices
- Real-time systems
- Network applications
- APIs and services

---

## Level 5: Natural Language Parallelism

**Philosophy:** Automatic parallelization, no explicit concurrency

### Syntax

#### Implicit Parallel Processing
```nexuslang
# Compiler automatically parallelizes
process these files in parallel
 for each file in directory
 read the file
 transform the data
 save the results
 end
end

# Natural language barriers
wait for all processing to complete
show "All done!"
```

#### Natural Concurrent Operations
```nexuslang
# Multiple operations happen concurrently automatically
fetch user data from database
fetch posts from database 
fetch comments from database

# Compiler detects these can run in parallel

when all data is ready
 combine everything
 show the results
end
```

#### Natural Async I/O
```nexuslang
# All I/O is automatically async
ask the API for user information
ask the database for recent orders
ask the cache for preferences

# These all happen concurrently
# Execution continues automatically

when everything is available
 create the response
 send to client
end
```

**Use Cases:**
- Scripts
- Prototypes
- Teaching/learning
- Non-programmer users
- Rapid development

---

## Feature Comparison Matrix

| Feature | L1: Manual | L2: Explicit | L3: Structured | L4: Goroutines | L5: Natural |
|---------|-----------|--------------|----------------|----------------|-------------|
| **Control** | Maximum | High | Medium | Low | Minimum |
| **Safety** | Manual | RAII | Automatic | Automatic | Automatic |
| **Overhead** | None | Minimal | Low | Low | Low |
| **Scalability** | Limited | Medium | Good | Excellent | Excellent |
| **Complexity** | Highest | High | Medium | Low | Lowest |
| **Learning Curve** | Expert | Advanced | Intermediate | Beginner | Novice |

---

## Choosing the Right Level

### Decision Tree

```
Need hardware-level control? (kernel, drivers)
 YES Level 1: Manual Threading
 NO
 
Need deterministic behavior? (real-time, embedded)
 YES Level 2: Explicit Threading
 NO
 
Need structured parallelism? (desktop apps, tools)
 YES Level 3: Structured Concurrency
 NO
 
Building server/network app? (web, API, services)
 YES Level 4: Goroutines
 NO
 
Scripting or prototyping?
 YES Level 5: Natural Language
```

---

## Implementation Roadmap

### Phase 1: Foundations (Q1 2026)
- Level 2: Basic threading via FFI
- Mutex and lock primitives
- Thread pool implementation

### Phase 2: Goroutines (Q2 2026)
- M:N scheduler implementation
- Channel types and operations
- Spawn keyword and syntax
- Select statement

### Phase 3: Advanced Features (Q3 2026)
- Level 3: Structured concurrency
- Async/await (optional)
- Context and cancellation
- Work-stealing scheduler

### Phase 4: Optimization (Q4 2026)
- Level 1: Inline assembly for atomics
- Zero-copy channel operations
- Compiler-based parallelization
- Level 5: Natural language detection

---

## Examples

### Web Server (All Levels)

#### Level 1: Manual
```nexuslang
extern function socket from library "c"
extern function bind from library "c"
extern function listen from library "c"
extern function accept from library "c"
extern function pthread_create from library "pthread"

function handle_connection with fd as Integer returns Pointer
 # Manual request handling
 return null
end

function start_server
 set sock to call socket with 2, 1, 0
 # Manual bind, listen, accept loop
 while true
 set client_fd to call accept with sock, null, null
 set thread as Integer
 call pthread_create with address of thread, null, address of handle_connection, client_fd
 end
end
```

#### Level 2: Explicit
```nexuslang
import from stdlib/net
import from stdlib/threading

function start_server
 set server to create tcp server on port 8080
 set pool to create thread pool with 16 threads
 
 while true
 set connection to server.accept
 pool.submit with lambda: handle_connection with connection
 end
end
```

#### Level 3: Structured
```nexuslang
import from stdlib/http

function start_server
 set server to create http server on port 8080
 
 concurrent
 server.on "GET /" do with req
 return "Hello World"
 end
 
 server.listen
 end
end
```

#### Level 4: Goroutines
```nexuslang
import from stdlib/http

function start_server
 set server to create http server on port 8080
 
 server.on "GET /" do with req
 spawn
 set data to fetch_data with req
 return json with data
 end
 end
 
 server.listen
end
```

#### Level 5: Natural
```nexuslang
create a web server on port 8080

when someone visits the homepage
 fetch their data
 show them a welcome page
end

when someone requests user data
 get it from the database
 send it as JSON
end

start listening for requests
```

---

## Migration Between Levels

Developers can **start at any level** and **move between levels** as needed:

```nexuslang
# Start with natural language (Level 5)
process all files in parallel

# Optimize critical section with goroutines (Level 4)
for each file in files
 spawn
 call process_file with file
 end
end

# Further optimize with explicit threading (Level 2)
set pool to create thread pool with 8 threads
for each file in files
 pool.submit with lambda: process_file with file
end
```

---

## Future: AI-Assisted Level Selection

Future NexusLang IDE will **automatically suggest** the right level:

```nexuslang
# You write:
process files in parallel

# IDE suggests:
" This could be optimized with Level 4 goroutines for better performance"

# Or:
" This critical section should use Level 2 explicit locks for safety"
```

---

**NLPL Concurrency: The right abstraction for every task.** 
