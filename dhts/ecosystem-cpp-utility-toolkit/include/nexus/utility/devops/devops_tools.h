#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <map>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstdint>
#include <set>

namespace nexus::utility::devops {

class DeploymentCanaryAnalyzer {
public:
    struct Version { std::string name; double error_rate, latency_p99; int request_count; double cpu_usage; };
    struct CanaryResult { bool promote = true; double error_increase_pct = 0; double latency_increase_pct = 0; std::vector<std::string> blocking_issues; };

    static CanaryResult analyze(const Version& baseline, const Version& canary, double max_error_increase=0.5, double max_latency_increase=0.2) {
        CanaryResult cr;
        if(baseline.request_count>0) {
            cr.error_increase_pct = (canary.error_rate - baseline.error_rate)/(baseline.error_rate+0.01)*100;
            cr.latency_increase_pct = (canary.latency_p99 - baseline.latency_p99)/baseline.latency_p99*100;
        }
        if(cr.error_increase_pct > max_error_increase*100) { cr.promote=false; cr.blocking_issues.push_back("Error rate increased "+std::to_string(cr.error_increase_pct)+"%"); }
        if(cr.latency_increase_pct > max_latency_increase*100) { cr.promote=false; cr.blocking_issues.push_back("Latency increased "+std::to_string(cr.latency_increase_pct)+"%"); }
        return cr;
    }
    static std::string report(const CanaryResult& cr) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(2);
        oss<<"═══ Canary Analysis ═══\n";
        oss<<"  Promote: "<<(cr.promote?"✓":"⚠ BLOCKED")<<"\n";
        oss<<"  Error Δ:  "<<cr.error_increase_pct<<"% | Latency Δ: "<<cr.latency_increase_pct<<"%\n";
        for(auto& i:cr.blocking_issues) oss<<"  ⚠ "<<i<<"\n";
        return oss.str();
    }
};

class ArtifactIntegrityVerifier {
public:
    struct Artifact { std::string path; uint64_t size; std::string sha256; std::string signature; };
    struct VerificationResult { bool valid=true; bool size_match=true, hash_match=true, sig_valid=true; std::vector<std::string> errors; };

    static VerificationResult verify(const Artifact& expected, const Artifact& actual) {
        VerificationResult vr;
        if(expected.size != actual.size) { vr.size_match=false; vr.errors.push_back("Size mismatch"); }
        if(expected.sha256 != actual.sha256) { vr.hash_match=false; vr.errors.push_back("Hash mismatch"); }
        vr.valid = vr.size_match && vr.hash_match;
        return vr;
    }
    static std::string report(const VerificationResult& vr) {
        std::ostringstream oss;
        oss<<"═══ Artifact Integrity ═══\nValid: "<<(vr.valid?"✓":"⚠")<<"\n";
        oss<<"  Size: "<<(vr.size_match?"✓":"⚠")<<" | Hash: "<<(vr.hash_match?"✓":"⚠")<<" | Sig: "<<(vr.sig_valid?"✓":"⚠")<<"\n";
        return oss.str();
    }
};

class PipelineStageTimer {
public:
    struct Stage { std::string name; double duration_sec; bool success; };
    struct PipelineStats { double total_duration=0; std::string slowest_stage; double avg_duration=0; size_t failed_stages=0; };

    static PipelineStats analyze(const std::vector<Stage>& stages) {
        PipelineStats ps;
        ps.total_duration=0; double max_dur=0;
        for(auto& s:stages) { ps.total_duration+=s.duration_sec; if(!s.success) ps.failed_stages++; if(s.duration_sec>max_dur){max_dur=s.duration_sec;ps.slowest_stage=s.name;} }
        ps.avg_duration = stages.size()>0 ? ps.total_duration/stages.size() : 0;
        return ps;
    }
    static std::string report(const PipelineStats& ps) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(1);
        oss<<"═══ Pipeline Timing ═══\nTotal: "<<ps.total_duration<<"s | Failed: "<<ps.failed_stages<<"\nSlowest: "<<ps.slowest_stage<<"\n";
        return oss.str();
    }
};

class ConfigDriftDetector {
public:
    using ConfigMap = std::map<std::string,std::string>;
    struct DriftResult { size_t total_keys=0, different_keys=0, missing_keys=0, extra_keys=0; double drift_pct=0; };
    static DriftResult detect(const ConfigMap& baseline, const ConfigMap& current) {
        DriftResult dr;
        std::set<std::string> all_keys;
        for(auto&[k,_]:baseline) all_keys.insert(k);
        for(auto&[k,_]:current) all_keys.insert(k);
        dr.total_keys=all_keys.size();
        for(auto& k:all_keys) {
            bool in_b=baseline.count(k), in_c=current.count(k);
            if(!in_b) dr.extra_keys++;
            else if(!in_c) dr.missing_keys++;
            else if(baseline.at(k)!=current.at(k)) dr.different_keys++;
        }
        dr.drift_pct = dr.total_keys>0 ? (dr.different_keys+dr.missing_keys+dr.extra_keys)*100.0/dr.total_keys : 0;
        return dr;
    }
    static std::string report(const DriftResult& dr) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(1);
        oss<<"═══ Config Drift ═══\n";
        oss<<"  Drift: "<<dr.drift_pct<<"%" << (dr.drift_pct>5?" ⚠":" ✓")<<"\n";
        oss<<"  Diff:"<<dr.different_keys<<" | Missing:"<<dr.missing_keys<<" | Extra:"<<dr.extra_keys<<"\n";
        return oss.str();
    }
};

class DependencyFreshnessTracker {
public:
    struct Dependency { std::string name, current_version, latest_version; int days_since_update; bool has_cve; };
    struct FreshnessReport { size_t outdated=0, critical=0; double freshness_score=0; std::vector<std::string> needs_update; };
    static FreshnessReport check(const std::vector<Dependency>& deps, int max_days=365) {
        FreshnessReport fr;
        int total_age=0;
        for(auto& d:deps) {
            if(d.current_version!=d.latest_version){fr.outdated++;fr.needs_update.push_back(d.name);}
            if(d.has_cve) fr.critical++;
            total_age+=d.days_since_update;
        }
        fr.freshness_score = deps.size()>0 ? 1.0-static_cast<double>(fr.outdated)/deps.size() : 1;
        return fr;
    }
    static std::string report(const FreshnessReport& fr) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(1);
        oss<<"═══ Dependency Freshness ═══\n";
        oss<<"  Score: "<<(fr.freshness_score*100)<<"% | Outdated: "<<fr.outdated<<" | CVEs: "<<fr.critical<<(fr.critical>0?" ⚠":" ✓")<<"\n";
        return oss.str();
    }
};

} // namespace nexus::utility::devops
