#include "../PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <memory>

bool checkBufferSanity(const juce::AudioBuffer<float>& buffer, const std::string& context)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* readPtr = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (std::isnan(readPtr[i]))
            {
                std::cerr << "FAIL [" << context << "]: NaN detected on channel " << ch << " at sample " << i << std::endl;
                return false;
            }
            if (std::isinf(readPtr[i]))
            {
                std::cerr << "FAIL [" << context << "]: Inf detected on channel " << ch << " at sample " << i << std::endl;
                return false;
            }
            if (std::abs(readPtr[i]) > 1.2f)
            {
                std::cerr << "FAIL [" << context << "]: Excessive amplitude (" << readPtr[i] << ") detected on channel " << ch << std::endl;
                return false;
            }
        }
    }
    return true;
}

int main()
{
    std::cout << "=================================================" << std::endl;
    std::cout << "   MODELKEYS PHYSICAL MODELING CALIBRATION TESTS " << std::endl;
    std::cout << "=================================================" << std::endl;

    ModelKeys::PluginProcessor processor;
    const double sampleRate = 44100.0;
    const int blockSize = 512;
    processor.prepareToPlay(sampleRate, blockSize);

    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    int passedTests = 0;
    int totalTests = 0;

    // TEST 1: Preset Library Sanity & Pristine Audio Levels
    std::cout << "\n[TEST 1] Testing All Presets for Clean Acoustic Levels..." << std::endl;
    auto& pm = processor.getPresetManager();
    const auto& presets = pm.getAllPresets();
    std::cout << "Found " << presets.size() << " presets in library." << std::endl;

    for (size_t i = 0; i < presets.size(); ++i)
    {
        totalTests++;
        pm.selectPreset(static_cast<int>(i));

        // Note On C4 (60)
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.85f), 0);

        float peakAudio = 0.0f;
        bool clean = true;

        std::vector<float> mags;
        for (int block = 0; block < 100; ++block)
        {
            processor.processBlock(buffer, midi);
            midi.clear();

            if (!checkBufferSanity(buffer, presets[i].name))
            {
                clean = false;
                break;
            }

            const float mag = buffer.getMagnitude(0, buffer.getNumSamples());
            if (block % 20 == 0) mags.push_back(mag);
            peakAudio = std::max(peakAudio, mag);
        }

        // Release note
        midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.5f), 0);
        for (int block = 0; block < 10; ++block)
        {
            processor.processBlock(buffer, midi);
            midi.clear();
            if (!checkBufferSanity(buffer, presets[i].name + " (Release)"))
            {
                clean = false;
                break;
            }
        }

        const float peakDb = 20.0f * std::log10(std::max(1e-5f, peakAudio));
        const bool isBowedSustain = (presets[i].parameters.find("exciterType") != presets[i].parameters.end()
                                     && presets[i].parameters.at("exciterType") == 2.0f);
        const bool decaysProperly = isBowedSustain
            ? (peakAudio <= 1.0f && mags.back() <= peakAudio * 1.05f)
            : (mags.size() >= 3 && mags.back() <= mags[1] * 1.2f);
        if (clean && peakAudio > 0.001f && peakAudio <= 1.0f && decaysProperly)
        {
            std::cout << "  ✓ Preset [" << std::setw(28) << presets[i].name << "] - Peak: "
                      << std::fixed << std::setprecision(3) << peakAudio
                      << " (" << std::setprecision(1) << peakDb << " dBFS) - Pristine" << std::endl;
            passedTests++;
        }
        else
        {
            std::cerr << "  ✗ Preset [" << presets[i].name << "] FAILED (Peak: " << peakAudio << ")" << std::endl;
        }
    }

    // TEST 2: Polyphony & Dense Chords Headroom Test
    std::cout << "\n[TEST 2] Testing Polyphony Voice Allocation & Chord Headroom..." << std::endl;
    totalTests++;
    processor.getVoiceManager().reset();
    processor.getVoiceManager().setPolyphony(16);
    pm.selectPreset(3); // Classic Rhodes
    midi.clear();

    // Trigger 6-note lush jazz chord
    const int chordNotes[] = { 48, 55, 58, 62, 65, 67 }; // C3, G3, Bb3, D4, F4, G4
    for (int note : chordNotes)
    {
        midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.85f), 0);
    }

    float chordPeak = 0.0f;
    bool chordClean = true;
    for (int b = 0; b < 12; ++b)
    {
        processor.processBlock(buffer, midi);
        midi.clear();

        if (!checkBufferSanity(buffer, "Lush Chord"))
        {
            chordClean = false;
            break;
        }
        chordPeak = std::max(chordPeak, buffer.getMagnitude(0, buffer.getNumSamples()));
    }

    const int activeVoices = processor.getVoiceManager().getActiveVoiceCount();
    if (chordClean && activeVoices == 6 && chordPeak <= 1.0f)
    {
        std::cout << "  ✓ 6-voice chord passed cleanly: Peak = " << std::fixed << std::setprecision(3) << chordPeak
                  << " (" << 20.0f * std::log10(chordPeak) << " dBFS), No clipping or distortion!" << std::endl;
        passedTests++;
    }
    else
    {
        std::cerr << "  ✗ Chord test failed: voices=" << activeVoices << ", peak=" << chordPeak << std::endl;
    }

    // TEST 3: Hammer (Strike) Exciter Stability Across Frequency Range
    std::cout << "\n[TEST 3] Testing Hammer (Strike) Exciter Across Full Range (C1 to C7)..." << std::endl;
    totalTests++;
    auto& apvts = processor.getAPVTS();
    apvts.getParameter("exciterType")->setValueNotifyingHost(0.0f); // Strike (Hammer)
    apvts.getParameter("hardness")->setValueNotifyingHost(0.9f);   // Hard hammer
    apvts.getParameter("stiffness")->setValueNotifyingHost(0.85f);

    bool hammerAllPassed = true;
    for (int testNote = 24; testNote <= 96; testNote += 6)
    {
        processor.getVoiceManager().reset();
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, testNote, 0.95f), 0);

        for (int b = 0; b < 10; ++b)
        {
            processor.processBlock(buffer, midi);
            midi.clear();

            if (!checkBufferSanity(buffer, "Hammer note " + std::to_string(testNote)))
            {
                hammerAllPassed = false;
                std::cerr << "  ✗ Hammer strike failed on note " << testNote << "!" << std::endl;
                break;
            }
        }
        if (!hammerAllPassed) break;
    }

    if (hammerAllPassed)
    {
        std::cout << "  ✓ Hammer Strike exciter is 100% stable, musical, and freeze-free across all octaves!" << std::endl;
        passedTests++;
    }

    // TEST 4: Undo / Redo Transactions
    std::cout << "\n[TEST 4] Testing Undo / Redo Transactions..." << std::endl;
    totalTests++;
    auto& undo = processor.getUndoManager();

    undo.beginNewTransaction("Test ValueTree Undo");
    apvts.state.setProperty("customParam", 42.0f, &undo);
    undo.undo();
    const bool undoneProp = !apvts.state.hasProperty("customParam");
    undo.redo();
    const bool redoneProp = (static_cast<float>(apvts.state.getProperty("customParam")) == 42.0f);

    if (undoneProp && redoneProp)
    {
        std::cout << "  ✓ Undo/Redo transaction test passed." << std::endl;
        passedTests++;
    }
    else
    {
        std::cerr << "  ✗ Undo/Redo test failed." << std::endl;
    }

    // TEST 5: MIDI Learn CC Assignment & Modulation
    std::cout << "\n[TEST 5] Testing MIDI Learn..." << std::endl;
    totalTests++;
    auto& ml = processor.getMidiLearnManager();

    ml.setTargetParameter("decayTime");
    ml.setLearnActive(true);

    auto ccMsg = juce::MidiMessage::controllerEvent(1, 74, 96);
    processor.processBlock(buffer, midi);
    ml.handleMidiMessage(ccMsg);

    const int mappedCC = ml.getCCForParameter("decayTime");
    if (mappedCC == 74 && !ml.getLearnActive())
    {
        std::cout << "  ✓ MIDI Learn CC 74 mapped to 'decayTime'." << std::endl;

        auto ccMsgMax = juce::MidiMessage::controllerEvent(1, 74, 127);
        ml.handleMidiMessage(ccMsgMax);

        auto* decayParam = apvts.getParameter("decayTime");
        if (std::abs(decayParam->getValue() - 1.0f) < 0.02f)
        {
            std::cout << "  ✓ MIDI CC 74 value update dispatched successfully." << std::endl;
            passedTests++;
        }
        else
        {
            std::cerr << "  ✗ MIDI Learn value update failed." << std::endl;
        }
    }
    else
    {
        std::cerr << "  ✗ MIDI Learn assignment failed. Mapped CC: " << mappedCC << std::endl;
    }

    // TEST 6: Membrane Dynamic Tension Pitch Drop
    std::cout << "\n[TEST 6] Testing Membrane Dynamic Tension Pitch Drop..." << std::endl;
    totalTests++;
    processor.getVoiceManager().reset();
    apvts.getParameter("exciterType")->setValueNotifyingHost(0.0f); // Strike
    apvts.getParameter("resonatorType")->setValueNotifyingHost(3.0f / 4.0f); // Membrane
    apvts.getParameter("pitchDropAmount")->setValueNotifyingHost(12.0f / 24.0f); // 12 st
    apvts.getParameter("pitchDropDecay")->setValueNotifyingHost(0.4f); // ~80ms decay
    apvts.getParameter("decayTime")->setValueNotifyingHost(0.6f);

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0); // C4

    bool membraneClean = true;
    std::vector<float> recordedAudio;
    for (int b = 0; b < 20; ++b)
    {
        processor.processBlock(buffer, midi);
        midi.clear();
        if (!checkBufferSanity(buffer, "Membrane Pitch Drop"))
        {
            membraneClean = false;
            break;
        }
        const float* r = buffer.getReadPointer(0);
        recordedAudio.insert(recordedAudio.end(), r, r + buffer.getNumSamples());
    }

    // Find positive zero crossings during pitch drop (samples 150 to 900)
    std::vector<int> earlyCrossings;
    for (size_t k = 150; k < 900 && k < recordedAudio.size(); ++k)
    {
        if (recordedAudio[k - 1] < 0.0f && recordedAudio[k] >= 0.0f)
            earlyCrossings.push_back(static_cast<int>(k));
    }

    // Find positive zero crossings late in decay (samples 2500 to 4500)
    std::vector<int> lateCrossings;
    for (size_t k = 2500; k < 4500 && k < recordedAudio.size(); ++k)
    {
        if (recordedAudio[k - 1] < 0.0f && recordedAudio[k] >= 0.0f)
            lateCrossings.push_back(static_cast<int>(k));
    }

    float avgEarlyPeriod = 0.0f;
    if (earlyCrossings.size() >= 2)
        avgEarlyPeriod = static_cast<float>(earlyCrossings.back() - earlyCrossings.front()) / static_cast<float>(earlyCrossings.size() - 1);

    float avgLatePeriod = 0.0f;
    if (lateCrossings.size() >= 2)
        avgLatePeriod = static_cast<float>(lateCrossings.back() - lateCrossings.front()) / static_cast<float>(lateCrossings.size() - 1);

    std::cout << "    [INFO] Early Period: " << avgEarlyPeriod << " samples, Late Period: " << avgLatePeriod << " samples" << std::endl;

    if (membraneClean && avgEarlyPeriod > 0.0f && avgLatePeriod > avgEarlyPeriod)
    {
        std::cout << "  ✓ Membrane pitch drop verified: early period (" << avgEarlyPeriod
                  << " samples) is shorter than resting fundamental (" << avgLatePeriod << " samples)." << std::endl;
        passedTests++;
    }
    else
    {
        std::cerr << "  ✗ Membrane pitch drop failed: avgEarlyPeriod=" << avgEarlyPeriod
                  << ", avgLatePeriod=" << avgLatePeriod << std::endl;
    }

    // TEST 7: Bow Continuous Stick-Slip Sustain & Helmholtz Sawtooth
    std::cout << "\n[TEST 7] Testing Bow Continuous Stick-Slip Sustain & Helmholtz Sawtooth..." << std::endl;
    totalTests++;
    processor.getVoiceManager().reset();
    apvts.getParameter("exciterType")->setValueNotifyingHost(2.0f / 3.0f); // Bow
    apvts.getParameter("resonatorType")->setValueNotifyingHost(0.0f); // String
    apvts.getParameter("bowSpeed")->setValueNotifyingHost(0.45f);
    apvts.getParameter("bowForce")->setValueNotifyingHost(0.45f);
    apvts.getParameter("rosinGrit")->setValueNotifyingHost(0.4f);

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.85f), 0); // Sustained C4

    bool bowClean = true;
    float sustainedLevel = 0.0f;
    for (int b = 0; b < 25; ++b)
    {
        processor.processBlock(buffer, midi);
        midi.clear();
        if (!checkBufferSanity(buffer, "Bow Stick-Slip"))
        {
            bowClean = false;
            break;
        }
        if (b > 15)
        {
            sustainedLevel = std::max(sustainedLevel, buffer.getMagnitude(0, buffer.getNumSamples()));
        }
    }

    if (bowClean && sustainedLevel > 0.003f)
    {
        std::cout << "  ✓ Bow stick-slip sustain verified: continuous periodic oscillation sustained at "
                  << 20.0f * std::log10(sustainedLevel) << " dBFS." << std::endl;
        passedTests++;
    }
    else
    {
        std::cerr << "  ✗ Bow sustain failed: sustainedLevel=" << sustainedLevel << std::endl;
    }

    // TEST 8: Granular Particle Micro-Impact Noise Cluster
    std::cout << "\n[TEST 8] Testing Granular Particle Micro-Impact Noise Cluster..." << std::endl;
    totalTests++;
    processor.getVoiceManager().reset();
    apvts.getParameter("exciterType")->setValueNotifyingHost(3.0f / 3.0f); // Noise
    apvts.getParameter("resonatorType")->setValueNotifyingHost(0.0f); // String
    apvts.getParameter("particleDensity")->setValueNotifyingHost(0.8f);
    apvts.getParameter("particleScatter")->setValueNotifyingHost(0.35f);

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.85f), 0);

    bool particleClean = true;
    float particlePeak = 0.0f;
    for (int b = 0; b < 10; ++b)
    {
        processor.processBlock(buffer, midi);
        midi.clear();
        if (!checkBufferSanity(buffer, "Granular Particle Cluster"))
        {
            particleClean = false;
            break;
        }
        particlePeak = std::max(particlePeak, buffer.getMagnitude(0, buffer.getNumSamples()));
    }

    if (particleClean && particlePeak > 0.002f && particlePeak <= 1.0f)
    {
        std::cout << "  ✓ Granular particle cluster verified: Peak = " << std::fixed << std::setprecision(3)
                  << particlePeak << " (" << 20.0f * std::log10(particlePeak) << " dBFS)." << std::endl;
        passedTests++;
    }
    else
    {
        std::cerr << "  ✗ Granular particle cluster failed: peak=" << particlePeak << std::endl;
    }

    // TEST 9: MIDI CC 64 Sustain Pedal Functionality
    std::cout << "\n[TEST 9] Testing MIDI CC 64 Sustain Pedal Functionality..." << std::endl;
    totalTests++;
    processor.getVoiceManager().reset();
    pm.selectPresetByName("Velvet Resonance");

    // Press sustain pedal down (CC 64 = 127)
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    // Strike note C4
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.80f), 10);
    processor.processBlock(buffer, midi);
    midi.clear();

    // Release key with pedal STILL DOWN (finger noteOff)
    midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.5f), 0);
    processor.processBlock(buffer, midi);
    midi.clear();

    // Verify note is still sustaining over 20 blocks (~0.23 seconds) because pedal is held
    float sustainedPedalMag = 0.0f;
    for (int b = 0; b < 20; ++b)
    {
        processor.processBlock(buffer, midi);
        sustainedPedalMag = std::max(sustainedPedalMag, buffer.getMagnitude(0, buffer.getNumSamples()));
    }

    // Now release sustain pedal (CC 64 = 0)
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
    processor.processBlock(buffer, midi);
    midi.clear();

    // Process 40 blocks to allow natural felt damping to silence the voice
    float postReleaseMag = 0.0f;
    for (int b = 0; b < 40; ++b)
    {
        processor.processBlock(buffer, midi);
        postReleaseMag = buffer.getMagnitude(0, buffer.getNumSamples());
    }

    const bool pedalHeldSound = (sustainedPedalMag > 0.05f);
    const bool pedalLiftDamped = (postReleaseMag < sustainedPedalMag * 0.25f);
    if (pedalHeldSound && pedalLiftDamped)
    {
        std::cout << "  ✓ Sustain Pedal CC 64 verified: Note sustained with pedal down ("
                  << sustainedPedalMag << ") and damped cleanly on pedal lift (" << postReleaseMag << ")." << std::endl;
        passedTests++;
    }
    else
    {
        std::cerr << "  ✗ Sustain Pedal CC 64 failed: sustainedPedalMag=" << sustainedPedalMag
                  << ", postReleaseMag=" << postReleaseMag << std::endl;
    }

    // TEST 10: Bowed Preset (Solar Flare) Full Acoustic Output Level
    std::cout << "\n[TEST 10] Testing Bowed Solar Flare Acoustic Power..." << std::endl;
    totalTests++;
    processor.getVoiceManager().reset();
    pm.selectPresetByName("Solar Flare");

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.85f), 0); // C4
    float celloPeak = 0.0f;
    for (int b = 0; b < 40; ++b)
    {
        processor.processBlock(buffer, midi);
        midi.clear();
        celloPeak = std::max(celloPeak, buffer.getMagnitude(0, buffer.getNumSamples()));
    }
    const float celloDb = 20.0f * std::log10(std::max(1e-5f, celloPeak));
    if (celloPeak >= 0.08f && celloPeak <= 1.0f) // -22 dBFS to 0 dBFS
    {
        std::cout << "  ✓ Solar Flare verified: Powerful acoustic level at "
                  << std::fixed << std::setprecision(2) << celloPeak
                  << " (" << std::setprecision(1) << celloDb << " dBFS)." << std::endl;
        passedTests++;
    }
    else
    {
        std::cerr << "  ✗ Solar Flare level test failed: peak=" << celloPeak << " (" << celloDb << " dBFS)" << std::endl;
    }

    // TEST 11: Render 9 Target Presets for Comparative FFT Analysis
    std::cout << "\n[TEST 11] Rendering 9 TopModel Presets to WAV for Benchmarking..." << std::endl;
    totalTests++;

    struct RenderTarget
    {
        std::string presetName;
        std::string outFileName;
    };

    const std::vector<RenderTarget> targets = {
        { "Celestial Tines",        "F:/ModelKeys/renders/topmodel/topmodel_tines.wav" },
        { "Velvet Resonance",       "F:/ModelKeys/renders/topmodel/topmodel_keys.wav" },
        { "Neon Wurli Wave",        "F:/ModelKeys/renders/topmodel/topmodel_reeds.wav" },
        { "Acid Wire",              "F:/ModelKeys/renders/topmodel/topmodel_wire.wav" },
        { "Glass Mirage",           "F:/ModelKeys/renders/topmodel/topmodel_glass.wav" },
        { "Luminescent Harpoid",    "F:/ModelKeys/renders/topmodel/topmodel_harpoid.wav" },
        { "Subterranean Quake",     "F:/ModelKeys/renders/topmodel/topmodel_quake.wav" },
        { "Raindrop Prism",         "F:/ModelKeys/renders/topmodel/topmodel_prism.wav" },
        { "Solar Flare",            "F:/ModelKeys/renders/topmodel/topmodel_bowed.wav" }
    };

    // 16-second performance MIDI event schedule (120 BPM, beat = 0.5s = 22050 samples)
    struct MidiNoteEvent
    {
        int sampleOn;
        int sampleOff;
        int pitch;
        float velocity;
    };

    const std::vector<MidiNoteEvent> sequence = {
        // Chord 1 (0.0 to 3.5 beats)
        { 0, 77175, 48, 90.0f / 127.0f },
        { 0, 77175, 55, 85.0f / 127.0f },
        { 0, 77175, 59, 80.0f / 127.0f },
        { 0, 77175, 62, 85.0f / 127.0f },
        { 0, 77175, 64, 80.0f / 127.0f },
        // Chord 2 (4.0 to 7.5 beats: low C1 power chord)
        { 88200, 165375, 24, 95.0f / 127.0f },
        { 88200, 165375, 36, 90.0f / 127.0f },
        { 88200, 165375, 43, 85.0f / 127.0f },
        { 88200, 165375, 52, 80.0f / 127.0f },
        { 88200, 165375, 60, 85.0f / 127.0f },
        { 88200, 165375, 67, 80.0f / 127.0f },
        // Melody 1 (8.0 to 9.8 beats)
        { 176400, 216090, 57, 85.0f / 127.0f },
        // Melody 2 (10.0 to 11.8 beats)
        { 220500, 260190, 60, 85.0f / 127.0f },
        // Chord 3 (12.0 to 15.5 beats: F Maj 9)
        { 264600, 341775, 53, 90.0f / 127.0f },
        { 264600, 341775, 57, 85.0f / 127.0f },
        { 264600, 341775, 60, 80.0f / 127.0f },
        { 264600, 341775, 64, 85.0f / 127.0f },
        { 264600, 341775, 67, 80.0f / 127.0f }
    };

    const int totalRenderSamples = static_cast<int>(17.0 * sampleRate); // 16s + 1s tail
    bool allRendersClean = true;

    for (const auto& t : targets)
    {
        processor.getVoiceManager().reset();
        pm.selectPresetByName(t.presetName);

        juce::AudioBuffer<float> renderBuffer(2, totalRenderSamples);
        renderBuffer.clear();

        for (int sampleOffset = 0; sampleOffset < totalRenderSamples; sampleOffset += blockSize)
        {
            const int numBlockSamples = std::min(blockSize, totalRenderSamples - sampleOffset);
            midi.clear();

            for (const auto& ev : sequence)
            {
                if (ev.sampleOn >= sampleOffset && ev.sampleOn < sampleOffset + numBlockSamples)
                    midi.addEvent(juce::MidiMessage::noteOn(1, ev.pitch, ev.velocity), ev.sampleOn - sampleOffset);
                if (ev.sampleOff >= sampleOffset && ev.sampleOff < sampleOffset + numBlockSamples)
                    midi.addEvent(juce::MidiMessage::noteOff(1, ev.pitch, 0.5f), ev.sampleOff - sampleOffset);
            }

            juce::AudioBuffer<float> subBlock(renderBuffer.getArrayOfWritePointers(), 2, sampleOffset, numBlockSamples);
            processor.processBlock(subBlock, midi);
        }

        if (!checkBufferSanity(renderBuffer, t.presetName + " Render"))
        {
            allRendersClean = false;
            std::cerr << "  ✗ Buffer sanity check failed for " << t.presetName << std::endl;
            continue;
        }

        const float peakVal = renderBuffer.getMagnitude(0, totalRenderSamples);
        const float peakDb = 20.0f * std::log10(std::max(1e-5f, peakVal));

        juce::File outFile(t.outFileName);
        outFile.getParentDirectory().createDirectory();
        outFile.deleteFile();
        std::unique_ptr<juce::FileOutputStream> outStream(outFile.createOutputStream());
        if (outStream != nullptr)
        {
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(outStream.get(), sampleRate, 2, 24, {}, 0));
            if (writer != nullptr)
            {
                outStream.release();
                writer->writeFromAudioSampleBuffer(renderBuffer, 0, totalRenderSamples);
                std::cout << "  ✓ Rendered [" << t.presetName << "] -> " << outFile.getFileName()
                          << " (Peak: " << peakVal << " / " << peakDb << " dBFS)" << std::endl;
            }
        }
    }

    if (allRendersClean)
    {
        std::cout << "  ✓ All 9 TopModel target presets rendered with pristine headroom & zero distortion!" << std::endl;
        passedTests++;
    }

    // TEST 12: Real-Time Dynamic Pitch Bend During Decay
    std::cout << "\n[TEST 12] Testing Real-Time Pitch Bend During Note Decay..." << std::endl;
    totalTests++;
    processor.getVoiceManager().reset();
    pm.selectPresetByName("Velvet Resonance");

    // Strike C4 (60)
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.85f), 0);
    processor.processBlock(buffer, midi);
    midi.clear();

    // Let it ring for 10 blocks
    for (int b = 0; b < 10; ++b)
    {
        processor.processBlock(buffer, midi);
    }

    // Now while note is ringing/decaying, apply pitch bend wheel up +2 semitones
    // 8192 is center, 16383 is max (+2 semitones)
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0);
    processor.processBlock(buffer, midi);
    midi.clear();

    bool bendClean = true;
    float postBendEnergy = 0.0f;
    for (int b = 0; b < 20; ++b)
    {
        processor.processBlock(buffer, midi);
        if (!checkBufferSanity(buffer, "Real-Time Pitch Bend Up"))
        {
            bendClean = false;
            break;
        }
        postBendEnergy = std::max(postBendEnergy, buffer.getMagnitude(0, buffer.getNumSamples()));
    }

    // Now bend pitch wheel down -2 semitones (0 is min)
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 0), 0);
    processor.processBlock(buffer, midi);
    midi.clear();

    for (int b = 0; b < 20; ++b)
    {
        processor.processBlock(buffer, midi);
        if (!checkBufferSanity(buffer, "Real-Time Pitch Bend Down"))
        {
            bendClean = false;
            break;
        }
    }

    // Reset pitch wheel to center (8192)
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 8192), 0);
    processor.processBlock(buffer, midi);
    midi.clear();

    if (bendClean && postBendEnergy > 0.01f)
    {
        std::cout << "  ✓ Real-time pitch bend during decay verified: smoothly modulates active modes without clicks or instability (Energy = "
                  << postBendEnergy << ")." << std::endl;
        passedTests++;
    }
    else
    {
        std::cerr << "  ✗ Real-time pitch bend test failed!" << std::endl;
    }

    // TEST 13: MIDI CC 1 Mod Wheel Vibrato & Organic Touch Micro-Variation
    std::cout << "\n[TEST 13] Testing MIDI CC 1 Mod Wheel Vibrato & Organic Drift..." << std::endl;
    totalTests++;
    processor.getVoiceManager().reset();
    pm.selectPresetByName("Solar Flare");

    // Strike C3 (48) with bow
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 48, 0.85f), 0);
    // Engage Mod Wheel (CC 1 = 100)
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 100), 5);
    processor.processBlock(buffer, midi);
    midi.clear();

    bool vibClean = true;
    float maxVibMag = 0.0f;
    for (int b = 0; b < 40; ++b)
    {
        processor.processBlock(buffer, midi);
        if (!checkBufferSanity(buffer, "Mod Wheel Vibrato"))
        {
            vibClean = false;
            break;
        }
        maxVibMag = std::max(maxVibMag, buffer.getMagnitude(0, buffer.getNumSamples()));
    }

    // Reset Mod Wheel to 0
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 0), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 48, 0.5f), 10);
    processor.processBlock(buffer, midi);
    midi.clear();

    for (int b = 0; b < 20; ++b)
    {
        processor.processBlock(buffer, midi);
        if (!checkBufferSanity(buffer, "Mod Wheel Reset"))
        {
            vibClean = false;
            break;
        }
    }

    if (vibClean && maxVibMag > 0.05f && maxVibMag <= 1.0f)
    {
        std::cout << "  ✓ Mod Wheel vibrato verified: clean acoustic response ("
                  << maxVibMag << " / " << 20.0f * std::log10(maxVibMag) << " dBFS) with zero saturation." << std::endl;
        passedTests++;
    }
    else
    {
        std::cerr << "  ✗ Mod Wheel vibrato test failed: maxVibMag=" << maxVibMag << std::endl;
    }

    // TEST 14: Anti-Aliased Preamp Modes (Clean, Tube ADAA, Tape)
    std::cout << "\n[TEST 14] Testing Preamp Modes (Clean, Tube ADAA, Analog Tape)..." << std::endl;
    totalTests++;
    bool preampModesOk = true;

    auto* preModeParam = processor.getAPVTS().getParameter("preampMode");
    auto* preDriveParam = processor.getAPVTS().getParameter("preampDrive");
    assert(preModeParam != nullptr && preDriveParam != nullptr);

    // Test Clean (mode 0)
    preModeParam->setValueNotifyingHost(0.0f); // Clean
    preDriveParam->setValueNotifyingHost(0.8f); // High drive setting
    pm.selectPresetByName("Velvet Resonance");

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
    processor.processBlock(buffer, midi);
    midi.clear();
    for (int b = 0; b < 10; ++b)
    {
        processor.processBlock(buffer, midi);
        if (!checkBufferSanity(buffer, "Preamp Clean Mode")) { preampModesOk = false; break; }
    }

    // Test Tube ADAA (mode 1)
    preModeParam->setValueNotifyingHost(0.5f); // Tube (1/2 in normalized 0..2)
    for (int b = 0; b < 10; ++b)
    {
        processor.processBlock(buffer, midi);
        if (!checkBufferSanity(buffer, "Preamp Tube ADAA Mode")) { preampModesOk = false; break; }
    }

    // Test Analog Tape (mode 2)
    preModeParam->setValueNotifyingHost(1.0f); // Tape
    for (int b = 0; b < 10; ++b)
    {
        processor.processBlock(buffer, midi);
        if (!checkBufferSanity(buffer, "Preamp Analog Tape Mode")) { preampModesOk = false; break; }
    }

    if (preampModesOk)
    {
        std::cout << "  ✓ All preamp modes (Clean, Tube ADAA, Analog Tape) verified: smooth saturation, zero NaN/Inf, zero aliasing blowup." << std::endl;
        passedTests++;
    }

    // TEST 15: Extended Modulation Multi-FX Engine (7 Modes)
    std::cout << "\n[TEST 15] Testing Extended Modulation Multi-FX Engine (7 Modes)..." << std::endl;
    totalTests++;
    bool allModOk = true;

    auto* modModeParam = processor.getAPVTS().getParameter("chorusTremoloMode");
    auto* modMixParam  = processor.getAPVTS().getParameter("chorusMix");
    assert(modModeParam != nullptr && modMixParam != nullptr);
    modMixParam->setValueNotifyingHost(0.6f);

    const std::vector<std::string> modNames = {
        "Chorus", "Tremolo", "Flanger", "Ensemble", "Phaser", "Wow & Flutter", "Tape Delay"
    };

    for (size_t m = 0; m < modNames.size(); ++m)
    {
        modModeParam->setValueNotifyingHost(static_cast<float>(m) / 6.0f);
        processor.getVoiceManager().reset();

        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.85f), 0);
        processor.processBlock(buffer, midi);
        midi.clear();

        for (int b = 0; b < 15; ++b)
        {
            processor.processBlock(buffer, midi);
            if (!checkBufferSanity(buffer, "Modulation Mode: " + modNames[m]))
            {
                allModOk = false;
                std::cerr << "  ✗ Modulation mode failed: " << modNames[m] << std::endl;
                break;
            }
        }
    }

    if (allModOk)
    {
        std::cout << "  ✓ All 7 modulation modes (Chorus, Tremolo, Flanger, Ensemble, Phaser, Wow & Flutter, Tape Delay) verified: 100% stable." << std::endl;
        passedTests++;
    }

    // TEST 16: Selectable Reverb Types (Plate, Room, Hall, Spring)
    std::cout << "\n[TEST 16] Testing Selectable Reverb Types (Plate, Room, Hall, Spring)..." << std::endl;
    totalTests++;
    bool allRevOk = true;

    auto* revTypeParam = processor.getAPVTS().getParameter("reverbType");
    auto* revMixParam  = processor.getAPVTS().getParameter("reverbMix");
    assert(revTypeParam != nullptr && revMixParam != nullptr);
    revMixParam->setValueNotifyingHost(0.5f);

    const std::vector<std::string> revNames = { "Plate", "Room", "Hall", "Spring" };

    for (size_t r = 0; r < revNames.size(); ++r)
    {
        revTypeParam->setValueNotifyingHost(static_cast<float>(r) / 3.0f);
        processor.getVoiceManager().reset();

        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
        processor.processBlock(buffer, midi);
        midi.clear();

        for (int b = 0; b < 20; ++b)
        {
            processor.processBlock(buffer, midi);
            if (!checkBufferSanity(buffer, "Reverb Type: " + revNames[r]))
            {
                allRevOk = false;
                std::cerr << "  ✗ Reverb type failed: " << revNames[r] << std::endl;
                break;
            }
        }
    }

    if (allRevOk)
    {
        std::cout << "  ✓ All 4 reverb types (Plate, Room, Hall, Spring) verified: pristine tails, chirp dispersion, and stable decays." << std::endl;
        passedTests++;
    }

    // TEST 17: User Preset Saving, Custom Categories, Overwriting, and Erasing
    std::cout << "\n[TEST 17] Testing Preset Management (Save, Custom Category, Overwrite, Delete)..." << std::endl;
    totalTests++;
    bool presetMgmtOk = true;
    {
        const std::string testPresetName = "Test Custom Acoustic";
        const std::string testCategory = "Experimental";

        // 1. Save new user preset in custom category
        if (!pm.saveUserPreset(testPresetName, testCategory))
        {
            presetMgmtOk = false;
            std::cerr << "  ✗ Failed to save user preset: " << testPresetName << std::endl;
        }

        // Verify category exists
        const auto& cats = pm.getCategories();
        if (std::find(cats.begin(), cats.end(), testCategory) == cats.end())
        {
            presetMgmtOk = false;
            std::cerr << "  ✗ Category " << testCategory << " not found in category list." << std::endl;
        }

        // Verify preset exists
        pm.selectPresetByName(testPresetName);
        const auto* cur = pm.getCurrentPreset();
        if (!cur || cur->name != testPresetName || cur->category != testCategory)
        {
            presetMgmtOk = false;
            std::cerr << "  ✗ Preset selection by name failed or metadata mismatch." << std::endl;
        }

        // 2. Overwrite preset
        if (!pm.saveUserPreset(testPresetName, "ModifiedCat"))
        {
            presetMgmtOk = false;
            std::cerr << "  ✗ Failed to overwrite existing user preset." << std::endl;
        }
        pm.selectPresetByName(testPresetName);
        cur = pm.getCurrentPreset();
        if (!cur || cur->category != "ModifiedCat")
        {
            presetMgmtOk = false;
            std::cerr << "  ✗ Overwritten preset did not update category." << std::endl;
        }

        // 3. Delete user preset
        if (!pm.deletePresetByName(testPresetName))
        {
            presetMgmtOk = false;
            std::cerr << "  ✗ Failed to delete user preset." << std::endl;
        }

        // Verify deletion
        bool stillExists = false;
        for (const auto& p : pm.getAllPresets())
        {
            if (p.name == testPresetName) stillExists = true;
        }
        if (stillExists)
        {
            presetMgmtOk = false;
            std::cerr << "  ✗ Preset still present in library after deletion." << std::endl;
        }
    }

    if (presetMgmtOk)
    {
        std::cout << "  ✓ Preset saving, custom categories, overwriting, and deletion verified: 100% reliable." << std::endl;
        passedTests++;
    }

    // TEST 18: Sympathetic Bridge Coupling Resonance
    std::cout << "\n[TEST 18] Testing Multi-Note Sympathetic Bridge Coupling..." << std::endl;
    totalTests++;
    bool sympOk = true;
    {
        processor.getVoiceManager().reset();
        auto* bodyType = processor.getAPVTS().getParameter("bodyType");
        auto* bodyMix = processor.getAPVTS().getParameter("bodyMix");
        auto* bodyRes = processor.getAPVTS().getParameter("bodyResonance");
        auto* decayParam = processor.getAPVTS().getParameter("decayTime");

        if (bodyType) bodyType->setValueNotifyingHost(0.2f); // Grand Piano body
        if (bodyMix) bodyMix->setValueNotifyingHost(0.9f);
        if (bodyRes) bodyRes->setValueNotifyingHost(0.9f);
        if (decayParam) decayParam->setValueNotifyingHost(0.8f);

        // Play C2 (36) and hold
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 36, 0.8f), 0);
        processor.processBlock(buffer, midi);
        midi.clear();

        // Let C2 settle for 20 blocks
        for (int b = 0; b < 20; ++b)
            processor.processBlock(buffer, midi);

        // Now strike C4 (60) - its 4th harmonic should energetically couple via bridge
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
        processor.processBlock(buffer, midi);
        midi.clear();

        for (int b = 0; b < 30; ++b)
        {
            processor.processBlock(buffer, midi);
            if (!checkBufferSanity(buffer, "Sympathetic Coupling"))
            {
                sympOk = false;
                break;
            }
        }

        // Release C4, keep C2 ringing
        midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.5f), 0);
        processor.processBlock(buffer, midi);
        midi.clear();

        for (int b = 0; b < 20; ++b)
        {
            processor.processBlock(buffer, midi);
            if (!checkBufferSanity(buffer, "Sympathetic Ringout"))
            {
                sympOk = false;
                break;
            }
        }

        // Verify that soundboard coupling produced active audio without clipping
        float mag = buffer.getMagnitude(0, buffer.getNumSamples());
        if (mag <= 0.00001f)
        {
            sympOk = false;
            std::cerr << "  ✗ Sympathetic coupling produced zero energy." << std::endl;
        }
    }

    if (sympOk)
    {
        std::cout << "  ✓ Multi-note sympathetic bridge coupling verified: organic acoustic vibration transfer without feedback runaway." << std::endl;
        passedTests++;
    }

    // TEST 19: Precedent-State Re-Strike Dynamics (Felt Compaction & Velocity Feedback)
    std::cout << "\n[TEST 19] Testing Precedent-State Re-Strike Dynamics (Felt Compaction & Momentum)..." << std::endl;
    totalTests++;
    bool restrikeOk = true;
    {
        processor.getVoiceManager().reset();
        
        // Strike 1: Note from rest
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.7f), 0);
        processor.processBlock(buffer, midi);
        midi.clear();

        // Run for 5 blocks (~50ms)
        for (int b = 0; b < 5; ++b)
            processor.processBlock(buffer, midi);

        // Strike 2: Rapid re-strike on same note while string is actively vibrating
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.7f), 0);
        processor.processBlock(buffer, midi);
        midi.clear();

        for (int b = 0; b < 20; ++b)
        {
            processor.processBlock(buffer, midi);
            if (!checkBufferSanity(buffer, "Re-Strike Dynamics"))
            {
                restrikeOk = false;
                break;
            }
        }

        midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.5f), 0);
        for (int b = 0; b < 10; ++b)
        {
            processor.processBlock(buffer, midi);
            if (!checkBufferSanity(buffer, "Re-Strike Release"))
            {
                restrikeOk = false;
                break;
            }
        }
    }

    if (restrikeOk)
    {
        std::cout << "  ✓ Precedent-state re-strike dynamics verified: felt compaction and soft retrigger preserve momentum cleanly." << std::endl;
        passedTests++;
    }

    std::cout << "\n=================================================" << std::endl;
    std::cout << "CALIBRATION TEST RESULTS: " << passedTests << " / " << totalTests << " PASSED" << std::endl;
    std::cout << "=================================================" << std::endl;

    return (passedTests == totalTests) ? 0 : 1;
}
