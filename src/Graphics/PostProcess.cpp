// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/PostProcess.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Rectangle;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        Rectangle Screen(Xna::GraphicsDevice& device)
        {
            const auto& viewport = device.getViewportProperty();
            return Rectangle(0, 0, viewport.getWidthProperty(), viewport.getHeightProperty());
        }

        void Begin(Xna::SpriteBatch& batch, const Xna::BlendState* blend)
        {
            batch.Begin(Xna::SpriteSortMode::Immediate, blend, &Xna::SamplerState::LinearClamp, &Xna::DepthStencilState::None,
                        &Xna::RasterizerState::CullNone, nullptr);
        }

        int Byte(const float value)
        {
            return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
        }

        // A 1 x 1 white texture made on first use on the device a pass runs on.
        std::shared_ptr<Xna::Texture2D> White(Xna::GraphicsDevice& device, std::shared_ptr<Xna::Texture2D>& cache)
        {
            if (!cache) {
                cache = std::make_shared<Xna::Texture2D>(device, 1, 1);
                const Color white(255, 255, 255, 255);
                cache->SetData(&white, 1);
            }
            return cache;
        }
    }

    PostProcessChain::PostProcessChain(Xna::GraphicsDevice& device)
        : device_(device), batch_(std::make_unique<Xna::SpriteBatch>(device))
    {
    }

    void PostProcessChain::Add(std::string name, PostPass pass)
    {
        if (!pass) {
            throw std::invalid_argument("PostProcessChain::Add: no pass");
        }
        passes_.push_back({std::move(name), std::move(pass)});
    }

    std::size_t PostProcessChain::Remove(const std::string& name)
    {
        return std::erase_if(passes_, [&name](const Named& named) { return named.name == name; });
    }

    void PostProcessChain::Ensure(const int width, const int height)
    {
        if (scene_ && scene_->getWidthProperty() == width && scene_->getHeightProperty() == height) {
            return;
        }
        scene_ = std::make_unique<Xna::RenderTarget2D>(device_, width, height, false, Xna::SurfaceFormat::Color,
                                                       Xna::DepthFormat::Depth24);
        spare_ = std::make_unique<Xna::RenderTarget2D>(device_, width, height, false, Xna::SurfaceFormat::Color,
                                                       Xna::DepthFormat::None);
    }

    void PostProcessChain::BeginScene()
    {
        const Rectangle screen = Screen(device_);
        Ensure(std::max(1, screen.Width), std::max(1, screen.Height));
        device_.SetRenderTarget(scene_.get());
    }

    void PostProcessChain::EndScene()
    {
        if (!scene_) {
            throw std::logic_error("PostProcessChain::EndScene: BeginScene first");
        }
        Xna::RenderTarget2D* source = scene_.get();
        if (passes_.empty()) {
            device_.SetRenderTarget(nullptr);
            Begin(*batch_, &Xna::BlendState::Opaque);
            batch_->Draw(*source, Screen(device_), Color(255, 255, 255, 255));
            batch_->End();
            return;
        }
        for (std::size_t i = 0; i < passes_.size(); ++i) {
            const bool last = i + 1 == passes_.size();
            Xna::RenderTarget2D* target = last ? nullptr : (source == scene_.get() ? spare_.get() : scene_.get());
            device_.SetRenderTarget(target);
            device_.Clear(Color(0, 0, 0, 255));
            passes_[i].pass(device_, *source, *batch_);
            source = target;
        }
    }

    PostPass ColorGradePass(std::function<Vector3()> tint, std::function<float()> brightness)
    {
        return [tint = std::move(tint), brightness = std::move(brightness)](Xna::GraphicsDevice& device, Xna::Texture2D& source,
                                                                            Xna::SpriteBatch& batch) {
            const Vector3 color = (tint ? tint() : Vector3(1.0f, 1.0f, 1.0f)) * (brightness ? brightness() : 1.0f);
            Begin(batch, &Xna::BlendState::Opaque);
            batch.Draw(source, Screen(device), Color(Byte(color.X), Byte(color.Y), Byte(color.Z), 255));
            batch.End();
        };
    }

    PostPass FadePass(const Color color, std::function<float()> amount)
    {
        auto white = std::make_shared<std::shared_ptr<Xna::Texture2D>>();
        return [color, amount = std::move(amount), white](Xna::GraphicsDevice& device, Xna::Texture2D& source,
                                                          Xna::SpriteBatch& batch) {
            const float a = amount ? std::clamp(amount(), 0.0f, 1.0f) : 0.0f;
            Begin(batch, &Xna::BlendState::Opaque);
            batch.Draw(source, Screen(device), Color(255, 255, 255, 255));
            batch.End();
            if (a > 0.0f) {
                Begin(batch, &Xna::BlendState::NonPremultiplied);
                batch.Draw(*White(device, *white), Screen(device),
                           Color(static_cast<int>(color.getRProperty()), static_cast<int>(color.getGProperty()),
                                 static_cast<int>(color.getBProperty()), Byte(a)));
                batch.End();
            }
        };
    }

    PostPass VignettePass(Xna::GraphicsDevice& device, const float strength)
    {
        // Transparent in the middle, darkening towards the corners.
        constexpr int Size = 128;
        std::vector<Color> pixels(static_cast<std::size_t>(Size * Size));
        for (int y = 0; y < Size; ++y) {
            for (int x = 0; x < Size; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f) / Size * 2.0f - 1.0f;
                const float dy = (static_cast<float>(y) + 0.5f) / Size * 2.0f - 1.0f;
                const float r = std::sqrt(dx * dx + dy * dy) / std::sqrt(2.0f);
                const float t = std::clamp((r - 0.45f) / 0.55f, 0.0f, 1.0f);
                pixels[static_cast<std::size_t>(y * Size + x)] = Color(0, 0, 0, Byte(t * t * std::clamp(strength, 0.0f, 1.0f)));
            }
        }
        auto gradient = std::make_shared<Xna::Texture2D>(device, Size, Size);
        gradient->SetData(pixels.data(), static_cast<int>(pixels.size()));
        return [gradient](Xna::GraphicsDevice& on, Xna::Texture2D& source, Xna::SpriteBatch& batch) {
            Begin(batch, &Xna::BlendState::Opaque);
            batch.Draw(source, Screen(on), Color(255, 255, 255, 255));
            batch.End();
            Begin(batch, &Xna::BlendState::NonPremultiplied);
            batch.Draw(*gradient, Screen(on), Color(255, 255, 255, 255));
            batch.End();
        };
    }
}
