#pragma once

#include "Voice.h"
#include "BodyModel.h"
#include "FX/ChorusTremolo.h"
#include "FX/PreampDrive.h"
#include "FX/PlateReverb.h"
#include "PhysicalConstants.h"
#include <array>
#include <vector>
#include <algorithm>

namespace ModelKeys
{
    class VoiceManager
    {
    public:
        VoiceManager() = default;

        void prepare(double sampleRate, int samplesPerBlock)
        {
            juce::ignoreUnused(samplesPerBlock);
            fs = sampleRate > 0.0 ? sampleRate : 44100.0;
            for (auto& v : voices)
                v.prepare(fs);

            bodyL.prepare(fs);
            bodyR.prepare(fs);
            chorusTremolo.prepare(fs);
            preamp.prepare(fs);
            reverb.prepare(fs);

            reset();
        }

        void reset()
        {
            sustainPedalDown = false;
            for (auto& v : voices)
                v.reset();

            bodyL.reset();
            bodyR.reset();
            chorusTremolo.reset();
            preamp.reset();
            reverb.reset();

            pitchBendSemitones = 0.0f;
            smoothPolyScale = 1.0f;
        }

        void setPolyphony(int maxVoices)
        {
            polyphonyLimit = std::clamp(maxVoices, 1, MAX_VOICES);
        }

        void setPitchBend(float semitones)
        {
            pitchBendSemitones = semitones;
            const float bendFactor = std::pow(2.0f, pitchBendSemitones / 12.0f);
            for (int i = 0; i < polyphonyLimit; ++i)
            {
                if (voices[i].isActive())
                    voices[i].setPitchBendFactor(bendFactor);
            }
        }

        void setModWheel(float mw)
        {
            modWheel = std::clamp(mw, 0.0f, 1.0f);
            for (int i = 0; i < polyphonyLimit; ++i)
            {
                voices[i].setModWheel(modWheel);
            }
        }

        void noteOn(int note, float velocity,
                    ExciterType excType, float hardness, float contactPos, float stiffness,
                    float bowSpeed, float bowForce,
                    float strikeClick, float rosinGrit,
                    float particleDensity, float particleScatter,
                    ResonatorType resType, float decayTime, float brightness, float inharmonicity,
                    float pickupPos,
                    float pitchDropAmount, float pitchDropDecay,
                    float beatingBloom, float materialBalance,
                    PickupType pkType, float pickupDrive, float pickupTone)
        {
            const float bendFactor = std::pow(2.0f, pitchBendSemitones / 12.0f);

            int bestIdx = -1;

            // 1. Look for an inactive voice within limit
            for (int i = 0; i < polyphonyLimit; ++i)
            {
                if (!voices[i].isActive())
                {
                    bestIdx = i;
                    break;
                }
            }

            // 2. If all active, find releasing voice with lowest energy
            if (bestIdx == -1)
            {
                float lowestEnergy = 1e9f;
                for (int i = 0; i < polyphonyLimit; ++i)
                {
                    if (voices[i].isReleasing() && voices[i].getEnergy() < lowestEnergy)
                    {
                        lowestEnergy = voices[i].getEnergy();
                        bestIdx = i;
                    }
                }
            }

            // 3. If pedal is down, steal a voice held only by the pedal with lowest energy
            if (bestIdx == -1 && sustainPedalDown)
            {
                float lowestPedalEnergy = 1e9f;
                for (int i = 0; i < polyphonyLimit; ++i)
                {
                    if (voices[i].isHeldByPedal() && voices[i].getEnergy() < lowestPedalEnergy)
                    {
                        lowestPedalEnergy = voices[i].getEnergy();
                        bestIdx = i;
                    }
                }
            }

            // 4. If all sustaining by fingers, steal oldest voice
            if (bestIdx == -1)
            {
                uint64_t oldestAge = 0;
                for (int i = 0; i < polyphonyLimit; ++i)
                {
                    if (voices[i].getAge() > oldestAge)
                    {
                        oldestAge = voices[i].getAge();
                        bestIdx = i;
                    }
                }
            }

            if (bestIdx >= 0 && bestIdx < polyphonyLimit)
            {
                voices[bestIdx].noteOn(note, velocity,
                                       excType, hardness, contactPos, stiffness,
                                       bowSpeed, bowForce,
                                       strikeClick, rosinGrit,
                                       particleDensity, particleScatter,
                                       resType, decayTime, brightness, inharmonicity,
                                       pickupPos,
                                       pitchDropAmount, pitchDropDecay,
                                       beatingBloom, materialBalance,
                                       pkType, pickupDrive, pickupTone,
                                       bendFactor, modWheel);
            }
        }

        void noteOff(int note, float releaseVelocity = 0.5f, float damperFactor = 0.35f)
        {
            for (int i = 0; i < polyphonyLimit; ++i)
            {
                if (voices[i].isActive() && voices[i].getMidiNote() == note && !voices[i].isReleasing())
                {
                    if (sustainPedalDown)
                    {
                        voices[i].setHeldByPedal(true);
                    }
                    else
                    {
                        voices[i].noteOff(releaseVelocity, damperFactor);
                    }
                }
            }
        }

        void setSustainPedal(bool isDown, float damperFactor = 0.35f)
        {
            sustainPedalDown = isDown;
            if (!sustainPedalDown)
            {
                // Pedal released: release all voices that were held solely by the pedal
                for (int i = 0; i < polyphonyLimit; ++i)
                {
                    if (voices[i].isActive() && voices[i].isHeldByPedal())
                    {
                        voices[i].setHeldByPedal(false);
                        voices[i].noteOff(0.5f, damperFactor);
                    }
                }
            }
        }

        bool isSustainPedalDown() const noexcept { return sustainPedalDown; }

        void allNotesOff(float damperFactor = 0.2f)
        {
            sustainPedalDown = false;
            for (auto& v : voices)
            {
                v.setHeldByPedal(false);
                if (v.isActive())
                    v.noteOff(0.5f, damperFactor);
            }
        }

        // Configure Body & FX
        void configureBody(BodyType type, float size, float resonance, float mix)
        {
            currentBodyResonance = (type != BodyType::Off) ? resonance : 0.0f;
            currentBodyMix = mix;

            // When sustain pedal is held, acoustic grand piano dampers lift across all 88 strings,
            // creating an authentic sympathetic resonance soundboard bloom
            const float effRes = sustainPedalDown ? std::min(1.0f, resonance * 1.15f + 0.05f) : resonance;
            const float effMix = sustainPedalDown ? std::min(1.0f, mix * 1.08f) : mix;
            bodyL.configure(type, size, effRes, effMix);
            bodyR.configure(type, size, effRes, effMix);
        }

        void configurePreamp(PreampType type, float drive, float tone, float level, bool enabled = true)
        {
            preamp.configure(type, drive, tone, level, enabled);
        }

        void configurePreamp(float drive, float tone, float level, bool enabled = true)
        {
            preamp.configure(drive, tone, level, enabled);
        }

        void configureChorusTremolo(ChorusTremolo::Mode mode, float rate, float depth, float mix, bool enabled = true)
        {
            chorusTremolo.configure(mode, rate, depth, mix, enabled);
        }

        void configureReverb(PlateReverb::Type type, float size, float damping, float mix, bool enabled = true)
        {
            reverb.configure(type, size, damping, mix, enabled);
        }

        void configureReverb(float size, float damping, float mix, bool enabled = true)
        {
            reverb.configure(size, damping, mix, enabled);
        }

        // Render audio block
        void processBlock(float* leftOut, float* rightOut, int numSamples, float masterGain)
        {
            // Sympathetic bridge coupling factor: active notes mutually drive each other through the soundboard
            // Doubled when sustain pedal lifts all dampers across the acoustic soundboard
            const float baseCoupling = currentBodyResonance * currentBodyMix * 0.010f;
            const float effCoupling = sustainPedalDown ? (baseCoupling * 2.2f) : baseCoupling;

            for (int s = 0; s < numSamples; ++s)
            {
                // 1. Calculate aggregate bridge velocity from previous sample of active voices
                float aggregateBridgeVel = 0.0f;
                int activeCount = 0;

                for (int i = 0; i < polyphonyLimit; ++i)
                {
                    if (voices[i].isActive())
                    {
                        aggregateBridgeVel += voices[i].getContactVelocity();
                        activeCount++;
                    }
                }

                currentActiveVoiceCount = activeCount;

                float voiceSumL = 0.0f;
                float voiceSumR = 0.0f;

                for (int i = 0; i < polyphonyLimit; ++i)
                {
                    if (voices[i].isActive())
                    {
                        // Sympathetic force is the bridge energy from OTHER vibrating notes
                        const float otherNotesBridgeVel = aggregateBridgeVel - voices[i].getContactVelocity();
                        const float sympatheticForce = effCoupling * otherNotesBridgeVel;

                        const float vOut = voices[i].processSample(sympatheticForce);
                        const int note = voices[i].getMidiNote();
                        // Authentic keyboard stereo spread: A0 (-0.35) -> C4 (0.0) -> C8 (+0.35)
                        const float pan = (note >= 0) ? std::clamp((note - 60) * (0.35f / 48.0f), -0.35f, 0.35f) : 0.0f;
                        voiceSumL += vOut * (1.0f - pan);
                        voiceSumR += vOut * (1.0f + pan);
                    }
                }

                // Polyphony headroom scale factor: preserves clear headroom when playing dense chords
                const float targetPolyScale = (activeCount > 1) ? (1.0f / std::sqrt(static_cast<float>(activeCount))) : 1.0f;
                // Slewed over ~35ms at 44.1kHz (alpha = 0.0015) to prevent instantaneous step jumps / clicks when voices finish releasing
                smoothPolyScale += 0.0015f * (targetPolyScale - smoothPolyScale);
                const float scaledL = voiceSumL * smoothPolyScale;
                const float scaledR = voiceSumR * smoothPolyScale;

                // Pass summed voices through stereo body acoustic resonator
                float sampleL = bodyL.processSample(scaledL);
                float sampleR = bodyR.processSample(scaledR);

                // Preamp drive
                preamp.process(sampleL, sampleR);

                // Chorus / Tremolo
                chorusTremolo.process(sampleL, sampleR);

                // Plate Reverb
                reverb.process(sampleL, sampleR);

                // Master volume gain
                sampleL *= masterGain;
                sampleR *= masterGain;

                // Smooth musical soft-saturator (prevents harsh digital clipping or DAW automute)
                leftOut[s]  = softLimit(sampleL);
                rightOut[s] = softLimit(sampleR);
            }
        }

        int getActiveVoiceCount() const noexcept { return currentActiveVoiceCount; }
        const std::array<Voice, MAX_VOICES>& getVoices() const noexcept { return voices; }

    private:
        static inline float softLimit(float x)
        {
            // Mathematically C1-continuous soft limiter:
            // 1. Strictly linear f(x) = x for |x| <= 0.75 (zero distortion, derivative 1.0)
            // 2. Smooth hyperbolic saturation above 0.75 asymptotically leveling off to 0.98
            // 3. Matching value and first derivative at x = 0.75: zero jump discontinuity or buzzy glitch
            constexpr float T = 0.75f;
            constexpr float M = 0.98f;
            constexpr float range = M - T; // 0.23f

            const float absX = std::abs(x);
            if (absX <= T)
                return x;

            const float sign = (x >= 0.0f) ? 1.0f : -1.0f;
            return sign * (T + range * std::tanh((absX - T) / range));
        }

        double fs = 44100.0;
        int polyphonyLimit = 16;
        float pitchBendSemitones = 0.0f;
        float modWheel = 0.0f;
        int currentActiveVoiceCount = 0;
        bool sustainPedalDown = false;
        float smoothPolyScale = 1.0f;
        float currentBodyResonance = 0.85f;
        float currentBodyMix = 0.85f;

        std::array<Voice, MAX_VOICES> voices;
        BodyModel bodyL;
        BodyModel bodyR;
        PreampDrive preamp;
        ChorusTremolo chorusTremolo;
        PlateReverb reverb;
    };
}
