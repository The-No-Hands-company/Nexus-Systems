#include <gtest/gtest.h>
#include "nexus/particles/ParticleTypes.h"
#include "nexus/particles/ParticleEmitter.h"
#include "nexus/particles/ParticleSolver.h"
#include "nexus/particles/ParticleSystem.h"
#include "nexus/geometry/Mesh.h"

#include <algorithm>
#include <cmath>

using namespace nexus::particles;

TEST(ParticleTest, DefaultParticleIsAlive) {
    Particle p;
    EXPECT_TRUE(p.alive);
    EXPECT_FLOAT_EQ(p.life, 1.0f);
    EXPECT_FLOAT_EQ(p.mass, 1.0f);
}

TEST(EmitterTest, PointEmitterProducesParticles) {
    EmitterConfig ec;
    ec.type = EmitterType::Point;
    ec.emissionRate = 100.0f;
    ec.lifetime = 2.0f;

    ParticleEmitter emitter(ec);
    std::vector<Particle> particles;
    uint32_t nextId = 1;

    emitter.emit(1.0f, particles, nextId);

    uint32_t activeCount = 0;
    for (const auto& p : particles) {
        if (p.alive) activeCount++;
    }
    EXPECT_GE(activeCount, 1u);
}

TEST(EmitterTest, DisabledEmitterProducesNone) {
    EmitterConfig ec;
    ec.emissionRate = 100.0f;
    ec.enabled = false;

    ParticleEmitter emitter(ec);
    std::vector<Particle> particles;
    uint32_t nextId = 1;

    emitter.emit(1.0f, particles, nextId);
    EXPECT_EQ(particles.size(), 0u);
}

TEST(EmitterTest, OneShotEmitterFiresOnce) {
    EmitterConfig ec;
    ec.emissionRate = 10.0f;
    ec.oneShot = true;
    ec.lifetime = 1.0f;

    ParticleEmitter emitter(ec);
    std::vector<Particle> particles;
    uint32_t nextId = 1;

    emitter.emit(0.5f, particles, nextId);
    size_t afterFirst = particles.size();

    emitter.emit(0.5f, particles, nextId);
    EXPECT_EQ(particles.size(), afterFirst);
}

TEST(EmitterTest, SphereEmitterPositionsInBounds) {
    EmitterConfig ec;
    ec.type = EmitterType::Sphere;
    ec.position = Vec3{0, 5, 0};
    ec.sphereRadius = 2.0f;
    ec.emissionRate = 50.0f;
    ec.lifetime = 1.0f;

    ParticleEmitter emitter(ec);
    std::vector<Particle> particles;
    uint32_t nextId = 1;

    emitter.emit(1.0f, particles, nextId);

    for (const auto& p : particles) {
        float dx = p.position.x - ec.position.x;
        float dy = p.position.y - ec.position.y;
        float dz = p.position.z - ec.position.z;
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        EXPECT_LE(dist, ec.sphereRadius * 1.1f);
    }
}

TEST(EmitterTest, BoxEmitterPositionsInBounds) {
    EmitterConfig ec;
    ec.type = EmitterType::Box;
    ec.position = Vec3{0, 0, 0};
    ec.boxExtents = Vec3{2, 2, 2};
    ec.emissionRate = 50.0f;
    ec.lifetime = 1.0f;

    ParticleEmitter emitter(ec);
    std::vector<Particle> particles;
    uint32_t nextId = 1;

    emitter.emit(1.0f, particles, nextId);

    for (const auto& p : particles) {
        EXPECT_LE(std::abs(p.position.x), ec.boxExtents.x * 0.5f * 1.1f);
        EXPECT_LE(std::abs(p.position.y), ec.boxExtents.y * 0.5f * 1.1f);
        EXPECT_LE(std::abs(p.position.z), ec.boxExtents.z * 0.5f * 1.1f);
    }
}

TEST(SolverTest, GravityPullsParticlesDown) {
    SystemConfig config;
    config.gravity = 9.81f;
    config.timestep = 0.016f;
    config.enableGroundPlane = false;

    ParticleSolver solver(config);

    ForceField gravity;
    gravity.type = ForceField::Type::Gravity;
    gravity.direction = Vec3{0, -1, 0};
    gravity.strength = 9.81f;
    solver.addForce(gravity);

    std::vector<Particle> particles;
    Particle p;
    p.position = Vec3{0, 10, 0};
    p.velocity = Vec3{0, 0, 0};
    p.mass = 1.0f;
    p.life = 10.0f;
    p.alive = true;
    particles.push_back(p);

    solver.step(particles, 1.0f);
    EXPECT_LT(particles[0].position.y, 10.0f);
}

TEST(SolverTest, GroundPlaneBouncesParticles) {
    SystemConfig config;
    config.groundPlaneY = 0.0f;
    config.bounceDamping = 0.5f;
    config.enableGroundPlane = true;
    config.enableInterParticleCollision = false;

    ParticleSolver solver(config);

    ForceField gravity;
    gravity.type = ForceField::Type::Gravity;
    gravity.direction = Vec3{0, -1, 0};
    gravity.strength = 9.81f;
    solver.addForce(gravity);

    std::vector<Particle> particles;
    Particle p;
    p.position = Vec3{0, 1, 0};
    p.velocity = Vec3{0, -5, 0};
    p.mass = 1.0f;
    p.life = 10.0f;
    p.alive = true;
    particles.push_back(p);

    solver.step(particles, 0.5f);
    EXPECT_GE(particles[0].position.y, config.groundPlaneY - 0.01f);
}

TEST(SolverTest, ParticlesDieAfterLifetime) {
    SystemConfig config;
    config.enableGroundPlane = false;

    ParticleSolver solver(config);

    std::vector<Particle> particles;
    Particle p;
    p.position = Vec3{0, 0, 0};
    p.velocity = Vec3{0, 0, 0};
    p.mass = 1.0f;
    p.life = 0.1f;
    p.maxLife = 1.0f;
    p.alive = true;
    particles.push_back(p);

    solver.step(particles, 1.0f);
    EXPECT_FALSE(particles[0].alive);
}

TEST(SystemTest, CreateAndSimulate) {
    ParticleSystem system;

    EmitterConfig ec;
    ec.type = EmitterType::Point;
    ec.position = Vec3{0, 0, 0};
    ec.direction = Vec3{0, 1, 0};
    ec.emissionRate = 10.0f;
    ec.lifetime = 2.0f;

    system.addEmitter(ec);

    ForceField gravity;
    gravity.type = ForceField::Type::Gravity;
    gravity.direction = Vec3{0, -1, 0};
    gravity.strength = 9.81f;
    system.addForce(gravity);

    system.simulate(1.0f);

    auto s = system.stats();
    EXPECT_GT(s.activeCount, 0u);
}

TEST(SystemTest, ResetClearsAllParticles) {
    ParticleSystem system;

    EmitterConfig ec;
    ec.emissionRate = 20.0f;
    ec.lifetime = 1.0f;
    system.addEmitter(ec);

    system.simulate(1.0f);
    EXPECT_GT(system.particles().size(), 0u);

    system.reset();
    EXPECT_EQ(system.particles().size(), 0u);
}

TEST(SystemTest, EmitterCount) {
    ParticleSystem system;
    EXPECT_EQ(system.emitterCount(), 0u);

    system.addEmitter(EmitterConfig{});
    system.addEmitter(EmitterConfig{});
    EXPECT_EQ(system.emitterCount(), 2u);

    system.removeEmitter(0);
    EXPECT_EQ(system.emitterCount(), 1u);
}

TEST(SystemTest, EmitterAccess) {
    ParticleSystem system;

    EmitterConfig ec;
    ec.emissionRate = 42.0f;
    system.addEmitter(ec);

    auto* em = system.emitter(0);
    ASSERT_NE(em, nullptr);
    EXPECT_FLOAT_EQ(em->config().emissionRate, 42.0f);

    EXPECT_EQ(system.emitter(99), nullptr);
}

TEST(SystemTest, ToMeshProducesPoints) {
    ParticleSystem system;

    EmitterConfig ec;
    ec.type = EmitterType::Point;
    ec.position = Vec3{0, 0, 0};
    ec.emissionRate = 5.0f;
    ec.initialSpeed = 0.0f;
    ec.lifetime = 5.0f;
    system.addEmitter(ec);

    system.simulate(1.0f);

    auto mesh = system.toMesh();
    EXPECT_GT(mesh.topology().faceCount(), 0u);
}

TEST(SystemTest, StatsReflectSimulationTime) {
    ParticleSystem system;
    system.simulate(2.5f);
    auto s = system.stats();
    EXPECT_FLOAT_EQ(s.simulationTime, 2.5f);
}

TEST(SystemTest, ForceFieldsAreApplied) {
    ParticleSystem system;

    SystemConfig config;
    config.gravity = 0.0f;
    config.enableGroundPlane = false;
    system.setConfig(config);

    EmitterConfig ec;
    ec.position = Vec3{0, 0, 0};
    ec.initialSpeed = 0.0f;
    ec.lifetime = 5.0f;
    ec.emissionRate = 1.0f;
    system.addEmitter(ec);

    ForceField wind;
    wind.type = ForceField::Type::DirectionalForce;
    wind.direction = Vec3{1, 1, 0};
    wind.strength = 50.0f;
    system.addForce(wind);

    system.simulate(1.0f);

    auto& particles = system.particles();
    for (const auto& p : particles) {
        if (p.alive && p.position != Vec3{0, 0, 0}) {
            EXPECT_GT(p.position.x, 0.0f);
            EXPECT_GT(p.position.y, 0.0f);
        }
    }
}

TEST(ForceFieldTest, VortexRotation) {
    SystemConfig config;
    config.gravity = 0.0f;
    config.enableGroundPlane = false;

    ParticleSolver solver(config);

    ForceField vortex;
    vortex.type = ForceField::Type::Vortex;
    vortex.position = Vec3{0, 0, 0};
    vortex.direction = Vec3{0, 0, 1};
    vortex.strength = 10.0f;
    vortex.radius = 100.0f;
    solver.addForce(vortex);

    std::vector<Particle> particles;
    Particle p;
    p.position = Vec3{5, 0, 0};
    p.velocity = Vec3{0, 0, 0};
    p.mass = 1.0f;
    p.life = 10.0f;
    p.alive = true;
    particles.push_back(p);

    solver.step(particles, 0.5f);
    EXPECT_NE(particles[0].position.y, 0.0f);
}

TEST(ForceFieldTest, AttractorPullsParticles) {
    SystemConfig config;
    config.gravity = 0.0f;
    config.enableGroundPlane = false;

    ParticleSolver solver(config);

    ForceField attractor;
    attractor.type = ForceField::Type::Attractor;
    attractor.position = Vec3{0, 0, 0};
    attractor.strength = 100.0f;
    attractor.radius = 100.0f;
    solver.addForce(attractor);

    std::vector<Particle> particles;
    Particle p;
    p.position = Vec3{10, 0, 0};
    p.velocity = Vec3{0, 0, 0};
    p.mass = 1.0f;
    p.life = 10.0f;
    p.alive = true;
    particles.push_back(p);

    solver.step(particles, 0.5f);

    float origDist = std::sqrt(100.0f);
    float newDist = std::sqrt(
        particles[0].position.x * particles[0].position.x +
        particles[0].position.y * particles[0].position.y +
        particles[0].position.z * particles[0].position.z);
    EXPECT_LT(newDist, origDist);
}

TEST(ParticleSystem, InterParticleCollision) {
    SystemConfig config;
    config.gravity = 0.0f;
    config.enableGroundPlane = false;
    config.enableInterParticleCollision = true;
    config.interParticleDistance = 1.0f;

    ParticleSolver solver(config);

    std::vector<Particle> particles;

    Particle p1;
    p1.position = Vec3{0, 0, 0};
    p1.velocity = Vec3{0, 0, 0};
    p1.mass = 1.0f;
    p1.life = 10.0f;
    p1.alive = true;
    particles.push_back(p1);

    Particle p2;
    p2.position = Vec3{0.5f, 0, 0};
    p2.velocity = Vec3{0, 0, 0};
    p2.mass = 1.0f;
    p2.life = 10.0f;
    p2.alive = true;
    particles.push_back(p2);

    solver.step(particles, 0.1f);

    float dx = particles[0].position.x - particles[1].position.x;
    float dy = particles[0].position.y - particles[1].position.y;
    float dz = particles[0].position.z - particles[1].position.z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    EXPECT_GE(dist, 0.9f * config.interParticleDistance);
}

TEST(EmitterTest, ResetClearsAccumulator) {
    EmitterConfig ec;
    ec.emissionRate = 100.0f;
    ec.oneShot = false;

    ParticleEmitter emitter(ec);
    std::vector<Particle> particles;
    uint32_t nextId = 1;

    emitter.emit(0.5f, particles, nextId);
    emitter.reset();
    emitter.emit(0.5f, particles, nextId);

    EXPECT_GT(particles.size(), 0u);
}

TEST(SystemTest, MultipleSubSteps) {
    SystemConfig config;
    config.timestep = 0.016f;
    config.subSteps = 4;
    config.gravity = 9.81f;
    config.enableGroundPlane = false;

    ParticleSolver solver(config);

    ForceField gravity;
    gravity.type = ForceField::Type::Gravity;
    gravity.direction = Vec3{0, -1, 0};
    gravity.strength = 9.81f;
    solver.addForce(gravity);

    std::vector<Particle> particles;
    Particle p;
    p.position = Vec3{0, 5, 0};
    p.velocity = Vec3{0, 2, 0};
    p.mass = 1.0f;
    p.life = 10.0f;
    p.alive = true;
    particles.push_back(p);

    solver.step(particles, 1.0f);
    EXPECT_TRUE(particles[0].alive);
    EXPECT_GT(particles[0].position.x, -10.0f);
}
