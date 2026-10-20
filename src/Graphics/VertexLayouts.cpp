// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/VertexLayouts.hpp"

#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace CnaEngine::Graphics
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Vector4;
    using Microsoft::Xna::Framework::Graphics::VertexDeclaration;
    using Microsoft::Xna::Framework::Graphics::VertexElement;
    using Microsoft::Xna::Framework::Graphics::VertexElementFormat;
    using Microsoft::Xna::Framework::Graphics::VertexElementUsage;

    namespace
    {
        constexpr VertexAttribute AllAttributes[] = {
            VertexAttribute::Position, VertexAttribute::Normal,    VertexAttribute::Tangent,
            VertexAttribute::Color,    VertexAttribute::TexCoord0, VertexAttribute::TexCoord1,
            VertexAttribute::BlendIndices, VertexAttribute::BlendWeights,
        };

        int SizeOf(const VertexElementFormat format)
        {
            switch (format) {
                case VertexElementFormat::Vector2: return 8;
                case VertexElementFormat::Vector3: return 12;
                case VertexElementFormat::Vector4: return 16;
                case VertexElementFormat::Color:
                case VertexElementFormat::Byte4: return 4;
                default: return 0;
            }
        }

        // Formats that can carry an attribute, and whether the format loses
        // information for it.
        bool Representable(const VertexAttribute attribute, const VertexElementFormat format, bool& narrowed)
        {
            narrowed = false;
            switch (attribute) {
                case VertexAttribute::Position:
                case VertexAttribute::Normal:
                    return format == VertexElementFormat::Vector3 || format == VertexElementFormat::Vector4;
                case VertexAttribute::Tangent:
                    narrowed = format == VertexElementFormat::Vector3;
                    return format == VertexElementFormat::Vector3 || format == VertexElementFormat::Vector4;
                case VertexAttribute::Color:
                    return format == VertexElementFormat::Color || format == VertexElementFormat::Vector4;
                case VertexAttribute::TexCoord0:
                case VertexAttribute::TexCoord1:
                    return format == VertexElementFormat::Vector2;
                case VertexAttribute::BlendIndices:
                    return format == VertexElementFormat::Byte4;
                case VertexAttribute::BlendWeights:
                    return format == VertexElementFormat::Vector4;
            }
            return false;
        }

        // Every attribute is first widened to four floats (w = 1 for points,
        // 0 for directions) and then written in the element's format.
        Vector4 Fetch(const MeshData& mesh, const VertexAttribute attribute, const std::size_t i, const bool present)
        {
            switch (attribute) {
                case VertexAttribute::Position: {
                    const Vector3& p = mesh.positions[i];
                    return Vector4(p.X, p.Y, p.Z, 1.0f);
                }
                case VertexAttribute::Normal: {
                    const Vector3 n = present ? mesh.normals[i] : Vector3::UnitY;
                    return Vector4(n.X, n.Y, n.Z, 0.0f);
                }
                case VertexAttribute::Tangent:
                    return present ? mesh.tangents[i] : Vector4(1.0f, 0.0f, 0.0f, 1.0f);
                case VertexAttribute::Color: {
                    const Color c = present ? mesh.colors[i] : Color(255, 255, 255, 255);
                    return Vector4(c.getRProperty() / 255.0f, c.getGProperty() / 255.0f, c.getBProperty() / 255.0f,
                                   c.getAProperty() / 255.0f);
                }
                case VertexAttribute::TexCoord0: {
                    const Vector2 t = present ? mesh.texCoords0[i] : Vector2::Zero;
                    return Vector4(t.X, t.Y, 0.0f, 0.0f);
                }
                case VertexAttribute::TexCoord1: {
                    const Vector2 t = present ? mesh.texCoords1[i] : Vector2::Zero;
                    return Vector4(t.X, t.Y, 0.0f, 0.0f);
                }
                case VertexAttribute::BlendIndices: {
                    const auto b = present ? mesh.blendIndices[i] : std::array<std::uint8_t, 4>{0, 0, 0, 0};
                    return Vector4(b[0], b[1], b[2], b[3]);
                }
                case VertexAttribute::BlendWeights:
                    return present ? mesh.blendWeights[i] : Vector4(1.0f, 0.0f, 0.0f, 0.0f);
            }
            return Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        void Store(std::uint8_t* destination, const VertexElementFormat format, const VertexAttribute attribute,
                   const MeshData& mesh, const std::size_t i, const bool present, const Vector4& value)
        {
            switch (format) {
                case VertexElementFormat::Vector2: {
                    const float data[2] = {value.X, value.Y};
                    std::memcpy(destination, data, sizeof(data));
                    break;
                }
                case VertexElementFormat::Vector3: {
                    const float data[3] = {value.X, value.Y, value.Z};
                    std::memcpy(destination, data, sizeof(data));
                    break;
                }
                case VertexElementFormat::Vector4: {
                    const float data[4] = {value.X, value.Y, value.Z, value.W};
                    std::memcpy(destination, data, sizeof(data));
                    break;
                }
                case VertexElementFormat::Color: {
                    // The GPU format is XNA's packed value (bytes R, G, B, A in
                    // memory). CNA's C++ Color is not a 4-byte POD, so the
                    // packed value is written explicitly; a color attribute is
                    // copied exactly rather than round-tripped through floats.
                    const Color c = (attribute == VertexAttribute::Color && present) ? mesh.colors[i]
                                                                                     : Color(value);
                    const std::uint32_t packed = c.getPackedValueProperty();
                    std::memcpy(destination, &packed, sizeof(packed));
                    break;
                }
                case VertexElementFormat::Byte4: {
                    // Bone indices, one byte each (copied exactly).
                    const std::uint8_t data[4] = {static_cast<std::uint8_t>(value.X), static_cast<std::uint8_t>(value.Y),
                                                  static_cast<std::uint8_t>(value.Z), static_cast<std::uint8_t>(value.W)};
                    std::memcpy(destination, data, sizeof(data));
                    break;
                }
                default: break;
            }
        }

        VertexLayout MakeLayout(const char* name, std::initializer_list<VertexLayoutElement> elements)
        {
            return VertexLayout(name, std::vector<VertexLayoutElement>(elements));
        }

        constexpr VertexLayoutElement Position{VertexAttribute::Position, VertexElementFormat::Vector3,
                                               VertexElementUsage::Position, 0};
        constexpr VertexLayoutElement Normal{VertexAttribute::Normal, VertexElementFormat::Vector3,
                                             VertexElementUsage::Normal, 0};
        constexpr VertexLayoutElement Tangent{VertexAttribute::Tangent, VertexElementFormat::Vector4,
                                              VertexElementUsage::Tangent, 0};
        constexpr VertexLayoutElement Colour{VertexAttribute::Color, VertexElementFormat::Color,
                                             VertexElementUsage::Color, 0};
        constexpr VertexLayoutElement Uv0{VertexAttribute::TexCoord0, VertexElementFormat::Vector2,
                                          VertexElementUsage::TextureCoordinate, 0};
        constexpr VertexLayoutElement Uv1{VertexAttribute::TexCoord1, VertexElementFormat::Vector2,
                                          VertexElementUsage::TextureCoordinate, 1};
        constexpr VertexLayoutElement Weights{VertexAttribute::BlendWeights, VertexElementFormat::Vector4,
                                              VertexElementUsage::BlendWeight, 0};
        constexpr VertexLayoutElement Bones{VertexAttribute::BlendIndices, VertexElementFormat::Byte4,
                                            VertexElementUsage::BlendIndices, 0};
    }

    VertexLayout::VertexLayout(std::string name, std::vector<VertexLayoutElement> elements)
        : name_(std::move(name)), elements_(std::move(elements))
    {
        std::vector<VertexElement> declared;
        for (const auto& element : elements_) {
            const int size = SizeOf(element.format);
            bool narrowed = false;
            if (size == 0 || !Representable(element.attribute, element.format, narrowed)) {
                throw std::invalid_argument("VertexLayout '" + name_ + "': format cannot carry "
                                            + ToString(element.attribute));
            }
            if (attributes_.Has(element.attribute)) {
                throw std::invalid_argument("VertexLayout '" + name_ + "': duplicate " + ToString(element.attribute));
            }
            attributes_.Add(element.attribute);
            offsets_.push_back(stride_);
            declared.emplace_back(stride_, element.format, element.usage, element.usageIndex);
            stride_ += size;
        }
        if (!attributes_.Has(VertexAttribute::Position)) {
            throw std::invalid_argument("VertexLayout '" + name_ + "': a Position element is required");
        }
        declaration_ = std::make_shared<const VertexDeclaration>(stride_, std::move(declared));
    }

    const VertexLayout& VertexLayout::PositionColor()
    {
        static const VertexLayout layout = MakeLayout("PositionColor", {Position, Colour});
        return layout;
    }

    const VertexLayout& VertexLayout::PositionTexture()
    {
        static const VertexLayout layout = MakeLayout("PositionTexture", {Position, Uv0});
        return layout;
    }

    const VertexLayout& VertexLayout::PositionColorTexture()
    {
        static const VertexLayout layout = MakeLayout("PositionColorTexture", {Position, Colour, Uv0});
        return layout;
    }

    const VertexLayout& VertexLayout::PositionNormalTexture()
    {
        static const VertexLayout layout = MakeLayout("PositionNormalTexture", {Position, Normal, Uv0});
        return layout;
    }

    const VertexLayout& VertexLayout::PositionNormalColorTexture()
    {
        static const VertexLayout layout = MakeLayout("PositionNormalColorTexture", {Position, Normal, Colour, Uv0});
        return layout;
    }

    const VertexLayout& VertexLayout::PositionNormalDualTexture()
    {
        static const VertexLayout layout = MakeLayout("PositionNormalDualTexture", {Position, Normal, Uv0, Uv1});
        return layout;
    }

    const VertexLayout& VertexLayout::PositionNormalTangentTexture()
    {
        static const VertexLayout layout =
            MakeLayout("PositionNormalTangentTexture", {Position, Normal, Tangent, Uv0});
        return layout;
    }

    const VertexLayout& VertexLayout::PositionNormalTextureSkinned()
    {
        static const VertexLayout layout =
            MakeLayout("PositionNormalTextureSkinned", {Position, Normal, Uv0, Weights, Bones});
        return layout;
    }

    const VertexLayout& VertexLayout::PositionNormalTangentTextureSkinned()
    {
        static const VertexLayout layout =
            MakeLayout("PositionNormalTangentTextureSkinned", {Position, Normal, Tangent, Uv0, Weights, Bones});
        return layout;
    }

    InterleavedVertices Interleave(const MeshData& mesh, const VertexLayout& layout, const MissingAttributePolicy policy)
    {
        if (const auto problems = mesh.Validate(); !problems.empty()) {
            throw std::invalid_argument("Interleave: invalid mesh: " + problems.front());
        }
        InterleavedVertices result;
        const AttributeMask present = mesh.Attributes();
        const AttributeMask required = layout.Attributes();
        result.missing = required.Without(present);
        result.dropped = present.Without(required);
        if (mesh.VertexCount() == 0) {
            result.missing = AttributeMask{};
        }
        if (!result.missing.Empty() && policy == MissingAttributePolicy::Fail) {
            throw std::invalid_argument("Interleave: layout '" + layout.Name() + "' needs " + Describe(result.missing)
                                        + " which the mesh does not have");
        }
        for (const auto& element : layout.Elements()) {
            bool narrowed = false;
            Representable(element.attribute, element.format, narrowed);
            if (narrowed && present.Has(element.attribute)) {
                result.narrowed.Add(element.attribute);
            }
        }

        result.stride = layout.Stride();
        result.vertexCount = static_cast<int>(mesh.VertexCount());
        result.bytes.assign(mesh.VertexCount() * static_cast<std::size_t>(layout.Stride()), 0);
        const auto& elements = layout.Elements();
        const auto& offsets = layout.Offsets();
        for (std::size_t i = 0; i < mesh.VertexCount(); ++i) {
            std::uint8_t* vertex = result.bytes.data() + i * static_cast<std::size_t>(layout.Stride());
            for (std::size_t e = 0; e < elements.size(); ++e) {
                const bool has = present.Has(elements[e].attribute);
                const Vector4 value = Fetch(mesh, elements[e].attribute, i, has);
                Store(vertex + offsets[e], elements[e].format, elements[e].attribute, mesh, i, has, value);
            }
        }
        return result;
    }

    std::string Describe(const AttributeMask attributes)
    {
        std::string text;
        for (const auto attribute : AllAttributes) {
            if (attributes.Has(attribute)) {
                text += (text.empty() ? "" : ", ") + std::string(ToString(attribute));
            }
        }
        return text;
    }
}
