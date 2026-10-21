// SPDX-License-Identifier: MIT
//
// Core/SunPosition.hpp against the games' own solar geometry, compiled
// read-only. The games compute in single precision (car, room) or with an
// ephemeris for the declination (house), so the comparison is within a
// tolerance rather than bit for bit. A game not checked out beside this
// repository leaves out its test.
#include "CnaEngine/Core/SunPosition.hpp"

#if defined(CNA_ENGINE_PROBE_CAR_SIMULATOR)
#include "CarSim/Render/LightingRig.hpp"
#endif
#if defined(CNA_ENGINE_PROBE_ROOM_SIMULATOR)
#include "CnaRoom/Sim/TimeOfDay.hpp"
#endif
#if defined(CNA_ENGINE_PROBE_HOUSE_SIMULATOR)
#include "cnahouse/environment/SunModel.hpp"
#endif

#include <gtest/gtest.h>

#include <cmath>

namespace
{
    // Azimuths 359.9 and 0.1 are 0.2 degrees apart.
    double AzimuthDifference(const double a, const double b)
    {
        const double difference = std::fmod(std::fabs(a - b), 360.0);
        return std::min(difference, 360.0 - difference);
    }
}

#if defined(CNA_ENGINE_PROBE_CAR_SIMULATOR)
TEST(SunEquivalenceTests, CarSimulatorLightingRig)
{
    // car-simulator: a fixed declination and solar noon at 13:00 (summer time).
    for (const float latitude : {49.8f, 0.0f, -33.9f, 64.1f}) {
        for (const float declination : {20.0f, 0.0f, -23.4f}) {
            for (float hours = 0.0f; hours < 24.0f; hours += 0.37f) {
                CarSim::Render::LightingRig rig;
                rig.latitudeDeg = latitude;
                rig.sunDeclinationDeg = declination;
                rig.SetTimeOfDay(hours);
                const CnaEngine::SunAngles engine =
                    CnaEngine::SunAnglesFor(latitude, declination, CnaEngine::HourAngleDegrees(hours, 13.0));
                ASSERT_NEAR(engine.elevationDegrees, rig.sunElevationDeg, 2e-3)
                    << latitude << " " << declination << " " << hours;
                // Before 01:00 the rig's unwrapped hour angle mirrors its azimuth
                // about north (upstream-issues.md, UP-016).
                const double expectedAzimuth = hours < 1.0f ? 360.0 - engine.azimuthDegrees : engine.azimuthDegrees;
                if (std::fabs(engine.elevationDegrees) < 89.9) {
                    ASSERT_LT(AzimuthDifference(expectedAzimuth, rig.sunAzimuthDeg), 2e-2)
                        << latitude << " " << declination << " " << hours;
                }
                if (hours >= 1.0f && rig.sunElevationDeg >= -0.5f) { // below -0.5 the key light is the moon
                    const auto toSun = CnaEngine::DirectionToSun(engine);
                    ASSERT_NEAR(-toSun.X, rig.sunDirection.X, 1e-4f);
                    ASSERT_NEAR(-toSun.Y, rig.sunDirection.Y, 1e-4f);
                    ASSERT_NEAR(-toSun.Z, rig.sunDirection.Z, 1e-4f);
                }
            }
        }
    }
}
#endif

#if defined(CNA_ENGINE_PROBE_ROOM_SIMULATOR)
TEST(SunEquivalenceTests, RoomSimulatorTimeOfDay)
{
    // room-simulator: Cooper's declination by day of year, solar noon at 12:00,
    // north along +Z.
    for (const float latitude : {50.1f, 0.0f, -41.3f, 70.0f}) {
        for (const int day : {1, 81, 172, 266, 355}) {
            for (float hours = 0.0f; hours < 24.0f; hours += 0.41f) {
                CnaRoom::TimeOfDay room;
                room.setLatitudeDegrees(latitude);
                room.setDayOfYear(day);
                room.setHours(hours);
                const CnaEngine::SunAngles engine = CnaEngine::SunAnglesAt(latitude, day, hours);
                ASSERT_NEAR(engine.elevationDegrees, room.sunElevationDegrees(), 2e-3)
                    << latitude << " " << day << " " << hours;
                if (std::fabs(engine.elevationDegrees) < 89.9) {
                    ASSERT_LT(AzimuthDifference(engine.azimuthDegrees, room.sunAzimuthDegrees()), 2e-2)
                        << latitude << " " << day << " " << hours;
                }
                const auto toSun = CnaEngine::DirectionToSun(engine);
                const auto roomToSun = room.sunDirection();
                ASSERT_NEAR(toSun.X, roomToSun.X, 1e-4f);
                ASSERT_NEAR(toSun.Y, roomToSun.Y, 1e-4f);
                ASSERT_NEAR(-toSun.Z, roomToSun.Z, 1e-4f) << "room's north is +Z";
            }
        }
    }
}
#endif

#if defined(CNA_ENGINE_PROBE_HOUSE_SIMULATOR)
TEST(SunEquivalenceTests, HouseSimulatorEphemeris)
{
    // house-simulator: declination and hour angle from an ephemeris; given
    // the same two, the spherical triangle must agree in double precision.
    for (const auto& observer : {cnahouse::environment::SunObserver{}, cnahouse::environment::ReykjavikObserver(),
                                 cnahouse::environment::EquatorObserver()}) {
        for (double day = -3000.0; day < 12000.0; day += 13.37) {
            const auto house = cnahouse::environment::SunPositionAt(day, observer);
            const CnaEngine::SunAngles engine =
                CnaEngine::SunAnglesFor(observer.latitudeDeg, house.declinationDeg, house.hourAngleDeg);
            ASSERT_NEAR(engine.elevationDegrees, house.altitudeDeg, 1e-9) << day;
            ASSERT_LT(AzimuthDifference(engine.azimuthDegrees, house.azimuthDeg), 1e-9) << day;
        }
    }
    // Cooper's declination stays within about a degree of the ephemeris.
    for (double day = 0.0; day < 365.0; day += 1.0) {
        const auto house = cnahouse::environment::SunPositionAt(day, {});
        // Day 0 since J2000 is 1 January 2000 at noon: day of year day + 1.5.
        EXPECT_NEAR(CnaEngine::SolarDeclinationDegrees(day + 1.5), house.declinationDeg, 1.5) << day;
    }
}
#endif
