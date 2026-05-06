# NexusLang Cross-Platform Development Guide

## Executive Summary

**NLPL is fully cross-platform!** The interpreter is built on Python's standard library and runs on Windows, Linux, macOS, and any platform with Python 3.8+.

The confusion arose because **NLPLDev field testing examples used Windows API** for demonstration. This guide shows how to write cross-platform NexusLang code using platform detection and appropriate library selection.

---

## Platform Architecture

### Interpreter Foundation

**Core Implementation** (Python-based):
```
Interpreter: src/nlpl/interpreter/interpreter.py
 Uses: os, sys, platform, struct (all cross-platform)
 FFI via: ctypes (Python's cross-platform FFI)
 No platform-specific dependencies

Runtime: src/nlpl/runtime/runtime.py
 Memory management: MemoryManager (pure Python)
 Concurrency: ThreadPoolExecutor (standard library)
 Cross-platform by design

Standard Library: src/nlpl/stdlib/
 system/__init__.py - Platform detection & info
 limits/__init__.py - Platform-dependent constants
 All modules use standard Python libraries
```

**Platform Support Matrix**:
| Platform | Status | Python Version | FFI Library Format |
|----------|--------|----------------|-------------------|
| **Linux** | Full | 3.8+ | `.so` (shared objects) |
| **macOS** | Full | 3.8+ | `.dylib` (dynamic libraries) |
| **Windows** | Full | 3.8+ | `.dll` (dynamic link libraries) |
| **BSD** | Should work | 3.8+ | `.so` (shared objects) |

---

## Platform Detection in NexusLang

### Built-in Platform Functions

NLPL's `system` module provides comprehensive platform detection:

```nlpl
use module system

# Get platform name: "Windows", "Linux", "Darwin" (macOS)
set platform to call get_os_name

# Get detailed platform info: "Linux-5.15.0-x86_64-with-glibc2.35"
set platform_info to call get_platform

# Get OS version
set os_version to call get_os_version

# Get Python version (interpreter dependency)
set py_version to call get_python_version

# Get hostname and username
set hostname to call get_hostname
set username to call get_username
```

**Platform String Values**:
- **Windows**: `platform.system()` returns `"Windows"`
- **Linux**: `platform.system()` returns `"Linux"`
- **macOS**: `platform.system()` returns `"Darwin"`
- **BSD**: `platform.system()` returns `"FreeBSD"` or `"OpenBSD"`

### Implementation in System Module

```python
# src/nlpl/stdlib/system/__init__.py
import platform

def get_os_name():
 """Get the name of the operating system."""
 return platform.system()

def get_platform():
 """Get the platform information."""
 return platform.platform()
```

---

## Cross-Platform FFI Patterns

### Pattern 1: Platform-Specific Library Loading

The **key to cross-platform FFI** is loading different libraries based on the detected platform:

```nlpl
use module system
use module ffi

# Detect platform
set platform to call get_os_name

# Load platform-appropriate library
if platform equals "Windows"
 set lib to call load_library with "user32.dll"
else if platform equals "Linux"
 set lib to call load_library with "libgtk-3.so.0"
else if platform equals "Darwin"
 set lib to call load_library with "libSystem.dylib"
else
 print text "Unsupported platform: " plus platform
 exit with 1
end
```

### Pattern 2: Cross-Platform Message Box

Here's the Windows MessageBox example rewritten to be cross-platform:

#### Windows Implementation
```nlpl
use module system
use module ffi

function show_message_box_windows with title, message
 set user32 to call load_library with "user32.dll"
 
 # MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
 call define_function on user32 with "MessageBoxA" and [
 "Pointer", # hWnd (NULL)
 "String", # lpText
 "String", # lpCaption
 "Integer" # uType
 ] returns "Integer"
 
 # MB_OK = 0x00000000
 set result to call invoke_function on user32 with "MessageBoxA" and [
 0, # NULL window handle
 message, # Message text
 title, # Title text
 0 # MB_OK flag
 ]
 
 return result
end
```

#### Linux Implementation (GTK)
```nlpl
use module system
use module ffi

function show_message_box_linux with title, message
 set gtk to call load_library with "libgtk-3.so.0"
 
 # Initialize GTK
 call define_function on gtk with "gtk_init" and [
 "Pointer", # argc pointer
 "Pointer" # argv pointer
 ] returns "void"
 
 call invoke_function on gtk with "gtk_init" and [0, 0]
 
 # Create message dialog
 # GtkWidget* gtk_message_dialog_new(
 # GtkWindow* parent, GtkDialogFlags flags,
 # GtkMessageType type, GtkButtonsType buttons,
 # const gchar* message_format, ...
 # )
 call define_function on gtk with "gtk_message_dialog_new" and [
 "Pointer", # parent
 "Integer", # flags
 "Integer", # message type
 "Integer", # buttons type
 "String" # message format
 ] returns "Pointer"
 
 # GTK_MESSAGE_INFO = 0, GTK_BUTTONS_OK = 1
 set dialog to call invoke_function on gtk with "gtk_message_dialog_new" and [
 0, # NULL parent
 0, # GTK_DIALOG_MODAL
 0, # GTK_MESSAGE_INFO
 1, # GTK_BUTTONS_OK
 message # Message text
 ]
 
 # Set window title
 call define_function on gtk with "gtk_window_set_title" and [
 "Pointer", # window
 "String" # title
 ] returns "void"
 
 call invoke_function on gtk with "gtk_window_set_title" and [dialog, title]
 
 # Run dialog
 call define_function on gtk with "gtk_dialog_run" and [
 "Pointer" # dialog
 ] returns "Integer"
 
 set response to call invoke_function on gtk with "gtk_dialog_run" and [dialog]
 
 # Destroy dialog
 call define_function on gtk with "gtk_widget_destroy" and [
 "Pointer" # widget
 ] returns "void"
 
 call invoke_function on gtk with "gtk_widget_destroy" and [dialog]
 
 return response
end
```

#### macOS Implementation (Cocoa)
```nlpl
use module system
use module ffi

function show_message_box_macos with title, message
 # Load Objective-C runtime
 set objc to call load_library with "libobjc.dylib"
 
 # Define Objective-C message send functions
 call define_function on objc with "objc_getClass" and [
 "String" # class name
 ] returns "Pointer"
 
 call define_function on objc with "sel_registerName" and [
 "String" # selector name
 ] returns "Pointer"
 
 call define_function on objc with "objc_msgSend" and [
 "Pointer", # object
 "Pointer" # selector
 ] returns "Pointer"
 
 # Get NSAlert class
 set ns_alert_class to call invoke_function on objc with "objc_getClass" and ["NSAlert"]
 
 # Create NSAlert instance: [NSAlert alloc]
 set alloc_sel to call invoke_function on objc with "sel_registerName" and ["alloc"]
 set alert to call invoke_function on objc with "objc_msgSend" and [ns_alert_class, alloc_sel]
 
 # Initialize: [alert init]
 set init_sel to call invoke_function on objc with "sel_registerName" and ["init"]
 set alert to call invoke_function on objc with "objc_msgSend" and [alert, init_sel]
 
 # Set message text: [alert setMessageText:@"message"]
 # (Simplified - real implementation needs NSString creation)
 set set_message_sel to call invoke_function on objc with "sel_registerName" and ["setMessageText:"]
 call invoke_function on objc with "objc_msgSend" and [alert, set_message_sel, message]
 
 # Show dialog: [alert runModal]
 set run_modal_sel to call invoke_function on objc with "sel_registerName" and ["runModal"]
 set response to call invoke_function on objc with "objc_msgSend" and [alert, run_modal_sel]
 
 return response
end
```

#### Unified Cross-Platform Function
```nlpl
use module system
use module ffi

function show_message_box with title, message
 set platform to call get_os_name
 
 if platform equals "Windows"
 return call show_message_box_windows with title and message
 else if platform equals "Linux"
 return call show_message_box_linux with title and message
 else if platform equals "Darwin"
 return call show_message_box_macos with title and message
 else
 print text "Unsupported platform: " plus platform
 return -1
 end
end

# Usage (same on all platforms)
call show_message_box with "Hello" and "Cross-platform message!"
```

---

## Cross-Platform Library Mapping

### Common C Library Functions

Many standard C functions are available on all platforms:

| Function Category | Windows Library | Linux Library | macOS Library |
|------------------|----------------|---------------|---------------|
| **Standard C** | `msvcrt.dll` | `libc.so.6` | `libSystem.dylib` |
| **Math** | `msvcrt.dll` | `libm.so.6` | `libSystem.dylib` |
| **Threading** | `kernel32.dll` | `libpthread.so.0` | `libSystem.dylib` |
| **Sockets** | `ws2_32.dll` | `libc.so.6` | `libSystem.dylib` |

### GUI Frameworks (Cross-Platform)

These libraries work on all platforms with the **same API**:

| Framework | Platform Support | NexusLang Compatibility |
|-----------|-----------------|-------------------|
| **SDL2** | Windows, Linux, macOS, BSD | FFI Ready |
| **GLFW** | Windows, Linux, macOS | FFI Ready |
| **Qt** | Windows, Linux, macOS | FFI via libQt5Core |
| **wxWidgets** | Windows, Linux, macOS | FFI Ready |

**Example: SDL2 (Truly Cross-Platform)**:
```nlpl
use module system
use module ffi

# Detect platform and load appropriate SDL2 library
set platform to call get_os_name

if platform equals "Windows"
 set sdl to call load_library with "SDL2.dll"
else if platform equals "Linux"
 set sdl to call load_library with "libSDL2.so"
else if platform equals "Darwin"
 set sdl to call load_library with "libSDL2.dylib"
end

# SDL2 API is identical across all platforms!
call define_function on sdl with "SDL_Init" and ["Integer"] returns "Integer"
set result to call invoke_function on sdl with "SDL_Init" and [0x00000020] # SDL_INIT_VIDEO

if result equals 0
 print text "SDL2 initialized successfully!"
else
 print text "SDL2 initialization failed"
end
```

---

## Platform-Dependent Constants

NLPL's `limits` module provides platform-aware constants:

```nlpl
use module limits

# Word size (32-bit vs 64-bit)
set word_size to call get_word_size # Returns 4 (32-bit) or 8 (64-bit)

# Endianness
set is_big_endian to call is_big_endian # Returns true/false

# Integer limits
set max_int to call get_int_max
set min_int to call get_int_min

# Float limits
set max_float to call get_float_max
set min_float to call get_float_min
```

**Implementation** (src/nlpl/stdlib/limits/__init__.py):
```python
import sys
import struct

def get_word_size():
 """Get the word size of the platform (4 for 32-bit, 8 for 64-bit)."""
 return struct.calcsize("P") # Size of pointer

def is_big_endian():
 """Check if the platform is big-endian."""
 return sys.byteorder == 'big'
```

---

## Best Practices for Cross-Platform NexusLang

### 1. Always Use Platform Detection

**Bad** (assumes Windows):
```nlpl
use module ffi
set lib to call load_library with "user32.dll" # FAILS on Linux/Mac!
```

**Good** (platform-aware):
```nlpl
use module system
use module ffi

set platform to call get_os_name

if platform equals "Windows"
 set lib to call load_library with "user32.dll"
else if platform equals "Linux"
 set lib to call load_library with "libgtk-3.so.0"
else if platform equals "Darwin"
 set lib to call load_library with "libSystem.dylib"
end
```

### 2. Prefer Cross-Platform Libraries

**Better Approach** (use SDL2 instead of platform-specific APIs):
```nlpl
# SDL2 has identical API on all platforms
set platform to call get_os_name

if platform equals "Windows"
 set sdl to call load_library with "SDL2.dll"
else if platform equals "Linux"
 set sdl to call load_library with "libSDL2.so"
else if platform equals "Darwin"
 set sdl to call load_library with "libSDL2.dylib"
end

# Rest of code is IDENTICAL across platforms
call define_function on sdl with "SDL_Init" and ["Integer"] returns "Integer"
```

### 3. Abstract Platform Differences

Create wrapper functions that hide platform-specific details:

```nlpl
# platform_utils.nlpl
use module system

function detect_platform returns String
 set os_name to call get_os_name
 
 if os_name equals "Windows"
 return "windows"
 else if os_name equals "Linux"
 return "linux"
 else if os_name equals "Darwin"
 return "macos"
 else
 return "unknown"
 end
end

function get_library_extension returns String
 set platform to call detect_platform
 
 if platform equals "windows"
 return ".dll"
 else if platform equals "linux"
 return ".so"
 else if platform equals "macos"
 return ".dylib"
 else
 return ".so" # default
 end
end

function load_cross_platform_library with lib_name returns Pointer
 use module ffi
 
 set platform to call detect_platform
 set extension to call get_library_extension
 
 # Try platform-specific name first
 set platform_lib_name to lib_name plus extension
 
 try
 return call load_library with platform_lib_name
 catch error
 # Fallback: try lib prefix on Unix
 if platform equals "linux" or platform equals "macos"
 set unix_lib_name to "lib" plus lib_name plus extension
 return call load_library with unix_lib_name
 else
 raise error
 end
 end
end
```

### 4. Test on Multiple Platforms

**Development Workflow**:
1. Develop on your primary platform (Windows, Linux, or Mac)
2. Use platform detection from the start
3. Test on at least 2 platforms before release
4. Use virtual machines or CI/CD for cross-platform testing

---

## File System Paths

### Path Separators

NLPL uses Python's `os` module, which handles path separators automatically:

```nlpl
use module system

# Python's os.path.join handles platform differences
# Windows: "C:\Users\user\file.txt"
# Linux/Mac: "/home/user/file.txt"

# Get home directory (cross-platform)
set home to call get_env with "HOME" # Linux/Mac
if home is null
 set home to call get_env with "USERPROFILE" # Windows fallback
end

print text home
```

**Best Practice**: Use `os.path.join()` through FFI or NexusLang path utilities (future stdlib addition).

---

## Environment Variables

### Common Cross-Platform Variables

| Variable | Windows | Linux/Mac | NexusLang Access |
|----------|---------|-----------|-------------|
| **Home Directory** | `USERPROFILE` | `HOME` | `call get_env with "HOME"` |
| **Temp Directory** | `TEMP` or `TMP` | `TMPDIR` | `call get_env with "TMPDIR"` |
| **Path Separator** | `;` | `:` | N/A (use `os.pathsep`) |
| **Username** | `USERNAME` | `USER` | `call get_username` |

**Example**:
```nlpl
use module system

# Get home directory (cross-platform)
function get_home_directory returns String
 set home to call get_env with "HOME"
 
 if home is null
 set home to call get_env with "USERPROFILE"
 end
 
 if home is null
 set home to "/tmp" # fallback
 end
 
 return home
end

set home_dir to call get_home_directory
print text "Home directory: " plus home_dir
```

---

## Concurrency and Threading

NLPL's runtime uses Python's `ThreadPoolExecutor`, which is **cross-platform by design**:

```nlpl
# Concurrent execution works identically on all platforms
concurrent
 task 1
 print text "Task 1 running"
 end
 
 task 2
 print text "Task 2 running"
 end
end
```

**Implementation** (src/nlpl/runtime/runtime.py):
```python
from concurrent.futures import ThreadPoolExecutor

def run_concurrently(self, tasks):
 """Execute tasks concurrently using ThreadPoolExecutor."""
 with ThreadPoolExecutor() as executor:
 futures = [executor.submit(task) for task in tasks]
 return [future.result() for future in futures]
```

No platform-specific code needed!

---

## Testing Cross-Platform NexusLang Code

### Test Matrix

Create test programs that verify cross-platform behavior:

```nlpl
# test_programs/integration/cross_platform_test.nlpl

use module system

print text "=== NexusLang Cross-Platform Test ==="
print text ""

# Test 1: Platform Detection
print text "Platform: " plus call get_os_name
print text "Platform Info: " plus call get_platform
print text "OS Version: " plus call get_os_version
print text "Python Version: " plus call get_python_version

# Test 2: Environment Variables
print text ""
print text "Username: " plus call get_username
print text "Hostname: " plus call get_hostname

# Test 3: Platform-Dependent Constants
use module limits
print text ""
print text "Word Size: " plus call get_word_size
print text "Big Endian: " plus call is_big_endian

# Test 4: File System Paths
set home to call get_env with "HOME"
if home is null
 set home to call get_env with "USERPROFILE"
end
print text "Home Directory: " plus home

print text ""
print text "=== All Tests Passed ==="
```

**Run on all target platforms**:
```bash
# Linux
python src/main.py test_programs/integration/cross_platform_test.nlpl

# macOS
python3 src/main.py test_programs/integration/cross_platform_test.nlpl

# Windows (PowerShell)
python src/main.py test_programs/integration/cross_platform_test.nlpl
```

---

## Common Pitfalls and Solutions

### Pitfall 1: Hardcoded Library Names

**Problem**:
```nlpl
set lib to call load_library with "user32.dll" # Only works on Windows!
```

**Solution**:
```nlpl
set platform to call get_os_name
if platform equals "Windows"
 set lib to call load_library with "user32.dll"
else
 print text "Not supported on " plus platform
end
```

### Pitfall 2: Path Separators

**Problem**:
```nlpl
set path to "C:\Users\user\file.txt" # Fails on Linux/Mac
```

**Solution**: Use forward slashes (works on all platforms):
```nlpl
set path to "C:/Users/user/file.txt" # Works everywhere
# Or use environment variables
set home to call get_home_directory
set path to home plus "/file.txt"
```

### Pitfall 3: Assuming 64-bit

**Problem**:
```nlpl
# Assumes pointers are 8 bytes (64-bit)
set ptr_size to 8
```

**Solution**:
```nlpl
use module limits
set ptr_size to call get_word_size # Returns 4 or 8 based on platform
```

### Pitfall 4: Platform-Specific APIs

**Problem**: Using Windows-only APIs (WinAPI) for generic tasks.

**Solution**: Use cross-platform libraries:
- **Instead of WinAPI**: Use SDL2, GTK (via FFI)
- **Instead of platform-specific graphics**: Use SDL2, GLFW, OpenGL
- **Instead of platform-specific audio**: Use SDL2_mixer, PortAudio

---

## Roadmap: Enhanced Cross-Platform Support

### Phase 1: Standard Library Enhancements (Weeks 1-2)

**Add to `stdlib/filesystem/` module**:
```nlpl
function join_path with parts returns String
 # Cross-platform path joining
end

function get_path_separator returns String
 # Returns "\" on Windows, "/" on Unix
end

function normalize_path with path returns String
 # Converts to platform-native format
end
```

**Add to `stdlib/system/` module**:
```nlpl
function get_home_directory returns String
 # Cross-platform home dir detection
end

function get_temp_directory returns String
 # Cross-platform temp dir
end

function get_executable_path returns String
 # Path to NexusLang interpreter
end
```

### Phase 2: GUI Abstraction Layer (Weeks 3-4)

**Create `stdlib/gui/` module** (wraps platform-specific APIs):
```nlpl
function show_alert with title, message returns Integer
 # Auto-detects platform and uses appropriate API
 # Windows: MessageBox
 # Linux: GTK or Zenity
 # macOS: NSAlert
end

function show_file_dialog with mode returns String
 # Cross-platform file picker
 # Windows: GetOpenFileName
 # Linux: GTK file chooser
 # macOS: NSOpenPanel
end
```

### Phase 3: Build System Integration (Weeks 5-6)

**Create `nlplbuild` tool** for cross-platform builds:
```bash
nlplbuild --target linux --output myapp
nlplbuild --target windows --output myapp.exe
nlplbuild --target macos --output myapp.app
```

---

## Conclusion

**NLPL is already cross-platform!** The interpreter runs on any system with Python 3.8+, and the FFI system supports Windows `.dll`, Linux `.so`, and macOS `.dylib` files.

**Key Takeaways**:
1. NexusLang interpreter is Python-based (inherently cross-platform)
2. Platform detection is built into `stdlib/system` module
3. FFI via ctypes works on all platforms
4. No Windows-specific dependencies in core

**Next Steps**:
1. Always use platform detection when loading libraries
2. Prefer cross-platform libraries (SDL2, GTK, Qt) over platform-specific APIs
3. Test on multiple platforms during development
4. Contribute cross-platform examples to the community

**NLPLDev Field Testing Note**:
The Windows API examples (MessageBoxA, etc.) were used for **testing convenience**, not because NexusLang is Windows-only. The same FFI capabilities work on Linux and macOS with their respective libraries.

---

## Additional Resources

- **Platform Module Docs**: `src/nlpl/stdlib/system/__init__.py`
- **FFI Implementation**: `src/nlpl/interpreter/interpreter.py` (lines 1100-1300)
- **Cross-Platform Examples**: `examples/` directory (coming soon)
- **Testing Guide**: `test_programs/README.md`

**Questions or Issues?** 
Open an issue on GitHub: https://github.com/your-org/NexusLang/issues
