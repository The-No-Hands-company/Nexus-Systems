# Tutorial 13: Performance Optimization

**Time:** ~60 minutes  
**Prerequisites:** [Memory Management](02-memory-management.md), [Generics](../intermediate/01-generics-and-type-system.md)

---

## Part 1 — Measure First

Never optimize code you have not measured.  Use the built-in benchmarking
tools to identify actual bottlenecks before making changes.

```nexuslang
import system

set start to system.time_nanoseconds()
# ... code to measure ...
set elapsed to system.time_nanoseconds() minus start
print text "Elapsed: " plus convert elapsed to string plus " ns"
```

Or with the benchmark harness (see `benchmarks/`):

```bash
nlpl build bench
```

---

## Part 2 — Algorithmic Improvements

The biggest gains always come from choosing a better algorithm, not from
micro-optimising a bad one.

| Problem | Naive | Better |
|---------|-------|--------|
| Find item in list | Linear scan O(n) | Pre-sort + binary search O(log n) |
| Unique items | Nested loops O(n²) | Set membership O(1) per check |
| Repeated string concat | O(n²) copies | Collect list then `join` |
| Tree search | DFS with re-traversal | Memoised DP |

---

## Part 3 — Avoiding Repeated Allocation

Allocating inside a hot loop creates gc pressure.  Pre-allocate and reuse:

```nexuslang
# Slow: allocates a new string on every iteration
set report to ""
for each row in rows
    set report to report plus format_row with row   # O(n^2) copies

# Fast: collect into a list and join once
set parts to []
for each row in rows
    append (format_row with row) to parts
set report to join parts with separator: "\n"
```

---

## Part 4 — Type Annotations in Hot Paths

Explicit type annotations let the interpreter skip runtime type inference in
tight loops:

```nexuslang
# Without annotations (slower in interpreter)
set total to 0
for each n in big_list
    set total to total plus n

# With annotations (faster)
set total as Integer to 0
for each n as Integer in big_list
    set total to total plus n
```

---

## Part 5 — Using the Right Collection

| Use case | Collection | Why |
|----------|-----------|-----|
| Indexed access by position | `List` | O(1) index |
| Fast membership test | `Set` | O(1) contains |
| Key-value lookup | `Dict` | O(1) lookup |
| LIFO | `Stack` (from collections) | O(1) push/pop |
| FIFO queue | `Queue` (from collections) | O(1) enqueue/dequeue |

```nexuslang
import collections

set seen to collections.new_set()
set result to []

for each item in data
    if not collections.set_contains with seen and item
        collections.set_add with seen and item
        append item to result

# result now contains unique items in original order, O(n) total
```

---

## Part 6 — Concurrency for CPU-Bound Work

Spread independent computations across multiple threads:

```nexuslang
import system

function process_chunk with chunk as List of Integer returns Integer
    set acc to 0
    for each n as Integer in chunk
        set acc to acc plus (n times n)
    return acc
end

function parallel_sum with data as List of Integer returns Integer
    set cpu_count to system.cpu_count()
    set chunk_size to length(data) divided by cpu_count
    set threads to []

    set i to 0
    while i is less than cpu_count
        set start_idx to i times chunk_size
        set end_idx to (i plus 1) times chunk_size
        set chunk to data[start_idx to end_idx]
        append (system.spawn_thread with process_chunk and chunk) to threads
        set i to i plus 1

    set total to 0
    for each t in threads
        set total to total plus (system.join_thread with t)
    return total
end
```

---

## Part 7 — Low-Level Optimizations via Inline Assembly

For SIMD or hardware-specific acceleration:

```nexuslang
# Horizontal sum of 4 floats using SSE
set v to [1.0, 2.0, 3.0, 4.0]
set sum to 0.0
set ptr to address of v[0]

asm
    code
        "movups xmm0, [$1]"
        "haddps xmm0, xmm0"
        "haddps xmm0, xmm0"
        "movss  [$0], xmm0"
    outputs "=m": sum
    inputs  "r":  ptr
    clobbers "xmm0"
end

print text convert sum to string    # 10.0
```

---

## Part 8 — Build Profile and LTO

Always benchmark release builds, not dev builds:

```toml
[profile.release]
opt-level = 3
lto       = true
```

```bash
nlpl build bench --profile release
```

LTO (link-time optimisation) enables cross-module inlining and dead code
elimination.

---

## Part 9 — Reading a Profiler Output

```bash
# Attach callgrind (Linux)
valgrind --tool=callgrind nlpl program.nlpl
callgrind_annotate callgrind.out.*

# Or use perf
perf record nlpl program.nlpl
perf report
```

Focus on functions that appear at the top of the report — these consume the
most CPU time and are the highest-value targets.

---

## Summary

| Technique | When to use |
|-----------|-------------|
| Better algorithm | Always first |
| Pre-allocate outside loops | Hot loops with known max size |
| Type annotations | Tight numeric loops |
| Right collection | Membership test, key lookup |
| Thread parallelism | CPU-bound independent chunks |
| Inline assembly + SIMD | Innermost kernel loops |
| Release profile + LTO | Always for production benchmarks |

**Next:** [Writing Standard Library Modules](04-writing-stdlib-modules.md)
