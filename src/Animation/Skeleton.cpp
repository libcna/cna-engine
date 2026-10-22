// SPDX-License-Identifier: MIT
#include "CnaEngine/Animation/Skeleton.hpp"

#include <algorithm>
#include <stdexcept>

namespace CnaEngine::Animation
{
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Quaternion;
    using Microsoft::Xna::Framework::Vector3;

    Matrix BonePose::ToMatrix() const
    {
        return Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(translation);
    }

    BonePose BonePose::FromMatrix(const Matrix& matrix)
    {
        BonePose pose;
        if (!matrix.Decompose(pose.scale, pose.rotation, pose.translation)) {
            throw std::invalid_argument("BonePose::FromMatrix: the matrix has no scale-rotation-translation form");
        }
        return pose;
    }

    BonePose BonePose::Blend(const BonePose& from, const BonePose& to, const float amount)
    {
        BonePose pose;
        pose.translation = Vector3::Lerp(from.translation, to.translation, amount);
        pose.scale = Vector3::Lerp(from.scale, to.scale, amount);
        pose.rotation = Quaternion::Slerp(from.rotation, to.rotation, amount);
        return pose;
    }

    std::optional<int> Skeleton::Find(const std::string_view name) const
    {
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (names[i] == name) {
                return static_cast<int>(i);
            }
        }
        return std::nullopt;
    }

    void Skeleton::Validate() const
    {
        const std::size_t count = parents.size();
        if (bindPose.size() != count || inverseBind.size() != count || (!names.empty() && names.size() != count) ||
            (!rootPrefix.empty() && rootPrefix.size() != count)) {
            throw std::invalid_argument("Skeleton: parents, bind pose, inverse binds (and names, root prefix) differ in size");
        }
        for (std::size_t i = 0; i < count; ++i) {
            if (parents[i] >= static_cast<int>(i) || parents[i] < -1) {
                throw std::invalid_argument("Skeleton: bone " + std::to_string(i) +
                                            " has a parent that does not come before it");
            }
        }
    }

    Skeleton Skeleton::FromBindPose(std::vector<std::string> names, std::vector<int> parents,
                                    std::vector<BonePose> bindPose)
    {
        Skeleton skeleton;
        skeleton.names = std::move(names);
        skeleton.parents = std::move(parents);
        skeleton.bindPose = std::move(bindPose);
        skeleton.inverseBind.assign(skeleton.parents.size(), Matrix::getIdentityProperty());
        skeleton.Validate();
        std::vector<Matrix> model;
        skeleton.ToModelSpace(skeleton.bindPose, model);
        for (std::size_t i = 0; i < model.size(); ++i) {
            skeleton.inverseBind[i] = Matrix::Invert(model[i]);
        }
        return skeleton;
    }

    void Skeleton::ToModelSpace(const std::span<const BonePose> local, std::vector<Matrix>& model) const
    {
        if (local.size() != parents.size()) {
            throw std::invalid_argument("Skeleton::ToModelSpace: one pose per bone is needed");
        }
        model.resize(parents.size());
        for (std::size_t i = 0; i < parents.size(); ++i) {
            const Matrix own = local[i].ToMatrix();
            const int parent = parents[i];
            if (parent >= 0) {
                model[i] = own * model[static_cast<std::size_t>(parent)];
            } else {
                model[i] = rootPrefix.empty() ? own : own * rootPrefix[i];
            }
        }
    }

    BonePose BoneTrack::Sample(const float time) const
    {
        if (keys.empty()) {
            return {};
        }
        if (keys.size() == 1 || time <= keys.front().time) {
            return keys.front().pose;
        }
        if (time >= keys.back().time) {
            return keys.back().pose;
        }
        const auto next = std::upper_bound(keys.begin(), keys.end(), time,
                                           [](const float t, const BoneKey& key) { return t < key.time; });
        const BoneKey& b = *next;
        const BoneKey& a = *(next - 1);
        const float span = b.time - a.time;
        const float amount = span > 0.0f ? (time - a.time) / span : 0.0f;
        return BonePose::Blend(a.pose, b.pose, amount);
    }

    void AnimationClip::Sample(const float time, const std::span<BonePose> pose) const
    {
        for (const auto& track : tracks) {
            if (!track.keys.empty() && track.bone >= 0 && static_cast<std::size_t>(track.bone) < pose.size()) {
                pose[static_cast<std::size_t>(track.bone)] = track.Sample(time);
            }
        }
    }
}
