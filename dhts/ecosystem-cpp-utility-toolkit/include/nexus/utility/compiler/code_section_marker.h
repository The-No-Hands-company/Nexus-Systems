#pragma once
#include <string>
#include <vector>
namespace nexus::utility::compiler {
class CodeSectionMarker {
public:
    enum class Section { Hot, Cold, Startup, Shutdown };
    struct SectionInfo { std::string function; Section section; std::string reason; };
    static CodeSectionMarker& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void markFunction(const std::string& func, Section section, const std::string& reason="");
    SectionInfo getInfo(const std::string& func) const;
    std::vector<SectionInfo> getBySection(Section sec) const;
    std::vector<SectionInfo> getAll() const;
    void clear();
private:
    CodeSectionMarker()=default; ~CodeSectionMarker()=default; bool enabled_=false;
    std::vector<SectionInfo> sections_;
};
} // namespace nexus::utility::compiler