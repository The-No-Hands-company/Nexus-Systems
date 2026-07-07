#pragma once
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
} // namespace nexus::utility::testing