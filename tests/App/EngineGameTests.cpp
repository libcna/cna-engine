// SPDX-License-Identifier: MIT
#include "CnaEngine/App/EngineGame.hpp"

#include "CnaEngine/Graphics/MeshBuilder.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"

#include <gtest/gtest.h>

#include <charconv>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace CnaEngine::App
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        GameOptions SmallWindow()
        {
            GameOptions options;
            options.title = "engine game test";
            options.width = 64;
            options.height = 48;
            options.clearColor = Color(10, 20, 30, 255);
            return options;
        }

        // A red box in front of the camera; counts what the engine calls.
        class BoxGame final : public EngineGame
        {
        public:
            BoxGame() : EngineGame(SmallWindow()) {}

            int loads = 0;
            int steps = 0;
            int frames = 0;
            double simulated = 0.0;
            unsigned seed = 0;
            bool pressQuitOnce = false;

        protected:
            void LoadGame() override
            {
                ++loads;
                Graphics::MeshBuilder builder;
                builder.AddBox(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
                box_ = Graphics::GpuMesh::Upload(getGraphicsDeviceProperty(), builder.Take(),
                                                 Graphics::VertexLayout::PositionNormalTexture());
                material_.shading = Graphics::ShadingModel::Unlit;
                material_.diffuseColor = Vector3(1.0f, 0.0f, 0.0f);
                MainCamera().SetPosition(Vector3(0.0f, 0.0f, 5.0f));
            }
            void FixedUpdate(const double step) override
            {
                ++steps;
                simulated += step;
            }
            void UpdateFrame(double) override { ++frames; }
            void Submit(Frame& frame) override { frame.Add(box_, material_, Matrix::getIdentityProperty()); }
            Input::InputSnapshot ReadInput() override
            {
                Input::InputSnapshot snapshot;
                if (pressQuitOnce && frames == 2) {
                    snapshot.keyboard =
                        Microsoft::Xna::Framework::Input::KeyboardState({Microsoft::Xna::Framework::Input::Keys::Escape});
                }
                return snapshot;
            }
            bool ParseArgument(const std::span<const std::string_view> arguments, std::size_t& index) override
            {
                if (arguments[index] != "--seed" || index + 1 >= arguments.size()) {
                    return false;
                }
                const auto text = arguments[++index];
                std::from_chars(text.data(), text.data() + text.size(), seed);
                return true;
            }

        private:
            Graphics::GpuMesh box_;
            Graphics::Material material_;
        };

        void Parse(EngineGame& game, std::vector<std::string_view> arguments)
        {
            game.ParseCommandLine(arguments);
        }
    }

    TEST(EngineGameTest, ReadsTheSharedAndTheGamesOwnOptions)
    {
        BoxGame game;
        Parse(game, {"--frames", "3", "--lockstep", "--width", "320", "--height", "200", "--seed", "77", "--windowed"});
        EXPECT_EQ(game.Automation().Options().frames, 3);
        EXPECT_TRUE(game.Automation().Options().lockstep);
        EXPECT_EQ(game.Options().width, 320);
        EXPECT_EQ(game.Options().height, 200);
        EXPECT_FALSE(game.Options().fullScreen);
        EXPECT_EQ(game.seed, 77u);
    }

    TEST(EngineGameTest, RejectsUnknownArgumentsAndBadValues)
    {
        BoxGame game;
        EXPECT_THROW(Parse(game, {"--no-such-option"}), std::invalid_argument);
        EXPECT_THROW(Parse(game, {"--width", "wide"}), std::invalid_argument);
        EXPECT_THROW(Parse(game, {"--width", "10"}), std::invalid_argument);
        EXPECT_THROW(Parse(game, {"--height"}), std::invalid_argument);
        EXPECT_THROW(Parse(game, {"--frames", "-1"}), std::invalid_argument);
        EXPECT_FALSE(EngineGame::Usage().empty());
    }

    TEST(EngineGameTest, BuiltInActionsAreDeclaredAndBound)
    {
        BoxGame game;
        const auto& actions = game.Actions();
        EXPECT_EQ(actions.ActionName(game.BuiltIns().quit), "Quit");
        EXPECT_EQ(actions.ActionName(game.BuiltIns().screenshot), "Screenshot");
        EXPECT_EQ(actions.ActionName(game.BuiltIns().statistics), "Statistics");
        EXPECT_EQ(actions.ActionName(game.BuiltIns().fullScreen), "FullScreen");
        EXPECT_FALSE(actions.Bindings(game.BuiltIns().quit).empty());
        EXPECT_THROW((void)game.Renderer(), std::logic_error);
    }

    // A lockstep run: one fixed step per drawn frame, then the run stops by
    // itself and the screenshot shows the box in the middle and the clear
    // colour in the corner.
    TEST(EngineGameTest, AnAutomatedLockstepRunDrawsStepsAndPhotographs)
    {
        const auto shot = std::filesystem::current_path() / "engine_game_test.png";
        std::filesystem::remove(shot);
        BoxGame game;
        Parse(game, {"--frames", "4", "--lockstep", "--screenshot", shot.string()});
        game.Run();

        EXPECT_EQ(game.loads, 1);
        EXPECT_EQ(game.steps, 4);
        EXPECT_EQ(game.frames, 4);
        EXPECT_DOUBLE_EQ(game.simulated, 4.0 / 60.0);
        EXPECT_EQ(game.RenderedLastFrame().drawCalls, 1u);
        EXPECT_EQ(game.CulledLastFrame().visible, 1u);

        const auto& result = game.Automation().Screenshot();
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(result->success) << result->error;
        EXPECT_TRUE(std::filesystem::exists(shot));
        EXPECT_EQ(result->width, 64);
        EXPECT_EQ(result->height, 48);
        EXPECT_EQ(Detail::ExitCodeAfterRun(game), 0);
        std::filesystem::remove(shot);
    }

    TEST(EngineGameTest, TheCameraFollowsTheWindowsAspect)
    {
        BoxGame game;
        Parse(game, {"--frames", "1", "--lockstep"});
        game.Run();
        EXPECT_FLOAT_EQ(game.MainCamera().AspectRatio(), 64.0f / 48.0f);
    }

    TEST(EngineGameTest, QuitEndsTheRun)
    {
        BoxGame game;
        game.pressQuitOnce = true;
        // A long run that only the quit action can end early.
        Parse(game, {"--frames", "1000", "--lockstep"});
        game.Run();
        EXPECT_LT(game.frames, 10);
    }
}
