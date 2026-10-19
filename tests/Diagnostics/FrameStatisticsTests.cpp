// SPDX-License-Identifier: MIT
#include "CnaEngine/Diagnostics/FrameStatistics.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace CnaEngine::Diagnostics
{
    TEST(FrameStatisticsTests, EmptyWindowReportsZero)
    {
        const FrameStatistics stats(4);
        EXPECT_EQ(stats.AverageSeconds(), 0.0);
        EXPECT_EQ(stats.FramesPerSecond(), 0.0);
        EXPECT_EQ(stats.PercentileSeconds(0.99), 0.0);
        EXPECT_THROW(FrameStatistics(0), std::invalid_argument);
    }

    TEST(FrameStatisticsTests, RollingWindowDropsOldestFrames)
    {
        FrameStatistics stats(3);
        for (const double frame : {0.010, 0.020, 0.030, 0.040}) {
            stats.AddFrame(frame);
        }
        EXPECT_EQ(stats.FrameCount(), 4u);
        EXPECT_EQ(stats.SampleCount(), 3u);
        EXPECT_NEAR(stats.AverageSeconds(), 0.030, 1e-12);
        EXPECT_DOUBLE_EQ(stats.MinSeconds(), 0.020);
        EXPECT_DOUBLE_EQ(stats.MaxSeconds(), 0.040);
        EXPECT_DOUBLE_EQ(stats.LastSeconds(), 0.040);
        EXPECT_NEAR(stats.FramesPerSecond(), 1.0 / 0.030, 1e-9);
    }

    TEST(FrameStatisticsTests, PercentilesUseNearestRank)
    {
        FrameStatistics stats(100);
        for (int i = 1; i <= 100; ++i) {
            stats.AddFrame(i / 1000.0);
        }
        EXPECT_DOUBLE_EQ(stats.PercentileSeconds(0.99), 0.099);
        EXPECT_DOUBLE_EQ(stats.PercentileSeconds(0.5), 0.050);
        EXPECT_DOUBLE_EQ(stats.PercentileSeconds(1.0), 0.100);
        EXPECT_DOUBLE_EQ(stats.PercentileSeconds(0.0), 0.001);
    }

    TEST(FrameStatisticsTests, InvalidDurationsAreRejected)
    {
        FrameStatistics stats(8);
        stats.AddFrame(-1.0);
        stats.AddFrame(std::numeric_limits<double>::quiet_NaN());
        stats.AddFrame(std::numeric_limits<double>::infinity());
        EXPECT_EQ(stats.FrameCount(), 0u);
        EXPECT_EQ(stats.RejectedCount(), 3u);
        stats.Clear();
        EXPECT_EQ(stats.RejectedCount(), 0u);
    }
}
