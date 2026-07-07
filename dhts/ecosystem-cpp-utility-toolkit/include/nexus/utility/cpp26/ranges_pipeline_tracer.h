#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <any>

namespace nexus::utility::cpp26 {

/// Trace C++26 ranges pipeline execution (zip, concat, adjacent, chunk, slide, stride)
class RangesPipelineTracer {
public:
    struct PipelineStep {
        std::string operation;       // "filter", "transform", "zip", "chunk", etc.
        size_t elementsProcessed;
        size_t elementsProduced;
        std::chrono::microseconds processingTime{0};
        bool shortCircuited;
    };

    struct PipelineTrace {
        std::string pipelineName;
        std::vector<PipelineStep> steps;
        std::chrono::microseconds totalTime{0};
        size_t totalInput;
        size_t totalOutput;
    };

    void startPipeline(const std::string& name, size_t inputSize) {
        current_ = {name, {}, std::chrono::microseconds(0), inputSize, 0};
        startTime_ = std::chrono::steady_clock::now();
    }

    void recordStep(const std::string& op, size_t processed, size_t produced,
                    std::chrono::microseconds time) {
        current_.steps.push_back({op, processed, produced, time, false});
    }

    void recordShortCircuit(const std::string& op) {
        if (!current_.steps.empty()) current_.steps.back().shortCircuited = true;
    }

    PipelineTrace endPipeline() {
        auto end = std::chrono::steady_clock::now();
        current_.totalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - startTime_);
        for (const auto& s : current_.steps) current_.totalOutput = s.elementsProduced;
        traces_.push_back(current_);
        return current_;
    }

    std::vector<PipelineTrace> getTraces() const { return traces_; }

    PipelineTrace getLastTrace() const {
        return traces_.empty() ? PipelineTrace{} : traces_.back();
    }

    size_t totalTraces() const { return traces_.size(); }

    void clear() { traces_.clear(); }

private:
    PipelineTrace current_;
    std::chrono::steady_clock::time_point startTime_;
    std::vector<PipelineTrace> traces_;
};
} // namespace nexus::utility::cpp26
