// SPDX-License-Identifier: MIT
#include "CnaEngine/Core/SunPosition.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace CnaEngine
{
    namespace
    {
        constexpr double DegreesToRadians = std::numbers::pi / 180.0;
        constexpr double RadiansToDegrees = 180.0 / std::numbers::pi;
    }

    double SolarDeclinationDegrees(const double dayOfYear) noexcept
    {
        return 23.44 * std::sin(2.0 * std::numbers::pi * (dayOfYear - 81.0) / 365.0);
    }

    double HourAngleDegrees(const double solarTimeHours, const double solarNoonHours) noexcept
    {
        return 15.0 * (solarTimeHours - solarNoonHours);
    }

    SunAngles SunAnglesFor(const double latitudeDegrees, const double declinationDegrees,
                           const double hourAngleDegrees) noexcept
    {
        const double latitude = latitudeDegrees * DegreesToRadians;
        const double declination = declinationDegrees * DegreesToRadians;
        const double hourAngle = hourAngleDegrees * DegreesToRadians;
        const double sinLatitude = std::sin(latitude);
        const double cosLatitude = std::cos(latitude);
        const double sinDeclination = std::sin(declination);
        const double cosDeclination = std::cos(declination);
        const double sinHourAngle = std::sin(hourAngle);
        const double cosHourAngle = std::cos(hourAngle);
        // An ulp outside [-1, 1] would make asin a NaN.
        const double sinElevation =
            std::clamp(sinLatitude * sinDeclination + cosLatitude * cosDeclination * cosHourAngle, -1.0, 1.0);
        double azimuth = std::atan2(-cosDeclination * sinHourAngle,
                                    cosLatitude * sinDeclination - sinLatitude * cosDeclination * cosHourAngle)
                         * RadiansToDegrees;
        azimuth = std::fmod(azimuth, 360.0);
        if (azimuth < 0.0) {
            azimuth += 360.0;
        }
        if (azimuth >= 360.0) {
            azimuth -= 360.0;
        }
        return SunAngles{std::asin(sinElevation) * RadiansToDegrees, azimuth};
    }

    SunAngles SunAnglesAt(const double latitudeDegrees, const double dayOfYear, const double solarTimeHours,
                          const double solarNoonHours) noexcept
    {
        return SunAnglesFor(latitudeDegrees, SolarDeclinationDegrees(dayOfYear),
                            HourAngleDegrees(solarTimeHours, solarNoonHours));
    }

    Microsoft::Xna::Framework::Vector3 DirectionToSun(const SunAngles& angles) noexcept
    {
        const double elevation = angles.elevationDegrees * DegreesToRadians;
        const double azimuth = angles.azimuthDegrees * DegreesToRadians;
        const double horizontal = std::cos(elevation);
        return Microsoft::Xna::Framework::Vector3(static_cast<float>(std::sin(azimuth) * horizontal),
                                                  static_cast<float>(std::sin(elevation)),
                                                  static_cast<float>(-std::cos(azimuth) * horizontal));
    }
}
