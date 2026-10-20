// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace CnaEngine::Graphics
{
    /// Lines and wire boxes for looking inside a frame: bounds, rays, paths,
    /// probe positions. Queue them during the frame, Draw() them after the
    /// scene (XNA's BasicEffect with vertex colors, unlit, no fog), Clear()
    /// for the next frame. Strict-profile: only XNA 4.0 API.
    class DebugDraw
    {
    public:
        DebugDraw() = default;
        ~DebugDraw();
        DebugDraw(const DebugDraw&) = delete;
        DebugDraw& operator=(const DebugDraw&) = delete;

        void Line(const Microsoft::Xna::Framework::Vector3& from, const Microsoft::Xna::Framework::Vector3& to,
                  const Microsoft::Xna::Framework::Color& color = Microsoft::Xna::Framework::Color(255, 255, 255, 255));
        /// The twelve edges of the box `size` across, centred on `center`.
        void Box(const Microsoft::Xna::Framework::Vector3& center, const Microsoft::Xna::Framework::Vector3& size,
                 const Microsoft::Xna::Framework::Color& color = Microsoft::Xna::Framework::Color(255, 255, 255, 255));
        void Box(const Microsoft::Xna::Framework::BoundingBox& box,
                 const Microsoft::Xna::Framework::Color& color = Microsoft::Xna::Framework::Color(255, 255, 255, 255));
        void Clear() noexcept { vertices_.clear(); }

        /// Line segments queued (a box is twelve).
        [[nodiscard]] std::size_t LineCount() const noexcept { return vertices_.size() / 2; }
        [[nodiscard]] const std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor>& Vertices()
            const noexcept
        {
            return vertices_;
        }

        /// Draws the queued lines as `camera` sees them. With `depthTest`
        /// they are hidden behind scene geometry (depth read, not written);
        /// without it they draw on top. The device's blend, depth and
        /// rasterizer states are restored afterwards. Returns the number of
        /// lines drawn.
        std::size_t Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Camera& camera,
                         bool depthTest = true);

    private:
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor> vertices_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
        Microsoft::Xna::Framework::Graphics::GraphicsDevice* effectDevice_ = nullptr;
    };
}
