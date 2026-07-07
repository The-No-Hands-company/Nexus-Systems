#!/usr/bin/env python3
"""Flesh out build (11), compiler (9), testing (8) TODO skeletons."""
import os
BASE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug"
INC = f"{BASE}/include/nexus/utility"
SRC = f"{BASE}/src/utility"

def gen(cat, name, header, source):
    os.makedirs(f"{INC}/{cat}", exist_ok=True)
    os.makedirs(f"{SRC}/{cat}", exist_ok=True)
    with open(f"{INC}/{cat}/{name}.h", 'w') as f: f.write(header)
    with open(f"{SRC}/{cat}/{name}.cpp", 'w') as f: f.write(source)

# ===== BUILD (11) =====
gen('build', 'build_time_profiler',
'''#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
namespace nexus::utility::build {
class BuildTimeProfiler {
public:
    struct BuildStep { std::string name; std::chrono::milliseconds duration{0}; size_t fileCount; };
    static BuildTimeProfiler& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void startStep(const std::string& name);
    void endStep(const std::string& name, size_t fileCount = 1);
    std::vector<BuildStep> getSteps() const;
    std::chrono::milliseconds getTotalTime() const;
    BuildStep getSlowestStep() const;
    void clear();
private:
    BuildTimeProfiler()=default; ~BuildTimeProfiler()=default; bool enabled_=false;
    std::unordered_map<std::string,std::chrono::steady_clock::time_point> active_;
    std::vector<BuildStep> steps_;
};
}''',
'''#include "nexus/utility/build/build_time_profiler.h"
#include <algorithm>
namespace nexus::utility::build {
BuildTimeProfiler& BuildTimeProfiler::instance(){static BuildTimeProfiler i;return i;}
void BuildTimeProfiler::initialize(){enabled_=true;active_.clear();steps_.clear();}
void BuildTimeProfiler::shutdown(){enabled_=false;}
bool BuildTimeProfiler::isEnabled()const{return enabled_;}
void BuildTimeProfiler::startStep(const std::string& name){if(!enabled_)return;active_[name]=std::chrono::steady_clock::now();}
void BuildTimeProfiler::endStep(const std::string& name,size_t fc){auto it=active_.find(name);if(it==active_.end())return;auto d=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-it->second);steps_.push_back({name,d,fc});active_.erase(it);}
auto BuildTimeProfiler::getSteps()const->std::vector<BuildStep>{return steps_;}
std::chrono::milliseconds BuildTimeProfiler::getTotalTime()const{std::chrono::milliseconds t{0};for(auto&s:steps_)t+=s.duration;return t;}
auto BuildTimeProfiler::getSlowestStep()const->BuildStep{if(steps_.empty())return{};return *std::max_element(steps_.begin(),steps_.end(),[](auto&a,auto&b){return a.duration<b.duration;});}
void BuildTimeProfiler::clear(){steps_.clear();active_.clear();}
}''')

gen('build', 'circular_include_detector',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
namespace nexus::utility::build {
class CircularIncludeDetector {
public:
    struct CycleReport { std::vector<std::string> path; bool found; };
    static CircularIncludeDetector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordInclude(const std::string& from, const std::string& to);
    CycleReport detectCycles() const;
    std::string generateDotGraph() const;
    size_t getIncludeCount() const;
    void clear();
private:
    CircularIncludeDetector()=default; ~CircularIncludeDetector()=default; bool enabled_=false;
    std::unordered_map<std::string,std::vector<std::string>> graph_;
    bool dfs(const std::string&,std::unordered_set<std::string>&,std::unordered_set<std::string>&,std::vector<std::string>&)const;
};
}''',
'''#include "nexus/utility/build/circular_include_detector.h"
#include <sstream>
namespace nexus::utility::build {
CircularIncludeDetector& CircularIncludeDetector::instance(){static CircularIncludeDetector i;return i;}
void CircularIncludeDetector::initialize(){enabled_=true;graph_.clear();}
void CircularIncludeDetector::shutdown(){enabled_=false;}
bool CircularIncludeDetector::isEnabled()const{return enabled_;}
void CircularIncludeDetector::recordInclude(const std::string& from,const std::string& to){if(!enabled_)return;graph_[from].push_back(to);if(graph_.find(to)==graph_.end())graph_[to];}
bool CircularIncludeDetector::dfs(const std::string& n,std::unordered_set<std::string>& v,std::unordered_set<std::string>& r,std::vector<std::string>& p)const{
    v.insert(n);r.insert(n);p.push_back(n);
    auto it=graph_.find(n);if(it!=graph_.end())for(auto&c:it->second){if(v.find(c)==v.end()){if(dfs(c,v,r,p))return true;}else if(r.find(c)!=r.end()){p.push_back(c);return true;}}
    r.erase(n);p.pop_back();return false;
}
auto CircularIncludeDetector::detectCycles()const->CycleReport{
    std::unordered_set<std::string> visited,rec;std::vector<std::string> path;
    for(auto&[n,_]:graph_){if(visited.find(n)==visited.end()){std::vector<std::string> p;if(dfs(n,visited,rec,p))return {p,true};}}return {{},false};
}
std::string CircularIncludeDetector::generateDotGraph()const{
    std::ostringstream os;os<<"digraph Includes {\\n  rankdir=LR;\\n";
    for(auto&[f,tos]:graph_)for(auto&t:tos)os<<"  \\\""<<f<<"\\\" -> \\\""<<t<<"\\\";\\n";
    os<<"}\\n";return os.str();
}
size_t CircularIncludeDetector::getIncludeCount()const{size_t c=0;for(auto&[_,t]:graph_)c+=t.size();return c;}
void CircularIncludeDetector::clear(){graph_.clear();}
}''')

gen('build', 'compiler_flag_validator',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_set>
namespace nexus::utility::build {
class CompilerFlagValidator {
public:
    struct FlagResult { std::string flag; bool valid; bool known; std::string description; };
    static CompilerFlagValidator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    FlagResult validate(const std::string& flag);
    void addKnownFlag(const std::string& flag, const std::string& desc);
    std::vector<FlagResult> getHistory() const;
    size_t getValidatedCount() const;
    void clear();
private:
    CompilerFlagValidator()=default; ~CompilerFlagValidator()=default; bool enabled_=false;
    std::unordered_map<std::string,std::string> knownFlags_;
    std::vector<FlagResult> history_;
    void buildDefaults();
};
}''',
'''#include "nexus/utility/build/compiler_flag_validator.h"
namespace nexus::utility::build {
CompilerFlagValidator& CompilerFlagValidator::instance(){static CompilerFlagValidator i;return i;}
void CompilerFlagValidator::initialize(){enabled_=true;buildDefaults();}
void CompilerFlagValidator::shutdown(){enabled_=false;}
bool CompilerFlagValidator::isEnabled()const{return enabled_;}
void CompilerFlagValidator::buildDefaults(){
    knownFlags_["-Wall"]="Enable all common warnings";
    knownFlags_["-Wextra"]="Enable extra warnings";
    knownFlags_["-Werror"]="Treat warnings as errors";
    knownFlags_["-O0"]="No optimization";
    knownFlags_["-O2"]="Standard optimization";
    knownFlags_["-O3"]="Aggressive optimization";
    knownFlags_["-g"]="Debug symbols";
    knownFlags_["-std=c++23"]="C++23 standard";
    knownFlags_["-fPIC"]="Position-independent code";
    knownFlags_["-march=native"]="Target native architecture";
}
auto CompilerFlagValidator::validate(const std::string& flag)->FlagResult{
    FlagResult r{flag,true,false,""};
    auto it=knownFlags_.find(flag);
    if(it!=knownFlags_.end()){r.known=true;r.description=it->second;}
    else{r.known=false;r.description="Unknown flag";}
    if(flag.empty())r.valid=false;
    history_.push_back(r);return r;
}
void CompilerFlagValidator::addKnownFlag(const std::string& f,const std::string& d){knownFlags_[f]=d;}
auto CompilerFlagValidator::getHistory()const->std::vector<FlagResult>{return history_;}
size_t CompilerFlagValidator::getValidatedCount()const{return history_.size();}
void CompilerFlagValidator::clear(){history_.clear();}
}''')

gen('build', 'header_dependency_analyzer',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
namespace nexus::utility::build {
class HeaderDependencyAnalyzer {
public:
    struct DependencyStats { std::string header; size_t directDeps; size_t transitiveDeps; bool isHeavy; };
    static HeaderDependencyAnalyzer& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordDependency(const std::string& header, const std::string& dependency);
    DependencyStats analyze(const std::string& header) const;
    std::vector<DependencyStats> analyzeAll() const;
    std::vector<std::string> getHeaviestHeaders(size_t topN = 10) const;
    size_t getTotalDependencies() const;
    void clear();
private:
    HeaderDependencyAnalyzer()=default; ~HeaderDependencyAnalyzer()=default; bool enabled_=false;
    std::unordered_map<std::string,std::unordered_set<std::string>> deps_;
};
}''',
'''#include "nexus/utility/build/header_dependency_analyzer.h"
#include <algorithm>
namespace nexus::utility::build {
HeaderDependencyAnalyzer& HeaderDependencyAnalyzer::instance(){static HeaderDependencyAnalyzer i;return i;}
void HeaderDependencyAnalyzer::initialize(){enabled_=true;deps_.clear();}
void HeaderDependencyAnalyzer::shutdown(){enabled_=false;}
bool HeaderDependencyAnalyzer::isEnabled()const{return enabled_;}
void HeaderDependencyAnalyzer::recordDependency(const std::string& h,const std::string& d){if(!enabled_)return;deps_[h].insert(d);if(deps_.find(d)==deps_.end())deps_[d];}
auto HeaderDependencyAnalyzer::analyze(const std::string& h)const->DependencyStats{DependencyStats{h,deps_.count(h)?deps_.at(h).size():0,0,false};}
auto HeaderDependencyAnalyzer::analyzeAll()const->std::vector<DependencyStats>{
    std::vector<DependencyStats> r;
    for(auto&[n,ds]:deps_){DependencyStats s{n,ds.size(),0,ds.size()>20};r.push_back(s);}
    return r;
}
auto HeaderDependencyAnalyzer::getHeaviestHeaders(size_t topN)const->std::vector<std::string>{
    std::vector<std::pair<std::string,size_t>> v;
    for(auto&[n,ds]:deps_)v.emplace_back(n,ds.size());
    std::sort(v.begin(),v.end(),[](auto&a,auto&b){return a.second>b.second;});
    std::vector<std::string> r;for(size_t i=0;i<topN&&i<v.size();i++)r.push_back(v[i].first);return r;
}
size_t HeaderDependencyAnalyzer::getTotalDependencies()const{size_t c=0;for(auto&[_,ds]:deps_)c+=ds.size();return c;}
void HeaderDependencyAnalyzer::clear(){deps_.clear();}
}''')

gen('build', 'incremental_build_validator',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
namespace nexus::utility::build {
class IncrementalBuildValidator {
public:
    struct FileStamp { std::string path; std::chrono::system_clock::time_point lastModified; bool changed; };
    static IncrementalBuildValidator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordFile(const std::string& path);
    bool hasChanged(const std::string& path) const;
    std::vector<std::string> getChangedFiles() const;
    size_t getTotalFiles() const;
    size_t getChangedCount() const;
    void clear();
private:
    IncrementalBuildValidator()=default; ~IncrementalBuildValidator()=default; bool enabled_=false;
    std::unordered_map<std::string,std::chrono::system_clock::time_point> files_;
    std::vector<std::string> changed_;
};
}''',
'''#include "nexus/utility/build/incremental_build_validator.h"
#include <filesystem>
namespace nexus::utility::build {
IncrementalBuildValidator& IncrementalBuildValidator::instance(){static IncrementalBuildValidator i;return i;}
void IncrementalBuildValidator::initialize(){enabled_=true;files_.clear();changed_.clear();}
void IncrementalBuildValidator::shutdown(){enabled_=false;}
bool IncrementalBuildValidator::isEnabled()const{return enabled_;}
void IncrementalBuildValidator::recordFile(const std::string& path){
    if(!enabled_)return;
    auto ft=std::filesystem::last_write_time(path);
    auto sctp=std::chrono::time_point_cast<std::chrono::system_clock::duration>(ft-std::filesystem::file_time_type::clock::now()+std::chrono::system_clock::now());
    auto it=files_.find(path);
    if(it!=files_.end()&&it->second!=sctp){changed_.push_back(path);}
    files_[path]=sctp;
}
bool IncrementalBuildValidator::hasChanged(const std::string& path)const{
    for(auto&c:changed_)if(c==path)return true;return false;
}
auto IncrementalBuildValidator::getChangedFiles()const->std::vector<std::string>{return changed_;}
size_t IncrementalBuildValidator::getTotalFiles()const{return files_.size();}
size_t IncrementalBuildValidator::getChangedCount()const{return changed_.size();}
void IncrementalBuildValidator::clear(){files_.clear();changed_.clear();}
}''')

gen('build', 'library_version_checker',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::build {
class LibraryVersionChecker {
public:
    struct LibVersion { std::string name; std::string version; std::string required; bool compatible; };
    static LibraryVersionChecker& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerLibrary(const std::string& name, const std::string& version);
    void setRequired(const std::string& name, const std::string& minVersion);
    LibVersion check(const std::string& name) const;
    std::vector<LibVersion> checkAll() const;
    bool allCompatible() const;
    void clear();
private:
    LibraryVersionChecker()=default; ~LibraryVersionChecker()=default; bool enabled_=false;
    std::unordered_map<std::string,std::string> versions_;
    std::unordered_map<std::string,std::string> required_;
    static int compareVersions(const std::string& a, const std::string& b);
};
}''',
'''#include "nexus/utility/build/library_version_checker.h"
#include <sstream>
namespace nexus::utility::build {
LibraryVersionChecker& LibraryVersionChecker::instance(){static LibraryVersionChecker i;return i;}
void LibraryVersionChecker::initialize(){enabled_=true;versions_.clear();required_.clear();}
void LibraryVersionChecker::shutdown(){enabled_=false;}
bool LibraryVersionChecker::isEnabled()const{return enabled_;}
void LibraryVersionChecker::registerLibrary(const std::string& n,const std::string& v){if(!enabled_)return;versions_[n]=v;}
void LibraryVersionChecker::setRequired(const std::string& n,const std::string& v){required_[n]=v;}

int LibraryVersionChecker::compareVersions(const std::string& a,const std::string& b){
    std::vector<int> va,vb;std::istringstream sa(a),sb(b);std::string t;
    while(std::getline(sa,t,'.'))va.push_back(std::stoi(t));
    while(std::getline(sb,t,'.'))vb.push_back(std::stoi(t));
    while(va.size()<vb.size())va.push_back(0);while(vb.size()<va.size())vb.push_back(0);
    for(size_t i=0;i<va.size();i++){if(va[i]<vb[i])return -1;if(va[i]>vb[i])return 1;}
    return 0;
}

auto LibraryVersionChecker::check(const std::string& n)const->LibVersion{
    LibVersion r{n,"","",true};
    auto v=versions_.find(n);if(v!=versions_.end())r.version=v->second;
    auto req=required_.find(n);r.required=req!=required_.end()?req->second:"";
    r.compatible=r.required.empty()||compareVersions(r.version,r.required)>=0;
    return r;
}
auto LibraryVersionChecker::checkAll()const->std::vector<LibVersion>{
    std::vector<LibVersion> r;for(auto&[n,_]:versions_)r.push_back(check(n));return r;
}
bool LibraryVersionChecker::allCompatible()const{for(auto&[n,_]:versions_)if(!check(n).compatible)return false;return true;}
void LibraryVersionChecker::clear(){versions_.clear();required_.clear();}
}''')

gen('build', 'linker_diagnostic_parser',
'''#pragma once
#include <string>
#include <vector>
namespace nexus::utility::build {
class LinkerDiagnosticParser {
public:
    struct LinkerError { std::string symbol; std::string location; std::string suggestion; bool isUndefined; };
    static LinkerDiagnosticParser& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    LinkerError parse(const std::string& errorMessage);
    std::vector<LinkerError> parseLog(const std::string& logOutput);
    std::vector<LinkerError> getErrors() const;
    size_t getErrorCount() const;
    void clear();
private:
    LinkerDiagnosticParser()=default; ~LinkerDiagnosticParser()=default; bool enabled_=false;
    std::vector<LinkerError> errors_;
};
}''',
'''#include "nexus/utility/build/linker_diagnostic_parser.h"
#include <regex>
namespace nexus::utility::build {
LinkerDiagnosticParser& LinkerDiagnosticParser::instance(){static LinkerDiagnosticParser i;return i;}
void LinkerDiagnosticParser::initialize(){enabled_=true;errors_.clear();}
void LinkerDiagnosticParser::shutdown(){enabled_=false;}
bool LinkerDiagnosticParser::isEnabled()const{return enabled_;}

auto LinkerDiagnosticParser::parse(const std::string& msg)->LinkerError{
    LinkerError e;
    std::smatch m;
    if(std::regex_search(msg,m,std::regex(R"(undefined reference to [`'](\\w+))"))){e.symbol=m[1];e.isUndefined=true;}
    else if(std::regex_search(msg,m,std::regex(R"(multiple definition of [`'](\\w+))"))){e.symbol=m[1];e.isUndefined=false;}
    if(std::regex_search(msg,m,std::regex(R"((\\S+:\\d+))")))e.location=m[1];
    e.suggestion=e.isUndefined?"Check linking order and library dependencies":"Use inline or anonymous namespace";
    errors_.push_back(e);return e;
}
auto LinkerDiagnosticParser::parseLog(const std::string& log)->std::vector<LinkerError>{
    std::vector<LinkerError> r;std::istringstream ss(log);std::string line;
    while(std::getline(ss,line)){if(line.find("undefined reference")!=std::string::npos||line.find("multiple definition")!=std::string::npos)r.push_back(parse(line));}
    return r;
}
auto LinkerDiagnosticParser::getErrors()const->std::vector<LinkerError>{return errors_;}
size_t LinkerDiagnosticParser::getErrorCount()const{return errors_.size();}
void LinkerDiagnosticParser::clear(){errors_.clear();}
}''')

gen('build', 'missing_symbol_tracer',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_set>
namespace nexus::utility::build {
class MissingSymbolTracer {
public:
    struct MissingSymbol { std::string name; std::string suggestedLibrary; std::string headerFile; };
    static MissingSymbolTracer& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerMissing(const std::string& symbol);
    void addMapping(const std::string& symbol, const std::string& library, const std::string& header);
    std::vector<MissingSymbol> resolve(const std::string& symbol) const;
    std::vector<MissingSymbol> getUnresolved() const;
    size_t getMissingCount() const;
    void clear();
private:
    MissingSymbolTracer()=default; ~MissingSymbolTracer()=default; bool enabled_=false;
    std::unordered_map<std::string,std::pair<std::string,std::string>> symbolMap_;
    std::unordered_set<std::string> missing_;
};
}''',
'''#include "nexus/utility/build/missing_symbol_tracer.h"
namespace nexus::utility::build {
MissingSymbolTracer& MissingSymbolTracer::instance(){static MissingSymbolTracer i;return i;}
void MissingSymbolTracer::initialize(){enabled_=true;missing_.clear();}
void MissingSymbolTracer::shutdown(){enabled_=false;}
bool MissingSymbolTracer::isEnabled()const{return enabled_;}
void MissingSymbolTracer::registerMissing(const std::string& sym){if(!enabled_)return;missing_.insert(sym);}
void MissingSymbolTracer::addMapping(const std::string& sym,const std::string& lib,const std::string& hdr){symbolMap_[sym]={lib,hdr};}
auto MissingSymbolTracer::resolve(const std::string& sym)const->std::vector<MissingSymbol>{
    std::vector<MissingSymbol> r;auto it=symbolMap_.find(sym);
    if(it!=symbolMap_.end())r.push_back({sym,it->second.first,it->second.second});return r;
}
auto MissingSymbolTracer::getUnresolved()const->std::vector<MissingSymbol>{
    std::vector<MissingSymbol> r;for(auto& s:missing_){auto it=symbolMap_.find(s);r.push_back({s,it!=symbolMap_.end()?it->second.first:"",it!=symbolMap_.end()?it->second.second:""});}
    return r;
}
size_t MissingSymbolTracer::getMissingCount()const{return missing_.size();}
void MissingSymbolTracer::clear(){missing_.clear();}
}''')

gen('build', 'precompiled_header_validator',
'''#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::build {
class PrecompiledHeaderValidator {
public:
    struct PchStats { std::string pchFile; size_t headerCount; std::chrono::milliseconds compileTime{0}; bool valid; };
    static PrecompiledHeaderValidator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordPch(const std::string& pch, size_t headerCount, std::chrono::milliseconds time);
    PchStats getStats(const std::string& pch) const;
    std::vector<PchStats> getAllStats() const;
    size_t getPchCount() const;
    void clear();
private:
    PrecompiledHeaderValidator()=default; ~PrecompiledHeaderValidator()=default; bool enabled_=false;
    std::vector<PchStats> stats_;
};
}''',
'''#include "nexus/utility/build/precompiled_header_validator.h"
namespace nexus::utility::build {
PrecompiledHeaderValidator& PrecompiledHeaderValidator::instance(){static PrecompiledHeaderValidator i;return i;}
void PrecompiledHeaderValidator::initialize(){enabled_=true;stats_.clear();}
void PrecompiledHeaderValidator::shutdown(){enabled_=false;}
bool PrecompiledHeaderValidator::isEnabled()const{return enabled_;}
void PrecompiledHeaderValidator::recordPch(const std::string& f,size_t c,std::chrono::milliseconds t){if(!enabled_)return;stats_.push_back({f,c,t,true});}
auto PrecompiledHeaderValidator::getStats(const std::string& f)const->PchStats{for(auto&s:stats_)if(s.pchFile==f)return s;return{};}
auto PrecompiledHeaderValidator::getAllStats()const->std::vector<PchStats>{return stats_;}
size_t PrecompiledHeaderValidator::getPchCount()const{return stats_.size();}
void PrecompiledHeaderValidator::clear(){stats_.clear();}
}''')

gen('build', 'symbol_conflict_detector',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::build {
class SymbolConflictDetector {
public:
    struct Conflict { std::string symbol; std::vector<std::string> locations; };
    static SymbolConflictDetector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerSymbol(const std::string& symbol, const std::string& location);
    std::vector<Conflict> detectConflicts() const;
    size_t getSymbolCount() const;
    size_t getConflictCount() const;
    void clear();
private:
    SymbolConflictDetector()=default; ~SymbolConflictDetector()=default; bool enabled_=false;
    std::unordered_map<std::string,std::vector<std::string>> symbols_;
};
}''',
'''#include "nexus/utility/build/symbol_conflict_detector.h"
namespace nexus::utility::build {
SymbolConflictDetector& SymbolConflictDetector::instance(){static SymbolConflictDetector i;return i;}
void SymbolConflictDetector::initialize(){enabled_=true;symbols_.clear();}
void SymbolConflictDetector::shutdown(){enabled_=false;}
bool SymbolConflictDetector::isEnabled()const{return enabled_;}
void SymbolConflictDetector::registerSymbol(const std::string& sym,const std::string& loc){if(!enabled_)return;symbols_[sym].push_back(loc);}
auto SymbolConflictDetector::detectConflicts()const->std::vector<Conflict>{
    std::vector<Conflict> r;for(auto&[sym,locs]:symbols_)if(locs.size()>1)r.push_back({sym,locs});return r;
}
size_t SymbolConflictDetector::getSymbolCount()const{return symbols_.size();}
size_t SymbolConflictDetector::getConflictCount()const{size_t c=0;for(auto&[_,l]:symbols_)if(l.size()>1)c++;return c;}
void SymbolConflictDetector::clear(){symbols_.clear();}
}''')

gen('build', 'unity_build_analyzer',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_set>
namespace nexus::utility::build {
class UnityBuildAnalyzer {
public:
    struct UnityStats { std::string unityFile; size_t sourceFiles; size_t totalLines; std::vector<std::string> conflicts; };
    static UnityBuildAnalyzer& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void addUnityFile(const std::string& name, const std::vector<std::string>& sources);
    void reportConflict(const std::string& unityFile, const std::string& conflict);
    UnityStats getStats(const std::string& unityFile) const;
    std::vector<UnityStats> getAllStats() const;
    size_t getConflictCount() const;
    void clear();
private:
    UnityBuildAnalyzer()=default; ~UnityBuildAnalyzer()=default; bool enabled_=false;
    std::unordered_map<std::string,UnityStats> stats_;
};
}''',
'''#include "nexus/utility/build/unity_build_analyzer.h"
namespace nexus::utility::build {
UnityBuildAnalyzer& UnityBuildAnalyzer::instance(){static UnityBuildAnalyzer i;return i;}
void UnityBuildAnalyzer::initialize(){enabled_=true;stats_.clear();}
void UnityBuildAnalyzer::shutdown(){enabled_=false;}
bool UnityBuildAnalyzer::isEnabled()const{return enabled_;}
void UnityBuildAnalyzer::addUnityFile(const std::string& n,const std::vector<std::string>& srcs){if(!enabled_)return;stats_[n]={n,srcs.size(),0,{}};}
void UnityBuildAnalyzer::reportConflict(const std::string& uf,const std::string& c){auto it=stats_.find(uf);if(it!=stats_.end())it->second.conflicts.push_back(c);}
auto UnityBuildAnalyzer::getStats(const std::string& uf)const->UnityStats{auto it=stats_.find(uf);return it!=stats_.end()?it->second:UnityStats{};}
auto UnityBuildAnalyzer::getAllStats()const->std::vector<UnityStats>{std::vector<UnityStats> r;for(auto&[_,s]:stats_)r.push_back(s);return r;}
size_t UnityBuildAnalyzer::getConflictCount()const{size_t c=0;for(auto&[_,s]:stats_)c+=s.conflicts.size();return c;}
void UnityBuildAnalyzer::clear(){stats_.clear();}
}''')

print("Build (11) done!")
