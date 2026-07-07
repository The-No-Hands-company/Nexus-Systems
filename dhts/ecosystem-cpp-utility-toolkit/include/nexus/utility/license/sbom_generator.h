#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstddef>

namespace nexus::utility::license {

/**
 * @brief Generate a Software Bill of Materials (SBOM) from tracked components.
 *
 * Each component records name, version, license and supplier. generateSBOM()
 * emits a JSON-like document listing every component, suitable for consumption
 * by downstream SBOM tooling.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category License
 * @version 1.0.0
 */
class SbomGenerator {
public:
    struct Component {
        std::string name;
        std::string version;
        std::string license;
        std::string supplier;
    };

    /// @brief Get singleton instance
    static SbomGenerator& instance() {
        static SbomGenerator inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        components_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        components_.clear();
    }

    /// @brief Check if the utility is currently enabled
    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Enable the utility
    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    /// @brief Disable the utility
    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    // ── Domain methods ──────────────────────────────────────────────────────

    /// @brief Add a component to the SBOM.
    void addComponent(const std::string& name, const std::string& version,
                      const std::string& license, const std::string& supplier) {
        std::lock_guard<std::mutex> lock(mutex_);
        components_.push_back({name, version, license, supplier});
    }

    /// @brief Generate the SBOM document as a JSON-like string.
    std::string generateSBOM() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string out = "{\n  \"bomFormat\": \"NexusSBOM\",\n";
        out += "  \"specVersion\": \"1.0\",\n";
        out += "  \"componentCount\": " + std::to_string(components_.size()) + ",\n";
        out += "  \"components\": [\n";
        for (std::size_t i = 0; i < components_.size(); ++i) {
            const auto& c = components_[i];
            out += "    {\n";
            out += "      \"name\": \"" + escape(c.name) + "\",\n";
            out += "      \"version\": \"" + escape(c.version) + "\",\n";
            out += "      \"license\": \"" + escape(c.license) + "\",\n";
            out += "      \"supplier\": \"" + escape(c.supplier) + "\"\n";
            out += "    }";
            if (i + 1 < components_.size()) out += ",";
            out += "\n";
        }
        out += "  ]\n}";
        return out;
    }

    /// @brief Number of components in the SBOM.
    std::size_t getComponentCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return components_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "SbomGenerator[components=" + std::to_string(components_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        components_.clear();
    }

private:
    SbomGenerator() = default;
    ~SbomGenerator() = default;

    SbomGenerator(const SbomGenerator&) = delete;
    SbomGenerator& operator=(const SbomGenerator&) = delete;
    SbomGenerator(SbomGenerator&&) = delete;
    SbomGenerator& operator=(SbomGenerator&&) = delete;

    static std::string escape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '"' || c == '\\') out += '\\';
            out += c;
        }
        return out;
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<Component> components_;
};

} // namespace nexus::utility::license
