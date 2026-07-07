#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <any>
#include <typeindex>
#include <stdexcept>

namespace nexus::utility::serialization {

/// Data format migration tool for evolving data schemas over versions
class DataMigrationTool {
public:
    using MigrationFn = std::function<std::any(const std::any&)>;

    struct Migration {
        uint32_t from_version;
        uint32_t to_version;
        std::string description;
        MigrationFn transform;
    };

    struct MigrationChain {
        std::string name;
        std::vector<Migration> migrations;
    };

    DataMigrationTool() = default;

    /// Register a migration chain
    void registerChain(MigrationChain chain) {
        chains_[chain.name] = std::move(chain);
    }

    /// Migrate data from one version to another
    std::any migrate(const std::string& chain_name, uint32_t from_version,
                     uint32_t to_version, const std::any& data) {
        auto it = chains_.find(chain_name);
        if (it == chains_.end()) {
            throw std::runtime_error("Unknown migration chain: " + chain_name);
        }

        const auto& chain = it->second;
        auto current = data;
        uint32_t version = from_version;

        if (from_version < to_version) {
            // Forward migration
            for (const auto& mig : chain.migrations) {
                if (mig.from_version == version) {
                    current = mig.transform(current);
                    version = mig.to_version;
                    if (version == to_version) break;
                }
            }
        } else if (from_version > to_version) {
            throw std::runtime_error("Reverse migration not supported yet");
        }

        if (version != to_version) {
            throw std::runtime_error(
                "Migration path not found: " + std::to_string(from_version) +
                " -> " + std::to_string(to_version));
        }

        return current;
    }

    /// Check if a migration path exists
    bool canMigrate(const std::string& chain_name, uint32_t from, uint32_t to) const {
        auto it = chains_.find(chain_name);
        if (it == chains_.end()) return false;

        if (from == to) return true;
        if (from > to) return false;

        uint32_t version = from;
        for (const auto& mig : it->second.migrations) {
            if (mig.from_version == version) {
                version = mig.to_version;
                if (version == to) return true;
            }
        }
        return false;
    }

    /// Get the latest version in a chain
    uint32_t latestVersion(const std::string& chain_name) const {
        auto it = chains_.find(chain_name);
        if (it == chains_.end()) return 0;
        if (it->second.migrations.empty()) return 0;

        uint32_t max_ver = 0;
        for (const auto& mig : it->second.migrations) {
            if (mig.to_version > max_ver) max_ver = mig.to_version;
        }
        return max_ver;
    }

    /// List all registered chains
    std::vector<std::string> listChains() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : chains_) {
            names.push_back(name);
        }
        return names;
    }

    /// Generate a migration report for a chain
    std::string generateReport(const std::string& chain_name) const {
        auto it = chains_.find(chain_name);
        if (it == chains_.end()) return "Unknown chain: " + chain_name;

        std::string report = "Migration Chain: " + chain_name + "\n";
        report += "Migrations:\n";
        for (const auto& mig : it->second.migrations) {
            report += "  v" + std::to_string(mig.from_version) +
                      " -> v" + std::to_string(mig.to_version) +
                      ": " + mig.description + "\n";
        }
        return report;
    }

private:
    std::map<std::string, MigrationChain> chains_;
};

} // namespace nexus::utility::serialization
