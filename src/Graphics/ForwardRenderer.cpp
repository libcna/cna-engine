// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/ForwardRenderer.hpp"

#include "CnaEngine/Graphics/Instancing.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/CullMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Graphics::BlendState;
    using Microsoft::Xna::Framework::Graphics::DepthStencilState;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::RasterizerState;
    using Microsoft::Xna::Framework::Graphics::SamplerState;

    namespace
    {
        // Texture slots binders set samplers on (PbrEffect samples five).
        constexpr int SamplerSlots = 5;

        // Restores the states a renderer changes, whatever happens in between.
        class StateScope
        {
        public:
            explicit StateScope(GraphicsDevice& device)
                : device_(device),
                  blend_(device.getBlendStateProperty()),
                  depth_(device.getDepthStencilStateProperty()),
                  rasterizer_(device.getRasterizerStateProperty())
            {
                for (int slot = 0; slot < SamplerSlots; ++slot) {
                    samplers_.push_back(device.getSamplerStatesProperty()[slot]);
                }
            }
            ~StateScope()
            {
                device_.setBlendStateProperty(blend_);
                device_.setDepthStencilStateProperty(depth_);
                device_.setRasterizerStateProperty(rasterizer_);
                for (int slot = 0; slot < SamplerSlots; ++slot) {
                    device_.getSamplerStatesProperty()[slot] = samplers_[static_cast<std::size_t>(slot)];
                }
            }
            StateScope(const StateScope&) = delete;
            StateScope& operator=(const StateScope&) = delete;

        private:
            GraphicsDevice& device_;
            BlendState blend_;
            DepthStencilState depth_;
            RasterizerState rasterizer_;
            std::vector<SamplerState> samplers_;
        };
    }

    ForwardRenderer::ForwardRenderer(GraphicsDevice& device)
        : device_(device), stock_(std::make_shared<StockMaterialBinder>(device)), binder_(stock_)
    {
        sink_ = [this](const Material& material, const std::string& what) {
            std::string message = "material '" + material.name + "': " + what;
            if (std::find(fallbacks_.begin(), fallbacks_.end(), message) == fallbacks_.end()) {
                fallbacks_.push_back(std::move(message));
            }
        };
    }

    void ForwardRenderer::SetMaterialBinder(std::shared_ptr<IMaterialBinder> binder)
    {
        binder_ = binder ? std::move(binder) : std::shared_ptr<IMaterialBinder>(stock_);
    }

    void ForwardRenderer::AddPass(const PassPoint point, std::string name, PassCallback callback)
    {
        passes_.push_back(Pass{point, std::move(name), std::move(callback)});
    }

    std::size_t ForwardRenderer::RemovePass(const std::string& name)
    {
        return std::erase_if(passes_, [&](const Pass& pass) { return pass.name == name; });
    }

    void ForwardRenderer::RunPasses(const PassPoint point, const Camera& camera, RenderStatistics& statistics)
    {
        for (const auto& pass : passes_) {
            if (pass.point == point && pass.callback) {
                pass.callback(device_, camera, statistics);
            }
        }
    }

    void ForwardRenderer::Render(const RenderQueue& queue, const Camera& camera, RenderStatistics* statistics)
    {
        RenderStatistics local;
        RenderStatistics& stats = statistics != nullptr ? *statistics : local;
        fallbacks_.clear();
        const StateScope restore(device_);
        binder_->BeginFrame(device_, camera, lighting_);

        RunPasses(PassPoint::BeforeOpaque, camera, stats);
        DrawBucket(queue.Items(RenderBucket::Sequenced), RenderBucket::Sequenced, camera, stats);
        DrawBucket(queue.Items(RenderBucket::Opaque), RenderBucket::Opaque, camera, stats);
        DrawBucket(queue.Items(RenderBucket::AlphaTested), RenderBucket::AlphaTested, camera, stats);
        RunPasses(PassPoint::AfterOpaque, camera, stats);
        DrawBucket(queue.Items(RenderBucket::Transparent), RenderBucket::Transparent, camera, stats);
        RunPasses(PassPoint::AfterTransparent, camera, stats);
        DrawBucket(queue.Items(RenderBucket::Overlay), RenderBucket::Overlay, camera, stats);
        RunPasses(PassPoint::AfterOverlay, camera, stats);
    }

    const RasterizerState& ForwardRenderer::RasterizerFor(const Material& material, const bool mirrored)
    {
        // XNA's front faces are clockwise; counter-clockwise materials and a
        // mirrored view (Camera::SetMirrored) each turn them around.
        using Microsoft::Xna::Framework::Graphics::CullMode;
        const bool flipped = (material.frontFace == FrontFace::CounterClockwise) != mirrored;
        const CullMode cull = material.doubleSided ? CullMode::None
                              : flipped            ? CullMode::CullClockwiseFace
                                                   : CullMode::CullCounterClockwiseFace;
        if (material.depthBias == 0.0f && material.slopeScaleDepthBias == 0.0f) {
            return cull == CullMode::None                ? RasterizerState::CullNone
                   : cull == CullMode::CullClockwiseFace ? RasterizerState::CullClockwise
                                                         : RasterizerState::CullCounterClockwise;
        }
        for (const auto& biased : biasedStates_) {
            if (biased.cull == cull && biased.depthBias == material.depthBias
                && biased.slopeScale == material.slopeScaleDepthBias) {
                return *biased.state;
            }
        }
        auto state = std::make_unique<RasterizerState>();
        state->setCullModeProperty(cull);
        state->setDepthBiasProperty(material.depthBias);
        state->setSlopeScaleDepthBiasProperty(material.slopeScaleDepthBias);
        biasedStates_.push_back(BiasedState{cull, material.depthBias, material.slopeScaleDepthBias, std::move(state)});
        return *biasedStates_.back().state;
    }

    void ForwardRenderer::DrawBucket(const std::span<const RenderItem> items, const RenderBucket bucket,
                                     const Camera& camera, RenderStatistics& statistics)
    {
        if (bucket == RenderBucket::Sequenced && items.empty()) {
            // Leave the device as the BeforeOpaque passes left it.
            return;
        }
        switch (bucket) {
            case RenderBucket::Sequenced:
                // Set item by item below, from each material's bucket.
                break;
            case RenderBucket::Opaque:
            case RenderBucket::AlphaTested:
                device_.setBlendStateProperty(BlendState::Opaque);
                device_.setDepthStencilStateProperty(DepthStencilState::Default);
                break;
            case RenderBucket::Transparent:
                device_.setBlendStateProperty(BlendState::AlphaBlend);
                device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
                break;
            case RenderBucket::Overlay:
            case RenderBucket::Count:
                device_.setBlendStateProperty(BlendState::AlphaBlend);
                device_.setDepthStencilStateProperty(DepthStencilState::None);
                break;
        }
        device_.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);
        for (const auto& item : items) {
            const Material& material = *item.material;
            // A sequenced item draws as it would in its material's bucket.
            const RenderBucket drawnAs = bucket == RenderBucket::Sequenced ? BucketFor(material) : bucket;
            device_.setRasterizerStateProperty(RasterizerFor(material, camera.Mirrored()));
            if (bucket == RenderBucket::Sequenced) {
                const bool blended = drawnAs == RenderBucket::Transparent;
                device_.setBlendStateProperty(blended ? binder_->BlendStateFor(item) : BlendState::Opaque);
                device_.setDepthStencilStateProperty(blended ? DepthStencilState::DepthRead
                                                             : DepthStencilState::Default);
            } else if (bucket == RenderBucket::Transparent || bucket == RenderBucket::Overlay) {
                // What the binder's effect writes decides the blend (premultiplied or straight).
                device_.setBlendStateProperty(binder_->BlendStateFor(item));
            }
            if (item.instances == nullptr) {
                if (binder_->Apply(device_, item, drawnAs, camera, sink_)) {
                    item.mesh->Draw(device_, &statistics);
                }
                continue;
            }
            InstanceSet& set = *item.instances;
            if (set.Mesh() != item.mesh) {
                sink_(material, "instanced item ignored: its InstanceSet draws a different mesh");
                continue;
            }
            if (item.bones != nullptr) {
                sink_(material, "instanced item ignored: skinned items are not instanced");
                continue;
            }
            if (device_.getGraphicsProfileProperty() == Microsoft::Xna::Framework::Graphics::GraphicsProfile::HiDef
                && item.mesh->Indexed()) {
                // One draw: the effect's World is the shared base, the
                // instance stream supplies each instance's own transform.
                if (binder_->Apply(device_, item, drawnAs, camera, sink_)) {
                    set.DrawHardware(device_, &statistics);
                }
                continue;
            }
            // Reach, or a non-indexed mesh: one draw per instance through the binder.
            RenderItem single = item;
            single.instances = nullptr;
            for (const Microsoft::Xna::Framework::Matrix& instance : set.VisibleWorlds()) {
                single.world = instance * item.world;
                if (binder_->Apply(device_, single, drawnAs, camera, sink_)) {
                    item.mesh->Draw(device_, &statistics);
                }
            }
        }
    }
}
