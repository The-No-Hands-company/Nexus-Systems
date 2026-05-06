# NexusLang Multi-Level Examples

**Demonstrating the Same Programs at All Five Abstraction Levels**

---

## Overview

This document shows how the **same functionality** can be implemented at different abstraction levels in NexusLang. Each level trades control for convenience, but **all compile to the same performant native code**.

---

## Example 1: HTTP Web Server

### Requirements
- Listen on port 8080
- Handle GET requests to `/hello`
- Return "Hello, World!"
- Handle 10,000 concurrent connections

---

### Level 5: Natural Language

**Lines of Code:** ~10 
**Capable of:** Prototyping, scripts, rapid development

```nexuslang
# Create a simple web server
create a web server on port 8080

when someone visits "/hello"
 say hello to them with "Hello, World!"
end

when someone visits the home page
 show them a welcome message
end

start listening for requests
tell me when it's running
```

**Pros:**
- Extremely readable
- No technical knowledge needed
- Perfect for prototypes

**Cons:**
- Limited control
- Ambiguity possible
- Not for performance tuning

---

### Level 4: Goroutines (High-Level)

**Lines of Code:** ~20 
**Capable of:** Production web services, APIs, microservices

```nexuslang
import from stdlib/http

function start_server
 # Create HTTP server
 set server to create http server on port 8080
 
 # Define routes with automatic concurrency
 server.on "GET /hello" do with request
 spawn
 # Each request handled in its own goroutine
 # Automatically scales to 10,000+ concurrent connections
 return new response with 200, "Hello, World!"
 end
 end
 
 server.on "GET /" do with request
 spawn
 set welcome to "Welcome to NexusLang server!"
 return new response with 200, welcome
 end
 end
 
 # Start listening (non-blocking)
 print text "Server running on port 8080"
 server.listen
end

# Run server
start_server
```

**Pros:**
- 10,000+ concurrent connections easily
- Automatic goroutine management
- Clean, readable code
- Production-ready

**Cons:**
- Less control over scheduling
- Some abstraction overhead

---

### Level 3: Application Programming

**Lines of Code:** ~40 
**Capable of:** Desktop apps, custom servers, applications with specific requirements

```nexuslang
import from stdlib/net
import from stdlib/threading

class HttpServer
 property socket as ServerSocket
 property thread_pool as ThreadPool
 
 function initialize with port as Integer
 set socket to create tcp server socket on port
 set thread_pool to create thread pool with 16 threads
 end
 
 function handle_connection with client as ClientSocket
 # Parse HTTP request
 set request_line to client.read_line
 set parts to request_line.split with " "
 set method to parts[0]
 set path to parts[1]
 
 # Route handling
 if path equals "/hello"
 set response to "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!"
 client.write with response
 otherwise if path equals "/"
 set response to "HTTP/1.1 200 OK\r\nContent-Length: 25\r\n\r\nWelcome to NexusLang server!"
 client.write with response
 otherwise
 set response to "HTTP/1.1 404 Not Found\r\n\r\n"
 client.write with response
 end
 
 client.close
 end
 
 function start
 print text "Server running on port 8080"
 
 while true
 set client to socket.accept
 
 # Submit to thread pool
 thread_pool.submit with lambda: handle_connection with client
 end
 end
end

# Create and start server
set server to new HttpServer with 8080
server.start
```

**Pros:**
- Explicit thread control
- Custom error handling
- Fine-tune performance
- Structured code

**Cons:**
- More verbose
- Manual thread pool management
- More complex than Level 4

---

### Level 2: Systems Programming

**Lines of Code:** ~80 
**Capable of:** High-performance servers, embedded systems, performance-critical applications

```nexuslang
# Direct system calls via FFI
extern function socket with domain as Integer, type as Integer, protocol as Integer returns Integer from library "c"
extern function bind with sockfd as Integer, addr as Pointer, addrlen as Integer returns Integer from library "c"
extern function listen with sockfd as Integer, backlog as Integer returns Integer from library "c"
extern function accept with sockfd as Integer, addr as Pointer, addrlen as Pointer returns Integer from library "c"
extern function read with fd as Integer, buf as Pointer, count as Integer returns Integer from library "c"
extern function write with fd as Integer, buf as Pointer, count as Integer returns Integer from library "c"
extern function close with fd as Integer returns Integer from library "c"

extern function pthread_create with thread as Pointer, attr as Pointer, start_routine as FunctionPointer, arg as Pointer returns Integer from library "pthread"
extern function pthread_detach with thread as Integer returns Integer from library "pthread"

# Socket address structure
struct sockaddr_in with packed layout
 sin_family as Word
 sin_port as Word
 sin_addr as Integer
 sin_zero as Array of Byte with 8 elements
end

function handle_client with client_fd_ptr as Pointer returns Pointer
 set client_fd to dereference client_fd_ptr as Integer
 
 # Allocate buffer
 set buffer to allocate with 4096
 
 # Read request
 set bytes_read to call read with client_fd, buffer, 4096
 
 # Parse HTTP method and path (simplified)
 set request as String to buffer as String
 
 # Prepare response
 if request contains "/hello"
 set response to "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!"
 otherwise if request contains "/ "
 set response to "HTTP/1.1 200 OK\r\nContent-Length: 25\r\n\r\nWelcome to NexusLang server!"
 otherwise
 set response to "HTTP/1.1 404 Not Found\r\n\r\n"
 end
 
 # Write response
 call write with client_fd, response as Pointer, response.length
 
 # Cleanup
 call close with client_fd
 free buffer
 free client_fd_ptr
 
 return null
end

function start_server with port as Integer
 # Create socket
 set sockfd to call socket with 2, 1, 0 # AF_INET, SOCK_STREAM
 
 # Setup address
 set addr as sockaddr_in
 set addr.sin_family to 2 # AF_INET
 set addr.sin_port to (port bitwise left shift 8) bitwise or (port bitwise right shift 8) # htons
 set addr.sin_addr to 0 # INADDR_ANY
 
 # Bind
 call bind with sockfd, address of addr as Pointer, sizeof sockaddr_in
 
 # Listen
 call listen with sockfd, 128
 
 print text "Server running on port ", port
 
 # Accept loop
 while true
 set client_fd to call accept with sockfd, null, null
 
 # Allocate client_fd for thread
 set client_fd_ptr to allocate with sizeof Integer
 write client_fd to client_fd_ptr
 
 # Create thread for each connection
 set thread as Integer
 call pthread_create with address of thread, null, address of handle_client, client_fd_ptr
 call pthread_detach with thread
 end
end

# Start server
start_server with 8080
```

**Pros:**
- Maximum performance
- Full control over syscalls
- Minimal overhead
- Portable to embedded systems

**Cons:**
- Very verbose
- Manual memory management
- Error-prone
- Platform-specific

---

### Level 1: Assembly-Level

**Lines of Code:** ~150+ 
**Capable of:** Bare metal programming, OS components, extreme optimization

```nexuslang
# Define syscall numbers
constant SYS_SOCKET to 41
constant SYS_BIND to 49
constant SYS_LISTEN to 50
constant SYS_ACCEPT to 43
constant SYS_READ to 0
constant SYS_WRITE to 1
constant SYS_CLOSE to 3

function syscall_socket returns Integer
 inline assembly
 mov rax, 41 # SYS_SOCKET
 mov rdi, 2 # AF_INET
 mov rsi, 1 # SOCK_STREAM
 mov rdx, 0 # protocol
 syscall
 # Result in RAX
 end
end

function syscall_bind with sockfd as Integer, addr as Pointer, addrlen as Integer returns Integer
 inline assembly
 mov rax, 49 # SYS_BIND
 mov rdi, [sockfd]
 mov rsi, [addr]
 mov rdx, [addrlen]
 syscall
 # Result in RAX
 end
end

function syscall_listen with sockfd as Integer, backlog as Integer returns Integer
 inline assembly
 mov rax, 50 # SYS_LISTEN
 mov rdi, [sockfd]
 mov rsi, [backlog]
 syscall
 end
end

function syscall_accept with sockfd as Integer returns Integer
 inline assembly
 mov rax, 43 # SYS_ACCEPT
 mov rdi, [sockfd]
 xor rsi, rsi # NULL addr
 xor rdx, rdx # NULL addrlen
 syscall
 end
end

function syscall_read with fd as Integer, buf as Pointer, count as Integer returns Integer
 inline assembly
 mov rax, 0 # SYS_READ
 mov rdi, [fd]
 mov rsi, [buf]
 mov rdx, [count]
 syscall
 end
end

function syscall_write with fd as Integer, buf as Pointer, count as Integer returns Integer
 inline assembly
 mov rax, 1 # SYS_WRITE
 mov rdi, [fd]
 mov rsi, [buf]
 mov rdx, [count]
 syscall
 end
end

function handle_client with client_fd as Integer
 # Allocate 4KB stack buffer
 set buffer_size to 4096
 inline assembly
 sub rsp, 4096 # Allocate on stack
 mov rbx, rsp # Buffer pointer in RBX
 end
 
 # Read request
 set bytes_read to syscall_read with client_fd, @rbx, buffer_size
 
 # Simple path check (assembly string comparison)
 # ... (omitted for brevity, would be ~50 lines of assembly)
 
 # Write response
 set response to "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!"
 set response_len to 54
 syscall_write with client_fd, address of response, response_len
 
 # Close
 inline assembly
 mov rax, 3 # SYS_CLOSE
 mov rdi, [client_fd]
 syscall
 
 add rsp, 4096 # Free stack buffer
 end
end

function start_server with port as Integer
 # Create socket
 set sockfd to syscall_socket
 
 # Setup sockaddr_in on stack
 inline assembly
 sub rsp, 16 # sizeof(sockaddr_in)
 mov word ptr [rsp], 2 # AF_INET
 mov ax, [port]
 xchg al, ah # htons
 mov word ptr [rsp+2], ax
 mov dword ptr [rsp+4], 0 # INADDR_ANY
 mov rsi, rsp # addr pointer
 end
 
 # Bind
 syscall_bind with sockfd, @rsi, 16
 
 # Listen
 syscall_listen with sockfd, 128
 
 # Accept loop
 while true
 set client_fd to syscall_accept with sockfd
 
 # For simplicity, handle inline (no threading at this level)
 handle_client with client_fd
 end
end

# Start
start_server with 8080
```

**Pros:**
- Absolute maximum control
- Zero runtime overhead
- Direct syscalls
- Well-suited to system programming

**Cons:**
- Extremely verbose
- Architecture-specific
- Very error-prone
- Not portable

---

## Example 2: Image Processing Pipeline

### Requirements
- Load 1000 images from directory
- Resize each to 800x600
- Apply blur filter
- Save to output directory
- Process in parallel

---

### Level 5: Natural Language

```nexuslang
find all images in "input/" folder
for each image
 resize it to 800 by 600
 apply a blur filter
 save to "output/" folder
end
do this in parallel
tell me when finished
```

---

### Level 4: Goroutines

```nexuslang
import from stdlib/image
import from stdlib/file

function process_images with input_dir as String, output_dir as String
 set images to list_files in input_dir with pattern "*.jpg"
 set completed to create channel of String
 
 # Spawn goroutine for each image
 for each image_path in images
 spawn
 # Load
 set img to load_image from image_path
 
 # Process
 set resized to resize with img, 800, 600
 set blurred to apply_blur with resized, radius: 5
 
 # Save
 set output_path to output_dir plus "/" plus image_path.filename
 save_image with blurred to output_path
 
 # Notify completion
 send image_path to completed
 end
 end
 
 # Wait for all
 for set i to 0 while i is less than images.length
 set done to receive from completed
 print text "Processed: ", done
 end
 
 print text "All images processed!"
end

process_images with "input/", "output/"
```

---

### Level 3: Application Programming

```nexuslang
import from stdlib/image
import from stdlib/threading

class ImageProcessor
 property thread_pool as ThreadPool
 property completed_count as Atomic of Integer
 property total_count as Integer
 
 function initialize with num_threads as Integer
 set thread_pool to create thread pool with num_threads
 set completed_count to new Atomic of Integer with 0
 end
 
 function process_single with input_path as String, output_path as String
 try
 set img to load_image from input_path
 set resized to resize with img, 800, 600
 set blurred to apply_blur with resized, radius: 5
 save_image with blurred to output_path
 
 set count to completed_count.increment
 print text "Processed ", count, " of ", total_count
 catch e as Exception
 print text "Error processing ", input_path, ": ", e.message
 end
 end
 
 function process_directory with input_dir as String, output_dir as String
 set images to list_files in input_dir with pattern "*.jpg"
 set total_count to images.length
 
 for each image_path in images
 set output_path to output_dir plus "/" plus image_path.filename
 thread_pool.submit with lambda: process_single with image_path, output_path
 end
 
 thread_pool.wait_all
 print text "All ", total_count, " images processed!"
 end
end

set processor to new ImageProcessor with 8
processor.process_directory with "input/", "output/"
```

---

### Level 2: Systems Programming

```nexuslang
import from stdlib/image_raw # Low-level image I/O

extern function pthread_create with thread as Pointer, attr as Pointer, routine as FunctionPointer, arg as Pointer returns Integer from library "pthread"
extern function pthread_join with thread as Integer, retval as Pointer returns Integer from library "pthread"

struct ImageTask
 input_path as String
 output_path as String
end

struct WorkerContext
 tasks as Pointer to Array of ImageTask
 task_count as Integer
 current_index as Pointer to Atomic of Integer
end

function worker_thread with ctx_ptr as Pointer returns Pointer
 set ctx to ctx_ptr as Pointer to WorkerContext
 
 while true
 # Atomic fetch-and-add to get next task
 set index to atomic_fetch_add with ctx.current_index, 1
 
 if index is greater than or equal to ctx.task_count
 break
 end
 
 set task to ctx.tasks[index]
 
 # Load image (manual memory management)
 set img_data to load_raw_image with task.input_path
 if img_data equals null
 continue
 end
 
 # Resize (manual buffer allocation)
 set resized_data to allocate with 800 times 600 times 3
 resize_image_raw with img_data, resized_data, 800, 600
 free img_data
 
 # Blur (in-place)
 apply_blur_raw with resized_data, 800, 600, 5
 
 # Save
 save_raw_image with resized_data, task.output_path, 800, 600
 free resized_data
 end
 
 return null
end

function process_images_parallel with input_dir as String, output_dir as String, num_threads as Integer
 # Build task list
 set files to list_directory with input_dir
 set tasks to allocate with sizeof ImageTask times files.length
 
 set task_count to 0
 for each file in files
 if file ends with ".jpg"
 set tasks[task_count].input_path to input_dir plus "/" plus file
 set tasks[task_count].output_path to output_dir plus "/" plus file
 set task_count to task_count plus 1
 end
 end
 
 # Setup worker context
 set ctx as WorkerContext
 set ctx.tasks to tasks
 set ctx.task_count to task_count
 set current_index to allocate with sizeof Integer
 write 0 to current_index
 set ctx.current_index to current_index as Pointer to Atomic of Integer
 
 # Create threads
 set threads to allocate with sizeof Integer times num_threads
 for set i to 0 while i is less than num_threads
 call pthread_create with 
 address of threads[i],
 null,
 address of worker_thread,
 address of ctx
 end
 
 # Join threads
 for set i to 0 while i is less than num_threads
 call pthread_join with threads[i], null
 end
 
 # Cleanup
 free tasks
 free current_index
 free threads
 
 print text "Processed ", task_count, " images"
end

process_images_parallel with "input/", "output/", 8
```

---

## Example 3: Operating System Kernel (Boot Sequence)

This example shows how **Level 1** enables OS development.

### Level 1: Assembly-Level Kernel Boot

```nexuslang
# Bootloader entry point (called by BIOS/UEFI)
function _start
 # Disable interrupts during init
 inline assembly
 cli
 end
 
 # Setup stack
 inline assembly
 mov esp, 0x9F000 # Stack at 640KB
 mov ebp, esp
 end
 
 # Initialize GDT (Global Descriptor Table)
 call setup_gdt
 
 # Initialize IDT (Interrupt Descriptor Table)
 call setup_idt
 
 # Enable paging
 call setup_paging
 
 # Enable interrupts
 inline assembly
 sti
 end
 
 # Jump to kernel main
 call kernel_main
 
 # Halt if kernel returns
 inline assembly
 halt_loop:
 hlt
 jmp halt_loop
 end
end

# Setup Global Descriptor Table
function setup_gdt
 # GDT structure in assembly
 inline assembly
 lgdt [gdt_descriptor]
 
 # Reload segment registers
 mov ax, 0x10 # Data segment selector
 mov ds, ax
 mov es, ax
 mov fs, ax
 mov gs, ax
 mov ss, ax
 
 # Far jump to reload CS
 jmp 0x08:flush_cs
 flush_cs:
 end
end

# Setup Interrupt Descriptor Table
function setup_idt
 # Write interrupt handlers
 set idt_base to 0x0000 as Pointer
 
 # Install handlers (assembly)
 inline assembly
 # Timer interrupt (IRQ 0)
 mov ebx, timer_handler
 mov [idt_base], bx
 mov [idt_base + 6], 0x8E00
 
 # Keyboard interrupt (IRQ 1)
 mov ebx, keyboard_handler
 mov [idt_base + 8], bx
 mov [idt_base + 14], 0x8E00
 end
 
 # Load IDT
 inline assembly
 lidt [idt_descriptor]
 end
end

# Enable paging with identity mapping
function setup_paging
 set page_directory to 0x1000 as Pointer
 
 # Clear page directory
 inline assembly
 mov edi, 0x1000
 mov ecx, 1024
 xor eax, eax
 rep stosd
 end
 
 # Identity map first 4MB
 inline assembly
 mov edi, 0x1000 # Page directory
 mov eax, 0x2003 # Page table at 0x2000, present + writable
 mov [edi], eax
 
 mov edi, 0x2000 # Page table
 mov eax, 0x0003 # Present + writable
 mov ecx, 1024
 fill_page_table:
 mov [edi], eax
 add eax, 0x1000 # Next 4KB page
 add edi, 4
 loop fill_page_table
 
 # Enable paging
 mov eax, 0x1000 # Page directory
 mov cr3, eax
 mov eax, cr0
 or eax, 0x80000000 # Set paging bit
 mov cr0, eax
 end
end

# Timer interrupt handler
function timer_handler
 inline assembly
 pusha # Save all registers
 
 # Acknowledge interrupt
 mov al, 0x20
 out 0x20, al
 
 popa # Restore registers
 iretd # Return from interrupt
 end
end

# Write to VGA text mode
function write_string with text as Pointer, color as Byte
 set vga_buffer to 0xB8000 as Pointer to Word
 set i to 0
 
 while text[i] is not equal to 0
 inline assembly
 mov al, [text + i]
 mov ah, [color]
 mov bx, [i]
 shl bx, 1
 mov [0xB8000 + bx], ax
 end
 set i to i plus 1
 end
end

# Kernel main function
function kernel_main
 # Clear screen
 inline assembly
 mov edi, 0xB8000
 mov ecx, 80 * 25
 mov ax, 0x0F20 # White space
 rep stosw
 end
 
 # Print boot message
 write_string with "NLPL Kernel v0.1" as Pointer, 0x0F
 
 # Initialize memory manager
 call init_memory_manager
 
 # Initialize process scheduler
 call init_scheduler
 
 # Initialize device drivers
 call init_drivers
 
 # Kernel loop
 while true
 inline assembly
 hlt # Halt until interrupt
 end
 end
end
```

**This Level 1 code:**
- Boots on bare metal (no OS)
- Sets up CPU protection features
- Handles interrupts
- Manages memory directly
- **Written in readable English-like syntax!**

**No other language** can do this with readable syntax.

---

## Comparison Summary

| Aspect | L5: Natural | L4: Goroutines | L3: Application | L2: Systems | L1: Assembly |
|--------|-------------|----------------|-----------------|-------------|--------------|
| **Lines of Code** | ~10 | ~20 | ~40 | ~80 | ~150+ |
| **Readability** | | | | | |
| **Control** | | | | | |
| **Performance** | | | | | |
| **Concurrency** | Auto | Excellent | Manual | Manual | None |
| **Memory Management** | Auto | Auto | Manual | Manual | Manual |
| **Error Handling** | Auto | Auto | Manual | Manual | None |
| **Capable of** | Scripts, prototypes | Web apps, services | Desktop apps | High-perf systems | System programming |

**Key Insight:** All levels compile to the same performant code! Choose based on your needs, not performance.

---

## When to Use Each Level

### Use Level 5 When:
- Rapid prototyping
- Teaching beginners
- Scripts and automation
- Non-technical users

### Use Level 4 When:
- Web servers and APIs
- Microservices
- Concurrent I/O
- Production applications

### Use Level 3 When:
- Desktop applications
- Games
- Command-line tools
- Libraries

### Use Level 2 When:
- High-performance code
- Embedded systems
- Device drivers
- System utilities

### Use Level 1 When:
- Operating systems
- Bootloaders
- Bare metal code
- Hardware initialization

---

## Mixing Levels: Real-World Game Engine

```nexuslang
# ============================================
# LEVEL 1: GPU Driver Communication
# ============================================
function write_gpu_command with command as Integer, data as Pointer
 inline assembly
 mov edx, 0xC000 # GPU command port
 mov eax, [command]
 out dx, eax
 
 mov edx, 0xC004 # GPU data port
 mov eax, [data]
 out dx, eax
 end
end

# ============================================
# LEVEL 2: Memory Pool for Fast Allocation
# ============================================
struct MemoryPool
 blocks as Pointer
 free_list as Pointer
 block_size as Integer
end

function create_pool with block_size as Integer returns Pointer to MemoryPool
 set pool to allocate with sizeof MemoryPool
 set pool.block_size to block_size
 # ... setup free list
 return pool
end

# ============================================
# LEVEL 3: Game Object System
# ============================================
class GameObject
 property position as Vector3
 property velocity as Vector3
 property components as List of Component
 
 function update with delta_time as Float
 # Update physics
 set position to position plus (velocity times delta_time)
 
 # Update components
 for each component in components
 component.update with delta_time
 end
 end
end

# ============================================
# LEVEL 4: Asset Loading (Concurrent)
# ============================================
function load_level with level_name as String returns Level
 set textures_channel to create channel of Texture
 set models_channel to create channel of Model
 
 # Load assets concurrently
 spawn
 set textures to load_all_textures for level_name
 send textures to textures_channel
 end
 
 spawn
 set models to load_all_models for level_name
 send models to models_channel
 end
 
 # Wait for both
 set textures to receive from textures_channel
 set models to receive from models_channel
 
 return new Level with textures, models
end

# ============================================
# LEVEL 5: Game Logic Scripting
# ============================================
when player collides with enemy
 reduce player health by 10
 push player back
 play damage sound
end

when player health reaches 0
 show game over screen
 restart level after 3 seconds
end
```

**This is the power of NexusLang:** Choose the right level for each part of your program!

---

**NLPL: One language, infinite possibilities.** 
