// SPDX-License-Identifier: MIT
#include "CnaEngine/UI/Painter.hpp"

#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace CnaEngine::UI
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector2;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    TEST(FontTest, TheBuiltInFontMeasuresInWholeCells)
    {
        Xna::GraphicsDevice device;
        const BuiltInFont font(device);
        EXPECT_EQ(font.Measure("").X, 0.0f);
        EXPECT_EQ(font.Measure("A").X, 5.0f);
        EXPECT_EQ(font.Measure("AB").X, 11.0f);
        EXPECT_EQ(font.Measure("AB").Y, 7.0f);
        EXPECT_EQ(font.Measure("ABC\nA").X, 17.0f);
        EXPECT_EQ(font.Measure("A\nB").Y, 16.0f);
        // An unknown character is drawn as '?'.
        EXPECT_EQ(BuiltInFont::Rows('\x01'), BuiltInFont::Rows('?'));
        EXPECT_NE(BuiltInFont::Rows('a'), BuiltInFont::Rows('A'));
    }

    // "HI" at scale 2 from (2, 2): H's left column and I's middle column are
    // lit, the gap between the letters is not.
    TEST(FontTest, TextIsDrawnWhereItIsPlaced)
    {
        Xna::GraphicsDevice device;
        Xna::RenderTarget2D target(device, 32, 24, false, Xna::SurfaceFormat::Color, Xna::DepthFormat::None);
        const BuiltInFont font(device);
        Painter painter(device, 24.0f); // one virtual unit per pixel
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        painter.Begin();
        TextStyle style;
        style.scale = 2.0f;
        style.shadow = false;
        painter.Text(font, "HI", Vector2(2.0f, 2.0f), style);
        painter.End();
        device.SetRenderTarget(nullptr);
        std::vector<Color> pixels(32 * 24);
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        auto lit = [&](const int x, const int y) { return pixels[static_cast<std::size_t>(y * 32 + x)].getRProperty() > 128; };
        EXPECT_TRUE(lit(2, 2));   // H, top of the left stroke
        EXPECT_TRUE(lit(3, 14));  // H, bottom of the left stroke
        EXPECT_TRUE(lit(8, 8));   // H's bar
        EXPECT_FALSE(lit(4, 4));  // inside the H
        EXPECT_FALSE(lit(13, 8)); // between the letters
        EXPECT_TRUE(lit(18, 8));  // I's stem: 12 + 2 * 2 + 2
        EXPECT_FALSE(lit(30, 20));
    }

    TEST(PainterTest, AnchorsPlaceBoxesOnAVirtualScreen)
    {
        Xna::GraphicsDevice device; // 800 x 480 back buffer by default
        Painter painter(device, 240.0f);
        painter.Begin();
        painter.End();
        const Vector2 size = painter.VirtualSize();
        EXPECT_FLOAT_EQ(size.Y, 240.0f);
        EXPECT_NEAR(size.X / size.Y, 800.0f / 480.0f, 1e-4f);
        EXPECT_FLOAT_EQ(painter.Scale(), 2.0f);
        const Vector2 box(40.0f, 10.0f);
        const Box topLeft = painter.Place(Anchor::TopLeft, box, Vector2(5.0f, 6.0f));
        EXPECT_FLOAT_EQ(topLeft.x, 5.0f);
        EXPECT_FLOAT_EQ(topLeft.y, 6.0f);
        const Box centre = painter.Place(Anchor::Center, box);
        EXPECT_FLOAT_EQ(centre.x, (size.X - 40.0f) * 0.5f);
        EXPECT_FLOAT_EQ(centre.y, 115.0f);
        const Box bottomRight = painter.Place(Anchor::BottomRight, box, Vector2(5.0f, 6.0f));
        EXPECT_FLOAT_EQ(bottomRight.x, size.X - 45.0f);
        EXPECT_FLOAT_EQ(bottomRight.y, 240.0f - 16.0f);
        EXPECT_TRUE(bottomRight.Contains(Vector2(bottomRight.x + 1.0f, bottomRight.y + 1.0f)));
        const Vector2 virtualPoint = painter.ToVirtual(Vector2(100.0f, 60.0f));
        EXPECT_FLOAT_EQ(virtualPoint.X, 50.0f);
        EXPECT_FLOAT_EQ(virtualPoint.Y, 30.0f);
    }

    TEST(PainterTest, ToastsFadeAndLeave)
    {
        Toasts toasts;
        toasts.Show("Saved", 1.0f);
        toasts.Show("Screenshot", 3.0f);
        EXPECT_EQ(toasts.Count(), 2u);
        toasts.Update(1.5f);
        EXPECT_EQ(toasts.Count(), 1u);
        toasts.Update(2.0f);
        EXPECT_EQ(toasts.Count(), 0u);
    }
}
