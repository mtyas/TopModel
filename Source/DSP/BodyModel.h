#pragma once

#include "PhysicalConstants.h"
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>

namespace ModelKeys
{
    class BodyModel
    {
    public:
        struct Biquad
        {
            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
            float a1 = 0.0f, a2 = 0.0f;
            float z1 = 0.0f, z2 = 0.0f;

            void reset()
            {
                z1 = 0.0f;
                z2 = 0.0f;
            }

            inline float process(float in)
            {
                const float out = in * b0 + z1;
                z1 = in * b1 - a1 * out + z2;
                z2 = in * b2 - a2 * out;
                return out;
            }

            void setPeak(float sampleRate, float freqHz, float q, float gainDb)
            {
                const float clampedFreq = std::clamp(freqHz, 20.0f, static_cast<float>(sampleRate * 0.45));
                const float w0 = 2.0f * 3.14159265f * clampedFreq / static_cast<float>(sampleRate);
                const float alpha = std::sin(w0) / (2.0f * std::max(0.1f, q));
                const float A = std::pow(10.0f, gainDb / 40.0f);

                const float a0_inv = 1.0f / (1.0f + alpha / A);
                b0 = (1.0f + alpha * A) * a0_inv;
                b1 = (-2.0f * std::cos(w0)) * a0_inv;
                b2 = (1.0f - alpha * A) * a0_inv;
                a1 = (-2.0f * std::cos(w0)) * a0_inv;
                a2 = (1.0f - alpha / A) * a0_inv;
            }

            void setLowShelf(float sampleRate, float freqHz, float q, float gainDb)
            {
                const float clampedFreq = std::clamp(freqHz, 20.0f, static_cast<float>(sampleRate * 0.45));
                const float w0 = 2.0f * 3.14159265f * clampedFreq / static_cast<float>(sampleRate);
                const float A = std::pow(10.0f, gainDb / 40.0f);
                const float cosW = std::cos(w0);
                const float sinW = std::sin(w0);
                const float alpha = sinW / (2.0f * std::max(0.1f, q));
                const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;

                const float a0 = (A + 1.0f) + (A - 1.0f) * cosW + twoSqrtAAlpha;
                const float a0_inv = 1.0f / a0;

                b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosW + twoSqrtAAlpha)) * a0_inv;
                b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW)) * a0_inv;
                b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosW - twoSqrtAAlpha)) * a0_inv;
                a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosW)) * a0_inv;
                a2 = ((A + 1.0f) + (A - 1.0f) * cosW - twoSqrtAAlpha) * a0_inv;
            }

            void setHighShelf(float sampleRate, float freqHz, float q, float gainDb)
            {
                const float clampedFreq = std::clamp(freqHz, 20.0f, static_cast<float>(sampleRate * 0.45));
                const float w0 = 2.0f * 3.14159265f * clampedFreq / static_cast<float>(sampleRate);
                const float A = std::pow(10.0f, gainDb / 40.0f);
                const float cosW = std::cos(w0);
                const float sinW = std::sin(w0);
                const float alpha = sinW / (2.0f * std::max(0.1f, q));
                const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;

                const float a0 = (A + 1.0f) - (A - 1.0f) * cosW + twoSqrtAAlpha;
                const float a0_inv = 1.0f / a0;

                b0 = (A * ((A + 1.0f) + (A - 1.0f) * cosW + twoSqrtAAlpha)) * a0_inv;
                b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW)) * a0_inv;
                b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosW - twoSqrtAAlpha)) * a0_inv;
                a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosW)) * a0_inv;
                a2 = ((A + 1.0f) - (A - 1.0f) * cosW - twoSqrtAAlpha) * a0_inv;
            }
        };

        // Allpass delay element for soundboard plate dispersion & cavity diffusion
        struct AllpassDelay
        {
            std::vector<float> buffer;
            int writeIndex = 0;
            int delaySamples = 100;
            float g = 0.35f;

            void prepare(int delay)
            {
                delaySamples = std::max(1, delay);
                buffer.assign(delaySamples, 0.0f);
                writeIndex = 0;
            }

            void reset()
            {
                std::fill(buffer.begin(), buffer.end(), 0.0f);
                writeIndex = 0;
            }

            inline float process(float in)
            {
                if (buffer.empty()) return in;
                const float bufOut = buffer[writeIndex];
                const float out = -g * in + bufOut;
                buffer[writeIndex] = in + g * out;
                if (++writeIndex >= delaySamples)
                    writeIndex = 0;
                return out;
            }
        };

        BodyModel() = default;

        void prepare(double sampleRate)
        {
            fs = sampleRate > 0.0 ? sampleRate : 44100.0;
            const double srRatio = fs / 44100.0;

            // Prime delay lengths for natural plate reflection diffusion
            ap1.prepare(std::max(2, static_cast<int>(79 * srRatio)));
            ap2.prepare(std::max(2, static_cast<int>(139 * srRatio)));
            ap3.prepare(std::max(2, static_cast<int>(239 * srRatio)));
            ap4.prepare(std::max(2, static_cast<int>(379 * srRatio)));

            reset();
        }

        void reset()
        {
            for (auto& f : filters)
                f.reset();
            ap1.reset();
            ap2.reset();
            ap3.reset();
            ap4.reset();
        }

        void configure(BodyType type, float bodySize, float bodyResonance, float mixAmount)
        {
            currentType = type;
            sizeFactor = std::clamp(bodySize, 0.5f, 2.0f);
            resonance = std::clamp(bodyResonance, 0.1f, 1.0f);
            mix = std::clamp(mixAmount, 0.0f, 1.0f);

            // Update diffusion feedback gain based on resonance
            const float diffGain = std::clamp(0.20f + 0.38f * resonance, 0.15f, 0.60f);
            ap1.g = diffGain;
            ap2.g = diffGain * 0.92f;
            ap3.g = diffGain * 0.85f;
            ap4.g = diffGain * 0.78f;

            updateFilters();
        }

        float processSample(float input)
        {
            if (currentType == BodyType::Off || mix <= 0.001f)
                return input;

            // 1. Soundboard plate dispersion / body cavity diffusion
            float diffused = input;
            if (useDiffuser)
            {
                diffused = ap1.process(diffused);
                diffused = ap2.process(diffused);
                diffused = ap3.process(diffused);
                diffused = ap4.process(diffused);
            }

            // Blend direct bridge drive with diffused soundboard bloom
            const float plateBlend = 0.58f * input + 0.42f * diffused;

            // 2. Shape through broad physical acoustic formants
            float filtered = plateBlend;
            for (int i = 0; i < numActiveFilters; ++i)
            {
                filtered = filters[i].process(filtered);
            }

            // Apply acoustic radiation gain compensation
            const float compensated = filtered * gainComp;

            // Mix dry bridge string signal with radiated wooden body
            return (1.0f - mix) * input + mix * compensated;
        }

    private:
        void updateFilters()
        {
            const float freqScale = 1.0f / sizeFactor;
            // Broad, musical acoustic Q factor (Q ~ 1.2 to 2.2): covers full octaves
            const float qBase = 1.0f + 1.4f * resonance;
            const float resFactor = 0.5f + 0.5f * resonance;

            useDiffuser = true;

            switch (currentType)
            {
                case BodyType::Off:
                    numActiveFilters = 0;
                    gainComp = 1.0f;
                    useDiffuser = false;
                    break;

                case BodyType::AcousticGuitar:
                    numActiveFilters = 4;
                    // Helmholtz air cavity resonance (102 Hz) gives deep acoustic body thump
                    filters[0].setPeak(static_cast<float>(fs), 102.0f * freqScale, qBase * 1.5f, 6.8f * resFactor);
                    // Spruce top soundboard breathing mode (208 Hz)
                    filters[1].setPeak(static_cast<float>(fs), 208.0f * freqScale, qBase * 1.3f, 5.8f * resFactor);
                    // Rosewood / Mahogany back plate resonance (315 Hz)
                    filters[2].setPeak(static_cast<float>(fs), 315.0f * freqScale, qBase * 1.2f, 4.0f * resFactor);
                    // Upper spruce top radiation resonance (2800 Hz) adds crisp acoustic ring & pick sparkle
                    filters[3].setPeak(static_cast<float>(fs), 2800.0f * freqScale, qBase * 1.0f, 3.2f * resFactor);
                    gainComp = 0.72f;
                    break;

                case BodyType::PianoSoundboard:
                    numActiveFilters = 5;
                    // Steinway D Rim mass & cavity foundation mode (75 Hz): deep authoritative grand piano bass
                    filters[0].setPeak(static_cast<float>(fs), 75.0f * freqScale, qBase * 1.5f, 6.5f * resFactor);
                    // Spruce soundboard belly breathing fundamental (145 Hz): rich warm acoustic bloom
                    filters[1].setPeak(static_cast<float>(fs), 145.0f * freqScale, qBase * 1.3f, 5.5f * resFactor);
                    // Bridge mechanical mobility & singing midrange formant (340 Hz): singing trichord projection
                    filters[2].setPeak(static_cast<float>(fs), 340.0f * freqScale, qBase * 1.2f, 3.5f * resFactor);
                    // Upper bridge radiation formant (1800 Hz): clear acoustic projection
                    filters[3].setPeak(static_cast<float>(fs), 1800.0f * freqScale, qBase * 1.1f, 2.5f * resFactor);
                    // High spruce internal damping shelf (>3.4 kHz): tames harsh metallic clatter
                    filters[4].setHighShelf(static_cast<float>(fs), 3400.0f, 0.707f, -4.5f * resFactor);
                    gainComp = 0.78f;
                    break;

                case BodyType::ViolinBody:
                    numActiveFilters = 5;
                    if (sizeFactor >= 1.25f)
                    {
                        // Cello body acoustics:
                        // A0 Helmholtz air mode (~105 Hz) gives deep bowed body foundation
                        filters[0].setPeak(static_cast<float>(fs), 105.0f * freqScale, qBase * 1.4f, 6.0f * resFactor);
                        // B1- lower wood plate mode (~165 Hz)
                        filters[1].setPeak(static_cast<float>(fs), 165.0f * freqScale, qBase * 1.3f, 5.5f * resFactor);
                        // B1+ upper wood plate mode (~230 Hz)
                        filters[2].setPeak(static_cast<float>(fs), 230.0f * freqScale, qBase * 1.2f, 4.5f * resFactor);
                        // Bridge Hill radiation formant (~2800 Hz): singing acoustic projection matching Synful Orchestra
                        filters[3].setPeak(static_cast<float>(fs), 2800.0f, qBase * 1.3f, 5.5f * resFactor);
                        // Upper air & wood plate absorption: gentle rolloff preserving string presence and bow harmonics
                        filters[4].setHighShelf(static_cast<float>(fs), 6800.0f, 0.707f, -3.5f * resFactor);
                    }
                    else
                    {
                        // Violin body acoustics:
                        // A0 Helmholtz air mode (~280 Hz)
                        filters[0].setPeak(static_cast<float>(fs), 280.0f * freqScale, qBase * 1.3f, 5.5f * resFactor);
                        // B1- wood mode (~440 Hz)
                        filters[1].setPeak(static_cast<float>(fs), 440.0f * freqScale, qBase * 1.2f, 5.0f * resFactor);
                        // B1+ wood mode (~550 Hz)
                        filters[2].setPeak(static_cast<float>(fs), 550.0f * freqScale, qBase * 1.2f, 4.0f * resFactor);
                        // Bridge Hill radiation formant (~3200 Hz)
                        filters[3].setPeak(static_cast<float>(fs), 3200.0f, qBase * 1.4f, 5.0f * resFactor);
                        // Upper air & wood absorption (>7.5 kHz)
                        filters[4].setHighShelf(static_cast<float>(fs), 7500.0f, 0.707f, -4.0f * resFactor);
                    }
                    gainComp = 0.72f;
                    break;

                case BodyType::RhodesTonebar:
                    numActiveFilters = 4;
                    // Lower tonebar mass resonance: warm, solid 180 Hz foundation
                    filters[0].setPeak(static_cast<float>(fs), 180.0f * freqScale, qBase * 1.3f, 4.5f * resFactor);
                    // Central tonebar resonant belly (400 Hz): singing body presence
                    filters[1].setPeak(static_cast<float>(fs), 400.0f * freqScale, qBase * 1.3f, 4.5f * resFactor);
                    // Bell chime presence (1800 Hz): subtle tine definition
                    filters[2].setPeak(static_cast<float>(fs), 1800.0f * freqScale, qBase * 1.1f, 2.0f * resFactor);
                    // Inductive coil absorption above 2.8 kHz: smooth vintage warmth
                    filters[3].setHighShelf(static_cast<float>(fs), 2800.0f * freqScale, 0.707f, -6.5f * resFactor);
                    gainComp = 0.72f;
                    break;

                case BodyType::DrumShell:
                    numActiveFilters = 3;
                    filters[0].setPeak(static_cast<float>(fs), 160.0f * freqScale, qBase * 1.5f, 5.5f * resFactor);
                    filters[1].setPeak(static_cast<float>(fs), 320.0f * freqScale, qBase * 1.3f, 4.0f * resFactor);
                    filters[2].setPeak(static_cast<float>(fs), 800.0f * freqScale, qBase * 1.0f, 3.0f * resFactor);
                    gainComp = 0.60f;
                    break;

                case BodyType::WurliReedBar:
                    numActiveFilters = 4;
                    // Wurlitzer 200A cast zinc reed chassis warmth
                    filters[0].setPeak(static_cast<float>(fs), 220.0f * freqScale, qBase * 1.3f, 4.5f * resFactor);
                    // Distinct hollow reed bar nasal formant (540 Hz)
                    filters[1].setPeak(static_cast<float>(fs), 540.0f * freqScale, qBase * 1.3f, 6.0f * resFactor);
                    // Biting reed presence & electrostatic quack (2100 Hz)
                    filters[2].setPeak(static_cast<float>(fs), 2100.0f * freqScale, qBase * 1.2f, 5.0f * resFactor);
                    // Cabinet top absorption
                    filters[3].setHighShelf(static_cast<float>(fs), 4800.0f * freqScale, 0.707f, -5.0f * resFactor);
                    gainComp = 0.68f;
                    break;

                case BodyType::HarpSoundbox:
                    numActiveFilters = 4;
                    // Concert harp conical wooden soundbox belly (150 Hz): warm acoustic bloom
                    filters[0].setPeak(static_cast<float>(fs), 150.0f * freqScale, qBase * 1.3f, 7.0f * resFactor);
                    // Soundboard rib resonance (320 Hz)
                    filters[1].setPeak(static_cast<float>(fs), 320.0f * freqScale, qBase * 1.4f, 5.0f * resFactor);
                    // Column pillar acoustic cavity (620 Hz)
                    filters[2].setPeak(static_cast<float>(fs), 620.0f * freqScale, qBase * 1.3f, 3.0f * resFactor);
                    // High gut/wood damping shelf above 3.2 kHz: eliminates guitar-like twang, matching Pianoteq centroid
                    filters[3].setHighShelf(static_cast<float>(fs), 3200.0f * freqScale, 0.707f, -6.5f * resFactor);
                    gainComp = 0.75f;
                    break;

                case BodyType::MarimbaResonator:
                    numActiveFilters = 4;
                    // Sub-bass roll-off filter
                    filters[0].setLowShelf(static_cast<float>(fs), 140.0f * freqScale, 0.707f, -8.0f * resFactor);
                    // Quarter-wave tubular air column resonances (tuned pipe boost at 480 Hz & 720 Hz)
                    filters[1].setPeak(static_cast<float>(fs), 480.0f * freqScale, qBase * 1.5f, 6.5f * resFactor);
                    filters[2].setPeak(static_cast<float>(fs), 720.0f * freqScale, qBase * 1.4f, 5.5f * resFactor);
                    // Upper tubular absorption
                    filters[3].setHighShelf(static_cast<float>(fs), 2800.0f * freqScale, 0.707f, -15.0f * resFactor);
                    gainComp = 0.70f;
                    break;

                case BodyType::SteelDrumBarrel:
                    numActiveFilters = 4;
                    // 55-gallon steel skirt cavity Helmholtz resonance
                    filters[0].setPeak(static_cast<float>(fs), 260.0f * freqScale, qBase * 1.4f, 4.5f * resFactor);
                    // Steel barrel rim ring
                    filters[1].setPeak(static_cast<float>(fs), 650.0f * freqScale, qBase * 1.3f, 4.5f * resFactor);
                    // Metallic dish harmonic projection
                    filters[2].setPeak(static_cast<float>(fs), 1400.0f * freqScale, qBase * 1.2f, 4.0f * resFactor);
                    // High metallic ping & sizzle
                    filters[3].setPeak(static_cast<float>(fs), 4800.0f * freqScale, 1.2f, 5.0f * resFactor);
                    gainComp = 0.65f;
                    break;
            }
        }

        double fs = 44100.0;
        BodyType currentType = BodyType::AcousticGuitar;
        float sizeFactor = 1.0f;
        float resonance = 0.5f;
        float mix = 0.7f;
        float gainComp = 1.0f;
        int numActiveFilters = 4;
        bool useDiffuser = true;

        std::array<Biquad, 6> filters;
        AllpassDelay ap1;
        AllpassDelay ap2;
        AllpassDelay ap3;
        AllpassDelay ap4;
    };
}
