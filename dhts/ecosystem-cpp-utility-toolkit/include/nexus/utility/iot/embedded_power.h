#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <map>
#include <cmath>

namespace nexus::utility::iot {

/// Power consumption estimator for embedded operations
class PowerConsumptionEstimator {
public:
    struct Component { std::string name; double current_ma_active; double current_ma_sleep; };
    struct DutyCycle { double active_pct; double sleep_pct; double duration_ms; };

    struct PowerEstimate {
        double avg_current_ma = 0;
        double peak_current_ma = 0;
        double energy_per_cycle_mj = 0;   // mJ per duty cycle
        double battery_life_hours = 0;     // estimated with given capacity
    };

    static PowerEstimate estimate(const std::vector<Component>& comps,
                                    const DutyCycle& duty,
                                    double battery_capacity_mah = 2000) {
        PowerEstimate pe;
        for (auto& c : comps) {
            pe.avg_current_ma += c.current_ma_active * duty.active_pct +
                                  c.current_ma_sleep * duty.sleep_pct;
            pe.peak_current_ma += c.current_ma_active;
        }
        pe.energy_per_cycle_mj = pe.avg_current_ma * 3.3 * duty.duration_ms / 1000.0;
        pe.battery_life_hours = pe.avg_current_ma > 0 ? battery_capacity_mah / pe.avg_current_ma : 0;
        return pe;
    }

    static std::string report(const PowerEstimate& pe) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Power Estimation ═══\n";
        oss << "  Avg Current:    " << pe.avg_current_ma << " mA\n";
        oss << "  Peak Current:   " << pe.peak_current_ma << " mA\n";
        oss << "  Energy/Cycle:   " << pe.energy_per_cycle_mj << " mJ\n";
        oss << "  Battery Life:   " << pe.battery_life_hours << " hours\n";
        return oss.str();
    }
};

/// ISR latency analyzer
class IsrLatencyAnalyzer {
public:
    struct IsrRecord { std::string name; double latency_us; size_t stack_used; bool nested; };

    struct LatencyAnalysis {
        double avg_latency_us = 0, max_latency_us = 0, min_latency_us = 1e9;
        size_t deadline_violations = 0;
        size_t stack_overflows = 0;
        size_t nested_isrs = 0;
    };

    static LatencyAnalysis analyze(const std::vector<IsrRecord>& records, double deadline_us = 100) {
        LatencyAnalysis la;
        if (records.empty()) return la;
        double sum = 0;
        for (auto& r : records) {
            sum += r.latency_us;
            la.max_latency_us = std::max(la.max_latency_us, r.latency_us);
            la.min_latency_us = std::min(la.min_latency_us, r.latency_us);
            if (r.latency_us > deadline_us) la.deadline_violations++;
            if (r.stack_used > 2048) la.stack_overflows++;
            if (r.nested) la.nested_isrs++;
        }
        la.avg_latency_us = sum / records.size();
        return la;
    }

    static std::string report(const LatencyAnalysis& la) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(1);
        oss << "═══ ISR Latency Analysis ═══\n";
        oss << "  Avg/Max/Min:  " << la.avg_latency_us << " / " << la.max_latency_us << " / " << la.min_latency_us << " us\n";
        oss << "  Violations:   " << la.deadline_violations << (la.deadline_violations > 0 ? " ⚠" : " ✓") << "\n";
        oss << "  Stack Ovrflw: " << la.stack_overflows << (la.stack_overflows > 0 ? " ⚠" : " ✓") << "\n";
        return oss.str();
    }
};

/// Watchdog timer config validator
class WatchdogValidator {
public:
    struct WatchdogConfig { uint32_t timeout_ms; bool window_mode; uint32_t window_start_ms; bool enabled; };

    struct Validation { bool valid = true; std::vector<std::string> warnings;
        static Validation ok() { return {}; }
        static Validation warn(std::string s) { Validation v; v.warnings.push_back(s); return v; }
    };

    static Validation validate(const WatchdogConfig& cfg, uint32_t max_task_time_ms) {
        Validation v;
        if (!cfg.enabled) v.warnings.push_back("Watchdog disabled ⚠");
        if (cfg.timeout_ms < max_task_time_ms) v.warnings.push_back("Timeout < max task time ⚠");
        if (cfg.timeout_ms < 10) v.warnings.push_back("Timeout too short (<10ms)");
        if (cfg.timeout_ms > 60000) v.warnings.push_back("Timeout excessive (>60s)");
        v.valid = v.warnings.empty();
        return v;
    }

    static std::string report(const Validation& v) {
        std::ostringstream oss;
        oss << "═══ Watchdog Validation ═══\n  Valid: " << (v.valid ? "✓" : "⚠") << "\n";
        for (auto& w : v.warnings) oss << "  ⚠ " << w << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::iot
