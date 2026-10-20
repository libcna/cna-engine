// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/DebugDraw.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Graphics::BasicEffect;
    using Microsoft::Xna::Framework::Graphics::BlendState;
    using Microsoft::Xna::Framework::Graphics::DepthStencilState;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::PrimitiveType;
    using Microsoft::Xna::Framework::Graphics::RasterizerState;
    using Microsoft::Xna::Framework::Graphics::VertexPositionColor;

    DebugDraw::~DebugDraw() = default;

    void DebugDraw::Line(const Vector3& from, const Vector3& to, const Color& color)
    {
        vertices_.emplace_back(from, color);
        vertices_.emplace_back(to, color);
    }

    void DebugDraw::Box(const Vector3& center, const Vector3& size, const Color& color)
    {
        const Vector3 half = size * 0.5f;
        Box(BoundingBox(center - half, center + half), color);
    }

    void DebugDraw::Box(const BoundingBox& box, const Color& color)
    {
        const Vector3& a = box.Min;
        const Vector3& b = box.Max;
        const Vector3 corners[8] = {
            {a.X, a.Y, a.Z}, {b.X, a.Y, a.Z}, {b.X, b.Y, a.Z}, {a.X, b.Y, a.Z},
            {a.X, a.Y, b.Z}, {b.X, a.Y, b.Z}, {b.X, b.Y, b.Z}, {a.X, b.Y, b.Z},
        };
        constexpr int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                      {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& edge : edges) {
            Line(corners[edge[0]], corners[edge[1]], color);
        }
    }

    std::size_t DebugDraw::Draw(GraphicsDevice& device, const Camera& camera, const bool depthTest)
    {
        const std::size_t lines = LineCount();
        if (lines == 0) {
            return 0;
        }
        if (!effect_ || effectDevice_ != &device) {
            effect_ = std::make_unique<BasicEffect>(device);
            effect_->setVertexColorEnabledProperty(true);
            effect_->setLightingEnabledProperty(false);
            effect_->setTextureEnabledProperty(false);
            effect_->setFogEnabledProperty(false);
            effectDevice_ = &device;
        }
        effect_->setWorldProperty(Matrix::getIdentityProperty());
        effect_->setViewProperty(camera.View());
        effect_->setProjectionProperty(camera.Projection());

        const BlendState blend = device.getBlendStateProperty();
        const DepthStencilState depth = device.getDepthStencilStateProperty();
        const RasterizerState rasterizer = device.getRasterizerStateProperty();
        device.setBlendStateProperty(BlendState::AlphaBlend);
        device.setDepthStencilStateProperty(depthTest ? DepthStencilState::DepthRead : DepthStencilState::None);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        auto& passes = effect_->getCurrentTechniqueProperty()->getPassesProperty();
        for (int i = 0; i < passes.getCountProperty(); ++i) {
            passes[i]->Apply();
            device.DrawUserPrimitives(PrimitiveType::LineList, vertices_.data(), 0, static_cast<int>(lines),
                                      VertexPositionColor::getVertexDeclarationStatic());
        }
        device.setBlendStateProperty(blend);
        device.setDepthStencilStateProperty(depth);
        device.setRasterizerStateProperty(rasterizer);
        return lines;
    }
}
