// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <memory>
#include <string_view>

namespace CnaEngine::UI
{
    /// Text a SpriteBatch can draw: a game's own SpriteFont, or the
    /// engine's built-in one when it has none.
    class IFont
    {
    public:
        virtual ~IFont() = default;
        /// The size of `text` at scale 1 (one line; '\n' starts another).
        [[nodiscard]] virtual Microsoft::Xna::Framework::Vector2 Measure(std::string_view text) const = 0;
        [[nodiscard]] virtual float LineHeight() const = 0;
        /// Draws `text` with its top-left corner at `position`. The batch has begun.
        virtual void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch, std::string_view text,
                          Microsoft::Xna::Framework::Vector2 position, Microsoft::Xna::Framework::Color color,
                          float scale = 1.0f) const = 0;
    };

    /// A 5 x 7 pixel font of the 95 printable ASCII characters in an
    /// 8 x 8 cell, built in code: text without any content file. Other
    /// characters draw as '?'. Island-, room-, car- and train-simulator each
    /// keep such a table (docs/game-facilities-survey.md); this one is the
    /// engine's own.
    class BuiltInFont final : public IFont
    {
    public:
        explicit BuiltInFont(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        [[nodiscard]] Microsoft::Xna::Framework::Vector2 Measure(std::string_view text) const override;
        [[nodiscard]] float LineHeight() const override { return 9.0f; }
        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch, std::string_view text,
                  Microsoft::Xna::Framework::Vector2 position, Microsoft::Xna::Framework::Color color,
                  float scale = 1.0f) const override;

        /// The bits of a glyph's rows, top first, bit 4 the leftmost column.
        [[nodiscard]] static const unsigned char* Rows(char character) noexcept;

    private:
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> atlas_;
    };

    /// XNA's SpriteFont (from the content manager) as an IFont; borrowed.
    class SpriteFontText final : public IFont
    {
    public:
        explicit SpriteFontText(const Microsoft::Xna::Framework::Graphics::SpriteFont& font) : font_(&font) {}

        [[nodiscard]] Microsoft::Xna::Framework::Vector2 Measure(std::string_view text) const override;
        [[nodiscard]] float LineHeight() const override;
        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch, std::string_view text,
                  Microsoft::Xna::Framework::Vector2 position, Microsoft::Xna::Framework::Color color,
                  float scale = 1.0f) const override;

    private:
        const Microsoft::Xna::Framework::Graphics::SpriteFont* font_;
    };
}
