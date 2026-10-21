// SPDX-License-Identifier: MIT
#include "CnaEngine/Core/SunPosition.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace CnaEngine
{
    TEST(SunPositionTests, DeclinationFollowsTheSeasons)
    {
        EXPECT_DOUBLE_EQ(SolarDeclinationDegrees(81.0), 0.0) << "the March equinox";
        EXPECT_NEAR(SolarDeclinationDegrees(172.0), 23.44, 0.01) << "the June solstice";
        EXPECT_NEAR(SolarDeclinationDegrees(355.0), -23.44, 0.01) << "the December solstice";
        EXPECT_DOUBLE_EQ(HourAngleDegrees(13.5, 13.0), 7.5);
        EXPECT_DOUBLE_EQ(HourAngleDegrees(6.0), -90.0);
    }

    TEST(SunPositionTests, NoonIsSouthAndMorningEast)
    {
        const SunAngles noon = SunAnglesFor(50.0, 0.0, 0.0);
        EXPECT_NEAR(noon.elevationDegrees, 40.0, 1e-12);
        EXPECT_NEAR(noon.azimuthDegrees, 180.0, 1e-12);
        const SunAngles morning = SunAnglesFor(0.0, 0.0, -90.0);
        EXPECT_NEAR(morning.elevationDegrees, 0.0, 1e-12);
        EXPECT_NEAR(morning.azimuthDegrees, 90.0, 1e-12);
        EXPECT_NEAR(SunAnglesFor(0.0, 0.0, 90.0).azimuthDegrees, 270.0, 1e-12);
        EXPECT_NEAR(SunAnglesFor(-35.0, -20.0, 0.0).azimuthDegrees, 0.0, 1e-12) << "southern summer: north";
        const SunAngles midnight = SunAnglesAt(49.8, 172.0, 0.0);
        EXPECT_LT(midnight.elevationDegrees, 0.0);
        const SunAngles pole = SunAnglesFor(90.0, 90.0, 33.0);
        EXPECT_FALSE(std::isnan(pole.elevationDegrees));
        EXPECT_FALSE(std::isnan(pole.azimuthDegrees));
    }

    TEST(SunPositionTests, DirectionsUseXnaAxes)
    {
        const auto east = DirectionToSun({0.0, 90.0});
        EXPECT_NEAR(east.X, 1.0f, 1e-6f);
        EXPECT_NEAR(east.Z, 0.0f, 1e-6f);
        const auto north = DirectionToSun({0.0, 0.0});
        EXPECT_NEAR(north.Z, -1.0f, 1e-6f) << "north is XNA's Forward";
        const auto south = DirectionToSun({45.0, 180.0});
        EXPECT_NEAR(south.Y, std::sqrt(0.5f), 1e-6f);
        EXPECT_NEAR(south.Z, std::sqrt(0.5f), 1e-6f);
    }
}
