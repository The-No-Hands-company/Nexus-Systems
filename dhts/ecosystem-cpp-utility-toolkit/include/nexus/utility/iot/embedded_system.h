#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <functional>
#include <cmath>

namespace nexus::utility::iot {

/// RTOS task monitor and analyzer
class RtosTaskMonitor {
public:
    struct TaskInfo { std::string name; int priority; uint32_t stack_high_water; double cpu_usage_pct; uint32_t runtime_ms; bool is_running; };

    struct TaskAnalysis {
        size_t total_tasks = 0;
        int max_priority = 0;
        double total_cpu = 0;
        size_t blocked_tasks = 0;
        std::vector<std::string> high_stack_usage;
        std::string highest_cpu_task;
        bool has_deadlock_risk = false;
    };

    static TaskAnalysis analyze(const std::vector<TaskInfo>& tasks) {
        TaskAnalysis ta;
        ta.total_tasks = tasks.size();
        double max_cpu = 0;

        for (auto& t : tasks) {
            ta.max_priority = std::max(ta.max_priority, t.priority);
            ta.total_cpu += t.cpu_usage_pct;
            if (!t.is_running) ta.blocked_tasks++;
            if (t.stack_high_water < 512) ta.high_stack_usage.push_back(t.name);
            if (t.cpu_usage_pct > max_cpu) { max_cpu = t.cpu_usage_pct; ta.highest_cpu_task = t.name; }
        }
        ta.has_deadlock_risk = ta.blocked_tasks > ta.total_tasks / 2;
        return ta;
    }

    static std::string report(const TaskAnalysis& ta) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(1);
        oss << "═══ RTOS Task Monitor ═══\n";
        oss << "  Tasks:         " << ta.total_tasks << "\n";
        oss << "  Max Priority:  " << ta.max_priority << "\n";
        oss << "  Total CPU:     " << ta.total_cpu << "%\n";
        oss << "  Blocked:       " << ta.blocked_tasks << (ta.has_deadlock_risk ? " ⚠ risk" : " ✓") << "\n";
        oss << "  Top CPU:       " << ta.highest_cpu_task << "\n";
        for (auto& s : ta.high_stack_usage) oss << "  Stack warn:    " << s << " ⚠\n";
        return oss.str();
    }
};

/// Memory map validator for embedded systems
class MemoryMapValidator {
public:
    struct Region { std::string name; uint32_t start; uint32_t size; std::string type; }; // flash/ram/peripheral

    struct MemoryAnalysis {
        uint32_t total_flash = 0, total_ram = 0;
        uint32_t used_flash = 0, used_ram = 0;
        bool has_overlap = false;
        bool has_gaps = false;
        std::vector<std::string> overlaps;
    };

    static MemoryAnalysis analyze(const std::vector<Region>& regions) {
        MemoryAnalysis ma;
        std::vector<Region> sorted = regions;
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b){ return a.start < b.start; });

        for (size_t i = 1; i < sorted.size(); ++i) {
            uint32_t prev_end = sorted[i-1].start + sorted[i-1].size;
            if (sorted[i].start < prev_end) {
                ma.has_overlap = true;
                ma.overlaps.push_back(sorted[i-1].name + "↔" + sorted[i].name);
            }
            if (sorted[i].start > prev_end) ma.has_gaps = true;
        }

        for (auto& r : regions) {
            if (r.type == "flash") { ma.total_flash += r.size; ma.used_flash += r.size/2; }
            if (r.type == "ram") { ma.total_ram += r.size; ma.used_ram += r.size/3; }
        }
        return ma;
    }

    static std::string report(const MemoryAnalysis& ma) {
        std::ostringstream oss;
        oss << "═══ Memory Map Validation ═══\n";
        oss << "  Flash: " << ma.used_flash << "/" << ma.total_flash << " bytes\n";
        oss << "  RAM:   " << ma.used_ram << "/" << ma.total_ram << " bytes\n";
        oss << "  Overlap: " << (ma.has_overlap ? "⚠ YES" : "✓ none") << "\n";
        oss << "  Gaps:    " << (ma.has_gaps ? "⚠ YES" : "✓ none") << "\n";
        return oss.str();
    }
};

/// Bootloader integrity checker
class BootloaderIntegrityChecker {
public:
    struct FirmwareImage { std::vector<uint8_t> data; uint32_t expected_crc; uint32_t version; std::string signature; };

    struct IntegrityResult {
        bool crc_valid = false;
        bool signature_valid = false;
        bool version_valid = false;
        uint32_t computed_crc = 0;
    };

    static IntegrityResult check(const FirmwareImage& fw, uint32_t min_version = 0) {
        IntegrityResult ir;
        ir.computed_crc = crc32(fw.data.data(), fw.data.size());
        ir.crc_valid = (ir.computed_crc == fw.expected_crc);
        ir.version_valid = (fw.version >= min_version);
        ir.signature_valid = !fw.signature.empty();
        return ir;
    }

    static std::string report(const IntegrityResult& ir) {
        std::ostringstream oss;
        oss << "═══ Bootloader Integrity ═══\n";
        oss << "  CRC:       " << (ir.crc_valid ? "✓ valid" : "⚠ MISMATCH") << "\n";
        oss << "  Signature: " << (ir.signature_valid ? "✓ present" : "⚠ missing") << "\n";
        oss << "  Version:   " << (ir.version_valid ? "✓ ok" : "⚠ too old") << "\n";
        return oss.str();
    }

    static uint32_t crc32(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
        return ~crc;
    }
};

/// Sensor calibration validator
class SensorCalibrationValidator {
public:
    struct CalibrationPoint { double reference; double measured; };

    struct CalibrationResult {
        double offset = 0, gain = 1.0;
        double linearity_error = 0;  // max deviation from linear fit
        double r_squared = 0;
        bool needs_recalibration = false;
    };

    static CalibrationResult analyze(const std::vector<CalibrationPoint>& points) {
        CalibrationResult cr;
        if (points.size() < 2) { cr.needs_recalibration = true; return cr; }

        double sx=0,sy=0,sxy=0,sxx=0,syy=0; auto n=points.size();
        for (auto& p : points) {
            sx+=p.reference; sy+=p.measured;
            sxy+=p.reference*p.measured; sxx+=p.reference*p.reference; syy+=p.measured*p.measured;
        }
        double denom = n*sxx - sx*sx;
        cr.gain = (std::abs(denom) > 1e-10) ? (n*sxy - sx*sy)/denom : 1;
        cr.offset = (sy - cr.gain*sx)/n;
        double r_num = n*sxy - sx*sy;
        double r_den = std::sqrt((n*sxx-sx*sx)*(n*syy-sy*sy));
        cr.r_squared = (r_den > 0) ? (r_num*r_num)/(r_den*r_den) : 0;

        double max_err = 0;
        for (auto& p : points) max_err = std::max(max_err, std::abs(p.measured - (cr.gain*p.reference+cr.offset)));
        cr.linearity_error = max_err;
        cr.needs_recalibration = cr.r_squared < 0.99 || cr.linearity_error > 0.05;
        return cr;
    }

    static std::string report(const CalibrationResult& cr) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(4);
        oss << "═══ Sensor Calibration ═══\n";
        oss << "  Offset/Gain:  " << cr.offset << " / " << cr.gain << "\n";
        oss << "  R²:           " << cr.r_squared << "\n";
        oss << "  Linearity Err: " << cr.linearity_error << "\n";
        oss << "  Needs Recal:  " << (cr.needs_recalibration ? "⚠ YES" : "✓ no") << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::iot
