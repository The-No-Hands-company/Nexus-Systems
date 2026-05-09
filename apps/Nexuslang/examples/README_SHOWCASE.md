# Async Channels Parallel Pipeline Showcase

**File**: `examples/23_async_channels_parallel_pipeline.nlpl`

This example demonstrates NexusLang's capabilities in **concurrent data processing**, combining three key language features:

1. **Async Functions** — Non-blocking aggregation task that consumes results via a channel
2. **Channel Communication** — Type-safe inter-task message passing between parallel processors and the aggregator
3. **Parallel For Loops** — Multi-threaded processing of sensor readings across CPU cores

## Scenario

A realistic IoT pipeline processes readings from multiple sensors:

- **10 sensors** each generating **100 readings** (1000 total readings)
- **Parallel processor**: Chunks are processed concurrently, computing local min/max/sum/avg
- **Async aggregator**: Runs in background, consuming results from a channel, updating global statistics, and logging progress
- **Main thread**: Orchestrates the flow, partitioning work and waiting for completion

## What This Demonstrates

### Natural Language Syntax
```nlpl
* Define async aggregation task
async function aggregate_from_channel with reading_chan as Channel and total_expected as Integer
  set total_received to 0
  while total_received is less than total_expected
    set result to receive from reading_chan
    * Process result...
  end
end

* Launch async task
set aggregator_task to spawn aggregate_from_channel with result_chan and total_readings
```

### Parallel Processing
```nlpl
parallel for each reading in readings
  if reading is less than chunk_min
    set chunk_min to reading
  end
  * ... compare and aggregate ...
end
```

### Channel-Based Communication
```nlpl
set result_chan to create channel
send chunk_report to result_chan
set result to receive from reading_chan
```

### Task Coordination
```nlpl
* Wait for async aggregator to finish
await aggregator_task
```

## Expected Output

```
NexusLang Async/Channels/Parallel Showcase
=============================================

Created 1000 synthetic sensor readings

Launching parallel processing chunks...

Processed chunk 0 with 250 readings
Processed chunk 1 with 250 readings
Processed chunk 2 with 250 readings
Processed chunk 3 with 250 readings

Intermediate: Processed 250 readings so far
Intermediate: Processed 500 readings so far
Intermediate: Processed 750 readings so far

Final Pipeline Statistics:
  Total readings processed: 1000
  Sum: [computed sum]
  Min: [computed min]
  Max: [computed max]
  Average: [computed average]
  Reports aggregated: 4

Pipeline complete!
```

## Key NexusLang Features Showcased

| Feature | Line(s) | Purpose |
|---------|---------|---------|
| **Struct Types** | Lines 20–24, 26–32 | Define strongly-typed SensorReading and AggregateReport |
| **List Processing** | Lines 34–44 | Generate synthetic data using natural-language loops |
| **Parallel For** | Lines 58–67 | Process chunks concurrently with `parallel for each` |
| **Async Functions** | Lines 70–112 | Define long-running aggregation task with `async` keyword |
| **Channels** | Lines 125–128 | Create and communicate via type-safe channel |
| **Spawning Tasks** | Line 130 | Launch async task with `spawn` and capture handle |
| **Task Awaiting** | Line 158 | Wait for completion with `await` |
| **Natural Control Flow** | Throughout | Readable `if`/`while`/`for` instead of curly-brace syntax |

## Compilation & Execution

```bash
# Compile and run
python src/main.py examples/23_async_channels_parallel_pipeline.nlpl

# With debug output
python src/main.py examples/23_async_channels_parallel_pipeline.nlpl --debug
```

## Design Notes

- **Chunk Partitioning**: Work is split into 4 chunks to demonstrate parallel efficiency
- **Progress Reporting**: Aggregator logs every 50 readings received
- **Type Safety**: All structures use explicit field types
- **Error Handling**: Null checks on channel receives (defensive programming)
- **Readability**: Natural language keywords make the pipeline logic clear without ceremony

This example serves as a **production-grade proof** that NexusLang successfully integrates async, channels, and parallel processing into a coherent, readable, real-world scenario.
