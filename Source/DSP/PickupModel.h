#pragma once

#include "PhysicalConstants.h"
#include <cmath>
#include <algorithm>

namespace ModelKeys
{
    class PickupModel
    {
    public:
        PickupModel() = default;

        void prepare(double sampleRate)
        {
            fs = sampleRate > 0.0 ? sampleRate : 44100.0;
            reset();
        }

        void reset()
        {
            biquadS1 = 0.0f;
            biquadS2 = 0.0f;
            biquad2S1 = 0.0f;
            biquad2S2 = 0.0f;
            dcBlockStateX = 0.0f;
            dcBlockStateY = 0.0f;
        }

        void configure(PickupType type, float drive, float tone)
        {
            pickupType = type;
            driveParam = std::clamp(drive, 0.0f, 1.0f);
            toneParam = std::clamp(tone, 0.0f, 1.0f);
            hasSecondFilter = false;

            const float nyquist = static_cast<float>(fs * 0.49);

            if (pickupType == PickupType::Electromagnetic)
            {
                // Authentic vintage Rhodes inductive coil impedance (12 dB/oct rolloff)
                const float lpCutoffHz = std::clamp(1600.0f + 4200.0f * (toneParam * toneParam), 200.0f, nyquist);
                const float q = 0.85f;
                setupLowpass(lpCutoffHz, q);
            }
            else if (pickupType == PickupType::Electrostatic)
            {
                // Vintage Wurlitzer 200A preamp EQ: wide bandwidth with mid presence (peaking around 2.2 kHz)
                const float lpCutoffHz = std::clamp(6500.0f + 5500.0f * toneParam, 1000.0f, nyquist);
                const float q = 1.15f;
                setupLowpass(lpCutoffHz, q);
            }
            else if (pickupType == PickupType::ClavinetDualCoil)
            {
                // Clavinet D6 dual single-coil out-of-phase comb notch filter (~1450 Hz notch)
                const float notchFreq = std::clamp(1100.0f + 700.0f * toneParam, 400.0f, nyquist);
                setupNotch(notchFreq, 1.6f);
                // Out-of-phase magnetic cancellation chokes ultra-low bass while preserving fundamental punch (~5.5% bass)
                setupHighpass(115.0f, 0.707f);
            }
            else if (pickupType == PickupType::Microphone)
            {
                // Acoustic studio microphone: natural air rolloff (5.5 kHz to 14 kHz)
                const float lpCutoffHz = std::clamp(5500.0f + 8500.0f * (toneParam * toneParam), 1500.0f, nyquist);
                setupLowpass(lpCutoffHz, 0.707f);
            }
            else // Piezo
            {
                // Piezo contact: ultra wideband crisp transparency
                setupLowpass(std::min(18000.0f, nyquist), 0.707f);
            }
        }

        float processSample(float input)
        {
            // 1. Sub-bass preserving DC Blocker (cutoff ~3.5 Hz to retain deep piano bass down to A0=27.5 Hz)
            const float dcIn = input;
            const float dcOut = dcIn - dcBlockStateX + 0.9995f * dcBlockStateY;
            dcBlockStateX = dcIn;
            dcBlockStateY = dcOut;

            float signal = dcOut;

            switch (pickupType)
            {
                case PickupType::Electromagnetic:
                {
                    // Authentic magnetic proximity saturation:
                    // As tine tip approaches the magnet pole piece, flux increases non-linearly (2nd harmonic bark & growl)
                    const float driveGain = 1.0f + 2.4f * driveParam;
                    const float x = signal * driveGain;

                    const float asym = 0.40f * driveParam;
                    const float num = x + asym * x * std::abs(x);
                    const float den = 1.0f + asym * std::abs(x) + 0.65f * x * x;
                    signal = num / den;
                    break;
                }

                case PickupType::Electrostatic:
                {
                    // Wurlitzer capacitive reed-to-plate electrostatic modulation:
                    // V(x) ~ x / (1 - k*x), producing biting odd+even reed quack and bark
                    const float driveGain = 1.0f + 2.2f * driveParam;
                    const float x = signal * driveGain;
                    const float k = 0.42f * driveParam;
                    const float den = std::max(0.18f, 1.0f - k * x);
                    signal = std::tanh((x / den) * 0.85f);
                    break;
                }

                case PickupType::ClavinetDualCoil:
                {
                    // Clavinet dual single-coil magnetic bite with vintage high-gain overdrive
                    const float clavDrive = 1.0f + 3.0f * driveParam;
                    const float x = signal * clavDrive;
                    signal = std::tanh(x + 0.25f * driveParam * x * std::abs(x));
                    break;
                }

                case PickupType::Piezo:
                {
                    // Piezo: crisp, linear transducer with wide dynamic headroom
                    const float piezoDrive = 1.0f + driveParam * 0.8f;
                    signal = std::tanh(signal * piezoDrive);
                    break;
                }

                case PickupType::Microphone:
                {
                    // Microphone: natural acoustic roll-off and gentle warmth
                    const float micDrive = 1.0f + driveParam * 0.5f;
                    signal = std::tanh(signal * micDrive);
                    break;
                }
            }

            // 2. 2nd-order Direct Form II Transposed primary filter
            float filtered = b0 * signal + biquadS1;
            biquadS1 = b1 * signal - a1 * filtered + biquadS2;
            biquadS2 = b2 * signal - a2 * filtered;

            // Optional 2nd filter stage (e.g. Clavinet out-of-phase low-cut)
            if (hasSecondFilter)
            {
                const float in2 = filtered;
                filtered = b0_2 * in2 + biquad2S1;
                biquad2S1 = b1_2 * in2 - a1_2 * filtered + biquad2S2;
                biquad2S2 = b2_2 * in2 - a2_2 * filtered;
            }

            return filtered;
        }

    private:
        void setupLowpass(float fc, float q)
        {
            const float w0 = 2.0f * 3.14159265f * fc / static_cast<float>(fs);
            const float cosW0 = std::cos(w0);
            const float sinW0 = std::sin(w0);
            const float alpha = sinW0 / (2.0f * std::max(0.05f, q));

            const float b0_raw = (1.0f - cosW0) * 0.5f;
            const float b1_raw = 1.0f - cosW0;
            const float b2_raw = (1.0f - cosW0) * 0.5f;
            const float a0_raw = 1.0f + alpha;
            const float a1_raw = -2.0f * cosW0;
            const float a2_raw = 1.0f - alpha;

            const float invA0 = 1.0f / a0_raw;
            b0 = b0_raw * invA0;
            b1 = b1_raw * invA0;
            b2 = b2_raw * invA0;
            a1 = a1_raw * invA0;
            a2 = a2_raw * invA0;
        }

        void setupNotch(float fc, float q)
        {
            const float w0 = 2.0f * 3.14159265f * fc / static_cast<float>(fs);
            const float cosW0 = std::cos(w0);
            const float sinW0 = std::sin(w0);
            const float alpha = sinW0 / (2.0f * std::max(0.05f, q));

            const float b0_raw = 1.0f;
            const float b1_raw = -2.0f * cosW0;
            const float b2_raw = 1.0f;
            const float a0_raw = 1.0f + alpha;
            const float a1_raw = -2.0f * cosW0;
            const float a2_raw = 1.0f - alpha;

            const float invA0 = 1.0f / a0_raw;
            b0 = b0_raw * invA0;
            b1 = b1_raw * invA0;
            b2 = b2_raw * invA0;
            a1 = a1_raw * invA0;
            a2 = a2_raw * invA0;
        }

        void setupHighpass(float fc, float q)
        {
            const float w0 = 2.0f * 3.14159265f * fc / static_cast<float>(fs);
            const float cosW0 = std::cos(w0);
            const float sinW0 = std::sin(w0);
            const float alpha = sinW0 / (2.0f * std::max(0.05f, q));

            const float b0_raw = (1.0f + cosW0) * 0.5f;
            const float b1_raw = -(1.0f + cosW0);
            const float b2_raw = (1.0f + cosW0) * 0.5f;
            const float a0_raw = 1.0f + alpha;
            const float a1_raw = -2.0f * cosW0;
            const float a2_raw = 1.0f - alpha;

            const float invA0 = 1.0f / a0_raw;
            b0_2 = b0_raw * invA0;
            b1_2 = b1_raw * invA0;
            b2_2 = b2_raw * invA0;
            a1_2 = a1_raw * invA0;
            a2_2 = a2_raw * invA0;
            hasSecondFilter = true;
        }

        double fs = 44100.0;
        PickupType pickupType = PickupType::Electromagnetic;
        float driveParam = 0.3f;
        float toneParam = 0.5f;

        // Biquad 2nd-order primary filter coefficients
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float biquadS1 = 0.0f;
        float biquadS2 = 0.0f;

        // Secondary biquad filter (e.g. Clavinet phase cancellation highpass)
        bool hasSecondFilter = false;
        float b0_2 = 1.0f, b1_2 = 0.0f, b2_2 = 0.0f;
        float a1_2 = 0.0f, a2_2 = 0.0f;
        float biquad2S1 = 0.0f;
        float biquad2S2 = 0.0f;

        float dcBlockStateX = 0.0f;
        float dcBlockStateY = 0.0f;
    };
}
