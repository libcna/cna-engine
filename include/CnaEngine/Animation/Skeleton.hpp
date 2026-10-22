// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace CnaEngine::Animation
{
    /// A bone's transform relative to its parent: scale, then rotation, then
    /// translation (XNA's CreateScale * CreateFromQuaternion * CreateTranslation).
    struct BonePose
    {
        Microsoft::Xna::Framework::Vector3 translation = Microsoft::Xna::Framework::Vector3::Zero;
        Microsoft::Xna::Framework::Quaternion rotation = Microsoft::Xna::Framework::Quaternion::Identity;
        Microsoft::Xna::Framework::Vector3 scale = Microsoft::Xna::Framework::Vector3(1.0f, 1.0f, 1.0f);

        [[nodiscard]] Microsoft::Xna::Framework::Matrix ToMatrix() const;
        /// Through XNA's Matrix::Decompose; throws std::invalid_argument when
        /// it reports no decomposition.
        [[nodiscard]] static BonePose FromMatrix(const Microsoft::Xna::Framework::Matrix& matrix);
        /// Translation and scale lerped, rotation slerped (as CNA's
        /// AnimationPlayer between keyframes).
        [[nodiscard]] static BonePose Blend(const BonePose& from, const BonePose& to, float amount);
    };

    /// The bones of a skinned model: names, parents (before their children;
    /// -1 for a root), the bind pose relative to the parents, and the inverse
    /// of each bone's bind pose in model space, which the skin palette needs.
    ///
    /// CNA's SkinningData (XNA's Skinned Model Sample) carries the same; an
    /// enhanced adapter converts it (Enhanced/ModelImport.hpp). Games that
    /// build rigs in code (street, island) fill one directly.
    struct Skeleton
    {
        std::vector<std::string> names;
        std::vector<int> parents;
        std::vector<BonePose> bindPose;
        std::vector<Microsoft::Xna::Framework::Matrix> inverseBind;
        /// What each root bone's transform is composed with (the scene above
        /// an imported skeleton); empty: identity for every root.
        std::vector<Microsoft::Xna::Framework::Matrix> rootPrefix;

        [[nodiscard]] std::size_t Count() const noexcept { return parents.size(); }
        [[nodiscard]] std::optional<int> Find(std::string_view name) const;
        /// Throws std::invalid_argument when the sizes differ or a parent does
        /// not come before its child.
        void Validate() const;
        /// A skeleton from names, parents and a bind pose; the inverse binds
        /// are computed from the bind pose.
        [[nodiscard]] static Skeleton FromBindPose(std::vector<std::string> names, std::vector<int> parents,
                                                   std::vector<BonePose> bindPose);
        /// Model-space transforms of a local pose (one per bone).
        void ToModelSpace(std::span<const BonePose> local, std::vector<Microsoft::Xna::Framework::Matrix>& model) const;
    };

    struct BoneKey
    {
        float time = 0.0f;
        BonePose pose;
    };

    /// One bone's keys, in time order.
    struct BoneTrack
    {
        int bone = -1;
        std::vector<BoneKey> keys;

        /// The pose at `time`: the first key before the first, the last after
        /// the last, interpolated between (Blend).
        [[nodiscard]] BonePose Sample(float time) const;
    };

    /// A named animation: tracks for some bones over `duration` seconds;
    /// bones without a track keep their bind pose.
    struct AnimationClip
    {
        std::string name;
        float duration = 0.0f;
        std::vector<BoneTrack> tracks;

        /// Writes the tracked bones' poses at `time` into `pose` (sized to
        /// the skeleton); others are left as they are.
        void Sample(float time, std::span<BonePose> pose) const;
    };
}
