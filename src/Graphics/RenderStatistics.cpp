// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/RenderStatistics.hpp"

#include <atomic>

namespace CnaEngine::Graphics
{
    namespace
    {
        std::atomic<std::int64_t> liveMeshes{0};
        std::atomic<std::int64_t> liveVertexBytes{0};
        std::atomic<std::int64_t> liveIndexBytes{0};
    }

    GpuMemoryStatistics CurrentGpuMemory() noexcept
    {
        return GpuMemoryStatistics{liveMeshes.load(std::memory_order_relaxed),
                                   liveVertexBytes.load(std::memory_order_relaxed),
                                   liveIndexBytes.load(std::memory_order_relaxed)};
    }

    namespace Detail
    {
        void AdjustGpuMemory(const std::int64_t meshes, const std::int64_t vertexBytes,
                             const std::int64_t indexBytes) noexcept
        {
            liveMeshes.fetch_add(meshes, std::memory_order_relaxed);
            liveVertexBytes.fetch_add(vertexBytes, std::memory_order_relaxed);
            liveIndexBytes.fetch_add(indexBytes, std::memory_order_relaxed);
        }
    }
}
