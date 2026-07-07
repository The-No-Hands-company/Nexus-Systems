#!/usr/bin/env python3
"""Flesh out testing (8), graphics (8), database (8) skeletons."""
import os
BASE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug"
INC, SRC = f"{BASE}/include/nexus/utility", f"{BASE}/src/utility"
def gen(cat, name, h, s):
    os.makedirs(f"{INC}/{cat}", exist_ok=True); os.makedirs(f"{SRC}/{cat}", exist_ok=True)
    with open(f"{INC}/{cat}/{name}.h",'w') as f: f.write(h)
    with open(f"{SRC}/{cat}/{name}.cpp",'w') as f: f.write(s)

# === TESTING (8) ===

gen('testing','coverage_marker',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_set>
namespace nexus::utility::testing {
class CoverageMarker {
public:
    static CoverageMarker& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void markExecuted(const std::string& file, int line);
    std::unordered_set<int> getCoveredLines(const std::string& file) const;
    bool isCovered(const std::string& file, int line) const;
    std::vector<std::string> getTrackedFiles() const;
    double getCoveragePercent(const std::string& file, int totalLines) const;
    void clear();
private:
    CoverageMarker()=default; ~CoverageMarker()=default; bool enabled_=false;
    std::unordered_map<std::string,std::unordered_set<int>> coverage_;
};
}''',
'''#include "nexus/utility/testing/coverage_marker.h"
namespace nexus::utility::testing {
CoverageMarker& CoverageMarker::instance(){static CoverageMarker i;return i;}
void CoverageMarker::initialize(){enabled_=true;coverage_.clear();}
void CoverageMarker::shutdown(){enabled_=false;}
bool CoverageMarker::isEnabled()const{return enabled_;}
void CoverageMarker::markExecuted(const std::string& f,int l){if(!enabled_)return;coverage_[f].insert(l);}
auto CoverageMarker::getCoveredLines(const std::string& f)const->std::unordered_set<int>{auto it=coverage_.find(f);return it!=coverage_.end()?it->second:std::unordered_set<int>{};}
bool CoverageMarker::isCovered(const std::string& f,int l)const{auto it=coverage_.find(f);return it!=coverage_.end()&&it->second.count(l);}
auto CoverageMarker::getTrackedFiles()const->std::vector<std::string>{std::vector<std::string> r;for(auto&[n,_]:coverage_)r.push_back(n);return r;}
double CoverageMarker::getCoveragePercent(const std::string& f,int total)const{auto it=coverage_.find(f);if(!it||total<=0)return 0.0;return(double)it->second.size()/total*100.0;}
void CoverageMarker::clear(){coverage_.clear();}
}''')

gen('testing','fuzzer_harness',
'''#pragma once
#include <string>
#include <vector>
#include <functional>
#include <chrono>
namespace nexus::utility::testing {
class FuzzerHarness {
public:
    using FuzzFn = std::function<void(const std::vector<uint8_t>&)>;
    struct FuzzResult { std::string target; size_t iterations; size_t crashes; size_t hangs; std::chrono::milliseconds duration{0}; };
    static FuzzerHarness& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerTarget(const std::string& name, FuzzFn fn);
    FuzzResult runFuzzer(const std::string& name, size_t iterations, std::chrono::milliseconds timeout=std::chrono::milliseconds(5000));
    std::vector<FuzzResult> getResults() const;
    size_t getTotalCrashes() const;
    void clear();
private:
    FuzzerHarness()=default; ~FuzzerHarness()=default; bool enabled_=false;
    std::unordered_map<std::string,FuzzFn> targets_;
    std::vector<FuzzResult> results_;
};
}''',
'''#include "nexus/utility/testing/fuzzer_harness.h"
#include <random>
namespace nexus::utility::testing {
FuzzerHarness& FuzzerHarness::instance(){static FuzzerHarness i;return i;}
void FuzzerHarness::initialize(){enabled_=true;targets_.clear();results_.clear();}
void FuzzerHarness::shutdown(){enabled_=false;}
bool FuzzerHarness::isEnabled()const{return enabled_;}
void FuzzerHarness::registerTarget(const std::string& n,FuzzFn f){if(f)targets_[n]=f;}
FuzzerHarness::FuzzResult FuzzerHarness::runFuzzer(const std::string& name,size_t iters,std::chrono::milliseconds timeout){
    FuzzResult r{name,0,0,0,std::chrono::milliseconds(0)};
    auto it=targets_.find(name);if(it==targets_.end())return r;
    auto start=std::chrono::steady_clock::now();
    std::mt19937 rng{std::random_device{}()};
    for(size_t i=0;i<iters;i++){
        if(std::chrono::steady_clock::now()-start>timeout){r.hangs++;break;}
        size_t sz=rng()%1024;std::vector<uint8_t> data(sz);
        for(auto&b:data)b=rng()%256;
        try{it->second(data);r.iterations++;}
        catch(...){r.crashes++;}
    }
    r.duration=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start);
    results_.push_back(r);return r;
}
auto FuzzerHarness::getResults()const->std::vector<FuzzResult>{return results_;}
size_t FuzzerHarness::getTotalCrashes()const{size_t c=0;for(auto&r:results_)c+=r.crashes;return c;}
void FuzzerHarness::clear(){targets_.clear();results_.clear();}
}''')

gen('testing','mock_generator',
'''#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <any>
namespace nexus::utility::testing {
class MockGenerator {
public:
    struct MockCall { std::string method; std::vector<std::any> args; size_t callIndex; };
    using MockBehavior = std::function<std::any(const std::vector<std::any>&)>;
    static MockGenerator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerMock(const std::string& className);
    void setBehavior(const std::string& className, const std::string& method, MockBehavior behavior);
    void recordCall(const std::string& className, const std::string& method, const std::vector<std::any>& args);
    std::vector<MockCall> getCalls(const std::string& className) const;
    size_t getCallCount(const std::string& className) const;
    void verifyCalled(const std::string& className, const std::string& method, size_t expectedCount);
    void clear();
private:
    MockGenerator()=default; ~MockGenerator()=default; bool enabled_=false;
    std::unordered_map<std::string,std::unordered_map<std::string,MockBehavior>> behaviors_;
    std::unordered_map<std::string,std::vector<MockCall>> calls_;
};
}''',
'''#include "nexus/utility/testing/mock_generator.h"
#include <stdexcept>
namespace nexus::utility::testing {
MockGenerator& MockGenerator::instance(){static MockGenerator i;return i;}
void MockGenerator::initialize(){enabled_=true;behaviors_.clear();calls_.clear();}
void MockGenerator::shutdown(){enabled_=false;}
bool MockGenerator::isEnabled()const{return enabled_;}
void MockGenerator::registerMock(const std::string& c){if(!enabled_)return;calls_[c];}
void MockGenerator::setBehavior(const std::string& c,const std::string& m,MockBehavior b){behaviors_[c][m]=b;}
void MockGenerator::recordCall(const std::string& c,const std::string& m,const std::vector<std::any>& a){calls_[c].push_back({m,a,calls_[c].size()});}
auto MockGenerator::getCalls(const std::string& c)const->std::vector<MockCall>{auto it=calls_.find(c);return it!=calls_.end()?it->second:std::vector<MockCall>{};}
size_t MockGenerator::getCallCount(const std::string& c)const{auto it=calls_.find(c);return it!=calls_.end()?it->second.size():0;}
void MockGenerator::verifyCalled(const std::string& c,const std::string& m,size_t expected){
    size_t actual=0;auto it=calls_.find(c);if(it!=calls_.end()){for(auto&call:it->second)if(call.method==m)actual++;}
    if(actual!=expected)throw std::runtime_error("Mock verification failed: "+c+"::"+m+" expected "+std::to_string(expected)+" calls, got "+std::to_string(actual));
}
void MockGenerator::clear(){behaviors_.clear();calls_.clear();}
}''')

gen('testing','property_tester',
'''#pragma once
#include <string>
#include <vector>
#include <functional>
#include <chrono>
namespace nexus::utility::testing {
class PropertyTester {
public:
    using PropertyFn = std::function<bool()>;
    struct PropertyResult { std::string name; size_t testsRun; size_t testsPassed; bool propertyHolds; std::string counterexample; std::chrono::milliseconds duration{0}; };
    static PropertyTester& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerProperty(const std::string& name, PropertyFn test);
    PropertyResult runProperty(const std::string& name, size_t iterations=100);
    std::vector<PropertyResult> getResults() const;
    size_t getFailureCount() const;
    void clear();
private:
    PropertyTester()=default; ~PropertyTester()=default; bool enabled_=false;
    std::unordered_map<std::string,PropertyFn> properties_;
    std::vector<PropertyResult> results_;
};
}''',
'''#include "nexus/utility/testing/property_tester.h"
namespace nexus::utility::testing {
PropertyTester& PropertyTester::instance(){static PropertyTester i;return i;}
void PropertyTester::initialize(){enabled_=true;properties_.clear();results_.clear();}
void PropertyTester::shutdown(){enabled_=false;}
bool PropertyTester::isEnabled()const{return enabled_;}
void PropertyTester::registerProperty(const std::string& n,PropertyFn f){if(f)properties_[n]=f;}
PropertyTester::PropertyResult PropertyTester::runProperty(const std::string& name,size_t iters){
    PropertyResult r{name,iters,0,true,"",std::chrono::milliseconds(0)};
    auto it=properties_.find(name);if(it==properties_.end())return r;
    auto start=std::chrono::steady_clock::now();
    for(size_t i=0;i<iters;i++){try{if(it->second())r.testsPassed++;else{r.propertyHolds=false;return r;}}catch(...){r.propertyHolds=false;return r;}}
    r.duration=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start);
    r.propertyHolds=(r.testsPassed==iters);results_.push_back(r);return r;
}
auto PropertyTester::getResults()const->std::vector<PropertyResult>{return results_;}
size_t PropertyTester::getFailureCount()const{size_t c=0;for(auto&r:results_)if(!r.propertyHolds)c++;return c;}
void PropertyTester::clear(){properties_.clear();results_.clear();}
}''')

gen('testing','regression_detector',
'''#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::testing {
class RegressionDetector {
public:
    struct Benchmark { std::string name; std::chrono::microseconds baseline{0}; std::chrono::microseconds current{0}; double regressionPercent; };
    static RegressionDetector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void setBaseline(const std::string& name, std::chrono::microseconds time);
    void recordResult(const std::string& name, std::chrono::microseconds time);
    Benchmark check(const std::string& name) const;
    std::vector<Benchmark> checkAll() const;
    std::vector<Benchmark> getRegressions(double thresholdPercent=10.0) const;
    size_t getRegressionCount() const;
    void clear();
private:
    RegressionDetector()=default; ~RegressionDetector()=default; bool enabled_=false;
    std::unordered_map<std::string,std::chrono::microseconds> baselines_;
    std::unordered_map<std::string,std::chrono::microseconds> results_;
};
}''',
'''#include "nexus/utility/testing/regression_detector.h"
namespace nexus::utility::testing {
RegressionDetector& RegressionDetector::instance(){static RegressionDetector i;return i;}
void RegressionDetector::initialize(){enabled_=true;baselines_.clear();results_.clear();}
void RegressionDetector::shutdown(){enabled_=false;}
bool RegressionDetector::isEnabled()const{return enabled_;}
void RegressionDetector::setBaseline(const std::string& n,std::chrono::microseconds t){baselines_[n]=t;}
void RegressionDetector::recordResult(const std::string& n,std::chrono::microseconds t){results_[n]=t;}
RegressionDetector::Benchmark RegressionDetector::check(const std::string& n)const{
    Benchmark b{n,std::chrono::microseconds(0),std::chrono::microseconds(0),0.0};
    auto bl=baselines_.find(n);if(bl!=baselines_.end())b.baseline=bl->second;
    auto r=results_.find(n);if(r!=results_.end())b.current=r->second;
    if(b.baseline.count()>0)b.regressionPercent=(double)(b.current-b.baseline).count()/b.baseline.count()*100.0;
    return b;
}
auto RegressionDetector::checkAll()const->std::vector<Benchmark>{std::vector<Benchmark> r;for(auto&[n,_]:baselines_)r.push_back(check(n));return r;}
auto RegressionDetector::getRegressions(double t)const->std::vector<Benchmark>{std::vector<Benchmark> r;for(auto&b:checkAll())if(b.regressionPercent>t)r.push_back(b);return r;}
size_t RegressionDetector::getRegressionCount()const{return getRegressions().size();}
void RegressionDetector::clear(){baselines_.clear();results_.clear();}
}''')

gen('testing','test_data_generator',
'''#pragma once
#include <string>
#include <vector>
#include <random>
#include <cstdint>
namespace nexus::utility::testing {
class TestDataGenerator {
public:
    static TestDataGenerator& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    std::string randomString(size_t length=32);
    int64_t randomInt(int64_t min, int64_t max);
    double randomDouble(double min=0.0, double max=1.0);
    std::vector<uint8_t> randomBytes(size_t count);
    std::string randomEmail();
    std::string randomUrl();
    template<typename T> std::vector<T> randomVector(size_t count, T min, T max);
    size_t getGeneratedCount() const;
    void clear();
private:
    TestDataGenerator()=default; ~TestDataGenerator()=default; bool enabled_=false;
    std::mt19937 rng_{std::random_device{}()};
    size_t generatedCount_=0;
};
}''',
'''#include "nexus/utility/testing/test_data_generator.h"
namespace nexus::utility::testing {
TestDataGenerator& TestDataGenerator::instance(){static TestDataGenerator i;return i;}
void TestDataGenerator::initialize(){enabled_=true;generatedCount_=0;}
void TestDataGenerator::shutdown(){enabled_=false;}
bool TestDataGenerator::isEnabled()const{return enabled_;}
std::string TestDataGenerator::randomString(size_t len){static const char ch[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";std::uniform_int_distribution<size_t> d(0,sizeof(ch)-2);std::string r;for(size_t i=0;i<len;i++)r+=ch[d(rng_)];generatedCount_++;return r;}
int64_t TestDataGenerator::randomInt(int64_t mn,int64_t mx){generatedCount_++;return std::uniform_int_distribution<int64_t>(mn,mx)(rng_);}
double TestDataGenerator::randomDouble(double mn,double mx){generatedCount_++;return std::uniform_real_distribution<double>(mn,mx)(rng_);}
std::vector<uint8_t> TestDataGenerator::randomBytes(size_t n){std::vector<uint8_t> r(n);std::uniform_int_distribution<int> d(0,255);for(auto&b:r)b=d(rng_);generatedCount_++;return r;}
std::string TestDataGenerator::randomEmail(){generatedCount_++;return randomString(8)+"@"+randomString(6)+".com";}
std::string TestDataGenerator::randomUrl(){generatedCount_++;return "https://"+randomString(10)+".com/"+randomString(6);}
size_t TestDataGenerator::getGeneratedCount()const{return generatedCount_;}
void TestDataGenerator::clear(){generatedCount_=0;}
}''')

gen('testing','test_fixture_base',
'''#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <functional>
namespace nexus::utility::testing {
class TestFixtureBase {
public:
    struct TestResult { std::string name; bool passed; std::chrono::microseconds duration{0}; std::string error; };
    using TestFn = std::function<void()>;
    static TestFixtureBase& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerTest(const std::string& suite, const std::string& name, TestFn fn);
    void registerSetup(const std::string& suite, TestFn fn);
    void registerTeardown(const std::string& suite, TestFn fn);
    std::vector<TestResult> runSuite(const std::string& suite);
    std::vector<TestResult> runAll();
    size_t getPassedCount() const;
    size_t getFailedCount() const;
    std::vector<TestResult> getResults() const;
    void clear();
private:
    TestFixtureBase()=default; ~TestFixtureBase()=default; bool enabled_=false;
    struct Suite { std::vector<std::pair<std::string,TestFn>> tests; TestFn setup; TestFn teardown; };
    std::unordered_map<std::string,Suite> suites_;
    std::vector<TestResult> results_;
};
}''',
'''#include "nexus/utility/testing/test_fixture_base.h"
namespace nexus::utility::testing {
TestFixtureBase& TestFixtureBase::instance(){static TestFixtureBase i;return i;}
void TestFixtureBase::initialize(){enabled_=true;suites_.clear();results_.clear();}
void TestFixtureBase::shutdown(){enabled_=false;}
bool TestFixtureBase::isEnabled()const{return enabled_;}
void TestFixtureBase::registerTest(const std::string& s,const std::string& n,TestFn f){if(f)suites_[s].tests.emplace_back(n,f);}
void TestFixtureBase::registerSetup(const std::string& s,TestFn f){if(f)suites_[s].setup=f;}
void TestFixtureBase::registerTeardown(const std::string& s,TestFn f){if(f)suites_[s].teardown=f;}
auto TestFixtureBase::runSuite(const std::string& s)->std::vector<TestResult>{
    std::vector<TestResult> r;auto it=suites_.find(s);if(it==suites_.end())return r;
    for(auto&[name,fn]:it->second.tests){
        auto start=std::chrono::steady_clock::now();TestResult tr{name,true,std::chrono::microseconds(0),""};
        try{if(it->second.setup)it->second.setup();fn();
            if(it->second.teardown)it->second.teardown();}
        catch(std::exception& e){tr.passed=false;tr.error=e.what();}
        tr.duration=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start);
        r.push_back(tr);results_.push_back(tr);
    }
    return r;
}
auto TestFixtureBase::runAll()->std::vector<TestResult>{std::vector<TestResult> r;for(auto&[n,_]:suites_){auto s=runSuite(n);r.insert(r.end(),s.begin(),s.end());}return r;}
size_t TestFixtureBase::getPassedCount()const{size_t c=0;for(auto&r:results_)if(r.passed)c++;return c;}
size_t TestFixtureBase::getFailedCount()const{size_t c=0;for(auto&r:results_)if(!r.passed)c++;return c;}
auto TestFixtureBase::getResults()const->std::vector<TestResult>{return results_;}
void TestFixtureBase::clear(){suites_.clear();results_.clear();}
}''')

gen('testing','test_timeout',
'''#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <functional>
namespace nexus::utility::testing {
class TestTimeout {
public:
    struct TimeoutResult { std::string testName; std::chrono::milliseconds timeout; std::chrono::milliseconds actualDuration; bool timedOut; };
    static TestTimeout& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    TimeoutResult runWithTimeout(const std::string& name, std::function<void()> test, std::chrono::milliseconds timeout=std::chrono::milliseconds(5000));
    std::vector<TimeoutResult> getResults() const;
    size_t getTimeoutCount() const;
    void setDefaultTimeout(std::chrono::milliseconds timeout);
    void clear();
private:
    TestTimeout()=default; ~TestTimeout()=default; bool enabled_=false;
    std::chrono::milliseconds defaultTimeout_{5000};
    std::vector<TimeoutResult> results_;
};
}''',
'''#include "nexus/utility/testing/test_timeout.h"
#include <thread>
#include <future>
namespace nexus::utility::testing {
TestTimeout& TestTimeout::instance(){static TestTimeout i;return i;}
void TestTimeout::initialize(){enabled_=true;results_.clear();}
void TestTimeout::shutdown(){enabled_=false;}
bool TestTimeout::isEnabled()const{return enabled_;}
TestTimeout::TimeoutResult TestTimeout::runWithTimeout(const std::string& name,std::function<void()> test,std::chrono::milliseconds timeout){
    TimeoutResult r{name,timeout,std::chrono::milliseconds(0),false};
    if(!test)return r;
    auto start=std::chrono::steady_clock::now();
    auto future=std::async(std::launch::async,test);
    if(future.wait_for(timeout)==std::future_status::timeout){r.timedOut=true;future.wait();}
    r.actualDuration=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start);
    results_.push_back(r);return r;
}
auto TestTimeout::getResults()const->std::vector<TimeoutResult>{return results_;}
size_t TestTimeout::getTimeoutCount()const{size_t c=0;for(auto&r:results_)if(r.timedOut)c++;return c;}
void TestTimeout::setDefaultTimeout(std::chrono::milliseconds t){defaultTimeout_=t;}
void TestTimeout::clear(){results_.clear();}
}''')

print("Testing (8) done!")
