#pragma once
#include <string>
#include <vector>
namespace nexus::utility::database {
class MigrationValidator {
public:
    struct Migration { std::string name; int version; bool applied; std::string checksum; };
    static MigrationValidator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerMigration(const std::string& name,int version,const std::string& sum);
    void markApplied(const std::string& name);
    std::vector<Migration> getPending() const; std::vector<Migration> getApplied() const; bool allApplied() const; void clear();
private:
    MigrationValidator()=default; ~MigrationValidator()=default; bool enabled_=false;
    std::vector<Migration> migrations_;
};
} // namespace nexus::utility::database