// SPDX-License-Identifier: MIT
//
// The engine's shadow map received by CNA's effects in an EngineGame: a box
// above a floor lit from the side casts its shadow on the side away from the
// light -- not mirrored, not missing -- and nothing darkens without shadows.
#include "CnaEngine/Enhanced/ShadowReception.hpp"

#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace CnaEngine::Enhanced
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        constexpr int Size = 128;

        App::GameOptions Small()
        {
            App::GameOptions options;
            options.width = Size;
            options.height = Size;
            options.assetDirectory.clear();
            options.clearColor = Color(0, 0, 0, 255);
            return options;
        }

        // A floor and a box 1.5 m up at (2, 1.5, 1); light travelling to -X and down.
        class ShadowGame final : public App::EngineGame
        {
        public:
            ShadowGame(const bool shadows, const bool pbr) : EngineGame(Small()), shadows_(shadows), pbr_(pbr) {}

            Graphics::CaptureResult shot;
            bool enabled = false;

        protected:
            void LoadGame() override
            {
                auto& device = getGraphicsDeviceProperty();
                const auto& layout = Graphics::VertexLayout::PositionNormalTexture();
                Graphics::MeshBuilder builder;
                builder.AddBox(Vector3(-6.0f, -0.5f, -6.0f), Vector3(6.0f, 0.0f, 6.0f));
                floor_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
                builder.AddBox(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f));
                box_ = Graphics::GpuMesh::Upload(device, builder.Take(), layout);
                if (pbr_) {
                    material_.shading = Graphics::ShadingModel::PhysicallyBased;
                    Renderer().SetMaterialBinder(std::make_shared<PbrMaterialBinder>(device, Renderer().StockBinder()));
                }
                auto& lighting = Renderer().Lighting();
                lighting.defaultLighting = false;
                lighting.lightDirection = Vector3(-0.6f, -0.8f, 0.0f);
                lighting.lightColor = Vector3(0.6f, 0.6f, 0.6f);
                lighting.ambientColor = Vector3(0.25f, 0.25f, 0.25f);
                MainCamera().SetLookAt(Vector3(0.0f, 12.0f, 0.01f), Vector3::Zero);
                if (shadows_) {
                    // The camera looks straight down from 12 m: the shadowed area's
                    // centre, 0.75 radii ahead of it, is on the floor.
                    enabled = EnableShadows(*this, {.size = 512, .radius = 16.0f, .ahead = 0.75f}, {.filterRadius = 0});
                }
            }
            void Submit(App::Frame& frame) override
            {
                frame.Add(floor_, material_, Matrix::getIdentityProperty());
                frame.Add(box_, material_, Matrix::CreateTranslation(2.0f, 1.5f, 1.0f));
            }
            void UpdateFrame(double) override
            {
                if (UpdateCount() == 1) {
                    (void)Screenshots().Request({.path = "", .keepPixels = true, .opaque = true});
                    if (const char* dir = std::getenv("SHADOW_DEBUG_DIR")) {
                        (void)Screenshots().Request({.path = std::string(dir) + (shadows_ ? "/shadowed" : "/lit") + (pbr_ ? "-pbr.png" : ".png"), .keepPixels = false, .opaque = true});
                    }
                }
            }
            void OnScreenshot(const Graphics::CaptureResult& result) override
            {
                if (!result.pixels.empty()) {
                    shot = result;
                }
            }

        private:
            bool shadows_;
            bool pbr_;
            Graphics::GpuMesh floor_;
            Graphics::GpuMesh box_;
            Graphics::Material material_;
        };

        struct Pass
        {
            Graphics::CaptureResult shot;
            Matrix viewProjection;
            bool enabled;
        };

        Pass Draw(const bool shadows, const bool pbr)
        {
            ShadowGame game(shadows, pbr);
            const std::vector<std::string_view> arguments{"--frames", "3", "--lockstep"};
            game.ParseCommandLine(arguments);
            game.Run();
            return {game.shot, game.MainCamera().ViewProjection(), game.enabled};
        }

        // The brightness of the floor at world (x, 0, z) in a shot.
        int Brightness(const Pass& run, const float x, const float z)
        {
            const auto clip = Microsoft::Xna::Framework::Vector4::Transform(Microsoft::Xna::Framework::Vector4(x, 0.0f, z, 1.0f),
                                                                            run.viewProjection);
            const int px = static_cast<int>((clip.X / clip.W * 0.5f + 0.5f) * Size);
            const int py = static_cast<int>((-clip.Y / clip.W * 0.5f + 0.5f) * Size);
            const Color c = run.shot.pixels[static_cast<std::size_t>(py * Size + px)];
            return c.getRProperty() + c.getGProperty() + c.getBProperty();
        }
    }

    // UP-019: the software renderer's receivers read a render target the
    // other way up from EasyGL's; the calibration measures it.
    TEST(ShadowReceptionTest, CalibrationFindsTheSoftwareRenderersOrientation)
    {
        Microsoft::Xna::Framework::Graphics::GraphicsDevice device;
        Graphics::ShadowMap map(device, 32);
        EXPECT_TRUE(CalibrateShadowMap(device, map));
        EXPECT_TRUE(map.DrawnUpsideDown());
    }

    TEST(ShadowReceptionTest, TheBoxShadowsTheFloorAwayFromTheLight)
    {
        for (const bool pbr : {false, true}) {
            const Pass lit = Draw(false, pbr);
            const Pass shadowed = Draw(true, pbr);
            ASSERT_TRUE(shadowed.enabled);
            ASSERT_TRUE(lit.shot.success && shadowed.shot.success);
            // The box's shadow: 1.5 m down along (-0.6, -0.8) from (2, 1.5, 1) is (0.875, 0, 1).
            const int inShadow = Brightness(shadowed, 0.875f, 1.0f);
            const int sameSpotLit = Brightness(lit, 0.875f, 1.0f);
            // The PBR key light weighs less against the ambient than the stock one: a fainter shadow.
            EXPECT_LT(inShadow, sameSpotLit * (pbr ? 9 : 3) / (pbr ? 10 : 4)) << (pbr ? "PBR" : "stock");
            // Mirrored places stay lit: across the box, and across z.
            EXPECT_NEAR(Brightness(shadowed, 3.2f, 1.0f), Brightness(lit, 3.2f, 1.0f), 6) << (pbr ? "PBR" : "stock");
            EXPECT_NEAR(Brightness(shadowed, 0.875f, -1.0f), Brightness(lit, 0.875f, -1.0f), 6) << (pbr ? "PBR" : "stock");
            EXPECT_NEAR(Brightness(shadowed, -3.0f, -3.0f), Brightness(lit, -3.0f, -3.0f), 6) << (pbr ? "PBR" : "stock");
        }
    }
}
