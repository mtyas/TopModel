#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ModelKeys
{
    juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        // Exciter Parameters
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "exciterType", 1 }, "Exciter Type",
            juce::StringArray { "Strike", "Pluck", "Bow", "Noise" }, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "hardness", 1 }, "Exciter Hardness",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "contactPos", 1 }, "Contact Position",
            juce::NormalisableRange<float>(0.02f, 0.98f, 0.01f), 0.2f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "stiffness", 1 }, "Exciter Stiffness",
            juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.5f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "bowSpeed", 1 }, "Bow Speed",
            juce::NormalisableRange<float>(0.05f, 2.0f, 0.01f), 0.5f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "bowForce", 1 }, "Bow Force",
            juce::NormalisableRange<float>(0.05f, 2.0f, 0.01f), 0.5f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "strikeClick", 1 }, "Mallet / Contact Click",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "rosinGrit", 1 }, "Rosin Bite / Grit",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "particleDensity", 1 }, "Particle Density",
            juce::NormalisableRange<float>(0.02f, 1.0f, 0.01f), 0.6f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "particleScatter", 1 }, "Particle Scatter",
            juce::NormalisableRange<float>(1.0f, 150.0f, 0.5f, 0.5f), 25.0f));

        // Resonator Parameters
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "resonatorType", 1 }, "Resonator Type",
            juce::StringArray { "String", "Clamped Tine", "Free Bar", "Membrane", "Plate", "Clamped Reed", "Steel Pan" }, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "decayTime", 1 }, "Decay Time",
            juce::NormalisableRange<float>(0.05f, 12.0f, 0.05f, 0.4f), 3.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "brightness", 1 }, "Brightness",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "inharmonicity", 1 }, "Inharmonicity",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.1f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "damperRelease", 1 }, "Damper / Release",
            juce::NormalisableRange<float>(0.05f, 1.0f, 0.01f), 0.35f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "pitchDropAmount", 1 }, "Pitch Drop Depth",
            juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 7.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "pitchDropDecay", 1 }, "Pitch Drop Decay",
            juce::NormalisableRange<float>(5.0f, 500.0f, 1.0f, 0.4f), 60.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "beatingBloom", 1 }, "Beating / Bloom",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "materialBalance", 1 }, "Material Balance / Damping",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

        // Body Parameters
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "bodyType", 1 }, "Body Type",
            juce::StringArray { "Off", "Acoustic Guitar", "Piano Soundboard", "Violin Body", "Rhodes Tonebar", "Drum Shell", "Wurli Reed Bar", "Harp Soundbox", "Marimba Resonator", "Steel Drum Barrel" }, 1));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "bodySize", 1 }, "Body Size",
            juce::NormalisableRange<float>(0.5f, 2.0f, 0.01f), 1.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "bodyResonance", 1 }, "Body Resonance",
            juce::NormalisableRange<float>(0.1f, 1.0f, 0.01f), 0.6f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "bodyMix", 1 }, "Body Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));

        // Pickup Parameters
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "pickupType", 1 }, "Pickup Type",
            juce::StringArray { "Electromagnetic", "Piezo", "Microphone", "Electrostatic", "Clavinet Dual-Coil" }, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "pickupPos", 1 }, "Pickup Position",
            juce::NormalisableRange<float>(0.02f, 0.98f, 0.01f), 0.85f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "pickupDrive", 1 }, "Pickup Drive / Bark",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "pickupTone", 1 }, "Pickup Tone",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.6f));

        // Preamp FX
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "preampMode", 1 }, "Preamp Mode",
            juce::StringArray { "Clean", "Warm Tube", "Analog Tape" }, 1));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "preampDrive", 1 }, "Preamp Drive",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "preampTone", 1 }, "Preamp Tone",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.6f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "preampLevel", 1 }, "Preamp Level",
            juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));

        // Modulation Multi-FX
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "chorusTremoloMode", 1 }, "Modulation Mode",
            juce::StringArray { "Chorus", "Tremolo", "Flanger", "Ensemble", "Phaser", "Wow & Flutter", "Tape Delay" }, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "chorusRate", 1 }, "Mod Rate / Time",
            juce::NormalisableRange<float>(0.05f, 12.0f, 0.05f), 1.2f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "chorusDepth", 1 }, "Mod Depth / Feedback",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "chorusMix", 1 }, "Mod Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f));

        // Reverb FX
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "reverbType", 1 }, "Reverb Type",
            juce::StringArray { "Plate", "Room", "Hall", "Spring" }, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "reverbSize", 1 }, "Reverb Size",
            juce::NormalisableRange<float>(0.05f, 0.98f, 0.01f), 0.65f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "reverbDamp", 1 }, "Reverb Damping",
            juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.35f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "reverbMix", 1 }, "Reverb Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f));

        // Global & Performance
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { "polyphony", 1 }, "Polyphony",
            1, 16, 16));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "masterGain", 1 }, "Master Volume",
            juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.85f));

        return { params.begin(), params.end() };
    }

    PluginProcessor::PluginProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, &undoManager, "Parameters", createParameterLayout()),
          presetManager(apvts, undoManager),
          midiLearnManager(apvts)
    {
        presetManager.initialize();
    }

    PluginProcessor::~PluginProcessor()
    {
    }

    const juce::String PluginProcessor::getName() const
    {
        return "TopModel";
    }

    bool PluginProcessor::acceptsMidi() const { return true; }
    bool PluginProcessor::producesMidi() const { return false; }
    bool PluginProcessor::isMidiEffect() const { return false; }
    double PluginProcessor::getTailLengthSeconds() const { return 3.0; }

    int PluginProcessor::getNumPrograms()
    {
        return static_cast<int>(presetManager.getAllPresets().size());
    }

    int PluginProcessor::getCurrentProgram()
    {
        return presetManager.getCurrentPresetIndex();
    }

    void PluginProcessor::setCurrentProgram(int index)
    {
        presetManager.selectPreset(index);
    }

    const juce::String PluginProcessor::getProgramName(int index)
    {
        const auto& list = presetManager.getAllPresets();
        if (index >= 0 && index < static_cast<int>(list.size()))
            return list[index].name;
        return {};
    }

    void PluginProcessor::changeProgramName(int index, const juce::String& newName)
    {
        juce::ignoreUnused(index, newName);
    }

    void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        juce::FloatVectorOperations::disableDenormalisedNumberSupport();
        voiceManager.prepare(sampleRate, samplesPerBlock);
    }

    void PluginProcessor::releaseResources()
    {
        voiceManager.reset();
    }

    bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
    {
        // Stereo output only
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
            return false;
        return true;
    }

    void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
    {
        juce::ScopedNoDenormals noDenormals;

        const int numSamples = buffer.getNumSamples();
        if (numSamples == 0) return;

        // Clear output buffer
        buffer.clear();

        // 1. Process keyboard state for GUI on-screen keyboard
        keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

        // 2. Read parameters from APVTS
        const auto excType = static_cast<ExciterType>(static_cast<int>(*apvts.getRawParameterValue("exciterType")));
        const float hardness = *apvts.getRawParameterValue("hardness");
        const float contactPos = *apvts.getRawParameterValue("contactPos");
        const float stiffness = *apvts.getRawParameterValue("stiffness");
        const float bowSpeed = *apvts.getRawParameterValue("bowSpeed");
        const float bowForce = *apvts.getRawParameterValue("bowForce");
        const float strikeClick = *apvts.getRawParameterValue("strikeClick");
        const float rosinGrit = *apvts.getRawParameterValue("rosinGrit");
        const float particleDensity = *apvts.getRawParameterValue("particleDensity");
        const float particleScatter = *apvts.getRawParameterValue("particleScatter");

        const auto resType = static_cast<ResonatorType>(static_cast<int>(*apvts.getRawParameterValue("resonatorType")));
        const float decayTime = *apvts.getRawParameterValue("decayTime");
        const float brightness = *apvts.getRawParameterValue("brightness");
        const float inharmonicity = *apvts.getRawParameterValue("inharmonicity");
        const float damperRelease = *apvts.getRawParameterValue("damperRelease");
        const float pitchDropAmount = *apvts.getRawParameterValue("pitchDropAmount");
        const float pitchDropDecay = *apvts.getRawParameterValue("pitchDropDecay");
        const float beatingBloom = *apvts.getRawParameterValue("beatingBloom");
        const float materialBalance = *apvts.getRawParameterValue("materialBalance");

        const auto bodyType = static_cast<BodyType>(static_cast<int>(*apvts.getRawParameterValue("bodyType")));
        const float bodySize = *apvts.getRawParameterValue("bodySize");
        const float bodyResonance = *apvts.getRawParameterValue("bodyResonance");
        const float bodyMix = *apvts.getRawParameterValue("bodyMix");

        const auto pkType = static_cast<PickupType>(static_cast<int>(*apvts.getRawParameterValue("pickupType")));
        const float pickupPos = *apvts.getRawParameterValue("pickupPos");
        const float pickupDrive = *apvts.getRawParameterValue("pickupDrive");
        const float pickupTone = *apvts.getRawParameterValue("pickupTone");

        const auto preMode = static_cast<PreampType>(static_cast<int>(*apvts.getRawParameterValue("preampMode")));
        const float preampDrive = *apvts.getRawParameterValue("preampDrive");
        const float preampTone = *apvts.getRawParameterValue("preampTone");
        const float preampLevel = *apvts.getRawParameterValue("preampLevel");

        const auto modMode = static_cast<ChorusTremolo::Mode>(static_cast<int>(*apvts.getRawParameterValue("chorusTremoloMode")));
        const float modRate = *apvts.getRawParameterValue("chorusRate");
        const float modDepth = *apvts.getRawParameterValue("chorusDepth");
        const float modMix = *apvts.getRawParameterValue("chorusMix");

        const auto revType = static_cast<PlateReverb::Type>(static_cast<int>(*apvts.getRawParameterValue("reverbType")));
        const float revSize = *apvts.getRawParameterValue("reverbSize");
        const float revDamp = *apvts.getRawParameterValue("reverbDamp");
        const float revMix = *apvts.getRawParameterValue("reverbMix");

        const int polyphony = static_cast<int>(*apvts.getRawParameterValue("polyphony"));
        const float masterGain = *apvts.getRawParameterValue("masterGain");

        // 3. Configure voice manager subsystems
        voiceManager.setPolyphony(polyphony);
        voiceManager.configureBody(bodyType, bodySize, bodyResonance, bodyMix);
        voiceManager.configurePreamp(preMode, preampDrive, preampTone, preampLevel);
        voiceManager.configureChorusTremolo(modMode, modRate, modDepth, modMix);
        voiceManager.configureReverb(revType, revSize, revDamp, revMix);

        // 4. Handle incoming MIDI events
        for (const auto metadata : midiMessages)
        {
            const auto msg = metadata.getMessage();

            // Check MIDI learn dispatcher first
            if (midiLearnManager.handleMidiMessage(msg))
            {
                continue;
            }

            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                voiceManager.noteOn(msg.getNoteNumber(), msg.getFloatVelocity(),
                                    excType, hardness, contactPos, stiffness,
                                    bowSpeed, bowForce,
                                    strikeClick, rosinGrit,
                                    particleDensity, particleScatter,
                                    resType, decayTime, brightness, inharmonicity,
                                    pickupPos,
                                    pitchDropAmount, pitchDropDecay,
                                    beatingBloom, materialBalance,
                                    pkType, pickupDrive, pickupTone);
            }
            else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            {
                voiceManager.noteOff(msg.getNoteNumber(), msg.getFloatVelocity(), damperRelease);
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                voiceManager.allNotesOff(damperRelease);
            }
            else if (msg.isController())
            {
                if (msg.getControllerNumber() == 64)
                {
                    // MIDI CC 64: Damper / Sustain Pedal
                    voiceManager.setSustainPedal(msg.getControllerValue() >= 64, damperRelease);
                }
                else if (msg.getControllerNumber() == 1)
                {
                    // MIDI CC 1: Mod Wheel (Expressive singing vibrato)
                    const float modWheel = static_cast<float>(msg.getControllerValue()) / 127.0f;
                    voiceManager.setModWheel(modWheel);
                }
            }
            else if (msg.isPitchWheel())
            {
                // Pitch bend range +/- 2 semitones
                const float bendNorm = static_cast<float>(msg.getPitchWheelValue() - 8192) / 8192.0f;
                voiceManager.setPitchBend(bendNorm * 2.0f);
            }
        }

        // 5. Render audio
        float* leftChannel = buffer.getWritePointer(0);
        float* rightChannel = buffer.getWritePointer(1);

        voiceManager.processBlock(leftChannel, rightChannel, numSamples, masterGain);

        // 6. Update peak meters for GUI
        float peakL = buffer.getMagnitude(0, 0, numSamples);
        float peakR = buffer.getMagnitude(1, 0, numSamples);
        meterL.store(peakL);
        meterR.store(peakR);
    }

    bool PluginProcessor::hasEditor() const { return true; }

    juce::AudioProcessorEditor* PluginProcessor::createEditor()
    {
        return new PluginEditor(*this);
    }

    void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
    {
        auto stateTree = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml(stateTree.createXml());
        copyXmlToBinary(*xml, destData);
    }

    void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
    {
        std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
        if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

// JUCE Plugin Entry Point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ModelKeys::PluginProcessor();
}
