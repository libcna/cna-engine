// SPDX-License-Identifier: MIT
#include "CnaEngine/Core/FixedStepClock.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CnaEngine
{
    FixedStepClock::FixedStepClock(const double stepSeconds, const double maxFrameSeconds, const int maxStepsPerFrame)
        : stepSeconds_(stepSeconds), maxFrameSeconds_(maxFrameSeconds), maxStepsPerFrame_(maxStepsPerFrame)
    {
        if (!(stepSeconds > 0.0) || !std::isfinite(stepSeconds) || !(maxFrameSeconds >= 0.0)
            || !std::isfinite(maxFrameSeconds) || maxStepsPerFrame <= 0) {
            throw std::invalid_argument("FixedStepClock: invalid step, frame clamp or step limit");
        }
    }

    int FixedStepClock::Advance(const double frameSeconds)
    {
        const double frame = std::isfinite(frameSeconds) ? std::clamp(frameSeconds, 0.0, maxFrameSeconds_) : 0.0;
        accumulator_ += frame;
        int steps = 0;
        while (accumulator_ >= stepSeconds_ && steps < maxStepsPerFrame_) {
            accumulator_ -= stepSeconds_;
            ++steps;
        }
        if (steps == maxStepsPerFrame_ && accumulator_ >= stepSeconds_) {
            ++saturatedFrames_;
        }
        totalSteps_ += static_cast<std::uint64_t>(steps);
        return steps;
    }

    double FixedStepClock::Alpha() const noexcept
    {
        return std::min(accumulator_ / stepSeconds_, 1.0);
    }

    void FixedStepClock::Reset() noexcept
    {
        accumulator_ = 0.0;
        totalSteps_ = 0;
        saturatedFrames_ = 0;
    }
}
