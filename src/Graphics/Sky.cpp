// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/Sky.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    namespace Xna = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        constexpr float Pi = 3.14159265358979f;
        constexpr float Degrees = Pi / 180.0f;

        Vector3 Mix(const Vector3& a, const Vector3& b, const float t) { return a + (b - a) * t; }

        SkyColors Mix(const SkyColors& a, const SkyColors& b, const float t)
        {
            SkyColors c;
            c.zenith = Mix(a.zenith, b.zenith, t);
            c.horizon = Mix(a.horizon, b.horizon, t);
            c.ground = Mix(a.ground, b.ground, t);
            c.sunLight = Mix(a.sunLight, b.sunLight, t);
            c.skyFill = Mix(a.skyFill, b.skyFill, t);
            c.ambient = Mix(a.ambient, b.ambient, t);
            c.fog = Mix(a.fog, b.fog, t);
            c.glow = Mix(a.glow, b.glow, t);
            c.sunDisc = Mix(a.sunDisc, b.sunDisc, t);
            c.stars = a.stars + (b.stars - a.stars) * t;
            return c;
        }

        Vector3 Grey(const Vector3& c, const float brightness)
        {
            const float luminance = 0.2126f * c.X + 0.7152f * c.Y + 0.0722f * c.Z;
            return Vector3(luminance, luminance, luminance) * brightness;
        }

        SkyColors MakeColors(const Vector3& zenith, const Vector3& horizon, const Vector3& ground, const Vector3& sunLight,
                      const Vector3& skyFill, const Vector3& ambient, const Vector3& glow, const Vector3& sunDisc,
                      const float stars)
        {
            SkyColors c;
            c.zenith = zenith;
            c.horizon = horizon;
            c.ground = ground;
            c.sunLight = sunLight;
            c.skyFill = skyFill;
            c.ambient = ambient;
            c.fog = horizon;
            c.glow = glow;
            c.sunDisc = sunDisc;
            c.stars = stars;
            return c;
        }

        Vector3 Clamp01(const Vector3& c)
        {
            return Vector3(std::clamp(c.X, 0.0f, 1.0f), std::clamp(c.Y, 0.0f, 1.0f), std::clamp(c.Z, 0.0f, 1.0f));
        }

        Vector3 Normalized(const Vector3& v)
        {
            const float length = std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
            return length > 1e-9f ? v * (1.0f / length) : Vector3::Up;
        }

        Vector3 Cross(const Vector3& a, const Vector3& b)
        {
            return Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
        }

        Vector3 FromAngles(const float azimuth, const float elevation)
        {
            return Vector3(std::cos(elevation) * std::sin(azimuth), std::sin(elevation), -std::cos(elevation) * std::cos(azimuth));
        }

        // The dome's colour in a direction: horizon to zenith above, horizon
        // to ground below, plus the glow around the sun (strongest when the
        // sun is low).
        Vector3 DomeColor(const SkyColors& colors, const Vector3& direction, const Vector3& sun)
        {
            const float elevation = std::asin(std::clamp(direction.Y, -1.0f, 1.0f)) / Degrees;
            Vector3 c = elevation >= 0.0f ? Mix(colors.horizon, colors.zenith, std::pow(elevation / 90.0f, 0.45f))
                                          : Mix(colors.horizon, colors.ground, std::min(1.0f, -elevation / 12.0f));
            const float towards = std::max(0.0f, direction.X * sun.X + direction.Y * sun.Y + direction.Z * sun.Z);
            const float low = std::clamp(1.0f - std::max(sun.Y, 0.0f) * 1.6f, 0.0f, 1.0f);
            const float above = elevation > -6.0f ? 1.0f : 0.0f;
            c = c + colors.glow * (std::pow(towards, 6.0f) * (0.35f + 0.65f * low) * above);
            return Clamp01(c);
        }

        Color ToColor(const Vector3& c, const float alpha = 1.0f)
        {
            return Color(static_cast<int>(std::lround(c.X * 255.0f)), static_cast<int>(std::lround(c.Y * 255.0f)),
                         static_cast<int>(std::lround(c.Z * 255.0f)), static_cast<int>(std::lround(alpha * 255.0f)));
        }
    }

    DayNightPalette DayNightPalette::Default()
    {
        std::vector<Key> keys;
        keys.push_back({-18.0f, MakeColors(Vector3(0.01f, 0.015f, 0.04f), Vector3(0.03f, 0.04f, 0.08f), Vector3(0.02f, 0.02f, 0.03f),
                                    Vector3(0.06f, 0.08f, 0.14f), Vector3(0.03f, 0.04f, 0.08f), Vector3(0.03f, 0.035f, 0.06f),
                                    Vector3::Zero, Vector3::Zero, 1.0f)});
        keys.push_back({-6.0f, MakeColors(Vector3(0.06f, 0.08f, 0.2f), Vector3(0.35f, 0.25f, 0.3f), Vector3(0.08f, 0.07f, 0.08f),
                                   Vector3(0.1f, 0.08f, 0.12f), Vector3(0.12f, 0.12f, 0.2f), Vector3(0.1f, 0.1f, 0.15f),
                                   Vector3(0.6f, 0.3f, 0.15f), Vector3::Zero, 0.4f)});
        keys.push_back({0.0f, MakeColors(Vector3(0.25f, 0.35f, 0.6f), Vector3(0.95f, 0.6f, 0.35f), Vector3(0.25f, 0.22f, 0.2f),
                                  Vector3(0.8f, 0.45f, 0.25f), Vector3(0.3f, 0.32f, 0.45f), Vector3(0.25f, 0.24f, 0.28f),
                                  Vector3(1.0f, 0.55f, 0.25f), Vector3(1.0f, 0.7f, 0.4f), 0.0f)});
        keys.push_back({10.0f, MakeColors(Vector3(0.3f, 0.5f, 0.85f), Vector3(0.85f, 0.75f, 0.6f), Vector3(0.35f, 0.33f, 0.3f),
                                   Vector3(1.0f, 0.85f, 0.65f), Vector3(0.35f, 0.42f, 0.55f), Vector3(0.3f, 0.32f, 0.36f),
                                   Vector3(0.9f, 0.6f, 0.35f), Vector3(1.0f, 0.9f, 0.7f), 0.0f)});
        keys.push_back({30.0f, MakeColors(Vector3(0.25f, 0.45f, 0.85f), Vector3(0.65f, 0.78f, 0.92f), Vector3(0.4f, 0.4f, 0.38f),
                                   Vector3(1.0f, 0.96f, 0.88f), Vector3(0.35f, 0.42f, 0.55f), Vector3(0.32f, 0.35f, 0.4f),
                                   Vector3(0.3f, 0.3f, 0.25f), Vector3(1.0f, 1.0f, 0.95f), 0.0f)});
        keys.push_back({90.0f, MakeColors(Vector3(0.2f, 0.4f, 0.82f), Vector3(0.62f, 0.76f, 0.92f), Vector3(0.4f, 0.4f, 0.38f),
                                   Vector3(1.0f, 0.97f, 0.9f), Vector3(0.35f, 0.42f, 0.55f), Vector3(0.33f, 0.36f, 0.41f),
                                   Vector3(0.25f, 0.25f, 0.2f), Vector3(1.0f, 1.0f, 0.97f), 0.0f)});
        return DayNightPalette(std::move(keys));
    }

    DayNightPalette::DayNightPalette(std::vector<Key> keys) : keys_(std::move(keys))
    {
        if (keys_.empty()) {
            throw std::invalid_argument("DayNightPalette: needs at least one key");
        }
        for (std::size_t i = 1; i < keys_.size(); ++i) {
            if (!(keys_[i].elevationDegrees > keys_[i - 1].elevationDegrees)) {
                throw std::invalid_argument("DayNightPalette: keys must rise in elevation");
            }
        }
    }

    SkyColors DayNightPalette::At(const float sunElevationDegrees, const float overcast) const
    {
        SkyColors colors;
        if (!(sunElevationDegrees > keys_.front().elevationDegrees)) {
            colors = keys_.front().colors;
        } else if (sunElevationDegrees >= keys_.back().elevationDegrees) {
            colors = keys_.back().colors;
        } else {
            std::size_t next = 1;
            while (keys_[next].elevationDegrees < sunElevationDegrees) {
                ++next;
            }
            const Key& a = keys_[next - 1];
            const Key& b = keys_[next];
            colors = Mix(a.colors, b.colors,
                         (sunElevationDegrees - a.elevationDegrees) / (b.elevationDegrees - a.elevationDegrees));
        }
        const float cloud = std::clamp(overcast, 0.0f, 1.0f);
        if (cloud > 0.0f) {
            // A grey, dimmer sky; the sun's light is mostly scattered into the fill.
            colors.zenith = Mix(colors.zenith, Grey(colors.zenith, 1.1f), cloud);
            colors.horizon = Mix(colors.horizon, Grey(colors.horizon, 0.95f), cloud);
            colors.fog = Mix(colors.fog, Grey(colors.fog, 0.95f), cloud);
            colors.skyFill = Mix(colors.skyFill, Grey(colors.skyFill + colors.sunLight * 0.3f, 1.0f), cloud);
            colors.sunLight = colors.sunLight * (1.0f - 0.8f * cloud);
            colors.glow = colors.glow * (1.0f - cloud);
            colors.sunDisc = colors.sunDisc * (1.0f - cloud);
            colors.stars *= 1.0f - cloud;
        }
        return colors;
    }

    SunAngles TimeOfDay::Sun() const noexcept
    {
        return SunAnglesAt(latitudeDegrees, dayOfYear, hours, solarNoonHours);
    }

    Vector3 TimeOfDay::DirectionToSun() const noexcept
    {
        return CnaEngine::DirectionToSun(Sun());
    }

    void TimeOfDay::Advance(const double seconds, const double timeScale) noexcept
    {
        if (!std::isfinite(seconds) || !std::isfinite(timeScale)) {
            return;
        }
        hours = std::fmod(hours + seconds * timeScale / 3600.0, 24.0);
        if (hours < 0.0) {
            hours += 24.0;
        }
    }

    void ApplySkyLighting(ForwardLighting& lighting, const SkyColors& colors, const Vector3& directionToSun,
                          const std::optional<float> fogStart, const std::optional<float> fogEnd)
    {
        const Vector3 sun = Normalized(directionToSun);
        lighting.defaultLighting = false;
        // Below the horizon the key light comes from the other side: the moon.
        lighting.lightDirection = sun.Y > -0.05f ? -sun : sun;
        lighting.lightColor = colors.sunLight;
        lighting.fillLights = {{Vector3(0.0f, -1.0f, 0.0f), colors.skyFill, Vector3::Zero}};
        lighting.ambientColor = colors.ambient;
        lighting.fogEnabled = fogStart.has_value() && fogEnd.has_value();
        lighting.fogColor = colors.fog;
        if (lighting.fogEnabled) {
            lighting.fogStart = *fogStart;
            lighting.fogEnd = *fogEnd;
        }
    }

    SkyDome::SkyDome(Xna::GraphicsDevice& device, const int segments, const int rings, const int stars,
                     const std::uint32_t seed)
    {
        if (segments < 8 || rings < 3 || stars < 0) {
            throw std::invalid_argument("SkyDome: needs at least 8 segments and 3 rings");
        }
        // Rings from 20 degrees below the horizon to just under the zenith,
        // closer together near the horizon where the colours change most.
        std::vector<float> elevations;
        for (int r = 0; r < rings; ++r) {
            const float t = static_cast<float>(r) / static_cast<float>(rings - 1);
            elevations.push_back((-20.0f + 108.0f * t * t * 0.5f + 108.0f * t * 0.5f) * Degrees);
        }
        auto at = [&](const int ring, const int segment) {
            return FromAngles(2.0f * Pi * static_cast<float>(segment) / static_cast<float>(segments),
                              elevations[static_cast<std::size_t>(ring)]);
        };
        for (int r = 0; r + 1 < rings; ++r) {
            for (int s = 0; s < segments; ++s) {
                const Vector3 a = at(r, s);
                const Vector3 b = at(r, s + 1);
                const Vector3 c = at(r + 1, s);
                const Vector3 d = at(r + 1, s + 1);
                dome_.insert(dome_.end(), {a, c, b, b, c, d});
            }
        }
        const Vector3 apex = Vector3::Up;
        const Vector3 nadir(0.0f, -1.0f, 0.0f);
        for (int s = 0; s < segments; ++s) {
            dome_.insert(dome_.end(), {at(rings - 1, s), apex, at(rings - 1, s + 1)});
            // Closed below the skirt, in the ground's colour, so no view looks into the clear colour.
            dome_.insert(dome_.end(), {at(0, s + 1), nadir, at(0, s)});
        }

        // Stars: small quads above 5 degrees, from a fixed seed.
        std::uint32_t state = seed != 0 ? seed : 1;
        auto random = [&state]() {
            state = state * 1664525u + 1013904223u;
            return static_cast<float>(state >> 8) / 16777216.0f;
        };
        for (int i = 0; i < stars; ++i) {
            const float azimuth = 2.0f * Pi * random();
            const float elevation = std::asin(0.09f + 0.91f * random());
            const Vector3 centre = FromAngles(azimuth, elevation);
            const Vector3 right = Normalized(Cross(centre, Vector3::Up)) * 0.003f;
            const Vector3 up = Normalized(Cross(right, centre)) * 0.003f;
            stars_.insert(stars_.end(), {centre - right - up, centre - right + up, centre + right - up,
                                         centre + right - up, centre - right + up, centre + right + up});
            starBrightness_.push_back(0.35f + 0.65f * random() * random());
        }

        effect_ = std::make_unique<Xna::BasicEffect>(device);
        effect_->setVertexColorEnabledProperty(true);
        effect_->setLightingEnabledProperty(false);
        effect_->setTextureEnabledProperty(false);
        effect_->setFogEnabledProperty(false);
        SetColors(SkyColors{}, Normalized(Vector3(0.3f, 0.8f, -0.5f)));
    }

    void SkyDome::SetColors(const SkyColors& colors, const Vector3& directionToSun)
    {
        const Vector3 sun = Normalized(directionToSun);
        domeVertices_.resize(dome_.size());
        for (std::size_t i = 0; i < dome_.size(); ++i) {
            domeVertices_[i] = Xna::VertexPositionColor(dome_[i], ToColor(DomeColor(colors, dome_[i], sun)));
        }

        // The sun's disc: a fan of 16 triangles around the sun, 1.5 degrees across.
        discVertices_.clear();
        if (sun.Y > -0.03f && colors.sunDisc.X + colors.sunDisc.Y + colors.sunDisc.Z > 0.0f) {
            const Vector3 right = Normalized(Cross(sun, std::fabs(sun.Y) > 0.99f ? Vector3::Right : Vector3::Up));
            const Vector3 up = Normalized(Cross(right, sun));
            const float radius = std::tan(1.5f * Degrees);
            const Color disc = ToColor(Clamp01(colors.sunDisc));
            for (int i = 0; i < 16; ++i) {
                const float a0 = 2.0f * Pi * static_cast<float>(i) / 16.0f;
                const float a1 = 2.0f * Pi * static_cast<float>(i + 1) / 16.0f;
                const Vector3 p0 = Normalized(sun + (right * std::cos(a0) + up * std::sin(a0)) * radius);
                const Vector3 p1 = Normalized(sun + (right * std::cos(a1) + up * std::sin(a1)) * radius);
                discVertices_.insert(discVertices_.end(),
                                     {Xna::VertexPositionColor(sun, disc), Xna::VertexPositionColor(p0, disc),
                                      Xna::VertexPositionColor(p1, disc)});
            }
        }

        starVisibility_ = std::clamp(colors.stars, 0.0f, 1.0f);
        starVertices_.resize(stars_.size());
        for (std::size_t i = 0; i < stars_.size(); ++i) {
            const float b = starVisibility_ * starBrightness_[i / 6];
            starVertices_[i] = Xna::VertexPositionColor(stars_[i], ToColor(Vector3(b, b, b * 0.95f)));
        }
    }

    void SkyDome::Draw(Xna::GraphicsDevice& device, const Camera& camera, RenderStatistics* statistics)
    {
        const float far = camera.FarPlane() > 0.0f ? camera.FarPlane() : 500.0f;
        const float near = std::max(camera.NearPlane(), 0.0f);
        // Inside the far plane, outside the near one.
        const float radius = std::max(far * 0.9f, near * 4.0f + 1e-3f);
        effect_->setWorldProperty(Matrix::CreateScale(radius) * Matrix::CreateTranslation(camera.Position()));
        effect_->setViewProperty(camera.View());
        effect_->setProjectionProperty(camera.Projection());

        const auto blend = device.getBlendStateProperty();
        const auto depth = device.getDepthStencilStateProperty();
        const auto rasterizer = device.getRasterizerStateProperty();
        device.setDepthStencilStateProperty(Xna::DepthStencilState::None);
        device.setRasterizerStateProperty(Xna::RasterizerState::CullNone);
        device.setBlendStateProperty(Xna::BlendState::Opaque);
        auto& passes = effect_->getCurrentTechniqueProperty()->getPassesProperty();
        auto draw = [&](std::vector<Xna::VertexPositionColor>& vertices) {
            if (vertices.empty()) {
                return;
            }
            const int triangles = static_cast<int>(vertices.size() / 3);
            for (int i = 0; i < passes.getCountProperty(); ++i) {
                passes[i]->Apply();
                device.DrawUserPrimitives(Xna::PrimitiveType::TriangleList, vertices.data(), 0, triangles,
                                          Xna::VertexPositionColor::getVertexDeclarationStatic());
            }
            if (statistics != nullptr) {
                statistics->Record(static_cast<std::uint64_t>(triangles), static_cast<std::uint64_t>(vertices.size()));
            }
        };
        draw(domeVertices_);
        draw(discVertices_);
        if (starVisibility_ > 0.0f) {
            device.setBlendStateProperty(Xna::BlendState::Additive);
            draw(starVertices_);
        }
        device.setBlendStateProperty(blend);
        device.setDepthStencilStateProperty(depth);
        device.setRasterizerStateProperty(rasterizer);
    }

    void SkyDome::AttachTo(ForwardRenderer& renderer, std::string passName)
    {
        renderer.AddPass(PassPoint::BeforeOpaque, std::move(passName),
                         [this](Xna::GraphicsDevice& device, const Camera& camera, RenderStatistics& statistics) {
                             Draw(device, camera, &statistics);
                         });
    }
}
