#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <functional>
#include <algorithm>
#include <climits>
#include <numeric>

namespace nexus::utility::distributed {

/// Raft/Paxos consensus protocol debugger
class ConsensusDebugger {
public:
    enum class Role { Follower, Candidate, Leader };
    enum class State { Active, Crashed, Partitioned };

    struct Node {
        int id;
        Role role = Role::Follower;
        int term = 0;
        int voted_for = -1;
        int commit_index = 0;
        int last_applied = 0;
        State state = State::Active;
        std::chrono::steady_clock::time_point last_heartbeat;
    };

    struct ClusterState {
        std::vector<Node> nodes;
        int current_term = 0;
        int leader_id = -1;
        size_t quorum_size = 0;
        bool has_leader = false;
        bool split_brain = false;
        int max_term_disparity = 0;
        std::vector<int> partitioned_nodes;
    };

    static ClusterState analyze(const std::vector<Node>& nodes) {
        ClusterState cs;
        cs.nodes = nodes;
        cs.quorum_size = nodes.size() / 2 + 1;

        // Find max term
        for (auto& n : nodes) cs.current_term = std::max(cs.current_term, n.term);

        // Count leaders
        int leader_count = 0;
        for (auto& n : nodes) {
            if (n.role == Role::Leader) { leader_count++; cs.leader_id = n.id; }
        }

        cs.has_leader = (leader_count == 1);
        cs.split_brain = (leader_count > 1);
        cs.max_term_disparity = maxTermDisparity(nodes);

        // Partition detection
        auto now = std::chrono::steady_clock::now();
        for (auto& n : nodes) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - n.last_heartbeat);
            if (elapsed > std::chrono::milliseconds(500)) cs.partitioned_nodes.push_back(n.id);
        }

        return cs;
    }

    static std::string report(const ClusterState& cs) {
        std::ostringstream oss;
        oss << "═══ Consensus Debug Report ═══\n";
        oss << "  Nodes:        " << cs.nodes.size() << "\n";
        oss << "  Quorum:       " << cs.quorum_size << "\n";
        oss << "  Term:         " << cs.current_term << "\n";
        oss << "  Has Leader:   " << (cs.has_leader ? "✓" : "⚠ NO") << "\n";
        oss << "  Split Brain:  " << (cs.split_brain ? "⚠ DETECTED" : "✓ none") << "\n";
        oss << "  Term Disparity:" << cs.max_term_disparity << "\n";
        oss << "  Partitioned:  " << cs.partitioned_nodes.size()
            << (cs.partitioned_nodes.empty() ? " ✓" : " ⚠") << "\n";
        return oss.str();
    }

private:
    static int maxTermDisparity(const std::vector<Node>& nodes) {
        int min_t=INT_MAX, max_t=INT_MIN;
        for (auto& n : nodes) { min_t=std::min(min_t,n.term); max_t=std::max(max_t,n.term); }
        return max_t - min_t;
    }
};

/// Leader election monitor
class LeaderElectionMonitor {
public:
    struct ElectionStats {
        int election_count = 0;
        double avg_election_time_ms = 0;
        double max_election_time_ms = 0;
        int term_changes = 0;
        double leader_stability = 0; // fraction of time with stable leader
        std::vector<double> election_durations;
    };

    static ElectionStats analyze(const std::vector<double>& election_times_ms,
                                   double total_time_ms) {
        ElectionStats es;
        es.election_count = static_cast<int>(election_times_ms.size());
        es.term_changes = es.election_count;
        if (!election_times_ms.empty()) {
            es.avg_election_time_ms = std::accumulate(election_times_ms.begin(), election_times_ms.end(), 0.0) / es.election_count;
            es.max_election_time_ms = *std::max_element(election_times_ms.begin(), election_times_ms.end());
        }
        double election_overhead = std::accumulate(election_times_ms.begin(), election_times_ms.end(), 0.0);
        es.leader_stability = total_time_ms > 0 ? 1.0 - election_overhead / total_time_ms : 1;
        es.election_durations = election_times_ms;
        return es;
    }

    static std::string report(const ElectionStats& es) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Leader Election Monitor ═══\n";
        oss << "  Elections:       " << es.election_count << "\n";
        oss << "  Avg Duration:    " << es.avg_election_time_ms << "ms\n";
        oss << "  Max Duration:    " << es.max_election_time_ms << "ms\n";
        oss << "  Stability:       " << (es.leader_stability*100) << "%\n";
        return oss.str();
    }
};

/// Distributed lock validator
class DistributedLockValidator {
public:
    struct LockEvent { std::string lock_name; int owner_id; bool acquired; std::chrono::milliseconds timestamp; };

    struct LockAnalysis {
        size_t deadlocks = 0;
        size_t contention_events = 0;
        size_t stale_locks = 0;
        double avg_hold_time_ms = 0;
        std::vector<std::string> held_too_long;
    };

    static LockAnalysis analyze(const std::vector<LockEvent>& events, std::chrono::milliseconds max_hold = std::chrono::milliseconds(5000)) {
        LockAnalysis la;
        std::map<std::string, std::vector<LockEvent>> by_lock;
        for (auto& e : events) by_lock[e.lock_name].push_back(e);

        for (auto& [name, evts] : by_lock) {
            int depth = 0;
            for (auto& e : evts) {
                if (e.acquired) {
                    depth++;
                    if (depth > 1) la.contention_events++;
                } else depth--;
            }
            // Check hold times
            for (size_t i = 0; i + 1 < evts.size(); i += 2) {
                if (evts[i].acquired && !evts[i+1].acquired) {
                    auto hold = evts[i+1].timestamp - evts[i].timestamp;
                    la.avg_hold_time_ms += hold.count();
                    if (hold > max_hold) la.held_too_long.push_back(name);
                }
            }
        }
        if (!events.empty()) la.avg_hold_time_ms /= (events.size() / 2);
        return la;
    }

    static std::string report(const LockAnalysis& la) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(1);
        oss << "═══ Distributed Lock Validation ═══\n";
        oss << "  Contention:  " << la.contention_events << (la.contention_events > 0 ? " ⚠" : " ✓") << "\n";
        oss << "  Avg Hold:    " << la.avg_hold_time_ms << "ms\n";
        oss << "  Stale Locks: " << la.stale_locks << (la.stale_locks > 0 ? " ⚠" : " ✓") << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::distributed
