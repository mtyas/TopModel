#pragma once

#include "ExciterModel.h"
#include "ResonatorModel.h"
#include "PickupModel.h"
#include "PhysicalConstants.h"
#include <cmath>

namespace ModelKeys
{
    class Voice
    {
    public:
        Voice() = default;

        void prepare(double sampleRate)
        {
            fs = sampleRate > 0.0 ? sampleRate : 44100.0;
            exciter.prepare(fs);
            resonator.prepare(fs);
            pickup.prepare(fs);
            reset();
        }

        void reset()
        {
            active = false;
            releasing = false;
            heldByPedal = false;
            fadeGain = 1.0f;
            midiNote = -1;
            velocity = 0.0f;
            age = 0;
            releaseCounter = 0;
            contactVelocityFeedback = 0.0f;
            currentPitchBendFactor = 1.0f;
            noteDetuneRatio = 1.0f;
            modWheelAmount = 0.0f;
            vibPhase = 0.0f;
            driftPhase = 0.0f;
            exciter.reset();
            resonator.reset();
            pickup.reset();
        }

        void noteOn(int note, float vel,
                    ExciterType excType, float hardness, float contactPos, float stiffness,
                    float bowSpeed, float bowForce,
                    float strikeClick, float rosinGrit,
                    float particleDensity, float particleScatter,
                    ResonatorType resType, float decayTime, float brightness, float inharmonicity,
                    float pickupPos,
                    float pitchDropAmount, float pitchDropDecay,
                    float beatingBloom, float materialBalance,
                    PickupType pkType, float pickupDrive, float pickupTone,
                    float pitchBendFactor = 1.0f,
                    float modWheel = 0.0f)
        {
            const bool isRestrike = active && (midiNote == note) && (resonator.getTotalEnergy() > 0.0005f);
            if (isRestrike)
            {
                // Preserve vibrating string phase & energy on re-strike (precedent state handover)
                resonator.softRetrigger(0.35f);
            }
            else
            {
                resonator.reset();
            }
            pickup.reset();

            // Felt compaction memory: rapid successive strikes (< 250ms) leave felt compacted,
            // producing more articulate, biting, punchy re-strikes
            float compactionBoost = 0.0f;
            if (age > 0 && age < static_cast<uint64_t>(0.25 * fs) && midiNote == note)
            {
                const float timeNorm = static_cast<float>(age) / static_cast<float>(0.25 * fs);
                compactionBoost = (1.0f - timeNorm) * 0.12f;
            }

            midiNote = note;
            velocity = vel;
            active = true;
            releasing = false;
            heldByPedal = false;
            isDeclicking = false;
            declickSamplesLeft = 0;
            fadeGain = 1.0f;
            age = 0;
            releaseCounter = 0;
            contactVelocityFeedback = 0.0f;
            currentPitchBendFactor = pitchBendFactor;
            modWheelAmount = modWheel;

            // High-quality per-note pseudo-random seed (deterministic & thread-safe)
            static uint32_t sessionCounter = 0x1A2B3C4Du;
            sessionCounter += 0x6D2B79F5u;
            uint32_t seed = static_cast<uint32_t>(note) * 1973u + static_cast<uint32_t>(vel * 1000.0f) * 9277u + sessionCounter;
            auto nextRnd = [&seed]() -> float {
                seed ^= seed << 13;
                seed ^= seed >> 17;
                seed ^= seed << 5;
                return (seed & 0x00FFFFFF) / static_cast<float>(0x01000000); // 0.0 to 1.0
            };

            // Organic touch micro-variations
            const float posJitter = (nextRnd() * 2.0f - 1.0f) * 0.015f * contactPos;
            const float effContactPos = std::clamp(contactPos + posJitter, 0.02f, 0.98f);

            const float hardJitter = (nextRnd() * 2.0f - 1.0f) * 0.020f * hardness;
            const float effHardness = std::clamp(hardness + hardJitter + compactionBoost, 0.02f, 0.98f);
            const float effClick = std::clamp(strikeClick + compactionBoost * 0.40f, 0.0f, 1.0f);

            // Subtle micro-detune: +/- 0.40 cents per note strike
            const float detuneCents = (nextRnd() * 2.0f - 1.0f) * 0.40f;
            noteDetuneRatio = std::pow(2.0f, detuneCents / 1200.0f);

            vibPhase = nextRnd() * 6.2831853f;
            driftPhase = nextRnd() * 6.2831853f;

            // Calculate MIDI note fundamental frequency in Hz with pitch bend and micro-detune
            const float baseFreq = 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
            const float freq = baseFreq * pitchBendFactor * noteDetuneRatio;

            // Setup exciter
            exciter.trigger(freq, velocity, excType, effHardness, effContactPos, stiffness,
                            bowSpeed, bowForce, effClick, rosinGrit,
                            particleDensity, particleScatter);
            exciter.setPitchBendFactor(pitchBendFactor);

            // Setup resonator (starts at initial bent frequency ratio, ready for dynamic real-time bend)
            resonator.configure(baseFreq, resType, decayTime, brightness, inharmonicity,
                                effContactPos, pickupPos, velocity,
                                pitchDropAmount, pitchDropDecay,
                                beatingBloom, materialBalance,
                                pitchBendFactor * noteDetuneRatio);

            // Setup pickup
            pickup.configure(pkType, pickupDrive, pickupTone);
        }

        void setPitchBendFactor(float factor) noexcept
        {
            currentPitchBendFactor = std::clamp(factor, 0.25f, 4.0f);
            exciter.setPitchBendFactor(currentPitchBendFactor);
        }

        void setModWheel(float mw) noexcept
        {
            modWheelAmount = std::clamp(mw, 0.0f, 1.0f);
        }

        void noteOff(float releaseVelocity = 0.5f, float damperFactor = 0.35f)
        {
            juce::ignoreUnused(releaseVelocity);
            if (!active) return;
            releasing = true;
            heldByPedal = false;
            exciter.release();
            // For bowed instruments, lifting the bow does not drop heavy felt dampers;
            // let the string resonate naturally with soft finger/string damping
            const float effDamper = (exciter.getType() == ExciterType::Bow)
                ? std::max(0.85f, damperFactor)
                : damperFactor;
            resonator.setDamperRelease(effDamper);
        }

        inline float processSample(float sympatheticForce = 0.0f)
        {
            if (!active) return 0.0f;

            age++;

            // Every 16 samples (~0.36ms): compute organic pitch drift and singing vibrato
            if ((age & 0x0F) == 0)
            {
                // Advance LFO phases (5.0 Hz vibrato, 0.35 Hz drift)
                vibPhase += static_cast<float>(2.0 * 3.141592653589793 * 5.0 * 16.0 / fs);
                if (vibPhase >= 6.2831853f) vibPhase -= 6.2831853f;

                driftPhase += static_cast<float>(2.0 * 3.141592653589793 * 0.35 * 16.0 / fs);
                if (driftPhase >= 6.2831853f) driftPhase -= 6.2831853f;

                // Subtle baseline organic thermal drift (+/- 0.8 cents)
                const float baselineDrift = std::sin(driftPhase) * 0.00045f;

                // Expressive Mod Wheel vibrato (up to +/- 30 cents at full mod wheel)
                const float mwVibrato = std::sin(vibPhase) * (0.018f * modWheelAmount);

                const float totalRatio = currentPitchBendFactor * noteDetuneRatio * (1.0f + baselineDrift + mwVibrato);
                resonator.setRealtimePitchBendFactor(totalRatio);
                exciter.setPitchBendFactor(totalRatio);
            }

            // 1. Compute exciter contact force F_c
            const float exciterForce = exciter.processSample(contactVelocityFeedback);

            // 2. Drive modal resonator bank with exciter force and sympathetic bridge coupling
            const auto resOut = resonator.processSample(exciterForce, sympatheticForce);
            contactVelocityFeedback = resOut.contactVelocity;

            // 3. Transduce resonator pickup displacement through pickup model
            const float pickupSignal = pickup.processSample(resOut.pickupSignal);

            // 4. Check for voice termination (smooth de-clicking fadeout to silence)
            if (releasing)
            {
                releaseCounter++;
                if (isDeclicking)
                {
                    declickSamplesLeft--;
                    fadeGain = 0.5f * (1.0f + std::cos(3.14159265f * static_cast<float>(64 - declickSamplesLeft) / 64.0f));
                    if (declickSamplesLeft <= 0)
                    {
                        active = false;
                        isDeclicking = false;
                        fadeGain = 1.0f;
                        return 0.0f;
                    }
                }
                else if (releaseCounter > 200 && resonator.getTotalEnergy() < 0.00003f && !exciter.isActive())
                {
                    isDeclicking = true;
                    declickSamplesLeft = 64;
                }
            }

            return pickupSignal * fadeGain;
        }

        bool isActive() const noexcept { return active; }
        bool isReleasing() const noexcept { return releasing; }
        bool isHeldByPedal() const noexcept { return heldByPedal; }
        void setHeldByPedal(bool held) noexcept { heldByPedal = held; }
        int getMidiNote() const noexcept { return midiNote; }
        uint64_t getAge() const noexcept { return age; }
        float getEnergy() const noexcept { return resonator.getTotalEnergy(); }
        float getContactVelocity() const noexcept { return active ? contactVelocityFeedback : 0.0f; }

        const ExciterModel& getExciter() const noexcept { return exciter; }
        const ResonatorModel& getResonator() const noexcept { return resonator; }

    private:
        double fs = 44100.0;
        bool active = false;
        bool releasing = false;
        bool heldByPedal = false;
        bool isDeclicking = false;
        int declickSamplesLeft = 0;
        float fadeGain = 1.0f;
        int midiNote = -1;
        float velocity = 0.0f;
        uint64_t age = 0;
        int releaseCounter = 0;
        float contactVelocityFeedback = 0.0f;

        float currentPitchBendFactor = 1.0f;
        float noteDetuneRatio = 1.0f;
        float modWheelAmount = 0.0f;
        float vibPhase = 0.0f;
        float driftPhase = 0.0f;

        ExciterModel exciter;
        ResonatorModel resonator;
        PickupModel pickup;
    };
}
