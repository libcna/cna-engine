// SPDX-License-Identifier: MIT
#include "CnaEngine/Extended/DebugOverlay.hpp"

#include "CNA/Extended/BitmapFonts/BitmapFontExtensions.hpp"
#include "CNA/Extended/ShapeExtensions.hpp"

#include <algorithm>
#include <cmath>

namespace CnaEngine::Extended
{
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Graphics::SpriteBatch;

    void DebugOverlay::AddFrame(const double seconds)
    {
        if (!std::isfinite(seconds) || seconds < 0.0) {
            return;
        }
        frames_.push_back(seconds);
        while (frames_.size() > static_cast<std::size_t>(std::max(1, style_.graphFrames))) {
            frames_.pop_front();
        }
    }

    Vector2 DebugOverlay::Measure(const CNA::Extended::BitmapFonts::BitmapFont* font,
                                  const std::vector<std::string>& lines) const
    {
        float width = static_cast<float>(std::max(1, style_.graphFrames));
        float height = style_.graphHeight;
        if (font != nullptr) {
            for (const auto& line : lines) {
                width = std::max(width, font->MeasureString(line).Width);
                height += static_cast<float>(font->getLineHeightProperty());
            }
        }
        return Vector2(width + 2.0f * style_.padding, height + 2.0f * style_.padding);
    }

    void DebugOverlay::Draw(SpriteBatch& spriteBatch, const CNA::Extended::BitmapFonts::BitmapFont* font,
                            const std::vector<std::string>& lines, const Vector2& position) const
    {
        const Vector2 size = Measure(font, lines);
        spriteBatch.Begin();
        CNA::Extended::FillRectangle(spriteBatch, position.X, position.Y, size.X, size.Y, style_.panel);

        // Bars grow upwards from the graph's baseline; twice the budget fills it.
        const float left = position.X + style_.padding;
        const float baseline = position.Y + style_.padding + style_.graphHeight;
        const double fullScale = 2.0 * std::max(1e-6, static_cast<double>(style_.budgetSeconds));
        float x = left + static_cast<float>(style_.graphFrames - static_cast<int>(frames_.size()));
        for (const double seconds : frames_) {
            const float height =
                static_cast<float>(std::min(1.0, seconds / fullScale)) * style_.graphHeight;
            if (height > 0.0f) {
                CNA::Extended::FillRectangle(spriteBatch, x, baseline - height, 1.0f, height,
                                             seconds > style_.budgetSeconds ? style_.slowBar : style_.bar);
            }
            x += 1.0f;
        }

        if (font != nullptr) {
            float y = baseline;
            for (const auto& line : lines) {
                CNA::Extended::BitmapFonts::DrawString(spriteBatch, *font, line, Vector2(left, y), style_.text, 0.0f);
                y += static_cast<float>(font->getLineHeightProperty());
            }
        }
        spriteBatch.End();
    }
}
