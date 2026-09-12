#pragma once

#include "PhysicalConstants.h"
#include <cmath>
#include <array>
#include <algorithm>

namespace ModelKeys
{
    class ResonatorModel
    {
    public:
        struct Mode
        {
            float baseFreq = 440.0f;
            float detuneHz = 0.5f;
            float r = 0.999f;
            float r2 = 0.999f;
            float cosW = 1.0f;
            float sinW = 0.0f;
            float cosW2 = 1.0f;
            float sinW2 = 0.0f;
            float bloom = 0.3f;
            float excCoupling = 1.0f;
            float pickupCoupling = 1.0f;
            float inputGain = 0.035f;

            float x = 0.0f;  // Primary polarization displacement
            float y = 0.0f;  // Primary polarization velocity
            float x2 = 0.0f; // Orthogonal twin displacement (Acoustic bloom & beating)
            float y2 = 0.0f; // Orthogonal twin velocity

            bool active = true;

            void reset()
            {
                x = 0.0f;
                y = 0.0f;
                x2 = 0.0f;
                y2 = 0.0f;
            }

            inline float step(float inputForce, float sympatheticForce = 0.0f)
            {
                if (!active) return 0.0f;

                // inputForce acts at exciter contact position (excCoupling)
                // sympatheticForce acts through the bridge/soundboard coupling
                const float drivenX = (inputForce * excCoupling + sympatheticForce * 0.70f) * inputGain;

                // Primary polarization update (strictly passive, uncoupled pole)
                const float newX1 = r * (cosW * x - sinW * y) + drivenX;
                const float newY1 = r * (sinW * x + cosW * y);

                // Orthogonal twin polarization update (strictly passive, uncoupled pole)
                const float twinDrive = drivenX * (0.50f + 0.50f * bloom);
                const float newX2 = r2 * (cosW2 * x2 - sinW2 * y2) + twinDrive;
                const float newY2 = r2 * (sinW2 * x2 + cosW2 * y2);

                // Sanitize against numerical overflow / NaN
                if (std::isnan(newX1) || std::abs(newX1) > 5.0f || std::isnan(newX2) || std::abs(newX2) > 5.0f)
                {
                    x = 0.0f;
                    y = 0.0f;
                    x2 = 0.0f;
                    y2 = 0.0f;
                    return 0.0f;
                }

                x = newX1;
                y = newY1;
                x2 = newX2;
                y2 = newY2;

                const float bloomMix = 0.50f + 0.50f * bloom;
                return (x + bloomMix * x2) * pickupCoupling;
            }
        };

        ResonatorModel() = default;

        void prepare(double sampleRate)
        {
            fs = sampleRate > 0.0 ? sampleRate : 44100.0;
            reset();
        }

        void reset()
        {
            for (auto& m : modes)
                m.reset();
            currentPitchDropST = 0.0f;
            pitchUpdateCounter = 0;
            totalEnergy = 0.0f;
            targetBendFactor = 1.0f;
            currentBendFactor = 1.0f;
            currentStretchRatio = 1.0f;
        }

        void softRetrigger(float dampingOfHighModes = 0.40f)
        {
            // Retain vibrating string momentum on re-strike:
            // Low modes retain 70-85% energy, while higher modes are damped by felt impact
            for (size_t i = 0; i < modes.size(); ++i)
            {
                const float retention = (i == 0) ? 0.85f : ((i < 4) ? 0.65f : dampingOfHighModes);
                modes[i].x *= retention;
                modes[i].y *= retention;
                modes[i].x2 *= retention;
                modes[i].y2 *= retention;
            }
        }

        void setRealtimePitchBendFactor(float factor) noexcept
        {
            targetBendFactor = std::clamp(factor, 0.25f, 4.0f);
        }

        void configure(float fundamentalFreq, ResonatorType type,
                       float decayTimeSec, float brightness, float inharmonicity,
                       float contactPosition, float pickupPosition,
                       float velocity = 0.8f,
                       float pitchDropSemitones = 0.0f, float pitchDropDecayMs = 60.0f,
                       float beatingBloomParam = 0.3f, float materialBalanceParam = 0.5f,
                       float initialPitchRatio = 1.0f)
        {
            resType = type;
            f0 = std::clamp(fundamentalFreq, 15.0f, 18000.0f);
            baseDecay = std::clamp(decayTimeSec, 0.02f, 15.0f);
            brightVal = std::clamp(brightness, 0.0f, 1.0f);
            inharmVal = inharmonicity;
            contactPos = std::clamp(contactPosition, 0.02f, 0.98f);
            pickupPos = std::clamp(pickupPosition, 0.02f, 0.98f);
            bloom = std::clamp(beatingBloomParam, 0.0f, 1.0f);
            materialBalance = std::clamp(materialBalanceParam, 0.0f, 1.0f);
            targetBendFactor = std::clamp(initialPitchRatio, 0.25f, 4.0f);
            currentBendFactor = targetBendFactor;

            // Membrane and steelpan non-linear tension pitch modulation
            // Strikes produce high initial tension resulting in an acoustic downward pitch drop
            if (type == ResonatorType::Membrane || type == ResonatorType::SteelPan || pitchDropSemitones > 0.05f)
            {
                currentPitchDropST = std::clamp(pitchDropSemitones, 0.0f, 24.0f) * (0.4f + 0.6f * velocity);
                const float decaySec = std::clamp(pitchDropDecayMs * 0.001f, 0.005f, 0.6f);
                pitchDropDecayRate = static_cast<float>(std::exp(-1.0 / (decaySec * fs)));
            }
            else
            {
                currentPitchDropST = 0.0f;
                pitchDropDecayRate = 0.0f;
            }

            pitchUpdateCounter = 0;
            updateModeCoefficients();
        }

        void setDamperRelease(float damperFactor)
        {
            // Acoustic Grand Piano physics: strings above F6 (~1396 Hz) have no felt dampers on acoustic grand
            if (resType == ResonatorType::String && f0 > 1396.0f)
                return;

            const float factor = std::clamp(damperFactor, 0.05f, 1.0f);
            for (size_t i = 0; i < modes.size(); ++i)
            {
                // Authentic felt damper physics: high partials absorb quickly into felt, fundamental thuds gently
                const float modeFeltAbsorption = 1.0f + 0.50f * static_cast<float>(i);
                const float effFactor = std::clamp(factor / modeFeltAbsorption, 0.02f, 1.0f);
                modes[i].r = std::pow(modes[i].r, 1.0f / effFactor);
                modes[i].r2 = std::pow(modes[i].r2, 1.0f / effFactor);
            }
        }

        struct Output
        {
            float pickupSignal = 0.0f;
            float contactVelocity = 0.0f;
        };

        Output processSample(float exciterForce, float sympatheticForce = 0.0f)
        {
            float sumPickup = 0.0f;
            float sumVelocityAtContact = 0.0f;
            float energyAccum = 0.0f;

            pitchUpdateCounter++;
            bool needFrequencyUpdate = false;

            // Handle dynamic membrane tension pitch drop
            if (currentPitchDropST > 0.005f)
            {
                currentPitchDropST *= pitchDropDecayRate;
                if ((pitchUpdateCounter & 0x0F) == 0)
                    needFrequencyUpdate = true;
            }
            else if (currentPitchDropST > 0.0f)
            {
                currentPitchDropST = 0.0f;
                needFrequencyUpdate = true;
            }

            // Handle real-time pitch bend slew (smooth continuous glide during sustain & decay)
            if (std::abs(currentBendFactor - targetBendFactor) > 0.00005f)
            {
                currentBendFactor += 0.0035f * (targetBendFactor - currentBendFactor);
                if ((pitchUpdateCounter & 0x0F) == 0)
                    needFrequencyUpdate = true;
            }
            else if (currentBendFactor != targetBendFactor)
            {
                currentBendFactor = targetBendFactor;
                needFrequencyUpdate = true;
            }

            // Nonlinear amplitude string stretch bloom (forte growl & dynamic pitch bloom)
            if ((pitchUpdateCounter & 0x1F) == 0 && (resType == ResonatorType::String || resType == ResonatorType::Plate))
            {
                const float stretchRatio = 1.0f + std::clamp(totalEnergy * 0.0012f, 0.0f, 0.0035f);
                if (std::abs(stretchRatio - currentStretchRatio) > 0.0001f)
                {
                    currentStretchRatio = stretchRatio;
                    needFrequencyUpdate = true;
                }
            }

            if (needFrequencyUpdate)
            {
                updateInstantaneousFrequencies();
            }

            int activeCount = 0;
            for (auto& m : modes)
            {
                if (!m.active) continue;

                const float sig = m.step(exciterForce, sympatheticForce);
                sumPickup += sig;

                sumVelocityAtContact += (m.y * m.sinW + 0.5f * m.y2 * m.sinW2) * m.excCoupling;
                energyAccum += std::abs(m.x) + std::abs(m.x2);
                activeCount++;
            }

            totalEnergy = energyAccum;

            // Headroom normalization across active modes (up to 32 modes)
            const float normFactor = activeCount > 0 ? (1.5f / std::sqrt(static_cast<float>(activeCount))) : 1.0f;
            sumPickup *= normFactor;

            return { sumPickup, sumVelocityAtContact };
        }

        float getTotalEnergy() const noexcept { return totalEnergy; }
        const std::array<Mode, MAX_MODES>& getModes() const noexcept { return modes; }

    private:
        void updateModeCoefficients()
        {
            const float nyquist = static_cast<float>(fs * 0.48);

            for (int i = 0; i < MAX_MODES; ++i)
            {
                float modeRatio = 1.0f;

                switch (resType)
                {
                    case ResonatorType::String:
                    {
                        const float n = static_cast<float>(i + 1);
                        // Railsback acoustic grand dispersion curve:
                        // Bass strings (A0-C2) have low B; treble strings (C5-C8) have high stiffness B
                        const float pitchScale = std::clamp(std::pow(f0 / 261.63f, 0.65f), 0.35f, 4.5f);
                        const float bFactor = std::max(0.0f, inharmVal * 0.00060f * pitchScale);
                        modeRatio = n * std::sqrt(1.0f + bFactor * n * n);
                        break;
                    }
                    case ResonatorType::ClampedTine:
                    {
                        modeRatio = ModalProfiles::ClampedTineRatios[i];
                        if (i == 4 || i == 7) // Tine bell chime modes (5.85x and 8.4x)
                        {
                            modeRatio *= (1.0f + (inharmVal - 0.5f) * 0.12f);
                        }
                        else if (i > 0)
                        {
                            // Harmonic tonebar resonance with subtle stiffness dispersion
                            const float n = static_cast<float>(i);
                            modeRatio *= (1.0f + inharmVal * 0.0004f * n * n);
                        }
                        break;
                    }
                    case ResonatorType::ClampedReed:
                    {
                        const float n = static_cast<float>(i + 1);
                        // Consonant harmonic series with subtle reed dispersion
                        const float bFactor = std::max(0.0f, inharmVal * 0.00025f);
                        modeRatio = n * std::sqrt(1.0f + bFactor * n * n);
                        break;
                    }
                    case ResonatorType::FreeBar:
                    {
                        modeRatio = ModalProfiles::FreeBarRatios[i];
                        if (i > 0)
                            modeRatio *= (1.0f + (inharmVal - 0.5f) * 0.15f * static_cast<float>(i));
                        break;
                    }
                    case ResonatorType::Membrane:
                    {
                        modeRatio = ModalProfiles::MembraneRatios[i];
                        break;
                    }
                    case ResonatorType::Plate:
                    {
                        modeRatio = ModalProfiles::PlateRatios[i];
                        if (i > 0)
                            modeRatio *= (1.0f + (inharmVal - 0.5f) * 0.25f * static_cast<float>(i));
                        break;
                    }
                    case ResonatorType::SteelPan:
                    {
                        modeRatio = ModalProfiles::SteelPanRatios[i];
                        if (i == 1 || i == 2)
                        {
                            // Tuned harmonic modes (2x octave, 3x fifth) with fine detune
                            modeRatio *= (1.0f + (inharmVal - 0.5f) * 0.04f);
                        }
                        else if (i > 2)
                        {
                            modeRatio *= (1.0f + (inharmVal - 0.5f) * 0.12f * static_cast<float>(i));
                        }
                        break;
                    }
                }

                const float baseModeFreq = f0 * modeRatio;

                if (baseModeFreq >= nyquist || baseModeFreq < 10.0f)
                {
                    modes[i].active = false;
                    continue;
                }

                modes[i].active = true;
                modes[i].baseFreq = baseModeFreq;

                // Acoustic unison detuning in cents (Steinway trichord / reed comb physics)
                // Produces natural acoustic beating & bloom proportional to pitch across the keyboard
                float centsDetune = 0.0f;
                if (resType == ResonatorType::String)
                {
                    // Grand Piano trichord unison detuning: 0.8 to 1.8 cents
                    // Slight spread in higher partials models bridge boundary anisotropy
                    centsDetune = bloom * (0.85f + 0.12f * static_cast<float>(i));
                }
                else if (resType == ResonatorType::ClampedReed)
                {
                    // Wurlitzer reed: subtle capacitive comb shimmer
                    centsDetune = bloom * (0.40f + 0.08f * static_cast<float>(i));
                }
                else
                {
                    centsDetune = bloom * (1.10f + 0.12f * static_cast<float>(i));
                }
                const float detuneHz = baseModeFreq * (std::pow(2.0f, centsDetune / 1200.0f) - 1.0f);
                modes[i].detuneHz = detuneHz;
                modes[i].bloom = bloom;

                // Damping computation tailored per geometry
                float dampingWeight = ModalProfiles::DefaultDampingWeights[i];

                if (resType == ResonatorType::FreeBar)
                {
                    // Marimba / Vibraphone wood damping:
                    // Mode 1 (tuned parabolic arch 4.0x) rings for ~60ms giving authentic wooden knock,
                    // while modes 2+ die extremely rapidly (<20ms) ensuring 0.0% high-mid overtone leakage
                    if (i == 1)
                    {
                        dampingWeight = 2.4f;
                    }
                    else if (i > 1)
                    {
                        const float woodAlpha = 6.0f + 14.0f * materialBalance;
                        dampingWeight = 6.0f + woodAlpha * static_cast<float>(i * i);
                    }
                }
                else if (resType == ResonatorType::ClampedTine)
                {
                    // Rhodes tine: Bell chime overtone (modes 4 and 7) decays quickly into warm sustained tonebar
                    if (i == 4 || i == 7)
                    {
                        dampingWeight = 25.0f + 25.0f * materialBalance;
                    }
                    else
                    {
                        // Higher harmonics roll off smoothly in the massive iron tonebar
                        const float tineAlpha = 0.45f + 1.5f * materialBalance;
                        dampingWeight = 1.0f + tineAlpha * static_cast<float>(i * i);
                    }
                }
                else if (resType == ResonatorType::ClampedReed)
                {
                    // Wurlitzer reed: harmonic modes sustain with singing warmth and classic reed bite
                    const float reedAlpha = 0.22f + 0.65f * materialBalance;
                    dampingWeight = 1.0f + reedAlpha * static_cast<float>(i);
                }
                else if (resType == ResonatorType::SteelPan)
                {
                    // Caribbean Steelpan: Fundamental, octave (mode 1), and octave+fifth (mode 2)
                    // ring with long sustain, while higher rim modes project bright metallic shimmer (4-8 kHz)
                    if (i <= 2)
                    {
                        dampingWeight = 1.0f + 0.15f * static_cast<float>(i);
                    }
                    else
                    {
                        const float panAlpha = 0.20f + 0.60f * materialBalance;
                        dampingWeight = 1.2f + panAlpha * static_cast<float>(i);
                    }
                }

                float modeDecaySec;
                if (resType == ResonatorType::ClampedTine && (i == 4 || i == 7))
                {
                    // Viscoelastic damping of tine flexural chime (5.85x and 8.4x):
                    // Rapidly decays within 45-75ms, giving the iconic Fender Rhodes
                    // neoprene hammer "ping" before melting into singing tonebar sustain
                    modeDecaySec = 0.045f + 0.035f * (1.0f - materialBalance);
                }
                else if (resType == ResonatorType::ClampedTine && i == 0)
                {
                    // Heavy iron tonebar anchors the fundamental
                    modeDecaySec = baseDecay;
                }
                else if (resType == ResonatorType::ClampedTine && i == 1)
                {
                    // Tonebar octave sympathetic resonance
                    modeDecaySec = baseDecay * 0.85f;
                }
                else
                {
                    const float hfLoss = (1.0f - brightVal) * (dampingWeight - 1.0f) * 0.85f;
                    modeDecaySec = std::max(0.004f, baseDecay / (1.0f + hfLoss));
                }

                const double decayRate = 6.907755 / (modeDecaySec * fs);
                if (resType == ResonatorType::String)
                {
                    // Grand Piano trichord bridge prompt / aftersound dual decay:
                    // Mode 1 (vertical soundboard drive): prompt decay (punchy acoustic attack)
                    // Mode 2 (horizontal / mistuned trichord): aftersound decay (long singing sustain)
                    modes[i].r = static_cast<float>(std::clamp(std::exp(-decayRate * 1.30), 0.1, 0.99995));
                    modes[i].r2 = static_cast<float>(std::clamp(std::exp(-decayRate * 0.55), 0.1, 0.99995));
                }
                else if (resType == ResonatorType::ClampedReed)
                {
                    // Wurlitzer reed: lively, singing reed vibration
                    modes[i].r = static_cast<float>(std::clamp(std::exp(-decayRate * 1.02), 0.1, 0.99995));
                    modes[i].r2 = static_cast<float>(std::clamp(std::exp(-decayRate * 0.96), 0.1, 0.99995));
                }
                else if (resType == ResonatorType::SteelPan)
                {
                    // Metallic shell twin-polarization ring
                    modes[i].r = static_cast<float>(std::clamp(std::exp(-decayRate * 0.95), 0.1, 0.99995));
                    modes[i].r2 = static_cast<float>(std::clamp(std::exp(-decayRate * 0.88), 0.1, 0.99995));
                }
                else
                {
                    modes[i].r = static_cast<float>(std::clamp(std::exp(-decayRate), 0.1, 0.99995));
                    modes[i].r2 = static_cast<float>(std::clamp(std::exp(-decayRate * 1.03), 0.1, 0.99995));
                }

                // Spatial coupling
                if (resType == ResonatorType::ClampedTine)
                {
                    // Cantilever beam boundary conditions: tip has maximum displacement
                    // Pickup sits right at the vibrating free tip; hammer strikes near tip
                    if (i == 0)
                    {
                        modes[i].excCoupling = 1.0f;
                        modes[i].pickupCoupling = 1.0f;
                    }
                    else if (i == 1) // 2f0 tonebar / magnetic flux 2nd harmonic
                    {
                        modes[i].excCoupling = 0.70f;
                        modes[i].pickupCoupling = 0.65f;
                    }
                    else if (i == 2) // 3f0
                    {
                        modes[i].excCoupling = 0.45f;
                        modes[i].pickupCoupling = 0.32f;
                    }
                    else if (i == 3) // 4f0
                    {
                        modes[i].excCoupling = 0.25f;
                        modes[i].pickupCoupling = 0.16f;
                    }
                    else if (i == 4) // 5.85 f0 tine bell chime
                    {
                        modes[i].excCoupling = 0.45f * (0.5f + 0.5f * brightVal);
                        modes[i].pickupCoupling = 0.35f;
                    }
                    else if (i == 7) // 8.4 f0 tine chime
                    {
                        modes[i].excCoupling = 0.25f * (0.5f + 0.5f * brightVal);
                        modes[i].pickupCoupling = 0.20f;
                    }
                    else
                    {
                        const float fall = 1.0f / (1.0f + 0.45f * static_cast<float>(i));
                        modes[i].excCoupling = fall * 0.20f;
                        modes[i].pickupCoupling = fall * 0.15f;
                    }
                }
                else
                {
                    const float mIndex = static_cast<float>(i + 1);
                    modes[i].excCoupling = std::sin(mIndex * 3.14159265f * contactPos);
                    modes[i].pickupCoupling = std::sin(mIndex * 3.14159265f * pickupPos);

                    if (std::abs(modes[i].excCoupling) < 0.05f)
                        modes[i].excCoupling = (modes[i].excCoupling >= 0.0f ? 0.05f : -0.05f);
                    if (std::abs(modes[i].pickupCoupling) < 0.05f)
                        modes[i].pickupCoupling = (modes[i].pickupCoupling >= 0.0f ? 0.05f : -0.05f);
                }

                // Modal input gain: Natural physical falloff so fundamental and body warmth
                // are solid and authoritative, matching Pianoteq's singing fundamental resonance
                float falloffCoeff = 0.30f;
                if (resType == ResonatorType::String)
                {
                    // Grand piano trichord singing midrange harmonics:
                    // falloffCoeff = 0.38f projects fundamental and lower octaves matching Steinway D 60% bass ratio
                    falloffCoeff = 0.38f;
                }
                else if (resType == ResonatorType::ClampedTine)
                    falloffCoeff = 0.18f;
                else if (resType == ResonatorType::ClampedReed)
                    falloffCoeff = 0.38f; // Hollow reed fundamental + prominent 2nd/3rd bite
                else if (resType == ResonatorType::FreeBar)
                    falloffCoeff = 0.55f; // Pure fundamental rosewood bar + 4x octave ring
                else if (resType == ResonatorType::SteelPan)
                    falloffCoeff = 0.22f; // Strong octave & fifth shell projection + bright metallic ping

                float baseGain = 0.045f;
                if (resType == ResonatorType::ClampedTine)
                    baseGain = 0.075f; // Rhodes tine direct magnetic coupling
                else if (resType == ResonatorType::ClampedReed)
                    baseGain = 0.068f; // Wurlitzer reed electrostatic modulation
                else if (resType == ResonatorType::String)
                    baseGain = 0.065f; // Grand piano & guitar bridge acoustic drive
                else if (resType == ResonatorType::SteelPan)
                    baseGain = 0.052f; // Steelpan concave shell resonance
                else if (resType == ResonatorType::FreeBar)
                    baseGain = 0.042f; // Rosewood bar

                const float modalFalloff = 1.0f / (1.0f + falloffCoeff * static_cast<float>(i));
                modes[i].inputGain = baseGain * modalFalloff;

                if (resType == ResonatorType::String)
                {
                    if (i == 0)
                    {
                        // Fundamental mode momentum: compensates for small contactPos (e.g. 1/8th strike node)
                        // ensuring deep foundation power matching real Steinway D bridge drive
                        const float fundExc = std::abs(modes[0].excCoupling);
                        const float fundBoost = 1.0f / std::max(0.28f, fundExc);
                        modes[0].inputGain *= std::min(3.2f, fundBoost);
                    }
                    else if (i == 1)
                    {
                        // Octave sympathetic acoustic momentum
                        modes[1].inputGain *= 1.35f;
                    }
                }

                if (resType == ResonatorType::ClampedTine)
                {
                    // Rhodes tine bell chime attack boost (modes 4 and 7: 5.85x and 8.4x)
                    // Delivers the signature crystalline metallic tine ping on initial strike
                    if (i == 4 || i == 7)
                    {
                        modes[i].inputGain = baseGain * (0.75f + 0.45f * materialBalance);
                    }
                }
                else if (resType == ResonatorType::ClampedReed)
                {
                    // Wurlitzer electrostatic comb: dominant odd harmonics (1, 3, 5, 7) produce
                    // the iconic hollow, reedy, cutting clarinet-like quack and bite
                    const int harmonicNumber = i + 1;
                    const bool isOdd = (harmonicNumber % 2 != 0);
                    const float reedWeight = isOdd ? 1.38f : 0.38f;
                    modes[i].inputGain *= reedWeight;
                }
            }

            updateInstantaneousFrequencies();
        }

        void updateInstantaneousFrequencies()
        {
            const float pitchDropRatio = (currentPitchDropST > 0.0f)
                ? std::pow(2.0f, currentPitchDropST / 12.0f)
                : 1.0f;
            const float totalRatio = pitchDropRatio * currentBendFactor * currentStretchRatio;
            const float nyquist = static_cast<float>(fs * 0.48);

            for (auto& m : modes)
            {
                if (!m.active) continue;

                const float effFreq = std::min(nyquist, m.baseFreq * totalRatio);
                const float w = static_cast<float>(2.0 * 3.141592653589793 * effFreq / fs);
                m.cosW = std::cos(w);
                m.sinW = std::sin(w);

                const float effFreq2 = std::min(nyquist, (m.baseFreq + m.detuneHz) * totalRatio);
                const float w2 = static_cast<float>(2.0 * 3.141592653589793 * effFreq2 / fs);
                m.cosW2 = std::cos(w2);
                m.sinW2 = std::sin(w2);
            }
        }

        double fs = 44100.0;
        ResonatorType resType = ResonatorType::String;
        float f0 = 440.0f;
        float baseDecay = 2.0f;
        float brightVal = 0.6f;
        float inharmVal = 0.1f;
        float contactPos = 0.2f;
        float pickupPos = 0.8f;
        float bloom = 0.3f;
        float materialBalance = 0.5f;

        float currentPitchDropST = 0.0f;
        float pitchDropDecayRate = 0.0f;
        int pitchUpdateCounter = 0;
        float totalEnergy = 0.0f;
        float targetBendFactor = 1.0f;
        float currentBendFactor = 1.0f;
        float currentStretchRatio = 1.0f;

        std::array<Mode, MAX_MODES> modes;
    };
}
