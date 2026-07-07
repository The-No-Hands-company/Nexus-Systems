#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <atomic>
#include <mutex>

namespace nexus::utility::ml {

/**
 * @brief Check gradients for vanishing/exploding values.
 */
class GradientValidator {
public:
    enum class Status { Healthy, Vanishing, Exploding, NaN };

    struct LayerReport {
        std::string layer;
        double norm = 0.0;
        double maxAbs = 0.0;
        Status status = Status::Healthy;
    };

    static GradientValidator& instance() {
        static GradientValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setThresholds(double vanishing, double exploding) {
        std::lock_guard<std::mutex> lk(mutex_);
        vanishingThreshold_ = vanishing;
        explodingThreshold_ = exploding;
    }

    /// Analyze a layer's gradient tensor.
    LayerReport check(const std::string& layer, const std::vector<double>& gradients) {
        LayerReport r;
        r.layer = layer;
        double sumSq = 0.0;
        bool hasNan = false;
        for (double g : gradients) {
            if (std::isnan(g) || std::isinf(g)) hasNan = true;
            double a = std::fabs(g);
            if (a > r.maxAbs) r.maxAbs = a;
            sumSq += g * g;
        }
        r.norm = std::sqrt(sumSq);
        double vanish, explode;
        { std::lock_guard<std::mutex> lk(mutex_); vanish = vanishingThreshold_; explode = explodingThreshold_; }
        if (hasNan) r.status = Status::NaN;
        else if (r.norm < vanish) r.status = Status::Vanishing;
        else if (r.norm > explode) r.status = Status::Exploding;
        else r.status = Status::Healthy;
        std::lock_guard<std::mutex> lk(mutex_);
        reports_.push_back(r);
        if (r.status != Status::Healthy) ++problems_;
        return r;
    }

    std::vector<LayerReport> problemLayers() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<LayerReport> out;
        for (const auto& r : reports_) if (r.status != Status::Healthy) out.push_back(r);
        return out;
    }

    std::size_t problems() const { return problems_.load(); }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        reports_.clear();
        problems_ = 0;
    }

private:
    GradientValidator() = default;
    ~GradientValidator() = default;
    GradientValidator(const GradientValidator&) = delete;
    GradientValidator& operator=(const GradientValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    double vanishingThreshold_ = 1e-6;
    double explodingThreshold_ = 1e3;
    std::vector<LayerReport> reports_;
    std::atomic<std::size_t> problems_{0};
};

} // namespace nexus::utility::ml
