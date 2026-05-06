# Phase 3: Core Language Features Development Roadmap

**Current Status**: Week 2 Day 2 Complete ✅  
**Performance**: 1.80-2.52x slower than C (exceeds 3x target) ✅  
**Pivot Decision**: Shift from optimization to core language features  
**Timeline**: Weeks 3-8 (6 weeks of focused core development)

---

## Strategic Rationale

### Why Pivot Now?

**Optimization Status:**
- ✅ Performance target exceeded (1.80x vs 3x goal)
- ✅ LLVM pipeline working well (5 optimization levels)
- ✅ Major optimizations complete (coroutine elimination)
- ✅ Diminishing returns on further optimization

**Language Capability Gaps** (Blockers for Real Applications):
- ❌ No reference counting or smart pointers (memory management)
- ❌ No Result/Option types (error handling primitive)
- ❌ No threading support (can't build servers, browsers)
- ❌ Limited stdlib (missing regex, JSON, dates, crypto, etc.)
- ❌ No C++ FFI (can only call C libraries)
- ❌ Limited generic types (no bounded parameters, no traits)

**PronBrow Browser Feasibility Analysis** revealed:
- Need 15-21 months of core development before real applications are possible
- Missing features are more critical than optimization
- Developer experience matters more than raw performance at this stage

---

## Phase 3 Weeks 3-4: Memory Management & Error Handling

**Goal**: Enable complex data structures and professional error handling

### Week 3: Reference Counting

**Objective**: Implement Rust-style reference counted types

#### Day 1-2: Rc<T> (Single-threaded Reference Counting)
- **Parser**: Add `Rc of Type` syntax
- **AST**: New `RcType` node
- **Type System**: `RcType` class with weak reference support
- **IR Generation**: 
  - `rc_new()` - Allocate with refcount=1
  - `rc_retain()` - Increment refcount
  - `rc_release()` - Decrement refcount, free if zero
  - LLVM intrinsics for atomic operations
- **Runtime**: Reference counting runtime library
- **Tests**: Circular reference detection, memory leak tests

**Syntax Example:**
```nlpl
set ptr to Rc of Integer with 42
set ptr2 to ptr  # Automatic retain
# ptr automatically released when out of scope
```

#### Day 3-4: Weak<T> (Weak References)
- **Purpose**: Break circular references
- **Implementation**: Weak pointer doesn't increment refcount
- **API**: 
  - `weak_of` - Create weak reference
  - `upgrade` - Attempt to get strong reference
  - Returns Option<Rc<T>>

**Syntax Example:**
```nlpl
set strong to Rc of Node with my_node
set weak to weak_of strong
# strong can be freed, weak becomes null
```

#### Day 5: Drop Semantics (Destructors)
- **Parser**: Add `destructor` function type
- **Execution**: Automatic cleanup when Rc refcount reaches zero
- **Integration**: Call destructors for struct members

**Syntax Example:**
```nlpl
struct File
    handle as Integer
    
    destructor
        call close_file with self.handle
    end
end
```

**Week 3 Deliverables:**
- ✅ Rc<T> type fully implemented
- ✅ Weak<T> for cycle breaking
- ✅ Automatic memory management (RAII)
- ✅ 20+ tests for memory correctness
- ✅ Example programs (linked list, tree, graph)

---

### Week 4: Result/Option Types

**Objective**: Professional error handling without exceptions

#### Day 1-2: Option<T> Type
- **Purpose**: Represent optional values (Some/None)
- **Parser**: Add `Option of Type` syntax
- **Pattern Matching**: `match` expressions for Option
- **Methods**:
  - `is_some()`, `is_none()`
  - `unwrap()` - Panic if None
  - `unwrap_or()` - Default value
  - `map()` - Transform wrapped value

**Syntax Example:**
```nlpl
function find_user with id as Integer returns Option of User
    if user_exists with id
        return Some with user
    else
        return None
    end
end

match call find_user with 42
    case Some with user
        print text user.name
    case None
        print text "User not found"
end
```

#### Day 3-4: Result<T, E> Type
- **Purpose**: Represent success/failure with error details
- **Syntax**: `Result of Value, Error`
- **Pattern Matching**: Destructuring Ok/Err
- **Methods**:
  - `is_ok()`, `is_err()`
  - `unwrap()` - Panic if Err
  - `unwrap_or()` - Default value
  - `map()`, `map_err()` - Transform wrapped value

**Syntax Example:**
```nlpl
function divide with a as Float, b as Float returns Result of Float, String
    if b is equal to 0.0
        return Err with "Division by zero"
    else
        return Ok with a divided by b
    end
end

match call divide with 10.0 and 0.0
    case Ok with result
        print text result
    case Err with message
        print text "Error: " concatenate message
end
```

#### Day 5: Error Propagation Operator (?)
- **Purpose**: Automatic error propagation (Rust-style `?`)
- **Syntax**: `try expression` or `expression?`
- **Behavior**: Return error immediately if Err, unwrap if Ok

**Syntax Example:**
```nlpl
function read_config returns Result of Config, String
    set file to try open_file with "config.txt"
    set contents to try read_all with file
    set config to try parse_config with contents
    return Ok with config
end
```

**Week 4 Deliverables:**
- ✅ Option<T> fully implemented
- ✅ Result<T, E> fully implemented
- ✅ Pattern matching for both types
- ✅ Error propagation operator (?)
- ✅ 30+ tests for error handling
- ✅ Rewrite existing error-prone code with Result/Option

---

## Phase 3 Weeks 5-6: Threading & Concurrency

**Goal**: Enable multi-threaded applications

### Week 5: Thread Primitives

#### Day 1-2: Thread Spawn/Join
- **FFI**: Integrate with pthread or native threads
- **API**:
  - `spawn_thread` - Create new thread
  - `join_thread` - Wait for completion
  - `thread_id` - Get current thread ID
- **Safety**: Thread-safe types (Arc<T>, Mutex<T>)

**Syntax Example:**
```nlpl
function worker_thread
    print text "Running in thread!"
end

set thread to spawn_thread with worker_thread
call join_thread with thread
```

#### Day 3-4: Synchronization Primitives
- **Mutex<T>**: Mutual exclusion lock
- **RwLock<T>**: Reader-writer lock
- **Semaphore**: Counting semaphore
- **Atomic<T>**: Lock-free atomic operations

**Syntax Example:**
```nlpl
set counter to Mutex of Integer with 0

function increment_counter
    set guard to lock counter
    set guard to guard plus 1
    unlock counter
end
```

#### Day 5: Arc<T> (Thread-Safe Reference Counting)
- **Purpose**: Share ownership across threads
- **Implementation**: Atomic reference counting
- **Combines**: Rc<T> + atomic operations

**Syntax Example:**
```nlpl
set data to Arc of List of Integer with list 1, 2, 3

function process_data with shared_data as Arc of List of Integer
    # Can safely access from multiple threads
end

set thread1 to spawn_thread with process_data and data
set thread2 to spawn_thread with process_data and data
```

**Week 5 Deliverables:**
- ✅ Thread spawn/join working
- ✅ Mutex, RwLock, Semaphore implemented
- ✅ Arc<T> for thread-safe sharing
- ✅ 25+ concurrency tests
- ✅ Multi-threaded benchmark suite

---

### Week 6: Channels & Message Passing

#### Day 1-3: Channels
- **mpsc**: Multi-producer, single-consumer channels
- **API**:
  - `create_channel()` - Returns (Sender, Receiver)
  - `send()` - Send message
  - `receive()` - Blocking receive
  - `try_receive()` - Non-blocking receive

**Syntax Example:**
```nlpl
set channel to create_channel of Integer

function producer with sender as Sender of Integer
    set i to 0
    while i is less than 10
        call send with sender and i
        set i to i plus 1
    end
end

function consumer with receiver as Receiver of Integer
    while true
        match try_receive with receiver
            case Some with value
                print value
            case None
                break
        end
    end
end

set thread1 to spawn_thread with producer and channel.sender
set thread2 to spawn_thread with consumer and channel.receiver
```

#### Day 4-5: Thread Pool
- **Purpose**: Reusable worker threads
- **API**:
  - `create_thread_pool(size)` - Create pool
  - `submit_task()` - Add work to queue
  - `shutdown()` - Clean shutdown

**Week 6 Deliverables:**
- ✅ Channel-based message passing
- ✅ Thread pool implementation
- ✅ Producer-consumer examples
- ✅ Parallel algorithm examples (parallel map, reduce)

---

## Phase 3 Weeks 7-8: Standard Library Expansion

**Goal**: Expand stdlib from 6 to 20+ modules

### Week 7: Essential Utilities

#### New Modules:
1. **regex** - Regular expression matching
   - `compile()` - Compile pattern
   - `match()` - Find matches
   - `replace()` - String replacement
   - `split()` - Split by pattern

2. **json** - JSON parsing and serialization
   - `parse()` - String to JSON object
   - `stringify()` - Object to JSON string
   - `validate()` - Schema validation

3. **datetime** - Date and time handling
   - `now()` - Current time
   - `parse()` - Parse ISO8601
   - `format()` - Custom formatting
   - `add_duration()` - Arithmetic

4. **fs** (advanced) - Enhanced file operations
   - `walk_dir()` - Recursive directory traversal
   - `copy()`, `move()`, `remove()` - File operations
   - `metadata()` - File info
   - `watch()` - File system notifications

5. **process** - Process management
   - `spawn()` - Run external command
   - `pipe()` - Process pipes
   - `env()` - Environment variables

**Week 7 Deliverables:**
- ✅ 5 new stdlib modules
- ✅ 100+ new functions
- ✅ Comprehensive documentation
- ✅ 50+ unit tests per module

---

### Week 8: Advanced Modules

#### New Modules:
6. **crypto** - Cryptographic operations
   - `hash()` - SHA256, SHA512, MD5
   - `hmac()` - HMAC signatures
   - `encrypt()`, `decrypt()` - AES encryption
   - `random_bytes()` - Secure random generation

7. **compress** - Compression utilities
   - `gzip()`, `ungzip()` - Gzip compression
   - `deflate()`, `inflate()` - Deflate algorithm
   - `zip()`, `unzip()` - ZIP archives

8. **encoding** - Encoding/decoding
   - `base64_encode()`, `base64_decode()`
   - `url_encode()`, `url_decode()`
   - `hex_encode()`, `hex_decode()`
   - `utf8_validate()` - UTF-8 validation

9. **testing** - Unit testing framework
   - `assert_equal()`, `assert_not_equal()`
   - `assert_true()`, `assert_false()`
   - `test_suite()` - Test organization
   - `benchmark()` - Performance testing

10. **logging** - Logging framework
    - `log_debug()`, `log_info()`, `log_error()`
    - `configure()` - Log levels and outputs
    - `format()` - Custom log formatting

**Week 8 Deliverables:**
- ✅ 5 additional stdlib modules
- ✅ Total 15 new modules (21 total including original 6)
- ✅ Comprehensive test coverage
- ✅ Example programs demonstrating each module

---

## Success Metrics

### Performance (Maintained)
- ✅ Within 3x of C performance (already achieved: 1.80-2.52x)
- Target: Maintain performance while adding features

### Memory Management
- ✅ Zero memory leaks in reference counted code
- ✅ Circular reference detection working
- ✅ RAII working correctly (destructors called)

### Error Handling
- ✅ 100% of stdlib functions use Result/Option
- ✅ Error propagation operator working
- ✅ Pattern matching complete

### Threading
- ✅ Multi-threaded programs compile and run
- ✅ No data races detected
- ✅ Thread pool handles 1000+ tasks

### Standard Library
- ✅ 20+ modules (vs 6 currently)
- ✅ 500+ functions (vs ~50 currently)
- ✅ 100% documented with examples
- ✅ 90%+ test coverage

### Developer Experience
- ✅ Clear error messages for all new features
- ✅ Example programs for every major feature
- ✅ Documentation up-to-date

---

## Risk Mitigation

### Risk 1: Feature Scope Creep
**Mitigation**: Strict adherence to weekly deliverables, no gold-plating

### Risk 2: Performance Regression
**Mitigation**: Run benchmark suite after each major change

### Risk 3: Breaking Changes
**Mitigation**: Maintain backward compatibility, deprecation warnings

### Risk 4: Testing Burden
**Mitigation**: Write tests alongside features, not after

---

## Post-Phase 3 Plan

### Phase 4: Advanced Features (4-6 weeks)
1. **Generics Enhancement**
   - Bounded type parameters
   - Trait-like system
   - Associated types

2. **C++ FFI**
   - Call C++ libraries
   - Template instantiation
   - Exception handling across boundary

3. **Module System Enhancement**
   - Package manager prototype
   - Dependency resolution
   - Semantic versioning

### Phase 5: Real-World Application (4-6 weeks)
1. **Build Substantial Project**
   - Options: HTTP server, database, compiler tool
   - Validates all features working together
   - Identifies remaining gaps

2. **Performance Tuning**
   - Profile real application
   - Optimize hot paths
   - Target 1.5x of C performance

3. **Documentation & Tutorials**
   - Complete language guide
   - Tutorial series
   - API reference

---

## Timeline Summary

| Phase | Duration | Focus | Status |
|-------|----------|-------|--------|
| **Week 1-2** | 2 weeks | Optimization | ✅ COMPLETE |
| **Week 3-4** | 2 weeks | Memory & Errors | 🔄 NEXT |
| **Week 5-6** | 2 weeks | Threading | 📅 PLANNED |
| **Week 7-8** | 2 weeks | Stdlib Expansion | 📅 PLANNED |
| **Total** | 8 weeks | Phase 3 | 25% COMPLETE |

---

## Next Steps

**Immediate Actions (Week 3 Day 1):**
1. Design Rc<T> API and syntax
2. Implement parser support for `Rc of Type`
3. Create AST nodes for reference counted types
4. Begin runtime library for reference counting
5. Write first Rc<T> test program

**Preparation:**
- Review Rust's Rc/Arc implementation for inspiration
- Study LLVM atomic intrinsics documentation
- Plan memory leak detection strategy

---

**Phase 3 Status**: Week 2 COMPLETE, Week 3 READY TO START  
**Strategic Direction**: Core language features over optimization  
**Goal**: Make NexusLang capable of building real applications  
**Timeline**: 6 more weeks of focused development ahead
