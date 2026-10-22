// SPDX-License-Identifier: MIT
#include "CnaEngine/Extended/ParticleBillboards.hpp"

#include "CnaEngine/Graphics/ForwardRenderer.hpp"

#include "CNA/Extended/World3DEXT/ParticleEmitter3DEXT.hpp"

#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace CnaEngine::Extended
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;
    namespace World = CNA::Extended::World3DEXT;

    // A puff of particles at the origin is drawn as quads that face the camera.
    TEST(ParticleBillboardsTest, LiveParticlesBecomeOneTransparentItem)
    {
        Xna::GraphicsDevice device;
        ParticleBillboards billboards(device);
        World::ParticleEffect3DEXT effect;
        auto& emitter = effect.AddEmitterEXT(std::make_unique<World::ParticleEmitter3DEXT>());
        emitter.MinSpeedEXT = emitter.MaxSpeedEXT = 0.0f;
        emitter.MinScaleEXT = emitter.MaxScaleEXT = 2.0f;
        emitter.StartColorEXT = emitter.EndColorEXT = Color(255, 160, 40, 255);
        emitter.StartOpacityEXT = emitter.EndOpacityEXT = 1.0f;

        Graphics::Camera camera;
        camera.SetOrthographic(8.0f, 8.0f, 0.1f, 50.0f);
        camera.SetLookAt(Vector3(0.0f, 0.0f, 10.0f), Vector3::Zero);
        EXPECT_FALSE(billboards.Build(effect, camera).has_value());

        Xna::RenderTarget2D target(device, 32, 32, false, Xna::SurfaceFormat::Color, Xna::DepthFormat::Depth24);
        Graphics::ForwardRenderer renderer(device);
        auto draw = [&](const Graphics::RenderItem& item, Graphics::RenderStatistics& statistics) {
            Graphics::RenderQueue queue;
            queue.Submit(item);
            queue.Sort(camera.Position());
            device.SetRenderTarget(&target);
            device.Clear(Color(0, 0, 0, 255));
            renderer.Render(queue, camera, &statistics);
            device.SetRenderTarget(nullptr);
            std::vector<Color> pixels(32 * 32);
            target.GetData(pixels.data(), static_cast<int>(pixels.size()));
            return pixels;
        };

        // One particle: an orange dot at the centre, the corner 4 m off dark.
        emitter.EmitEXT(1, Vector3::Zero);
        auto item = billboards.Build(effect, camera);
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(item->material->blend, Graphics::BlendMode::Additive);
        Graphics::RenderStatistics one;
        auto pixels = draw(*item, one);
        const Color centre = pixels[16 * 32 + 16];
        EXPECT_GT(centre.getRProperty(), 200);
        EXPECT_LT(centre.getBProperty(), 80);
        EXPECT_EQ(pixels[0].getRProperty(), 0);
        EXPECT_EQ(one.primitives, 2u);

        // Ten: one item of twenty triangles.
        emitter.EmitEXT(9, Vector3::Zero);
        item = billboards.Build(effect, camera);
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(billboards.Count(), 10u);
        Graphics::RenderStatistics ten;
        (void)draw(*item, ten);
        EXPECT_EQ(ten.primitives, 20u);
    }
}
