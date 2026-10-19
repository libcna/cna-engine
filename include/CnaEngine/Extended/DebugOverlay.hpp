// SPDX-License-Identifier: MIT
//
// Enhanced profile (ADR-001): this header depends on cna-extended and must
// never be included from CnaEngine::Core or CnaEngine::Graphics.
#pragma once

#include "CnaEngine/Diagnostics/FrameStatistics.hpp"

#include "CNA/Extended/BitmapFonts/BitmapFont.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <deque>
#include <string>
#include <vector>

namespace CnaEngine::Extended
{
    /// On-screen presentation of engine diagnostics using cna-extended's
    /// shape and bitmap-font drawing.
    ///
    /// The overlay only presents; the metrics come from
    /// Graphics::FormatDiagnostics() and Diagnostics::FrameStatistics, which
    /// work without any overlay. Game-specific benchmark UI stays in the game.
    class DebugOverlay
    {
    public:
        struct Style
        {
            Microsoft::Xna::Framework::Color panel = Microsoft::Xna::Framework::Color(0, 0, 0, 160);
            Microsoft::Xna::Framework::Color text = Microsoft::Xna::Framework::Color(255, 255, 255, 255);
            Microsoft::Xna::Framework::Color bar = Microsoft::Xna::Framework::Color(80, 200, 120, 255);
            /// Bars for frames slower than `budgetSeconds` use this color.
            Microsoft::Xna::Framework::Color slowBar = Microsoft::Xna::Framework::Color(230, 80, 60, 255);
            float budgetSeconds = 1.0f / 60.0f;
            float padding = 6.0f;
            float graphHeight = 40.0f;
            /// Graph width in pixels; one bar per frame.
            int graphFrames = 120;
        };

        DebugOverlay() = default;
        explicit DebugOverlay(const Style& style) : style_(style) {}

        /// Records a frame time for the graph (independent of FrameStatistics).
        void AddFrame(double seconds);
        [[nodiscard]] const std::deque<double>& Frames() const noexcept { return frames_; }

        /// Draws the panel at `position`: the frame-time graph, then one line
        /// per entry of `lines` when a font is given (a null font draws the
        /// graph only). Calls Begin/End on `spriteBatch` itself.
        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& spriteBatch,
                  const CNA::Extended::BitmapFonts::BitmapFont* font, const std::vector<std::string>& lines,
                  const Microsoft::Xna::Framework::Vector2& position) const;

        /// Panel size for the given content, for layout.
        [[nodiscard]] Microsoft::Xna::Framework::Vector2 Measure(const CNA::Extended::BitmapFonts::BitmapFont* font,
                                                                 const std::vector<std::string>& lines) const;

    private:
        Style style_;
        std::deque<double> frames_;
    };
}
