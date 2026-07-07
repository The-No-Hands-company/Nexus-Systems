#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>

namespace nexus::utility::profiling {

/**
 * @brief Generates flame graph data from stack samples.
 */
class FlameGraphGenerator {
public:
    struct StackSample {
        std::vector<std::string> frames;  // Bottom to top
        size_t count = 1;
    };

    /**
     * @brief Adds a stack sample.
     */
    static void addSample(const std::vector<std::string>& frames) {
        std::lock_guard lock(mutex_);
        
        // Create stack key
        std::string key;
        for (const auto& frame : frames) {
            if (!key.empty()) key += ";";
            key += frame;
        }
        
        samples_[key]++;
    }

    /**
     * @brief Generates flame graph data in folded format.
     */
    static std::string generateFoldedStacks() {
        std::lock_guard lock(mutex_);
        std::ostringstream output;
        
        for (const auto& [stack, count] : samples_) {
            output << stack << " " << count << "\n";
        }
        
        return output.str();
    }

    /**
     * @brief Exports to file.
     */
    static bool exportToFile(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        file << generateFoldedStacks();
        return true;
    }

    /**
     * @brief Generates SVG flame graph (simplified).
     */
    static std::string generateSVG(size_t width = 1200, size_t height = 800) {
        std::ostringstream svg;
        
        svg << "<?xml version=\"1.0\" standalone=\"no\"?>\n";
        svg << "<svg width=\"" << width << "\" height=\"" << height << "\" "
            << "xmlns=\"http://www.w3.org/2000/svg\">\n";
        svg << "<text x=\"10\" y=\"30\" font-size=\"20\">Flame Graph</text>\n";
        
        // Simplified visualization - full implementation would render actual flame graph
        std::lock_guard lock(mutex_);
        size_t y = 60;
        for (const auto& [stack, count] : samples_) {
            svg << "<rect x=\"10\" y=\"" << y << "\" width=\"" 
                << (count * 10) << "\" height=\"20\" fill=\"orange\"/>\n";
            svg << "<text x=\"15\" y=\"" << (y + 15) << "\" font-size=\"12\">"
                << stack << " (" << count << ")</text>\n";
            y += 25;
        }
        
        svg << "</svg>\n";
        return svg.str();
    }

    /**
     * @brief Resets all samples.
     */
    static void reset() {
        std::lock_guard lock(mutex_);
        samples_.clear();
    }

    /**
     * @brief Gets total sample count.
     */
    static size_t getTotalSamples() {
        std::lock_guard lock(mutex_);
        size_t total = 0;
        for (const auto& [_, count] : samples_) {
            total += count;
        }
        return total;
    }

private:
    static inline std::mutex mutex_;
    static inline std::unordered_map<std::string, size_t> samples_;
};

} // namespace nexus::utility::profiling
