#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>
#include "FactoryPresets.h"

namespace ModelKeys
{
    class PresetManager
    {
    public:
        class Listener
        {
        public:
            virtual ~Listener() = default;
            virtual void presetChanged(const Preset& newPreset, int presetIndex) = 0;
            virtual void presetListUpdated() = 0;
        };

        PresetManager(juce::AudioProcessorValueTreeState& apvts, juce::UndoManager& undoManager);
        ~PresetManager() = default;

        void addListener(Listener* listener);
        void removeListener(Listener* listener);

        void initialize();

        const std::vector<Preset>& getAllPresets() const noexcept { return presets; }
        const std::vector<std::string>& getCategories() const noexcept { return categories; }

        int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }
        const Preset* getCurrentPreset() const;

        void selectPreset(int index);
        void selectPresetByName(const std::string& name);
        void nextPreset(const std::string& categoryFilter = "All");
        void prevPreset(const std::string& categoryFilter = "All");

        bool saveUserPreset(const std::string& name, const std::string& category = "User");
        bool deletePreset(int index);
        bool deletePresetByName(const std::string& name);
        bool deleteUserPreset(const std::string& name) { return deletePresetByName(name); }
        void restoreFactoryPresets();

        void applyPresetToAPVTS(const Preset& preset);

    private:
        void refreshPresetList();
        juce::File getUserPresetDirectory();
        juce::File getDeletedPresetsFile();

        juce::AudioProcessorValueTreeState& state;
        juce::UndoManager& undo;

        std::vector<Preset> presets;
        std::vector<std::string> categories;
        int currentPresetIndex = 0;

        juce::ListenerList<Listener> listeners;
    };
}
