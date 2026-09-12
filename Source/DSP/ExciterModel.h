#pragma once

#include "PhysicalConstants.h"
#include <cmath>
#include <algorithm>

namespace ModelKeys
{
    class ExciterModel
    {
    public:
        ExciterModel() = default;

        void prepare(double sampleRate)
        {
            fs = sampleRate > 0.0 ? sampleRate : 44100.0;
            reset();
        }

        void reset()
        {
            phase = 0.0f;
            active = false;
            sustained = false;
            currentForce = 0.0f;
            pluckDisplacement = 0.0f;
            bowSlipping = false;
            bowDisplacement = 0.0f;
            bowEnv = 0.0f;
            bowEnvState = BowEnvState::Idle;
            pitchBendFactor = 1.0f;
            bowLpY1 = 0.0f;
            bowLpY2 = 0.0f;
            bpX1 = 0.0f;
            bpX2 = 0.0f;
            bpY1 = 0.0f;
            bpY2 = 0.0f;
            rngState = 123456789u;
        }

        void setPitchBendFactor(float factor) noexcept
        {
            pitchBendFactor = std::clamp(factor, 0.25f, 4.0f);
        }

        void trigger(float noteFreq, float noteVelocity, ExciterType type,
                     float hardness, float position, float stiffness,
                     float bowSpeedParam, float bowForceParam,
                     float strikeClickParam = 0.35f, float rosinGritParam = 0.35f,
                     float particleDensityParam = 0.6f, float particleScatterMs = 25.0f)
        {
            exciterType = type;
            freq = std::clamp(noteFreq, 20.0f, 16000.0f);
            velocity = std::clamp(noteVelocity, 0.02f, 1.0f);
            contactPos = std::clamp(position, 0.02f, 0.98f);
            active = true;
            sustained = true;
            phase = 0.0f;

            // 1. Strike contact duration and non-linear elastomeric felt compaction (Pianoteq-grade touch dynamics)
            const float dynamicHardness = std::clamp(hardness * std::pow(velocity, 0.46f) + 0.32f * (velocity - 0.5f), 0.04f, 0.98f);
            dynamicHardnessVal = dynamicHardness;
            stiffnessVal = std::clamp(stiffness, 0.05f, 0.95f);
            strikeClick = std::clamp(strikeClickParam, 0.0f, 1.0f);
            rosinGrit = std::clamp(rosinGritParam, 0.0f, 1.0f);
            particleDensity = std::clamp(particleDensityParam, 0.02f, 1.0f);

            // Physical hammer contact duration scaling:
            // Grand piano & mallet physical contact: ~1.2 ms (bass) down to 0.50 ms (C4) and 0.18 ms (treble)
            // Power law scaling: T_c propto freq^(-0.35) * velocity^(-0.22)
            const float pitchRefFactor = std::pow(std::max(27.5f, freq) / 261.63f, -0.35f);
            const float velFactor = std::pow(std::clamp(velocity, 0.05f, 1.0f), -0.22f);
            const float hardnessFactor = (1.15f - 0.70f * dynamicHardness);
            const float contactSec = 0.00060f * pitchRefFactor * velFactor * hardnessFactor;
            const float clampedDurationSec = std::clamp(contactSec, 0.00015f, 0.0022f);
            contactDurationSamples = std::max(2.0f, static_cast<float>(clampedDurationSec * fs));
            hertzianP = 1.8f + 1.2f * dynamicHardness;
            peakForce = velocity * (0.75f + 0.45f * dynamicHardness);

            // Strike click transient: authentic felt/wood core knock transient
            const float clickSec = std::clamp(0.0012f * (1.2f - 0.6f * dynamicHardness), 0.0002f, 0.0025f);
            clickDurationSamples = std::max(2.0f, static_cast<float>(clickSec * fs));
            clickFreq = 1800.0f + 1200.0f * dynamicHardness;

            // 2. Pluck / Tangent initialization
            // Scaled potential energy release with pitch-dependent release timing (thicker low strings release over a slightly longer displacement time)
            pluckDisplacement = velocity * (1.55f + 0.85f * dynamicHardness);
            const float pluckSec = std::clamp((0.28f / freq) * (0.85f - 0.35f * dynamicHardness), 0.00035f, 0.0035f);
            pluckReleaseSamples = std::max(4.0f, static_cast<float>(pluckSec * fs));

            // 3. Bow stick-slip initialization with dynamic velocity-scaled player attack (fast bite on forte, graceful swell on piano)
            bowSpeed = std::clamp(bowSpeedParam * (0.4f + 0.6f * velocity), 0.05f, 1.5f);
            bowForce = std::clamp(bowForceParam * (0.3f + 0.7f * velocity), 0.05f, 1.5f);
            bowSlipping = false;
            bowDisplacement = 0.0f;
            const float attackSec = (0.003f + 0.015f * (1.0f - velocity)) * (1.15f - 0.50f * dynamicHardness);
            bowAttackSamples = std::max(10.0f, static_cast<float>(attackSec * fs));
            const float releaseSec = 0.035f;
            bowReleaseSamples = std::max(16.0f, static_cast<float>(releaseSec * fs));
            bowEnv = 0.0f;
            bowEnvState = BowEnvState::Attack;
            bowPeriodSamples = static_cast<float>(fs / freq);
            bowPhase = 0.0f;
            bowBeta = std::clamp(contactPos, 0.08f, 0.22f);
            bowVibratoPhase = 0.0f;
            bowAge = 0;
            attackBiteSamples = std::max(8.0f, static_cast<float>(0.005f * fs)); // ~5ms initial hair bite

            // Bridge admittance filter cutoff (wide open 8.5 kHz - 15.0 kHz: singing brilliance and rosin rasp)
            const float fc = std::clamp(8500.0f + 6500.0f * dynamicHardness, 4000.0f, static_cast<float>(fs * 0.46));
            bowLpCoeff = std::clamp(static_cast<float>(2.0 * 3.141592653589793 * fc / fs), 0.05f, 0.95f);
            bowLpY1 = 0.0f;
            bowLpY2 = 0.0f;

            // 4. Granular micro-impact scatter initialization
            const float scatterSec = std::clamp(particleScatterMs * 0.001f, 0.001f, 0.15f);
            scatterDurationSamples = std::max(8.0f, static_cast<float>(scatterSec * fs));

            // Resonant grain filter setup (2-pole bandpass)
            const float grainCenterFreq = std::clamp(300.0f + 7000.0f * dynamicHardness, 150.0f, static_cast<float>(fs * 0.45));
            const float w0 = static_cast<float>(2.0 * 3.141592653589793 * grainCenterFreq / fs);
            bpR = 0.82f + 0.12f * dynamicHardness;
            bpCosW = std::cos(w0);
            bpSinW = std::sin(w0);
            bpX1 = 0.0f;
            bpX2 = 0.0f;
            bpY1 = 0.0f;
            bpY2 = 0.0f;
        }

        void release()
        {
            sustained = false;
            if (exciterType == ExciterType::Bow)
            {
                bowEnvState = BowEnvState::Release;
            }
        }

        // Process one sample and return exciter contact force F_c
        float processSample(float resonatorVelocityFeedback = 0.0f)
        {
            if (!active) return 0.0f;

            float force = 0.0f;

            switch (exciterType)
            {
                case ExciterType::Strike:
                {
                    // Closed-form Hertzian elastomeric contact collision:
                    // F(t) = F_0 * sin^p(pi * t / tau_c)
                    if (phase < contactDurationSamples)
                    {
                        const float normalizedTime = phase / contactDurationSamples; // 0..1
                        const float sineHalf = std::sin(3.141592653589793f * normalizedTime);

                        // Physical relative velocity feedback (Hunt-Crossley elastomeric contact collision):
                        // v_rel = v_hammer - v_string.
                        // Moving against hammer (negative feedback) stiffens compression; moving away cushions it.
                        const float relVelFactor = 1.0f - std::clamp(resonatorVelocityFeedback * 0.16f, -0.45f, 0.45f);
                        force = peakForce * relVelFactor * std::pow(std::max(0.0f, sineHalf), hertzianP);

                        // Mallet mechanical contact knock transient (felt hammer, neoprene tip, or wooden shank)
                        if (phase < clickDurationSamples && strikeClick > 0.01f)
                        {
                            const float clickNorm = phase / clickDurationSamples;
                            const float clickEnv = std::sin(3.141592653589793f * clickNorm) * (1.0f - clickNorm);
                            const float clickAngle = static_cast<float>(2.0 * 3.141592653589793 * clickFreq * (phase / fs));
                            const float clickWave = std::sin(clickAngle);
                            force += strikeClick * velocity * 0.55f * clickEnv * clickWave * relVelFactor;
                        }

                        phase += 1.0f;
                    }
                    else
                    {
                        active = false;
                        force = 0.0f;
                    }
                    break;
                }

                case ExciterType::Pluck:
                {
                    // Physical Plectrum / Fingernail Snap Release:
                    // 1. Ultra-fast 2-sample rise from F(0) = 0 to peak force (C1-continuous, click-free)
                    // 2. Asymmetric exponential snap-back decay (tau_snap = 4-12 samples)
                    // 3. Plectrum scrape burst for tactile string twang and pluck bite
                    const float riseSamples = 2.0f;
                    const float snapDecayRate = 1.0f / std::max(3.0f, 14.0f * (1.15f - 0.75f * stiffnessVal));
                    const float maxPluckSamples = std::min(pluckReleaseSamples, 80.0f);

                    if (phase < maxPluckSamples)
                    {
                        float baseForce = 0.0f;
                        if (phase <= riseSamples)
                        {
                            // Smooth sine quarter-wave rise: F(0) = 0, F(2) = pluckDisplacement
                            const float riseNorm = phase / riseSamples;
                            baseForce = pluckDisplacement * std::sin(1.57079632679f * riseNorm);
                        }
                        else
                        {
                            // Exponential release snap-back as string slips free from pick/nail
                            const float decayPhase = phase - riseSamples;
                            baseForce = pluckDisplacement * std::exp(-decayPhase * snapDecayRate);
                        }

                        // Tactile plectrum edge scrape transient (for guitars/clavinet, muted for soft harp pads)
                        float scrapeForce = 0.0f;
                        if (dynamicHardnessVal > 0.35f || strikeClick > 0.10f)
                        {
                            const float scrapeEnv = std::exp(-phase * 0.25f);
                            const float rnd = nextRandomFloat() * 2.0f - 1.0f;
                            scrapeForce = (0.08f + 0.25f * strikeClick) * rnd * pluckDisplacement * scrapeEnv;
                        }

                        force = baseForce + scrapeForce;
                        phase += 1.0f;
                    }
                    else
                    {
                        active = false;
                        force = 0.0f;
                    }
                    break;
                }

                case ExciterType::Bow:
                {
                    if (bowEnvState == BowEnvState::Attack)
                    {
                        bowEnv += 1.0f / bowAttackSamples;
                        if (bowEnv >= 1.0f)
                        {
                            bowEnv = 1.0f;
                            bowEnvState = BowEnvState::Sustain;
                        }
                    }
                    else if (bowEnvState == BowEnvState::Release)
                    {
                        bowEnv -= 1.0f / bowReleaseSamples;
                        if (bowEnv <= 0.0f)
                        {
                            bowEnv = 0.0f;
                            bowEnvState = BowEnvState::Idle;
                            active = false;
                            currentForce = 0.0f;
                            return 0.0f;
                        }
                    }
                    else if (bowEnvState == BowEnvState::Idle)
                    {
                        active = false;
                        currentForce = 0.0f;
                        return 0.0f;
                    }

                    // Responsive player envelope ramp: immediate hair contact with fast acceleration
                    const float smoothEnv = 0.35f + 0.65f * std::sin(1.57079632679f * bowEnv);

                    // Expressive human player vibrato LFO (5.2 Hz, smooth 200ms onset)
                    bowAge++;
                    const float vibOnset = std::clamp((static_cast<float>(bowAge) - 0.15f * static_cast<float>(fs)) / (0.30f * static_cast<float>(fs)), 0.0f, 1.0f);
                    bowVibratoPhase += static_cast<float>(2.0 * 3.141592653589793 * 5.2 / fs);
                    if (bowVibratoPhase >= 6.2831853f) bowVibratoPhase -= 6.2831853f;
                    const float vibForceMod = 1.0f + 0.08f * std::sin(bowVibratoPhase) * vibOnset;

                    // Effective fundamental Helmholtz oscillation period (tracks pitch bend dynamically)
                    const float effPeriod = std::max(2.0f, bowPeriodSamples / pitchBendFactor);

                    // Track Helmholtz phase
                    bowPhase += 1.0f;
                    if (bowPhase >= effPeriod)
                        bowPhase -= effPeriod;
                    const float normPhase = std::clamp(bowPhase / effPeriod, 0.0f, 1.0f);

                    // Effective bow speed and normal force scale with envelope and vibrato
                    const float effBowSpeed = bowSpeed * smoothEnv;
                    const float normalForce = bowForce * (0.45f + 0.55f * velocity) * smoothEnv * vibForceMod;

                    // String velocity at contact point: feedback modulates corner timing
                    const float scaledFeedback = std::clamp(resonatorVelocityFeedback * 0.12f, -1.5f, 1.5f);
                    const float relVel = effBowSpeed - scaledFeedback;

                    // Raman physical Helmholtz stick-slip sawtooth:
                    // In stick phase (normPhase in [bowBeta, 1.0]): string displacement rises with bow hair (-1.0 to +1.0)
                    // In slip phase (normPhase in [0, bowBeta]): string snaps back (+1.0 to -1.0)
                    float rawHelmholtz = 0.0f;
                    if (normPhase < bowBeta)
                    {
                        const float u = normPhase / bowBeta; // 0 to 1
                        rawHelmholtz = 1.0f - 2.0f * u;
                    }
                    else
                    {
                        const float s = (normPhase - bowBeta) / (1.0f - bowBeta); // 0 to 1
                        rawHelmholtz = s * 2.0f - 1.0f;
                    }

                    // Friction non-linearity with thermal rosin micro-drift (living acoustic string friction)
                    const float mu_s = 0.85f + 0.35f * rosinGrit;
                    const float mu_d = 0.25f + 0.10f * rosinGrit;
                    const float v0 = 0.20f;
                    const float velFactor = 1.0f / (1.0f + (relVel * relVel) / (v0 * v0));
                    const float mu = mu_d + (mu_s - mu_d) * velFactor;

                    bowThermalPhase += 0.00035f;
                    if (bowThermalPhase > 6.2831853f) bowThermalPhase -= 6.2831853f;
                    const float thermalFrictionMod = 1.0f + 0.035f * rosinGrit * std::sin(bowThermalPhase * 5.17f);
                    const float frictionForce = normalForce * mu * rawHelmholtz * thermalFrictionMod;

                    // Rosin surface grit noise (tactile acoustic bow hair grain)
                    const float gritRand = nextRandomFloat() * 2.0f - 1.0f;
                    const float gritNoise = rosinGrit * 0.12f * normalForce * gritRand * smoothEnv;

                    const float rawForce = frictionForce + gritNoise;

                    // Mechanical bridge admittance filtering (smooth 1-pole preserving singing brilliance)
                    bowLpY1 += bowLpCoeff * (rawForce - bowLpY1);

                    // Blend filtered bridge drive with direct harmonic bite
                    const float bridgeForce = 0.55f * bowLpY1 + 0.45f * rawForce;
                    float finalForce = bridgeForce * 0.038f;

                    // Initial Martelé Hair Bite: tactile attack crunch in the first 4-8 ms
                    if (bowAge < static_cast<uint64_t>(attackBiteSamples))
                    {
                        const float biteNorm = static_cast<float>(bowAge) / attackBiteSamples;
                        const float biteEnv = (1.0f - biteNorm) * std::exp(-2.5f * biteNorm);
                        finalForce += velocity * (0.05f + 0.09f * dynamicHardnessVal) * biteEnv * (normPhase < bowBeta ? -1.0f : 1.0f);
                    }

                    force = std::clamp(finalForce, -0.40f, 0.40f);
                    break;
                }

                case ExciterType::Noise:
                {
                    // Granular Micro-Impact Particle Cluster (Poisson scattered contact pulses)
                    if (phase < scatterDurationSamples)
                    {
                        const float normTime = phase / scatterDurationSamples;
                        // Decay envelope of the scatter window
                        const float env = (1.0f - normTime) * std::exp(-2.8f * normTime);

                        // Stochastic Poisson impact trigger
                        float impulse = 0.0f;
                        const float pThresh = std::clamp(particleDensity * 0.35f, 0.02f, 0.70f);
                        if (nextRandomFloat() < pThresh)
                        {
                            const float grainAmp = (nextRandomFloat() * 2.0f - 1.0f);
                            impulse = grainAmp * env * velocity * 3.5f;
                        }

                        // 2-Pole Resonant Bandpass filter to shape grain tone (wood / wire / brush)
                        const float filtered = (1.0f - bpR) * impulse + 2.0f * bpR * bpCosW * bpY1 - (bpR * bpR) * bpY2;
                        bpY2 = bpY1;
                        bpY1 = filtered;

                        force = std::clamp(filtered * 1.4f, -0.9f, 0.9f);
                        phase += 1.0f;
                    }
                    else
                    {
                        active = false;
                        force = 0.0f;
                    }
                    break;
                }
            }

            currentForce = force;
            return force;
        }

        bool isActive() const noexcept { return active; }
        ExciterType getType() const noexcept { return exciterType; }
        float getContactPosition() const noexcept { return contactPos; }
        float getCurrentForce() const noexcept { return currentForce; }

    private:
        // Fast, deterministic, thread-safe pseudo-random number generator
        inline float nextRandomFloat() noexcept
        {
            rngState = rngState * 1664525u + 1013904223u;
            return static_cast<float>(rngState & 0x00FFFFFF) / static_cast<float>(0x01000000);
        }

        double fs = 44100.0;
        ExciterType exciterType = ExciterType::Strike;
        bool active = false;
        bool sustained = false;
        uint32_t rngState = 123456789u;

        float freq = 440.0f;
        float velocity = 0.8f;
        float contactPos = 0.2f;
        float stiffnessVal = 0.5f;
        float strikeClick = 0.35f;
        float rosinGrit = 0.35f;
        float particleDensity = 0.6f;

        float phase = 0.0f;
        float contactDurationSamples = 100.0f;
        float hertzianP = 2.0f;
        float peakForce = 1.0f;
        float clickDurationSamples = 50.0f;
        float clickFreq = 3500.0f;

        float pluckDisplacement = 0.0f;
        float pluckReleaseSamples = 50.0f;

        float dynamicHardnessVal = 0.5f;
        float bowSpeed = 0.5f;
        float bowForce = 0.5f;
        bool bowSlipping = false;
        float bowDisplacement = 0.0f;
        enum class BowEnvState { Idle, Attack, Sustain, Release };
        BowEnvState bowEnvState = BowEnvState::Idle;
        float bowEnv = 0.0f;
        float bowAttackSamples = 2400.0f;
        float bowReleaseSamples = 2000.0f;
        float bowPeriodSamples = 100.0f;
        float bowPhase = 0.0f;
        float bowBeta = 0.12f;
        float bowVibratoPhase = 0.0f;
        uint64_t bowAge = 0;
        float attackBiteSamples = 220.0f;
        float bowThermalPhase = 0.0f;
        float pitchBendFactor = 1.0f;
        float bowLpY1 = 0.0f;
        float bowLpY2 = 0.0f;
        float bowLpCoeff = 0.35f;

        float scatterDurationSamples = 500.0f;
        float bpR = 0.85f;
        float bpCosW = 1.0f;
        float bpSinW = 0.0f;
        float bpX1 = 0.0f, bpX2 = 0.0f, bpY1 = 0.0f, bpY2 = 0.0f;

        float currentForce = 0.0f;
    };
}
