// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"
#include "CnaEngine/Graphics/RenderStatistics.hpp"

#include "Microsoft/Xna/Framework/BoundingSphere.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace CnaEngine::Graphics
{
    /// Independent visibility policies; each can be switched off on its own.
    struct VisibilitySettings
    {
        /// Items sharing no bit with this mask are rejected.
        std::uint32_t layerMask = 0xFFFFFFFFu;
        /// Items whose bounds lie entirely beyond this distance are rejected.
        float maxDistance = std::numeric_limits<float>::infinity();
        /// Reject items entirely outside the camera frustum.
        bool frustum = true;
    };

    enum class Visibility : std::uint8_t
    {
        Visible,
        RejectedByLayer,
        RejectedByDistance,
        RejectedByFrustum,
    };

    /// Classifies bounds conservatively: an item is rejected only when it is
    /// certainly invisible (partially inside counts as visible). Policies are
    /// tested in the order layer, distance, frustum.
    [[nodiscard]] Visibility Classify(const Microsoft::Xna::Framework::BoundingSphere& bounds, std::uint32_t layers,
                                      const Camera& camera, const VisibilitySettings& settings);

    /// Counts per rejection reason, for diagnostics.
    struct CullStatistics
    {
        std::uint64_t considered = 0;
        std::uint64_t visible = 0;
        std::uint64_t byLayer = 0;
        std::uint64_t byDistance = 0;
        std::uint64_t byFrustum = 0;
    };

    /// Submits the visible `candidates` to `queue`. Adds to `stats` and, when
    /// given, to `render` (submitted/culled).
    void CullInto(std::span<const RenderItem> candidates, const Camera& camera, const VisibilitySettings& settings,
                  RenderQueue& queue, CullStatistics& stats, RenderStatistics* render = nullptr);

    /// Distance-based level-of-detail thresholds with hysteresis.
    ///
    /// `distances[i]` is where level i switches to level i + 1 (ascending).
    /// Moving away, a switch happens beyond distance * (1 + hysteresis);
    /// moving closer, below distance * (1 - hysteresis). Small camera
    /// movements around a threshold therefore never flip the level back and
    /// forth.
    class LodSelector
    {
    public:
        explicit LodSelector(std::vector<float> distances, float hysteresis = 0.1f);

        [[nodiscard]] int LevelCount() const noexcept { return static_cast<int>(distances_.size()) + 1; }
        /// Level for `distance` given the item's previous level (-1 when it
        /// has none: no hysteresis is applied).
        [[nodiscard]] int Select(float distance, int previous = -1) const;

    private:
        std::vector<float> distances_;
        float hysteresis_;
    };
}
