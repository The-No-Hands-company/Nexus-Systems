#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <any>

namespace nexus::utility::cpp26 {

class ReflectionDebugger {
public:
    struct MemberInfo {
        std::string name;
        std::string typeName;
        size_t offset;
        size_t size;
        bool isFunction;
        bool isStatic;
    };

    struct TypeInfo {
        std::string name;
        size_t size;
        size_t alignment;
        std::vector<MemberInfo> members;
        bool isReflected;
    };

    void registerType(const std::string& name, size_t size, size_t alignment) {
        types_[name] = {name, size, alignment, {}, true};
    }

    void addMember(const std::string& type, const MemberInfo& member) {
        types_[type].members.push_back(member);
    }

    TypeInfo getType(const std::string& name) const {
        auto it = types_.find(name);
        return it != types_.end() ? it->second : TypeInfo{};
    }

    std::vector<TypeInfo> getAllTypes() const {
        std::vector<TypeInfo> r;
        for (const auto& [name, info] : types_) r.push_back(info);
        return r;
    }

    size_t reflectedTypeCount() const { return types_.size(); }

    void clear() { types_.clear(); }

private:
    std::unordered_map<std::string, TypeInfo> types_;
};

} // namespace nexus::utility::cpp26
