// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"

#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace CnaEngine::Audio
{
    /// How a positional sound fades with distance: full volume within
    /// `reference`, then reference / distance raised to `rolloff`, and
    /// silent at `radius`, reached through a fade over the last `edge` share
    /// of it (no sound stops with a click at its reach).
    struct Falloff
    {
        float reference = 1.0f;
        float radius = 30.0f;
        float rolloff = 1.0f;
        float edge = 0.2f;
    };

    /// Where a listener is and which way it faces.
    struct Listener
    {
        Microsoft::Xna::Framework::Vector3 position = Microsoft::Xna::Framework::Vector3::Zero;
        Microsoft::Xna::Framework::Vector3 forward = Microsoft::Xna::Framework::Vector3::Forward;
        Microsoft::Xna::Framework::Vector3 up = Microsoft::Xna::Framework::Vector3::Up;
    };

    /// The camera's ears.
    [[nodiscard]] Listener ListenerFrom(const Graphics::Camera& camera);

    /// The volume (0..1) and stereo pan (-1 left .. 1 right) of a sound at
    /// `source` for `listener`. Hand-computed rather than XNA's Apply3D, as
    /// backrooms-, car- and house-simulator do it: the same on every platform
    /// and testable.
    struct Spatial
    {
        float volume = 0.0f;
        float pan = 0.0f;
    };
    [[nodiscard]] Spatial Spatialize(const Listener& listener, const Microsoft::Xna::Framework::Vector3& source,
                                     const Falloff& falloff);

    /// Named groups of interchangeable sounds (a surface's footsteps, a
    /// door's creaks), picked from with a seed so a replay sounds the same.
    /// The sounds are borrowed.
    class SoundBank
    {
    public:
        void Add(std::string name, const Microsoft::Xna::Framework::Audio::SoundEffect& sound);
        [[nodiscard]] std::size_t Variants(std::string_view name) const;
        /// A variant of `name` (null when there is none); advances `seed`.
        [[nodiscard]] const Microsoft::Xna::Framework::Audio::SoundEffect* Pick(std::string_view name,
                                                                               std::uint32_t& seed) const;

    private:
        std::unordered_map<std::string, std::vector<const Microsoft::Xna::Framework::Audio::SoundEffect*>> sounds_;
    };

    using VoiceId = std::uint32_t;

    struct VoiceOptions
    {
        /// The mix group whose volume scales this voice ("effects",
        /// "ambience", "footsteps", "music" -- any name).
        std::string category = "effects";
        float volume = 1.0f;
        /// Octaves, -1..1 (XNA's pitch).
        float pitch = 0.0f;
        bool loop = false;
        /// A positional voice; none: a plain stereo sound with `pan`.
        std::optional<Microsoft::Xna::Framework::Vector3> position;
        float pan = 0.0f;
        Falloff falloff;
        /// Higher keeps its voice when there are more sounds than voices.
        int priority = 0;
    };

    enum class AudioState : std::uint8_t
    {
        Ready,
        /// No audio device (or it failed): every call is accepted and silent.
        Silent,
    };

    /// The games' sound owner: one-shots and loops, positional or not, mixed
    /// by category under a master volume, with a voice budget -- the nearest
    /// and most important sounds keep their voices, the rest wait silently
    /// (loops) or are dropped (one-shots). Safe without an audio device: the
    /// first failure turns it Silent, with the reason kept, and nothing
    /// throws. School's and museum's AudioSystem, street's SoundScape, house's
    /// states and mix categories (docs/game-facilities-survey.md).
    ///
    /// Sounds are borrowed and must outlive their voices. Call Update once a
    /// frame, after SetListener.
    class SoundSystem
    {
    public:
        /// Street and island keep about six voices per kind; 24 in all is the default.
        explicit SoundSystem(int voiceBudget = 24);
        ~SoundSystem();
        SoundSystem(const SoundSystem&) = delete;
        SoundSystem& operator=(const SoundSystem&) = delete;

        [[nodiscard]] AudioState State() const noexcept { return state_; }
        [[nodiscard]] const std::string& SilentReason() const noexcept { return reason_; }

        void SetMasterVolume(float volume) noexcept;
        [[nodiscard]] float MasterVolume() const noexcept { return master_; }
        void SetMuted(bool muted) noexcept { muted_ = muted; }
        [[nodiscard]] bool Muted() const noexcept { return muted_; }
        void SetCategoryVolume(std::string_view category, float volume);
        [[nodiscard]] float CategoryVolume(std::string_view category) const;

        void SetListener(const Listener& listener) noexcept { listener_ = listener; }
        [[nodiscard]] const Listener& CurrentListener() const noexcept { return listener_; }

        /// Starts a voice; 0 when nothing plays (silent, muted out of the
        /// budget, or a one-shot out of reach).
        VoiceId Play(const Microsoft::Xna::Framework::Audio::SoundEffect& sound, VoiceOptions options = {});
        void Stop(VoiceId voice);
        void StopAll();
        /// Moves a positional voice (an engine on a moving car).
        void SetPosition(VoiceId voice, const Microsoft::Xna::Framework::Vector3& position);
        void SetVolume(VoiceId voice, float volume);
        [[nodiscard]] bool IsPlaying(VoiceId voice) const;
        /// The volume a voice plays at now (0 for one waiting out of the budget).
        [[nodiscard]] float EffectiveVolume(VoiceId voice) const;
        [[nodiscard]] float EffectivePan(VoiceId voice) const;

        /// Retires finished one-shots, places positional voices for the
        /// listener and gives the budget's voices to the most audible.
        void Update(float seconds);
        /// Voices tracked (playing or waiting).
        [[nodiscard]] std::size_t VoiceCount() const noexcept { return voices_.size(); }
        /// Voices audible now.
        [[nodiscard]] std::size_t AudibleCount() const noexcept;

    private:
        struct Voice
        {
            VoiceId id = 0;
            const Microsoft::Xna::Framework::Audio::SoundEffect* sound = nullptr;
            std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffectInstance> instance;
            VoiceOptions options;
            float age = 0.0f;
            float duration = 0.0f;
            float volume = 0.0f;
            float pan = 0.0f;
            bool audible = false;
        };

        void Fail(const char* what, const std::exception* error) noexcept;
        void Apply(Voice& voice);
        [[nodiscard]] Voice* Find(VoiceId voice);
        [[nodiscard]] const Voice* Find(VoiceId voice) const;
        [[nodiscard]] float Mix(const Voice& voice) const;

        int budget_;
        AudioState state_ = AudioState::Ready;
        std::string reason_;
        float master_ = 1.0f;
        bool muted_ = false;
        std::unordered_map<std::string, float> categories_;
        Listener listener_;
        std::vector<Voice> voices_;
        VoiceId next_ = 1;
    };
}
