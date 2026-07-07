#include "nexus/utility/security/sql_injection_detector.h"
#include <regex>
namespace nexus::utility::security {
SqlInjectionDetector& SqlInjectionDetector::instance(){static SqlInjectionDetector i;return i;}
void SqlInjectionDetector::initialize(){std::lock_guard l(mutex_);enabled_=true;buildDefaultPatterns();}
void SqlInjectionDetector::shutdown(){std::lock_guard l(mutex_);enabled_=false;patterns_.clear();history_.clear();detectionCount_=0;}
bool SqlInjectionDetector::isEnabled()const{std::lock_guard l(mutex_);return enabled_;}
void SqlInjectionDetector::buildDefaultPatterns(){try{patterns_.push_back(std::regex(R"(UNION\s+SELECT)",std::regex::icase));patterns_.push_back(std::regex(R"(DROP\s+TABLE)",std::regex::icase));patterns_.push_back(std::regex(R"(INSERT\s+INTO)",std::regex::icase));patterns_.push_back(std::regex(R"(DELETE\s+FROM)",std::regex::icase));patterns_.push_back(std::regex(R"(OR\s+'.*?'\s*=\s*'.*?')",std::regex::icase));patterns_.push_back(std::regex(R"(--\s)",std::regex::icase));patterns_.push_back(std::regex(R"(xp_cmdshell)",std::regex::icase));}catch(const std::regex_error&){patterns_.clear();}}
SqlInjectionDetector::DetectionResult SqlInjectionDetector::scan(const std::string& in){std::lock_guard l(mutex_);DetectionResult r;r.input=in;if(!enabled_||in.empty())return r;for(auto& p:patterns_){std::smatch m;if(std::regex_search(in,m,p)){r.suspicious=true;r.pattern=m.str();r.position=m.position();++detectionCount_;history_.push_back(r);return r;}}return r;}
bool SqlInjectionDetector::addCustomPattern(const std::string& p){try{std::lock_guard l(mutex_);patterns_.push_back(std::regex(p,std::regex::icase));return true;}catch(const std::regex_error&){return false;}}
size_t SqlInjectionDetector::getDetectionCount()const{std::lock_guard l(mutex_);return detectionCount_;}
auto SqlInjectionDetector::getHistory()const->std::vector<DetectionResult>{std::lock_guard l(mutex_);return history_;}
void SqlInjectionDetector::clear(){std::lock_guard l(mutex_);history_.clear();detectionCount_=0;}
} // namespace nexus::utility::security
