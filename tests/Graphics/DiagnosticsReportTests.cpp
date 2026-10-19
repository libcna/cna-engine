// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/DiagnosticsReport.hpp"

#include <gtest/gtest.h>

#include <clocale>
#include <string>

namespace CnaEngine::Graphics
{
    TEST(DiagnosticsReportTests, FormatsEveryProvidedSource)
    {
        Diagnostics::FrameStatistics frames(4);
        frames.AddFrame(0.016);
        frames.AddFrame(0.020);
        RenderStatistics render;
        render.Record(100, 300);
        render.Record(20, 60);
        CullStatistics culling{10, 4, 1, 2, 3};
        std::int64_t now = 0;
        Diagnostics::CpuProfiler profiler([&] { return now; });
        profiler.SetEnabled(true);
        profiler.Record("Update", 1'250'000);
        profiler.Record("Draw", 3'000'000);
        profiler.EndFrame();

        DiagnosticsSources sources;
        sources.frames = &frames;
        sources.render = &render;
        sources.culling = &culling;
        sources.profiler = &profiler;
        sources.includeGpuMemory = false;
        const auto lines = FormatDiagnostics(sources);
        ASSERT_EQ(lines.size(), 4u);
        EXPECT_EQ(lines[0], "frame 18.00 ms (55.6 fps) min 16.00 max 20.00 p99 20.00");
        EXPECT_EQ(lines[1], "draws 2  triangles 120  vertices 360");
        EXPECT_EQ(lines[2], "culling 10 considered, 4 visible (layer 1, distance 2, frustum 3)");
        EXPECT_EQ(lines[3], "cpu Update 1.25 ms, Draw 3.00 ms");
    }

    TEST(DiagnosticsReportTests, MissingSourcesAreOmitted)
    {
        DiagnosticsSources sources;
        sources.includeGpuMemory = false;
        EXPECT_TRUE(FormatDiagnostics(sources).empty());
        sources.includeGpuMemory = true;
        const auto lines = FormatDiagnostics(sources);
        ASSERT_EQ(lines.size(), 1u);
        EXPECT_EQ(lines[0].rfind("gpu meshes ", 0), 0u);
    }

    TEST(DiagnosticsReportTests, DecimalSeparatorIgnoresTheCLocale)
    {
        const std::string previous = std::setlocale(LC_NUMERIC, nullptr);
        // A comma locale may be unavailable on the test machine; the check is
        // meaningful only when it is.
        const bool comma = std::setlocale(LC_NUMERIC, "de_DE.UTF-8") != nullptr
                           || std::setlocale(LC_NUMERIC, "cs_CZ.UTF-8") != nullptr;
        Diagnostics::FrameStatistics frames(1);
        frames.AddFrame(0.0125);
        DiagnosticsSources sources;
        sources.frames = &frames;
        sources.includeGpuMemory = false;
        const auto lines = FormatDiagnostics(sources);
        std::setlocale(LC_NUMERIC, previous.c_str());
        ASSERT_EQ(lines.size(), 1u);
        EXPECT_NE(lines[0].find("12.50 ms"), std::string::npos) << lines[0] << (comma ? " (comma locale)" : "");
    }
}
