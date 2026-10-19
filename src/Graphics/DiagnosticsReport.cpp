// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/DiagnosticsReport.hpp"

#include <array>
#include <charconv>

namespace CnaEngine::Graphics
{
    namespace
    {
        // std::to_chars never consults the C locale, unlike printf.
        std::string Fixed(const double value, const int precision)
        {
            std::array<char, 64> buffer{};
            const auto [ptr, ec] =
                std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed, precision);
            return ec == std::errc{} ? std::string(buffer.data(), ptr) : std::string("?");
        }

        std::string Ms(const double seconds) { return Fixed(seconds * 1000.0, 2); }
        std::string MiB(const std::int64_t bytes) { return Fixed(static_cast<double>(bytes) / (1024.0 * 1024.0), 1); }
    }

    std::vector<std::string> FormatDiagnostics(const DiagnosticsSources& sources)
    {
        std::vector<std::string> lines;
        if (sources.frames != nullptr && sources.frames->SampleCount() > 0) {
            const auto& f = *sources.frames;
            lines.push_back("frame " + Ms(f.AverageSeconds()) + " ms (" + Fixed(f.FramesPerSecond(), 1) + " fps) min "
                            + Ms(f.MinSeconds()) + " max " + Ms(f.MaxSeconds()) + " p99 "
                            + Ms(f.PercentileSeconds(0.99)));
        }
        if (sources.render != nullptr) {
            const auto& r = *sources.render;
            lines.push_back("draws " + std::to_string(r.drawCalls) + "  triangles " + std::to_string(r.primitives)
                            + "  vertices " + std::to_string(r.vertices));
        }
        if (sources.culling != nullptr) {
            const auto& c = *sources.culling;
            lines.push_back("culling " + std::to_string(c.considered) + " considered, " + std::to_string(c.visible)
                            + " visible (layer " + std::to_string(c.byLayer) + ", distance "
                            + std::to_string(c.byDistance) + ", frustum " + std::to_string(c.byFrustum) + ")");
        }
        if (sources.lod != nullptr && !sources.lod->perLevel.empty()) {
            std::string line = "lod";
            for (std::size_t level = 0; level < sources.lod->perLevel.size(); ++level) {
                line += (level == 0 ? " " : ", ") + std::to_string(level) + ": "
                        + std::to_string(sources.lod->perLevel[level]);
            }
            lines.push_back(std::move(line));
        }
        if (sources.profiler != nullptr && !sources.profiler->LastFrame().empty()) {
            std::string line = "cpu";
            bool first = true;
            for (const auto& section : sources.profiler->LastFrame()) {
                line += (first ? " " : ", ") + section.name + " " + Fixed(static_cast<double>(section.nanoseconds) / 1e6, 2)
                        + " ms";
                first = false;
            }
            lines.push_back(std::move(line));
        }
        if (sources.includeGpuMemory) {
            const auto memory = CurrentGpuMemory();
            lines.push_back("gpu meshes " + std::to_string(memory.meshes) + ", vertex " + MiB(memory.vertexBytes)
                            + " MiB, index " + MiB(memory.indexBytes) + " MiB");
        }
        return lines;
    }
}
