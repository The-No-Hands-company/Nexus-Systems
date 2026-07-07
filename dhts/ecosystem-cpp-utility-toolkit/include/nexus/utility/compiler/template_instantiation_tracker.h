#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::compiler {
class TemplateInstantiationTracker {
public:
    struct Instantiation { std::string templateName; std::string fullType; std::string location; size_t depth; };
    static TemplateInstantiationTracker& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordInstantiation(const std::string& tpl, const std::string& fullType, const std::string& loc, size_t depth=0);
    std::vector<Instantiation> getInstantiations(const std::string& tpl) const;
    std::vector<Instantiation> getAll() const;
    size_t getTotalCount() const;
    size_t getMaxDepth() const;
    std::vector<std::string> getMostInstantiated(size_t topN=10) const;
    void clear();
private:
    TemplateInstantiationTracker()=default; ~TemplateInstantiationTracker()=default; bool enabled_=false;
    std::unordered_map<std::string,std::vector<Instantiation>> instantiations_;
    size_t maxDepth_=0;
};
} // namespace nexus::utility::compiler