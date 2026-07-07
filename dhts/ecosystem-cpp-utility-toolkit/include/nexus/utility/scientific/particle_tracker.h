#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <cmath>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::scientific {

/**
 * @brief Track particles with positions and velocities in an N-body / PIC simulation.
 */
class ParticleTracker {
public:
    struct Particle {
        std::uint64_t id = 0;
        std::array<double, 3> position{0, 0, 0};
        std::array<double, 3> velocity{0, 0, 0};
        double mass = 1.0;
    };

    static ParticleTracker& instance() {
        static ParticleTracker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    std::uint64_t spawn(const std::array<double, 3>& pos,
                        const std::array<double, 3>& vel,
                        double mass = 1.0) {
        std::lock_guard<std::mutex> lk(mutex_);
        std::uint64_t id = ++nextId_;
        particles_.push_back(Particle{id, pos, vel, mass});
        idIndex_[id] = particles_.size() - 1;
        return id;
    }

    bool update(std::uint64_t id, const std::array<double, 3>& pos,
                const std::array<double, 3>& vel) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = idIndex_.find(id);
        if (it == idIndex_.end()) return false;
        particles_[it->second].position = pos;
        particles_[it->second].velocity = vel;
        return true;
    }

    /// Advance all particles by dt using their current velocities (explicit Euler).
    void step(double dt) {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& p : particles_)
            for (int i = 0; i < 3; ++i)
                p.position[i] += p.velocity[i] * dt;
    }

    double totalKineticEnergy() const {
        std::lock_guard<std::mutex> lk(mutex_);
        double e = 0.0;
        for (const auto& p : particles_) {
            double v2 = p.velocity[0]*p.velocity[0] + p.velocity[1]*p.velocity[1] +
                        p.velocity[2]*p.velocity[2];
            e += 0.5 * p.mass * v2;
        }
        return e;
    }

    std::size_t count() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return particles_.size();
    }

    std::vector<Particle> particles() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return particles_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        particles_.clear();
        idIndex_.clear();
        nextId_ = 0;
    }

private:
    ParticleTracker() = default;
    ~ParticleTracker() = default;
    ParticleTracker(const ParticleTracker&) = delete;
    ParticleTracker& operator=(const ParticleTracker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Particle> particles_;
    std::unordered_map<std::uint64_t, std::size_t> idIndex_;
    std::uint64_t nextId_ = 0;
};

} // namespace nexus::utility::scientific
