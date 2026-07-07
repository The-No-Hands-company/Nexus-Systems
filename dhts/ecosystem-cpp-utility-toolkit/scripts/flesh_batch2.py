#!/usr/bin/env python3
"""Flesh out compiler (9) TODO skeletons."""
import os
BASE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug"
INC, SRC = f"{BASE}/include/nexus/utility", f"{BASE}/src/utility"
def gen(cat, name, header, source):
    os.makedirs(f"{INC}/{cat}", exist_ok=True); os.makedirs(f"{SRC}/{cat}", exist_ok=True)
    with open(f"{INC}/{cat}/{name}.h",'w') as f: f.write(header)
    with open(f"{SRC}/{cat}/{name}.cpp",'w') as f: f.write(source)

# --- compiler (9) ---
for item in [
('abi_compatibility_checker',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::compiler {
class AbiCompatibilityChecker {
public:
    struct AbiVersion { std::string component; int version; bool compatible; std::string notes; };
    static AbiCompatibilityChecker& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerComponent(const std::string& name, int version);
    AbiVersion check(const std::string& name, int expectedVersion) const;
    std::vector<AbiVersion> checkAll(int expectedVersion) const;
    size_t componentCount() const;
    void clear();
private:
    AbiCompatibilityChecker()=default; ~AbiCompatibilityChecker()=default; bool enabled_=false;
    std::unordered_map<std::string,int> components_;
};
}''',
'''#include "nexus/utility/compiler/abi_compatibility_checker.h"
namespace nexus::utility::compiler {
AbiCompatibilityChecker& AbiCompatibilityChecker::instance(){static AbiCompatibilityChecker i;return i;}
void AbiCompatibilityChecker::initialize(){enabled_=true;components_.clear();}
void AbiCompatibilityChecker::shutdown(){enabled_=false;}
bool AbiCompatibilityChecker::isEnabled()const{return enabled_;}
void AbiCompatibilityChecker::registerComponent(const std::string& n,int v){components_[n]=v;}
auto AbiCompatibilityChecker::check(const std::string& n,int ev)const->AbiVersion{auto it=components_.find(n);int v=it!=components_.end()?it->second:0;return{n,v,v>=ev,""};}
auto AbiCompatibilityChecker::checkAll(int ev)const->std::vector<AbiVersion>{std::vector<AbiVersion> r;for(auto&[n,v]:components_)r.push_back(check(n,ev));return r;}
size_t AbiCompatibilityChecker::componentCount()const{return components_.size();}
void AbiCompatibilityChecker::clear(){components_.clear();}
}'''),

('code_section_marker',
'''#pragma once
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
}''',
'''#include "nexus/utility/compiler/code_section_marker.h"
namespace nexus::utility::compiler {
CodeSectionMarker& CodeSectionMarker::instance(){static CodeSectionMarker i;return i;}
void CodeSectionMarker::initialize(){enabled_=true;sections_.clear();}
void CodeSectionMarker::shutdown(){enabled_=false;}
bool CodeSectionMarker::isEnabled()const{return enabled_;}
void CodeSectionMarker::markFunction(const std::string& f,Section s,const std::string& r){sections_.push_back({f,s,r});}
auto CodeSectionMarker::getInfo(const std::string& f)const->SectionInfo{for(auto&s:sections_)if(s.function==f)return s;return{};}
auto CodeSectionMarker::getBySection(Section sec)const->std::vector<SectionInfo>{std::vector<SectionInfo> r;for(auto&s:sections_)if(s.section==sec)r.push_back(s);return r;}
auto CodeSectionMarker::getAll()const->std::vector<SectionInfo>{return sections_;}
void CodeSectionMarker::clear(){sections_.clear();}
}'''),

('compile_time_counter',
'''#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::compiler {
class CompileTimeCounter {
public:
    struct CompileUnit { std::string file; std::chrono::milliseconds time{0}; size_t lines; size_t templates; };
    static CompileTimeCounter& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordUnit(const std::string& file, std::chrono::milliseconds time, size_t lines, size_t templates=0);
    CompileUnit getUnit(const std::string& file) const;
    std::vector<CompileUnit> getSlowest(size_t topN=10) const;
    std::chrono::milliseconds getTotalTime() const;
    size_t getUnitCount() const;
    void clear();
private:
    CompileTimeCounter()=default; ~CompileTimeCounter()=default; bool enabled_=false;
    std::vector<CompileUnit> units_;
};
}''',
'''#include "nexus/utility/compiler/compile_time_counter.h"
#include <algorithm>
namespace nexus::utility::compiler {
CompileTimeCounter& CompileTimeCounter::instance(){static CompileTimeCounter i;return i;}
void CompileTimeCounter::initialize(){enabled_=true;units_.clear();}
void CompileTimeCounter::shutdown(){enabled_=false;}
bool CompileTimeCounter::isEnabled()const{return enabled_;}
void CompileTimeCounter::recordUnit(const std::string& f,std::chrono::milliseconds t,size_t l,size_t tp){units_.push_back({f,t,l,tp});}
auto CompileTimeCounter::getUnit(const std::string& f)const->CompileUnit{for(auto&u:units_)if(u.file==f)return u;return{};}
auto CompileTimeCounter::getSlowest(size_t n)const->std::vector<CompileUnit>{auto u=units_;std::sort(u.begin(),u.end(),[](auto&a,auto&b){return a.time>b.time;});if(n<u.size())u.resize(n);return u;}
std::chrono::milliseconds CompileTimeCounter::getTotalTime()const{std::chrono::milliseconds t{0};for(auto&u:units_)t+=u.time;return t;}
size_t CompileTimeCounter::getUnitCount()const{return units_.size();}
void CompileTimeCounter::clear(){units_.clear();}
}'''),

('inline_decision_reporter',
'''#pragma once
#include <string>
#include <vector>
namespace nexus::utility::compiler {
class InlineDecisionReporter {
public:
    struct InlineDecision { std::string function; bool inlined; std::string reason; size_t size; };
    static InlineDecisionReporter& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordDecision(const std::string& func, bool inlined, const std::string& reason, size_t size=0);
    std::vector<InlineDecision> getDecisions() const;
    size_t getInlinedCount() const;
    size_t getNotInlinedCount() const;
    double getInliningRate() const;
    void clear();
private:
    InlineDecisionReporter()=default; ~InlineDecisionReporter()=default; bool enabled_=false;
    std::vector<InlineDecision> decisions_;
    size_t inlined_=0,notInlined_=0;
};
}''',
'''#include "nexus/utility/compiler/inline_decision_reporter.h"
namespace nexus::utility::compiler {
InlineDecisionReporter& InlineDecisionReporter::instance(){static InlineDecisionReporter i;return i;}
void InlineDecisionReporter::initialize(){enabled_=true;decisions_.clear();inlined_=0;notInlined_=0;}
void InlineDecisionReporter::shutdown(){enabled_=false;}
bool InlineDecisionReporter::isEnabled()const{return enabled_;}
void InlineDecisionReporter::recordDecision(const std::string& f,bool inl,const std::string& r,size_t sz){decisions_.push_back({f,inl,r,sz});if(inl)inlined_++;else notInlined_++;}
auto InlineDecisionReporter::getDecisions()const->std::vector<InlineDecision>{return decisions_;}
size_t InlineDecisionReporter::getInlinedCount()const{return inlined_;}
size_t InlineDecisionReporter::getNotInlinedCount()const{return notInlined_;}
double InlineDecisionReporter::getInliningRate()const{size_t t=inlined_+notInlined_;return t>0?(double)inlined_/t*100.0:0.0;}
void InlineDecisionReporter::clear(){decisions_.clear();inlined_=0;notInlined_=0;}
}'''),

('macro_expansion_debugger',
'''#pragma once
#include <string>
#include <vector>
namespace nexus::utility::compiler {
class MacroExpansionDebugger {
public:
    struct ExpansionStep { std::string macro; int depth; std::string result; };
    static MacroExpansionDebugger& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void startExpansion(const std::string& macro);
    void recordStep(const std::string& macro, int depth, const std::string& result);
    std::vector<ExpansionStep> getExpansion(const std::string& macro) const;
    size_t getMacroCount() const;
    size_t getMaxDepth(const std::string& macro) const;
    void clear();
private:
    MacroExpansionDebugger()=default; ~MacroExpansionDebugger()=default; bool enabled_=false;
    std::unordered_map<std::string,std::vector<ExpansionStep>> expansions_;
};
}''',
'''#include "nexus/utility/compiler/macro_expansion_debugger.h"
#include <algorithm>
namespace nexus::utility::compiler {
MacroExpansionDebugger& MacroExpansionDebugger::instance(){static MacroExpansionDebugger i;return i;}
void MacroExpansionDebugger::initialize(){enabled_=true;expansions_.clear();}
void MacroExpansionDebugger::shutdown(){enabled_=false;}
bool MacroExpansionDebugger::isEnabled()const{return enabled_;}
void MacroExpansionDebugger::startExpansion(const std::string& m){if(!enabled_)return;expansions_[m];}
void MacroExpansionDebugger::recordStep(const std::string& m,int d,const std::string& r){expansions_[m].push_back({m,d,r});}
auto MacroExpansionDebugger::getExpansion(const std::string& m)const->std::vector<ExpansionStep>{auto it=expansions_.find(m);return it!=expansions_.end()?it->second:std::vector<ExpansionStep>{};}
size_t MacroExpansionDebugger::getMacroCount()const{return expansions_.size();}
size_t MacroExpansionDebugger::getMaxDepth(const std::string& m)const{auto it=expansions_.find(m);if(it==expansions_.end())return 0;size_t mx=0;for(auto&s:it->second)mx=std::max(mx,(size_t)s.depth);return mx;}
void MacroExpansionDebugger::clear(){expansions_.clear();}
}'''),

('optimization_barrier',
'''#pragma once
#include <string>
#include <vector>
namespace nexus::utility::compiler {
class OptimizationBarrier {
public:
    struct BarrierRecord { std::string variable; std::string location; std::string reason; };
    static OptimizationBarrier& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordBarrier(const std::string& var, const std::string& loc, const std::string& reason);
    std::vector<BarrierRecord> getBarriers() const;
    size_t getBarrierCount() const;
    void clear();
private:
    OptimizationBarrier()=default; ~OptimizationBarrier()=default; bool enabled_=false;
    std::vector<BarrierRecord> barriers_;
};
}''',
'''#include "nexus/utility/compiler/optimization_barrier.h"
namespace nexus::utility::compiler {
OptimizationBarrier& OptimizationBarrier::instance(){static OptimizationBarrier i;return i;}
void OptimizationBarrier::initialize(){enabled_=true;barriers_.clear();}
void OptimizationBarrier::shutdown(){enabled_=false;}
bool OptimizationBarrier::isEnabled()const{return enabled_;}
void OptimizationBarrier::recordBarrier(const std::string& v,const std::string& l,const std::string& r){barriers_.push_back({v,l,r});}
auto OptimizationBarrier::getBarriers()const->std::vector<BarrierRecord>{return barriers_;}
size_t OptimizationBarrier::getBarrierCount()const{return barriers_.size();}
void OptimizationBarrier::clear(){barriers_.clear();}
}'''),

('symbol_visibility_checker',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::compiler {
class SymbolVisibilityChecker {
public:
    enum class Visibility { Default, Hidden, Internal, Protected };
    struct SymbolInfo { std::string name; Visibility visibility; std::string library; };
    static SymbolVisibilityChecker& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerSymbol(const std::string& name, Visibility vis, const std::string& lib);
    SymbolInfo getSymbol(const std::string& name) const;
    std::vector<SymbolInfo> getSymbolsByLib(const std::string& lib) const;
    std::vector<SymbolInfo> getByVisibility(Visibility vis) const;
    size_t getSymbolCount() const;
    void clear();
private:
    SymbolVisibilityChecker()=default; ~SymbolVisibilityChecker()=default; bool enabled_=false;
    std::unordered_map<std::string,SymbolInfo> symbols_;
};
}''',
'''#include "nexus/utility/compiler/symbol_visibility_checker.h"
namespace nexus::utility::compiler {
SymbolVisibilityChecker& SymbolVisibilityChecker::instance(){static SymbolVisibilityChecker i;return i;}
void SymbolVisibilityChecker::initialize(){enabled_=true;symbols_.clear();}
void SymbolVisibilityChecker::shutdown(){enabled_=false;}
bool SymbolVisibilityChecker::isEnabled()const{return enabled_;}
void SymbolVisibilityChecker::registerSymbol(const std::string& n,Visibility v,const std::string& l){symbols_[n]={n,v,l};}
auto SymbolVisibilityChecker::getSymbol(const std::string& n)const->SymbolInfo{auto it=symbols_.find(n);return it!=symbols_.end()?it->second:SymbolInfo{};}
auto SymbolVisibilityChecker::getSymbolsByLib(const std::string& l)const->std::vector<SymbolInfo>{std::vector<SymbolInfo> r;for(auto&[_,s]:symbols_)if(s.library==l)r.push_back(s);return r;}
auto SymbolVisibilityChecker::getByVisibility(Visibility v)const->std::vector<SymbolInfo>{std::vector<SymbolInfo> r;for(auto&[_,s]:symbols_)if(s.visibility==v)r.push_back(s);return r;}
size_t SymbolVisibilityChecker::getSymbolCount()const{return symbols_.size();}
void SymbolVisibilityChecker::clear(){symbols_.clear();}
}'''),

('template_instantiation_tracker',
'''#pragma once
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
}''',
'''#include "nexus/utility/compiler/template_instantiation_tracker.h"
#include <algorithm>
namespace nexus::utility::compiler {
TemplateInstantiationTracker& TemplateInstantiationTracker::instance(){static TemplateInstantiationTracker i;return i;}
void TemplateInstantiationTracker::initialize(){enabled_=true;instantiations_.clear();maxDepth_=0;}
void TemplateInstantiationTracker::shutdown(){enabled_=false;}
bool TemplateInstantiationTracker::isEnabled()const{return enabled_;}
void TemplateInstantiationTracker::recordInstantiation(const std::string& t,const std::string& ft,const std::string& l,size_t d){instantiations_[t].push_back({t,ft,l,d});maxDepth_=std::max(maxDepth_,d);}
auto TemplateInstantiationTracker::getInstantiations(const std::string& t)const->std::vector<Instantiation>{auto it=instantiations_.find(t);return it!=instantiations_.end()?it->second:std::vector<Instantiation>{};}
auto TemplateInstantiationTracker::getAll()const->std::vector<Instantiation>{std::vector<Instantiation> r;for(auto&[_,v]:instantiations_)r.insert(r.end(),v.begin(),v.end());return r;}
size_t TemplateInstantiationTracker::getTotalCount()const{size_t c=0;for(auto&[_,v]:instantiations_)c+=v.size();return c;}
size_t TemplateInstantiationTracker::getMaxDepth()const{return maxDepth_;}
auto TemplateInstantiationTracker::getMostInstantiated(size_t n)const->std::vector<std::string>{
    std::vector<std::pair<std::string,size_t>> v;for(auto&[t,is]:instantiations_)v.emplace_back(t,is.size());
    std::sort(v.begin(),v.end(),[](auto&a,auto&b){return a.second>b.second;});
    std::vector<std::string> r;for(size_t i=0;i<n&&i<v.size();i++)r.push_back(v[i].first);return r;
}
void TemplateInstantiationTracker::clear(){instantiations_.clear();maxDepth_=0;}
}'''),

('vtable_inspector',
'''#pragma once
#include <string>
#include <vector>
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
}''',
'''#include "nexus/utility/compiler/vtable_inspector.h"
namespace nexus::utility::compiler {
VtableInspector& VtableInspector::instance(){static VtableInspector i;return i;}
void VtableInspector::initialize(){enabled_=true;vtables_.clear();}
void VtableInspector::shutdown(){enabled_=false;}
bool VtableInspector::isEnabled()const{return enabled_;}
void VtableInspector::registerClass(const std::string& c){if(!enabled_)return;vtables_[c];}
void VtableInspector::addMethod(const std::string& c,const std::string& m,size_t i,bool p){vtables_[c].push_back({c,m,i,p});}
auto VtableInspector::getVtable(const std::string& c)const->std::vector<VirtualMethod>{auto it=vtables_.find(c);return it!=vtables_.end()?it->second:std::vector<VirtualMethod>{};}
size_t VtableInspector::getVtableSize(const std::string& c)const{auto it=vtables_.find(c);return it!=vtables_.end()?it->second.size():0;}
size_t VtableInspector::getClassCount()const{return vtables_.size();}
void VtableInspector::clear(){vtables_.clear();}
}'''),
]:
    gen('compiler', item[0], item[1], item[2])

print("Compiler (9) done!")
