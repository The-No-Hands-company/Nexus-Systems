#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ostream>
#include <unordered_map>

#include <set>
namespace nexus::utility::metrics {

/// @brief Code complexity analyzer
class ComplexityAnalyzer {
public:
    struct FunctionMetrics {
        std::string function_name;
        size_t line_count = 0;
        size_t cyclomatic_complexity = 0;
        size_t max_nesting_depth = 0;
        size_t parameter_count = 0;
    };

    /// Analyze function complexity (requires source code parsing)
    static FunctionMetrics analyzeFunction(const std::string& source_code);

    /// Simple heuristic-based analysis
    static size_t estimateCyclomaticComplexity(const std::string& source_code);
    static size_t countNestingDepth(const std::string& source_code);
};

/// @brief Code coverage tracker
class CoverageTracker {
public:
    void markLineExecuted(const std::string& file, int line_number);
    void registerLine(const std::string& file, int line_number);

    struct CoverageStats {
        size_t total_lines = 0;
        size_t executed_lines = 0;
        float coverage_percent = 0.0f;
    };

    CoverageStats getStats(const std::string& file = "") const;
    void printReport(std::ostream& os) const;

    void clear();

private:
    struct FileInfo {
        std::set<int> all_lines;
        std::set<int> executed_lines;
    };

    std::unordered_map<std::string, FileInfo> files_;
};

/// @brief Performance metrics collector
class PerformanceMetrics {
public:
    void recordMetric(const std::string& name, double value);

    struct Stats {
        double min = 0.0;
        double max = 0.0;
        double mean = 0.0;
        double stddev = 0.0;
        size_t count = 0;
    };

    Stats getStats(const std::string& name) const;
    void printReport(std::ostream& os) const;

    void clear();

private:
    struct MetricData {
        std::vector<double> values;
    };

    std::unordered_map<std::string, MetricData> metrics_;
};

} // namespace nexus::utility::metrics
