// SPDX-License-Identifier: MIT
//
// A skinned, animated glTF (tests/fixtures/models/skinned_strip.gltf, made by
// make_skinned_strip.py) loaded by CNA at run time, shown through ModelView
// and animated by the engine's Animator: the palette matches CNA's own
// AnimationPlayer, and the skinned part is drawn by the PBR binder.
#include "CnaEngine/Animation/Animator.hpp"
#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Enhanced/ModelImport.hpp"
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/AnimationPlayer.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

namespace CnaEngine::Enhanced
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        App::GameOptions Small()
        {
            App::GameOptions options;
            options.width = 96;
            options.height = 96;
            options.clearColor = Color(0, 0, 0, 255);
            options.assetDirectory.clear();
            return options;
        }

        // Shows the strip, bent by `bendAt` seconds of the "bend" clip.
        class StripGame final : public App::EngineGame
        {
        public:
            explicit StripGame(const float bendAt) : EngineGame(Small()), bendAt_(bendAt) {}

            std::unique_ptr<Xna::Model> model;
            std::unique_ptr<Graphics::ModelView> view;
            const Xna::SkinningData* skin = nullptr;
            Animation::Skeleton skeleton;
            std::vector<Animation::AnimationClip> clips;
            std::unique_ptr<Animation::Animator> animator;

        protected:
            void LoadGame() override
            {
                getContentProperty().setRootDirectoryProperty(CNA_ENGINE_TEST_MODELS);
                model = std::make_unique<Xna::Model>(getContentProperty().Load<Xna::Model>("skinned_strip"));
                view = std::make_unique<Graphics::ModelView>(*model, PbrMaterialFromEffect);
                skin = FindSkinningData(*model);
                ASSERT_NE(skin, nullptr);
                skeleton = SkeletonFromSkinningData(*skin);
                clips = ClipsFromSkinningData(*skin);
                animator = std::make_unique<Animation::Animator>(skeleton);
                animator->Play(clips.front(), {.fadeSeconds = 0.0f});
                animator->SetTime(bendAt_);
                Renderer().SetMaterialBinder(
                    std::make_shared<PbrMaterialBinder>(getGraphicsDeviceProperty(), Renderer().StockBinder()));
                MainCamera().SetLookAt(Vector3(0.0f, 1.2f, 4.0f), Vector3(0.0f, 1.2f, 0.0f));
            }
            void Submit(App::Frame& frame) override { frame.Add(*view, Matrix::getIdentityProperty(), &animator->SkinPalette()); }
            void UpdateFrame(double) override
            {
                if (UpdateCount() == 1) {
                    (void)Screenshots().Request({.path = "", .keepPixels = true, .opaque = true});
                }
            }
            void OnScreenshot(const Graphics::CaptureResult& result) override { shot = result; }

        public:
            Graphics::CaptureResult shot;

        private:
            float bendAt_;
        };

        // Orange pixels in the left and right halves of a screenshot.
        std::pair<int, int> Orange(const Graphics::CaptureResult& shot)
        {
            int left = 0;
            int right = 0;
            for (int y = 0; y < shot.height; ++y) {
                for (int x = 0; x < shot.width; ++x) {
                    const Color c = shot.pixels[static_cast<std::size_t>(y * shot.width + x)];
                    if (c.getRProperty() > 60 && c.getRProperty() > c.getBProperty() + 30) {
                        (x < shot.width / 2 ? left : right) += 1;
                    }
                }
            }
            return {left, right};
        }

        Graphics::CaptureResult Photograph(const float bendAt)
        {
            StripGame game(bendAt);
            const std::vector<std::string_view> arguments{"--frames", "3", "--lockstep"};
            game.ParseCommandLine(arguments);
            game.Run();
            return game.shot;
        }
    }

    TEST(ModelImportTest, AnImportedSkinnedModelBecomesPartsSkeletonAndClips)
    {
        StripGame game(0.0f);
        const std::vector<std::string_view> arguments{"--frames", "1", "--lockstep"};
        game.ParseCommandLine(arguments);
        game.Run();
        ASSERT_NE(game.view, nullptr);
        ASSERT_EQ(game.view->Parts().size(), 1u);
        const auto& part = game.view->Parts()[0];
        EXPECT_TRUE(part.material.skinned);
        EXPECT_EQ(part.material.material.shading, Graphics::ShadingModel::PhysicallyBased);
        EXPECT_NEAR(part.material.material.diffuseColor.Y, 0.5f, 1e-3f);
        EXPECT_NEAR(part.material.material.roughness, 0.8f, 1e-3f);
        EXPECT_TRUE(part.material.material.doubleSided);
        EXPECT_TRUE(game.view->Fallbacks().empty());
        EXPECT_EQ(game.skeleton.Count(), 2u);
        ASSERT_EQ(game.clips.size(), 2u);
        EXPECT_EQ(game.clips[0].name, "bend");
        EXPECT_EQ(game.clips[1].name, "sway");
        EXPECT_NEAR(game.clips[0].duration, 2.0f, 1e-6f);
        EXPECT_GE(game.RenderedLastFrame().drawCalls, 1u);
    }

    // The engine's Animator against CNA's AnimationPlayer on the same data.
    TEST(ModelImportTest, ThePaletteMatchesCnasAnimationPlayer)
    {
        StripGame game(0.0f);
        const std::vector<std::string_view> arguments{"--frames", "1", "--lockstep"};
        game.ParseCommandLine(arguments);
        game.Run();
        ASSERT_NE(game.skin, nullptr);
        for (const auto& clip : game.clips) {
            Xna::AnimationPlayer player(*game.skin);
            player.StartClip(game.skin->AnimationClips.at(clip.name));
            Animation::Animator animator(game.skeleton);
            animator.Play(clip, {.fadeSeconds = 0.0f});
            for (const float t : {0.0f, 0.25f, 0.5f, 1.0f, 1.3f, 1.9f}) {
                player.Update(System::TimeSpan::FromSeconds(t), false);
                animator.SetTime(t);
                const auto& expected = player.GetSkinTransforms();
                const auto& actual = animator.SkinPalette();
                ASSERT_EQ(expected.size(), actual.size());
                for (std::size_t b = 0; b < expected.size(); ++b) {
                    const float* e = &expected[b].M11;
                    const float* a = &actual[b].M11;
                    for (int k = 0; k < 16; ++k) {
                        EXPECT_NEAR(a[k], e[k], 2e-4f) << clip.name << " t=" << t << " bone " << b << " element " << k;
                    }
                }
            }
        }
    }

    TEST(ModelImportTest, TheSkinnedPartIsDrawnAndFollowsThePose)
    {
        const auto straight = Photograph(0.0f);
        const auto bent = Photograph(1.0f);
        ASSERT_TRUE(straight.success) << straight.error;
        ASSERT_TRUE(bent.success) << bent.error;
        const auto [straightLeft, straightRight] = Orange(straight);
        const auto [bentLeft, bentRight] = Orange(bent);
        // Drawn at all (street's imported figure drew nothing on its skinned path).
        EXPECT_GT(straightLeft + straightRight, 100);
        // Straight it is symmetric; bent 60 degrees about Z its tip leans to -X (left).
        EXPECT_NEAR(straightLeft, straightRight, (straightLeft + straightRight) / 5);
        EXPECT_GT(bentLeft, bentRight + bentRight / 4);
    }
}
