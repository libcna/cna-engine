// SPDX-License-Identifier: MIT
#include "CnaEngine/Animation/Animator.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

namespace CnaEngine::Animation
{
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Quaternion;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        constexpr float Pi = 3.14159265358979f;

        BonePose At(const Vector3& translation, const Quaternion& rotation = Quaternion::Identity)
        {
            BonePose pose;
            pose.translation = translation;
            pose.rotation = rotation;
            return pose;
        }

        Quaternion AboutZ(const float degrees)
        {
            return Quaternion::CreateFromAxisAngle(Vector3(0.0f, 0.0f, 1.0f), degrees * Pi / 180.0f);
        }

        // root at the origin, tip one metre up.
        Skeleton Chain()
        {
            return Skeleton::FromBindPose({"root", "tip"}, {-1, 0}, {At(Vector3::Zero), At(Vector3(0.0f, 1.0f, 0.0f))});
        }

        // The tip turns about Z: 0 degrees at 0 s, 90 at 1 s, 0 at 2 s.
        AnimationClip Bend(const float degrees = 90.0f)
        {
            AnimationClip clip;
            clip.name = "bend";
            clip.duration = 2.0f;
            BoneTrack track;
            track.bone = 1;
            track.keys = {{0.0f, At(Vector3(0.0f, 1.0f, 0.0f))},
                          {1.0f, At(Vector3(0.0f, 1.0f, 0.0f), AboutZ(degrees))},
                          {2.0f, At(Vector3(0.0f, 1.0f, 0.0f))}};
            clip.tracks.push_back(track);
            return clip;
        }

        // A model-space point skinned entirely by `bone`.
        Vector3 Skin(const Animator& animator, const int bone, const Vector3& point)
        {
            return Vector3::Transform(point, animator.SkinPalette()[static_cast<std::size_t>(bone)]);
        }

        void ExpectNear(const Vector3& actual, const Vector3& expected, const float tolerance = 1e-4f)
        {
            EXPECT_NEAR(actual.X, expected.X, tolerance);
            EXPECT_NEAR(actual.Y, expected.Y, tolerance);
            EXPECT_NEAR(actual.Z, expected.Z, tolerance);
        }
    }

    TEST(AnimationTest, PosesRoundTripThroughMatrices)
    {
        BonePose pose = At(Vector3(1.0f, 2.0f, 3.0f), AboutZ(30.0f));
        pose.scale = Vector3(2.0f, 2.0f, 2.0f);
        const BonePose back = BonePose::FromMatrix(pose.ToMatrix());
        ExpectNear(back.translation, pose.translation);
        ExpectNear(back.scale, pose.scale);
        EXPECT_NEAR(std::fabs(Quaternion::Dot(back.rotation, pose.rotation)), 1.0f, 1e-5f);
    }

    TEST(AnimationTest, TheBindPoseSkinsNothing)
    {
        const Skeleton skeleton = Chain();
        const Vector3 tipBind = Vector3::Transform(Vector3::Zero, Matrix::Invert(skeleton.inverseBind[1]));
        ExpectNear(tipBind, Vector3(0.0f, 1.0f, 0.0f));
        const Animator animator(skeleton);
        ExpectNear(Skin(animator, 0, Vector3(0.3f, 0.2f, 0.1f)), Vector3(0.3f, 0.2f, 0.1f));
        ExpectNear(Skin(animator, 1, Vector3(0.0f, 2.0f, 0.0f)), Vector3(0.0f, 2.0f, 0.0f));
        EXPECT_EQ(skeleton.Find("tip"), 1);
        EXPECT_FALSE(skeleton.Find("hand").has_value());
    }

    TEST(AnimationTest, AClipBendsTheTipAndInterpolates)
    {
        const Skeleton skeleton = Chain();
        const AnimationClip bend = Bend();
        Animator animator(skeleton);
        animator.Play(bend, {.fadeSeconds = 0.0f});
        animator.Update(1.0f);
        // (0, 2, 0) is a metre above the tip; turned 90 degrees about Z it points along -X.
        ExpectNear(Skin(animator, 1, Vector3(0.0f, 2.0f, 0.0f)), Vector3(-1.0f, 1.0f, 0.0f));
        ExpectNear(Skin(animator, 0, Vector3(0.0f, 0.5f, 0.0f)), Vector3(0.0f, 0.5f, 0.0f));
        animator.SetTime(0.5f); // halfway: 45 degrees
        const float s = std::sqrt(0.5f);
        ExpectNear(Skin(animator, 1, Vector3(0.0f, 2.0f, 0.0f)), Vector3(-s, 1.0f + s, 0.0f));
        // The tip's own transform, for attachments.
        ExpectNear(Vector3::Transform(Vector3::Zero, animator.BoneTransform(1)), Vector3(0.0f, 1.0f, 0.0f));
        EXPECT_THROW((void)animator.BoneTransform(2), std::out_of_range);
    }

    TEST(AnimationTest, LoopsWrapAndOnceClampsAndFinishes)
    {
        const Skeleton skeleton = Chain();
        const AnimationClip bend = Bend();
        Animator looping(skeleton);
        looping.Play(bend, {.fadeSeconds = 0.0f});
        looping.Update(5.0f); // 5 s of a 2 s loop: 1 s
        EXPECT_NEAR(looping.Time(), 1.0f, 1e-5f);
        EXPECT_FALSE(looping.Finished());

        Animator once(skeleton);
        once.Play(bend, {.loop = false, .fadeSeconds = 0.0f});
        once.Update(5.0f);
        EXPECT_NEAR(once.Time(), 2.0f, 1e-6f);
        EXPECT_TRUE(once.Finished());

        Animator fast(skeleton);
        fast.Play(bend, {.speed = 2.0f, .fadeSeconds = 0.0f});
        fast.Update(0.5f);
        EXPECT_NEAR(fast.Time(), 1.0f, 1e-6f);
    }

    TEST(AnimationTest, ClipsCrossFade)
    {
        const Skeleton skeleton = Chain();
        const AnimationClip left = Bend(90.0f);
        const AnimationClip right = Bend(-90.0f);
        Animator animator(skeleton);
        animator.Play(left, {.fadeSeconds = 0.0f});
        animator.SetTime(1.0f);
        animator.Play(right, {.fadeSeconds = 1.0f});
        EXPECT_TRUE(animator.Fading());
        // Half a second on: the left clip at 1.5 s (45 degrees left), the
        // right one at 0.5 s (45 degrees right), weighed half and half: straight.
        animator.Update(0.5f);
        ExpectNear(Skin(animator, 1, Vector3(0.0f, 2.0f, 0.0f)), Vector3(0.0f, 2.0f, 0.0f));
        animator.Update(0.5f);
        EXPECT_FALSE(animator.Fading());
        ExpectNear(Skin(animator, 1, Vector3(0.0f, 2.0f, 0.0f)), Vector3(1.0f, 1.0f, 0.0f));
        // Playing the clip that plays does not restart it.
        animator.Play(right);
        EXPECT_NEAR(animator.Time(), 1.0f, 1e-6f);
        animator.Stop(0.0f);
        EXPECT_EQ(animator.Clip(), nullptr);
        ExpectNear(Skin(animator, 1, Vector3(0.0f, 2.0f, 0.0f)), Vector3(0.0f, 2.0f, 0.0f));
    }

    TEST(AnimationTest, InvalidSkeletonsAreRejected)
    {
        EXPECT_THROW((void)Skeleton::FromBindPose({"a", "b"}, {1, -1}, {BonePose{}, BonePose{}}), std::invalid_argument);
        Skeleton uneven = Chain();
        uneven.bindPose.pop_back();
        EXPECT_THROW(Animator{uneven}, std::invalid_argument);
    }
}
