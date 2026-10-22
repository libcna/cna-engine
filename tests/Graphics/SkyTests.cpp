// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Sky.hpp"

#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        constexpr int Size = 64;

        void ExpectNear(const Vector3& actual, const Vector3& expected, const float tolerance = 1e-5f)
        {
            EXPECT_NEAR(actual.X, expected.X, tolerance);
            EXPECT_NEAR(actual.Y, expected.Y, tolerance);
            EXPECT_NEAR(actual.Z, expected.Z, tolerance);
        }

        // A dome with plain colours: red horizon, blue zenith, green ground.
        SkyColors Plain()
        {
            SkyColors colors;
            colors.zenith = Vector3(0.0f, 0.0f, 1.0f);
            colors.horizon = Vector3(1.0f, 0.0f, 0.0f);
            colors.ground = Vector3(0.0f, 1.0f, 0.0f);
            colors.glow = Vector3::Zero;
            colors.sunDisc = Vector3::Zero;
            colors.stars = 0.0f;
            return colors;
        }

        class SkyDomeTest : public ::testing::Test
        {
        protected:
            SkyDomeTest()
                : target(device, Size, Size, false, Xna::SurfaceFormat::Color, Xna::DepthFormat::Depth24), dome(device)
            {
                camera.SetPerspective(Microsoft::Xna::Framework::MathHelper::PiOver2, 1.0f, 0.1f, 100.0f);
                camera.SetPosition(Vector3(3.0f, 1.0f, -2.0f)); // the dome follows the camera
            }

            std::vector<Color> Draw()
            {
                device.SetRenderTarget(&target);
                device.Clear(Color(0, 0, 0, 255));
                dome.Draw(device, camera);
                device.SetRenderTarget(nullptr);
                std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
                target.GetData(pixels.data(), static_cast<int>(pixels.size()));
                return pixels;
            }

            static Color At(const std::vector<Color>& pixels, const int x, const int y)
            {
                return pixels[static_cast<std::size_t>(y * Size + x)];
            }

            Xna::GraphicsDevice device;
            Xna::RenderTarget2D target;
            SkyDome dome;
            Camera camera;
        };
    }

    TEST(SkyTest, ThePaletteInterpolatesByElevation)
    {
        const auto palette = DayNightPalette::Default();
        const auto& keys = palette.Keys();
        ExpectNear(palette.At(-90.0f).zenith, keys.front().colors.zenith);
        ExpectNear(palette.At(120.0f).horizon, keys.back().colors.horizon);
        ExpectNear(palette.At(30.0f).sunLight, keys[4].colors.sunLight);
        const SkyColors halfway = palette.At(5.0f);
        ExpectNear(halfway.horizon, (keys[2].colors.horizon + keys[3].colors.horizon) * 0.5f);
        EXPECT_FLOAT_EQ(palette.At(-30.0f).stars, 1.0f);
        EXPECT_FLOAT_EQ(palette.At(40.0f).stars, 0.0f);

        const SkyColors clear = palette.At(40.0f);
        const SkyColors grey = palette.At(40.0f, 1.0f);
        EXPECT_LT(grey.sunLight.X, clear.sunLight.X * 0.5f);
        EXPECT_FLOAT_EQ(grey.sunDisc.X, 0.0f);
        EXPECT_NEAR(grey.horizon.X, grey.horizon.Z, 1e-5f); // grey

        EXPECT_THROW(DayNightPalette({}), std::invalid_argument);
        EXPECT_THROW(DayNightPalette({{10.0f, {}}, {5.0f, {}}}), std::invalid_argument);
    }

    TEST(SkyTest, TheClockPutsTheSunWhereSunPositionDoes)
    {
        TimeOfDay time;
        time.hours = 12.0;
        time.dayOfYear = 172.0;
        time.latitudeDegrees = 50.0;
        EXPECT_NEAR(time.Sun().elevationDegrees, 90.0 - 50.0 + 23.44, 0.6);
        const Vector3 sun = time.DirectionToSun();
        EXPECT_GT(sun.Y, 0.85f);
        EXPECT_GT(sun.Z, 0.0f); // north is -Z: at 50 degrees north the noon sun stands south
        time.hours = 0.0;
        EXPECT_LT(time.Sun().elevationDegrees, 0.0);
        time.hours = 23.5;
        time.Advance(3600.0);
        EXPECT_NEAR(time.hours, 0.5, 1e-9);
        time.Advance(60.0, 60.0); // a minute at sixty times: an hour
        EXPECT_NEAR(time.hours, 1.5, 1e-9);
    }

    TEST(SkyTest, TheLightFollowsTheSunAndTheMoon)
    {
        ForwardLighting lighting;
        const auto day = DayNightPalette::Default().At(40.0f);
        const Vector3 sun = Vector3(0.0f, 0.8f, -0.6f);
        ApplySkyLighting(lighting, day, sun, 20.0f, 80.0f);
        EXPECT_FALSE(lighting.defaultLighting);
        ExpectNear(lighting.lightDirection, -sun);
        ExpectNear(lighting.lightColor, day.sunLight);
        ASSERT_EQ(lighting.fillLights.size(), 1u);
        EXPECT_TRUE(lighting.fogEnabled);
        ExpectNear(lighting.fogColor, day.horizon);
        EXPECT_FLOAT_EQ(lighting.fogEnd, 80.0f);

        const auto night = DayNightPalette::Default().At(-30.0f);
        ApplySkyLighting(lighting, night, -sun);
        ExpectNear(lighting.lightDirection, -sun); // the moon, opposite the sun below
        EXPECT_FALSE(lighting.fogEnabled);
    }

    TEST_F(SkyDomeTest, TheDomeShadesFromTheGroundThroughTheHorizonToTheZenith)
    {
        dome.SetColors(Plain(), Vector3(0.0f, -1.0f, 0.0f));
        camera.SetYawPitchRoll(0.3f, 0.0f); // level: the horizon across the middle
        auto pixels = Draw();
        const Color middle = At(pixels, Size / 2, Size / 2);
        EXPECT_GT(middle.getRProperty(), 200);
        EXPECT_LT(middle.getBProperty(), 60);
        const Color low = At(pixels, Size / 2, Size - 2);
        EXPECT_GT(low.getGProperty(), low.getRProperty());

        camera.SetYawPitchRoll(0.3f, 1.5f); // nearly straight up
        pixels = Draw();
        const Color top = At(pixels, Size / 2, Size / 2);
        EXPECT_GT(top.getBProperty(), 200);
        EXPECT_LT(top.getRProperty(), 60);
    }

    TEST_F(SkyDomeTest, TheSunDiscAndTheStarsShow)
    {
        SkyColors colors = Plain();
        colors.sunDisc = Vector3(0.0f, 1.0f, 0.0f);
        const Vector3 ahead = Vector3(0.0f, 0.2f, -1.0f);
        dome.SetColors(colors, ahead);
        camera.LookAt(camera.Position() + ahead);
        auto pixels = Draw();
        const Color centre = At(pixels, Size / 2, Size / 2);
        EXPECT_GT(centre.getGProperty(), 200);
        EXPECT_LT(centre.getRProperty(), 80);

        auto bright = [&](const std::vector<Color>& image) {
            int count = 0;
            for (const Color& c : image) {
                count += c.getRProperty() > 120 && c.getGProperty() > 120 && c.getBProperty() > 120 ? 1 : 0;
            }
            return count;
        };
        colors = Plain();
        colors.zenith = colors.horizon = Vector3(0.0f, 0.0f, 0.05f);
        colors.stars = 1.0f;
        dome.SetColors(colors, Vector3(0.0f, -1.0f, 0.0f));
        camera.SetYawPitchRoll(0.0f, 1.2f);
        EXPECT_GT(bright(Draw()), 0);
        colors.stars = 0.0f;
        dome.SetColors(colors, Vector3(0.0f, -1.0f, 0.0f));
        EXPECT_EQ(bright(Draw()), 0);
    }
}
