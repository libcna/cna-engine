// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/ScreenCapture.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/PresentationParameters.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::GraphicsProfile;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    namespace
    {
        // Output stays inside the test's working directory (the build tree).
        class ScratchDirectory
        {
        public:
            explicit ScratchDirectory(const std::string& name) : path_(std::filesystem::current_path() / name)
            {
                std::filesystem::remove_all(path_);
            }
            ~ScratchDirectory() { std::filesystem::remove_all(path_); }
            [[nodiscard]] std::string File(const std::string& name) const { return (path_ / name).string(); }
            [[nodiscard]] const std::filesystem::path& Path() const { return path_; }

        private:
            std::filesystem::path path_;
        };

        Texture2D LoadPng(GraphicsDevice& device, const std::string& path)
        {
            System::IO::FileStream stream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
            return Texture2D::FromStream(device, stream);
        }
    }

    TEST(ScreenCaptureTests, RenderTargetCaptureKeepsPixelsAndWritesPng)
    {
        GraphicsDevice device;
        RenderTarget2D target(device, 8, 4);
        device.SetRenderTarget(&target);
        device.Clear(Color(10, 200, 30, 255));
        device.SetRenderTarget(nullptr);

        ScratchDirectory scratch("cna_engine_capture_rt");
        CaptureOptions options;
        options.path = scratch.File("nested/rt.png");
        options.keepPixels = true;
        const CaptureResult result = CaptureRenderTarget(device, target, options);
        ASSERT_TRUE(result.success) << result.error;
        EXPECT_EQ(result.width, 8);
        EXPECT_EQ(result.height, 4);
        ASSERT_EQ(result.pixels.size(), 32u);
        EXPECT_EQ(result.pixels[5], Color(10, 200, 30, 255));
        EXPECT_EQ(result.path, options.path);

        Texture2D decoded = LoadPng(device, result.path);
        EXPECT_EQ(decoded.getWidthProperty(), 8);
        EXPECT_EQ(decoded.getHeightProperty(), 4);
        std::vector<Color> roundTrip(32);
        decoded.GetData(roundTrip.data(), 32);
        EXPECT_EQ(roundTrip, result.pixels) << "PNG output is lossless";
    }

    TEST(ScreenCaptureTests, BackBufferCaptureReportsItsSize)
    {
        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef); // Reach cannot read the back buffer
        device.Clear(Color(1, 2, 3, 255));
        CaptureOptions options;
        options.keepPixels = true;
        const CaptureResult result = CaptureBackBuffer(device, options);
        ASSERT_TRUE(result.success) << result.error;
        const auto& parameters = device.getPresentationParametersProperty();
        EXPECT_EQ(result.width, parameters.getBackBufferWidthProperty());
        EXPECT_EQ(result.height, parameters.getBackBufferHeightProperty());
        ASSERT_FALSE(result.pixels.empty());
        EXPECT_EQ(result.pixels.front(), Color(1, 2, 3, 255));
        EXPECT_TRUE(result.path.empty());
    }

    TEST(ScreenCaptureTests, ReachBackBufferCaptureFailsExplicitly)
    {
        GraphicsDevice device;
        ASSERT_EQ(device.getGraphicsProfileProperty(), GraphicsProfile::Reach);
        const CaptureResult result = CaptureBackBuffer(device, CaptureOptions{});
        EXPECT_FALSE(result.success);
        EXPECT_NE(result.error.find("Reach"), std::string::npos) << result.error;
    }

    TEST(ScreenCaptureTests, FailuresAreExplicit)
    {
        GraphicsDevice device;
        const CaptureResult mismatch = WritePng(device, std::vector<Color>(3), 2, 2, "never.png");
        EXPECT_FALSE(mismatch.success);
        EXPECT_NE(mismatch.error.find("pixel count"), std::string::npos);
        EXPECT_FALSE(std::filesystem::exists("never.png"));

        ScratchDirectory scratch("cna_engine_capture_fail");
        std::filesystem::create_directories(scratch.Path());
        // A directory where the file should go cannot be opened for writing.
        std::filesystem::create_directories(scratch.File("taken.png"));
        const CaptureResult blocked = WritePng(device, std::vector<Color>(4), 2, 2, scratch.File("taken.png"));
        EXPECT_FALSE(blocked.success);
        EXPECT_FALSE(blocked.error.empty());

        RenderTarget2D target(device, 2, 2);
        target.Dispose();
        const CaptureResult disposed = CaptureRenderTarget(device, target, CaptureOptions{});
        EXPECT_FALSE(disposed.success);
        EXPECT_NE(disposed.error.find("disposed"), std::string::npos);
    }

    TEST(ScreenCaptureTests, QueueUsesDeterministicNamesAndFulfilsInOrder)
    {
        GraphicsDevice device;
        device.SetGraphicsProfileEXT(GraphicsProfile::HiDef);
        device.Clear(Color(0, 0, 0, 255));
        ScratchDirectory scratch("cna_engine_capture_queue");
        ScreenshotQueue queue(scratch.Path().string(), "shot");
        EXPECT_EQ(std::filesystem::path(queue.NextPath()).filename(), "shot-000001.png");
        const auto first = queue.RequestNext();
        const auto second = queue.RequestNext();
        const auto memoryOnly = queue.Request(CaptureOptions{});
        EXPECT_TRUE(queue.HasPending());

        const auto results = queue.Process(device);
        EXPECT_FALSE(queue.HasPending());
        ASSERT_EQ(results.size(), 3u);
        EXPECT_EQ(results[0].requestId, first);
        EXPECT_EQ(results[1].requestId, second);
        EXPECT_EQ(results[2].requestId, memoryOnly);
        for (const auto& result : results) {
            EXPECT_TRUE(result.success) << result.error;
        }
        EXPECT_EQ(std::filesystem::path(results[1].path).filename(), "shot-000002.png");
        EXPECT_TRUE(std::filesystem::exists(results[0].path));
        EXPECT_TRUE(results[2].path.empty());
        EXPECT_TRUE(queue.Process(device).empty());
    }
}
