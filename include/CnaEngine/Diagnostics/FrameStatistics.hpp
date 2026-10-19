// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace CnaEngine::Diagnostics
{
    /// Rolling frame-time statistics over the last `window` frames.
    ///
    /// Frame durations are supplied by the caller (typically
    /// GameTime::getElapsedGameTimeProperty() or a measured wall-clock delta),
    /// so the statistics are deterministic under fixed-step or replayed time.
    class FrameStatistics
    {
    public:
        explicit FrameStatistics(std::size_t window = 120);

        /// Records one frame of `seconds` duration (negative or non-finite
        /// values are ignored and counted as rejected).
        void AddFrame(double seconds);
        void Clear();

        /// Frames recorded since construction or Clear().
        [[nodiscard]] std::uint64_t FrameCount() const noexcept { return frameCount_; }
        [[nodiscard]] std::uint64_t RejectedCount() const noexcept { return rejected_; }
        /// Number of frames currently in the window.
        [[nodiscard]] std::size_t SampleCount() const noexcept { return count_; }

        /// Statistics over the window; all zero when it is empty.
        [[nodiscard]] double LastSeconds() const noexcept { return last_; }
        [[nodiscard]] double AverageSeconds() const noexcept;
        [[nodiscard]] double MinSeconds() const noexcept;
        [[nodiscard]] double MaxSeconds() const noexcept;
        /// Frames per second derived from the average frame time.
        [[nodiscard]] double FramesPerSecond() const noexcept;
        /// Frame time at `fraction` (0..1) of the sorted window, e.g. 0.99 for
        /// the 99th percentile ("1 % low" frame time).
        [[nodiscard]] double PercentileSeconds(double fraction) const;

    private:
        std::vector<double> samples_;
        std::size_t next_ = 0;
        std::size_t count_ = 0;
        double sum_ = 0.0;
        double last_ = 0.0;
        std::uint64_t frameCount_ = 0;
        std::uint64_t rejected_ = 0;
    };
}
