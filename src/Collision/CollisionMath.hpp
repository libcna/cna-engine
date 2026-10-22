// SPDX-License-Identifier: MIT
//
// Closest-point routines of the collision module (private). The segment and
// triangle routines follow Ericson, Real-Time Collision Detection (2005),
// sections 5.1.5 and 5.1.9.
#pragma once

#include "CnaEngine/Collision/StaticWorld.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace CnaEngine::Collision::Detail
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    inline float Dot(const Vector3& a, const Vector3& b) noexcept { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }
    inline Vector3 Cross(const Vector3& a, const Vector3& b) noexcept
    {
        return Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
    }
    inline float LengthSquared(const Vector3& v) noexcept { return Dot(v, v); }
    inline float Length(const Vector3& v) noexcept { return std::sqrt(Dot(v, v)); }
    inline Vector3 Normalized(const Vector3& v) noexcept
    {
        const float length = Length(v);
        return length > 1e-12f ? v * (1.0f / length) : Vector3::Zero;
    }

    inline Vector3 ClosestPointOnSegment(const Vector3& point, const Vector3& a, const Vector3& b) noexcept
    {
        const Vector3 ab = b - a;
        const float lengthSquared = Dot(ab, ab);
        if (lengthSquared < 1e-12f) {
            return a;
        }
        const float t = std::clamp(Dot(point - a, ab) / lengthSquared, 0.0f, 1.0f);
        return a + ab * t;
    }

    inline Vector3 Clamp(const Vector3& p, const BoundingBox& box) noexcept
    {
        return Vector3(std::clamp(p.X, box.Min.X, box.Max.X), std::clamp(p.Y, box.Min.Y, box.Max.Y),
                       std::clamp(p.Z, box.Min.Z, box.Max.Z));
    }

    /// Closest points of segment [a, b] and a box, by alternating projection
    /// (both sets are convex); exact within a few iterations for the upright
    /// segments of capsules. Equal points: the segment touches the box.
    inline void ClosestSegmentBox(const Vector3& a, const Vector3& b, const BoundingBox& box, Vector3& onSegment,
                                  Vector3& onBox) noexcept
    {
        Vector3 p = (a + b) * 0.5f;
        for (int i = 0; i < 32; ++i) {
            const Vector3 q = Clamp(p, box);
            const Vector3 next = ClosestPointOnSegment(q, a, b);
            const bool settled = LengthSquared(next - p) < 1e-14f;
            p = next;
            if (settled) {
                break;
            }
        }
        onSegment = p;
        onBox = Clamp(p, box);
    }

    /// The shortest way out of a box for an upright capsule whose axis is in
    /// it: the least overlap of the capsule's bounds with the box, per side.
    inline std::optional<Contact> BoxEscape(const Vector3& bottom, const Vector3& top, float radius,
                                            const BoundingBox& box) noexcept
    {
        const float minX = bottom.X - radius;
        const float maxX = bottom.X + radius;
        const float minY = bottom.Y - radius;
        const float maxY = top.Y + radius;
        const float minZ = bottom.Z - radius;
        const float maxZ = bottom.Z + radius;
        const struct
        {
            float depth;
            Vector3 normal;
        } ways[6] = {
            {box.Max.X - minX, Vector3(1.0f, 0.0f, 0.0f)},  {maxX - box.Min.X, Vector3(-1.0f, 0.0f, 0.0f)},
            {box.Max.Y - minY, Vector3(0.0f, 1.0f, 0.0f)},  {maxY - box.Min.Y, Vector3(0.0f, -1.0f, 0.0f)},
            {box.Max.Z - minZ, Vector3(0.0f, 0.0f, 1.0f)},  {maxZ - box.Min.Z, Vector3(0.0f, 0.0f, -1.0f)},
        };
        int best = -1;
        for (int i = 0; i < 6; ++i) {
            if (ways[i].depth > 0.0f && (best < 0 || ways[i].depth < ways[best].depth)) {
                best = i;
            }
        }
        if (best < 0) {
            return std::nullopt;
        }
        Contact contact;
        contact.normal = ways[best].normal;
        contact.depth = ways[best].depth;
        contact.point = Clamp(bottom, box);
        return contact;
    }

    inline void ClosestSegmentSegment(const Vector3& p1, const Vector3& q1, const Vector3& p2, const Vector3& q2,
                                      Vector3& c1, Vector3& c2) noexcept
    {
        constexpr float epsilon = 1e-12f;
        const Vector3 d1 = q1 - p1;
        const Vector3 d2 = q2 - p2;
        const Vector3 r = p1 - p2;
        const float a = Dot(d1, d1);
        const float e = Dot(d2, d2);
        const float f = Dot(d2, r);
        float s = 0.0f;
        float t = 0.0f;
        if (a <= epsilon && e <= epsilon) {
            // both are points
        } else if (a <= epsilon) {
            t = std::clamp(f / e, 0.0f, 1.0f);
        } else {
            const float c = Dot(d1, r);
            if (e <= epsilon) {
                s = std::clamp(-c / a, 0.0f, 1.0f);
            } else {
                const float b = Dot(d1, d2);
                const float denominator = a * e - b * b;
                s = denominator != 0.0f ? std::clamp((b * f - c * e) / denominator, 0.0f, 1.0f) : 0.0f;
                t = (b * s + f) / e;
                if (t < 0.0f) {
                    t = 0.0f;
                    s = std::clamp(-c / a, 0.0f, 1.0f);
                } else if (t > 1.0f) {
                    t = 1.0f;
                    s = std::clamp((b - c) / a, 0.0f, 1.0f);
                }
            }
        }
        c1 = p1 + d1 * s;
        c2 = p2 + d2 * t;
    }

    inline Vector3 ClosestPointOnTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c) noexcept
    {
        const Vector3 ab = b - a;
        const Vector3 ac = c - a;
        const Vector3 ap = p - a;
        const float d1 = Dot(ab, ap);
        const float d2 = Dot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) {
            return a;
        }
        const Vector3 bp = p - b;
        const float d3 = Dot(ab, bp);
        const float d4 = Dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) {
            return b;
        }
        const float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
            return a + ab * (d1 / (d1 - d3));
        }
        const Vector3 cp = p - c;
        const float d5 = Dot(ab, cp);
        const float d6 = Dot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) {
            return c;
        }
        const float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
            return a + ac * (d2 / (d2 - d6));
        }
        const float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
            return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
        }
        const float denominator = 1.0f / (va + vb + vc);
        return a + ab * (vb * denominator) + ac * (vc * denominator);
    }

    /// Closest points of segment [p, q] and triangle abc: the crossing when
    /// the segment passes through it, else the nearest of the endpoints'
    /// projections and the three edges.
    inline void ClosestSegmentTriangle(const Vector3& p, const Vector3& q, const Vector3& a, const Vector3& b,
                                       const Vector3& c, Vector3& onSegment, Vector3& onTriangle) noexcept
    {
        // Does the segment cross the triangle?
        const Vector3 direction = q - p;
        const Vector3 ab = b - a;
        const Vector3 ac = c - a;
        const Vector3 h = Cross(direction, ac);
        const float determinant = Dot(ab, h);
        if (std::fabs(determinant) > 1e-12f) {
            const float inverse = 1.0f / determinant;
            const Vector3 s = p - a;
            const float u = Dot(s, h) * inverse;
            const Vector3 k = Cross(s, ab);
            const float v = Dot(direction, k) * inverse;
            const float t = Dot(ac, k) * inverse;
            if (u >= 0.0f && v >= 0.0f && u + v <= 1.0f && t >= 0.0f && t <= 1.0f) {
                onSegment = p + direction * t;
                onTriangle = onSegment;
                return;
            }
        }
        float best = std::numeric_limits<float>::infinity();
        auto consider = [&](const Vector3& segmentPoint, const Vector3& trianglePoint) {
            const float distance = LengthSquared(segmentPoint - trianglePoint);
            if (distance < best) {
                best = distance;
                onSegment = segmentPoint;
                onTriangle = trianglePoint;
            }
        };
        consider(p, ClosestPointOnTriangle(p, a, b, c));
        consider(q, ClosestPointOnTriangle(q, a, b, c));
        const Vector3 corners[3] = {a, b, c};
        for (int i = 0; i < 3; ++i) {
            Vector3 s;
            Vector3 t;
            ClosestSegmentSegment(p, q, corners[i], corners[(i + 1) % 3], s, t);
            consider(s, t);
        }
    }

    /// Closest points of an upright segment [bottom, top] and a solid vertical
    /// cylinder; equal points when the segment is inside it.
    inline void ClosestSegmentCylinder(const Vector3& bottom, const Vector3& top, const Vector3& base, float radius,
                                       float height, Vector3& onSegment, Vector3& onCylinder) noexcept
    {
        const float low = std::max(bottom.Y, base.Y);
        const float high = std::min(top.Y, base.Y + height);
        float y;
        float cylinderY;
        if (low <= high) {
            y = (low + high) * 0.5f;
            cylinderY = y;
        } else if (bottom.Y > base.Y + height) {
            y = bottom.Y;
            cylinderY = base.Y + height;
        } else {
            y = top.Y;
            cylinderY = base.Y;
        }
        onSegment = Vector3(bottom.X, y, bottom.Z);
        float dx = bottom.X - base.X;
        float dz = bottom.Z - base.Z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance > radius) {
            dx *= radius / distance;
            dz *= radius / distance;
        }
        onCylinder = Vector3(base.X + dx, cylinderY, base.Z + dz);
    }

    /// The shortest way out of a cylinder for an upright capsule whose axis
    /// is in it: sideways, up onto its top or down below it.
    inline std::optional<Contact> CylinderEscape(const Vector3& bottom, const Vector3& top, float radius,
                                                 const Vector3& base, float cylinderRadius, float height) noexcept
    {
        const float dx = bottom.X - base.X;
        const float dz = bottom.Z - base.Z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        Contact contact;
        contact.normal = distance > 1e-6f ? Vector3(dx / distance, 0.0f, dz / distance) : Vector3(1.0f, 0.0f, 0.0f);
        contact.depth = cylinderRadius + radius - distance;
        const float up = base.Y + height - (bottom.Y - radius);
        const float down = top.Y + radius - base.Y;
        if (up > 0.0f && up < contact.depth) {
            contact.normal = Vector3::Up;
            contact.depth = up;
        }
        if (down > 0.0f && down < contact.depth) {
            contact.normal = Vector3(0.0f, -1.0f, 0.0f);
            contact.depth = down;
        }
        if (!(contact.depth > 0.0f)) {
            return std::nullopt;
        }
        contact.point = Vector3(bottom.X, std::clamp(bottom.Y, base.Y, base.Y + height), bottom.Z);
        return contact;
    }
}
