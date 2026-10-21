// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/ScreenCapture.hpp"

#include "Microsoft/Xna/Framework/Graphics/PresentationParameters.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <utility>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
    using Microsoft::Xna::Framework::Graphics::Texture2D;

    namespace
    {
        CaptureResult Failure(std::string error)
        {
            CaptureResult result;
            result.error = std::move(error);
            return result;
        }

        // A Color-format surface's texels as R, G, B, A bytes. CNA's Color is a
        // polymorphic type four times the size of its four bytes, so captures
        // are read and written as bytes and become Colors only when kept.
        using Rgba = std::vector<std::uint8_t>;

        // Fills a texture through `upload` and saves it as a PNG file.
        template <typename Upload>
        CaptureResult WriteTexturePng(GraphicsDevice& device, const int width, const int height,
                                      const std::string& path, Upload&& upload)
        {
            if (path.empty()) {
                return Failure("WritePng: empty path");
            }
            try {
                const std::filesystem::path file(path);
                if (file.has_parent_path()) {
                    std::filesystem::create_directories(file.parent_path());
                }
                Texture2D texture(device, width, height);
                upload(texture);
                System::IO::FileStream stream(path, System::IO::FileMode::Create, System::IO::FileAccess::Write);
                texture.SaveAsPng(&stream, width, height);
            } catch (const std::exception& e) {
                return Failure("WritePng: " + path + ": " + e.what());
            } catch (...) {
                return Failure("WritePng: " + path + ": unknown error");
            }
            CaptureResult result;
            result.success = true;
            result.width = width;
            result.height = height;
            result.path = path;
            return result;
        }

        // Writes the file (when asked) and trims the pixel copy (when not kept).
        CaptureResult Finish(GraphicsDevice& device, std::vector<Color> pixels, const int width, const int height,
                             const CaptureOptions& options)
        {
            if (options.opaque) {
                for (Color& pixel : pixels) {
                    pixel.setAProperty(255);
                }
            }
            CaptureResult result;
            if (!options.path.empty()) {
                result = WritePng(device, pixels, width, height, options.path);
                if (!result.success) {
                    return result;
                }
            }
            result.success = true;
            result.width = width;
            result.height = height;
            if (options.keepPixels) {
                result.pixels = std::move(pixels);
            }
            return result;
        }

        CaptureResult Finish(GraphicsDevice& device, Rgba rgba, const int width, const int height,
                             const CaptureOptions& options)
        {
            if (options.opaque) {
                for (std::size_t alpha = 3; alpha < rgba.size(); alpha += 4) {
                    rgba[alpha] = 255;
                }
            }
            CaptureResult result;
            if (!options.path.empty()) {
                result = WriteTexturePng(device, width, height, options.path, [&](Texture2D& texture) {
                    texture.SetData(rgba.data(), static_cast<int>(rgba.size()));
                });
                if (!result.success) {
                    return result;
                }
            }
            result.success = true;
            result.width = width;
            result.height = height;
            if (options.keepPixels) {
                result.pixels.reserve(rgba.size() / 4);
                for (std::size_t texel = 0; texel < rgba.size(); texel += 4) {
                    result.pixels.emplace_back(rgba[texel], rgba[texel + 1], rgba[texel + 2], rgba[texel + 3]);
                }
            }
            return result;
        }
    }

    CaptureResult WritePng(GraphicsDevice& device, const std::vector<Color>& pixels, const int width, const int height,
                           const std::string& path)
    {
        if (width <= 0 || height <= 0 || pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
            return Failure("WritePng: pixel count does not match " + std::to_string(width) + "x" + std::to_string(height));
        }
        return WriteTexturePng(device, width, height, path, [&](Texture2D& texture) {
            texture.SetData(pixels.data(), static_cast<int>(pixels.size()));
        });
    }

    CaptureResult CaptureBackBuffer(GraphicsDevice& device, const CaptureOptions& options)
    {
        try {
            const auto& parameters = device.getPresentationParametersProperty();
            const int width = parameters.getBackBufferWidthProperty();
            const int height = parameters.getBackBufferHeightProperty();
            if (width <= 0 || height <= 0) {
                return Failure("CaptureBackBuffer: back buffer has no size");
            }
            const std::size_t texels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            if (parameters.getBackBufferFormatProperty() == SurfaceFormat::Color) {
                Rgba rgba(texels * 4);
                device.GetBackBufferData(rgba.data(), static_cast<int>(rgba.size()));
                return Finish(device, std::move(rgba), width, height, options);
            }
            std::vector<Color> pixels(texels);
            device.GetBackBufferData(pixels.data(), static_cast<int>(pixels.size()));
            return Finish(device, std::move(pixels), width, height, options);
        } catch (const std::exception& e) {
            return Failure(std::string("CaptureBackBuffer: ") + e.what());
        } catch (...) {
            return Failure("CaptureBackBuffer: unknown error");
        }
    }

    CaptureResult CaptureRenderTarget(GraphicsDevice& device, const RenderTarget2D& target,
                                      const CaptureOptions& options)
    {
        try {
            if (target.getIsDisposedProperty()) {
                return Failure("CaptureRenderTarget: render target is disposed");
            }
            const int width = target.getWidthProperty();
            const int height = target.getHeightProperty();
            const std::size_t texels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            if (target.getFormatProperty() == SurfaceFormat::Color) {
                Rgba rgba(texels * 4);
                target.GetData(rgba.data(), static_cast<int>(rgba.size()));
                return Finish(device, std::move(rgba), width, height, options);
            }
            std::vector<Color> pixels(texels);
            target.GetData(pixels.data(), static_cast<int>(pixels.size()));
            return Finish(device, std::move(pixels), width, height, options);
        } catch (const std::exception& e) {
            return Failure(std::string("CaptureRenderTarget: ") + e.what());
        } catch (...) {
            return Failure("CaptureRenderTarget: unknown error");
        }
    }

    ScreenshotQueue::ScreenshotQueue(std::string directory, std::string prefix)
        : directory_(std::move(directory)), prefix_(std::move(prefix))
    {
    }

    std::uint64_t ScreenshotQueue::Request(CaptureOptions options)
    {
        const std::uint64_t id = nextId_++;
        pending_.push_back(Pending{id, std::move(options)});
        return id;
    }

    std::string ScreenshotQueue::NextPath() const
    {
        char number[32];
        std::snprintf(number, sizeof(number), "%06llu", static_cast<unsigned long long>(sequence_));
        return (std::filesystem::path(directory_) / (prefix_ + "-" + number + ".png")).string();
    }

    std::uint64_t ScreenshotQueue::RequestNext(const bool keepPixels)
    {
        CaptureOptions options;
        options.path = NextPath();
        options.keepPixels = keepPixels;
        ++sequence_;
        return Request(std::move(options));
    }

    template <typename Capture>
    std::vector<CaptureResult> ScreenshotQueue::Drain(Capture&& capture)
    {
        std::vector<CaptureResult> results;
        while (!pending_.empty()) {
            Pending request = std::move(pending_.front());
            pending_.pop_front();
            CaptureResult result = capture(request.options);
            result.requestId = request.id;
            results.push_back(std::move(result));
        }
        return results;
    }

    std::vector<CaptureResult> ScreenshotQueue::Process(GraphicsDevice& device)
    {
        return Drain([&](const CaptureOptions& options) { return CaptureBackBuffer(device, options); });
    }

    std::vector<CaptureResult> ScreenshotQueue::Process(GraphicsDevice& device, const RenderTarget2D& source)
    {
        return Drain([&](const CaptureOptions& options) { return CaptureRenderTarget(device, source, options); });
    }
}
