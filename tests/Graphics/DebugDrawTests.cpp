// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/DebugDraw.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::BlendState;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::RasterizerState;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;

    TEST(DebugDrawTests, QueuesLinesAndBoxEdges)
    {
        DebugDraw debug;
        debug.Line(Vector3::Zero, Vector3::UnitX, Color(255, 0, 0, 255));
        debug.Box(Vector3::Zero, Vector3(2.0f, 2.0f, 2.0f));
        EXPECT_EQ(debug.LineCount(), 13u);
        EXPECT_EQ(debug.Vertices()[0].Color, Color(255, 0, 0, 255));
        debug.Box(BoundingBox(Vector3::Zero, Vector3::One));
        EXPECT_EQ(debug.LineCount(), 25u);
        debug.Clear();
        EXPECT_EQ(debug.LineCount(), 0u);
    }

    TEST(DebugDrawTests, DrawsLinesAndRestoresTheDeviceState)
    {
        constexpr int size = 64;
        GraphicsDevice device;
        RenderTarget2D target(device, size, size);
        device.SetRenderTarget(&target);
        device.Clear(Color(0, 0, 0, 255));
        device.setBlendStateProperty(BlendState::Additive);
        device.setRasterizerStateProperty(RasterizerState::CullClockwise);

        Camera camera;
        camera.SetOrthographic(2.0f, 2.0f, 0.1f, 10.0f);
        camera.SetLookAt(Vector3(0.0f, 0.0f, 5.0f), Vector3::Zero);
        DebugDraw debug;
        debug.Line(Vector3(-1.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Color(0, 255, 0, 255));
        EXPECT_EQ(debug.Draw(device, camera, false), 1u);
        EXPECT_EQ(device.getBlendStateProperty().getNameProperty(), BlendState::Additive.getNameProperty());
        EXPECT_EQ(device.getRasterizerStateProperty().getNameProperty(),
                  RasterizerState::CullClockwise.getNameProperty());
        device.SetRenderTarget(nullptr);

        std::vector<Color> pixels(static_cast<std::size_t>(size * size));
        target.GetData(pixels.data(), static_cast<int>(pixels.size()));
        int green = 0;
        for (int y = size / 2 - 1; y <= size / 2; ++y) {
            for (int x = 0; x < size; ++x) {
                green += pixels[static_cast<std::size_t>(y * size + x)].getGProperty() > 128 ? 1 : 0;
            }
        }
        EXPECT_GE(green, size - 4) << "the line crosses the whole view";
        EXPECT_EQ(pixels[0], Color(0, 0, 0, 255)) << "nothing else is drawn";
        EXPECT_EQ(DebugDraw{}.Draw(device, camera), 0u);
    }
}
