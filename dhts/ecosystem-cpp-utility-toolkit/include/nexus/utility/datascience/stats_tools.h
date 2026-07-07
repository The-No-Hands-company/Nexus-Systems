#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <map>
#include <random>
#include <functional>

namespace nexus::utility::datascience {

class OutlierDetector {
public:
    enum Method { IQR, ZScore, MAD };
    struct OutlierResult { std::vector<size_t> outlier_indices; double threshold; size_t total; double pct; };

    static OutlierResult detect(const std::vector<double>& data, Method m=IQR) {
        OutlierResult r; r.total=data.size();
        std::vector<double> sorted=data; std::sort(sorted.begin(),sorted.end());
        if(m==IQR) {
            double q1=sorted[sorted.size()/4], q3=sorted[sorted.size()*3/4], iqr=q3-q1;
            r.threshold=iqr*1.5;
            for(size_t i=0;i<data.size();++i) if(data[i]<q1-r.threshold||data[i]>q3+r.threshold) r.outlier_indices.push_back(i);
        } else if(m==ZScore) {
            double mean=std::accumulate(data.begin(),data.end(),0.0)/data.size();
            double sd=0; for(auto v:data) sd+=(v-mean)*(v-mean); sd=std::sqrt(sd/data.size());
            r.threshold=3.0*sd;
            for(size_t i=0;i<data.size();++i) if(std::abs(data[i]-mean)>r.threshold) r.outlier_indices.push_back(i);
        } else {
            double med=sorted[sorted.size()/2];
            std::vector<double> adev; for(auto v:data) adev.push_back(std::abs(v-med));
            std::sort(adev.begin(),adev.end());
            double mad=adev[adev.size()/2];
            r.threshold=mad*3;
            for(size_t i=0;i<data.size();++i) if(std::abs(data[i]-med)>r.threshold) r.outlier_indices.push_back(i);
        }
        r.pct=r.outlier_indices.size()*100.0/r.total;
        return r;
    }
    static std::string report(const OutlierResult& r) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(2);
        oss<<"═══ Outlier Detection ═══\n";
        oss<<"  Outliers: "<<r.outlier_indices.size()<<" / "<<r.total<<" ("<<r.pct<<"%)\n";
        return oss.str();
    }
};

class CorrelationMatrixAnalyzer {
public:
    struct CorrMatrix { std::vector<std::string> var_names; std::vector<std::vector<double>> matrix; };

    static CorrMatrix compute(const std::vector<std::string>& names, const std::vector<std::vector<double>>& data) {
        CorrMatrix cm; cm.var_names=names;
        size_t n=names.size(); cm.matrix.resize(n,std::vector<double>(n,0));
        for(size_t i=0;i<n;++i)
            for(size_t j=0;j<n;++j)
                cm.matrix[i][j]=pearsonCorrelation(data[i],data[j]);
        return cm;
    }

    static std::vector<std::pair<std::string,std::string>> findStrongCorrelations(const CorrMatrix& cm, double threshold=0.8) {
        std::vector<std::pair<std::string,std::string>> sc;
        for(size_t i=0;i<cm.matrix.size();++i)
            for(size_t j=i+1;j<cm.matrix.size();++j)
                if(std::abs(cm.matrix[i][j])>threshold)
                    sc.push_back({cm.var_names[i],cm.var_names[j]});
        return sc;
    }

    static std::string report(const CorrMatrix& cm) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(2);
        oss<<"═══ Correlation Matrix ═══\n";
        auto strong=findStrongCorrelations(cm);
        for(auto&[a,b]:strong) {
            double r=pearsonCorrelation({},{});
            for(size_t i=0;i<cm.var_names.size();++i) if(cm.var_names[i]==a) for(size_t j=0;j<cm.var_names.size();++j) if(cm.var_names[j]==b) r=cm.matrix[i][j];
            oss<<"  "<<a<<" ↔ "<<b<<": r="<<r<<(std::abs(r)>0.9?" ⚠ multicol":"")<<"\n";
        }
        return oss.str();
    }

private:
    static double pearsonCorrelation(const std::vector<double>& x, const std::vector<double>& y) {
        size_t n=std::min(x.size(),y.size()); if(n<2) return 0;
        double sx=0,sy=0,sxy=0,sx2=0,sy2=0;
        for(size_t i=0;i<n;++i){sx+=x[i];sy+=y[i];sxy+=x[i]*y[i];sx2+=x[i]*x[i];sy2+=y[i]*y[i];}
        double num=n*sxy-sx*sy, den=std::sqrt((n*sx2-sx*sx)*(n*sy2-sy*sy));
        return den>1e-10?num/den:0;
    }
};

class TimeSeriesAnomalyDetector {
public:
    struct AnomalyPoint { size_t index; double value, expected, deviation; };
    struct AnomalyResult { std::vector<AnomalyPoint> anomalies; double threshold; };

    static AnomalyResult detect(const std::vector<double>& series, int window=10, double sigma=3.0) {
        AnomalyResult ar;
        if(series.size()<(size_t)window) return ar;
        std::vector<double> ma(series.size());
        for(size_t i=window-1;i<series.size();++i) {
            double sum=0; for(size_t j=i-window+1;j<=i;++j) sum+=series[j];
            ma[i]=sum/window;
        }
        std::vector<double> residuals;
        for(size_t i=window-1;i<series.size();++i) residuals.push_back(series[i]-ma[i]);
        double rmean=std::accumulate(residuals.begin(),residuals.end(),0.0)/residuals.size();
        double rsd=0; for(auto v:residuals) rsd+=(v-rmean)*(v-rmean); rsd=std::sqrt(rsd/residuals.size());
        ar.threshold=sigma*rsd;
        for(size_t i=0;i<residuals.size();++i)
            if(std::abs(residuals[i])>ar.threshold)
                ar.anomalies.push_back({i+window-1,series[i+window-1],ma[i+window-1],std::abs(residuals[i])});
        return ar;
    }
    static std::string report(const AnomalyResult& ar) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(2);
        oss<<"═══ Time Series Anomalies ═══\n";
        oss<<"  Count: "<<ar.anomalies.size()<<" (threshold σ×3)\n";
        for(auto&a:ar.anomalies) oss<<"  t="<<a.index<<" val="<<a.value<<" exp="<<a.expected<<" dev="<<a.deviation<<"\n";
        return oss.str();
    }
};

class BootstrapSampler {
public:
    struct BootstrapResult { double mean, stddev, ci_lower, ci_upper; std::vector<double> means; };
    static BootstrapResult sample(const std::vector<double>& data, int n_bootstrap=1000, double ci=0.95) {
        BootstrapResult br;
        std::mt19937_64 rng(42);
        std::uniform_int_distribution<size_t> dist(0,data.size()-1);
        br.means.resize(n_bootstrap);
        for(int b=0;b<n_bootstrap;++b) {
            double sum=0; for(int i=0;i<(int)data.size();++i) sum+=data[dist(rng)];
            br.means[b]=sum/data.size();
        }
        double msum=0; for(auto v:br.means) msum+=v;
        br.mean=msum/n_bootstrap;
        double var=0; for(auto v:br.means) var+=(v-br.mean)*(v-br.mean);
        br.stddev=std::sqrt(var/(n_bootstrap-1));
        auto sorted=br.means; std::sort(sorted.begin(),sorted.end());
        double alpha=(1-ci)/2;
        br.ci_lower=sorted[(size_t)(alpha*n_bootstrap)];
        br.ci_upper=sorted[(size_t)((1-alpha)*n_bootstrap-1)];
        return br;
    }
    static std::string report(const BootstrapResult& br) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(4);
        oss<<"═══ Bootstrap ═══\nMean: "<<br.mean<<" ± "<<br.stddev<<"\nCI: ["<<br.ci_lower<<", "<<br.ci_upper<<"]\n";
        return oss.str();
    }
};

class FeatureImportanceRanker {
public:
    struct FeatureScore { std::string name; double importance; };
    static std::vector<FeatureScore> rankByVariance(const std::vector<std::string>& names, const std::vector<std::vector<double>>& features) {
        std::vector<FeatureScore> scores;
        for(size_t i=0;i<names.size();++i) {
            double mean=std::accumulate(features[i].begin(),features[i].end(),0.0)/features[i].size();
            double var=0; for(auto v:features[i]) var+=(v-mean)*(v-mean);
            scores.push_back({names[i],var/features[i].size()});
        }
        std::sort(scores.begin(),scores.end(),[](auto&a,auto&b){return a.importance>b.importance;});
        return scores;
    }
    static std::string report(const std::vector<FeatureScore>& ranks) {
        std::ostringstream oss; oss<<std::fixed<<std::setprecision(3);
        oss<<"═══ Feature Importance ═══\n";
        for(auto&f:ranks) oss<<"  "<<f.name<<": "<<f.importance<<"\n";
        return oss.str();
    }
};

} // namespace nexus::utility::datascience
