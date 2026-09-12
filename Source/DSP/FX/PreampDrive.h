#pragma once

#include "../PhysicalConstants.h"
#include <cmath>
#include <algorithm>

namespace ModelKeys
{
    class PreampDrive
    {
    public:
        PreampDrive() = default;

        void prepare(double sampleRate)
        {
            fs = sampleRate > 0.0 ? sampleRate : 44100.0;
            reset();
        }

        void reset()
        {
            lpStateL = 0.0f; lpStateR = 0.0f;
            bumpS1_L = 0.0f; bumpS2_L = 0.0f;
            bumpS1_R = 0.0f; bumpS2_R = 0.0f;
            dcX_L = 0.0f; dcY_L = 0.0f;
            dcX_R = 0.0f; dcY_R = 0.0f;
        }

        void configure(PreampType type, float driveAmount, float toneAmount, float outLevel, bool enabled = true)
        {
            isEnabled = enabled;
            preampType = type;
            drive = std::clamp(driveAmount, 0.0f, 1.0f);
            tone = std::clamp(toneAmount, 0.0f, 1.0f);
            level = std::clamp(outLevel, 0.0f, 2.0f);

            // Dynamic drive gain: 1.0 (0 dB clean) up to 8.0 (+18.1 dB full overdrive)
            gain = 1.0f + 7.0f * (drive * std::sqrt(drive));

            // Tone low-pass filter: 1.5 kHz to 16 kHz
            const float cutoff = 1500.0f + 14500.0f * (tone * tone);
            const float w0 = 2.0f * 3.14159265f * cutoff / static_cast<float>(fs);
            toneCoeff = std::clamp(w0 / (1.0f + w0), 0.02f, 0.98f);

            // EQ Filter configuration (Tube Cathode Warmth vs Tape Head Bump)
            if (preampType == PreampType::Tape && drive > 0.01f)
            {
                // Tape Head Bump: +3.5 dB peaking boost at 68 Hz (Q=1.3)
                const float bumpW0 = 2.0f * 3.14159265f * 68.0f / static_cast<float>(fs);
                const float A = std::pow(10.0f, (3.5f * drive) / 40.0f);
                const float alpha = std::sin(bumpW0) / (2.0f * 1.3f);
                const float a0 = 1.0f + alpha / A;
                bump_b0 = (1.0f + alpha * A) / a0;
                bump_b1 = (-2.0f * std::cos(bumpW0)) / a0;
                bump_b2 = (1.0f - alpha * A) / a0;
                bump_a1 = (-2.0f * std::cos(bumpW0)) / a0;
                bump_a2 = (1.0f - alpha / A) / a0;
            }
            else if (preampType == PreampType::Tube && drive > 0.01f)
            {
                // Tube Cathode Warmth: +2.5 dB low shelf at 110 Hz
                const float shelfW0 = 2.0f * 3.14159265f * 110.0f / static_cast<float>(fs);
                const float A = std::pow(10.0f, (2.5f * drive) / 40.0f);
                const float cosW = std::cos(shelfW0);
                const float sinW = std::sin(shelfW0);
                const float alpha = sinW / (2.0f * 0.707f);
                const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
                const float a0 = (A + 1.0f) + (A - 1.0f) * cosW + twoSqrtAAlpha;
                bump_b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosW + twoSqrtAAlpha)) / a0;
                bump_b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW)) / a0;
                bump_b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosW - twoSqrtAAlpha)) / a0;
                bump_a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosW)) / a0;
                bump_a2 = ((A + 1.0f) + (A - 1.0f) * cosW - twoSqrtAAlpha) / a0;
            }
            else
            {
                bump_b0 = 1.0f; bump_b1 = 0.0f; bump_b2 = 0.0f;
                bump_a1 = 0.0f; bump_a2 = 0.0f;
            }
        }

        void configure(float driveAmount, float toneAmount, float outLevel, bool enabled = true)
        {
            configure(preampType, driveAmount, toneAmount, outLevel, enabled);
        }

        void process(float& sampleL, float& sampleR)
        {
            // Clean / Bypass or zero drive: 100% linear transparent pass-through
            if (!isEnabled || preampType == PreampType::Clean || drive <= 0.001f)
            {
                sampleL *= level;
                sampleR *= level;
                return;
            }

            sampleL = processChannel(sampleL, lpStateL, bumpS1_L, bumpS2_L, dcX_L, dcY_L);
            sampleR = processChannel(sampleR, lpStateR, bumpS1_R, bumpS2_R, dcX_R, dcY_R);
        }

    private:
        inline float processChannel(float in, float& lpState, float& bS1, float& bS2, float& dcX, float& dcY)
        {
            // 1. Coloration EQ: Cathode low shelf (Tube) or Head Bump (Tape)
            const float eqIn = in;
            const float eqOut = bump_b0 * eqIn + bS1;
            bS1 = bump_b1 * eqIn - bump_a1 * eqOut + bS2;
            bS2 = bump_b2 * eqIn - bump_a2 * eqOut;

            // 2. Drive amplification
            const float x = eqOut * gain;

            float sat = 0.0f;

            if (preampType == PreampType::Tube)
            {
                // Class-A Triode 12AX7 transfer curve:
                // Physical negative grid bias produces prominent 2nd-harmonic warmth (H2 ~ -21 dBc)
                // C-infinity smooth, zero division, zero DC offset at rest
                const float gridBias = 0.40f * drive;
                sat = std::tanh(x + gridBias) - std::tanh(gridBias);
            }
            else // PreampType::Tape
            {
                // Magnetic Tape Hysteresis / Sigmoid Saturation:
                // Symmetrical odd-harmonic compression (H3 ~ -14 dBc) that glues dynamics
                sat = x / std::sqrt(1.0f + x * x);
            }

            // 3. Post-saturation tone filter (1-pole)
            lpState += toneCoeff * (sat - lpState);
            const float filtered = lpState;

            // 4. DC Blocker (~5 Hz) to eliminate any asymmetric tube bias offset
            const float dcOut = filtered - dcX + 0.995f * dcY;
            dcX = filtered;
            dcY = dcOut;

            // 5. Auto-gain compensation and master level scaling
            const float autoGain = 1.0f / std::sqrt(gain);
            return dcOut * (level * autoGain);
        }

        double fs = 44100.0;
        bool isEnabled = true;
        PreampType preampType = PreampType::Clean;
        float drive = 0.15f;
        float tone = 0.6f;
        float level = 1.0f;
        float gain = 1.0f;
        float toneCoeff = 0.5f;

        float bump_b0 = 1.0f, bump_b1 = 0.0f, bump_b2 = 0.0f;
        float bump_a1 = 0.0f, bump_a2 = 0.0f;

        float lpStateL = 0.0f, lpStateR = 0.0f;
        float bumpS1_L = 0.0f, bumpS2_L = 0.0f;
        float bumpS1_R = 0.0f, bumpS2_R = 0.0f;
        float dcX_L = 0.0f, dcY_L = 0.0f;
        float dcX_R = 0.0f, dcY_R = 0.0f;
    };
}
