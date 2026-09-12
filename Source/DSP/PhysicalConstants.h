#pragma once

#include <cmath>
#include <array>

namespace ModelKeys
{
    constexpr int MAX_MODES = 32;
    constexpr int MAX_VOICES = 16;

    enum class ExciterType
    {
        Strike = 0,   // Felt/rubber/wood hammer (Piano, Rhodes, Mallets, Drums)
        Pluck = 1,    // Plectrum/pick/anvil snap (Guitar, Clavinet, Harpsichord)
        Bow = 2,      // Stick-slip continuous friction (Violin, Cello, Bowed Bar)
        Noise = 3     // Filtered granular/burst contact (Snare, Scrape, Brush)
    };

    enum class ResonatorType
    {
        String = 0,      // Harmonic string with inharmonicity (Guitar, Piano, Clavinet, Harp)
        ClampedTine = 1, // Clamped-free beam with massive tonebar (Rhodes, Kalimba)
        FreeBar = 2,     // Free-free acoustic tuned bar (Marimba, Vibraphone)
        Membrane = 3,    // 2D Circular Bessel membrane (Drums, Timpani)
        Plate = 4,       // Dense 2D metallic plate (Bowed bar, Gong, Bell)
        ClampedReed = 5, // Clamped steel reed with tip mass (Wurlitzer 200A)
        SteelPan = 6     // Tuned concave steelpan dish (Caribbean Steel Drum)
    };

    enum class BodyType
    {
        Off = 0,
        AcousticGuitar = 1,
        PianoSoundboard = 2,
        ViolinBody = 3,
        RhodesTonebar = 4,
        DrumShell = 5,
        WurliReedBar = 6,
        HarpSoundbox = 7,
        MarimbaResonator = 8,
        SteelDrumBarrel = 9
    };

    enum class PickupType
    {
        Electromagnetic = 0, // Magnetic coil with non-linear proximity saturation (Rhodes bark)
        Piezo = 1,           // Contact transducer (crisp attack, wide bandwidth)
        Microphone = 2,      // Acoustic room air transduction
        Electrostatic = 3,   // Polarized capacitive comb transduction (Wurlitzer reed quack/bite)
        ClavinetDualCoil = 4 // Dual single-coils with out-of-phase comb notch (funk bite)
    };

    enum class PreampType
    {
        Clean = 0, // 100% transparent linear gain, zero saturation or aliasing
        Tube = 1,  // Anti-aliased ADAA triode saturation with warm even/odd harmonics
        Tape = 2   // Analog tape saturation with soft saturation and gentle high roll-off
    };

    enum class ModulationType
    {
        Chorus = 0,     // Stereo quadrature delay modulation
        Tremolo = 1,    // 180-degree stereo auto-pan
        Flanger = 2,    // Short delay with resonant comb feedback
        Ensemble = 3,   // 3-phase dual-LFO BBD string ensemble (Solina/Juno)
        Phaser = 4,     // 6-pole allpass ladder with resonant sweep
        WowFlutter = 5, // Vintage tape motor capstan & belt drift
        Delay = 6       // Stereo ping-pong tape delay with damping
    };

    enum class ReverbType
    {
        Plate = 0,  // EMT 140 high-density plate shimmer
        Room = 1,   // Intimate studio wooden acoustic space
        Hall = 2,   // Expansive concert grand hall / cathedral bloom
        Spring = 3  // Dispersive spring tank with chirp/boing and metallic flutter
    };

    // Modal frequency ratios for different physical geometries (normalized to mode 0 = 1.0)
    struct ModalProfiles
    {
        // 1. Rhodes tine coupled to asymmetric tuned tonebar (Fender Rhodes)
        // Fundamental & tonebar resonance is harmonic (1, 2, 3, 4...) with prominent tine bell chime (5.85x, 8.4x)
        static constexpr std::array<float, MAX_MODES> ClampedTineRatios = {
            1.000f, 2.000f, 3.000f, 4.000f, 5.850f, 5.000f, 6.000f, 8.400f,
            7.000f, 8.000f, 9.000f, 10.000f, 11.000f, 12.000f, 13.000f, 14.000f,
            15.000f, 16.000f, 17.000f, 18.000f, 19.000f, 20.000f, 21.000f, 22.000f,
            23.000f, 24.000f, 25.000f, 26.000f, 27.000f, 28.000f, 29.000f, 30.000f
        };

        // 2. Wurlitzer 200A Clamped Steel Reed (electrostatic comb transduction produces harmonic series)
        static constexpr std::array<float, MAX_MODES> ClampedReedRatios = {
            1.000f,  2.000f,  3.000f,  4.000f,  5.000f,  6.000f,  7.000f,  8.000f,
            9.000f, 10.000f, 11.000f, 12.000f, 13.000f, 14.000f, 15.000f, 16.000f,
            17.000f, 18.000f, 19.000f, 20.000f, 21.000f, 22.000f, 23.000f, 24.000f,
            25.000f, 26.000f, 27.000f, 28.000f, 29.000f, 30.000f, 31.000f, 32.000f
        };

        // 3. Tuned acoustic bar with undercut arch (Marimba / Vibraphone)
        static constexpr std::array<float, MAX_MODES> FreeBarRatios = {
            1.000f, 3.980f, 9.250f, 16.300f, 24.600f, 34.200f, 45.100f, 57.300f,
            70.800f, 85.600f, 101.700f, 119.100f, 137.800f, 157.800f, 179.100f, 201.700f,
            225.50f, 250.50f, 276.70f, 304.10f, 332.70f, 362.50f, 393.50f, 425.70f,
            459.10f, 493.70f, 529.50f, 566.50f, 604.70f, 644.10f, 684.70f, 726.50f
        };

        // 4. Circular membrane Bessel function zeros j_{m,n} / j_{0,1} (Drums)
        static constexpr std::array<float, MAX_MODES> MembraneRatios = {
            1.000f, 1.593f, 2.135f, 2.295f, 2.653f, 2.917f, 3.155f, 3.500f,
            3.600f, 4.060f, 4.154f, 4.600f, 4.646f, 5.089f, 5.148f, 5.589f,
            5.650f, 6.100f, 6.180f, 6.620f, 6.700f, 7.150f, 7.230f, 7.680f,
            7.760f, 8.210f, 8.300f, 8.750f, 8.840f, 9.290f, 9.380f, 9.830f
        };

        // 5. Square/Rectangular metal plate modes (Bowed bar, Bells, Gongs)
        static constexpr std::array<float, MAX_MODES> PlateRatios = {
            1.000f, 1.340f, 1.710f, 2.120f, 2.510f, 2.980f, 3.420f, 3.890f,
            4.410f, 5.020f, 5.610f, 6.250f, 6.910f, 7.620f, 8.350f, 9.120f,
            9.920f, 10.75f, 11.61f, 12.50f, 13.42f, 14.37f, 15.35f, 16.36f,
            17.40f, 18.47f, 19.57f, 20.70f, 21.86f, 23.05f, 24.27f, 25.52f
        };

        // 6. Tuned concave steelpan dish (Caribbean Steel Drum / Lead Pan)
        // Note dome tuned to fundamental (1.00), octave along circumferential axis (2.00),
        // and octave+fifth along radial axis (3.00), with metallic upper boundary modes.
        static constexpr std::array<float, MAX_MODES> SteelPanRatios = {
            1.000f,  2.000f,  3.000f,  4.240f,  5.850f,  7.620f,  9.480f, 11.450f,
            13.620f, 15.900f, 18.300f, 20.800f, 23.400f, 26.100f, 28.900f, 31.800f,
            34.800f, 37.900f, 41.100f, 44.400f, 47.800f, 51.300f, 54.900f, 58.600f,
            62.400f, 66.300f, 70.300f, 74.400f, 78.600f, 82.900f, 87.300f, 91.800f
        };

        // Mode damping weightings (how quickly high modes decay relative to fundamental)
        static constexpr std::array<float, MAX_MODES> DefaultDampingWeights = {
            1.0f,  1.3f,  1.8f,  2.4f,  3.2f,  4.2f,  5.5f,  7.0f,
            9.0f,  11.5f, 14.5f, 18.0f, 22.0f, 27.0f, 33.0f, 40.0f,
            48.0f, 57.0f, 67.0f, 78.0f, 90.0f, 103.0f, 117.0f, 132.0f,
            148.0f, 165.0f, 183.0f, 202.0f, 222.0f, 243.0f, 265.0f, 288.0f
        };
    };
}
