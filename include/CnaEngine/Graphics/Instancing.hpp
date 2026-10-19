// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/RenderStatistics.hpp"
#include "CnaEngine/Graphics/Visibility.hpp"

#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IEffectMatrices.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace CnaEngine::Graphics
{
    /// How instances of one mesh are drawn.
    enum class InstancingPath : std::uint8_t
    {
        /// One DrawInstancedPrimitives call; needs the HiDef profile and an
        /// effect that reads the per-instance stream (InstanceDeclaration()).
        Hardware,
        /// One draw per instance with the effect's World matrix set; works
        /// with any effect and profile.
        PerInstance,
    };

    /// Many copies of one mesh, each with its own world matrix.
    ///
    /// The per-instance stream is a 64-byte world matrix declared as four
    /// Vector4 TextureCoordinate elements with usage indices 1-4 (the layout
    /// of the XNA instancing samples). Instance data layout and update
    /// frequency are the caller's: Update() may run every frame (dynamic
    /// objects) or once (static scenery); only changed data is uploaded.
    class InstanceSet
    {
    public:
        /// The GPU declaration of the per-instance stream.
        [[nodiscard]] static const Microsoft::Xna::Framework::Graphics::VertexDeclaration& InstanceDeclaration();

        InstanceSet() = default;
        explicit InstanceSet(const GpuMesh* mesh) : mesh_(mesh) {}

        void SetMesh(const GpuMesh* mesh) noexcept { mesh_ = mesh; }
        [[nodiscard]] const GpuMesh* Mesh() const noexcept { return mesh_; }

        void Clear() noexcept;
        void Add(const Microsoft::Xna::Framework::Matrix& world);
        void Set(std::size_t index, const Microsoft::Xna::Framework::Matrix& world);
        [[nodiscard]] std::size_t Count() const noexcept { return worlds_.size(); }
        [[nodiscard]] const std::vector<Microsoft::Xna::Framework::Matrix>& Worlds() const noexcept { return worlds_; }

        /// Keeps only instances whose world bounds pass `settings`; the set
        /// of visible instances is used by the next Draw*. Adds to `stats`.
        void Cull(const Camera& camera, const VisibilitySettings& settings, CullStatistics& stats);
        /// Uses every instance (no culling).
        void UseAll();
        [[nodiscard]] std::size_t VisibleCount() const noexcept;

        /// Chooses the hardware path when the device profile allows it and the
        /// caller states that its effect consumes the instance stream.
        [[nodiscard]] static InstancingPath ChoosePath(const Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                                       bool effectReadsInstanceStream);

        /// Hardware path: uploads visible instance matrices (when changed),
        /// binds mesh + instance streams and issues one instanced draw. The
        /// caller has applied the effect pass. Throws std::logic_error on a
        /// Reach device. Returns false when there is nothing to draw.
        bool DrawHardware(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                          RenderStatistics* statistics = nullptr);

        /// Per-instance path: for each visible instance sets the World matrix
        /// on `matrices`, calls `applyPass` (which applies the effect pass)
        /// and draws. Returns the number of instances drawn.
        std::size_t DrawPerInstance(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                    Microsoft::Xna::Framework::Graphics::IEffectMatrices& matrices,
                                    const std::function<void()>& applyPass, RenderStatistics* statistics = nullptr);

    private:
        void Upload(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        const GpuMesh* mesh_ = nullptr;
        std::vector<Microsoft::Xna::Framework::Matrix> worlds_;
        std::vector<std::uint32_t> visible_;
        bool cullActive_ = false;
        bool dirty_ = true;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::DynamicVertexBuffer> instances_;
        std::vector<float> staging_;
    };
}
