// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/UI/Font.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>

namespace CnaEngine::UI
{
    /// Where on the screen something sits.
    enum class Anchor : std::uint8_t
    {
        TopLeft,
        Top,
        TopRight,
        Left,
        Center,
        Right,
        BottomLeft,
        Bottom,
        BottomRight,
    };

    /// A rectangle in virtual units.
    struct Box
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        [[nodiscard]] bool Contains(const Microsoft::Xna::Framework::Vector2& point) const noexcept
        {
            return point.X >= x && point.Y >= y && point.X < x + width && point.Y < y + height;
        }
    };

    struct TextStyle
    {
        Microsoft::Xna::Framework::Color color = Microsoft::Xna::Framework::Color(255, 255, 255, 255);
        /// Times the font's own size (the built-in font is 7 units tall at 1).
        float scale = 2.0f;
        /// A drop shadow one unit down and right, for text over the world.
        bool shadow = true;
        Microsoft::Xna::Framework::Color shadowColor = Microsoft::Xna::Framework::Color(0, 0, 0, 170);
    };

    /// A frame's 2D layer on a virtual resolution: `virtualHeight` units
    /// tall whatever the window, as wide as the window's aspect makes it, so
    /// a HUD laid out once fits every window (house-simulator's 1600 x 900
    /// layout, school's and museum's Hud: text with a shadow, panels,
    /// placement by anchor).
    ///
    /// Between Begin and End it owns a SpriteBatch in alpha blending with
    /// point sampling; it leaves the device's other states alone.
    class Painter
    {
    public:
        explicit Painter(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, float virtualHeight = 720.0f);

        /// Starts drawing over the device's viewport.
        void Begin();
        void End();

        [[nodiscard]] Microsoft::Xna::Framework::Vector2 VirtualSize() const noexcept { return size_; }
        /// Pixels per virtual unit.
        [[nodiscard]] float Scale() const noexcept { return scale_; }
        /// A point in pixels (the mouse) in virtual units.
        [[nodiscard]] Microsoft::Xna::Framework::Vector2 ToVirtual(const Microsoft::Xna::Framework::Vector2& pixels) const noexcept;

        /// The box of `size` placed at `anchor`, `margin` from the edges it touches.
        [[nodiscard]] Box Place(Anchor anchor, const Microsoft::Xna::Framework::Vector2& size,
                                const Microsoft::Xna::Framework::Vector2& margin = Microsoft::Xna::Framework::Vector2(16.0f, 16.0f)) const noexcept;

        void Fill(const Box& box, Microsoft::Xna::Framework::Color color);
        void Text(const IFont& font, std::string_view text, const Microsoft::Xna::Framework::Vector2& position,
                  const TextStyle& style = {});
        /// Text placed by anchor; returns where it went.
        Box Text(const IFont& font, std::string_view text, Anchor anchor,
                 const Microsoft::Xna::Framework::Vector2& margin = Microsoft::Xna::Framework::Vector2(16.0f, 16.0f),
                 const TextStyle& style = {});
        /// The size `text` takes in a style.
        [[nodiscard]] static Microsoft::Xna::Framework::Vector2 Measure(const IFont& font, std::string_view text,
                                                                         const TextStyle& style = {});

        [[nodiscard]] Microsoft::Xna::Framework::Graphics::SpriteBatch& Batch() noexcept { return *batch_; }

    private:
        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> batch_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> white_;
        float virtualHeight_;
        float scale_ = 1.0f;
        Microsoft::Xna::Framework::Vector2 size_;
    };

    /// Short messages that fade after a while ("Saved", "Screenshot taken").
    class Toasts
    {
    public:
        void Show(std::string text, float seconds = 3.0f);
        void Update(float seconds);
        /// Stacked from `anchor`, the newest nearest the edge, fading in their last half second.
        void Draw(Painter& painter, const IFont& font, Anchor anchor = Anchor::Top, const TextStyle& style = {});
        [[nodiscard]] std::size_t Count() const noexcept { return toasts_.size(); }

    private:
        struct Toast
        {
            std::string text;
            float left;
        };
        std::deque<Toast> toasts_;
    };
}
