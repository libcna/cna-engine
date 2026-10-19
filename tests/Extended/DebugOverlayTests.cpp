// SPDX-License-Identifier: MIT
#include "CnaEngine/Extended/DebugOverlay.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"

#include <gtest/gtest.h>

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
}
