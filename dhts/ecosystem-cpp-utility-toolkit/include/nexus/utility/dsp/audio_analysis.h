#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>
#include <random>

namespace nexus::utility::dsp {

/// FFT spectrum analyzer with window functions
class FftSpectrumAnalyzer {
public:
    enum Window { Rectangular, Hann, Hamming, Blackman };
    struct Spectrum { std::vector<double> magnitudes, phases, frequencies; double sample_rate; };
    struct Peak { double frequency, magnitude, prominence; };

    static Spectrum analyze(const std::vector<double>& signal, double sample_rate, Window w=Window::Hann) {
        auto n = signal.size();
        std::vector<double> windowed = applyWindow(signal, w);
        auto complex = realFFT(windowed);
        Spectrum s;
        s.sample_rate = sample_rate;
        s.magnitudes.resize(n/2); s.phases.resize(n/2); s.frequencies.resize(n/2);
        for (size_t i=0; i<n/2; ++i) {
            double re=complex[i*2], im=complex[i*2+1];
            s.magnitudes[i]=2*std::sqrt(re*re+im*im)/n;
            s.phases[i]=std::atan2(im,re);
            s.frequencies[i]=i*sample_rate/n;
        }
        return s;
    }

    static std::vector<Peak> findPeaks(const Spectrum& s, double min_mag=0.01, int max_peaks=10) {
        std::vector<Peak> peaks;
        for(size_t i=1; i<s.magnitudes.size()-1; ++i)
            if(s.magnitudes[i]>min_mag && s.magnitudes[i]>s.magnitudes[i-1] && s.magnitudes[i]>s.magnitudes[i+1])
                peaks.push_back({s.frequencies[i], s.magnitudes[i], 0});
        std::sort(peaks.begin(), peaks.end(), [](auto&a,auto&b){return a.magnitude>b.magnitude;});
        if(peaks.size()>(size_t)max_peaks) peaks.resize(max_peaks);
        return peaks;
    }

    static std::string report(const Spectrum& s) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(2);
        oss<<"═══ FFT Spectrum ═══\nSR: "<<s.sample_rate<<"Hz | Bins: "<<s.magnitudes.size()<<"\n";
        auto peaks=findPeaks(s);
        for(auto&p:peaks) oss<<"  Peak: "<<p.frequency<<" Hz (mag "<<p.magnitude<<")\n";
        return oss.str();
    }

    static std::vector<double> generateSine(double freq, double sr, size_t n, double amp=1) {
        std::vector<double> sig(n);
        for(size_t i=0;i<n;++i) sig[i]=amp*std::sin(2*M_PI*freq*i/sr);
        return sig;
    }

private:
    static std::vector<double> applyWindow(const std::vector<double>& s, Window w) {
        std::vector<double> r=s; auto n=s.size();
        for(size_t i=0;i<n;++i) {
            double coeff=1;
            switch(w) {
                case Hann: coeff=0.5*(1-std::cos(2*M_PI*i/(n-1))); break;
                case Hamming: coeff=0.54-0.46*std::cos(2*M_PI*i/(n-1)); break;
                case Blackman: coeff=0.42-0.5*std::cos(2*M_PI*i/(n-1))+0.08*std::cos(4*M_PI*i/(n-1)); break;
                default: break;
            }
            r[i]*=coeff;
        }
        return r;
    }

    static std::vector<double> realFFT(const std::vector<double>& x) {
        size_t n=x.size();
        std::vector<std::complex<double>> cx(n);
        for(size_t i=0;i<n;++i) cx[i]=std::complex<double>(x[i],0);
        // Simple DFT for portability
        std::vector<double> result(n*2);
        for(size_t k=0;k<n;++k) {
            std::complex<double> sum(0,0);
            for(size_t j=0;j<n;++j) {
                double angle=-2*M_PI*k*j/n;
                sum+=cx[j]*std::complex<double>(std::cos(angle),std::sin(angle));
            }
            result[k*2]=sum.real(); result[k*2+1]=sum.imag();
        }
        return result;
    }
};

/// Audio quality metrics (THD, SNR, SINAD)
class AudioQualityMetrics {
public:
    struct QualityResult { double thd_pct=0, snr_db=0, sinad_db=0, rms=0, peak=0, crest_factor=0; };

    static QualityResult measure(const std::vector<double>& signal, double fundamental_freq, double sample_rate) {
        QualityResult qr;
        double sum_sq=0, peak=0;
        for(auto v:signal){sum_sq+=v*v;peak=std::max(peak,std::abs(v));}
        qr.rms=std::sqrt(sum_sq/signal.size());
        qr.peak=peak;
        qr.crest_factor=qr.rms>0?peak/qr.rms:0;

        auto spec=FftSpectrumAnalyzer::analyze(signal, sample_rate);
        double fund_mag=0, harmonic_sum=0;
        for(size_t i=0;i<spec.frequencies.size();++i) {
            double f=spec.frequencies[i];
            bool is_harmonic=false;
            for(int h=1;h<=10;++h) if(std::abs(f-h*fundamental_freq)<1){is_harmonic=true;if(h==1)fund_mag=spec.magnitudes[i];else harmonic_sum+=spec.magnitudes[i]*spec.magnitudes[i];break;}
        }
        qr.thd_pct=fund_mag>0?std::sqrt(harmonic_sum)/fund_mag*100:0;
        qr.snr_db=fund_mag>0?20*std::log10(fund_mag/qr.rms):0;
        qr.sinad_db=qr.snr_db;

        return qr;
    }

    static std::string report(const QualityResult& qr) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(1);
        oss<<"═══ Audio Quality ═══\n";
        oss<<"  THD:    "<<qr.thd_pct<<"%" << (qr.thd_pct>1?" ⚠":" ✓")<<"\n";
        oss<<"  SNR:    "<<qr.snr_db<<" dB\n";
        oss<<"  RMS:    "<<qr.rms<<" | Crest: "<<qr.crest_factor<<"\n";
        return oss.str();
    }
};

/// Oscillator drift detector
class OscillatorDriftDetector {
public:
    struct DriftResult { double freq_drift_hz_per_sec=0, phase_drift_rad_per_sec=0; bool is_stable=true; };
    static DriftResult detect(const std::vector<double>& signal, double expected_freq, double sr) {
        DriftResult dr;
        auto spec=FftSpectrumAnalyzer::analyze(signal, sr);
        auto f0=spec.frequencies[0]; auto df=spec.sample_rate/spec.frequencies.size();
        double measured=0, max_mag=0;
        int expected_bin=(int)(expected_freq/df);
        for(int i=std::max(0,expected_bin-5);i<std::min((int)spec.magnitudes.size(),expected_bin+5);++i)
            if(spec.magnitudes[i]>max_mag){max_mag=spec.magnitudes[i];measured=spec.frequencies[i];}
        dr.freq_drift_hz_per_sec=(measured-expected_freq);
        dr.is_stable=std::abs(dr.freq_drift_hz_per_sec)<1.0;
        return dr;
    }
    static std::string report(const DriftResult& dr) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(2);
        oss<<"═══ Oscillator Drift ═══\n";
        oss<<"  Freq Drift: "<<dr.freq_drift_hz_per_sec<<" Hz/s" << (dr.is_stable?" ✓":" ⚠")<<"\n";
        return oss.str();
    }
};

/// Loudness meter (LUFS approximation)
class LoudnessMeter {
public:
    struct LoudnessResult { double integrated_lufs=0, short_term_lufs=0, peak_db=0, dynamic_range_db=0; };
    static LoudnessResult measure(const std::vector<double>& signal, double sr) {
        LoudnessResult lr;
        double sum_sq=0, peak=0;
        for(auto v:signal){sum_sq+=v*v;peak=std::max(peak,std::abs(v));}
        double rms=std::sqrt(sum_sq/signal.size());
        lr.integrated_lufs=rms>0?-0.691+10*std::log10(rms*rms):-70;
        lr.peak_db=peak>0?20*std::log10(peak):-70;
        lr.dynamic_range_db=lr.peak_db-lr.integrated_lufs;

        size_t window=sr*3; double st_sum=0;
        for(size_t i=std::max(size_t(0),signal.size()-window);i<signal.size();++i) st_sum+=signal[i]*signal[i];
        double st_rms=std::sqrt(st_sum/window);
        lr.short_term_lufs=st_rms>0?-0.691+10*std::log10(st_rms*st_rms):-70;
        return lr;
    }
    static std::string report(const LoudnessResult& lr) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(1);
        oss<<"═══ Loudness ═══\n";
        oss<<"  Integrated: "<<lr.integrated_lufs<<" LUFS\n";
        oss<<"  Short-term: "<<lr.short_term_lufs<<" LUFS\n";
        oss<<"  Peak:       "<<lr.peak_db<<" dB | DR: "<<lr.dynamic_range_db<<" dB\n";
        return oss.str();
    }
};

} // namespace nexus::utility::dsp
