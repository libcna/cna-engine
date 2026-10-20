// SPDX-License-Identifier: MIT
#include "CnaEngine/Graphics/TextureAtlas.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace CnaEngine::Graphics
{
    TEST(TextureAtlasTests, RegionsMapToTextureCoordinates)
    {
        TextureAtlas atlas(256, 128);
        atlas.Add("door", AtlasRect{64, 32, 32, 64});
        ASSERT_TRUE(atlas.Contains("door"));
        const UvRect uv = atlas.Uv("door");
        EXPECT_FLOAT_EQ(uv.u0, 0.25f);
        EXPECT_FLOAT_EQ(uv.v0, 0.25f);
        EXPECT_FLOAT_EQ(uv.u1, 0.375f);
        EXPECT_FLOAT_EQ(uv.v1, 0.75f);
        EXPECT_EQ(atlas.Rect("door").width, 32);

        atlas.Add("door", AtlasRect{0, 0, 256, 128});
        EXPECT_EQ(atlas.Count(), 1u) << "adding a name again replaces it";
        const UvRect full = atlas.Uv("door");
        EXPECT_FLOAT_EQ(full.u1, 1.0f);
        EXPECT_FLOAT_EQ(full.v1, 1.0f);
    }

    TEST(TextureAtlasTests, GridNamesFramesRowByRow)
    {
        TextureAtlas atlas(100, 100);
        atlas.AddGrid("walk", 16, 20, 3, 2, 2, 4, 1, 3);
        EXPECT_EQ(atlas.Count(), 6u);
        const AtlasRect& second = atlas.Rect("walk_1");
        EXPECT_EQ(second.x, 2 + 17);
        EXPECT_EQ(second.y, 4);
        const AtlasRect& fourth = atlas.Rect("walk_3");
        EXPECT_EQ(fourth.x, 2);
        EXPECT_EQ(fourth.y, 4 + 23);
        EXPECT_EQ(fourth.height, 20);
        EXPECT_THROW(atlas.AddGrid("", 16, 16, 1, 1), std::invalid_argument);
        EXPECT_THROW(atlas.AddGrid("x", 0, 16, 1, 1), std::invalid_argument);
        EXPECT_THROW(atlas.AddGrid("x", 16, 16, 1, 1, -1), std::invalid_argument);
    }

    TEST(TextureAtlasTests, UnknownRegionsAndUnknownSize)
    {
        TextureAtlas atlas;
        atlas.Add("tile", AtlasRect{0, 0, 8, 8});
        const UvRect none = atlas.Uv("tile");
        EXPECT_EQ(none.u1, 0.0f) << "no size, no coordinates";
        EXPECT_THROW((void)atlas.Uv("missing"), std::out_of_range);
        EXPECT_THROW((void)atlas.Rect("missing"), std::out_of_range);
        const UvRect fallback = atlas.UvOrDefault("missing", UvRect::Full());
        EXPECT_EQ(fallback.u1, 1.0f);
        atlas.SetSize(16, 16);
        EXPECT_FLOAT_EQ(atlas.UvOrDefault("tile").u1, 0.5f);
    }
}
