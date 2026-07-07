#pragma once

#include <random>
#include <concepts>
#include <mutex>
#include <array>

namespace nexus::utility::math {

/// @brief Thread-safe random number generator wrapper with proper seeding
class RandomGenerator {
public:
    template <std::integral T>
    [[nodiscard]] static T nextInt(T min, T max) noexcept {
        return getDistribution<std::uniform_int_distribution<T>>(min, max)(getEngine());
    }

    template <std::floating_point T>
    [[nodiscard]] static T nextDouble(T min, T max) noexcept {
        return getDistribution<std::uniform_real_distribution<T>>(min, max)(getEngine());
    }

    [[nodiscard]] static bool nextBool() noexcept {
        return nextInt(0, 1) == 1;
    }

    /// @brief Re-seed the thread-local engine with fresh entropy
    static void reseed() noexcept {
        auto& engine = getEngine();
        std::array<std::seed_seq::result_type, 8> seeds{};
        for (auto& s : seeds) {
            s = static_cast<std::seed_seq::result_type>(std::random_device{}());
        }
        std::seed_seq seq(seeds.begin(), seeds.end());
        engine.seed(seq);
    }

private:
    using Engine = std::mt19937_64;

    [[nodiscard]] static Engine& getEngine() noexcept {
        static thread_local Engine engine = makeSeededEngine();
        return engine;
    }

    [[nodiscard]] static Engine makeSeededEngine() noexcept {
        std::array<std::seed_seq::result_type, 8> seeds{};
        for (auto& s : seeds) {
            s = static_cast<std::seed_seq::result_type>(std::random_device{}());
        }
        std::seed_seq seq(seeds.begin(), seeds.end());
        return Engine(seq);
    }

    template <typename Dist, typename T>
    [[nodiscard]] static Dist getDistribution(T min, T max) noexcept {
        return Dist(min, max);
    }
};

} // namespace nexus::utility::math
