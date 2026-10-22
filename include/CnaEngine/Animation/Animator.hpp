// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Animation/Skeleton.hpp"

#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <span>
#include <vector>

namespace CnaEngine::Animation
{
    struct PlayOptions
    {
        bool loop = true;
        /// Playback rate; 1 is the clip's own speed.
        float speed = 1.0f;
        /// Seconds over which the previous clip (or the bind pose) fades out.
        float fadeSeconds = 0.2f;
        /// Play from the start even if this clip is already playing.
        bool restart = false;
    };

    /// Plays clips on a skeleton and produces the skin palette a skinned
    /// render item takes (RenderItem::bones, SkinnedEffect::SetBoneTransforms):
    /// per bone inverseBind * modelSpace, as CNA's AnimationPlayer and XNA's
    /// Skinned Model Sample compute it. Adds what the games wrote on top of
    /// that player: cross-fades between clips (island's blend of decomposed
    /// poses), speed, play-once clips, bone transforms for attachments.
    ///
    /// The skeleton and clips are borrowed and must outlive the animator.
    class Animator
    {
    public:
        /// Throws std::invalid_argument for an invalid skeleton.
        explicit Animator(const Skeleton& skeleton);

        /// Starts `clip`, fading from what plays now. Playing the clip that
        /// already plays changes only the options, unless `restart`.
        void Play(const AnimationClip& clip, PlayOptions options = {});
        /// Fades back to the bind pose.
        void Stop(float fadeSeconds = 0.2f);
        void Update(float seconds);
        /// Jumps the current clip to `time` (no fade).
        void SetTime(float time);

        [[nodiscard]] const AnimationClip* Clip() const noexcept { return current_.clip; }
        [[nodiscard]] float Time() const noexcept { return current_.time; }
        /// A clip played once has reached its end.
        [[nodiscard]] bool Finished() const noexcept;
        [[nodiscard]] bool Fading() const noexcept { return fade_ < fadeSeconds_; }

        [[nodiscard]] std::span<const BonePose> LocalPose() const noexcept { return local_; }
        /// Each bone in model space.
        [[nodiscard]] const std::vector<Microsoft::Xna::Framework::Matrix>& ModelTransforms() const noexcept { return model_; }
        /// inverseBind * model space, for skinning.
        [[nodiscard]] const std::vector<Microsoft::Xna::Framework::Matrix>& SkinPalette() const noexcept { return palette_; }
        /// A bone in model space, for attaching a prop (times the model's world).
        [[nodiscard]] const Microsoft::Xna::Framework::Matrix& BoneTransform(int bone) const;

    private:
        struct Track
        {
            const AnimationClip* clip = nullptr;
            float time = 0.0f;
            PlayOptions options;
        };
        void Advance(Track& track, float seconds) const;
        void Sample(const Track& track, std::vector<BonePose>& pose) const;
        void Compose();

        const Skeleton* skeleton_;
        Track current_;
        Track previous_;
        float fade_ = 0.0f;
        float fadeSeconds_ = 0.0f;
        std::vector<BonePose> local_;
        std::vector<BonePose> scratch_;
        std::vector<Microsoft::Xna::Framework::Matrix> model_;
        std::vector<Microsoft::Xna::Framework::Matrix> palette_;
    };
}
