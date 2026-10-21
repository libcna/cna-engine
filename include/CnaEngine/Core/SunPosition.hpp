// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace CnaEngine
{
    /// Where the sun stands in the sky. Degrees throughout.
    struct SunAngles
    {
        /// Above the true horizon (geometric: no refraction), -90..90.
        double elevationDegrees = 0.0;
        /// From north, clockwise through east, [0, 360).
        double azimuthDegrees = 0.0;
    };

    /// The sun's declination on a day of the year (1 = 1 January), Cooper's
    /// approximation 23.44 * sin(360 / 365 * (day - 81)): within about a
    /// degree of the ephemeris, plenty for lighting a scene.
    [[nodiscard]] double SolarDeclinationDegrees(double dayOfYear) noexcept;

    /// The hour angle of a local solar time, 15 degrees an hour from
    /// `solarNoonHours`: negative in the morning, positive in the afternoon. A
    /// clock running on summer time has its solar noon near 13:00.
    [[nodiscard]] double HourAngleDegrees(double solarTimeHours, double solarNoonHours = 12.0) noexcept;

    /// The sun's elevation and azimuth seen from `latitudeDegrees` (north
    /// positive) for a declination and an hour angle: the spherical triangle,
    /// in double precision, with the azimuth written as
    /// atan2(-cos d sin H, cos l sin d - sin l cos d cos H), which has no pole
    /// at a declination of +-90 degrees.
    [[nodiscard]] SunAngles SunAnglesFor(double latitudeDegrees, double declinationDegrees,
                                         double hourAngleDegrees) noexcept;

    /// SunAnglesFor with SolarDeclinationDegrees and HourAngleDegrees.
    [[nodiscard]] SunAngles SunAnglesAt(double latitudeDegrees, double dayOfYear, double solarTimeHours,
                                        double solarNoonHours = 12.0) noexcept;

    /// The unit vector towards the sun in XNA's axes with north along -Z
    /// (Vector3::Forward), east along +X and up along +Y. Its negation is the
    /// direction a DirectionalLight shines in.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 DirectionToSun(const SunAngles& angles) noexcept;
}
