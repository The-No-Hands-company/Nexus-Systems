#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <map>
#include <algorithm>
#include <mutex>

namespace nexus::utility::gamedev {

class EntityComponentDebugger {
public:
    static EntityComponentDebugger& instance() {
        static EntityComponentDebugger inst;
        return inst;
    }

    void initialize(const std::string& config = "") { config_ = config; enabled_ = true; }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    std::string getStatus() const { return enabled_ ? "active" : "inactive"; }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        entities_.clear();
    }

    void registerEntity(uint64_t entity, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        entities_[entity] = {name, {}};
    }

    void addComponent(uint64_t entity, const std::string& component) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entities_.find(entity);
        if (it == entities_.end()) return;
        auto& comps = it->second.components;
        if (std::find(comps.begin(), comps.end(), component) == comps.end()) {
            comps.push_back(component);
        }
    }

    void removeComponent(uint64_t entity, const std::string& component) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entities_.find(entity);
        if (it == entities_.end()) return;
        auto& comps = it->second.components;
        comps.erase(std::remove(comps.begin(), comps.end(), component), comps.end());
    }

    std::vector<std::string> getComponents(uint64_t entity) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entities_.find(entity);
        if (it == entities_.end()) return {};
        return it->second.components;
    }

    size_t getEntityCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entities_.size();
    }

    std::string getEntityName(uint64_t entity) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entities_.find(entity);
        if (it == entities_.end()) return "";
        return it->second.name;
    }

private:
    EntityComponentDebugger() = default;
    ~EntityComponentDebugger() = default;

    EntityComponentDebugger(const EntityComponentDebugger&) = delete;
    EntityComponentDebugger& operator=(const EntityComponentDebugger&) = delete;
    EntityComponentDebugger(EntityComponentDebugger&&) = delete;
    EntityComponentDebugger& operator=(EntityComponentDebugger&&) = delete;

    struct EntityInfo {
        std::string name;
        std::vector<std::string> components;
    };

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<uint64_t, EntityInfo> entities_;
};

} // namespace nexus::utility::gamedev
