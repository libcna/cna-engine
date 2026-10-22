// SPDX-License-Identifier: MIT
#include "CnaEngine/UI/Painter.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CnaEngine::UI
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector2;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        Color Faded(const Color& color, const float alpha)
        {
            return Color(static_cast<int>(color.getRProperty()), static_cast<int>(color.getGProperty()),
                         static_cast<int>(color.getBProperty()),
                         static_cast<int>(std::lround(static_cast<float>(color.getAProperty()) * std::clamp(alpha, 0.0f, 1.0f))));
        }
    }

    Painter::Painter(Xna::GraphicsDevice& device, const float virtualHeight)
        : device_(device),
          batch_(std::make_unique<Xna::SpriteBatch>(device)),
          white_(std::make_unique<Xna::Texture2D>(device, 1, 1)),
          virtualHeight_(virtualHeight)
    {
        if (!(virtualHeight > 0.0f)) {
            throw std::invalid_argument("Painter: the virtual height must be positive");
        }
        const Color white(255, 255, 255, 255);
        white_->SetData(&white, 1);
    }

    void Painter::Begin()
    {
        const auto& viewport = device_.getViewportProperty();
        const float height = static_cast<float>(std::max(1, viewport.getHeightProperty()));
        const float width = static_cast<float>(std::max(1, viewport.getWidthProperty()));
        scale_ = height / virtualHeight_;
        size_ = Vector2(width / scale_, virtualHeight_);
        batch_->Begin(Xna::SpriteSortMode::Deferred, &Xna::BlendState::NonPremultiplied, &Xna::SamplerState::PointClamp,
                      &Xna::DepthStencilState::None, &Xna::RasterizerState::CullNone, nullptr,
                      Microsoft::Xna::Framework::Matrix::CreateScale(scale_));
    }

    void Painter::End()
    {
        batch_->End();
    }

    Vector2 Painter::ToVirtual(const Vector2& pixels) const noexcept
    {
        return Vector2(pixels.X / scale_, pixels.Y / scale_);
    }

    Box Painter::Place(const Anchor anchor, const Vector2& size, const Vector2& margin) const noexcept
    {
        const auto column = static_cast<int>(anchor) % 3; // left, middle, right
        const auto row = static_cast<int>(anchor) / 3;    // top, middle, bottom
        Box box{0.0f, 0.0f, size.X, size.Y};
        box.x = column == 0 ? margin.X : column == 1 ? (size_.X - size.X) * 0.5f : size_.X - size.X - margin.X;
        box.y = row == 0 ? margin.Y : row == 1 ? (size_.Y - size.Y) * 0.5f : size_.Y - size.Y - margin.Y;
        return box;
    }

    void Painter::Fill(const Box& box, const Color color)
    {
        batch_->Draw(*white_,
                     Microsoft::Xna::Framework::Rectangle(static_cast<int>(std::lround(box.x)), static_cast<int>(std::lround(box.y)),
                                                          static_cast<int>(std::lround(box.width)),
                                                          static_cast<int>(std::lround(box.height))),
                     color);
    }

    void Painter::Text(const IFont& font, const std::string_view text, const Vector2& position, const TextStyle& style)
    {
        const Vector2 at(std::round(position.X), std::round(position.Y));
        if (style.shadow) {
            font.Draw(*batch_, text, at + Vector2(1.0f, 1.0f), style.shadowColor, style.scale);
        }
        font.Draw(*batch_, text, at, style.color, style.scale);
    }

    Box Painter::Text(const IFont& font, const std::string_view text, const Anchor anchor, const Vector2& margin,
                      const TextStyle& style)
    {
        const Box box = Place(anchor, Measure(font, text, style), margin);
        Text(font, text, Vector2(box.x, box.y), style);
        return box;
    }

    Vector2 Painter::Measure(const IFont& font, const std::string_view text, const TextStyle& style)
    {
        return font.Measure(text) * style.scale;
    }

    void Toasts::Show(std::string text, const float seconds)
    {
        toasts_.push_front({std::move(text), seconds});
    }

    void Toasts::Update(const float seconds)
    {
        for (auto& toast : toasts_) {
            toast.left -= seconds;
        }
        std::erase_if(toasts_, [](const Toast& toast) { return toast.left <= 0.0f; });
    }

    void Toasts::Draw(Painter& painter, const IFont& font, const Anchor anchor, const TextStyle& style)
    {
        // Place measures the margin from the edge an anchor touches, so the
        // stack grows away from it at the top and at the bottom alike.
        Vector2 margin(16.0f, 16.0f);
        for (const auto& toast : toasts_) {
            TextStyle faded = style;
            const float alpha = std::min(1.0f, toast.left * 2.0f);
            faded.color = Faded(style.color, alpha);
            faded.shadowColor = Faded(style.shadowColor, alpha);
            const Box box = painter.Text(font, toast.text, anchor, margin, faded);
            margin.Y += box.height + 6.0f;
        }
    }
}
