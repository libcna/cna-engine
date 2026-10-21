// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/ScreenCapture.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace CnaEngine::Graphics
{
    /// The command-line options an automated run (smoke test, capture,
    /// benchmark) shares across games.
    struct AutomatedRunOptions
    {
        /// --frames N: stop once N frames have been drawn (0: no limit).
        int frames = 0;
        /// --run-seconds S: stop once S simulated seconds have passed (0: no limit).
        double runSeconds = 0.0;
        /// --screenshot PATH: save the last frame as a PNG before stopping;
        /// without a frame or time limit, the first frame is the last.
        std::string screenshotPath;
        /// --lockstep: exactly one simulation step of the fixed length per
        /// drawn frame, so a capture at frame N shows the same scene however
        /// fast the renderer is (a fixed-step loop otherwise catches up after
        /// a slow frame with as many steps as the wall clock asks for).
        bool lockstep = false;

        /// Whether the run stops by itself (a frame or time limit).
        [[nodiscard]] bool Finite() const noexcept { return frames > 0 || runSeconds > 0.0; }
        /// Whether this is an automated run at all (a limit or a screenshot):
        /// such runs skip start menus and the like.
        [[nodiscard]] bool Automated() const noexcept { return Finite() || !screenshotPath.empty(); }
    };

    /// Reads one shared option at `args[index]`, advancing `index` past its
    /// value. Returns false, leaving `index` alone, for an argument that is not
    /// one of them; throws std::invalid_argument for a missing or bad value
    /// (negative frames or seconds, an empty path).
    bool ParseAutomatedRunOption(std::span<const std::string_view> args, std::size_t& index,
                                 AutomatedRunOptions& options);

    /// The usage lines of the shared options, for a game's --help.
    [[nodiscard]] std::string_view AutomatedRunUsage() noexcept;

    /// Drives an automated run from a game's Update and Draw:
    ///
    ///     void Update(GameTime& time) {
    ///         const auto step = run_.BeginUpdate(measured, TargetElapsedTime);
    ///         if (!step) return;            // a lockstep catch-up, or the run is over
    ///         Simulate(*step);
    ///     }
    ///     void Draw(const GameTime& time) {
    ///         if (run_.BeginDraw(device)) { Exit(); return; }
    ///         ... draw the frame ...
    ///         run_.EndDraw(avatarsReady);
    ///     }
    ///
    /// Once the limit is reached the simulation stops (BeginUpdate gives
    /// nothing) while frames go on being drawn until `ready` holds -- content
    /// still loading in the background must not be photographed half-done. The
    /// screenshot is then read from the completed frame at the start of the
    /// next Draw, after it was presented: reading the back buffer before
    /// Present is intermittent on some offscreen contexts (train-simulator's
    /// finding).
    class AutomatedRun
    {
    public:
        explicit AutomatedRun(AutomatedRunOptions options);

        [[nodiscard]] const AutomatedRunOptions& Options() const noexcept { return options_; }

        /// The seconds this Update advances the simulation: `measuredSeconds`,
        /// or `fixedStepSeconds` in lockstep. Nothing for an Update the run
        /// skips: a second one before the next Draw in lockstep, or any once
        /// the limit has been reached.
        [[nodiscard]] std::optional<double> BeginUpdate(double measuredSeconds, double fixedStepSeconds);

        /// Call first in Draw. Saves a pending screenshot (of the frame drawn
        /// last) and returns true when the run is over and the game should exit
        /// instead of drawing.
        [[nodiscard]] bool BeginDraw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        /// Call after drawing a frame. Counts it; once the limit is reached and
        /// `ready` holds, the run ends at the next BeginDraw (after saving the
        /// screenshot, when one was asked for).
        void EndDraw(bool ready = true);

        [[nodiscard]] int FramesDrawn() const noexcept { return framesDrawn_; }
        [[nodiscard]] double SimulatedSeconds() const noexcept { return simulatedSeconds_; }
        /// The limit has been reached: the simulation is frozen.
        [[nodiscard]] bool LimitReached() const noexcept { return limitReached_; }
        /// The outcome of the screenshot, once taken.
        [[nodiscard]] const std::optional<CaptureResult>& Screenshot() const noexcept { return screenshot_; }

    private:
        AutomatedRunOptions options_;
        int framesDrawn_ = 0;
        int updatesSinceDraw_ = 0;
        double simulatedSeconds_ = 0.0;
        bool limitReached_ = false;
        bool finished_ = false;
        std::optional<CaptureResult> screenshot_;
    };
}
