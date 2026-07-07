#include <gtest/gtest.h>
#include <cmath>

#include "nexus/utility/metrics/counter.h"
#include "nexus/utility/metrics/gauge.h"
#include "nexus/utility/metrics/histogram.h"
#include "nexus/utility/metrics/metrics_collector.h"

using namespace nexus::utility::metrics;

TEST(CounterTest, IncrementDecrement) {
    Counter c;
    EXPECT_EQ(c.value(), 0);
    c.inc();
    EXPECT_EQ(c.value(), 1);
    c.inc(5);
    EXPECT_EQ(c.value(), 6);
    c.dec(2);
    EXPECT_EQ(c.value(), 4);
}

TEST(CounterTest, SetAndReset) {
    Counter c;
    c.set(100);
    EXPECT_EQ(c.value(), 100);
    c.reset();
    EXPECT_EQ(c.value(), 0);
}

TEST(CounterTest, OperatorOverloads) {
    Counter c;
    ++c;
    EXPECT_EQ(c.value(), 1);
    c++;
    EXPECT_EQ(c.value(), 2);
}

TEST(LabeledCounterTest, LabeledOperations) {
    auto& lc = LabeledCounter::instance();
    lc.reset();

    lc.inc("requests", 10);
    lc.inc("errors", 1);
    lc.inc("requests", 5);

    EXPECT_EQ(lc.get("requests"), 15);
    EXPECT_EQ(lc.get("errors"), 1);
    EXPECT_EQ(lc.get("nonexistent"), 0);

    auto snap = lc.snapshot();
    EXPECT_EQ(snap.size(), 2);
    lc.reset();
}

TEST(GaugeTest, SetAndGet) {
    Gauge g;
    g.set(3.14);
    EXPECT_DOUBLE_EQ(g.value(), 3.14);
    EXPECT_DOUBLE_EQ(static_cast<double>(g), 3.14);
}

TEST(GaugeTest, IncrementDecrement) {
    Gauge g;
    g.set(10.0);
    g.inc(5.0);
    EXPECT_DOUBLE_EQ(g.value(), 15.0);
    g.dec(3.0);
    EXPECT_DOUBLE_EQ(g.value(), 12.0);
    g.reset();
    EXPECT_DOUBLE_EQ(g.value(), 0.0);
}

TEST(HistogramTest, BasicObservation) {
    Histogram h;
    h.configureLinear(0.0, 10.0, 5);

    h.observe(5.0);
    h.observe(15.0);
    h.observe(25.0);
    h.observe(35.0);

    EXPECT_EQ(h.count(), 4);
    auto snap = h.snapshot();
    EXPECT_EQ(snap.count, 4);
    EXPECT_GT(snap.sum, 0);
    EXPECT_LE(snap.min, snap.max);
    EXPECT_EQ(snap.bucket_bounds.size(), 5);
    EXPECT_EQ(snap.bucket_counts.size(), 6);
}

TEST(HistogramTest, Percentiles) {
    Histogram h;
    h.configureLinear(0.0, 1.0, 100);

    for (double v = 1.0; v <= 100.0; v += 1.0) {
        h.observe(v);
    }

    auto snap = h.snapshot();
    EXPECT_NEAR(snap.p50, 50.0, 5.0);
    EXPECT_NEAR(snap.p95, 95.0, 5.0);
    EXPECT_NEAR(snap.mean, 50.5, 1.0);
}

TEST(HistogramTest, ExponentialBuckets) {
    Histogram h;
    h.configureExponential(2.0, 0, 5);

    h.observe(1.0);
    h.observe(5.0);
    h.observe(20.0);

    auto snap = h.snapshot();
    EXPECT_EQ(snap.count, 3);
}

TEST(MetricsCollectorTest, Counters) {
    auto& mc = MetricsCollector::instance();
    mc.reset();

    mc.incCounter("http.requests", 1);
    mc.incCounter("http.requests", 2);
    mc.incCounter("http.errors", 1);

    EXPECT_EQ(mc.getCounter("http.requests"), 3);
    EXPECT_EQ(mc.getCounter("http.errors"), 1);
    EXPECT_EQ(mc.getCounter("unknown"), 0);
}

TEST(MetricsCollectorTest, Gauges) {
    auto& mc = MetricsCollector::instance();
    mc.reset();

    mc.setGauge("memory.used_mb", 512.5);
    mc.setGauge("cpu.percent", 75.0);

    EXPECT_DOUBLE_EQ(mc.getGauge("memory.used_mb"), 512.5);
    EXPECT_DOUBLE_EQ(mc.getGauge("cpu.percent"), 75.0);
}

TEST(MetricsCollectorTest, Histograms) {
    auto& mc = MetricsCollector::instance();
    mc.reset();

    mc.observeHistogram("request.latency_ms", 10.0);
    mc.observeHistogram("request.latency_ms", 20.0);

    auto snap = mc.getHistogramSnapshot("request.latency_ms");
    EXPECT_EQ(snap.count, 2);
    EXPECT_GT(snap.sum, 0);
}

TEST(MetricsCollectorTest, PrometheusExport) {
    auto& mc = MetricsCollector::instance();
    mc.reset();

    mc.incCounter("app.starts", 1);
    mc.setGauge("app.uptime", 3600.0);

    auto text = mc.exportPrometheus();
    EXPECT_TRUE(text.find("app.starts") != std::string::npos);
    EXPECT_TRUE(text.find("app.uptime") != std::string::npos);
    EXPECT_TRUE(text.find("# TYPE") != std::string::npos);
}
