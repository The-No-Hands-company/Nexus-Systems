#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <mutex>
namespace nexus::utility::network {
class NetworkPartitionSimulator {
public:
    struct Partition { std::string name; std::unordered_set<std::string> nodes; };
    using CommunicationFn = std::function<bool(const std::string& from, const std::string& to)>;
    static NetworkPartitionSimulator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void createPartition(const std::string& name, const std::vector<std::string>& nodes);
    void removePartition(const std::string& name);
    bool canCommunicate(const std::string& nodeA, const std::string& nodeB) const;
    bool isPartitioned(const std::string& node) const;
    std::vector<std::string> getPartitionMembers(const std::string& name) const;
    std::vector<Partition> getActivePartitions() const;
    size_t partitionCount() const;
    void clear();
private:
    NetworkPartitionSimulator() = default; ~NetworkPartitionSimulator() = default; bool enabled_ = false;
    std::unordered_map<std::string, Partition> partitions_;
    std::unordered_map<std::string, std::string> nodeToPartition_;
    mutable std::mutex mutex_;
};
} // namespace nexus::utility::network