#pragma once

#include "../PhysicalConstants.h"
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>

namespace ModelKeys
{
    class PlateReverb
    {
    public:
        using Type = ReverbType;

        PlateReverb() = default;

        void prepare(double sampleRate)
        {
            fs = sampleRate > 0.0 ? sampleRate : 44100.0;
            const float scale = static_cast<float>(fs / 44100.0);

            // 1. Plate Reverb lengths (EMT 140)
            const std::array<int, 8> plateCombs = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
            const std::array<int, 4> plateAps   = { 556, 441, 341, 225 };

            // 2. Room Reverb lengths (Tight studio chamber)
            const std::array<int, 8> roomCombs  = { 437, 519, 583, 677, 733, 811, 877, 941 };
            const std::array<int, 4> roomAps    = { 241, 179, 131, 97 };

            // 3. Hall Reverb lengths (Concert grand hall)
            const std::array<int, 8> hallCombs  = { 1823, 2011, 2203, 2417, 2659, 2903, 3167, 3449 };
            const std::array<int, 4> hallAps    = { 821, 647, 499, 367 };

            // 4. Spring Reverb lengths (3-spring tank)
            const std::array<int, 4> springCombs = { 1087, 1381, 1693, 1987 };
            const std::array<int, 6> springChirpAps = { 113, 157, 211, 281, 359, 439 };

            for (size_t i = 0; i < combsL.size(); ++i)
            {
                combsL[i].init(static_cast<size_t>(hallCombs[i] * scale) + 32);
                combsR[i].init(static_cast<size_t>((hallCombs[i] + 37) * scale) + 32);
            }

            for (size_t i = 0; i < allpassesL.size(); ++i)
            {
                allpassesL[i].init(static_cast<size_t>(hallAps[i] * scale) + 16);
                allpassesR[i].init(static_cast<size_t>((hallAps[i] + 31) * scale) + 16);
            }

            for (size_t i = 0; i < springDispL.size(); ++i)
            {
                springDispL[i].init(static_cast<size_t>(springChirpAps[i] * scale) + 8);
                springDispR[i].init(static_cast<size_t>((springChirpAps[i] + 19) * scale) + 8);
            }

            reset();
        }

        void reset()
        {
            for (auto& c : combsL) c.reset();
            for (auto& c : combsR) c.reset();
            for (auto& a : allpassesL) a.reset();
            for (auto& a : allpassesR) a.reset();
            for (auto& d : springDispL) d.reset();
            for (auto& d : springDispR) d.reset();

            springBpStateX1 = 0.0f; springBpStateY1 = 0.0f;
            springBpStateX2 = 0.0f; springBpStateY2 = 0.0f;
        }

        void configure(Type type, float roomSize, float dampingVal, float mixAmount, bool enabled = true)
        {
            isEnabled = enabled;
            reverbType = type;
            sizeParam = std::clamp(roomSize, 0.05f, 0.98f);
            dampParam = std::clamp(dampingVal, 0.05f, 0.95f);
            mix = std::clamp(mixAmount, 0.0f, 1.0f);

            const float scale = static_cast<float>(fs / 44100.0);

            switch (reverbType)
            {
                case Type::Plate:
                {
                    const std::array<int, 8> plateCombs = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
                    const std::array<int, 4> plateAps   = { 556, 441, 341, 225 };
                    const float feedback = 0.70f + 0.28f * sizeParam;
                    const float damp = dampParam * 0.38f;

                    for (size_t i = 0; i < 8; ++i)
                    {
                        combsL[i].setDelay(static_cast<size_t>(plateCombs[i] * scale));
                        combsL[i].feedback = feedback;
                        combsL[i].damp = damp;
                        combsR[i].setDelay(static_cast<size_t>((plateCombs[i] + 23) * scale));
                        combsR[i].feedback = feedback;
                        combsR[i].damp = damp;
                    }
                    for (size_t i = 0; i < 4; ++i)
                    {
                        allpassesL[i].setDelay(static_cast<size_t>(plateAps[i] * scale));
                        allpassesR[i].setDelay(static_cast<size_t>((plateAps[i] + 23) * scale));
                    }
                    break;
                }

                case Type::Room:
                {
                    const std::array<int, 8> roomCombs = { 437, 519, 583, 677, 733, 811, 877, 941 };
                    const std::array<int, 4> roomAps   = { 241, 179, 131, 97 };
                    const float feedback = 0.48f + 0.36f * sizeParam;
                    const float damp = 0.15f + dampParam * 0.55f; // Warmer wooden room absorption

                    for (size_t i = 0; i < 8; ++i)
                    {
                        combsL[i].setDelay(static_cast<size_t>(roomCombs[i] * scale));
                        combsL[i].feedback = feedback;
                        combsL[i].damp = damp;
                        combsR[i].setDelay(static_cast<size_t>((roomCombs[i] + 19) * scale));
                        combsR[i].feedback = feedback;
                        combsR[i].damp = damp;
                    }
                    for (size_t i = 0; i < 4; ++i)
                    {
                        allpassesL[i].setDelay(static_cast<size_t>(roomAps[i] * scale));
                        allpassesR[i].setDelay(static_cast<size_t>((roomAps[i] + 19) * scale));
                    }
                    break;
                }

                case Type::Hall:
                {
                    const std::array<int, 8> hallCombs = { 1823, 2011, 2203, 2417, 2659, 2903, 3167, 3449 };
                    const std::array<int, 4> hallAps   = { 821, 647, 499, 367 };
                    const float feedback = 0.76f + 0.22f * sizeParam;
                    const float damp = dampParam * 0.42f;

                    for (size_t i = 0; i < 8; ++i)
                    {
                        combsL[i].setDelay(static_cast<size_t>(hallCombs[i] * scale));
                        combsL[i].feedback = feedback;
                        combsL[i].damp = damp;
                        combsR[i].setDelay(static_cast<size_t>((hallCombs[i] + 37) * scale));
                        combsR[i].feedback = feedback;
                        combsR[i].damp = damp;
                    }
                    for (size_t i = 0; i < 4; ++i)
                    {
                        allpassesL[i].setDelay(static_cast<size_t>(hallAps[i] * scale));
                        allpassesR[i].setDelay(static_cast<size_t>((hallAps[i] + 31) * scale));
                    }
                    break;
                }

                case Type::Spring:
                {
                    // Spring tank delay lines
                    const std::array<int, 4> springCombs = { 1087, 1381, 1693, 1987 };
                    const float feedback = 0.65f + 0.28f * sizeParam;
                    const float damp = 0.10f + dampParam * 0.45f;

                    for (size_t i = 0; i < 4; ++i)
                    {
                        combsL[i].setDelay(static_cast<size_t>(springCombs[i] * scale));
                        combsL[i].feedback = feedback;
                        combsL[i].damp = damp;
                        combsR[i].setDelay(static_cast<size_t>((springCombs[i] + 43) * scale));
                        combsR[i].feedback = feedback;
                        combsR[i].damp = damp;
                    }
                    break;
                }
            }
        }

        void configure(float roomSize, float dampingVal, float mixAmount, bool enabled = true)
        {
            configure(reverbType, roomSize, dampingVal, mixAmount, enabled);
        }

        void process(float& sampleL, float& sampleR)
        {
            if (!isEnabled || mix <= 0.001f)
                return;

            float outL = 0.0f;
            float outR = 0.0f;

            if (reverbType == Type::Spring)
            {
                // Spring Tank Model:
                // 1. Transient dispersion cascade: allpasses delay high frequencies behind low frequencies (iconic "boing/chirp")
                float dispersedL = (sampleL + sampleR) * 0.022f;
                float dispersedR = dispersedL;

                for (size_t i = 0; i < springDispL.size(); ++i)
                {
                    dispersedL = springDispL[i].process(dispersedL);
                    dispersedR = springDispR[i].process(dispersedR);
                }

                // 2. Parallel spring coils
                for (size_t i = 0; i < 4; ++i)
                {
                    outL += combsL[i].process(dispersedL);
                    outR += combsR[i].process(dispersedR);
                }

                // 3. Spring tank transducer band-pass filter (~180 Hz to ~4.5 kHz)
                // Eliminates deep rumble and harsh fizz, giving authentic vintage spring tank color
                springBpStateX1 += 0.35f * (outL - springBpStateX1);
                springBpStateY1 += 0.08f * (springBpStateX1 - springBpStateY1);
                outL = (springBpStateX1 - springBpStateY1) * 1.35f;

                springBpStateX2 += 0.35f * (outR - springBpStateX2);
                springBpStateY2 += 0.08f * (springBpStateX2 - springBpStateY2);
                outR = (springBpStateX2 - springBpStateY2) * 1.35f;
            }
            else // Plate, Room, Hall
            {
                const float inputMono = (sampleL + sampleR) * (reverbType == Type::Room ? 0.022f : 0.015f);

                // Parallel comb filters
                for (size_t i = 0; i < 8; ++i)
                {
                    outL += combsL[i].process(inputMono);
                    outR += combsR[i].process(inputMono);
                }

                // Series allpass diffusers
                for (size_t i = 0; i < 4; ++i)
                {
                    outL = allpassesL[i].process(outL);
                    outR = allpassesR[i].process(outR);
                }
            }

            // Mix dry and wet
            sampleL = (1.0f - mix) * sampleL + mix * outL;
            sampleR = (1.0f - mix) * sampleR + mix * outR;
        }

    private:
        struct Comb
        {
            std::vector<float> buf;
            size_t idx = 0;
            size_t delayLen = 1000;
            float filterStore = 0.0f;
            float damp = 0.2f;
            float feedback = 0.8f;

            void init(size_t maxCapacity)
            {
                buf.assign(std::max<size_t>(maxCapacity, 16), 0.0f);
                idx = 0;
                delayLen = buf.size() - 4;
                filterStore = 0.0f;
            }

            void reset()
            {
                std::fill(buf.begin(), buf.end(), 0.0f);
                idx = 0;
                filterStore = 0.0f;
            }

            void setDelay(size_t len)
            {
                delayLen = std::clamp<size_t>(len, 4, buf.size() - 2);
            }

            inline float process(float in)
            {
                const float output = buf[idx];
                filterStore = (output * (1.0f - damp)) + (filterStore * damp);
                buf[idx] = in + (filterStore * feedback);
                if (++idx >= delayLen) idx = 0;
                return output;
            }
        };

        struct Allpass
        {
            std::vector<float> buf;
            size_t idx = 0;
            size_t delayLen = 500;
            static constexpr float feedback = 0.5f;

            void init(size_t maxCapacity)
            {
                buf.assign(std::max<size_t>(maxCapacity, 16), 0.0f);
                idx = 0;
                delayLen = buf.size() - 4;
            }

            void reset()
            {
                std::fill(buf.begin(), buf.end(), 0.0f);
                idx = 0;
            }

            void setDelay(size_t len)
            {
                delayLen = std::clamp<size_t>(len, 4, buf.size() - 2);
            }

            inline float process(float in)
            {
                const float bufOut = buf[idx];
                const float out = -in + bufOut;
                buf[idx] = in + (bufOut * feedback);
                if (++idx >= delayLen) idx = 0;
                return out;
            }
        };

        // Dispersive allpass for spring chirp/boing (negative feedforward, positive feedback)
        struct DispersiveAllpass
        {
            std::vector<float> buf;
            size_t idx = 0;
            static constexpr float g = 0.65f;

            void init(size_t size)
            {
                buf.assign(std::max<size_t>(size, 4), 0.0f);
                idx = 0;
            }

            void reset()
            {
                std::fill(buf.begin(), buf.end(), 0.0f);
                idx = 0;
            }

            inline float process(float in)
            {
                const float bufOut = buf[idx];
                const float out = -g * in + bufOut;
                buf[idx] = in + g * out;
                if (++idx >= buf.size()) idx = 0;
                return out;
            }
        };

        double fs = 44100.0;
        bool isEnabled = true;
        Type reverbType = Type::Plate;
        float sizeParam = 0.7f;
        float dampParam = 0.3f;
        float mix = 0.25f;

        std::array<Comb, 8> combsL;
        std::array<Comb, 8> combsR;
        std::array<Allpass, 4> allpassesL;
        std::array<Allpass, 4> allpassesR;

        // Dispersive cascade for spring reverb
        std::array<DispersiveAllpass, 6> springDispL;
        std::array<DispersiveAllpass, 6> springDispR;

        // Spring bandpass states
        float springBpStateX1 = 0.0f, springBpStateY1 = 0.0f;
        float springBpStateX2 = 0.0f, springBpStateY2 = 0.0f;
    };
}
