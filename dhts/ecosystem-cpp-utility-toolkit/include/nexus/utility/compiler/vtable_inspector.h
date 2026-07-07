#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::compiler {
class VtableInspector {
public:
    struct VirtualMethod { std::string className; std::string methodName; size_t vtableIndex; bool isPure; };
    static VtableInspector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerClass(const std::string& className);
    void addMethod(const std::string& className, const std::string& method, size_t index, bool pure=false);
    std::vector<VirtualMethod> getVtable(const std::string& className) const;
    size_t getVtableSize(const std::string& className) const;
    size_t getClassCount() const;
    void clear();
private:
    VtableInspector()=default; ~VtableInspector()=default; bool enabled_=false;
    std::unordered_map<std::string,std::vector<VirtualMethod>> vtables_;
};
} // namespace nexus::utility::compiler