// SPDX-License-Identifier: MIT
//
// Enhanced profile (ADR-001): this header uses cna-extended
// (CNA::Extended::World3DEXT particles) and belongs to
// CnaEngine::ExtendedIntegration only.
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"

#include "CNA/Extended/World3DEXT/ParticleEffect3DEXT.hpp"

#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace CnaEngine::Extended
{
    /// cna-extended's 3D particles (ParticleEffect3DEXT: emitters, profiles,
    /// modifiers) drawn by the engine's renderer: every live particle becomes
    /// a quad facing the camera, with its colour, opacity, scale and rotation,
    /// in one transparent render item -- sorted and drawn with the frame's
    /// other transparent items rather than in a separate pass (plan.md
    /// ENG-030, GAME-062). The material is additive with a soft round
    /// texture by default; change Material() for smoke (alpha blending, a
    /// texture of the game's).
    class ParticleBillboards
    {
    public:
        explicit ParticleBillboards(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        /// The quads of `effect` seen from `camera`, as an item that borrows
        /// this object's buffers and material until the next Build; nothing
        /// when no particle lives.
        [[nodiscard]] std::optional<Graphics::RenderItem> Build(const CNA::Extended::World3DEXT::ParticleEffect3DEXT& effect,
                                                                const Graphics::Camera& camera);
        [[nodiscard]] Graphics::Material& Material() noexcept { return material_; }
        /// Particles drawn by the last Build.
        [[nodiscard]] std::size_t Count() const noexcept { return count_; }

    private:
        void Reserve(std::size_t particles);
        void Upload();

        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> soft_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::DynamicVertexBuffer> vertices_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> indices_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColorTexture> staging_;
        std::vector<std::uint8_t> bytes_;
        std::size_t capacity_ = 0;
        std::size_t count_ = 0;
        Graphics::GpuMesh mesh_;
        Graphics::Material material_;
    };
}
