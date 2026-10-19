// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Visibility.hpp"

#include "Microsoft/Xna/Framework/ContainmentType.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::ContainmentType;
    using Microsoft::Xna::Framework::Vector3;

    Visibility Classify(const BoundingSphere& bounds, const std::uint32_t layers, const Camera& camera,
                        const VisibilitySettings& settings)
    {
        if ((layers & settings.layerMask) == 0) {
            return Visibility::RejectedByLayer;
        }
        if (std::isfinite(settings.maxDistance)) {
            const float distance = Vector3::Distance(bounds.Center, camera.Position()) - bounds.Radius;
            if (distance > settings.maxDistance) {
                return Visibility::RejectedByDistance;
            }
        }
        if (settings.frustum && camera.Frustum().Contains(bounds) == ContainmentType::Disjoint) {
            return Visibility::RejectedByFrustum;
        }
        return Visibility::Visible;
    }

    void CullInto(const std::span<const RenderItem> candidates, const Camera& camera, const VisibilitySettings& settings,
                  RenderQueue& queue, CullStatistics& stats, RenderStatistics* render)
    {
        std::uint64_t visible = 0;
        for (const auto& item : candidates) {
            ++stats.considered;
            switch (Classify(item.bounds, item.layers, camera, settings)) {
                case Visibility::Visible:
                    ++visible;
                    queue.Submit(item);
                    break;
                case Visibility::RejectedByLayer: ++stats.byLayer; break;
                case Visibility::RejectedByDistance: ++stats.byDistance; break;
                case Visibility::RejectedByFrustum: ++stats.byFrustum; break;
            }
        }
        stats.visible += visible;
        if (render != nullptr) {
            render->submitted += candidates.size();
            render->culled += candidates.size() - visible;
        }
    }

    LodSelector::LodSelector(std::vector<float> distances, const float hysteresis)
        : distances_(std::move(distances)), hysteresis_(hysteresis)
    {
        if (!std::isfinite(hysteresis) || hysteresis < 0.0f || hysteresis >= 1.0f) {
            throw std::invalid_argument("LodSelector: hysteresis must be within [0, 1)");
        }
        for (std::size_t i = 0; i < distances_.size(); ++i) {
            if (!std::isfinite(distances_[i]) || distances_[i] <= 0.0f || (i > 0 && distances_[i] <= distances_[i - 1])) {
                throw std::invalid_argument("LodSelector: distances must be positive and strictly ascending");
            }
        }
    }

    int LodSelector::Select(const float distance, const int previous) const
    {
        const int count = static_cast<int>(distances_.size());
        if (previous < 0 || previous > count) {
            // A level switches only beyond its threshold, as with hysteresis.
            const auto it = std::lower_bound(distances_.begin(), distances_.end(), distance);
            return static_cast<int>(it - distances_.begin());
        }
        int level = previous;
        while (level < count && distance > distances_[static_cast<std::size_t>(level)] * (1.0f + hysteresis_)) {
            ++level;
        }
        while (level > 0 && distance < distances_[static_cast<std::size_t>(level - 1)] * (1.0f - hysteresis_)) {
            --level;
        }
        return level;
    }
}
