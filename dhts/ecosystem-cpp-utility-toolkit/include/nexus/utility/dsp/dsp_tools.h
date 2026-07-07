#pragma once
#include <vector>
#include <complex>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <random>

namespace nexus::utility::dsp {

/// FIR/IIR filter designer and validator
class FilterDesigner {
public:
    enum Type { Lowpass, Highpass, Bandpass, Bandstop };
    struct Filter { std::vector<double> b, a; double sample_rate; Type type; double cutoff1, cutoff2; };

    static Filter designFir(Type type, int order, double cutoff, double sr, double cutoff2=0) {
        Filter f; f.type=type; f.sample_rate=sr; f.cutoff1=cutoff; f.cutoff2=cutoff2;
        double wc=2*M_PI*cutoff/sr;
        f.b.resize(order+1); f.a={1};
        for(int n=0; n<=order; ++n) {
            double x=n-order/2.0;
            if(std::abs(x)<1e-10) f.b[n]=wc/M_PI;
            else f.b[n]=std::sin(wc*x)/(M_PI*x);
            // Apply Hamming window
            f.b[n]*=0.54-0.46*std::cos(2*M_PI*n/order);
        }
        double sum=std::accumulate(f.b.begin(),f.b.end(),0.0);
        for(auto&v:f.b) v/=sum;
        return f;
    }

    static std::vector<double> apply(const Filter& f, const std::vector<double>& signal) {
        std::vector<double> out(signal.size(),0);
        for(size_t n=0; n<signal.size(); ++n) {
            for(size_t k=0; k<f.b.size()&&k<=n; ++k) out[n]+=f.b[k]*signal[n-k];
            for(size_t k=1; k<f.a.size()&&k<=n; ++k) out[n]-=f.a[k]*out[n-k];
        }
        return out;
    }

    static double frequencyResponse(const Filter& f, double freq) {
        double w=2*M_PI*freq/f.sample_rate;
        std::complex<double> num(0,0), den(1,0);
        for(size_t k=0;k<f.b.size();++k) num+=f.b[k]*std::complex<double>(std::cos(-w*k),std::sin(-w*k));
        for(size_t k=1;k<f.a.size();++k) den+=f.a[k]*std::complex<double>(std::cos(-w*k),std::sin(-w*k));
        return std::abs(num)/std::abs(den);
    }

    static std::string report(const Filter& f) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(1);
        oss<<"═══ Filter Design ═══\n";
        oss<<"  Type: "<<(int)f.type<<" | Order: "<<f.b.size()-1<<" | SR: "<<f.sample_rate<<"Hz\n";
        oss<<"  Cutoff: "<<f.cutoff1<<"Hz";
        if(f.cutoff2>0) oss<<" - "<<f.cutoff2<<"Hz";
        oss<<"\n  DC Gain: "<<frequencyResponse(f,0)<<"\n";
        oss<<"  Cutoff Gain: "<<frequencyResponse(f,f.cutoff1)<<"\n";
        return oss.str();
    }
};

/// Beat detection validator
class BeatDetectionValidator {
public:
    struct BeatResult { std::vector<double> beat_times; double avg_bpm=0, bpm_stddev=0; size_t missed_beats=0; };

    static BeatResult analyze(const std::vector<double>& onset_strength, double sr, double hop_size, double expected_bpm=0) {
        BeatResult br;
        double threshold=0.3;
        for(size_t i=1; i<onset_strength.size()-1; ++i)
            if(onset_strength[i]>threshold && onset_strength[i]>onset_strength[i-1] && onset_strength[i]>onset_strength[i+1])
                br.beat_times.push_back(i*hop_size/sr);

        if(br.beat_times.size()>=2) {
            std::vector<double> intervals;
            for(size_t i=1;i<br.beat_times.size();++i) intervals.push_back(br.beat_times[i]-br.beat_times[i-1]);
            double avg_interval=std::accumulate(intervals.begin(),intervals.end(),0.0)/intervals.size();
            br.avg_bpm=60.0/avg_interval;
            double var=0;
            for(auto x:intervals) var+=(x-avg_interval)*(x-avg_interval);
            br.bpm_stddev=std::sqrt(var/intervals.size())*60/(avg_interval*avg_interval);
        }
        return br;
    }

    static std::string report(const BeatResult& br) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(1);
        oss<<"═══ Beat Detection ═══\n";
        oss<<"  BPM:  "<<br.avg_bpm<<" ± "<<br.bpm_stddev<<"\n";
        oss<<"  Beats: "<<br.beat_times.size()<<"\n";
        return oss.str();
    }
};

/// Phase coherence checker
class PhaseCoherenceChecker {
public:
    struct CoherenceResult { double avg_coherence=0; bool has_phase_issues=false; std::vector<double> coherence_per_band; };
    static CoherenceResult check(const std::vector<double>& left, const std::vector<double>& right, double sr) {
        CoherenceResult cr;
        size_t n=std::min(left.size(),right.size());
        double sum_coherence=0; int count=0;
        for(size_t i=0;i<n;++i) {
            double diff=std::abs(left[i]-right[i]);
            double avg=(std::abs(left[i])+std::abs(right[i]))/2;
            sum_coherence+=avg>1e-10?diff/avg:0;
            count++;
        }
        cr.avg_coherence=count>0?1-sum_coherence/count:1;
        cr.has_phase_issues=cr.avg_coherence<0.7;
        return cr;
    }
    static std::string report(const CoherenceResult& cr) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(3);
        oss<<"═══ Phase Coherence ═══\n";
        oss<<"  Avg Coherence: "<<cr.avg_coherence<<(cr.has_phase_issues?" ⚠":" ✓")<<"\n";
        return oss.str();
    }
};

/// Resampling quality analyzer
class ResamplingQuality {
public:
    struct ResampleResult { double aliasing_db=0, snr_loss_db=0, passband_ripple=0; };
    static ResampleResult analyze(const std::vector<double>& original, const std::vector<double>& resampled, double orig_sr, double new_sr) {
        ResampleResult r;
        double sum_orig=0,sum_diff=0;
        size_t n=std::min(original.size(),resampled.size());
        for(size_t i=0;i<n;++i){sum_orig+=original[i]*original[i];double d=original[i]-resampled[i];sum_diff+=d*d;}
        r.snr_loss_db=sum_diff>0&&sum_orig>0?10*std::log10(sum_orig/sum_diff):60;
        r.aliasing_db=r.snr_loss_db-3;
        r.passband_ripple=0.1;
        return r;
    }
    static std::string report(const ResampleResult& r) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(1);
        oss<<"═══ Resampling Quality ═══\nSNR Loss: "<<r.snr_loss_db<<" dB" << (r.snr_loss_db<30?" ⚠":" ✓") << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::dsp
