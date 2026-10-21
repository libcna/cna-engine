// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/RenderQueue.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;

    RenderBucket BucketFor(const Material& material) noexcept
    {
        switch (material.blend) {
            case BlendMode::Opaque: return RenderBucket::Opaque;
            case BlendMode::AlphaTest: return RenderBucket::AlphaTested;
            case BlendMode::AlphaBlend:
            case BlendMode::Additive:
            case BlendMode::PremultipliedAlpha: return RenderBucket::Transparent;
        }
        return RenderBucket::Opaque;
    }

    BoundingSphere WorldBounds(const GpuMesh& mesh, const Matrix& world)
    {
        const BoundingSphere& local = mesh.Sphere();
        // Largest axis scale keeps the sphere conservative under any scale.
        const auto axisLength = [](float x, float y, float z) { return std::sqrt(x * x + y * y + z * z); };
        const float scale = std::max({axisLength(world.M11, world.M12, world.M13),
                                      axisLength(world.M21, world.M22, world.M23),
                                      axisLength(world.M31, world.M32, world.M33)});
        return BoundingSphere(Vector3::Transform(local.Center, world), local.Radius * scale);
    }

    void RenderQueue::Clear()
    {
        for (auto& bucket : entries_) {
            bucket.clear();
        }
        for (auto& bucket : sorted_) {
            bucket.clear();
        }
        materialRanks_.clear();
        sequence_ = 0;
    }

    void RenderQueue::Submit(const RenderItem& item, const std::optional<RenderBucket> bucket)
    {
        if (item.mesh == nullptr || item.material == nullptr) {
            throw std::invalid_argument("RenderQueue::Submit: an item needs a mesh and a material");
        }
        const RenderBucket target = bucket.value_or(BucketFor(*item.material));
        if (target == RenderBucket::Count) {
            throw std::invalid_argument("RenderQueue::Submit: invalid bucket");
        }
        const auto rank = materialRanks_.try_emplace(item.material, static_cast<std::uint32_t>(materialRanks_.size()));
        const auto index = static_cast<std::size_t>(target);
        entries_[index].push_back(Entry{item, sequence_++, rank.first->second, 0.0f});
        sorted_[index].push_back(item);
    }

    void RenderQueue::Sort(const Vector3& viewPosition)
    {
        for (std::size_t b = 0; b < entries_.size(); ++b) {
            auto& bucket = entries_[b];
            for (auto& entry : bucket) {
                entry.distanceSquared = Vector3::DistanceSquared(entry.item.bounds.Center, viewPosition);
            }
            switch (static_cast<RenderBucket>(b)) {
                case RenderBucket::Opaque:
                case RenderBucket::AlphaTested:
                    std::sort(bucket.begin(), bucket.end(), [](const Entry& a, const Entry& c) {
                        if (a.item.order != c.item.order) {
                            return a.item.order < c.item.order;
                        }
                        if (a.materialRank != c.materialRank) {
                            return a.materialRank < c.materialRank;
                        }
                        if (a.distanceSquared != c.distanceSquared) {
                            return a.distanceSquared < c.distanceSquared;
                        }
                        return a.sequence < c.sequence;
                    });
                    break;
                case RenderBucket::Transparent:
                    std::sort(bucket.begin(), bucket.end(), [](const Entry& a, const Entry& c) {
                        if (a.item.order != c.item.order) {
                            return a.item.order < c.item.order;
                        }
                        if (a.distanceSquared != c.distanceSquared) {
                            return a.distanceSquared > c.distanceSquared;
                        }
                        return a.sequence < c.sequence;
                    });
                    break;
                case RenderBucket::Sequenced:
                case RenderBucket::Overlay:
                case RenderBucket::Count:
                    std::sort(bucket.begin(), bucket.end(), [](const Entry& a, const Entry& c) {
                        if (a.item.order != c.item.order) {
                            return a.item.order < c.item.order;
                        }
                        return a.sequence < c.sequence;
                    });
                    break;
            }
            auto& out = sorted_[b];
            out.clear();
            for (const auto& entry : bucket) {
                out.push_back(entry.item);
            }
        }
    }

    std::span<const RenderItem> RenderQueue::Items(const RenderBucket bucket) const
    {
        if (bucket == RenderBucket::Count) {
            throw std::invalid_argument("RenderQueue::Items: invalid bucket");
        }
        return sorted_[static_cast<std::size_t>(bucket)];
    }

    std::size_t RenderQueue::Count() const noexcept
    {
        std::size_t count = 0;
        for (const auto& bucket : entries_) {
            count += bucket.size();
        }
        return count;
    }
}
