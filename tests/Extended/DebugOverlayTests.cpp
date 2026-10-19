// SPDX-License-Identifier: MIT
#include "CnaEngine/Extended/DebugOverlay.hpp"

#include "CNA/Extended/BitmapFonts/BitmapFontCharacter.hpp"
#include "CNA/Extended/Graphics/Texture2DRegion.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace CnaEngine::Extended
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::SpriteBatch;

    TEST(DebugOverlayTests, GraphKeepsTheLastFramesOnly)
    {
        DebugOverlay::Style style;
        style.graphFrames = 3;
        DebugOverlay overlay(style);
        for (const double frame : {0.01, 0.02, 0.03, 0.04, -1.0}) {
            overlay.AddFrame(frame);
        }
        EXPECT_EQ(overlay.Frames(), (std::deque<double>{0.02, 0.03, 0.04}));
        const Vector2 size = overlay.Measure(nullptr, {"ignored without a font"});
        EXPECT_FLOAT_EQ(size.X, 3.0f + 2.0f * style.padding);
        EXPECT_FLOAT_EQ(size.Y, style.graphHeight + 2.0f * style.padding);
    }

    TEST(DebugOverlayTests, DrawsPanelAndColoredBars)
    {
        GraphicsDevice device;
        constexpr int Size = 64;
        RenderTarget2D target(device, Size, Size);
        SpriteBatch spriteBatch(device);

        DebugOverlay::Style style;
        style.graphFrames = 40;
        style.graphHeight = 20.0f;
        style.padding = 4.0f;
        style.panel = Color(0, 0, 255, 255);
        DebugOverlay overlay(style);
        for (int i = 0; i < 40; ++i) {
            overlay.AddFrame(i < 20 ? style.budgetSeconds * 0.5 : style.budgetSeconds * 1.8);
        }

        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        overlay.Draw(spriteBatch, nullptr, {}, Vector2(2.0f, 2.0f));
        device.SetRenderTarget(nullptr);
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        const auto at = [&](int x, int y) { return pixels[static_cast<std::size_t>(y * Size + x)]; };

        const int baseline = 2 + 4 + 20;
        EXPECT_EQ(at(3, 3), style.panel) << "panel background";
        EXPECT_EQ(at(2 + 4 + 5, baseline - 2), style.bar) << "a fast frame";
        EXPECT_EQ(at(2 + 4 + 30, baseline - 2), style.slowBar) << "a frame over budget";
        EXPECT_EQ(at(2 + 4 + 5, baseline - 15), style.panel) << "fast bars are short";
        EXPECT_EQ(at(2 + 4 + 30, baseline - 15), style.slowBar) << "slow bars are tall";
        EXPECT_EQ(at(60, 60), Color(0, 0, 0, 255)) << "outside the panel";
    }

    TEST(DebugOverlayTests, DrawsTextLinesWithAGameFont)
    {
        using CNA::Extended::BitmapFonts::BitmapFont;
        using CNA::Extended::BitmapFonts::BitmapFontCharacter;
        using CNA::Extended::Graphics::Texture2DRegion;
        using Microsoft::Xna::Framework::Rectangle;
        using Microsoft::Xna::Framework::Graphics::Texture2D;

        GraphicsDevice device;
        constexpr int Size = 64;
        RenderTarget2D target(device, Size, Size);
        SpriteBatch spriteBatch(device);

        // A one-glyph font built in code: 'X' is a solid 4x6 block.
        Texture2D page(device, 4, 6);
        const std::vector<Color> solid(24, Color(255, 255, 255, 255));
        page.SetData(solid.data(), 24);
        auto glyph = std::make_shared<BitmapFontCharacter>(
            'X', std::make_shared<Texture2DRegion>(&page, Rectangle(0, 0, 4, 6)), 0, 0, 5);
        const BitmapFont font("probe", 6, 8, std::vector<std::shared_ptr<BitmapFontCharacter>>{glyph});

        DebugOverlay::Style style;
        style.graphFrames = 30;
        style.graphHeight = 10.0f;
        style.padding = 2.0f;
        style.panel = Color(0, 0, 255, 255);
        style.text = Color(255, 255, 0, 255);
        DebugOverlay overlay(style);
        const std::vector<std::string> lines = {"XX", "X"};
        const Vector2 size = overlay.Measure(&font, lines);
        EXPECT_FLOAT_EQ(size.Y, 10.0f + 2.0f * 8.0f + 4.0f) << "graph plus two text lines";

        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        overlay.Draw(spriteBatch, &font, lines, Vector2(0.0f, 0.0f));
        device.SetRenderTarget(nullptr);
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        const auto at = [&](int x, int y) { return pixels[static_cast<std::size_t>(y * Size + x)]; };

        const int firstLine = 2 + 10;      // padding + graph
        const int secondLine = firstLine + 8;
        EXPECT_EQ(at(2 + 1, firstLine + 2), style.text) << "first glyph of line 1";
        EXPECT_EQ(at(2 + 5 + 1, firstLine + 2), style.text) << "second glyph of line 1";
        EXPECT_EQ(at(2 + 4, firstLine + 2), style.panel) << "gap between glyphs";
        EXPECT_EQ(at(2 + 1, secondLine + 2), style.text) << "line 2";
        EXPECT_EQ(at(2 + 5 + 1, secondLine + 2), style.panel) << "line 2 has one glyph";
    }
}
