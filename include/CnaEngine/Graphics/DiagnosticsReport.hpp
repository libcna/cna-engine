// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Diagnostics/FrameStatistics.hpp"
#include "CnaEngine/Diagnostics/ScopedTimer.hpp"
#include "CnaEngine/Graphics/RenderStatistics.hpp"
#include "CnaEngine/Graphics/Visibility.hpp"

#include <string>
#include <vector>

namespace CnaEngine::Graphics
{
    /// The metrics a frame exposes, gathered without drawing anything. Any
    /// pointer may be null; the corresponding lines are omitted.
    struct DiagnosticsSources
    {
        const Diagnostics::FrameStatistics* frames = nullptr;
        const RenderStatistics* render = nullptr;
        const CullStatistics* culling = nullptr;
        const LodStatistics* lod = nullptr;
        const Diagnostics::CpuProfiler* profiler = nullptr;
        bool includeGpuMemory = true;
    };

    /// Formats the metrics as short, locale-independent text lines (for a log,
    /// a console, a test, or an overlay), e.g.
    ///   "frame 16.67 ms (60.0 fps) min 15.90 max 18.20 p99 18.10"
    ///   "draws 42  triangles 12840  vertices 9120"
    ///   "culling 310 considered, 128 visible (layer 0, distance 12, frustum 170)"
    ///   "lod 0: 41, 1: 10, 2: 3"
    ///   "cpu Update 1.20 ms, Draw 3.40 ms"
    ///   "gpu meshes 18, vertex 1.2 MiB, index 0.3 MiB"
    [[nodiscard]] std::vector<std::string> FormatDiagnostics(const DiagnosticsSources& sources);
}
