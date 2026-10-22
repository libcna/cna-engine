// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace CnaEngine::Graphics
{
    /// A full-screen step: reads `source` and draws the result into the
    /// device's current render target (the next step's source, or the back
    /// buffer for the last one), which the chain has set and cleared.
    using PostPass = std::function<void(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                        Microsoft::Xna::Framework::Graphics::Texture2D& source,
                                        Microsoft::Xna::Framework::Graphics::SpriteBatch& batch)>;

    /// Post-processing (plan.md ENG-035 step 4, GAME-060): the world is drawn
    /// into an off-screen target, then a chain of full-screen passes takes it
    /// to the back buffer -- the HUD drawn afterwards stays untouched. Passes
    /// are a game's own (any Effect, a custom shader through SpriteBatch) or
    /// the engine's strict ones below, which need nothing but SpriteBatch and
    /// so run on every renderer: island-simulator's time-of-day grade (a
    /// full-screen multiply), fades, a vignette. Room-simulator's shader chain
    /// (bloom, tonemap, FXAA) is out of reach without custom shaders.
    class PostProcessChain
    {
    public:
        explicit PostProcessChain(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        /// Adds a pass at the end of the chain.
        void Add(std::string name, PostPass pass);
        /// Removes every pass called `name`; returns how many.
        std::size_t Remove(const std::string& name);
        [[nodiscard]] std::size_t Count() const noexcept { return passes_.size(); }

        /// Sets the scene target (sized like the viewport, recreated when the
        /// viewport changes) as the device's target: draw the world after this.
        void BeginScene();
        /// Runs the passes from the scene target to the back buffer. Without
        /// passes the scene is copied as it is.
        void EndScene();

        [[nodiscard]] Microsoft::Xna::Framework::Graphics::RenderTarget2D* SceneTarget() noexcept { return scene_.get(); }

    private:
        struct Named
        {
            std::string name;
            PostPass pass;
        };
        void Ensure(int width, int height);

        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> batch_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::RenderTarget2D> scene_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::RenderTarget2D> spare_;
        std::vector<Named> passes_;
    };

    /// Multiplies the picture by `tint` and scales it by `brightness` -- a
    /// night grade, a sepia cast, a darkened pause screen.
    [[nodiscard]] PostPass ColorGradePass(std::function<Microsoft::Xna::Framework::Vector3()> tint,
                                          std::function<float()> brightness = {});
    /// Blends the picture towards `color` by `amount()` (0 none, 1 all): fades
    /// to black, a red flash when hurt.
    [[nodiscard]] PostPass FadePass(Microsoft::Xna::Framework::Color color, std::function<float()> amount);
    /// Darkens the corners by `strength` (0..1); the gradient is made once.
    [[nodiscard]] PostPass VignettePass(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, float strength = 0.5f);
}
