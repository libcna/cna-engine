// SPDX-License-Identifier: MIT
#include "CnaEngine/Collision/StaticWorld.hpp"

#include "CollisionMath.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CnaEngine::Collision
{
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Ray;
    using Microsoft::Xna::Framework::Vector3;
    using namespace Detail;

    namespace
    {
        constexpr float Epsilon = 1e-6f;

        bool Finite(const Vector3& v) noexcept
        {
            return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z);
        }

        // Slab test. Inside the box: a hit at 0, facing back along the ray.
        std::optional<std::pair<float, Vector3>> RayBox(const Ray& ray, const BoundingBox& box)
        {
            float enter = 0.0f;
            float exit = std::numeric_limits<float>::infinity();
            Vector3 normal = -Normalized(ray.Direction);
            const float origin[3] = {ray.Position.X, ray.Position.Y, ray.Position.Z};
            const float direction[3] = {ray.Direction.X, ray.Direction.Y, ray.Direction.Z};
            const float low[3] = {box.Min.X, box.Min.Y, box.Min.Z};
            const float high[3] = {box.Max.X, box.Max.Y, box.Max.Z};
            for (int axis = 0; axis < 3; ++axis) {
                if (std::fabs(direction[axis]) < Epsilon) {
                    if (origin[axis] < low[axis] || origin[axis] > high[axis]) {
                        return std::nullopt;
                    }
                    continue;
                }
                const float inverse = 1.0f / direction[axis];
                float near = (low[axis] - origin[axis]) * inverse;
                float far = (high[axis] - origin[axis]) * inverse;
                float sign = -1.0f; // the face at `low` faces -axis
                if (near > far) {
                    std::swap(near, far);
                    sign = 1.0f;
                }
                if (near > enter) {
                    enter = near;
                    normal = Vector3::Zero;
                    (axis == 0 ? normal.X : axis == 1 ? normal.Y : normal.Z) = sign;
                }
                exit = std::min(exit, far);
                if (enter > exit) {
                    return std::nullopt;
                }
            }
            return std::pair{enter, normal};
        }

        // Moller-Trumbore, two-sided; nothing for a ray in the triangle's plane.
        std::optional<float> RayTriangle(const Ray& ray, const Vector3& a, const Vector3& b, const Vector3& c)
        {
            const Vector3 ab = b - a;
            const Vector3 ac = c - a;
            const Vector3 p = Cross(ray.Direction, ac);
            const float determinant = Dot(ab, p);
            if (std::fabs(determinant) < 1e-9f) {
                return std::nullopt;
            }
            const float inverse = 1.0f / determinant;
            const Vector3 s = ray.Position - a;
            const float u = Dot(s, p) * inverse;
            if (u < 0.0f || u > 1.0f) {
                return std::nullopt;
            }
            const Vector3 q = Cross(s, ab);
            const float v = Dot(ray.Direction, q) * inverse;
            if (v < 0.0f || u + v > 1.0f) {
                return std::nullopt;
            }
            const float t = Dot(ac, q) * inverse;
            if (t < 0.0f) {
                return std::nullopt;
            }
            return t;
        }

        // A solid vertical cylinder: side and both caps.
        std::optional<std::pair<float, Vector3>> RayCylinder(const Ray& ray, const Vector3& base, float radius,
                                                             float height)
        {
            const float top = base.Y + height;
            const float ox = ray.Position.X - base.X;
            const float oz = ray.Position.Z - base.Z;
            const float dx = ray.Direction.X;
            const float dz = ray.Direction.Z;
            if (ox * ox + oz * oz <= radius * radius && ray.Position.Y >= base.Y && ray.Position.Y <= top) {
                return std::pair{0.0f, -Normalized(ray.Direction)};
            }
            std::optional<std::pair<float, Vector3>> best;
            auto consider = [&](const float t, const Vector3& normal) {
                if (t >= 0.0f && (!best || t < best->first)) {
                    best = std::pair{t, normal};
                }
            };
            const float a = dx * dx + dz * dz;
            if (a > Epsilon) {
                const float b = 2.0f * (ox * dx + oz * dz);
                const float c = ox * ox + oz * oz - radius * radius;
                const float discriminant = b * b - 4.0f * a * c;
                if (discriminant >= 0.0f) {
                    const float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
                    const float y = ray.Position.Y + t * ray.Direction.Y;
                    if (y >= base.Y && y <= top) {
                        consider(t, Normalized(Vector3(ox + t * dx, 0.0f, oz + t * dz)));
                    }
                }
            }
            if (std::fabs(ray.Direction.Y) > Epsilon) {
                for (const float y : {base.Y, top}) {
                    const float t = (y - ray.Position.Y) / ray.Direction.Y;
                    const float x = ox + t * dx;
                    const float z = oz + t * dz;
                    if (x * x + z * z <= radius * radius) {
                        consider(t, Vector3(0.0f, y == top ? 1.0f : -1.0f, 0.0f));
                    }
                }
            }
            return best;
        }

        // Facing the ray's origin.
        Vector3 Facing(Vector3 normal, const Vector3& direction)
        {
            if (Dot(normal, direction) > 0.0f) {
                normal = -normal;
            }
            return normal;
        }

        // The contact of an upright capsule's segment [bottom, top] with a
        // point `onShape` it is closest to, given the segment point `onSegment`.
        std::optional<Contact> ContactFrom(const Vector3& onSegment, const Vector3& onShape, float radius,
                                           const Vector3& fallbackNormal)
        {
            const Vector3 delta = onSegment - onShape;
            const float distance = Length(delta);
            if (distance >= radius) {
                return std::nullopt;
            }
            Contact contact;
            contact.normal = distance > Epsilon ? delta * (1.0f / distance) : fallbackNormal;
            contact.depth = radius - distance;
            contact.point = onShape;
            return contact;
        }
    }

    Vector3 Capsule::Bottom() const noexcept
    {
        return Vector3(feet.X, feet.Y + radius, feet.Z);
    }

    Vector3 Capsule::Top() const noexcept
    {
        return Vector3(feet.X, feet.Y + std::max(height - radius, radius), feet.Z);
    }

    namespace
    {
        // The cell (column, row) of world (x, z) and the fractions inside it.
        bool Locate(const HeightField& field, float x, float z, int& column, int& row, float& fx, float& fz)
        {
            const float gx = (x - field.origin.X) / field.cellSize;
            const float gz = (z - field.origin.Z) / field.cellSize;
            if (!(gx >= 0.0f && gz >= 0.0f && gx <= static_cast<float>(field.columns - 1) &&
                  gz <= static_cast<float>(field.rows - 1))) {
                return false;
            }
            column = std::min(static_cast<int>(gx), field.columns - 2);
            row = std::min(static_cast<int>(gz), field.rows - 2);
            fx = gx - static_cast<float>(column);
            fz = gz - static_cast<float>(row);
            return true;
        }

        Vector3 Corner(const HeightField& field, int column, int row)
        {
            return Vector3(field.origin.X + static_cast<float>(column) * field.cellSize,
                           field.origin.Y + field.heights[static_cast<std::size_t>(row * field.columns + column)],
                           field.origin.Z + static_cast<float>(row) * field.cellSize);
        }

        // The cell's triangle that holds (fx, fz): split from (0,0) to (1,1).
        void CellTriangle(const HeightField& field, int column, int row, bool upper, Vector3& a, Vector3& b, Vector3& c)
        {
            a = Corner(field, column, row);
            c = Corner(field, column + 1, row + 1);
            b = upper ? Corner(field, column, row + 1) : Corner(field, column + 1, row);
        }
    }

    std::optional<float> HeightField::HeightAt(const float x, const float z) const
    {
        int column = 0;
        int row = 0;
        float fx = 0.0f;
        float fz = 0.0f;
        if (columns < 2 || rows < 2 || !Locate(*this, x, z, column, row, fx, fz)) {
            return std::nullopt;
        }
        const float h00 = heights[static_cast<std::size_t>(row * columns + column)];
        const float h10 = heights[static_cast<std::size_t>(row * columns + column + 1)];
        const float h01 = heights[static_cast<std::size_t>((row + 1) * columns + column)];
        const float h11 = heights[static_cast<std::size_t>((row + 1) * columns + column + 1)];
        const float height = fz > fx ? h00 + fx * (h11 - h01) + fz * (h01 - h00) : h00 + fx * (h10 - h00) + fz * (h11 - h10);
        return origin.Y + height;
    }

    Vector3 HeightField::NormalAt(const float x, const float z) const
    {
        int column = 0;
        int row = 0;
        float fx = 0.0f;
        float fz = 0.0f;
        if (columns < 2 || rows < 2 || !Locate(*this, x, z, column, row, fx, fz)) {
            return Vector3::Up;
        }
        Vector3 a;
        Vector3 b;
        Vector3 c;
        CellTriangle(*this, column, row, fz > fx, a, b, c);
        Vector3 normal = Normalized(Cross(b - a, c - a));
        if (normal.Y < 0.0f) {
            normal = -normal;
        }
        return normal;
    }

    StaticWorld::StaticWorld(const float cellSize) : cellSize_(cellSize)
    {
        if (!(cellSize > 0.0f) || !std::isfinite(cellSize)) {
            throw std::invalid_argument("StaticWorld: the cell size must be positive");
        }
    }

    std::int64_t StaticWorld::CellKey(const int x, const int z) const noexcept
    {
        return (static_cast<std::int64_t>(x) << 32) ^ static_cast<std::int64_t>(static_cast<std::uint32_t>(z));
    }

    int StaticWorld::CellOf(const float coordinate) const noexcept
    {
        return static_cast<int>(std::floor(coordinate / cellSize_));
    }

    ShapeId StaticWorld::Insert(Shape shape)
    {
        shape.alive = true;
        ShapeId id;
        if (!free_.empty()) {
            id = free_.back();
            free_.pop_back();
            shapes_[id] = shape;
        } else {
            id = static_cast<ShapeId>(shapes_.size());
            if (id == TerrainId) {
                throw std::length_error("StaticWorld: too many shapes");
            }
            shapes_.push_back(shape);
        }
        ++alive_;
        for (int x = CellOf(shape.bounds.Min.X); x <= CellOf(shape.bounds.Max.X); ++x) {
            for (int z = CellOf(shape.bounds.Min.Z); z <= CellOf(shape.bounds.Max.Z); ++z) {
                cells_[CellKey(x, z)].push_back(id);
            }
        }
        return id;
    }

    ShapeId StaticWorld::AddBox(const BoundingBox& box, const std::uint32_t tag, const std::uint32_t layers)
    {
        if (!Finite(box.Min) || !Finite(box.Max) || box.Min.X > box.Max.X || box.Min.Y > box.Max.Y ||
            box.Min.Z > box.Max.Z) {
            throw std::invalid_argument("StaticWorld::AddBox: an inverted or non-finite box");
        }
        Shape shape;
        shape.kind = Kind::Box;
        shape.tag = tag;
        shape.layers = layers;
        shape.bounds = box;
        return Insert(shape);
    }

    ShapeId StaticWorld::AddTriangle(const Vector3& a, const Vector3& b, const Vector3& c, const std::uint32_t tag,
                                     const std::uint32_t layers)
    {
        if (!Finite(a) || !Finite(b) || !Finite(c) || LengthSquared(Cross(b - a, c - a)) < 1e-12f) {
            throw std::invalid_argument("StaticWorld::AddTriangle: a degenerate or non-finite triangle");
        }
        Shape shape;
        shape.kind = Kind::Triangle;
        shape.tag = tag;
        shape.layers = layers;
        shape.a = a;
        shape.b = b;
        shape.c = c;
        shape.bounds = BoundingBox(Vector3::Min(Vector3::Min(a, b), c), Vector3::Max(Vector3::Max(a, b), c));
        return Insert(shape);
    }

    std::size_t StaticWorld::AddTriangles(const std::span<const Vector3> positions,
                                          const std::span<const std::uint32_t> indices,
                                          const Microsoft::Xna::Framework::Matrix& world, const std::uint32_t tag,
                                          const std::uint32_t layers)
    {
        if (indices.size() % 3 != 0) {
            throw std::invalid_argument("StaticWorld::AddTriangles: the index count is not a multiple of three");
        }
        std::size_t added = 0;
        for (std::size_t i = 0; i < indices.size(); i += 3) {
            if (indices[i] >= positions.size() || indices[i + 1] >= positions.size() || indices[i + 2] >= positions.size()) {
                throw std::out_of_range("StaticWorld::AddTriangles: an index past the positions");
            }
            const Vector3 a = Vector3::Transform(positions[indices[i]], world);
            const Vector3 b = Vector3::Transform(positions[indices[i + 1]], world);
            const Vector3 c = Vector3::Transform(positions[indices[i + 2]], world);
            if (LengthSquared(Cross(b - a, c - a)) < 1e-12f) {
                continue;
            }
            AddTriangle(a, b, c, tag, layers);
            ++added;
        }
        return added;
    }

    ShapeId StaticWorld::AddCylinder(const Vector3& base, const float radius, const float height,
                                     const std::uint32_t tag, const std::uint32_t layers)
    {
        if (!Finite(base) || !(radius > 0.0f) || !(height > 0.0f) || !std::isfinite(radius) || !std::isfinite(height)) {
            throw std::invalid_argument("StaticWorld::AddCylinder: needs a positive radius and height");
        }
        Shape shape;
        shape.kind = Kind::Cylinder;
        shape.tag = tag;
        shape.layers = layers;
        shape.a = base;
        shape.radius = radius;
        shape.height = height;
        shape.bounds = BoundingBox(Vector3(base.X - radius, base.Y, base.Z - radius),
                                   Vector3(base.X + radius, base.Y + height, base.Z + radius));
        return Insert(shape);
    }

    void StaticWorld::SetHeightField(HeightField field, const std::uint32_t layers)
    {
        if (field.columns < 2 || field.rows < 2 || !(field.cellSize > 0.0f) ||
            field.heights.size() != static_cast<std::size_t>(field.columns) * static_cast<std::size_t>(field.rows)) {
            throw std::invalid_argument("StaticWorld::SetHeightField: needs at least 2 x 2 heights matching its size");
        }
        terrain_ = std::move(field);
        terrainLayers_ = layers;
    }

    void StaticWorld::ClearHeightField() noexcept
    {
        terrain_.reset();
    }

    bool StaticWorld::Remove(const ShapeId shape)
    {
        if (shape >= shapes_.size() || !shapes_[shape].alive) {
            return false;
        }
        const auto& bounds = shapes_[shape].bounds;
        for (int x = CellOf(bounds.Min.X); x <= CellOf(bounds.Max.X); ++x) {
            for (int z = CellOf(bounds.Min.Z); z <= CellOf(bounds.Max.Z); ++z) {
                auto cell = cells_.find(CellKey(x, z));
                if (cell != cells_.end()) {
                    std::erase(cell->second, shape);
                    if (cell->second.empty()) {
                        cells_.erase(cell);
                    }
                }
            }
        }
        shapes_[shape].alive = false;
        free_.push_back(shape);
        --alive_;
        return true;
    }

    void StaticWorld::Clear()
    {
        shapes_.clear();
        free_.clear();
        cells_.clear();
        alive_ = 0;
        terrain_.reset();
    }

    std::uint32_t StaticWorld::Tag(const ShapeId shape) const
    {
        if (shape == TerrainId && terrain_) {
            return terrain_->tag;
        }
        if (shape >= shapes_.size() || !shapes_[shape].alive) {
            throw std::out_of_range("StaticWorld::Tag: no such shape");
        }
        return shapes_[shape].tag;
    }

    void StaticWorld::Candidates(const float minX, const float minZ, const float maxX, const float maxZ,
                                 std::vector<ShapeId>& out) const
    {
        out.clear();
        for (int x = CellOf(minX); x <= CellOf(maxX); ++x) {
            for (int z = CellOf(minZ); z <= CellOf(maxZ); ++z) {
                const auto cell = cells_.find(CellKey(x, z));
                if (cell != cells_.end()) {
                    out.insert(out.end(), cell->second.begin(), cell->second.end());
                }
            }
        }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
    }

    std::optional<RayHit> StaticWorld::RayShape(const Ray& ray, const ShapeId id, const float maxDistance) const
    {
        const Shape& shape = shapes_[id];
        std::optional<std::pair<float, Vector3>> hit;
        switch (shape.kind) {
        case Kind::Box:
            hit = RayBox(ray, shape.bounds);
            break;
        case Kind::Triangle:
            if (const auto t = RayTriangle(ray, shape.a, shape.b, shape.c)) {
                hit = std::pair{*t, Facing(Normalized(Cross(shape.b - shape.a, shape.c - shape.a)), ray.Direction)};
            }
            break;
        case Kind::Cylinder:
            hit = RayCylinder(ray, shape.a, shape.radius, shape.height);
            break;
        }
        if (!hit || hit->first > maxDistance) {
            return std::nullopt;
        }
        RayHit result;
        result.distance = hit->first;
        result.point = ray.Position + ray.Direction * hit->first;
        result.normal = hit->second;
        result.shape = id;
        result.tag = shape.tag;
        return result;
    }

    std::optional<RayHit> StaticWorld::RayTerrain(const Ray& ray, const float maxDistance) const
    {
        const HeightField& field = *terrain_;
        // Walk the cells under the ray (2D DDA over X and Z).
        const float length = std::isfinite(maxDistance)
                                 ? maxDistance
                                 : (static_cast<float>(field.columns + field.rows) * field.cellSize * 2.0f +
                                    Length(ray.Position - field.origin)) /
                                       std::max(Length(ray.Direction), Epsilon);
        const float gx = (ray.Position.X - field.origin.X) / field.cellSize;
        const float gz = (ray.Position.Z - field.origin.Z) / field.cellSize;
        const float dx = ray.Direction.X / field.cellSize;
        const float dz = ray.Direction.Z / field.cellSize;
        int column = static_cast<int>(std::floor(gx));
        int row = static_cast<int>(std::floor(gz));
        const int stepX = dx > 0.0f ? 1 : -1;
        const int stepZ = dz > 0.0f ? 1 : -1;
        const float infinity = std::numeric_limits<float>::infinity();
        const float deltaX = std::fabs(dx) > Epsilon ? std::fabs(1.0f / dx) : infinity;
        const float deltaZ = std::fabs(dz) > Epsilon ? std::fabs(1.0f / dz) : infinity;
        float nextX = std::fabs(dx) > Epsilon ? ((dx > 0.0f ? std::floor(gx) + 1.0f : std::floor(gx)) - gx) / dx : infinity;
        float nextZ = std::fabs(dz) > Epsilon ? ((dz > 0.0f ? std::floor(gz) + 1.0f : std::floor(gz)) - gz) / dz : infinity;
        float t = 0.0f;
        const int maxSteps = 4 * (field.columns + field.rows) + 8;
        for (int step = 0; step < maxSteps && t <= length; ++step) {
            if (column >= 0 && row >= 0 && column < field.columns - 1 && row < field.rows - 1) {
                std::optional<std::pair<float, Vector3>> best;
                for (const bool upper : {false, true}) {
                    Vector3 a;
                    Vector3 b;
                    Vector3 c;
                    CellTriangle(field, column, row, upper, a, b, c);
                    if (const auto hit = RayTriangle(ray, a, b, c); hit && (!best || *hit < best->first)) {
                        best = std::pair{*hit, Facing(Normalized(Cross(b - a, c - a)), ray.Direction)};
                    }
                }
                if (best && best->first <= maxDistance) {
                    RayHit result;
                    result.distance = best->first;
                    result.point = ray.Position + ray.Direction * best->first;
                    result.normal = best->second;
                    result.shape = TerrainId;
                    result.tag = field.tag;
                    return result;
                }
            }
            if (nextX < nextZ) {
                t = nextX;
                nextX += deltaX;
                column += stepX;
            } else {
                t = nextZ;
                nextZ += deltaZ;
                row += stepZ;
            }
            if (!std::isfinite(t)) {
                break;
            }
        }
        return std::nullopt;
    }

    std::optional<RayHit> StaticWorld::Raycast(const Ray& ray, const float maxDistance, const std::uint32_t layers) const
    {
        if (!Finite(ray.Position) || !Finite(ray.Direction) || LengthSquared(ray.Direction) < 1e-12f) {
            throw std::invalid_argument("StaticWorld::Raycast: needs a finite ray with a direction");
        }
        std::optional<RayHit> best;
        if (terrain_ && (terrainLayers_ & layers) != 0) {
            best = RayTerrain(ray, maxDistance);
        }
        // The shapes in the cells the ray crosses, nearest cells first.
        const float cellLength = std::isfinite(maxDistance) ? maxDistance : 1e6f;
        const float dx = ray.Direction.X;
        const float dz = ray.Direction.Z;
        const float gx = ray.Position.X / cellSize_;
        const float gz = ray.Position.Z / cellSize_;
        int x = CellOf(ray.Position.X);
        int z = CellOf(ray.Position.Z);
        const int stepX = dx > 0.0f ? 1 : -1;
        const int stepZ = dz > 0.0f ? 1 : -1;
        const float infinity = std::numeric_limits<float>::infinity();
        const float deltaX = std::fabs(dx) > Epsilon ? std::fabs(cellSize_ / dx) : infinity;
        const float deltaZ = std::fabs(dz) > Epsilon ? std::fabs(cellSize_ / dz) : infinity;
        float nextX = std::fabs(dx) > Epsilon ? ((dx > 0.0f ? std::floor(gx) + 1.0f : std::floor(gx)) - gx) * cellSize_ / dx : infinity;
        float nextZ = std::fabs(dz) > Epsilon ? ((dz > 0.0f ? std::floor(gz) + 1.0f : std::floor(gz)) - gz) * cellSize_ / dz : infinity;
        float entered = 0.0f;
        std::vector<ShapeId> tested;
        // Bounded: a horizontal ray through the whole populated area at most.
        for (int step = 0; step < 1 << 20; ++step) {
            if (best && best->distance < entered) {
                break; // nothing in a farther cell can be nearer
            }
            if (const auto cell = cells_.find(CellKey(x, z)); cell != cells_.end()) {
                for (const ShapeId id : cell->second) {
                    if ((shapes_[id].layers & layers) == 0 || std::find(tested.begin(), tested.end(), id) != tested.end()) {
                        continue;
                    }
                    tested.push_back(id);
                    if (auto hit = RayShape(ray, id, maxDistance); hit && (!best || hit->distance < best->distance)) {
                        best = hit;
                    }
                }
            }
            if (cells_.empty() || (!std::isfinite(nextX) && !std::isfinite(nextZ))) {
                break;
            }
            if (nextX < nextZ) {
                entered = nextX;
                nextX += deltaX;
                x += stepX;
            } else {
                entered = nextZ;
                nextZ += deltaZ;
                z += stepZ;
            }
            if (entered > cellLength) {
                break;
            }
        }
        return best;
    }

    std::vector<Contact> StaticWorld::Contacts(const Capsule& capsule, const std::uint32_t layers) const
    {
        std::vector<Contact> contacts;
        const Vector3 bottom = capsule.Bottom();
        const Vector3 top = capsule.Top();
        const float r = capsule.radius;
        std::vector<ShapeId> candidates;
        Candidates(bottom.X - r, bottom.Z - r, bottom.X + r, bottom.Z + r, candidates);
        const Vector3 middle = (bottom + top) * 0.5f;
        for (const ShapeId id : candidates) {
            const Shape& shape = shapes_[id];
            if ((shape.layers & layers) == 0 || shape.bounds.Max.Y < bottom.Y - r || shape.bounds.Min.Y > top.Y + r) {
                continue;
            }
            std::optional<Contact> contact;
            switch (shape.kind) {
            case Kind::Box: {
                Vector3 onSegment;
                Vector3 onBox;
                ClosestSegmentBox(bottom, top, shape.bounds, onSegment, onBox);
                if (LengthSquared(onSegment - onBox) > 1e-12f) {
                    contact = ContactFrom(onSegment, onBox, r, Vector3::Up);
                } else {
                    // The axis is inside the box: the shortest way out of the
                    // capsule's bounds against the box's.
                    contact = BoxEscape(bottom, top, r, shape.bounds);
                }
                break;
            }
            case Kind::Triangle: {
                Vector3 onSegment;
                Vector3 onTriangle;
                ClosestSegmentTriangle(bottom, top, shape.a, shape.b, shape.c, onSegment, onTriangle);
                Vector3 normal = Normalized(Cross(shape.b - shape.a, shape.c - shape.a));
                if (Dot(middle - shape.a, normal) < 0.0f) {
                    normal = -normal;
                }
                contact = ContactFrom(onSegment, onTriangle, r, normal);
                break;
            }
            case Kind::Cylinder: {
                Vector3 onSegment;
                Vector3 onCylinder;
                ClosestSegmentCylinder(bottom, top, shape.a, shape.radius, shape.height, onSegment, onCylinder);
                if (LengthSquared(onSegment - onCylinder) > 1e-12f) {
                    contact = ContactFrom(onSegment, onCylinder, r, Vector3::Up);
                } else {
                    contact = CylinderEscape(bottom, top, r, shape.a, shape.radius, shape.height);
                }
                break;
            }
            }
            if (contact) {
                contact->shape = id;
                contact->tag = shape.tag;
                contacts.push_back(*contact);
            }
        }
        if (terrain_ && (terrainLayers_ & layers) != 0) {
            if (const auto ground = terrain_->HeightAt(bottom.X, bottom.Z)) {
                const Vector3 normal = terrain_->NormalAt(bottom.X, bottom.Z);
                // Distance from the lower sphere's centre to the local plane.
                const float distance = (bottom.Y - *ground) * normal.Y;
                if (distance < r) {
                    Contact contact;
                    contact.normal = normal;
                    contact.depth = r - distance;
                    contact.point = Vector3(bottom.X, *ground, bottom.Z);
                    contact.shape = TerrainId;
                    contact.tag = terrain_->tag;
                    contacts.push_back(contact);
                }
            }
        }
        // Deepest first; ties in a stable, deterministic order.
        std::stable_sort(contacts.begin(), contacts.end(),
                         [](const Contact& a, const Contact& b) { return a.depth > b.depth; });
        return contacts;
    }

    bool StaticWorld::Overlaps(const Capsule& capsule, const std::uint32_t layers) const
    {
        return !Contacts(capsule, layers).empty();
    }
}
