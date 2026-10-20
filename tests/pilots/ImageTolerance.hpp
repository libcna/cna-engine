// SPDX-License-Identifier: MIT
//
// Image comparison for the pilot scene comparisons, and the screenshot
// tolerance they enforce.
//
// The tolerance is a PROPOSAL for MILESTONE-2 ("screenshot comparisons have
// agreed tolerances"), derived from the measured differences in
// docs/pilots/screenshot-tolerances.md; it becomes the agreed tolerance when
// the repository owner accepts it. Until then it guards against regressions:
// a comparison whose image leaves the tolerance fails like one whose
// visibility differs.
#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <vector>

namespace CnaEngine::Pilot
{
    struct ImageDifference
    {
        /// Mean absolute difference per channel, in 1/255 steps.
        double mean = 0.0;
        /// Largest difference of any channel.
        int largest = 0;
        /// Pixels that differ at all.
        std::size_t differing = 0;
        /// Pixels whose largest channel difference exceeds VisibleStep:
        /// differences a viewer could notice, unlike one-level rounding.
        std::size_t visible = 0;
        std::size_t pixels = 0;
    };

    /// A channel difference above this many 1/255 steps counts as visible.
    inline constexpr int VisibleStep = 3;

    /// Proposed screenshot tolerance (see the header comment).
    struct ImageTolerance
    {
        double maxMean = 0.5;
        /// Share of pixels allowed to differ visibly.
        double maxVisibleShare = 0.01;
    };

    [[nodiscard]] inline ImageDifference Compare(const std::vector<Microsoft::Xna::Framework::Color>& expected,
                                                 const std::vector<Microsoft::Xna::Framework::Color>& actual)
    {
        ImageDifference d;
        d.pixels = std::min(expected.size(), actual.size());
        double sum = 0.0;
        for (std::size_t i = 0; i < d.pixels; ++i) {
            const int r = std::abs(expected[i].getRProperty() - actual[i].getRProperty());
            const int g = std::abs(expected[i].getGProperty() - actual[i].getGProperty());
            const int b = std::abs(expected[i].getBProperty() - actual[i].getBProperty());
            const int most = std::max({r, g, b});
            sum += r + g + b;
            d.largest = std::max(d.largest, most);
            d.differing += most > 0 ? 1u : 0u;
            d.visible += most > VisibleStep ? 1u : 0u;
        }
        d.mean = d.pixels == 0 ? 0.0 : sum / static_cast<double>(d.pixels * 3);
        return d;
    }

    [[nodiscard]] inline bool Within(const ImageDifference& d, const ImageTolerance& tolerance = {})
    {
        return d.mean <= tolerance.maxMean
               && static_cast<double>(d.visible) <= tolerance.maxVisibleShare * static_cast<double>(d.pixels);
    }
}
