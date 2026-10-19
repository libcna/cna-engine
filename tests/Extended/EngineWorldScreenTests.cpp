// SPDX-License-Identifier: MIT
#include "CnaEngine/Extended/EngineWorldScreen.hpp"

#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "System/TimeSpan.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace CnaEngine::Extended
{
    using Microsoft::Xna::Framework::GameTime;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;
    using System::TimeSpan;

    namespace
    {
        // Replays scripted snapshots and records the order of the hooks.
        class ScriptedScreen final : public EngineWorldScreen
        {
        public:
            std::vector<Input::InputSnapshot> script;
            std::size_t next = 0;
            int jumps = 0;
            Input::ActionId jump = Actions().AddAction("Jump");

        protected:
            Input::InputSnapshot CaptureInput() override
            {
                return next < script.size() ? script[next++] : Input::InputSnapshot{};
            }
            void UpdateScreen(GameTime&) override
            {
                if (Actions().Pressed(jump)) {
                    ++jumps;
                }
            }
        };

        Input::InputSnapshot Keys_(std::initializer_list<Keys> keys)
        {
            Input::InputSnapshot snapshot;
            snapshot.keyboard = KeyboardState(keys);
            return snapshot;
        }
    }

    TEST(EngineWorldScreenTests, InputDrivesActionsControllerAndWorldCamera)
    {
        ScriptedScreen screen;
        screen.Initialize();
        screen.Actions().Bind(screen.jump, Input::Binding::ForKey(Keys::Space));
        Graphics::CameraInputAxes axes;
        axes.moveForward = screen.Actions().AddAxis("MoveForward");
        axes.moveRight = screen.Actions().AddAxis("MoveRight");
        axes.lookYaw = screen.Actions().AddAxis("LookYaw");
        axes.lookPitch = screen.Actions().AddAxis("LookPitch");
        screen.Actions().BindAxis(axes.moveForward, Input::AxisBinding::ForKeys(Keys::S, Keys::W));
        screen.SetCameraController(std::make_unique<Graphics::FlyCameraController>(Graphics::FirstPersonSettings{2.0f, 1.0f, 1.5f}),
                                   axes);

        screen.script = {Keys_({Keys::W, Keys::Space}), Keys_({Keys::W, Keys::Space}), Keys_({})};
        GameTime time(TimeSpan::FromSeconds(0.0), TimeSpan::FromSeconds(0.5));
        for (int frame = 0; frame < 3; ++frame) {
            screen.Update(time);
            screen.Draw(time);
        }

        EXPECT_EQ(screen.jumps, 1) << "held Space presses once";
        EXPECT_NEAR(screen.Camera().Position().Z, -2.0f, 1e-5f) << "two updates at 2 units/s for 0.5 s";
        EXPECT_NEAR(screen.GetCamera3DEXT().getPositionProperty().Z, -2.0f, 1e-5f) << "world camera follows";
        EXPECT_EQ(screen.FrameStats().FrameCount(), 3u);
        EXPECT_DOUBLE_EQ(screen.FrameStats().AverageSeconds(), 0.5);
    }

    TEST(EngineWorldScreenTests, WithoutControllerTheCameraIsGameDriven)
    {
        ScriptedScreen screen;
        screen.Initialize();
        screen.Camera().SetPosition(Vector3(1.0f, 2.0f, 3.0f));
        GameTime time(TimeSpan::FromSeconds(0.0), TimeSpan::FromSeconds(0.016));
        screen.Update(time);
        EXPECT_EQ(screen.GetCamera3DEXT().getPositionProperty(), Vector3(1.0f, 2.0f, 3.0f));
    }
}
