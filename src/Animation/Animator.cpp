// SPDX-License-Identifier: MIT
#include "CnaEngine/Animation/Animator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CnaEngine::Animation
{
    using Microsoft::Xna::Framework::Matrix;

    Animator::Animator(const Skeleton& skeleton) : skeleton_(&skeleton)
    {
        skeleton.Validate();
        local_ = skeleton.bindPose;
        scratch_ = skeleton.bindPose;
        palette_.resize(skeleton.Count());
        Compose();
    }

    void Animator::Play(const AnimationClip& clip, const PlayOptions options)
    {
        if (current_.clip == &clip && !options.restart) {
            current_.options = options;
            return;
        }
        previous_ = current_;
        fadeSeconds_ = std::max(0.0f, options.fadeSeconds);
        fade_ = 0.0f;
        current_ = Track{&clip, 0.0f, options};
        Compose();
    }

    void Animator::Stop(const float fadeSeconds)
    {
        previous_ = current_;
        current_ = Track{};
        fadeSeconds_ = std::max(0.0f, fadeSeconds);
        fade_ = 0.0f;
        Compose();
    }

    void Animator::Advance(Track& track, const float seconds) const
    {
        if (track.clip == nullptr) {
            return;
        }
        const float duration = track.clip->duration;
        float time = track.time + seconds * track.options.speed;
        if (duration > 0.0f) {
            if (track.options.loop) {
                time = std::fmod(time, duration);
                if (time < 0.0f) {
                    time += duration;
                }
            } else {
                time = std::clamp(time, 0.0f, duration);
            }
        } else {
            time = 0.0f;
        }
        track.time = time;
    }

    void Animator::Update(const float seconds)
    {
        if (!std::isfinite(seconds)) {
            return;
        }
        Advance(current_, seconds);
        if (Fading()) {
            Advance(previous_, seconds);
            fade_ = std::min(fade_ + seconds, fadeSeconds_);
        }
        Compose();
    }

    void Animator::SetTime(const float time)
    {
        if (current_.clip != nullptr) {
            current_.time = 0.0f;
            Advance(current_, time / (current_.options.speed != 0.0f ? current_.options.speed : 1.0f));
            Compose();
        }
    }

    bool Animator::Finished() const noexcept
    {
        return current_.clip != nullptr && !current_.options.loop && current_.time >= current_.clip->duration;
    }

    void Animator::Sample(const Track& track, std::vector<BonePose>& pose) const
    {
        pose = skeleton_->bindPose;
        if (track.clip != nullptr) {
            track.clip->Sample(track.time, pose);
        }
    }

    void Animator::Compose()
    {
        Sample(current_, local_);
        if (Fading()) {
            Sample(previous_, scratch_);
            const float weight = fadeSeconds_ > 0.0f ? fade_ / fadeSeconds_ : 1.0f;
            for (std::size_t i = 0; i < local_.size(); ++i) {
                local_[i] = BonePose::Blend(scratch_[i], local_[i], weight);
            }
        }
        skeleton_->ToModelSpace(local_, model_);
        for (std::size_t i = 0; i < model_.size(); ++i) {
            palette_[i] = skeleton_->inverseBind[i] * model_[i];
        }
    }

    const Matrix& Animator::BoneTransform(const int bone) const
    {
        if (bone < 0 || static_cast<std::size_t>(bone) >= model_.size()) {
            throw std::out_of_range("Animator::BoneTransform: no such bone");
        }
        return model_[static_cast<std::size_t>(bone)];
    }
}
