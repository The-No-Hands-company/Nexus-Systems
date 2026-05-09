# Log Analyzer - Systems Programming Showcase

## Overview

The Log Analyzer is a comprehensive system monitoring and debugging tool that demonstrates NexusLang's capabilities for log processing, data aggregation, and systems administration. It's a realistic utility suitable for DevOps engineers, system administrators, and anyone managing production systems.

## Domain: Systems Administration & DevOps

This application showcases NexusLang's suitability for:
- System log processing and analysis
- Performance monitoring and alerting
- Data aggregation and statistics
- Pattern matching for error detection
- String parsing and text processing
- Contract-driven reliability

## Features Demonstrated

### 1. Structured Data Modeling
- **LogEntry**: Parsed log record with timestamp, level, source, message
- **LogStats**: Aggregated statistics (counts by level, error percentage)
- **AnalysisReport**: Complete analysis with filtered error/warning entries

```nlpl
struct LogEntry
  timestamp as String
  level as String
  source as String
  message as String
  line_number as Integer
end
```

### 2. String Parsing and Extraction
Functions demonstrate text processing for log parsing:

- `parse_timestamp` - Extract ISO 8601 timestamps
- `extract_log_level` - Identify severity levels (DEBUG, INFO, WARNING, ERROR, CRITICAL)
- `extract_source` - Extract component/module names
- `extract_message` - Extract message content

```nlpl
function extract_log_level with line as String returns String
  if line contains "ERROR"
    return "ERROR"
  end
  // ... level detection
end
```

### 3. Contracts for Data Validation
Contracts enforce data quality throughout the pipeline:

```nlpl
function parse_log_line with line as String and line_num as Integer returns LogEntry
  require line is not empty with "Log line cannot be empty"
  require line_num is greater than 0 with "Line number must be positive"
  // ... parsing logic
end
```

### 4. Data Aggregation and Statistics
Efficient computation of log metrics:

```nlpl
for each entry in entries
  if entry.level equals "ERROR"
    set error_count to error_count plus 1
  end
end
```

### 5. Pattern Matching for Classification
Logic for categorizing and filtering logs:

```nlpl
if entry.level equals "ERROR" or entry.level equals "CRITICAL"
  set errors to errors concatenated with [entry]
end
```

### 6. Multi-Return Functions
Functions returning tuples for combined results:

```nlpl
function extract_errors_and_warnings with entries as List of LogEntry 
  returns List of LogEntry and List of LogEntry
  // ... filtering logic
  return errors and warnings
end
```

### 7. Formatted Reporting
Professional log reports with statistics and findings:

```nlpl
print text ("  Total Entries: " concatenated with (stats.total_entries as String))
print text ("  Error Rate: " concatenated with ((stats.error_percentage times 100.0) as String) concatenated with "%")
```

## Execution Example

Running the log analyzer:

```bash
python src/main.py examples/systems/log_analyzer.nlpl
```

Expected output:
```
=== Log Analysis Report ===

Summary Statistics:
  Total Entries: 14
  DEBUG: 3
  INFO: 4
  WARNING: 3
  ERROR: 2
  CRITICAL: 1
  Error Rate: 21.43%

Critical Issues Found:
  [2026-05-04T14:36:22Z] ERROR in system
    Connection pool exhausted - rejecting new connections
  [2026-05-04T14:36:24Z] ERROR in system
    Unhandled exception in request handler
  [2026-05-04T14:37:45Z] CRITICAL in system
    Shutdown timeout - forcing exit

Warnings Detected:
  [2026-05-04T14:33:12Z] system
    Memory usage at 68% of limit
  [2026-05-04T14:35:01Z] system
    Query execution time: 1250ms (threshold: 1000ms)
  [2026-05-04T14:36:23Z] system
    Response timeout for client 192.168.1.100

Health Assessment:
  Status: WARNING - Elevated error rate

Log analysis completed successfully
```

## Real-World Enhancements

This application could be extended with:

### File I/O Integration
```nlpl
// Read from actual log files
function read_log_file with filename as String returns List of String
  // Open file, read lines, return contents
end

// Write analysis report to file
function write_report_to_file with report as AnalysisReport and filename as String
  // Write formatted report to disk
end
```

### Filtering and Search
- Time range filtering (before/after timestamps)
- Component-specific filtering
- Message content search with regex

### Advanced Analytics
- Trend analysis (error rate over time)
- Anomaly detection (unusual patterns)
- Correlation analysis (which errors follow which warnings)
- Root cause analysis (tracing error chains)

### Real-Time Streaming
- Read logs from stdin or log aggregators
- Use channels for streaming log events
- Async processing with alert generation

### Integration with Monitoring Systems
- Export metrics in Prometheus format
- Push alerts to Slack/PagerDuty
- Store metrics in time-series database

### Performance Optimization
- Parallel processing of large log files
- Index building for fast searches
- Memory-efficient streaming for huge logs

## Educational Value

This example teaches:

1. **Text Processing**: String parsing, extraction, pattern matching
2. **NexusLang Idioms**:
   - Struct-based data modeling
   - Collection filtering and aggregation
   - Multi-return functions
   - Contract-driven validation
   - Type-safe string operations
3. **Algorithm Design**:
   - Single-pass aggregation
   - Categorical classification
   - Statistical computation
4. **Systems Programming**:
   - Log format parsing
   - Severity level handling
   - Error rate calculation
   - Health assessment logic

## Performance Characteristics

**Computational Complexity**:
- Single pass parsing: O(n) where n = number of log lines
- Statistics calculation: O(n) single sweep
- Filtering: O(n) for each category

**Real-World Scale**:
- 1000 lines: ~milliseconds
- 100K lines: ~tenths of seconds
- 1M lines: ~seconds (suitable for batch processing)

**Parallel Opportunities**:
- Process multiple log files in parallel
- Partition logs by time range for parallel analysis
- Use channels for streaming multi-source logs

## Build and Deployment

Compile to native code:

```bash
python src/main.py examples/systems/log_analyzer.nlpl --compile --backend llvm
./log_analyzer_output
```

Deployment scenarios:
- Scheduled cron job for daily log analysis
- Integration with log aggregation pipelines (ELK, Splunk)
- Embedded in monitoring systems (Prometheus, Grafana)
- Standalone utility for ad-hoc log investigation

## Testing Strategy

Contract violations are caught immediately:

```nlpl
// This would fail:
set invalid_entry to parse_log_line with "" and -1
// Error: "Log line cannot be empty"
// Error: "Line number must be positive"
```

## Extension Ideas

1. **Structured Logging**: Support JSON log format parsing
2. **Time-Series Analysis**: Track metrics over time windows
3. **Machine Learning**: Anomaly detection via statistical models
4. **Distributed Logging**: Aggregate logs from multiple sources
5. **Alerting**: Real-time threshold-based alerts
6. **Visualization**: Generate charts and dashboards
7. **Compression**: Efficiently store analyzed logs
8. **Privacy**: Redact sensitive data from logs

## Typical Use Cases

1. **Daily Log Review**: Automated summary of system health
2. **Error Investigation**: Quick identification of problems
3. **Performance Monitoring**: Track response times and throughput
4. **Compliance**: Generate audit logs and reports
5. **Incident Response**: Rapid analysis during outages
6. **Capacity Planning**: Identify resource constraints
7. **Trend Analysis**: Long-term system behavior changes
8. **Integration Testing**: Validate expected log messages

## Related Examples

See also:
- [Portfolio Analyzer](../financial/portfolio_analyzer.nlpl) - Data aggregation patterns
- [Async Channels Pipeline](../23_async_channels_parallel_pipeline.nlpl) - Real-time event streaming
- [REST API Server](../web_backend/rest_api_users.nxl) - Structured data handling
- [CSV Analysis](../data_science/csv_analysis.nxl) - Similar data processing patterns

## Conclusion

The Log Analyzer demonstrates that NexusLang is highly suitable for systems programming tasks, combining readable natural language syntax with the performance and control needed for production monitoring tools. Its type safety and contract system make it particularly well-suited for reliability-critical DevOps utilities where correctness directly impacts system availability.
