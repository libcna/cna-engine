// SPDX-License-Identifier: MIT
#include "CnaEngine/Core/Noise.hpp"

#include <gtest/gtest.h>

namespace CnaEngine::Noise
{
    TEST(NoiseTests, HashesAreDeterministicAndSeeded)
    {
        EXPECT_EQ(Hash2(3, -7, 11u), Hash2(3, -7, 11u));
        EXPECT_NE(Hash2(3, -7, 11u), Hash2(3, -7, 12u));
        EXPECT_NE(Hash3(1, 2, 3, 4u), Hash3(1, 2, 4, 4u));
        for (int i = -50; i < 50; ++i) {
            const float r = Random2(i, i * 3, 9u);
            EXPECT_GE(r, 0.0f);
            EXPECT_LT(r, 1.0f);
        }
    }

    TEST(NoiseTests, LatticeNoiseTilesOnItsPeriod)
    {
        constexpr int period = 8;
        for (float x = 0.25f; x < 8.0f; x += 1.3f) {
            for (float y = 0.5f; y < 8.0f; y += 0.9f) {
                // x + 8 keeps fewer fractional bits than x: equal up to rounding.
                EXPECT_NEAR(Value2(x, y, period, 5u), Value2(x + period, y - period, period, 5u), 1e-5f);
                EXPECT_NEAR(Gradient2(x, y, period, 5u), Gradient2(x - period, y + period, period, 5u), 1e-5f);
                EXPECT_NEAR(Fbm(x, y, period, 4, 2.0f, 0.5f, 5u), Fbm(x + period, y, period, 4, 2.0f, 0.5f, 5u),
                            1e-5f);
                const float value = Value2(x, y, period, 5u);
                EXPECT_GE(value, 0.0f);
                EXPECT_LT(value, 1.0f);
            }
        }
        EXPECT_EQ(Wrap(-1, 8), 7);
        EXPECT_EQ(Wrap(-9, 0), -9) << "no period, no wrap";
    }

    TEST(NoiseTests, WorleyReportsTheNearestTwoFeatures)
    {
        for (float x = 0.1f; x < 6.0f; x += 0.7f) {
            float second = 0.0f;
            const float first = Worley2(x, x * 0.5f, 6, 3u, &second);
            EXPECT_GE(first, 0.0f);
            EXPECT_LE(first, second);
            EXPECT_LT(first, 1.5f) << "a feature point lies in every cell";
        }
    }

    TEST(NoiseTests, SmoothStepRampsEitherWay)
    {
        EXPECT_FLOAT_EQ(SmoothStep(0.0f, 1.0f, 0.5f), 0.5f);
        EXPECT_FLOAT_EQ(SmoothStep(1.0f, 0.0f, 0.25f), 1.0f - SmoothStep(0.0f, 1.0f, 0.25f));
        EXPECT_EQ(SmoothStep(0.5f, 0.5f, 0.4f), 0.0f);
        EXPECT_EQ(SmoothStep(0.5f, 0.5f, 0.6f), 1.0f);
        EXPECT_GT(Ridged(1.3f, 2.7f, 4, 3, 1u), 0.0f);
    }
}
