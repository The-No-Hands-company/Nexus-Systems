#pragma once

#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iomanip>
#include <chrono>
#include <set>

namespace nexus::utility::distributed {

/// Shard balance analyzer for distributed data stores
class ShardBalanceAnalyzer {
public:
    struct ShardInfo { int id; size_t size_bytes; size_t key_count; double load_factor; int node_id; };

    struct BalanceAnalysis {
        size_t total_shards = 0;
        double imbalance_ratio = 0;       // max_load / min_load
        double gini_coefficient = 0;
        size_t overloaded_shards = 0;
        size_t underloaded_shards = 0;
        std::vector<int> hotspots;
    };

    static BalanceAnalysis analyze(const std::vector<ShardInfo>& shards, double overload_factor = 1.5) {
        BalanceAnalysis ba;
        ba.total_shards = shards.size();
        if (shards.empty()) return ba;

        std::vector<double> loads;
        for (auto& s : shards) loads.push_back(s.key_count > 0 ? s.key_count : s.size_bytes);

        double max_l = *std::max_element(loads.begin(), loads.end());
        double min_l = *std::min_element(loads.begin(), loads.end());
        double avg = std::accumulate(loads.begin(), loads.end(), 0.0) / loads.size();

        ba.imbalance_ratio = min_l > 0 ? max_l / min_l : 1;
        ba.gini_coefficient = computeGini(loads);

        for (size_t i = 0; i < shards.size(); ++i) {
            if (loads[i] > avg * overload_factor) { ba.overloaded_shards++; ba.hotspots.push_back(shards[i].id); }
            if (loads[i] < avg / overload_factor) ba.underloaded_shards++;
        }

        return ba;
    }

    static std::string report(const BalanceAnalysis& ba) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(3);
        oss << "═══ Shard Balance Analysis ═══\n";
        oss << "  Shards:          " << ba.total_shards << "\n";
        oss << "  Imbalance Ratio: " << ba.imbalance_ratio
            << (ba.imbalance_ratio > 2.0 ? " ⚠" : " ✓") << "\n";
        oss << "  Gini Coeff:      " << ba.gini_coefficient
            << (ba.gini_coefficient > 0.4 ? " ⚠ uneven" : " ✓") << "\n";
        oss << "  Overloaded:      " << ba.overloaded_shards
            << (ba.overloaded_shards > 0 ? " ⚠" : " ✓") << "\n";
        return oss.str();
    }

private:
    static double computeGini(const std::vector<double>& data) {
        auto sorted = data; std::sort(sorted.begin(), sorted.end());
        double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
        double cum = 0, gini = 0; int n = sorted.size();
        for (int i = 0; i < n; ++i) { cum += sorted[i]; gini += (i+1)*sorted[i]; }
        return sum > 0 ? (2.0*gini)/(n*sum) - (n+1.0)/n : 0;
    }
};

/// Vector clock debugger for causality tracking
class VectorClockDebugger {
public:
    using Clock = std::map<int, int>;

    struct CausalityResult {
        bool concurrent = false;
        bool happens_before = false;
        bool happens_after = false;
        bool equal = false;
    };

    static CausalityResult compare(const Clock& a, const Clock& b) {
        CausalityResult cr;
        bool a_leq_b = true, b_leq_a = true, all_eq = true;
        std::set<int> all_ids;
        for (auto& [id,_] : a) all_ids.insert(id);
        for (auto& [id,_] : b) all_ids.insert(id);

        for (int id : all_ids) {
            int va = a.count(id) ? a.at(id) : 0;
            int vb = b.count(id) ? b.at(id) : 0;
            if (va > vb) a_leq_b = false;
            if (vb > va) b_leq_a = false;
            if (va != vb) all_eq = false;
        }

        cr.equal = all_eq;
        cr.happens_before = a_leq_b && !all_eq;
        cr.happens_after = b_leq_a && !all_eq;
        cr.concurrent = !a_leq_b && !b_leq_a;
        return cr;
    }

    static std::string clockToString(const Clock& c) {
        std::ostringstream oss; oss << "[";
        bool first = true;
        for (auto& [id, v] : c) { if (!first) oss << ","; first=false; oss << id<<":"<<v; }
        oss << "]";
        return oss.str();
    }
};

/// Gossip protocol tracer
class GossipProtocolTracer {
public:
    struct GossipEvent { int from, to; std::string key; int version; std::chrono::milliseconds time; };

    struct GossipAnalysis {
        double convergence_time_ms = 0;
        int total_messages = 0;
        int redundant_messages = 0;
        double message_efficiency = 0;
        std::vector<int> slow_nodes;
    };

    static GossipAnalysis analyze(const std::vector<GossipEvent>& events, int node_count) {
        GossipAnalysis ga;
        ga.total_messages = static_cast<int>(events.size());

        std::map<std::pair<int,int>, int> counts;
        for (auto& e : events) counts[{e.from, e.to}]++;
        for (auto& [_,c] : counts) if (c > 1) ga.redundant_messages += (c-1);

        ga.message_efficiency = ga.total_messages > 0 ? 1.0 - static_cast<double>(ga.redundant_messages)/ga.total_messages : 1;

        // Convergence: time until all nodes have the update
        if (!events.empty()) {
            std::set<int> informed;
            for (auto& e : events) {
                informed.insert(e.to);
                if (informed.size() >= static_cast<size_t>(node_count)) break;
            }
        }

        return ga;
    }

    static std::string report(const GossipAnalysis& ga) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(3);
        oss << "═══ Gossip Protocol Trace ═══\n";
        oss << "  Messages:      " << ga.total_messages << "\n";
        oss << "  Redundant:     " << ga.redundant_messages
            << (ga.redundant_messages > ga.total_messages/2 ? " ⚠ high" : " ✓") << "\n";
        oss << "  Efficiency:    " << (ga.message_efficiency*100) << "%\n";
        return oss.str();
    }
};

} // namespace nexus::utility::distributed
