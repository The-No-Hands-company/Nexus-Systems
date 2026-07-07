#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
namespace nexus::utility::database {
class CacheCoherenceValidator {
public:
    struct CoherenceCheck { std::string entity; uint64_t cacheVersion; uint64_t dbVersion; bool coherent; };
    static CacheCoherenceValidator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void trackEntity(const std::string& entity, uint64_t cacheVer, uint64_t dbVer);
    bool isCoherent(const std::string& entity) const;
    std::vector<CoherenceCheck> getAll() const; size_t getIncoherentCount() const; void clear();
private:
    CacheCoherenceValidator()=default; ~CacheCoherenceValidator()=default; bool enabled_=false;
    std::unordered_map<std::string,std::pair<uint64_t,uint64_t>> entities_;
};
} // namespace nexus::utility::database