// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/ShadowMap.hpp"

#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Instancing.hpp"
#include "CnaEngine/Graphics/Material.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DirectionalLight.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        constexpr float Grey = 0.5f; // the fog blends 0.5 to 1 over the light's depth

        void Apply(Xna::Effect& effect)
        {
            auto& passes = effect.getCurrentTechniqueProperty()->getPassesProperty();
            for (int i = 0; i < passes.getCountProperty(); ++i) {
                passes[i]->Apply();
            }
        }
    }

    ShadowMap::ShadowMap(Xna::GraphicsDevice& device, const int size) : size_(size), format_(Xna::SurfaceFormat::Color)
    {
        if (size < 16) {
            throw std::invalid_argument("ShadowMap: the size must be at least 16");
        }
        for (const auto format : {Xna::SurfaceFormat::Single, Xna::SurfaceFormat::HalfSingle, Xna::SurfaceFormat::Color}) {
            try {
                target_ = std::make_unique<Xna::RenderTarget2D>(device, size, size, false, format, Xna::DepthFormat::Depth24);
                format_ = format;
                break;
            } catch (...) {
                target_.reset();
            }
        }
        if (!target_) {
            throw std::runtime_error("ShadowMap: the device has no render target format for a shadow map");
        }
        // What a Single target really keeps: a mid grey must read back as 0.5
        // (CNA's software renderer stores Single as 8-bit colour, UP-018).
        precisionBits_ = format_ == Xna::SurfaceFormat::HalfSingle ? 11 : 8;
        if (format_ == Xna::SurfaceFormat::Single) {
            device.SetRenderTarget(target_.get());
            device.Clear(Microsoft::Xna::Framework::Color(128, 128, 128, 255));
            device.SetRenderTarget(nullptr);
            float probe = 0.0f;
            const Microsoft::Xna::Framework::Rectangle texel(0, 0, 1, 1);
            target_->GetData(0, &texel, &probe, 0, 1);
            precisionBits_ = std::fabs(probe - 128.0f / 255.0f) < 1e-4f ? 23 : 8;
        }

        rigid_ = std::make_unique<Xna::BasicEffect>(device);
        rigid_->setLightingEnabledProperty(false);
        rigid_->setTextureEnabledProperty(false);
        rigid_->setVertexColorEnabledProperty(false);
        rigid_->setDiffuseColorProperty(Vector3(Grey, Grey, Grey));
        rigid_->setFogEnabledProperty(true);
        rigid_->setFogColorProperty(Vector3(1.0f, 1.0f, 1.0f));

        // SkinnedEffect is always lit: no lights, the grey as emissive.
        skinned_ = std::make_unique<Xna::SkinnedEffect>(device);
        skinned_->setDiffuseColorProperty(Vector3::Zero);
        skinned_->setSpecularColorProperty(Vector3::Zero);
        skinned_->setAmbientLightColorProperty(Vector3::Zero);
        skinned_->setEmissiveColorProperty(Vector3(Grey, Grey, Grey));
        skinned_->getDirectionalLight0Property().setEnabledProperty(false);
        skinned_->getDirectionalLight1Property().setEnabledProperty(false);
        skinned_->getDirectionalLight2Property().setEnabledProperty(false);
        skinned_->setFogEnabledProperty(true);
        skinned_->setFogColorProperty(Vector3(1.0f, 1.0f, 1.0f));

        view_ = projection_ = viewProjection_ = Matrix::getIdentityProperty();
    }

    void ShadowMap::Fit(const Vector3& directionToLight, const Vector3& focus, const float radius)
    {
        if (!(radius > 0.0f) || !std::isfinite(radius)) {
            throw std::invalid_argument("ShadowMap::Fit: the radius must be positive");
        }
        const float length = directionToLight.Length();
        if (!(length > 1e-6f)) {
            throw std::invalid_argument("ShadowMap::Fit: needs a direction to the light");
        }
        const Vector3 towards = directionToLight * (1.0f / length);
        radius_ = radius;
        lightHeight_ = towards.Y;
        // From two radii towards the light to two radii past the focus.
        depth_ = radius * 4.0f;
        const Vector3 up = std::fabs(towards.Y) > 0.99f ? Vector3::Forward : Vector3::Up;
        // The light's orientation alone, then the focus in that frame snapped
        // to whole texels across the view (a grid fixed in the world), and the
        // eye two radii towards the light.
        const Matrix rotation = Matrix::CreateLookAt(Vector3::Zero, -towards, up);
        const Vector3 seen = Vector3::Transform(focus, rotation);
        const float texel = 2.0f * radius / static_cast<float>(size_);
        const float x = std::round(seen.X / texel) * texel;
        const float y = std::round(seen.Y / texel) * texel;
        view_ = rotation * Matrix::CreateTranslation(-x, -y, -(seen.Z + radius * 2.0f));
        projection_ = Matrix::CreateOrthographic(2.0f * radius, 2.0f * radius, 0.0f, depth_);
        viewProjection_ = view_ * projection_;
    }

    Matrix ShadowMap::DrawingProjection() const
    {
        // Receivers read the map at v = y * 0.5 + 0.5. Whether a render
        // target's first row is then the bottom of the light's view (EasyGL)
        // or its top (the software renderer) depends on the renderer
        // (upstream-issues.md UP-019); Enhanced::CalibrateShadowMap finds out.
        return upsideDown_ ? projection_ * Matrix::CreateScale(1.0f, -1.0f, 1.0f) : projection_;
    }

    bool ShadowMap::Casts(const RenderItem& item) noexcept
    {
        return item.mesh != nullptr && item.material != nullptr && item.material->blend == BlendMode::Opaque;
    }

    void ShadowMap::Render(Xna::GraphicsDevice& device, const std::span<const RenderItem> items, RenderStatistics* statistics)
    {
        drawn_ = 0;
        const auto blend = device.getBlendStateProperty();
        const auto depth = device.getDepthStencilStateProperty();
        const auto rasterizer = device.getRasterizerStateProperty();
        device.SetRenderTarget(target_.get());
        device.Clear(Microsoft::Xna::Framework::Color(255, 255, 255, 255));
        device.setBlendStateProperty(Xna::BlendState::Opaque);
        device.setDepthStencilStateProperty(Xna::DepthStencilState::Default);
        device.setRasterizerStateProperty(Xna::RasterizerState::CullNone);

        for (Xna::IEffectFog* fog : {static_cast<Xna::IEffectFog*>(rigid_.get()), static_cast<Xna::IEffectFog*>(skinned_.get())}) {
            fog->setFogStartProperty(0.0f);
            fog->setFogEndProperty(depth_);
        }
        for (Xna::IEffectMatrices* matrices :
             {static_cast<Xna::IEffectMatrices*>(rigid_.get()), static_cast<Xna::IEffectMatrices*>(skinned_.get())}) {
            matrices->setViewProperty(view_);
            matrices->setProjectionProperty(DrawingProjection());
        }

        const Microsoft::Xna::Framework::BoundingFrustum frustum(viewProjection_);
        for (const auto& item : items) {
            if (!Casts(item)) {
                continue;
            }
            if (item.instances != nullptr) {
                for (const auto& instance : item.instances->Worlds()) {
                    const Matrix world = instance * item.world;
                    if (!frustum.Intersects(WorldBounds(*item.mesh, world))) {
                        continue;
                    }
                    rigid_->setWorldProperty(world);
                    Apply(*rigid_);
                    drawn_ += item.mesh->Draw(device, statistics) ? 1 : 0;
                }
                continue;
            }
            if (!frustum.Intersects(item.bounds)) {
                continue;
            }
            if (item.bones != nullptr) {
                skinned_->setWorldProperty(item.world);
                skinned_->SetBoneTransforms(*item.bones);
                Apply(*skinned_);
            } else {
                rigid_->setWorldProperty(item.world);
                Apply(*rigid_);
            }
            drawn_ += item.mesh->Draw(device, statistics) ? 1 : 0;
        }

        device.SetRenderTarget(nullptr);
        device.setBlendStateProperty(blend);
        device.setDepthStencilStateProperty(depth);
        device.setRasterizerStateProperty(rasterizer);
    }

    float ShadowMap::SuggestedBias() const noexcept
    {
        // Stored depth runs 0.5..1 over depth_: half a unit per depth_. Three
        // texels of slope -- more as the light sinks, since level ground then
        // falls away steeply across a texel (up to eight times at a low sun)
        // -- plus one and a half steps of what the map can store.
        const float texel = 2.0f * radius_ / static_cast<float>(size_);
        const float height = std::clamp(std::fabs(lightHeight_), 0.0f, 1.0f);
        const float grazing = std::clamp(std::sqrt(1.0f - height * height) / std::max(height, 1e-3f), 1.0f, 8.0f);
        const float slope = 3.0f * texel * grazing / depth_ * 0.5f;
        const float step = std::ldexp(1.0f, -precisionBits_);
        return slope + 1.5f * step;
    }
}
