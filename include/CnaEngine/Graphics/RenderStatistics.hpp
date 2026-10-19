// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace CnaEngine::Graphics
{
    /// Submission counters for one frame (or any other interval the caller
    /// chooses). Engine draw helpers add to a RenderStatistics passed to them;
    /// direct CNA draws can be recorded with Record() so totals stay complete.
    struct RenderStatistics
    {
        std::uint64_t drawCalls = 0;
        std::uint64_t primitives = 0;
        std::uint64_t vertices = 0;
        /// Objects submitted for consideration and objects rejected by culling.
        std::uint64_t submitted = 0;
        std::uint64_t culled = 0;

        void Record(std::uint64_t primitiveCount, std::uint64_t vertexCount) noexcept
        {
            ++drawCalls;
            primitives += primitiveCount;
            vertices += vertexCount;
        }

        void Reset() noexcept { *this = RenderStatistics{}; }

        RenderStatistics& operator+=(const RenderStatistics& other) noexcept
        {
            drawCalls += other.drawCalls;
            primitives += other.primitives;
            vertices += other.vertices;
            submitted += other.submitted;
            culled += other.culled;
            return *this;
        }
    };

    /// Process-wide totals of GPU buffers owned by engine objects (GpuMesh).
    /// Counts what the engine allocated; buffers a game creates directly are
    /// not included.
    struct GpuMemoryStatistics
    {
        std::int64_t meshes = 0;
        std::int64_t vertexBytes = 0;
        std::int64_t indexBytes = 0;
    };

    [[nodiscard]] GpuMemoryStatistics CurrentGpuMemory() noexcept;

    namespace Detail
    {
        // Used by owning GPU resources; thread-safe.
        void AdjustGpuMemory(std::int64_t meshes, std::int64_t vertexBytes, std::int64_t indexBytes) noexcept;
    }
}
