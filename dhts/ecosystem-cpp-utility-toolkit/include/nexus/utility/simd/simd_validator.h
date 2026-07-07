#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <ostream>
#include <unordered_map>

namespace nexus::utility::simd {

/// @brief Validate SIMD operations
class SIMDValidator {
public:
    /// Check if data is properly aligned for SIMD
    template<size_t Alignment>
    static bool isAligned(const void* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) % Alignment == 0;
    }
    
    static bool isAligned16(const void* ptr) { return isAligned<16>(ptr); }
    static bool isAligned32(const void* ptr) { return isAligned<32>(ptr); }
    static bool isAligned64(const void* ptr) { return isAligned<64>(ptr); }
    
    /// Validate SIMD operation results
    static bool validateFloatArray(const float* data, size_t size);
    static bool validateDoubleArray(const double* data, size_t size);
};

/// @brief Detect CPU SIMD features
class CpuFeatureDetectorImpl {
public:
    struct Features {
        bool sse = false;
        bool sse2 = false;
        bool sse3 = false;
        bool ssse3 = false;
        bool sse4_1 = false;
        bool sse4_2 = false;
        bool avx = false;
        bool avx2 = false;
        bool avx512f = false;
        bool fma = false;
        bool neon = false;  // ARM
    };
    
    static Features detect();
    static void printFeatures(std::ostream& os);
    
    static bool hasSSE();
    static bool hasAVX();
    static bool hasAVX2();
    static bool hasAVX512();
    static bool hasNEON();

private:
    static Features cached_features_;
    static bool features_detected_;
};

/// @brief Check if loops were auto-vectorized
class VectorizationChecker {
public:
    /// Record whether a loop was vectorized
    void recordLoop(const std::string& function_name,
                   int line_number,
                   bool vectorized,
                   const std::string& reason = "");
    
    struct LoopInfo {
        std::string function_name;
        int line_number;
        bool vectorized;
        std::string reason;
    };
    
    const std::vector<LoopInfo>& getLoops() const { return loops_; }
    
    void printReport(std::ostream& os) const;
    
    size_t getVectorizedCount() const;
    size_t getTotalCount() const { return loops_.size(); }

private:
    std::vector<LoopInfo> loops_;
};

/// @brief Profile vectorized code performance
class VecProfilerImpl {
public:
    void recordScalarTime(const std::string& operation,
                         std::chrono::nanoseconds time);
    
    void recordVectorTime(const std::string& operation,
                         std::chrono::nanoseconds time);
    
    struct SpeedupStats {
        std::string operation;
        std::chrono::nanoseconds scalar_time;
        std::chrono::nanoseconds vector_time;
        double speedup;
    };
    
    std::vector<SpeedupStats> getSpeedups() const;
    void printReport(std::ostream& os) const;

private:
    std::unordered_map<std::string, std::chrono::nanoseconds> scalar_times_;
    std::unordered_map<std::string, std::chrono::nanoseconds> vector_times_;
};

/// @brief Alignment optimization helper
class AlignOptimizerImpl {
public:
    /// Suggest optimal alignment for data structure
    static size_t suggestAlignment(size_t element_size, size_t array_size);
    
    /// Check struct padding
    template<typename T>
    static size_t checkPadding() {
        return sizeof(T) - getActualDataSize<T>();
    }
    
    /// Print alignment recommendations
    static void printRecommendations(std::ostream& os);

private:
    template<typename T>
    static size_t getActualDataSize();
};

} // namespace nexus::utility::simd
