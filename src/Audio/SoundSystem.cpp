// SPDX-License-Identifier: MIT
#include "CnaEngine/Audio/SoundSystem.hpp"

#include "Microsoft/Xna/Framework/Audio/SoundState.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <numeric>
#include <stdexcept>

namespace CnaEngine::Audio
{
    using Microsoft::Xna::Framework::Vector3;
    namespace XnaAudio = Microsoft::Xna::Framework::Audio;

    namespace
    {
        float Dot(const Vector3& a, const Vector3& b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }

        Vector3 Cross(const Vector3& a, const Vector3& b)
        {
            return Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
        }

        Vector3 Normalized(const Vector3& v)
        {
            const float length = std::sqrt(Dot(v, v));
            return length > 1e-9f ? v * (1.0f / length) : Vector3::Zero;
        }
    }

    Listener ListenerFrom(const Graphics::Camera& camera)
    {
        Listener listener;
        listener.position = camera.Position();
        listener.forward = camera.Forward();
        listener.up = camera.Up();
        return listener;
    }

    Spatial Spatialize(const Listener& listener, const Vector3& source, const Falloff& falloff)
    {
        Spatial result;
        const Vector3 offset = source - listener.position;
        const float distance = std::sqrt(Dot(offset, offset));
        if (!(distance < falloff.radius)) {
            return result;
        }
        float volume = distance <= falloff.reference ? 1.0f : std::pow(falloff.reference / distance, falloff.rolloff);
        const float edgeStart = falloff.radius * (1.0f - std::clamp(falloff.edge, 0.0f, 1.0f));
        if (distance > edgeStart && falloff.radius > edgeStart) {
            volume *= (falloff.radius - distance) / (falloff.radius - edgeStart);
        }
        result.volume = std::clamp(volume, 0.0f, 1.0f);
        const Vector3 right = Normalized(Cross(listener.forward, listener.up));
        result.pan = distance > 1e-6f ? std::clamp(Dot(offset * (1.0f / distance), right), -1.0f, 1.0f) : 0.0f;
        return result;
    }

    void SoundBank::Add(std::string name, const XnaAudio::SoundEffect& sound)
    {
        sounds_[std::move(name)].push_back(&sound);
    }

    std::size_t SoundBank::Variants(const std::string_view name) const
    {
        const auto found = sounds_.find(std::string(name));
        return found == sounds_.end() ? 0 : found->second.size();
    }

    const XnaAudio::SoundEffect* SoundBank::Pick(const std::string_view name, std::uint32_t& seed) const
    {
        const auto found = sounds_.find(std::string(name));
        if (found == sounds_.end() || found->second.empty()) {
            return nullptr;
        }
        seed = seed * 1664525u + 1013904223u;
        return found->second[(seed >> 8) % found->second.size()];
    }

    SoundSystem::SoundSystem(const int voiceBudget) : budget_(voiceBudget)
    {
        if (voiceBudget < 1) {
            throw std::invalid_argument("SoundSystem: needs at least one voice");
        }
    }

    SoundSystem::~SoundSystem()
    {
        StopAll();
    }

    void SoundSystem::SetMasterVolume(const float volume) noexcept
    {
        master_ = std::clamp(volume, 0.0f, 1.0f);
    }

    void SoundSystem::SetCategoryVolume(const std::string_view category, const float volume)
    {
        categories_[std::string(category)] = std::clamp(volume, 0.0f, 1.0f);
    }

    float SoundSystem::CategoryVolume(const std::string_view category) const
    {
        const auto found = categories_.find(std::string(category));
        return found == categories_.end() ? 1.0f : found->second;
    }

    void SoundSystem::Fail(const char* what, const std::exception* error) noexcept
    {
        state_ = AudioState::Silent;
        reason_ = std::string(what) + ": " + (error != nullptr ? error->what() : "unknown error");
        for (auto& voice : voices_) {
            try {
                voice.instance->Stop();
            } catch (...) {
            }
        }
        voices_.clear();
    }

    SoundSystem::Voice* SoundSystem::Find(const VoiceId voice)
    {
        for (auto& v : voices_) {
            if (v.id == voice) {
                return &v;
            }
        }
        return nullptr;
    }

    const SoundSystem::Voice* SoundSystem::Find(const VoiceId voice) const
    {
        for (const auto& v : voices_) {
            if (v.id == voice) {
                return &v;
            }
        }
        return nullptr;
    }

    float SoundSystem::Mix(const Voice& voice) const
    {
        return muted_ ? 0.0f : voice.volume * voice.options.volume * CategoryVolume(voice.options.category) * master_;
    }

    VoiceId SoundSystem::Play(const XnaAudio::SoundEffect& sound, VoiceOptions options)
    {
        if (state_ == AudioState::Silent) {
            return 0;
        }
        if (options.position && !options.loop && Spatialize(listener_, *options.position, options.falloff).volume <= 0.0f) {
            return 0; // a one-shot nobody can hear
        }
        Voice voice;
        voice.id = next_++;
        voice.sound = &sound;
        voice.options = std::move(options);
        voice.options.pitch = std::clamp(voice.options.pitch, -1.0f, 1.0f);
        try {
            // XNA's CreateInstance; built in place from its result, so
            // neither a copy nor CNA's move constructor (an extension) is used.
            voice.instance.reset(new XnaAudio::SoundEffectInstance(sound.CreateInstance()));
            voice.instance->setIsLoopedProperty(voice.options.loop);
            const float pitch = voice.options.pitch;
            voice.instance->setPitchProperty(pitch);
            voice.duration = static_cast<float>(sound.getDurationProperty().getTotalSecondsProperty()) /
                             std::exp2(voice.options.pitch);
        } catch (const std::exception& e) {
            Fail("creating a voice", &e);
            return 0;
        } catch (...) {
            Fail("creating a voice", nullptr);
            return 0;
        }
        const VoiceId id = voice.id;
        voices_.push_back(std::move(voice));
        Update(0.0f);
        // A one-shot the budget had no voice for is gone already.
        return Find(id) != nullptr ? id : 0;
    }

    void SoundSystem::Stop(const VoiceId voice)
    {
        const auto found = std::find_if(voices_.begin(), voices_.end(), [voice](const Voice& v) { return v.id == voice; });
        if (found == voices_.end()) {
            return;
        }
        try {
            found->instance->Stop();
        } catch (const std::exception& e) {
            Fail("stopping a voice", &e);
            return;
        }
        voices_.erase(found);
    }

    void SoundSystem::StopAll()
    {
        while (!voices_.empty()) {
            Stop(voices_.back().id);
        }
    }

    void SoundSystem::SetPosition(const VoiceId voice, const Vector3& position)
    {
        if (Voice* v = Find(voice)) {
            v->options.position = position;
        }
    }

    void SoundSystem::SetVolume(const VoiceId voice, const float volume)
    {
        if (Voice* v = Find(voice)) {
            v->options.volume = std::clamp(volume, 0.0f, 1.0f);
        }
    }

    bool SoundSystem::IsPlaying(const VoiceId voice) const
    {
        const Voice* v = Find(voice);
        return v != nullptr && v->audible;
    }

    float SoundSystem::EffectiveVolume(const VoiceId voice) const
    {
        const Voice* v = Find(voice);
        return v != nullptr && v->audible ? Mix(*v) : 0.0f;
    }

    float SoundSystem::EffectivePan(const VoiceId voice) const
    {
        const Voice* v = Find(voice);
        return v != nullptr ? v->pan : 0.0f;
    }

    std::size_t SoundSystem::AudibleCount() const noexcept
    {
        return static_cast<std::size_t>(std::count_if(voices_.begin(), voices_.end(), [](const Voice& v) { return v.audible; }));
    }

    void SoundSystem::Apply(Voice& voice)
    {
        float volume = Mix(voice);
        float pan = voice.pan;
        voice.instance->setVolumeProperty(volume);
        voice.instance->setPanProperty(pan);
        const auto state = voice.instance->getStateProperty();
        if (state == XnaAudio::SoundState::Paused) {
            voice.instance->Resume();
        } else if (state == XnaAudio::SoundState::Stopped) {
            voice.instance->Play();
        }
    }

    void SoundSystem::Update(const float seconds)
    {
        if (state_ == AudioState::Silent) {
            return;
        }
        try {
            // Finished one-shots leave.
            std::erase_if(voices_, [seconds](Voice& voice) {
                voice.age += std::max(seconds, 0.0f);
                return !voice.options.loop && voice.audible && voice.age > voice.duration + 0.05f;
            });
            for (auto& voice : voices_) {
                if (voice.options.position) {
                    const Spatial spatial = Spatialize(listener_, *voice.options.position, voice.options.falloff);
                    voice.volume = spatial.volume;
                    voice.pan = spatial.pan;
                } else {
                    voice.volume = 1.0f;
                    voice.pan = std::clamp(voice.options.pan, -1.0f, 1.0f);
                }
            }
            // The budget's voices go to the most important, then the loudest.
            std::vector<std::size_t> order(voices_.size());
            std::iota(order.begin(), order.end(), std::size_t{0});
            std::stable_sort(order.begin(), order.end(), [this](const std::size_t a, const std::size_t b) {
                if (voices_[a].options.priority != voices_[b].options.priority) {
                    return voices_[a].options.priority > voices_[b].options.priority;
                }
                return Mix(voices_[a]) > Mix(voices_[b]);
            });
            int given = 0;
            std::vector<VoiceId> dropped;
            for (const std::size_t index : order) {
                Voice& voice = voices_[index];
                const bool audible = given < budget_ && voice.volume > 0.0f;
                if (audible) {
                    ++given;
                    voice.audible = true;
                    Apply(voice);
                } else if (voice.options.loop) {
                    if (voice.audible) {
                        voice.instance->Pause(); // waits for a voice
                    }
                    voice.audible = false;
                } else {
                    dropped.push_back(voice.id); // a one-shot does not wait
                }
            }
            for (const VoiceId id : dropped) {
                Stop(id);
            }
        } catch (const std::exception& e) {
            Fail("playing", &e);
        } catch (...) {
            Fail("playing", nullptr);
        }
    }
}
