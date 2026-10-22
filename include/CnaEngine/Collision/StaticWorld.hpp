// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace CnaEngine::Collision
{
    using ShapeId = std::uint32_t;

    /// An upright capsule, the body of a walking character: a vertical
    /// segment from feet + radius to feet + height - radius, swept by a
    /// sphere of `radius`. Height is at least twice the radius.
    struct Capsule
    {
        Microsoft::Xna::Framework::Vector3 feet = Microsoft::Xna::Framework::Vector3::Zero;
        float radius = 0.3f;
        float height = 1.8f;

        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Bottom() const noexcept;
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Top() const noexcept;
    };

    /// Where a ray met the world.
    struct RayHit
    {
        /// Along the ray, in units of its direction's length (world units for
        /// a unit direction); 0 when the ray starts inside a box.
        float distance = 0.0f;
        Microsoft::Xna::Framework::Vector3 point = Microsoft::Xna::Framework::Vector3::Zero;
        /// The surface's normal, facing the ray's origin.
        Microsoft::Xna::Framework::Vector3 normal = Microsoft::Xna::Framework::Vector3::Up;
        /// The shape hit; StaticWorld::TerrainId for the height field.
        ShapeId shape = 0;
        std::uint32_t tag = 0;
    };

    /// A capsule overlapping a shape: moving it by `normal * depth` separates them.
    struct Contact
    {
        Microsoft::Xna::Framework::Vector3 normal = Microsoft::Xna::Framework::Vector3::Up;
        float depth = 0.0f;
        Microsoft::Xna::Framework::Vector3 point = Microsoft::Xna::Framework::Vector3::Zero;
        ShapeId shape = 0;
        std::uint32_t tag = 0;
    };

    /// A regular grid of heights: `columns` samples along X, `rows` along Z,
    /// `cellSize` apart, starting at `origin` (its Y is added to every
    /// height). Each cell is two triangles split along the diagonal from
    /// (x, z) to (x + 1, z + 1); heights and normals are those triangles'.
    struct HeightField
    {
        Microsoft::Xna::Framework::Vector3 origin = Microsoft::Xna::Framework::Vector3::Zero;
        float cellSize = 1.0f;
        int columns = 0;
        int rows = 0;
        /// rows * columns heights, row by row (z major).
        std::vector<float> heights;
        std::uint32_t tag = 0;

        /// The height at world (x, z), or nothing outside the field.
        [[nodiscard]] std::optional<float> HeightAt(float x, float z) const;
        /// The upward normal at world (x, z) (Up outside the field).
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 NormalAt(float x, float z) const;
    };

    /// The static geometry characters walk in and rays hit: axis-aligned
    /// boxes, triangles (two-sided), vertical cylinders and one height
    /// field, in a uniform grid over X and Z (shapes span whole columns, so
    /// floors above one another share cells).
    ///
    /// This is what the games each wrote: school's and museum's boxes,
    /// car's boxes and cylinders in a uniform grid, island's cylinders and
    /// height field, house's triangles, boxes and height field in a grid
    /// (docs/game-facilities-survey.md). It is not a physics engine: shapes
    /// do not move or react; a game moves bodies with CharacterController.
    ///
    /// Every shape carries `layers` (a bit mask; queries take a mask) and a
    /// `tag` the game chooses (a surface kind, an object index).
    class StaticWorld
    {
    public:
        static constexpr ShapeId TerrainId = std::numeric_limits<ShapeId>::max();

        /// Throws std::invalid_argument unless cellSize > 0.
        explicit StaticWorld(float cellSize = 4.0f);

        ShapeId AddBox(const Microsoft::Xna::Framework::BoundingBox& box, std::uint32_t tag = 0,
                       std::uint32_t layers = 1);
        ShapeId AddTriangle(const Microsoft::Xna::Framework::Vector3& a, const Microsoft::Xna::Framework::Vector3& b,
                            const Microsoft::Xna::Framework::Vector3& c, std::uint32_t tag = 0, std::uint32_t layers = 1);
        /// Every triangle of an indexed list (three indices each), transformed
        /// by `world`; degenerate triangles are skipped. Returns how many were added.
        std::size_t AddTriangles(std::span<const Microsoft::Xna::Framework::Vector3> positions,
                                 std::span<const std::uint32_t> indices, const Microsoft::Xna::Framework::Matrix& world,
                                 std::uint32_t tag = 0, std::uint32_t layers = 1);
        /// A solid vertical cylinder standing on `base` (a tree, a post, a person).
        ShapeId AddCylinder(const Microsoft::Xna::Framework::Vector3& base, float radius, float height,
                            std::uint32_t tag = 0, std::uint32_t layers = 1);
        /// Throws std::invalid_argument for a field whose sizes do not match.
        void SetHeightField(HeightField field, std::uint32_t layers = 1);
        void ClearHeightField() noexcept;
        [[nodiscard]] const std::optional<HeightField>& Terrain() const noexcept { return terrain_; }

        /// Returns whether the shape existed.
        bool Remove(ShapeId shape);
        void Clear();
        [[nodiscard]] std::size_t ShapeCount() const noexcept { return alive_; }
        [[nodiscard]] std::uint32_t Tag(ShapeId shape) const;

        /// The nearest hit along `ray` within `maxDistance` (in units of the
        /// direction's length), on shapes in `layers`.
        [[nodiscard]] std::optional<RayHit> Raycast(const Microsoft::Xna::Framework::Ray& ray,
                                                    float maxDistance = std::numeric_limits<float>::infinity(),
                                                    std::uint32_t layers = 0xFFFFFFFFu) const;
        /// Every shape the capsule overlaps, with how to get out of it.
        [[nodiscard]] std::vector<Contact> Contacts(const Capsule& capsule, std::uint32_t layers = 0xFFFFFFFFu) const;
        [[nodiscard]] bool Overlaps(const Capsule& capsule, std::uint32_t layers = 0xFFFFFFFFu) const;

    private:
        enum class Kind : std::uint8_t { Box, Triangle, Cylinder };
        struct Shape
        {
            Kind kind = Kind::Box;
            bool alive = false;
            std::uint32_t tag = 0;
            std::uint32_t layers = 1;
            Microsoft::Xna::Framework::BoundingBox bounds;
            Microsoft::Xna::Framework::Vector3 a, b, c; // triangle corners; cylinder base in a
            float radius = 0.0f;
            float height = 0.0f;
        };

        ShapeId Insert(Shape shape);
        [[nodiscard]] std::int64_t CellKey(int x, int z) const noexcept;
        [[nodiscard]] int CellOf(float coordinate) const noexcept;
        void Candidates(float minX, float minZ, float maxX, float maxZ, std::vector<ShapeId>& out) const;
        [[nodiscard]] std::optional<RayHit> RayShape(const Microsoft::Xna::Framework::Ray& ray, ShapeId id,
                                                     float maxDistance) const;
        [[nodiscard]] std::optional<RayHit> RayTerrain(const Microsoft::Xna::Framework::Ray& ray, float maxDistance) const;

        float cellSize_;
        std::vector<Shape> shapes_;
        std::vector<ShapeId> free_;
        std::size_t alive_ = 0;
        std::unordered_map<std::int64_t, std::vector<ShapeId>> cells_;
        std::optional<HeightField> terrain_;
        std::uint32_t terrainLayers_ = 1;
    };
}
