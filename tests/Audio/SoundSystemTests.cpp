// SPDX-License-Identifier: MIT
#include "CnaEngine/Audio/SoundSystem.hpp"

#include "Microsoft/Xna/Framework/Audio/AudioChannels.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace CnaEngine::Audio
{
    using Microsoft::Xna::Framework::Vector3;
    namespace XnaAudio = Microsoft::Xna::Framework::Audio;

    namespace
    {
        // Half a second of a quiet 440 Hz tone, mono, 16-bit.
        std::unique_ptr<XnaAudio::SoundEffect> Tone(const float seconds = 0.5f)
        {
            constexpr int Rate = 22050;
            const int samples = static_cast<int>(seconds * Rate);
            std::vector<std::uint8_t> pcm(static_cast<std::size_t>(samples) * 2);
            for (int i = 0; i < samples; ++i) {
                const auto value = static_cast<std::int16_t>(1000.0 * std::sin(2.0 * 3.14159265 * 440.0 * i / Rate));
                pcm[static_cast<std::size_t>(i) * 2] = static_cast<std::uint8_t>(value & 0xFF);
                pcm[static_cast<std::size_t>(i) * 2 + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
            }
            return std::make_unique<XnaAudio::SoundEffect>(pcm, Rate, XnaAudio::AudioChannels::Mono);
        }

        VoiceOptions At(const Vector3& position, const bool loop = true)
        {
            VoiceOptions options;
            options.position = position;
            options.loop = loop;
            options.falloff.radius = 50.0f;
            return options;
        }
    }

    TEST(SoundTest, VolumeFallsWithDistanceAndFadesOutAtTheReach)
    {
        Listener listener; // at the origin looking along -Z: right is +X
        Falloff falloff;
        falloff.reference = 2.0f;
        falloff.radius = 20.0f;
        falloff.edge = 0.25f; // fades from 15 m
        EXPECT_FLOAT_EQ(Spatialize(listener, Vector3(0, 0, -1), falloff).volume, 1.0f);
        EXPECT_NEAR(Spatialize(listener, Vector3(0, 0, -4), falloff).volume, 0.5f, 1e-6f);
        EXPECT_NEAR(Spatialize(listener, Vector3(0, 0, -17.5f), falloff).volume, 2.0f / 17.5f * 0.5f, 1e-6f);
        EXPECT_FLOAT_EQ(Spatialize(listener, Vector3(0, 0, -20), falloff).volume, 0.0f);
        EXPECT_FLOAT_EQ(Spatialize(listener, Vector3(0, 0, -40), falloff).volume, 0.0f);
        EXPECT_NEAR(Spatialize(listener, Vector3(5, 0, 0), falloff).pan, 1.0f, 1e-6f);
        EXPECT_NEAR(Spatialize(listener, Vector3(-5, 0, 0), falloff).pan, -1.0f, 1e-6f);
        EXPECT_NEAR(Spatialize(listener, Vector3(0, 0, -5), falloff).pan, 0.0f, 1e-6f);
        // Turned to face +X, a sound at +Z is on the right.
        listener.forward = Vector3(1, 0, 0);
        EXPECT_NEAR(Spatialize(listener, Vector3(0, 0, 5), falloff).pan, 1.0f, 1e-6f);
    }

    TEST(SoundTest, BanksPickVariantsFromASeed)
    {
        const auto a = Tone();
        const auto b = Tone();
        SoundBank bank;
        bank.Add("grass", *a);
        bank.Add("grass", *b);
        EXPECT_EQ(bank.Variants("grass"), 2u);
        EXPECT_EQ(bank.Variants("stone"), 0u);
        std::uint32_t seed = 7;
        std::uint32_t again = 7;
        bool both[2] = {false, false};
        for (int i = 0; i < 20; ++i) {
            const auto* pick = bank.Pick("grass", seed);
            EXPECT_EQ(pick, bank.Pick("grass", again)); // the same seed, the same picks
            both[pick == a.get() ? 0 : 1] = true;
        }
        EXPECT_TRUE(both[0] && both[1]);
        EXPECT_EQ(bank.Pick("stone", seed), nullptr);
    }

    TEST(SoundTest, VoicesMixByCategoryUnderTheMaster)
    {
        const auto tone = Tone();
        SoundSystem sound;
        ASSERT_EQ(sound.State(), AudioState::Ready) << sound.SilentReason();
        VoiceOptions options;
        options.category = "ambience";
        options.volume = 0.8f;
        options.loop = true;
        const VoiceId voice = sound.Play(*tone, options);
        ASSERT_NE(voice, 0u);
        EXPECT_TRUE(sound.IsPlaying(voice));
        EXPECT_NEAR(sound.EffectiveVolume(voice), 0.8f, 1e-6f);
        sound.SetCategoryVolume("ambience", 0.5f);
        sound.SetMasterVolume(0.5f);
        sound.Update(0.016f);
        EXPECT_NEAR(sound.EffectiveVolume(voice), 0.2f, 1e-6f);
        sound.SetMuted(true);
        sound.Update(0.016f);
        EXPECT_FLOAT_EQ(sound.EffectiveVolume(voice), 0.0f);
        sound.Stop(voice);
        EXPECT_FALSE(sound.IsPlaying(voice));
        EXPECT_EQ(sound.VoiceCount(), 0u);
    }

    TEST(SoundTest, TheBudgetGoesToTheNearestAndLoopsWaitForIt)
    {
        const auto tone = Tone();
        SoundSystem sound(2);
        const VoiceId near = sound.Play(*tone, At(Vector3(0, 0, -2)));
        const VoiceId middle = sound.Play(*tone, At(Vector3(0, 0, -5)));
        const VoiceId far = sound.Play(*tone, At(Vector3(0, 0, -10)));
        ASSERT_NE(far, 0u); // a loop waits
        EXPECT_EQ(sound.VoiceCount(), 3u);
        EXPECT_EQ(sound.AudibleCount(), 2u);
        EXPECT_TRUE(sound.IsPlaying(near));
        EXPECT_TRUE(sound.IsPlaying(middle));
        EXPECT_FALSE(sound.IsPlaying(far));

        sound.SetPosition(far, Vector3(0, 0, -1)); // comes closest
        sound.Update(0.016f);
        EXPECT_TRUE(sound.IsPlaying(far));
        EXPECT_FALSE(sound.IsPlaying(middle));

        // A quiet one-shot finds no voice and is not kept.
        EXPECT_EQ(sound.Play(*tone, At(Vector3(0, 0, -30), false)), 0u);
        // An important one takes a voice.
        VoiceOptions alarm = At(Vector3(0, 0, -30), false);
        alarm.priority = 1;
        const VoiceId important = sound.Play(*tone, alarm);
        EXPECT_NE(important, 0u);
        EXPECT_TRUE(sound.IsPlaying(important));
    }

    TEST(SoundTest, OneShotsLeaveWhenDoneAndOutOfReachNeverStart)
    {
        const auto tone = Tone(0.5f);
        SoundSystem sound;
        EXPECT_EQ(sound.Play(*tone, At(Vector3(0, 0, -60), false)), 0u);
        const VoiceId shot = sound.Play(*tone, At(Vector3(1, 0, -1), false));
        ASSERT_NE(shot, 0u);
        EXPECT_GT(sound.EffectivePan(shot), 0.5f);
        sound.Update(0.3f);
        EXPECT_TRUE(sound.IsPlaying(shot));
        sound.Update(0.3f);
        EXPECT_FALSE(sound.IsPlaying(shot));
        EXPECT_EQ(sound.VoiceCount(), 0u);
        EXPECT_THROW(SoundSystem{0}, std::invalid_argument);
    }
}
