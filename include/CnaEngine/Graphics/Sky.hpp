// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Core/SunPosition.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/MaterialBinder.hpp"
#include "CnaEngine/Graphics/RenderStatistics.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace CnaEngine::Graphics
{
    /// The sky's and the light's colours at one moment (linear 0..1).
    struct SkyColors
    {
        Microsoft::Xna::Framework::Vector3 zenith = Microsoft::Xna::Framework::Vector3(0.25f, 0.45f, 0.85f);
        Microsoft::Xna::Framework::Vector3 horizon = Microsoft::Xna::Framework::Vector3(0.65f, 0.78f, 0.92f);
        /// Below the horizon (the dome's skirt).
        Microsoft::Xna::Framework::Vector3 ground = Microsoft::Xna::Framework::Vector3(0.4f, 0.4f, 0.38f);
        /// The sun's (or at night the moon's) light.
        Microsoft::Xna::Framework::Vector3 sunLight = Microsoft::Xna::Framework::Vector3(1.0f, 0.96f, 0.88f);
        /// Light from the sky above (a fill light pointing down).
        Microsoft::Xna::Framework::Vector3 skyFill = Microsoft::Xna::Framework::Vector3(0.35f, 0.42f, 0.55f);
        Microsoft::Xna::Framework::Vector3 ambient = Microsoft::Xna::Framework::Vector3(0.32f, 0.35f, 0.4f);
        /// The fog; the horizon's colour, so distant things fade into the sky.
        Microsoft::Xna::Framework::Vector3 fog = Microsoft::Xna::Framework::Vector3(0.65f, 0.78f, 0.92f);
        /// Added around the sun, strongest near the horizon (dawn, dusk).
        Microsoft::Xna::Framework::Vector3 glow = Microsoft::Xna::Framework::Vector3(0.3f, 0.3f, 0.25f);
        Microsoft::Xna::Framework::Vector3 sunDisc = Microsoft::Xna::Framework::Vector3(1.0f, 1.0f, 0.95f);
        /// How visible the stars are, 0..1.
        float stars = 0.0f;
    };

    /// Sky and light colours by the sun's elevation, interpolated between
    /// keys: the night-to-day progression car- and train-simulator mix from
    /// night, dusk and day colours, school's and museum's sky domes, house's
    /// zenith and horizon table by elevation.
    class DayNightPalette
    {
    public:
        struct Key
        {
            float elevationDegrees = 0.0f;
            SkyColors colors;
        };

        /// Night below -18 degrees, twilight, sunrise at 0, golden light at
        /// 10, day from 30.
        [[nodiscard]] static DayNightPalette Default();
        /// Throws std::invalid_argument without keys or when they are not in
        /// rising elevation.
        explicit DayNightPalette(std::vector<Key> keys);

        /// The colours at an elevation (clamped to the first and last key).
        /// `overcast` (0..1) greys the sky, dims the sun and hides its disc,
        /// glow and the stars.
        [[nodiscard]] SkyColors At(float sunElevationDegrees, float overcast = 0.0f) const;
        [[nodiscard]] const std::vector<Key>& Keys() const noexcept { return keys_; }

    private:
        std::vector<Key> keys_;
    };

    /// A clock for the sun: solar time, day of the year and latitude through
    /// SunPosition.
    struct TimeOfDay
    {
        double hours = 12.0;
        double dayOfYear = 172.0;
        double latitudeDegrees = 50.0;
        double solarNoonHours = 12.0;

        [[nodiscard]] SunAngles Sun() const noexcept;
        /// Towards the sun (north along -Z, east along +X).
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 DirectionToSun() const noexcept;
        /// Moves the clock by `seconds` of play at `timeScale` game seconds
        /// per real one, wrapping at midnight.
        void Advance(double seconds, double timeScale = 1.0) noexcept;
    };

    /// Lights the forward renderer for a sky: the key light from the sun
    /// (from the opposite side, as a moon, while the sun is below the
    /// horizon), the sky fill from above, the ambient colour, and fog in the
    /// horizon's colour from `fogStart` to `fogEnd` (none without them).
    void ApplySkyLighting(ForwardLighting& lighting, const SkyColors& colors,
                          const Microsoft::Xna::Framework::Vector3& directionToSun,
                          std::optional<float> fogStart = std::nullopt, std::optional<float> fogEnd = std::nullopt);

    /// A vertex-coloured sky dome drawn around the camera before everything
    /// else: a gradient from the horizon to the zenith, the ground's colour
    /// below the horizon (closed underneath), a glow around the sun, the
    /// sun's disc and stars that fade in at night. XNA's BasicEffect only, so strict games can use it.
    class SkyDome
    {
    public:
        /// Throws std::invalid_argument for fewer than 8 segments or 3 rings.
        explicit SkyDome(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, int segments = 32, int rings = 12,
                         int stars = 300, std::uint32_t seed = 1);

        /// Recolours the dome for a moment.
        void SetColors(const SkyColors& colors, const Microsoft::Xna::Framework::Vector3& directionToSun);
        /// Draws centred on the camera without writing depth, so anything
        /// drawn after it is in front. The dome's radius stays inside the
        /// camera's far plane.
        void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const Camera& camera,
                  RenderStatistics* statistics = nullptr);
        /// Draws as `renderer`'s first custom pass (PassPoint::BeforeOpaque).
        void AttachTo(ForwardRenderer& renderer, std::string passName = "sky");

    private:
        // Unit directions of the dome's triangles (three per triangle), the
        // stars' corners (six per star) and their brightness.
        std::vector<Microsoft::Xna::Framework::Vector3> dome_;
        std::vector<Microsoft::Xna::Framework::Vector3> stars_;
        std::vector<float> starBrightness_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor> domeVertices_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor> discVertices_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor> starVertices_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
        float starVisibility_ = 0.0f;
    };
}
