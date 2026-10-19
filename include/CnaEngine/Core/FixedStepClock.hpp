// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace CnaEngine
{
    /// Fixed-step simulation clock ("fix your timestep").
    ///
    /// Each frame's elapsed time is clamped to `maxFrameSeconds` (a stall,
    /// a debugger break or a loading hitch must not fast-forward the
    /// simulation) and added to an accumulator; whole steps are taken while
    /// the accumulator holds at least one step, at most `maxStepsPerFrame`.
    /// Time left over stays for the next frame, and Alpha() tells a renderer
    /// how far the presentation is between the last and the next step.
    ///
    /// This is car-simulator's vehicle loop (1/120 s steps, 0.25 s frame clamp,
    /// at most 30 steps) as a reusable policy. Time is accumulated in double
    /// precision.
    class FixedStepClock
    {
    public:
        /// Throws std::invalid_argument unless stepSeconds > 0,
        /// maxFrameSeconds >= 0 and maxStepsPerFrame > 0.
        explicit FixedStepClock(double stepSeconds, double maxFrameSeconds = 0.25, int maxStepsPerFrame = 30);

        /// Adds one frame of `frameSeconds` (negative or non-finite counts as
        /// zero) and returns the number of steps to simulate now.
        int Advance(double frameSeconds);

        /// Advance() and call `step(StepSeconds())` once per step; returns
        /// the number of steps taken.
        template <typename Step>
        int Run(double frameSeconds, Step&& step)
        {
            const int steps = Advance(frameSeconds);
            for (int i = 0; i < steps; ++i) {
                step(stepSeconds_);
            }
            return steps;
        }

        [[nodiscard]] double StepSeconds() const noexcept { return stepSeconds_; }
        /// Fraction of a step accumulated but not yet simulated, in [0, 1).
        [[nodiscard]] double Alpha() const noexcept;
        [[nodiscard]] double Accumulated() const noexcept { return accumulator_; }
        [[nodiscard]] std::uint64_t TotalSteps() const noexcept { return totalSteps_; }
        /// Simulated time: TotalSteps() * StepSeconds().
        [[nodiscard]] double SimulatedSeconds() const noexcept
        {
            return static_cast<double>(totalSteps_) * stepSeconds_;
        }
        /// Frames whose steps were capped by maxStepsPerFrame.
        [[nodiscard]] std::uint64_t SaturatedFrames() const noexcept { return saturatedFrames_; }

        void Reset() noexcept;

    private:
        double stepSeconds_;
        double maxFrameSeconds_;
        int maxStepsPerFrame_;
        double accumulator_ = 0.0;
        std::uint64_t totalSteps_ = 0;
        std::uint64_t saturatedFrames_ = 0;
    };
}
