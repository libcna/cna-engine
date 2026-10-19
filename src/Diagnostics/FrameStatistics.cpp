// SPDX-License-Identifier: MIT
#include "CnaEngine/Diagnostics/FrameStatistics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CnaEngine::Diagnostics
{
    FrameStatistics::FrameStatistics(const std::size_t window) : samples_(window, 0.0)
    {
        if (window == 0) {
            throw std::invalid_argument("FrameStatistics: window must hold at least one frame");
        }
    }

    void FrameStatistics::AddFrame(const double seconds)
    {
        if (!std::isfinite(seconds) || seconds < 0.0) {
            ++rejected_;
            return;
        }
        if (count_ == samples_.size()) {
            sum_ -= samples_[next_];
        } else {
            ++count_;
        }
        samples_[next_] = seconds;
        sum_ += seconds;
        next_ = (next_ + 1) % samples_.size();
        last_ = seconds;
        ++frameCount_;
        // Re-sum periodically so floating-point drift cannot accumulate.
        if (frameCount_ % 4096 == 0) {
            sum_ = 0.0;
            for (std::size_t i = 0; i < count_; ++i) {
                sum_ += samples_[i];
            }
        }
    }

    void FrameStatistics::Clear()
    {
        std::fill(samples_.begin(), samples_.end(), 0.0);
        next_ = 0;
        count_ = 0;
        sum_ = 0.0;
        last_ = 0.0;
        frameCount_ = 0;
        rejected_ = 0;
    }

    double FrameStatistics::AverageSeconds() const noexcept
    {
        return count_ == 0 ? 0.0 : sum_ / static_cast<double>(count_);
    }

    double FrameStatistics::MinSeconds() const noexcept
    {
        return count_ == 0 ? 0.0 : *std::min_element(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(count_));
    }

    double FrameStatistics::MaxSeconds() const noexcept
    {
        return count_ == 0 ? 0.0 : *std::max_element(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(count_));
    }

    double FrameStatistics::FramesPerSecond() const noexcept
    {
        const double average = AverageSeconds();
        return average > 0.0 ? 1.0 / average : 0.0;
    }

    double FrameStatistics::PercentileSeconds(const double fraction) const
    {
        if (count_ == 0) {
            return 0.0;
        }
        std::vector<double> sorted(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(count_));
        std::sort(sorted.begin(), sorted.end());
        const double clamped = std::clamp(fraction, 0.0, 1.0);
        const auto index = static_cast<std::size_t>(std::ceil(clamped * static_cast<double>(count_))) ;
        return sorted[index == 0 ? 0 : index - 1];
    }
}
