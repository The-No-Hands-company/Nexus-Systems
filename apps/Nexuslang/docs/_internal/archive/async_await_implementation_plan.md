# Async/Await Implementation Plan for NexusLang

## Overview

Implementing true async/await in NexusLang using LLVM's switched-resume coroutine model.

**Estimated Effort:** 80-120 hours
**Priority:** P1 - Last remaining CRITICAL blocker

## LLVM Coroutine Primer

### Core Intrinsics Needed

1. **Initialization:**
   - `llvm.coro.id(i32, ptr, ptr, ptr)` - Create coroutine ID
   - `llvm.coro.size.i64()` - Get frame size for allocation
   - `llvm.coro.begin(token, ptr)` - Begin coroutine, returns handle
   - `llvm.coro.alloc(token)` - Check if dynamic allocation needed

2. **Suspension:**
   - `llvm.coro.save(ptr)` - Save state for suspension
   - `llvm.coro.suspend(token, i1)` - Suspend point (returns -1=suspend, 0=resume, 1=destroy)

3. **Cleanup:**
   - `llvm.coro.free(token, ptr)` - Get memory to free
   - `llvm.coro.end(ptr, i1, token)` - Mark coroutine end

4. **External Control:**
   - `llvm.coro.resume(ptr)` - Resume a suspended coroutine
   - `llvm.coro.destroy(ptr)` - Destroy a coroutine
   - `llvm.coro.done(ptr)` - Check if at final suspend

### Coroutine Frame Structure

```llvm
%coro.frame = type { 
  ptr,  ; resume function pointer
  ptr,  ; destroy function pointer
  i32,  ; suspend index
  ...   ; local variables that survive suspension
}
```

## Implementation Phases

### Phase 1: Coroutine Infrastructure (15-20h)

**Goal:** Declare coroutine intrinsics and support functions

1. **Add intrinsic declarations:**
```python
def _declare_coroutine_intrinsics(self):
    """Declare LLVM coroutine intrinsics."""
    self.emit('; Coroutine intrinsics')
    self.emit('declare token @llvm.coro.id(i32, ptr, ptr, ptr)')
    self.emit('declare i64 @llvm.coro.size.i64()')
    self.emit('declare ptr @llvm.coro.begin(token, ptr)')
    self.emit('declare i1 @llvm.coro.alloc(token)')
    self.emit('declare token @llvm.coro.save(ptr)')
    self.emit('declare i8 @llvm.coro.suspend(token, i1)')
    self.emit('declare ptr @llvm.coro.free(token, ptr)')
    self.emit('declare i1 @llvm.coro.end(ptr, i1, token)')
    self.emit('declare void @llvm.coro.resume(ptr)')
    self.emit('declare void @llvm.coro.destroy(ptr)')
    self.emit('declare i1 @llvm.coro.done(ptr)')
    self.emit('')
```

2. **Create Promise type for async functions:**
```nlpl
# Promise<T> holds the result of an async computation
struct Promise of T
    result as T
    ready as Boolean
    waiting_coroutine as Pointer
end
```

3. **Memory allocation for coroutine frames:**
   - Use malloc/free for dynamic allocation
   - Track frame size via `llvm.coro.size.i64()`

### Phase 2: Async Function Generation (25-35h)

**Goal:** Generate proper coroutine structure for async functions

**Pattern for async function:**
```llvm
define ptr @async_func(params...) presplitcoroutine {
entry:
  ; Create promise for return value
  %promise = alloca %Promise.T
  
  ; Initialize coroutine
  %id = call token @llvm.coro.id(i32 0, ptr %promise, ptr null, ptr null)
  %need.alloc = call i1 @llvm.coro.alloc(token %id)
  br i1 %need.alloc, label %coro.alloc, label %coro.begin

coro.alloc:
  %size = call i64 @llvm.coro.size.i64()
  %mem = call ptr @malloc(i64 %size)
  br label %coro.begin

coro.begin:
  %phi = phi ptr [ null, %entry ], [ %mem, %coro.alloc ]
  %hdl = call ptr @llvm.coro.begin(token %id, ptr %phi)
  
  ; === Function body with suspend points ===
  ; ... user code ...
  
  ; Store result in promise
  store T %result, ptr %promise
  store i1 true, ptr %promise.ready

final.suspend:
  %final = call i8 @llvm.coro.suspend(token none, i1 true)
  switch i8 %final, label %suspend [
    i8 0, label %trap
    i8 1, label %cleanup
  ]

trap:
  call void @llvm.trap()
  unreachable

cleanup:
  %mem.cleanup = call ptr @llvm.coro.free(token %id, ptr %hdl)
  %need.free = icmp ne ptr %mem.cleanup, null
  br i1 %need.free, label %do.free, label %suspend

do.free:
  call void @free(ptr %mem.cleanup)
  br label %suspend

suspend:
  call i1 @llvm.coro.end(ptr %hdl, i1 false, token none)
  ret ptr %hdl
}
```

**Key Changes to `_generate_async_function_definition`:**
- Add `presplitcoroutine` attribute
- Generate coroutine ID and allocation
- Generate promise allocation
- Track variables that survive suspension
- Generate final suspend and cleanup

### Phase 3: Await Expression (20-30h)

**Goal:** Generate proper suspend point for await

**Pattern for await:**
```llvm
await.point.N:
  ; Save coroutine state before potential suspension
  %save.N = call token @llvm.coro.save(ptr %hdl)
  
  ; Get the awaited coroutine handle (from calling async func)
  %awaited = call ptr @async_operation(...)
  
  ; Check if already ready
  %done = call i1 @llvm.coro.done(ptr %awaited)
  br i1 %done, label %await.ready.N, label %await.suspend.N

await.suspend.N:
  ; Not ready - suspend this coroutine
  %suspend = call i8 @llvm.coro.suspend(token %save.N, i1 false)
  switch i8 %suspend, label %suspend [
    i8 0, label %await.ready.N
    i8 1, label %cleanup
  ]

await.ready.N:
  ; Extract result from awaited coroutine's promise
  %promise.addr = ; get promise from awaited handle
  %result = load T, ptr %promise.addr
  
  ; Destroy awaited coroutine
  call void @llvm.coro.destroy(ptr %awaited)
  
  ; Continue with result...
```

**Key implementation:**
- Each `await` becomes a suspend point
- Save variables that will be needed after resume
- Resume awaited coroutine if not done
- Extract result from promise when ready

### Phase 4: Event Loop / Scheduler (15-20h)

**Goal:** Simple single-threaded scheduler for coroutines

**Options:**
1. **Simple synchronous scheduler** - Run coroutines to completion one at a time
2. **Task queue scheduler** - Queue of ready coroutines, round-robin execution
3. **Full async runtime** - Like Rust's tokio (complex, future work)

**Start with Option 2 - Task Queue:**

```c
// Runtime support structure
struct TaskQueue {
    void** coroutines;
    int count;
    int capacity;
};

void task_queue_push(TaskQueue* q, void* coro);
void* task_queue_pop(TaskQueue* q);

void run_until_complete(void* main_coro) {
    TaskQueue queue = {0};
    task_queue_push(&queue, main_coro);
    
    while (queue.count > 0) {
        void* coro = task_queue_pop(&queue);
        if (!llvm_coro_done(coro)) {
            llvm_coro_resume(coro);
            if (!llvm_coro_done(coro)) {
                task_queue_push(&queue, coro);
            }
        }
    }
}
```

**NLPL side:**
```nlpl
# Generated for programs with async main
async function main
    # ... async code ...
end

# Compiler generates:
# 1. Wrap main in coroutine
# 2. Call run_until_complete(main_coroutine)
```

### Phase 5: Integration (10-15h)

1. **Async function tracking:**
   - Track which functions are async
   - Validate await only in async contexts
   - Type system for Promise<T>

2. **Coroutine handle management:**
   - Return handles from async functions
   - Pass handles for await operations
   - Cleanup on scope exit

3. **Error propagation:**
   - Exception handling in coroutines
   - Promise rejection / error state

### Phase 6: Testing & Polish (10-15h)

**Test Cases:**
1. Simple async/await with single suspend
2. Multiple awaits in sequence
3. Nested async calls
4. Concurrent tasks (multiple coroutines)
5. Error handling in async
6. Async with closures

## Required Code Changes

### Files to Modify

1. **llvm_ir_generator.py:**
   - `_declare_coroutine_intrinsics()` - Add intrinsic declarations
   - `_generate_async_function_definition()` - Full rewrite for coroutines
   - `_generate_await_expression()` - Full rewrite for suspend points
   - Add `presplitcoroutine` attribute handling
   - Track async context state

2. **runtime support:**
   - Create `nxl_async_runtime.c` with scheduler
   - Task queue implementation
   - `run_until_complete()` main driver

3. **ast.py / parser.py:**
   - Ensure async/await AST nodes are complete
   - Add Promise type support

### Challenges

1. **Variable spilling:** Variables used across suspend must be stored in frame
2. **Frame size calculation:** LLVM calculates this, but we need proper phi nodes
3. **Cleanup paths:** Every suspend point needs cleanup path
4. **Exception handling:** Exceptions in coroutines are complex

## Success Criteria

- [ ] Async functions compile to proper coroutines
- [ ] Await expressions create suspend points
- [ ] Coroutines can suspend and resume
- [ ] Simple scheduler runs coroutines
- [ ] Test: sequential awaits work
- [ ] Test: multiple concurrent coroutines work
- [ ] Test: async return values work
- [ ] Test: error propagation works

## References

- LLVM Coroutines: https://llvm.org/docs/Coroutines.html
- C++ Coroutines: https://en.cppreference.com/w/cpp/language/coroutines
- Rust async: https://rust-lang.github.io/async-book/

## Timeline

| Phase | Effort | Description |
|-------|--------|-------------|
| 1 | 15-20h | Coroutine infrastructure |
| 2 | 25-35h | Async function generation |
| 3 | 20-30h | Await expression |
| 4 | 15-20h | Scheduler |
| 5 | 10-15h | Integration |
| 6 | 10-15h | Testing |
| **Total** | **95-135h** | |
