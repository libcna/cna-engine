// SPDX-License-Identifier: MIT
#include "CnaEngine/Diagnostics/ScopedTimer.hpp"

#include <gtest/gtest.h>

namespace CnaEngine::Diagnostics
{
    TEST(ScopedTimerTests, DisabledProfilerNeverReadsTheClock)
    {
        int reads = 0;
        CpuProfiler profiler([&] { return static_cast<std::int64_t>(++reads); });
        {
            ScopedTimer timer(&profiler, "Update");
        }
        {
            ScopedTimer timer(nullptr, "Update");
        }
        profiler.EndFrame();
        EXPECT_EQ(reads, 0);
        EXPECT_TRUE(profiler.LastFrame().empty());
    }

    TEST(ScopedTimerTests, SectionsAccumulatePerFrame)
    {
        std::int64_t now = 0;
        CpuProfiler profiler([&] { return now; });
        profiler.SetEnabled(true);
        {
            ScopedTimer timer(&profiler, "Update");
            now += 100;
        }
        {
            ScopedTimer timer(&profiler, "Draw");
            now += 30;
            ScopedTimer nested(&profiler, "Draw/Shadows");
            now += 20;
        }
        {
            ScopedTimer timer(&profiler, "Update");
            now += 5;
        }
        profiler.EndFrame();
        const auto& sections = profiler.LastFrame();
        ASSERT_EQ(sections.size(), 3u);
        EXPECT_EQ(sections[0].name, "Update");
        EXPECT_EQ(sections[0].nanoseconds, 105);
        EXPECT_EQ(sections[0].calls, 2u);
        EXPECT_EQ(sections[1].name, "Draw/Shadows");
        EXPECT_EQ(sections[1].nanoseconds, 20);
        EXPECT_EQ(sections[2].name, "Draw");
        EXPECT_EQ(sections[2].nanoseconds, 50);

        profiler.EndFrame();
        EXPECT_TRUE(profiler.LastFrame().empty()) << "each frame starts empty";
    }
}
