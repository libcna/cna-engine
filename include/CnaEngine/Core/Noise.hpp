// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

/// Hash-based, tileable noise for procedural textures and terrain.
///
/// Hash-based rather than table-based: there is no permutation table and no
/// shared state, so generators can run on several threads without any
/// coordination, and a result depends only on the coordinates and the seed --
/// the same texture byte for byte in a debug build, a release build and on
/// another machine. Every lattice function is tileable on the `period` it is
/// given (a period of 0 or less does not wrap). Header-only so that texture
/// loops inline it.
///
/// The functions and their arithmetic are street-simulator's
/// (`CnaStreet::Noise`), which it builds its city's surfaces from; the pilot
/// probe compares them bit for bit.
namespace CnaEngine::Noise
{
    /// 32-bit integer hash (a variant of Wang/Jenkins mixing).
    [[nodiscard]] inline std::uint32_t Hash(std::uint32_t x) noexcept
    {
        x ^= x >> 16;
        x *= 0x7FEB352Du;
        x ^= x >> 15;
        x *= 0x846CA68Bu;
        x ^= x >> 16;
        return x;
    }

    [[nodiscard]] inline std::uint32_t Hash2(const int x, const int y, const std::uint32_t seed) noexcept
    {
        return Hash(static_cast<std::uint32_t>(x) * 0x27D4EB2Du ^ Hash(static_cast<std::uint32_t>(y) * 0x165667B1u ^ seed));
    }

    [[nodiscard]] inline std::uint32_t Hash3(const int x, const int y, const int z, const std::uint32_t seed) noexcept
    {
        return Hash(Hash2(x, y, seed) ^ (static_cast<std::uint32_t>(z) * 0x9E3779B9u));
    }

    /// Uniform in [0, 1) from a lattice cell.
    [[nodiscard]] inline float Random2(const int x, const int y, const std::uint32_t seed) noexcept
    {
        return static_cast<float>(Hash2(x, y, seed)) * (1.0f / 4294967296.0f);
    }

    [[nodiscard]] inline float Random3(const int x, const int y, const int z, const std::uint32_t seed) noexcept
    {
        return static_cast<float>(Hash3(x, y, z, seed)) * (1.0f / 4294967296.0f);
    }

    /// Quintic smoothstep (C2 continuous: normal maps derived from the noise
    /// show no lattice creases).
    [[nodiscard]] inline float Fade(const float t) noexcept { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

    [[nodiscard]] inline float Lerp(const float a, const float b, const float t) noexcept { return a + (b - a) * t; }

    /// Positive modulo, so a negative coordinate still lands in the tile;
    /// a period of 0 or less leaves `v` alone.
    [[nodiscard]] inline int Wrap(const int v, const int period) noexcept
    {
        if (period <= 0) {
            return v;
        }
        const int m = v % period;
        return m < 0 ? m + period : m;
    }

    /// Tileable 2-D value noise on an integer lattice, in [0, 1).
    [[nodiscard]] inline float Value2(const float x, const float y, const int period, const std::uint32_t seed) noexcept
    {
        const int xi = static_cast<int>(std::floor(x));
        const int yi = static_cast<int>(std::floor(y));
        const float xf = x - static_cast<float>(xi);
        const float yf = y - static_cast<float>(yi);
        const int x0 = Wrap(xi, period);
        const int x1 = Wrap(xi + 1, period);
        const int y0 = Wrap(yi, period);
        const int y1 = Wrap(yi + 1, period);
        const float u = Fade(xf);
        const float v = Fade(yf);
        return Lerp(Lerp(Random2(x0, y0, seed), Random2(x1, y0, seed), u),
                    Lerp(Random2(x0, y1, seed), Random2(x1, y1, seed), u), v);
    }

    /// Tileable gradient (Perlin-style) noise, mapped to about [0, 1]:
    /// sharper than value noise and without its axis-aligned bias.
    [[nodiscard]] inline float Gradient2(const float x, const float y, const int period, const std::uint32_t seed)
    {
        const int xi = static_cast<int>(std::floor(x));
        const int yi = static_cast<int>(std::floor(y));
        const float xf = x - static_cast<float>(xi);
        const float yf = y - static_cast<float>(yi);
        const auto dot = [&](const int cx, const int cy, const float dx, const float dy) {
            const float angle = Random2(Wrap(cx, period), Wrap(cy, period), seed) * 6.28318530718f;
            return std::cos(angle) * dx + std::sin(angle) * dy;
        };
        const float u = Fade(xf);
        const float v = Fade(yf);
        const float n00 = dot(xi, yi, xf, yf);
        const float n10 = dot(xi + 1, yi, xf - 1.0f, yf);
        const float n01 = dot(xi, yi + 1, xf, yf - 1.0f);
        const float n11 = dot(xi + 1, yi + 1, xf - 1.0f, yf - 1.0f);
        return Lerp(Lerp(n00, n10, u), Lerp(n01, n11, u), v) * 0.5f + 0.5f;
    }

    /// Fractional Brownian motion over Value2 (dirt, staining, mottling,
    /// cloud cover), normalised by the summed amplitudes. Each octave's period
    /// is the previous one times `lacunarity`, truncated.
    [[nodiscard]] inline float Fbm(const float x, const float y, const int period, const int octaves,
                                   const float lacunarity, const float gain, const std::uint32_t seed) noexcept
    {
        float sum = 0.0f;
        float amplitude = 1.0f;
        float total = 0.0f;
        float frequency = 1.0f;
        int octavePeriod = period;
        for (int i = 0; i < octaves; ++i) {
            sum += Value2(x * frequency, y * frequency, octavePeriod, seed + static_cast<std::uint32_t>(i) * 7919u)
                   * amplitude;
            total += amplitude;
            amplitude *= gain;
            frequency *= lacunarity;
            octavePeriod = static_cast<int>(static_cast<float>(octavePeriod) * lacunarity);
        }
        return total > 0.0f ? sum / total : 0.0f;
    }

    /// Ridged multifractal over Gradient2 (cracks, bark): halving amplitude,
    /// doubling frequency and period.
    [[nodiscard]] inline float Ridged(const float x, const float y, const int period, const int octaves,
                                      const std::uint32_t seed)
    {
        float sum = 0.0f;
        float amplitude = 1.0f;
        float total = 0.0f;
        float frequency = 1.0f;
        int octavePeriod = period;
        for (int i = 0; i < octaves; ++i) {
            const float n =
                Gradient2(x * frequency, y * frequency, octavePeriod, seed + static_cast<std::uint32_t>(i) * 6151u);
            const float r = 1.0f - std::fabs(n * 2.0f - 1.0f);
            sum += r * r * amplitude;
            total += amplitude;
            amplitude *= 0.5f;
            frequency *= 2.0f;
            octavePeriod *= 2;
        }
        return total > 0.0f ? sum / total : 0.0f;
    }

    /// Worley (cellular) noise: the distance to the nearest feature point
    /// (F1), and through `secondOut` to the second nearest (F2) -- F2 - F1
    /// draws the joints between cobbles, paving slabs and cracked mud.
    [[nodiscard]] inline float Worley2(const float x, const float y, const int period, const std::uint32_t seed,
                                       float* secondOut = nullptr)
    {
        const int xi = static_cast<int>(std::floor(x));
        const int yi = static_cast<int>(std::floor(y));
        float best = 1e9f;
        float second = 1e9f;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int cx = xi + dx;
                const int cy = yi + dy;
                const std::uint32_t h = Hash2(Wrap(cx, period), Wrap(cy, period), seed);
                const float px = static_cast<float>(cx) + static_cast<float>(h & 0xFFFFu) * (1.0f / 65536.0f);
                const float py = static_cast<float>(cy) + static_cast<float>((h >> 16) & 0xFFFFu) * (1.0f / 65536.0f);
                const float ddx = px - x;
                const float ddy = py - y;
                const float d = std::sqrt(ddx * ddx + ddy * ddy);
                if (d < best) {
                    second = best;
                    best = d;
                } else if (d < second) {
                    second = d;
                }
            }
        }
        if (secondOut != nullptr) {
            *secondOut = second;
        }
        return best;
    }

    [[nodiscard]] inline float Saturate(const float v) noexcept { return std::clamp(v, 0.0f, 1.0f); }

    /// Hermite interpolation between two edges, in either direction: a
    /// descending pair is how "1 where the value is small" is written, so it
    /// ramps instead of stepping (GLSL leaves edge0 >= edge1 undefined).
    [[nodiscard]] inline float SmoothStep(const float edge0, const float edge1, const float x) noexcept
    {
        if (edge0 == edge1) {
            return x < edge0 ? 0.0f : 1.0f;
        }
        const float t = Saturate((x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }
}
