// SPDX-License-Identifier: MIT
#include "CnaEngine/App/EngineGame.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace CnaEngine::App
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        // A white world (only the clear colour) through a post-processing chain.
        class PostGame final : public EngineGame
        {
        public:
            explicit PostGame(std::function<void(Graphics::PostProcessChain&, Microsoft::Xna::Framework::Graphics::GraphicsDevice&)> setup)
                : EngineGame(Options()), setup_(std::move(setup))
            {
            }

            Graphics::CaptureResult shot;

        protected:
            void LoadGame() override { setup_(EnablePostProcessing(), getGraphicsDeviceProperty()); }
            void UpdateFrame(double) override
            {
                if (UpdateCount() == 1) {
                    (void)Screenshots().Request({.path = "", .keepPixels = true, .opaque = true});
                }
            }
            void OnScreenshot(const Graphics::CaptureResult& result) override { shot = result; }

        private:
            static GameOptions Options()
            {
                GameOptions options;
                options.width = 64;
                options.height = 64;
                options.clearColor = Color(255, 255, 255, 255);
                options.assetDirectory.clear();
                return options;
            }
            std::function<void(Graphics::PostProcessChain&, Microsoft::Xna::Framework::Graphics::GraphicsDevice&)> setup_;
        };

        Graphics::CaptureResult Render(std::function<void(Graphics::PostProcessChain&, Microsoft::Xna::Framework::Graphics::GraphicsDevice&)> setup)
        {
            PostGame game(std::move(setup));
            const std::vector<std::string_view> arguments{"--frames", "3", "--lockstep"};
            game.ParseCommandLine(arguments);
            game.Run();
            return game.shot;
        }

        Color At(const Graphics::CaptureResult& shot, const int x, const int y)
        {
            return shot.pixels[static_cast<std::size_t>(y * shot.width + x)];
        }
    }

    TEST(PostProcessTest, WithoutPassesTheSceneIsCopied)
    {
        const auto shot = Render([](Graphics::PostProcessChain&, auto&) {});
        ASSERT_TRUE(shot.success) << shot.error;
        EXPECT_EQ(At(shot, 32, 32), Color(255, 255, 255, 255));
    }

    TEST(PostProcessTest, PassesChainGradeFadeAndVignette)
    {
        // A red grade at half brightness, then a vignette.
        const auto graded = Render([](Graphics::PostProcessChain& chain, auto& device) {
            chain.Add("grade", Graphics::ColorGradePass([] { return Vector3(1.0f, 0.2f, 0.2f); }, [] { return 0.5f; }));
            chain.Add("vignette", Graphics::VignettePass(device, 1.0f));
            EXPECT_EQ(chain.Count(), 2u);
        });
        ASSERT_TRUE(graded.success) << graded.error;
        const Color middle = At(graded, 32, 32);
        EXPECT_NEAR(middle.getRProperty(), 128, 3);
        EXPECT_NEAR(middle.getGProperty(), 26, 3);
        EXPECT_LT(At(graded, 0, 0).getRProperty(), middle.getRProperty() / 2); // the corner is darker

        const auto black = Render([](Graphics::PostProcessChain& chain, auto&) {
            chain.Add("fade", Graphics::FadePass(Color(0, 0, 0, 255), [] { return 1.0f; }));
            chain.Add("removed", Graphics::FadePass(Color(255, 0, 0, 255), [] { return 1.0f; }));
            EXPECT_EQ(chain.Remove("removed"), 1u);
        });
        ASSERT_TRUE(black.success) << black.error;
        EXPECT_EQ(At(black, 32, 32), Color(0, 0, 0, 255));
    }
}
