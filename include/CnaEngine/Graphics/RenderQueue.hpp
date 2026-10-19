// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Material.hpp"

#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace CnaEngine::Graphics
{
    /// Draw order groups, rendered in this order.
    enum class RenderBucket : std::uint8_t
    {
        Opaque,
        AlphaTested,
        Transparent,
        /// Screen-space or always-on-top work: no depth test, submission order.
        Overlay,
        Count,
    };

    [[nodiscard]] RenderBucket BucketFor(const Material& material) noexcept;

    /// One draw: a mesh, how it looks, and where it is.
    struct RenderItem
    {
        const GpuMesh* mesh = nullptr;
        const Material* material = nullptr;
        Microsoft::Xna::Framework::Matrix world = Microsoft::Xna::Framework::Matrix::getIdentityProperty();
        /// World-space bounds used for culling and depth sorting.
        Microsoft::Xna::Framework::BoundingSphere bounds;
        /// Bit set matched against visibility layer masks.
        std::uint32_t layers = 1;
        /// Coarse priority within a bucket: lower draws first.
        std::int32_t order = 0;
    };

    /// World-space bounds of `mesh` placed with `world` (conservative for
    /// non-uniform scale).
    [[nodiscard]] Microsoft::Xna::Framework::BoundingSphere WorldBounds(const GpuMesh& mesh,
                                                                       const Microsoft::Xna::Framework::Matrix& world);

    /// Per-frame list of render items grouped into buckets.
    ///
    /// Sorting contract (Sort()):
    ///  - Opaque and AlphaTested: by `order`, then by material (in the order
    ///    materials were first submitted, so state changes are grouped and the
    ///    result is identical between runs), then front to back;
    ///  - Transparent: by `order`, then back to front; ties keep submission order;
    ///  - Overlay: by `order`, then submission order.
    class RenderQueue
    {
    public:
        void Clear();
        /// Adds an item to the bucket of its material (or `bucket`). Throws
        /// std::invalid_argument when the mesh or material is missing.
        void Submit(const RenderItem& item, std::optional<RenderBucket> bucket = std::nullopt);
        void Sort(const Microsoft::Xna::Framework::Vector3& viewPosition);

        [[nodiscard]] std::span<const RenderItem> Items(RenderBucket bucket) const;
        [[nodiscard]] std::size_t Count() const noexcept;

    private:
        struct Entry
        {
            RenderItem item;
            std::uint32_t sequence;
            std::uint32_t materialRank;
            float distanceSquared;
        };

        std::array<std::vector<Entry>, static_cast<std::size_t>(RenderBucket::Count)> entries_;
        std::array<std::vector<RenderItem>, static_cast<std::size_t>(RenderBucket::Count)> sorted_;
        std::unordered_map<const Material*, std::uint32_t> materialRanks_;
        std::uint32_t sequence_ = 0;
        bool dirty_ = false;
    };
}
