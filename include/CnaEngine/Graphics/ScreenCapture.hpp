// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace CnaEngine::Graphics
{
    /// Outcome of one capture. Failure is always explicit: `success` is false
    /// and `error` says why; nothing is thrown.
    struct CaptureResult
    {
        bool success = false;
        std::uint64_t requestId = 0;
        int width = 0;
        int height = 0;
        /// File written, empty when the capture was kept in memory only.
        std::string path;
        std::string error;
        /// Row-major pixels (width * height) when CaptureOptions::keepPixels.
        std::vector<Microsoft::Xna::Framework::Color> pixels;
    };

    struct CaptureOptions
    {
        /// PNG file to write; empty captures to memory only.
        std::string path;
        bool keepPixels = false;
    };

    /// Reads the back buffer (call after drawing, before Present()). XNA's
    /// Reach profile cannot read the back buffer; there the result reports the
    /// failure and games capture a render target instead.
    [[nodiscard]] CaptureResult CaptureBackBuffer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                                  const CaptureOptions& options);
    /// Reads a render target's contents.
    [[nodiscard]] CaptureResult CaptureRenderTarget(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                                    const Microsoft::Xna::Framework::Graphics::RenderTarget2D& target,
                                                    const CaptureOptions& options);
    /// Writes `pixels` (width * height, row-major) as a PNG file.
    [[nodiscard]] CaptureResult WritePng(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                         const std::vector<Microsoft::Xna::Framework::Color>& pixels, int width,
                                         int height, const std::string& path);

    /// Screenshot requests made during update and fulfilled after the next
    /// frame is drawn, so a key press never captures a half-drawn frame.
    ///
    /// File names are deterministic: `<directory>/<prefix>-<sequence>.png`
    /// with a zero-padded sequence number, never a timestamp, so automated
    /// runs produce the same names every time.
    class ScreenshotQueue
    {
    public:
        explicit ScreenshotQueue(std::string directory = ".", std::string prefix = "screenshot");

        /// Queues a capture to an explicit path (or memory when empty).
        std::uint64_t Request(CaptureOptions options);
        /// Queues a capture to the next sequential file name.
        std::uint64_t RequestNext(bool keepPixels = false);
        [[nodiscard]] bool HasPending() const noexcept { return !pending_.empty(); }
        [[nodiscard]] std::string NextPath() const;

        /// Fulfils every pending request from the back buffer.
        [[nodiscard]] std::vector<CaptureResult> Process(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
        /// Fulfils every pending request from `source`, for games that render
        /// the frame off-screen first.
        [[nodiscard]] std::vector<CaptureResult> Process(
            Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
            const Microsoft::Xna::Framework::Graphics::RenderTarget2D& source);

    private:
        template <typename Capture>
        std::vector<CaptureResult> Drain(Capture&& capture);

        struct Pending
        {
            std::uint64_t id;
            CaptureOptions options;
        };

        std::string directory_;
        std::string prefix_;
        std::uint64_t nextId_ = 1;
        std::uint64_t sequence_ = 1;
        std::deque<Pending> pending_;
    };
}
