#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/VoiceManager.h"
#include "Presets/PresetManager.h"
#include "Midi/MidiLearnManager.h"

#if __has_include(<clap-juce-extensions/clap-juce-extensions.h>)
#include <clap-juce-extensions/clap-juce-extensions.h>
#define HAS_CLAP_EXTENSIONS 1
#else
#define HAS_CLAP_EXTENSIONS 0
#endif

namespace ModelKeys
{
    class PluginProcessor : public juce::AudioProcessor
#if HAS_CLAP_EXTENSIONS
                           , public clap_juce_extensions::clap_properties
#endif
    {
    public:
        PluginProcessor();
        ~PluginProcessor() override;

        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;

        bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override;

        const juce::String getName() const override;

        bool acceptsMidi() const override;
        bool producesMidi() const override;
        bool isMidiEffect() const override;
        double getTailLengthSeconds() const override;

        int getNumPrograms() override;
        int getCurrentProgram() override;
        void setCurrentProgram(int index) override;
        const juce::String getProgramName(int index) override;
        void changeProgramName(int index, const juce::String& newName) override;

        void getStateInformation(juce::MemoryBlock& destData) override;
        void setStateInformation(const void* data, int sizeInBytes) override;

        juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
        juce::UndoManager& getUndoManager() noexcept { return undoManager; }
        PresetManager& getPresetManager() noexcept { return presetManager; }
        MidiLearnManager& getMidiLearnManager() noexcept { return midiLearnManager; }
        VoiceManager& getVoiceManager() noexcept { return voiceManager; }

        float getMeterLevelL() const noexcept { return meterL.load(); }
        float getMeterLevelR() const noexcept { return meterR.load(); }

        // Keyboard note tracking for GUI display
        juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

        juce::UndoManager undoManager;
        juce::AudioProcessorValueTreeState apvts;
        VoiceManager voiceManager;
        PresetManager presetManager;
        MidiLearnManager midiLearnManager;
        juce::MidiKeyboardState keyboardState;

        std::atomic<float> meterL { 0.0f };
        std::atomic<float> meterR { 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
    };
}
