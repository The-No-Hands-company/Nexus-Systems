#pragma once

#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <algorithm>

namespace nexus::utility::concurrency {

/// @brief Detects deadlock cycles in wait-for graphs via DFS.
///
/// Builds a wait-for graph from lock tracking data and runs depth-first
/// search to detect cycles indicating potential deadlocks.
class DeadlockDetector {
public:
    /// @brief Check for deadlock cycle in wait-for graph.
    /// @param waiting_map ThreadID → MutexAddr (what each thread is waiting for)
    /// @param lock_owners  MutexAddr → ThreadID (who owns each lock)
    /// @return A ThreadID that is part of a cycle, or std::nullopt if no cycle found
    [[nodiscard]] static std::optional<std::thread::id> checkCycle(
        const std::unordered_map<std::thread::id, const void*>& waiting_map,
        const std::unordered_map<const void*, std::thread::id>& lock_owners);

    /// @brief Check if a specific thread is involved in any deadlock cycle.
    [[nodiscard]] static bool isThreadInCycle(
        std::thread::id tid,
        const std::unordered_map<std::thread::id, const void*>& waiting_map,
        const std::unordered_map<const void*, std::thread::id>& lock_owners) {
        // Build wait-for graph
        std::unordered_map<std::thread::id, std::thread::id> graph;
        for (const auto& [t, m] : waiting_map) {
            auto it = lock_owners.find(m);
            if (it != lock_owners.end()) graph[t] = it->second;
        }
        // DFS from tid to see if it can reach itself
        std::unordered_set<std::thread::id> visited;
        std::thread::id current = tid;
        while (true) {
            if (!visited.insert(current).second) return true; // Cycle found
            auto it = graph.find(current);
            if (it == graph.end()) return false;
            current = it->second;
        }
    }

    /// @brief Build a wait-for graph from lock tracking data.
    /// Returns map of ThreadID → ThreadID representing "T1 waits for T2".
    [[nodiscard]] static std::unordered_map<std::thread::id, std::thread::id> buildWaitForGraph(
        const std::unordered_map<std::thread::id, const void*>& waiting_map,
        const std::unordered_map<const void*, std::thread::id>& lock_owners) {
        std::unordered_map<std::thread::id, std::thread::id> graph;
        for (const auto& [tid, mutex] : waiting_map) {
            auto it = lock_owners.find(mutex);
            if (it != lock_owners.end()) graph[tid] = it->second;
        }
        return graph;
    }

    /// @brief Count the number of threads currently waiting for locks.
    [[nodiscard]] static size_t waitingThreadCount(
        const std::unordered_map<std::thread::id, const void*>& waiting_map) noexcept {
        return waiting_map.size();
    }

    /// @brief Check if a given mutex is involved in any wait relationship.
    [[nodiscard]] static bool isMutexContested(
        const void* mutex,
        const std::unordered_map<std::thread::id, const void*>& waiting_map,
        const std::unordered_map<const void*, std::thread::id>& lock_owners) {
        for (const auto& [tid, m] : waiting_map) {
            if (m == mutex) return true;
        }
        return lock_owners.find(mutex) != lock_owners.end();
    }
};

} // namespace nexus::utility::concurrency
