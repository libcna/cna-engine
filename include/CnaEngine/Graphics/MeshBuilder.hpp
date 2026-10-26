// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/MeshData.hpp"
#include "CnaEngine/Graphics/TextureAtlas.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"

#include <array>
#include <cstdint>

namespace CnaEngine::Graphics
{
    /// All attributes a builder vertex can carry; only those in the builder's
    /// attribute mask are stored.
    struct BuilderVertex
    {
        Microsoft::Xna::Framework::Vector3 position = Microsoft::Xna::Framework::Vector3::Zero;
        Microsoft::Xna::Framework::Vector3 normal = Microsoft::Xna::Framework::Vector3::UnitY;
        Microsoft::Xna::Framework::Vector4 tangent = Microsoft::Xna::Framework::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
        Microsoft::Xna::Framework::Color color = Microsoft::Xna::Framework::Color(255, 255, 255, 255);
        Microsoft::Xna::Framework::Vector2 texCoord0 = Microsoft::Xna::Framework::Vector2::Zero;
        Microsoft::Xna::Framework::Vector2 texCoord1 = Microsoft::Xna::Framework::Vector2::Zero;
        std::array<std::uint8_t, 4> blendIndices{0, 0, 0, 0};
        Microsoft::Xna::Framework::Vector4 blendWeights = Microsoft::Xna::Framework::Vector4(1.0f, 0.0f, 0.0f, 0.0f);
    };

    /// Faces of a box, in the order MeshBuilder::AddBox emits them.
    enum class BoxFace : std::uint8_t
    {
        PositiveZ,
        NegativeZ,
        PositiveX,
        NegativeX,
        PositiveY,
        NegativeY,
    };

    /// How one face of a box is drawn: whether at all, and which part of the
    /// texture it shows (upright and unmirrored as seen from outside; on the
    /// top and bottom faces the texture's top edge is towards -Z and +Z).
    struct BoxFaceStyle
    {
        bool visible = true;
        UvRect uv = UvRect::Full();
    };

    /// One style per BoxFace, indexed by the enum's value.
    using BoxFaces = std::array<BoxFaceStyle, 6>;

    /// The axis a plate's face looks along. Its width runs along X for Z and
    /// Y plates and along Z for X plates; its height along Y for Z and X
    /// plates and along Z for Y plates.
    enum class PlateAxis : std::uint8_t
    {
        Z,
        X,
        Y,
    };

    /// Appends vertices and triangles to a MeshData while keeping every stream
    /// consistent with a fixed attribute mask.
    ///
    /// Triangles and quads are given **clockwise as seen from their front**,
    /// matching MeshData's winding convention, so a builder never has to
    /// translate between conventions.
    ///
    /// Procedural shapes set positions, normals and texCoords0 (when present
    /// in the mask) and the builder's current color; tangents are left at the
    /// neutral default; call MeshData::ComputeTangents() afterwards when needed.
    class MeshBuilder
    {
    public:
        /// Builds into a new mesh with `attributes` (Position is always included).
        explicit MeshBuilder(AttributeMask attributes = {VertexAttribute::Position, VertexAttribute::Normal,
                                                         VertexAttribute::TexCoord0});

        [[nodiscard]] AttributeMask Attributes() const noexcept { return attributes_; }
        [[nodiscard]] const MeshData& Mesh() const noexcept { return mesh_; }
        /// Moves the built mesh out and resets the builder.
        [[nodiscard]] MeshData Take();

        /// Color used by the procedural shapes below.
        void SetColor(const Microsoft::Xna::Framework::Color& color) noexcept { color_ = color; }

        std::uint32_t AddVertex(const BuilderVertex& vertex);
        void AddTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c);
        /// Quad a-b-c-d in clockwise order seen from the front.
        void AddQuad(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d);

        // --- Procedural shapes (front faces outward) ---------------------------
        /// Axis-aligned box; each face has its own 4 vertices with a 0..1 UV square.
        void AddBox(const Microsoft::Xna::Framework::Vector3& min, const Microsoft::Xna::Framework::Vector3& max);
        /// Axis-aligned box with a texture region per face; invisible faces are
        /// left out (the hidden faces of adjacent blocks). Corners are taken
        /// from `min` and `max` exactly.
        void AddBox(const Microsoft::Xna::Framework::Vector3& min, const Microsoft::Xna::Framework::Vector3& max,
                    const BoxFaces& faces);
        /// A quad that is visible from both sides: the four `corners` (with
        /// their `texCoords`) twice, once wound for each side, so no culling
        /// state is needed. The corners run around the quad (bottom-left,
        /// bottom-right, top-right, top-left as seen from the side they run
        /// counterclockwise from, which shows `texCoords` as given); the
        /// other side's copy takes them mirrored back, so the texture reads
        /// unmirrored from both sides.
        void AddDoubleSidedQuad(const std::array<Microsoft::Xna::Framework::Vector3, 4>& corners,
                                const std::array<Microsoft::Xna::Framework::Vector2, 4>& texCoords);
        /// A flat, double-sided rectangle centred on `center` (signs, decals,
        /// leaves); see PlateAxis for which way `width` and `height` run. The
        /// region reads upright and unmirrored from both sides: on Z and X
        /// plates its top edge is up, on Y plates towards -Z seen from above.
        void AddPlate(const Microsoft::Xna::Framework::Vector3& center, float width, float height, PlateAxis axis,
                      const UvRect& uv = UvRect::Full());
        /// `planes` vertical double-sided plates through `center`, turned
        /// 180/planes degrees apart about +Y (plants, flames, sparks that look
        /// solid from every side).
        void AddCrossedPlates(const Microsoft::Xna::Framework::Vector3& center, float width, float height,
                              int planes = 3, const UvRect& uv = UvRect::Full());
        /// A quad facing the camera, spanned by its right and up vectors
        /// (`cameraRight`, `cameraUp`, e.g. the first two rows of the inverse
        /// view matrix): `size` world units wide and high, pivoting about
        /// `origin` (0..1 across the quad, (0.5, 0.5) its centre) and turned
        /// `rotation` radians in the view plane.
        void AddBillboard(const Microsoft::Xna::Framework::Vector3& position,
                          const Microsoft::Xna::Framework::Vector2& size,
                          const Microsoft::Xna::Framework::Vector3& cameraRight,
                          const Microsoft::Xna::Framework::Vector3& cameraUp, const UvRect& uv = UvRect::Full(),
                          const Microsoft::Xna::Framework::Vector2& origin = Microsoft::Xna::Framework::Vector2(0.5f,
                                                                                                                0.5f),
                          float rotation = 0.0f);
        /// Rectangle in the XZ plane at `center`, facing +Y, subdivided into
        /// `segmentsX` x `segmentsZ` cells; UVs span 0..uvScale.
        void AddGrid(const Microsoft::Xna::Framework::Vector3& center, float sizeX, float sizeZ, int segmentsX,
                     int segmentsZ, float uvScale = 1.0f);
        /// UV sphere; `slices` around Y (>= 3), `stacks` from pole to pole (>= 2).
        void AddSphere(const Microsoft::Xna::Framework::Vector3& center, float radius, int slices, int stacks);
        /// Cylinder along +Y from `baseCenter`; optional flat caps.
        void AddCylinder(const Microsoft::Xna::Framework::Vector3& baseCenter, float radius, float height,
                         int slices, bool caps = true);

    private:
        BuilderVertex Vertex(const Microsoft::Xna::Framework::Vector3& position,
                             const Microsoft::Xna::Framework::Vector3& normal,
                             const Microsoft::Xna::Framework::Vector2& uv) const;

        AttributeMask attributes_;
        MeshData mesh_;
        Microsoft::Xna::Framework::Color color_ = Microsoft::Xna::Framework::Color(255, 255, 255, 255);
    };
}
