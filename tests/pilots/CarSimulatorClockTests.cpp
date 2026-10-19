// SPDX-License-Identifier: MIT
// Read-only equivalence probe: the engine's FixedStepClock against the step
// loop inside car-simulator's Vehicle::Update (1/120 s steps, 0.25 s frame
// clamp, at most 30 steps). The same frame times drive car-simulator's own
// vehicle and an engine clock; car-simulator's step count per frame is
// recovered from its accumulator, which it keeps in single precision.
#include "CnaEngine/Core/FixedStepClock.hpp"

#include "CarSim/Sim/DriverControls.hpp"
#include "CarSim/Sim/Ground.hpp"
#include "CarSim/Sim/Vehicle.hpp"
#include "CarSim/Sim/VehicleDefinition.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace
{
    template <typename Tag, auto Member>
    struct Expose
    {
        friend constexpr auto Get(Tag) { return Member; }
    };
    struct Accumulator
    {
        friend constexpr auto Get(Accumulator);
    };
    template struct Expose<Accumulator, &CarSim::Sim::Vehicle::accumulator_>;

    float AccumulatorOf(const CarSim::Sim::Vehicle& vehicle)
    {
        return vehicle.*Get(Accumulator{});
    }

    struct Comparison
    {
        std::size_t frames = 0;
        std::size_t differingFrames = 0;
        std::int64_t carSimSteps = 0;
        std::int64_t engineSteps = 0;
        std::int64_t largestRunningDifference = 0;
    };

    // Feeds `frames` to both loops and compares step counts frame by frame.
    Comparison Compare(const std::vector<float>& frames)
    {
        using namespace CarSim::Sim;
        Vehicle vehicle(MakeReferenceVehicle(), TransmissionMode::Automatic);
        vehicle.PlaceAt({0.0f, 0.0f, 0.0f}, 0.0f);
        const FlatGround ground(0.0f);
        const DriverControls idle{};
        CnaEngine::FixedStepClock clock(static_cast<double>(kPhysicsStepSeconds), 0.25, 30);

        Comparison result;
        for (const float frame : frames) {
            const float before = AccumulatorOf(vehicle);
            vehicle.Update(idle, frame, ground);
            const float clamped = std::clamp(frame, 0.0f, 0.25f);
            const auto carSim = std::lround((before + clamped - AccumulatorOf(vehicle)) / kPhysicsStepSeconds);
            const int engine = clock.Advance(static_cast<double>(frame));

            ++result.frames;
            result.differingFrames += carSim != engine ? 1u : 0u;
            result.carSimSteps += carSim;
            result.engineSteps += engine;
            result.largestRunningDifference =
                std::max(result.largestRunningDifference, std::abs(result.carSimSteps - result.engineSteps));
        }
        return result;
    }

    // Deterministic frame times: `count` frames of `base` seconds with up to
    // `jitter` either way (a linear congruential generator, so every run and
    // every platform sees the same sequence).
    std::vector<float> Jittered(const std::size_t count, const float base, const float jitter)
    {
        std::vector<float> frames;
        frames.reserve(count);
        std::uint32_t state = 12345u;
        for (std::size_t i = 0; i < count; ++i) {
            state = state * 1664525u + 1013904223u;
            const float unit = static_cast<float>(state >> 8) / static_cast<float>(1u << 24); // [0, 1)
            frames.push_back(base + (unit * 2.0f - 1.0f) * jitter);
        }
        return frames;
    }
}

namespace CnaEngine
{
    TEST(CarSimulatorClockProbe, VsyncFrameRatesStepIdentically)
    {
        for (const float hz : {30.0f, 60.0f, 120.0f, 144.0f, 240.0f}) {
            const Comparison c = Compare(std::vector<float>(36'000, 1.0f / hz));
            EXPECT_EQ(c.differingFrames, 0u) << hz << " Hz";
            EXPECT_EQ(c.carSimSteps, c.engineSteps) << hz << " Hz";
        }
    }

    TEST(CarSimulatorClockProbe, HitchesAreClampedAndCappedTheSameWay)
    {
        const std::vector<float> frames{0.016f, 5.0f, 0.016f, 0.25f, 0.3f, 0.0f, -1.0f, 0.1f, 0.016f, 0.2501f};
        const Comparison c = Compare(frames);
        EXPECT_EQ(c.differingFrames, 0u);
        EXPECT_EQ(c.carSimSteps, c.engineSteps);
    }

    TEST(CarSimulatorClockProbe, JitteredFramesStayWithinOneStep)
    {
        // Ten minutes at a jittery 60 Hz. car-simulator's float accumulator
        // and the engine's double one can round a frame that lands within a
        // few ULPs of a step boundary differently; the step then happens one
        // frame earlier or later and the totals agree again.
        const Comparison c = Compare(Jittered(36'000, 1.0f / 60.0f, 0.004f));
        EXPECT_LE(c.largestRunningDifference, 1);
        EXPECT_LE(std::abs(c.carSimSteps - c.engineSteps), 1);
        EXPECT_LE(c.differingFrames * 1000, c.frames) << "at most one frame in a thousand";
        RecordProperty("differing_frames", static_cast<int>(c.differingFrames));
        RecordProperty("steps", static_cast<int>(c.carSimSteps));
    }
}
