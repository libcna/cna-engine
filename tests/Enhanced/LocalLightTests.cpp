// SPDX-License-Identifier: MIT
//
// A lamp over a dark floor through CNA's punctual light: the floor under it
// lights up through the stock and the PBR binders, the floor far away does
// not, and without lamps nothing changes.
#include "CnaEngine/Enhanced/LocalLightBinder.hpp"

#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace CnaEngine::Enhanced
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        constexpr int Size = 64;

        class LampGame final : public App::EngineGame
        {
        public:
            LampGame(const bool lamp, const bool pbr) : EngineGame(Options()), lamp_(lamp), pbr_(pbr) {}

            Graphics::CaptureResult shot;
            int lastLight = -2;

        protected:
            void LoadGame() override
            {
                auto& device = getGraphicsDeviceProperty();
                Graphics::MeshBuilder builder;
                builder.AddGrid(Vector3::Zero, 20.0f, 20.0f, 20, 20, 1.0f);
                floor_ = Graphics::GpuMesh::Upload(device, builder.Take(), Graphics::VertexLayout::PositionNormalTexture());
                if (pbr_) {
                    material_.shading = Graphics::ShadingModel::PhysicallyBased;
                    Renderer().SetMaterialBinder(std::make_shared<PbrMaterialBinder>(device, Renderer().StockBinder()));
                }
                auto& lighting = Renderer().Lighting();
                lighting.defaultLighting = false;
                lighting.lightColor = Vector3::Zero; // night: no sun
                lighting.ambientColor = Vector3(0.05f, 0.05f, 0.05f);
                lights_ = &InstallLocalLights(Renderer());
                if (lamp_) {
                    Graphics::LocalLight lamp;
                    lamp.position = Vector3(3.0f, 1.0f, 0.0f);
                    lamp.range = 4.0f;
                    lamp.color = Vector3(1.0f, 1.0f, 1.0f);
                    lights_->lights.push_back(lamp);
                }
                MainCamera().SetLookAt(Vector3(0.0f, 14.0f, 0.01f), Vector3::Zero);
            }
            void Submit(App::Frame& frame) override { frame.Add(floor_, material_, Matrix::getIdentityProperty()); }
            void UpdateFrame(double) override
            {
                if (UpdateCount() == 1) {
                    (void)Screenshots().Request({.path = "", .keepPixels = true, .opaque = true});
                }
                lastLight = lights_->LastLight();
            }
            void OnScreenshot(const Graphics::CaptureResult& result) override { shot = result; }

        private:
            static App::GameOptions Options()
            {
                App::GameOptions options;
                options.width = Size;
                options.height = Size;
                options.assetDirectory.clear();
                options.clearColor = Color(0, 0, 0, 255);
                return options;
            }
            bool lamp_;
            bool pbr_;
            LocalLightBinder* lights_ = nullptr;
            Graphics::GpuMesh floor_;
            Graphics::Material material_;
        };

        struct Lit
        {
            Graphics::CaptureResult shot;
            Matrix viewProjection;
            int lastLight;
        };

        Lit Draw(const bool lamp, const bool pbr)
        {
            LampGame game(lamp, pbr);
            const std::vector<std::string_view> arguments{"--frames", "3", "--lockstep"};
            game.ParseCommandLine(arguments);
            game.Run();
            return {game.shot, game.MainCamera().ViewProjection(), game.lastLight};
        }

        int Brightness(const Lit& lit, const float x, const float z)
        {
            const auto clip = Microsoft::Xna::Framework::Vector4::Transform(Microsoft::Xna::Framework::Vector4(x, 0.0f, z, 1.0f),
                                                                            lit.viewProjection);
            const int px = static_cast<int>((clip.X / clip.W * 0.5f + 0.5f) * Size);
            const int py = static_cast<int>((-clip.Y / clip.W * 0.5f + 0.5f) * Size);
            const Color c = lit.shot.pixels[static_cast<std::size_t>(py * Size + px)];
            return c.getRProperty() + c.getGProperty() + c.getBProperty();
        }
    }

    TEST(LocalLightTest, ALampLightsTheFloorUnderIt)
    {
        for (const bool pbr : {false, true}) {
            const Lit dark = Draw(false, pbr);
            const Lit lamp = Draw(true, pbr);
            ASSERT_TRUE(dark.shot.success && lamp.shot.success);
            EXPECT_EQ(lamp.lastLight, 0) << (pbr ? "PBR" : "stock");
            EXPECT_EQ(dark.lastLight, -1) << (pbr ? "PBR" : "stock");
            EXPECT_GT(Brightness(lamp, 3.0f, 0.0f), Brightness(dark, 3.0f, 0.0f) + 60) << (pbr ? "PBR" : "stock");
            EXPECT_NEAR(Brightness(lamp, -4.0f, -4.0f), Brightness(dark, -4.0f, -4.0f), 6) << (pbr ? "PBR" : "stock");
        }
    }
}
