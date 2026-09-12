#pragma once

#include "../PhysicalConstants.h"
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>

namespace ModelKeys
{
    class ChorusTremolo
    {
    public:
        using Mode = ModulationType;

        ChorusTremolo() = default;

        void prepare(double sampleRate)
        {
            fs = sampleRate > 0.0 ? sampleRate : 44100.0;
            // Allocate delay buffer for chorus/flanger/delay (up to 1.0s max delay)
            const size_t maxDelaySamples = static_cast<size_t>(1.2 * fs) + 64;
            bufferL.assign(maxDelaySamples, 0.0f);
            bufferR.assign(maxDelaySamples, 0.0f);
            writeIndex = 0;
            lfoPhase = 0.0f;
            lfoFastPhase = 0.0f;
            wowPhase = 0.0f;
            flutterPhase = 0.0f;

            for (auto& s : phaserStagesL) s = 0.0f;
            for (auto& s : phaserStagesR) s = 0.0f;
            flangerFeedbackL = 0.0f;
            flangerFeedbackR = 0.0f;
            delayFeedbackL = 0.0f;
            delayFeedbackR = 0.0f;
            delayDampL = 0.0f;
            delayDampR = 0.0f;
            tapeDampL = 0.0f;
            tapeDampR = 0.0f;
        }

        void reset()
        {
            std::fill(bufferL.begin(), bufferL.end(), 0.0f);
            std::fill(bufferR.begin(), bufferR.end(), 0.0f);
            writeIndex = 0;
            lfoPhase = 0.0f;
            lfoFastPhase = 0.0f;
            wowPhase = 0.0f;
            flutterPhase = 0.0f;

            for (auto& s : phaserStagesL) s = 0.0f;
            for (auto& s : phaserStagesR) s = 0.0f;
            flangerFeedbackL = 0.0f;
            flangerFeedbackR = 0.0f;
            delayFeedbackL = 0.0f;
            delayFeedbackR = 0.0f;
            delayDampL = 0.0f;
            delayDampR = 0.0f;
            tapeDampL = 0.0f;
            tapeDampR = 0.0f;
        }

        void configure(Mode fxMode, float rateParam, float depthParamVal, float mixAmount, bool enabled = true)
        {
            isEnabled = enabled;
            mode = fxMode;
            rate = std::clamp(rateParam, 0.05f, 12.0f);
            depthParam = std::clamp(depthParamVal, 0.0f, 1.0f);
            mix = std::clamp(mixAmount, 0.0f, 1.0f);

            lfoInc = static_cast<float>(2.0 * 3.141592653589793 * rate / fs);
            // Ensemble fast LFO (~6.0 Hz)
            lfoFastInc = static_cast<float>(2.0 * 3.141592653589793 * (rate * 4.5f + 1.0f) / fs);
            // Wow (~0.6 Hz) and Flutter (~7.5 Hz)
            wowInc = static_cast<float>(2.0 * 3.141592653589793 * (0.4f + 0.6f * rate) / fs);
            flutterInc = static_cast<float>(2.0 * 3.141592653589793 * (5.5f + 4.0f * rate) / fs);
        }

        // Backward compatibility overload
        void configure(int modeIdx, float rateParam, float depthParamVal, float mixAmount, bool enabled = true)
        {
            configure(static_cast<Mode>(std::clamp(modeIdx, 0, 6)), rateParam, depthParamVal, mixAmount, enabled);
        }

        void process(float& sampleL, float& sampleR)
        {
            if (!isEnabled || mix <= 0.001f)
                return;

            // Advance LFO phases
            lfoPhase += lfoInc;
            if (lfoPhase >= 6.2831853f) lfoPhase -= 6.2831853f;

            lfoFastPhase += lfoFastInc;
            if (lfoFastPhase >= 6.2831853f) lfoFastPhase -= 6.2831853f;

            wowPhase += wowInc;
            if (wowPhase >= 6.2831853f) wowPhase -= 6.2831853f;

            flutterPhase += flutterInc;
            if (flutterPhase >= 6.2831853f) flutterPhase -= 6.2831853f;

            switch (mode)
            {
                case Mode::Chorus:
                    processChorus(sampleL, sampleR);
                    break;

                case Mode::Tremolo:
                    processTremolo(sampleL, sampleR);
                    break;

                case Mode::Flanger:
                    processFlanger(sampleL, sampleR);
                    break;

                case Mode::Ensemble:
                    processEnsemble(sampleL, sampleR);
                    break;

                case Mode::Phaser:
                    processPhaser(sampleL, sampleR);
                    break;

                case Mode::WowFlutter:
                    processWowFlutter(sampleL, sampleR);
                    break;

                case Mode::Delay:
                    processDelay(sampleL, sampleR);
                    break;
            }
        }

    private:
        // 0. Chorus: Quadrature modulated stereo delay
        inline void processChorus(float& sampleL, float& sampleR)
        {
            if (bufferL.empty()) return;

            const size_t bufSize = bufferL.size();
            bufferL[writeIndex] = sampleL;
            bufferR[writeIndex] = sampleR;

            const float baseDelay = static_cast<float>(0.009 * fs);
            const float modDepth = static_cast<float>(0.0045 * fs) * depthParam;

            const float delayL = baseDelay + modDepth * std::sin(lfoPhase);
            const float delayR = baseDelay + modDepth * std::cos(lfoPhase);

            const float wetL = readInterpolated(bufferL, writeIndex, delayL);
            const float wetR = readInterpolated(bufferR, writeIndex, delayR);

            writeIndex = (writeIndex + 1) % bufSize;

            sampleL = (1.0f - mix * 0.5f) * sampleL + (mix * 0.70f) * wetL;
            sampleR = (1.0f - mix * 0.5f) * sampleR + (mix * 0.70f) * wetR;
        }

        // 1. Tremolo: Vintage 180-degree optical auto-pan
        inline void processTremolo(float& sampleL, float& sampleR)
        {
            const float sinL = std::sin(lfoPhase);
            const float modL = 1.0f - depthParam * 0.5f * (1.0f + sinL);
            const float modR = 1.0f - depthParam * 0.5f * (1.0f - sinL);

            const float dryL = sampleL;
            const float dryR = sampleR;

            sampleL = (1.0f - mix) * dryL + mix * (dryL * modL);
            sampleR = (1.0f - mix) * dryR + mix * (dryR * modR);
        }

        // 2. Flanger: Short comb delay with bipolar feedback
        inline void processFlanger(float& sampleL, float& sampleR)
        {
            if (bufferL.empty()) return;

            const size_t bufSize = bufferL.size();
            const float feedbackCoeff = depthParam * 0.82f;

            bufferL[writeIndex] = sampleL + flangerFeedbackL * feedbackCoeff;
            bufferR[writeIndex] = sampleR + flangerFeedbackR * feedbackCoeff;

            // Flanger delay sweep: 0.8 ms to 4.2 ms
            const float baseDelay = static_cast<float>(0.0008 * fs);
            const float sweepWidth = static_cast<float>(0.0034 * fs);
            const float normLfoL = 0.5f * (1.0f + std::sin(lfoPhase));
            const float normLfoR = 0.5f * (1.0f + std::cos(lfoPhase));

            const float delayL = baseDelay + sweepWidth * normLfoL;
            const float delayR = baseDelay + sweepWidth * normLfoR;

            const float wetL = readInterpolated(bufferL, writeIndex, delayL);
            const float wetR = readInterpolated(bufferR, writeIndex, delayR);

            flangerFeedbackL = std::clamp(wetL, -1.5f, 1.5f);
            flangerFeedbackR = std::clamp(wetR, -1.5f, 1.5f);

            writeIndex = (writeIndex + 1) % bufSize;

            sampleL = (1.0f - mix * 0.5f) * sampleL + (mix * 0.75f) * wetL;
            sampleR = (1.0f - mix * 0.5f) * sampleR + (mix * 0.75f) * wetR;
        }

        // 3. Ensemble: 3-phase dual-LFO BBD string ensemble (Solina/Juno)
        inline void processEnsemble(float& sampleL, float& sampleR)
        {
            if (bufferL.empty()) return;

            const size_t bufSize = bufferL.size();
            bufferL[writeIndex] = sampleL;
            bufferR[writeIndex] = sampleR;

            const float baseDelay = static_cast<float>(0.012 * fs);
            const float slowAmp = static_cast<float>(0.0030 * fs) * (0.4f + 0.6f * depthParam);
            const float fastAmp = static_cast<float>(0.0008 * fs) * (0.4f + 0.6f * depthParam);

            // Three phases separated by 120 degrees (2.094395 rad)
            constexpr float phi2 = 2.0943951f;
            constexpr float phi3 = 4.1887902f;

            const float d1 = baseDelay + slowAmp * std::sin(lfoPhase)        + fastAmp * std::sin(lfoFastPhase);
            const float d2 = baseDelay + slowAmp * std::sin(lfoPhase + phi2) + fastAmp * std::sin(lfoFastPhase + phi2);
            const float d3 = baseDelay + slowAmp * std::sin(lfoPhase + phi3) + fastAmp * std::sin(lfoFastPhase + phi3);

            const float tap1L = readInterpolated(bufferL, writeIndex, d1);
            const float tap2L = readInterpolated(bufferL, writeIndex, d2);
            const float tap3L = readInterpolated(bufferL, writeIndex, d3);

            const float tap1R = readInterpolated(bufferR, writeIndex, d1);
            const float tap2R = readInterpolated(bufferR, writeIndex, d2);
            const float tap3R = readInterpolated(bufferR, writeIndex, d3);

            writeIndex = (writeIndex + 1) % bufSize;

            const float wetL = 0.40f * tap1L + 0.40f * tap2L + 0.20f * tap3L;
            const float wetR = 0.20f * tap1R + 0.40f * tap2R + 0.40f * tap3R;

            sampleL = (1.0f - mix * 0.5f) * sampleL + (mix * 0.85f) * wetL;
            sampleR = (1.0f - mix * 0.5f) * sampleR + (mix * 0.85f) * wetR;
        }

        // 4. Phaser: 6-stage allpass ladder with resonant sweep
        inline void processPhaser(float& sampleL, float& sampleR)
        {
            // Swept notch center frequency: 220 Hz to 3600 Hz
            const float lfoSin = 0.5f * (1.0f + std::sin(lfoPhase));
            const float lfoCos = 0.5f * (1.0f + std::cos(lfoPhase));

            const float freqL = 220.0f + 3200.0f * (lfoSin * lfoSin);
            const float freqR = 220.0f + 3200.0f * (lfoCos * lfoCos);

            const float w0_L = static_cast<float>(3.141592653589793 * freqL / fs);
            const float w0_R = static_cast<float>(3.141592653589793 * freqR / fs);

            // First-order allpass coefficient: a = (tan(w0) - 1) / (tan(w0) + 1)
            const float tanL = std::tan(std::clamp(w0_L, 0.005f, 1.50f));
            const float tanR = std::tan(std::clamp(w0_R, 0.005f, 1.50f));
            const float aL = (tanL - 1.0f) / (tanL + 1.0f);
            const float aR = (tanR - 1.0f) / (tanR + 1.0f);

            const float feedbackCoeff = depthParam * 0.72f;

            // Process 6 stages for Left
            float xL = sampleL + phaserStagesL[5] * feedbackCoeff;
            for (size_t i = 0; i < 6; ++i)
            {
                const float y = aL * xL + phaserStagesL[i];
                phaserStagesL[i] = xL - aL * y;
                xL = y;
            }

            // Process 6 stages for Right
            float xR = sampleR + phaserStagesR[5] * feedbackCoeff;
            for (size_t i = 0; i < 6; ++i)
            {
                const float y = aR * xR + phaserStagesR[i];
                phaserStagesR[i] = xR - aR * y;
                xR = y;
            }

            // Blend out-of-phase output for deep moving notch cancellations
            sampleL = (1.0f - mix * 0.5f) * sampleL + (mix * 0.65f) * xL;
            sampleR = (1.0f - mix * 0.5f) * sampleR + (mix * 0.65f) * xR;
        }

        // 5. Wow & Flutter: Analog tape motor eccentricity and tape warmth
        inline void processWowFlutter(float& sampleL, float& sampleR)
        {
            if (bufferL.empty()) return;

            const size_t bufSize = bufferL.size();
            bufferL[writeIndex] = sampleL;
            bufferR[writeIndex] = sampleR;

            const float baseDelay = static_cast<float>(0.030 * fs);
            // Wow: slow capstan wobble (~0.6 Hz, +/- 2.5ms)
            const float wowL = std::sin(wowPhase);
            const float wowR = std::sin(wowPhase + 0.45f);
            // Flutter: fast belt/bearing flutter (~7.5 Hz, +/- 0.6ms)
            const float flutterL = std::sin(flutterPhase);
            const float flutterR = std::sin(flutterPhase + 0.85f);

            const float depthScale = depthParam * static_cast<float>(fs);
            const float delayL = baseDelay + (wowL * 0.0028f + flutterL * 0.0007f) * depthScale;
            const float delayR = baseDelay + (wowR * 0.0028f + flutterR * 0.0007f) * depthScale;

            float wetL = readInterpolated(bufferL, writeIndex, delayL);
            float wetR = readInterpolated(bufferR, writeIndex, delayR);

            writeIndex = (writeIndex + 1) % bufSize;

            // Gentle high-frequency tape head flux damping (~11 kHz)
            tapeDampL += 0.45f * (wetL - tapeDampL);
            tapeDampR += 0.45f * (wetR - tapeDampR);

            wetL = tapeDampL;
            wetR = tapeDampR;

            sampleL = (1.0f - mix) * sampleL + mix * wetL;
            sampleR = (1.0f - mix) * sampleR + mix * wetR;
        }

        // 6. Delay: Stereo ping-pong tape echo with analog damping
        inline void processDelay(float& sampleL, float& sampleR)
        {
            if (bufferL.empty()) return;

            const size_t bufSize = bufferL.size();

            // Delay time from 40 ms to 800 ms based on rate knob
            const float delaySec = 0.040f + (rate / 12.0f) * 0.760f;
            const float delaySamplesL = std::clamp(static_cast<float>(delaySec * fs), 10.0f, static_cast<float>(bufSize - 4));
            // Ping-pong stereo offset (Right channel at 3/4 dotted timing)
            const float delaySamplesR = std::clamp(static_cast<float>(delaySec * 0.75f * fs), 10.0f, static_cast<float>(bufSize - 4));

            const float feedbackCoeff = depthParam * 0.78f;

            // Read echo taps
            float echoL = readInterpolated(bufferL, writeIndex, delaySamplesL);
            float echoR = readInterpolated(bufferR, writeIndex, delaySamplesR);

            // Analog tape damping in feedback loop (~3.5 kHz roll-off)
            delayDampL += 0.35f * (echoL - delayDampL);
            delayDampR += 0.35f * (echoR - delayDampR);

            // Cross-channel ping-pong feedback injection
            bufferL[writeIndex] = sampleL + delayDampR * feedbackCoeff;
            bufferR[writeIndex] = sampleR + delayDampL * feedbackCoeff;

            writeIndex = (writeIndex + 1) % bufSize;

            sampleL = (1.0f - mix * 0.5f) * sampleL + (mix * 0.85f) * echoL;
            sampleR = (1.0f - mix * 0.5f) * sampleR + (mix * 0.85f) * echoR;
        }

        inline float readInterpolated(const std::vector<float>& buf, size_t wIdx, float delaySamples)
        {
            const float rPos = static_cast<float>(wIdx) - delaySamples;
            const float wrappedPos = rPos < 0.0f ? rPos + static_cast<float>(buf.size()) : rPos;
            const size_t idx0 = static_cast<size_t>(wrappedPos) % buf.size();
            const size_t idx1 = (idx0 + 1) % buf.size();
            const float frac = wrappedPos - std::floor(wrappedPos);
            return buf[idx0] + frac * (buf[idx1] - buf[idx0]);
        }

        double fs = 44100.0;
        bool isEnabled = true;
        Mode mode = Mode::Chorus;
        float rate = 1.2f;
        float depthParam = 0.5f;
        float mix = 0.5f;

        float lfoPhase = 0.0f;
        float lfoInc = 0.0f;
        float lfoFastPhase = 0.0f;
        float lfoFastInc = 0.0f;
        float wowPhase = 0.0f;
        float wowInc = 0.0f;
        float flutterPhase = 0.0f;
        float flutterInc = 0.0f;

        size_t writeIndex = 0;
        std::vector<float> bufferL;
        std::vector<float> bufferR;

        // Phaser state
        std::array<float, 6> phaserStagesL { 0.0f };
        std::array<float, 6> phaserStagesR { 0.0f };

        // Flanger state
        float flangerFeedbackL = 0.0f;
        float flangerFeedbackR = 0.0f;

        // Delay state
        float delayFeedbackL = 0.0f;
        float delayFeedbackR = 0.0f;
        float delayDampL = 0.0f;
        float delayDampR = 0.0f;

        // Tape damp
        float tapeDampL = 0.0f;
        float tapeDampR = 0.0f;
    };
}
