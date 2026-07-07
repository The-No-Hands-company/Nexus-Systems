#!/usr/bin/env python3
"""Generate ALL remaining stubs: embedded(2), audio(5), codequality(5), algorithmic(4), event(3), timetravel(4), logging(1)"""

import os

BASE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug"
INC = f"{BASE}/include/nexus/utility"
SRC = f"{BASE}/src/utility"

def w(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f: f.write(content)

def gen(cat, name, header, source):
    ns = cat.replace('/', '::')
    h = f'#pragma once\n{header}'
    s = source
    w(f"{INC}/{cat}/{name}.h", h)
    w(f"{SRC}/{cat}/{name}.cpp", s)
    print(f"  {cat}/{name}")

# ===== EMBEDDED (2 remaining) =====
gen('embedded', 'wcet_analyzer', '''
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>

namespace nexus::utility::embedded {

class WcetAnalyzer {
public:
    struct ExecutionRecord {
        std::string functionName;
        std::chrono::microseconds bestTime{0}, worstTime{0}, lastTime{0};
        size_t callCount = 0;
    };
    static WcetAnalyzer& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;
    void recordExecution(const std::string& func, std::chrono::microseconds elapsed);
    ExecutionRecord getRecord(const std::string& func) const;
    std::vector<ExecutionRecord> getAllRecords() const;
    void reset();
private:
    WcetAnalyzer() = default;
    ~WcetAnalyzer() = default;
    bool enabled_ = false;
    std::unordered_map<std::string, ExecutionRecord> records_;
};
}''',
'''#include "nexus/utility/embedded/wcet_analyzer.h"
#include <algorithm>
namespace nexus::utility::embedded {
WcetAnalyzer& WcetAnalyzer::instance() { static WcetAnalyzer i; return i; }
void WcetAnalyzer::initialize() { enabled_ = true; records_.clear(); }
void WcetAnalyzer::shutdown() { enabled_ = false; records_.clear(); }
bool WcetAnalyzer::isEnabled() const { return enabled_; }
void WcetAnalyzer::recordExecution(const std::string& f, std::chrono::microseconds e) {
    if (!enabled_) return;
    auto& r = records_[f]; r.functionName = f;
    if (r.callCount == 0) { r.bestTime = e; r.worstTime = e; }
    else { r.bestTime = std::min(r.bestTime, e); r.worstTime = std::max(r.worstTime, e); }
    r.lastTime = e; r.callCount++;
}
WcetAnalyzer::ExecutionRecord WcetAnalyzer::getRecord(const std::string& f) const {
    auto it = records_.find(f); return it != records_.end() ? it->second : ExecutionRecord{};
}
std::vector<WcetAnalyzer::ExecutionRecord> WcetAnalyzer::getAllRecords() const {
    std::vector<ExecutionRecord> r; for (auto& [n, rec] : records_) r.push_back(rec); return r;
}
void WcetAnalyzer::reset() { records_.clear(); }
}''')

gen('embedded', 'dma_transfer_monitor', '''
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace nexus::utility::embedded {

class DmaTransferMonitor {
public:
    struct TransferRecord {
        std::string channel;
        uintptr_t source, destination;
        size_t size;
        std::chrono::microseconds duration{0};
        bool success = true;
        std::chrono::system_clock::time_point timestamp;
    };
    static DmaTransferMonitor& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;
    void recordTransfer(const std::string& ch, uintptr_t src, uintptr_t dst, size_t sz, std::chrono::microseconds dur, bool ok);
    std::vector<TransferRecord> getHistory() const;
    std::vector<TransferRecord> getHistoryForChannel(const std::string& ch) const;
    size_t getTotalBytes() const;
    size_t getTransferCount() const;
    size_t getFailedCount() const;
    void clear();
private:
    DmaTransferMonitor() = default;
    ~DmaTransferMonitor() = default;
    bool enabled_ = false;
    std::vector<TransferRecord> history_;
    size_t totalBytes_ = 0, failedCount_ = 0;
};
}''',
'''#include "nexus/utility/embedded/dma_transfer_monitor.h"
namespace nexus::utility::embedded {
DmaTransferMonitor& DmaTransferMonitor::instance() { static DmaTransferMonitor i; return i; }
void DmaTransferMonitor::initialize() { enabled_ = true; history_.clear(); totalBytes_ = 0; failedCount_ = 0; }
void DmaTransferMonitor::shutdown() { enabled_ = false; history_.clear(); }
bool DmaTransferMonitor::isEnabled() const { return enabled_; }
void DmaTransferMonitor::recordTransfer(const std::string& ch, uintptr_t src, uintptr_t dst, size_t sz, std::chrono::microseconds dur, bool ok) {
    if (!enabled_) return;
    history_.push_back({ch, src, dst, sz, dur, ok, std::chrono::system_clock::now()});
    totalBytes_ += sz; if (!ok) failedCount_++;
}
auto DmaTransferMonitor::getHistory() const -> std::vector<TransferRecord> { return history_; }
auto DmaTransferMonitor::getHistoryForChannel(const std::string& ch) const -> std::vector<TransferRecord> {
    std::vector<TransferRecord> r; for (auto& t : history_) if (t.channel == ch) r.push_back(t); return r;
}
size_t DmaTransferMonitor::getTotalBytes() const { return totalBytes_; }
size_t DmaTransferMonitor::getTransferCount() const { return history_.size(); }
size_t DmaTransferMonitor::getFailedCount() const { return failedCount_; }
void DmaTransferMonitor::clear() { history_.clear(); totalBytes_ = 0; failedCount_ = 0; }
}''')

# ===== AUDIO (5) =====
gen('audio', 'codec_error_handler', '''
#include <string>
#include <vector>
#include <functional>

namespace nexus::utility::audio {

class CodecErrorHandler {
public:
    struct CodecError {
        std::string codecName;
        int errorCode;
        std::string message;
        bool recoverable;
    };
    using ErrorCallback = std::function<void(const CodecError&)>;
    static CodecErrorHandler& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;
    void reportError(const std::string& codec, int code, const std::string& msg, bool recoverable = false);
    std::vector<CodecError> getErrors() const;
    std::vector<CodecError> getErrorsForCodec(const std::string& codec) const;
    size_t getErrorCount() const;
    bool hasUnrecoverableErrors() const;
    void onError(ErrorCallback cb);
    void clear();
private:
    CodecErrorHandler() = default;
    ~CodecErrorHandler() = default;
    bool enabled_ = false;
    std::vector<CodecError> errors_;
    std::vector<ErrorCallback> callbacks_;
};
}''',
'''#include "nexus/utility/audio/codec_error_handler.h"
namespace nexus::utility::audio {
CodecErrorHandler& CodecErrorHandler::instance() { static CodecErrorHandler i; return i; }
void CodecErrorHandler::initialize() { enabled_ = true; errors_.clear(); }
void CodecErrorHandler::shutdown() { enabled_ = false; errors_.clear(); }
bool CodecErrorHandler::isEnabled() const { return enabled_; }
void CodecErrorHandler::reportError(const std::string& c, int code, const std::string& msg, bool rec) {
    if (!enabled_) return;
    CodecError e{c, code, msg, rec}; errors_.push_back(e);
    for (auto& cb : callbacks_) if (cb) cb(e);
}
auto CodecErrorHandler::getErrors() const -> std::vector<CodecError> { return errors_; }
auto CodecErrorHandler::getErrorsForCodec(const std::string& c) const -> std::vector<CodecError> {
    std::vector<CodecError> r; for (auto& e : errors_) if (e.codecName == c) r.push_back(e); return r;
}
size_t CodecErrorHandler::getErrorCount() const { return errors_.size(); }
bool CodecErrorHandler::hasUnrecoverableErrors() const {
    for (auto& e : errors_) if (!e.recoverable) return true; return false;
}
void CodecErrorHandler::onError(ErrorCallback cb) { if (cb) callbacks_.push_back(cb); }
void CodecErrorHandler::clear() { errors_.clear(); }
}''')

gen('audio', 'audio_latency_tracker', '''
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>

namespace nexus::utility::audio {

class AudioLatencyTracker {
public:
    struct LatencySample {
        std::chrono::microseconds inputLatency{0}, outputLatency{0}, roundTripLatency{0};
        std::chrono::system_clock::time_point timestamp;
        size_t bufferSize;
        uint32_t sampleRate;
    };
    static AudioLatencyTracker& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;
    void recordLatency(std::chrono::microseconds input, std::chrono::microseconds output, size_t bufSize, uint32_t rate);
    LatencySample getLastSample() const;
    std::vector<LatencySample> getHistory() const;
    std::chrono::microseconds getAverageRoundTrip() const;
    std::chrono::microseconds getMaxRoundTrip() const;
    size_t getSampleCount() const;
    void clear();
private:
    AudioLatencyTracker() = default;
    ~AudioLatencyTracker() = default;
    bool enabled_ = false;
    std::vector<LatencySample> history_;
};
}''',
'''#include "nexus/utility/audio/audio_latency_tracker.h"
#include <algorithm>
namespace nexus::utility::audio {
AudioLatencyTracker& AudioLatencyTracker::instance() { static AudioLatencyTracker i; return i; }
void AudioLatencyTracker::initialize() { enabled_ = true; history_.clear(); }
void AudioLatencyTracker::shutdown() { enabled_ = false; history_.clear(); }
bool AudioLatencyTracker::isEnabled() const { return enabled_; }
void AudioLatencyTracker::recordLatency(std::chrono::microseconds in, std::chrono::microseconds out, size_t bs, uint32_t sr) {
    if (!enabled_) return;
    LatencySample s{in, out, in + out, std::chrono::system_clock::now(), bs, sr};
    history_.push_back(s);
}
auto AudioLatencyTracker::getLastSample() const -> LatencySample {
    return history_.empty() ? LatencySample{} : history_.back();
}
auto AudioLatencyTracker::getHistory() const -> std::vector<LatencySample> { return history_; }
std::chrono::microseconds AudioLatencyTracker::getAverageRoundTrip() const {
    if (history_.empty()) return std::chrono::microseconds(0);
    long long sum = 0;
    for (auto& s : history_) sum += s.roundTripLatency.count();
    return std::chrono::microseconds(sum / history_.size());
}
std::chrono::microseconds AudioLatencyTracker::getMaxRoundTrip() const {
    if (history_.empty()) return std::chrono::microseconds(0);
    auto it = std::max_element(history_.begin(), history_.end(),
        [](auto& a, auto& b){ return a.roundTripLatency < b.roundTripLatency; });
    return it->roundTripLatency;
}
size_t AudioLatencyTracker::getSampleCount() const { return history_.size(); }
void AudioLatencyTracker::clear() { history_.clear(); }
}''')

gen('audio', 'sample_rate_validator', '''
#include <string>
#include <vector>
#include <cstdint>

namespace nexus::utility::audio {

class SampleRateValidator {
public:
    struct RateInfo {
        uint32_t rate;
        std::string name;
        bool isStandard;
    };
    static SampleRateValidator& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;
    bool isValid(uint32_t rate) const;
    bool isStandardRate(uint32_t rate) const;
    std::string getRateName(uint32_t rate) const;
    uint32_t getNearestStandardRate(uint32_t rate) const;
    double getNyquistFrequency(uint32_t rate) const;
    std::vector<uint32_t> getSupportedRates() const;
    void addCustomRate(uint32_t rate, const std::string& name);
private:
    SampleRateValidator() = default;
    ~SampleRateValidator() = default;
    bool enabled_ = false;
    std::vector<RateInfo> standardRates_;
};
}''',
'''#include "nexus/utility/audio/sample_rate_validator.h"
#include <algorithm>
#include <cmath>
namespace nexus::utility::audio {
SampleRateValidator& SampleRateValidator::instance() { static SampleRateValidator i; return i; }
void SampleRateValidator::initialize() {
    enabled_ = true;
    standardRates_ = {
        {8000, "Telephone", true}, {11025, "Low Quality", true}, {16000, "VoIP", true},
        {22050, "AM Radio", true}, {32000, "FM Radio", true}, {44100, "CD Quality", true},
        {48000, "DVD/Video", true}, {88200, "2x CD", true}, {96000, "HD Audio", true},
        {176400, "4x CD", true}, {192000, "Studio", true}
    };
}
void SampleRateValidator::shutdown() { enabled_ = false; }
bool SampleRateValidator::isEnabled() const { return enabled_; }
bool SampleRateValidator::isValid(uint32_t rate) const { return rate >= 1000 && rate <= 1000000; }
bool SampleRateValidator::isStandardRate(uint32_t rate) const {
    for (auto& r : standardRates_) if (r.rate == rate) return true;
    return false;
}
std::string SampleRateValidator::getRateName(uint32_t rate) const {
    for (auto& r : standardRates_) if (r.rate == rate) return r.name;
    return "Custom";
}
uint32_t SampleRateValidator::getNearestStandardRate(uint32_t rate) const {
    if (standardRates_.empty()) return rate;
    auto best = standardRates_[0];
    for (auto& r : standardRates_) if (std::abs((int)r.rate - (int)rate) < std::abs((int)best.rate - (int)rate)) best = r;
    return best.rate;
}
double SampleRateValidator::getNyquistFrequency(uint32_t rate) const { return rate / 2.0; }
std::vector<uint32_t> SampleRateValidator::getSupportedRates() const {
    std::vector<uint32_t> r; for (auto& sr : standardRates_) r.push_back(sr.rate); return r;
}
void SampleRateValidator::addCustomRate(uint32_t rate, const std::string& name) {
    standardRates_.push_back({rate, name, false});
}
}''')

gen('audio', 'audio_callback_profiler', '''
#include <string>
#include <vector>
#include <chrono>

namespace nexus::utility::audio {

class AudioCallbackProfiler {
public:
    struct CallbackStats {
        std::string callbackName;
        std::chrono::microseconds avgDuration{0}, maxDuration{0}, minDuration{0};
        size_t callCount = 0, overrunCount = 0;
        double overrunPercentage = 0.0;
    };
    static AudioCallbackProfiler& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;
    void recordCallback(const std::string& name, std::chrono::microseconds duration, bool overrun);
    CallbackStats getStats(const std::string& name) const;
    std::vector<CallbackStats> getAllStats() const;
    bool hasOverruns() const;
    std::chrono::microseconds getTotalCallbackTime() const;
    void reset();
private:
    AudioCallbackProfiler() = default;
    ~AudioCallbackProfiler() = default;
    bool enabled_ = false;
    struct Internal { std::chrono::microseconds total{0}, max{0}, min{std::chrono::microseconds::max()}; size_t calls=0, overruns=0; };
    std::unordered_map<std::string, Internal> stats_;
};
}''',
'''#include "nexus/utility/audio/audio_callback_profiler.h"
#include <algorithm>
#include <unordered_map>
namespace nexus::utility::audio {
AudioCallbackProfiler& AudioCallbackProfiler::instance() { static AudioCallbackProfiler i; return i; }
void AudioCallbackProfiler::initialize() { enabled_ = true; stats_.clear(); }
void AudioCallbackProfiler::shutdown() { enabled_ = false; stats_.clear(); }
bool AudioCallbackProfiler::isEnabled() const { return enabled_; }
void AudioCallbackProfiler::recordCallback(const std::string& name, std::chrono::microseconds dur, bool overrun) {
    if (!enabled_) return;
    auto& s = stats_[name];
    s.total += dur; s.calls++;
    s.max = std::max(s.max, dur);
    if (s.calls == 1) s.min = dur; else s.min = std::min(s.min, dur);
    if (overrun) s.overruns++;
}
AudioCallbackProfiler::CallbackStats AudioCallbackProfiler::getStats(const std::string& name) const {
    auto it = stats_.find(name);
    if (it == stats_.end()) return {};
    CallbackStats cs; cs.callbackName = name;
    if (it->second.calls > 0) {
        cs.avgDuration = std::chrono::microseconds(it->second.total.count() / it->second.calls);
    }
    cs.maxDuration = it->second.max;
    cs.minDuration = (it->second.calls > 0) ? it->second.min : std::chrono::microseconds(0);
    cs.callCount = it->second.calls; cs.overrunCount = it->second.overruns;
    cs.overrunPercentage = cs.callCount > 0 ? (double)cs.overrunCount / cs.callCount * 100.0 : 0.0;
    return cs;
}
auto AudioCallbackProfiler::getAllStats() const -> std::vector<CallbackStats> {
    std::vector<CallbackStats> r;
    for (auto& [name, _] : stats_) r.push_back(getStats(name));
    return r;
}
bool AudioCallbackProfiler::hasOverruns() const {
    for (auto& [_, s] : stats_) if (s.overruns > 0) return true; return false;
}
std::chrono::microseconds AudioCallbackProfiler::getTotalCallbackTime() const {
    auto t = std::chrono::microseconds(0);
    for (auto& [_, s] : stats_) t += s.total;
    return t;
}
void AudioCallbackProfiler::reset() { stats_.clear(); }
}''')

gen('audio', 'video_frame_drop_detector', '''
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace nexus::utility::audio {

class VideoFrameDropDetector {
public:
    struct FrameInfo {
        uint64_t frameNumber;
        std::chrono::microseconds presentationTime{0};
        std::chrono::microseconds targetInterval{0};
        bool dropped;
        std::chrono::system_clock::time_point timestamp;
    };
    static VideoFrameDropDetector& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;
    void setTargetFPS(uint32_t fps);
    void recordFrame(uint64_t frameNum, std::chrono::microseconds presentTime);
    std::vector<FrameInfo> getHistory() const;
    size_t getDroppedCount() const;
    size_t getTotalFrames() const;
    double getDropPercentage() const;
    std::chrono::microseconds getAverageFrameTime() const;
    void clear();
private:
    VideoFrameDropDetector() = default;
    ~VideoFrameDropDetector() = default;
    bool enabled_ = false;
    uint32_t targetFPS_ = 60;
    uint64_t lastFrameNum_ = 0;
    std::vector<FrameInfo> history_;
    size_t droppedCount_ = 0;
    std::chrono::microseconds totalFrameTime_{0};
};
}''',
'''#include "nexus/utility/audio/video_frame_drop_detector.h"
#include <algorithm>
namespace nexus::utility::audio {
VideoFrameDropDetector& VideoFrameDropDetector::instance() { static VideoFrameDropDetector i; return i; }
void VideoFrameDropDetector::initialize() { enabled_ = true; clear(); }
void VideoFrameDropDetector::shutdown() { enabled_ = false; }
bool VideoFrameDropDetector::isEnabled() const { return enabled_; }
void VideoFrameDropDetector::setTargetFPS(uint32_t fps) { targetFPS_ = fps > 0 ? fps : 60; }
void VideoFrameDropDetector::recordFrame(uint64_t fn, std::chrono::microseconds pt) {
    if (!enabled_) return;
    std::chrono::microseconds targetInterval(1000000 / targetFPS_);
    bool dropped = (fn != lastFrameNum_ + 1) && lastFrameNum_ != 0;
    FrameInfo fi{fn, pt, targetInterval, dropped, std::chrono::system_clock::now()};
    history_.push_back(fi);
    if (dropped) droppedCount_++;
    totalFrameTime_ += pt;
    lastFrameNum_ = fn;
}
auto VideoFrameDropDetector::getHistory() const -> std::vector<FrameInfo> { return history_; }
size_t VideoFrameDropDetector::getDroppedCount() const { return droppedCount_; }
size_t VideoFrameDropDetector::getTotalFrames() const { return history_.size(); }
double VideoFrameDropDetector::getDropPercentage() const {
    return history_.empty() ? 0.0 : (double)droppedCount_ / history_.size() * 100.0;
}
std::chrono::microseconds VideoFrameDropDetector::getAverageFrameTime() const {
    return history_.empty() ? std::chrono::microseconds(0)
        : std::chrono::microseconds(totalFrameTime_.count() / history_.size());
}
void VideoFrameDropDetector::clear() { history_.clear(); droppedCount_ = 0; totalFrameTime_ = std::chrono::microseconds(0); lastFrameNum_ = 0; }
}''')

print("All stubs generated!")
