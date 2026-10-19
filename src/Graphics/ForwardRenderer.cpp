// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/ForwardRenderer.hpp"

#include "CnaEngine/Graphics/Instancing.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/CullMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"

#include <algorithm>
#include <utility>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Graphics::BlendState;
    using Microsoft::Xna::Framework::Graphics::DepthStencilState;
    using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
    using Microsoft::Xna::Framework::Graphics::RasterizerState;

    namespace
    {
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
            }
            ~StateScope()
            {
                device_.setBlendStateProperty(blend_);
                device_.setDepthStencilStateProperty(depth_);
                device_.setRasterizerStateProperty(rasterizer_);
            }
            StateScope(const StateScope&) = delete;
            StateScope& operator=(const StateScope&) = delete;

        private:
            GraphicsDevice& device_;
            BlendState blend_;
            DepthStencilState depth_;
            RasterizerState rasterizer_;
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
        DrawBucket(queue.Items(RenderBucket::Opaque), RenderBucket::Opaque, camera, stats);
        DrawBucket(queue.Items(RenderBucket::AlphaTested), RenderBucket::AlphaTested, camera, stats);
        RunPasses(PassPoint::AfterOpaque, camera, stats);
        DrawBucket(queue.Items(RenderBucket::Transparent), RenderBucket::Transparent, camera, stats);
        RunPasses(PassPoint::AfterTransparent, camera, stats);
        DrawBucket(queue.Items(RenderBucket::Overlay), RenderBucket::Overlay, camera, stats);
        RunPasses(PassPoint::AfterOverlay, camera, stats);
    }

    const RasterizerState& ForwardRenderer::RasterizerFor(const Material& material)
    {
        if (material.depthBias == 0.0f && material.slopeScaleDepthBias == 0.0f) {
            return material.doubleSided ? RasterizerState::CullNone : RasterizerState::CullCounterClockwise;
        }
        for (const auto& biased : biasedStates_) {
            if (biased.doubleSided == material.doubleSided && biased.depthBias == material.depthBias
                && biased.slopeScale == material.slopeScaleDepthBias) {
                return *biased.state;
            }
        }
        auto state = std::make_unique<RasterizerState>();
        state->setCullModeProperty(material.doubleSided ? Microsoft::Xna::Framework::Graphics::CullMode::None
                                                        : Microsoft::Xna::Framework::Graphics::CullMode::CullCounterClockwiseFace);
        state->setDepthBiasProperty(material.depthBias);
        state->setSlopeScaleDepthBiasProperty(material.slopeScaleDepthBias);
        biasedStates_.push_back(BiasedState{material.doubleSided, material.depthBias, material.slopeScaleDepthBias,
                                            std::move(state)});
        return *biasedStates_.back().state;
    }

    void ForwardRenderer::DrawBucket(const std::span<const RenderItem> items, const RenderBucket bucket,
                                     const Camera& camera, RenderStatistics& statistics)
    {
        switch (bucket) {
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
            device_.setRasterizerStateProperty(RasterizerFor(material));
            if (bucket == RenderBucket::Transparent) {
                device_.setBlendStateProperty(material.blend == BlendMode::Additive ? BlendState::Additive
                                                                                    : BlendState::AlphaBlend);
            }
            if (item.instances == nullptr) {
                if (binder_->Apply(device_, item, bucket, camera, sink_)) {
                    item.mesh->Draw(device_, &statistics);
                }
                continue;
            }
            InstanceSet& set = *item.instances;
            if (set.Mesh() != item.mesh) {
                sink_(material, "instanced item ignored: its InstanceSet draws a different mesh");
                continue;
            }
            if (device_.getGraphicsProfileProperty() == Microsoft::Xna::Framework::Graphics::GraphicsProfile::HiDef) {
                // One draw: the effect's World is the shared base, the
                // instance stream supplies each instance's own transform.
                if (binder_->Apply(device_, item, bucket, camera, sink_)) {
                    set.DrawHardware(device_, &statistics);
                }
                continue;
            }
            // Reach: one draw per instance through the binder.
            RenderItem single = item;
            single.instances = nullptr;
            for (const Microsoft::Xna::Framework::Matrix& instance : set.VisibleWorlds()) {
                single.world = instance * item.world;
                if (binder_->Apply(device_, single, bucket, camera, sink_)) {
                    item.mesh->Draw(device_, &statistics);
                }
            }
        }
    }
}
