// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/AutomatedRun.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;

    TEST(AutomatedRunTests, ParsesTheSharedOptions)
    {
        const std::array<std::string_view, 9> args{"game", "--frames", "3", "--seed", "--screenshot", "shot.png",
                                                   "--lockstep", "--run-seconds", "1.5"};
        AutomatedRunOptions options;
        std::vector<std::string_view> others;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (!ParseAutomatedRunOption(args, i, options)) {
                others.push_back(args[i]);
            }
        }
        EXPECT_EQ(options.frames, 3);
        EXPECT_EQ(options.screenshotPath, "shot.png");
        EXPECT_TRUE(options.lockstep);
        EXPECT_DOUBLE_EQ(options.runSeconds, 1.5);
        EXPECT_TRUE(options.Finite());
        EXPECT_EQ(others, (std::vector<std::string_view>{"--seed"})) << "the game's own options stay its own";

        const auto parse = [](std::initializer_list<std::string_view> list) {
            const std::vector<std::string_view> arguments(list);
            AutomatedRunOptions ignored;
            std::size_t index = 0;
            return ParseAutomatedRunOption(arguments, index, ignored);
        };
        EXPECT_THROW(parse({"--frames"}), std::invalid_argument);
        EXPECT_THROW(parse({"--frames", "-1"}), std::invalid_argument);
        EXPECT_THROW(parse({"--frames", "3x"}), std::invalid_argument);
        EXPECT_THROW(parse({"--run-seconds", "nan"}), std::invalid_argument);
        EXPECT_THROW(parse({"--screenshot", ""}), std::invalid_argument);
        EXPECT_FALSE(AutomatedRunUsage().empty());
        EXPECT_FALSE(AutomatedRunOptions{}.Finite());
    }

    TEST(AutomatedRunTests, LockstepTakesOneFixedStepPerFrame)
    {
        GraphicsDevice device;
        AutomatedRun run({.lockstep = true});
        ASSERT_EQ(run.BeginUpdate(0.5, 1.0 / 60.0), 1.0 / 60.0) << "the fixed step, whatever was measured";
        EXPECT_FALSE(run.BeginUpdate(0.5, 1.0 / 60.0)) << "a catch-up step before the frame is drawn";
        EXPECT_FALSE(run.BeginDraw(device));
        run.EndDraw();
        EXPECT_TRUE(run.BeginUpdate(0.5, 1.0 / 60.0));
        EXPECT_DOUBLE_EQ(run.SimulatedSeconds(), 2.0 / 60.0);

        AutomatedRun measured({});
        EXPECT_EQ(measured.BeginUpdate(0.25, 1.0 / 60.0), 0.25);
        EXPECT_EQ(measured.BeginUpdate(0.25, 1.0 / 60.0), 0.25) << "without lockstep every update runs";
    }

    TEST(AutomatedRunTests, StopsAfterTheFrameLimitOnceReady)
    {
        GraphicsDevice device;
        AutomatedRun run({.frames = 2});
        for (int frame = 0; frame < 2; ++frame) {
            ASSERT_TRUE(run.BeginUpdate(0.01, 0.01));
            ASSERT_FALSE(run.BeginDraw(device));
            run.EndDraw(false);
        }
        EXPECT_TRUE(run.LimitReached());
        EXPECT_FALSE(run.BeginUpdate(0.01, 0.01)) << "the simulation is frozen at the requested frame";
        EXPECT_FALSE(run.BeginDraw(device)) << "content still loading";
        run.EndDraw(true);
        EXPECT_TRUE(run.BeginDraw(device));
        EXPECT_EQ(run.FramesDrawn(), 3);
        EXPECT_FALSE(run.Screenshot()) << "none was asked for";

        AutomatedRun timed({.runSeconds = 0.05});
        int frames = 0;
        while (!timed.BeginDraw(device)) {
            (void)timed.BeginUpdate(0.02, 0.02);
            timed.EndDraw();
            ++frames;
        }
        EXPECT_EQ(frames, 3) << "0.06 simulated seconds reach 0.05";
    }

    TEST(AutomatedRunTests, SavesTheLastFrameAfterItWasPresented)
    {
        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef); // Reach cannot read the back buffer
        const auto path = std::filesystem::current_path() / "cna_engine_automated_run.png";
        std::filesystem::remove(path);
        AutomatedRun run({.frames = 1, .screenshotPath = path.string()});
        ASSERT_FALSE(run.BeginDraw(device));
        device.Clear(Color(30, 60, 90, 255));
        run.EndDraw();
        ASSERT_TRUE(run.BeginDraw(device));
        ASSERT_TRUE(run.Screenshot());
        EXPECT_TRUE(run.Screenshot()->success) << run.Screenshot()->error;
        System::IO::FileStream stream(path.string(), System::IO::FileMode::Open, System::IO::FileAccess::Read);
        auto image = Microsoft::Xna::Framework::Graphics::Texture2D::FromStream(device, stream);
        Color texel;
        const Microsoft::Xna::Framework::Rectangle first(0, 0, 1, 1);
        image.GetData(0, &first, &texel, 0, 1);
        EXPECT_EQ(texel, Color(30, 60, 90, 255));
        EXPECT_TRUE(run.BeginDraw(device)) << "the run stays over; the file is written once";
        std::filesystem::remove(path);
    }
}
