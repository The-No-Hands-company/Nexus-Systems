#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
namespace nexus::templat {
using TemplateParam = std::variant<float,int,bool,std::string>;
struct TemplateDef { std::string name,category,description; std::vector<std::string> paramNames; std::vector<TemplateParam> defaultValues; };
class TemplateManager {
public: uint32_t registerTemplate(const TemplateDef& tpl); [[nodiscard]] const TemplateDef* getTemplate(uint32_t id)const; [[nodiscard]] const TemplateDef* findTemplate(const std::string& name)const; [[nodiscard]] std::vector<uint32_t> listTemplates()const; void removeTemplate(uint32_t id); [[nodiscard]] size_t count()const noexcept{return m_templates.size();} void clear();
private: std::vector<TemplateDef> m_templates; std::vector<uint32_t> m_ids; uint32_t m_nextId=1; };
}
