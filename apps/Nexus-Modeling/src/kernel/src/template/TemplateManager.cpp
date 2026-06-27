#include <algorithm>
namespace nexus::templat {
uint32_t TemplateManager::registerTemplate(const TemplateDef& t){TemplateDef td=t;m_templates.push_back(td);uint32_t id=m_nextId++;m_ids.push_back(id);return id;}
const TemplateDef* TemplateManager::getTemplate(uint32_t id)const{auto it=std::find(m_ids.begin(),m_ids.end(),id);return it!=m_ids.end()?&m_templates[static_cast<size_t>(it-m_ids.begin())]:nullptr;}
const TemplateDef* TemplateManager::findTemplate(const std::string& name)const{for(auto& t:m_templates)if(t.name==name)return &t;return nullptr;}
std::vector<uint32_t> TemplateManager::listTemplates()const{return m_ids;}
void TemplateManager::removeTemplate(uint32_t id){auto it=std::find(m_ids.begin(),m_ids.end(),id);if(it!=m_ids.end()){size_t idx=static_cast<size_t>(it-m_ids.begin());m_templates.erase(m_templates.begin()+static_cast<ptrdiff_t>(idx));m_ids.erase(it);}}
void TemplateManager::clear(){m_templates.clear();m_ids.clear();m_nextId=1;}
}
