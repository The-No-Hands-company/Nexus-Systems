#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <sstream>
#include <algorithm>

namespace nexus::utility::cpp26 {

/// Track C++26 module dependencies, imports, and circular reference detection
class ModuleDependencyTracker {
public:
    struct ModuleInfo {
        std::string name;
        bool isInterface;
        bool isImplementation;
        std::vector<std::string> imports;
        std::vector<std::string> exportedBy;
    };

    void registerModule(const std::string& name, bool isInterface = false) {
        modules_[name] = {name, isInterface, !isInterface, {}, {}};
    }

    void recordImport(const std::string& from, const std::string& to) {
        modules_[from].imports.push_back(to);
        modules_[to].exportedBy.push_back(from);
    }

    std::vector<std::string> findCircularDependencies() const {
        std::vector<std::string> cycles;
        for (const auto& [name, _] : modules_) {
            std::unordered_set<std::string> visited;
            std::unordered_set<std::string> recStack;
            std::vector<std::string> path;
            if (dfsDetectCycle(name, visited, recStack, path)) {
                std::string cycle;
                for (size_t i = 0; i < path.size(); i++) {
                    if (i > 0) cycle += " -> ";
                    cycle += path[i];
                }
                cycles.push_back(cycle);
            }
        }
        return cycles;
    }

    std::string generateDotGraph() const {
        std::ostringstream os;
        os << "digraph Cpp26Modules {\n  rankdir=LR;\n";
        for (const auto& [name, info] : modules_) {
            std::string shape = info.isInterface ? "box" : "ellipse";
            os << "  \"" << name << "\" [shape=" << shape << "];\n";
            for (const auto& imp : info.imports)
                os << "  \"" << name << "\" -> \"" << imp << "\";\n";
        }
        os << "}\n";
        return os.str();
    }

    ModuleInfo getModule(const std::string& name) const {
        auto it = modules_.find(name);
        return it != modules_.end() ? it->second : ModuleInfo{};
    }

    size_t moduleCount() const { return modules_.size(); }

    std::vector<std::string> getTopologicalOrder() const {
        std::vector<std::string> order;
        std::unordered_map<std::string, int> inDegree;
        for (auto& [name, info] : modules_) {
            if (inDegree.find(name) == inDegree.end()) inDegree[name] = 0;
            for (auto& imp : info.imports) inDegree[imp]++;
        }
        std::vector<std::string> queue;
        for (auto& [name, deg] : inDegree)
            if (deg == 0) queue.push_back(name);
        while (!queue.empty()) {
            auto name = queue.back(); queue.pop_back();
            order.push_back(name);
            for (auto& imp : modules_.at(name).imports) {
                if (--inDegree[imp] == 0) queue.push_back(imp);
            }
        }
        return order;
    }

    void clear() { modules_.clear(); }

private:
    std::unordered_map<std::string, ModuleInfo> modules_;

    bool dfsDetectCycle(const std::string& node, std::unordered_set<std::string>& visited,
                        std::unordered_set<std::string>& recStack,
                        std::vector<std::string>& path) const {
        visited.insert(node);
        recStack.insert(node);
        path.push_back(node);
        auto it = modules_.find(node);
        if (it != modules_.end()) {
            for (const auto& imp : it->second.imports) {
                if (visited.find(imp) == visited.end()) {
                    if (dfsDetectCycle(imp, visited, recStack, path)) return true;
                } else if (recStack.find(imp) != recStack.end()) {
                    path.push_back(imp);
                    return true;
                }
            }
        }
        recStack.erase(node);
        path.pop_back();
        return false;
    }
};
} // namespace nexus::utility::cpp26
