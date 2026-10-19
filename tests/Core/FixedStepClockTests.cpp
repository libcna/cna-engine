// SPDX-License-Identifier: MIT
#include "CnaEngine/Core/FixedStepClock.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace CnaEngine
{
    TEST(FixedStepClockTests, StepsAccumulateAcrossFrames)
    {
        FixedStepClock clock(0.01);
        EXPECT_EQ(clock.Advance(0.025), 2);
        EXPECT_NEAR(clock.Alpha(), 0.5, 1e-9);
        EXPECT_EQ(clock.Advance(0.006), 1) << "0.005 left + 0.006";
        EXPECT_NEAR(clock.Accumulated(), 0.001, 1e-9);
        EXPECT_EQ(clock.TotalSteps(), 3u);
        EXPECT_NEAR(clock.SimulatedSeconds(), 0.03, 1e-12);
    }

    TEST(FixedStepClockTests, LongFramesAreClampedAndStepsCapped)
    {
        FixedStepClock clock(1.0 / 120.0, 0.25, 30);
        EXPECT_EQ(clock.Advance(5.0), 30) << "a 5 s hitch counts as 0.25 s";
        EXPECT_EQ(clock.SaturatedFrames(), 0u) << "0.25 s is exactly 30 steps";

        FixedStepClock tight(0.01, 1.0, 5);
        EXPECT_EQ(tight.Advance(0.2), 5);
        EXPECT_EQ(tight.SaturatedFrames(), 1u);
        EXPECT_NEAR(tight.Accumulated(), 0.15, 1e-9) << "the remainder is kept, not dropped";
        EXPECT_EQ(tight.Advance(0.0), 5);
    }

    TEST(FixedStepClockTests, InvalidInputIsHarmless)
    {
        FixedStepClock clock(0.01);
        EXPECT_EQ(clock.Advance(-1.0), 0);
        EXPECT_EQ(clock.Advance(std::numeric_limits<double>::quiet_NaN()), 0);
        EXPECT_EQ(clock.Accumulated(), 0.0);
        EXPECT_THROW(FixedStepClock(0.0), std::invalid_argument);
        EXPECT_THROW(FixedStepClock(0.01, -1.0), std::invalid_argument);
        EXPECT_THROW(FixedStepClock(0.01, 0.25, 0), std::invalid_argument);
    }

    TEST(FixedStepClockTests, RunCallsTheStepWithTheFixedDelta)
    {
        FixedStepClock clock(0.5);
        double simulated = 0.0;
        int calls = 0;
        EXPECT_EQ(clock.Run(0.25, [&](double dt) { simulated += dt; ++calls; }), 0);
        EXPECT_EQ(clock.Run(0.25, [&](double dt) { simulated += dt; ++calls; }), 1);
        EXPECT_EQ(calls, 1);
        EXPECT_DOUBLE_EQ(simulated, 0.5);
        clock.Reset();
        EXPECT_EQ(clock.TotalSteps(), 0u);
        EXPECT_EQ(clock.Alpha(), 0.0);
    }
}
