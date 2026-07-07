#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>

#include "nexus/utility/statistics/anomaly_detector.h"
#include "nexus/utility/statistics/correlation_tracker.h"
#include "nexus/utility/statistics/distribution_analyzer.h"
#include "nexus/utility/statistics/variance_analyzer.h"
#include "nexus/utility/statistics/moving_average_tracker.h"

using namespace nexus::utility::statistics;

// ── AnomalyDetector ─────────────────────────────────────────────────────────

TEST(AnomalyDetectorTest, NoAnomaliesInNormalData) {
    auto& ad = AnomalyDetector::instance(); ad.initialize();
    for (int i = 0; i < 100; ++i) ad.addDataPoint(10.0);
    EXPECT_FALSE(ad.isAnomaly(10.5, 3.0));
    EXPECT_FALSE(ad.isAnomaly(9.5, 3.0));
}

TEST(AnomalyDetectorTest, DetectsOutlier) {
    auto& ad = AnomalyDetector::instance(); ad.initialize();
    for (int i = 0; i < 100; ++i) ad.addDataPoint(10.0);
    EXPECT_TRUE(ad.isAnomaly(100.0, 3.0));
}

TEST(AnomalyDetectorTest, MeanAndStdDev) {
    auto& ad = AnomalyDetector::instance(); ad.initialize();
    ad.addDataPoint(10.0);
    ad.addDataPoint(12.0);
    ad.addDataPoint(14.0);
    EXPECT_NEAR(ad.getMean(), 12.0, 0.01);
    EXPECT_GT(ad.getStdDev(), 0.0);
}

// ── CorrelationTracker ──────────────────────────────────────────────────────

TEST(CorrelationTrackerTest, PerfectPositive) {
    auto& ct = CorrelationTracker::instance(); ct.initialize();
    for (int i = 0; i < 10; ++i) ct.addDataPoint(static_cast<double>(i), static_cast<double>(i * 2));
    double r = ct.getCorrelation();
    EXPECT_NEAR(r, 1.0, 0.01);
    EXPECT_EQ(ct.getCount(), 10);
}

TEST(CorrelationTrackerTest, EmptyReturnsZero) {
    auto& ct = CorrelationTracker::instance(); ct.initialize();
    EXPECT_EQ(ct.getCorrelation(), 0.0);
    EXPECT_EQ(ct.getCount(), 0);
}

// ── DistributionAnalyzer ────────────────────────────────────────────────────

TEST(DistributionAnalyzerTest, BasicStats) {
    auto& da = DistributionAnalyzer::instance(); da.initialize();
    for (double v : {1.0, 2.0, 3.0, 4.0, 5.0}) da.addSample(v);
    EXPECT_DOUBLE_EQ(da.getMin(), 1.0);
    EXPECT_DOUBLE_EQ(da.getMax(), 5.0);
    EXPECT_NEAR(da.getMean(), 3.0, 0.01);
    EXPECT_NEAR(da.getMedian(), 3.0, 0.01);
    EXPECT_NEAR(da.getPercentile(0.5), 3.0, 0.01);
}

// ── VarianceAnalyzer ────────────────────────────────────────────────────────

TEST(VarianceAnalyzerTest, WelfordsAlgorithm) {
    auto& va = VarianceAnalyzer::instance(); va.initialize();
    for (double v : {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0}) va.addValue(v);
    EXPECT_NEAR(va.getMean(), 5.0, 0.01);
    EXPECT_GT(va.getVariance(), 0.0);
    EXPECT_GT(va.getStdDev(), 0.0);
    EXPECT_EQ(va.getCount(), 8);
}

// ── MovingAverageTracker ────────────────────────────────────────────────────

TEST(MovingAverageTrackerTest, SimpleMovingAverage) {
    auto& mat = MovingAverageTracker::instance(); mat.initialize();
    mat.setWindow(3);
    mat.addValue(1.0);
    mat.addValue(2.0);
    mat.addValue(3.0);
    EXPECT_NEAR(mat.getSMA(), 2.0, 0.01);
    mat.addValue(10.0);
    EXPECT_NEAR(mat.getSMA(), 5.0, 0.01);
}

TEST(MovingAverageTrackerTest, ExponentialMovingAverage) {
    auto& mat = MovingAverageTracker::instance(); mat.initialize();
    mat.addValue(10.0);
    mat.addValue(20.0);
    double ema = mat.getEMA(0.5);
    EXPECT_GT(ema, 10.0);
    EXPECT_LT(ema, 20.0);
}
