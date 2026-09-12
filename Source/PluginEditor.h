#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/CustomLookAndFeel.h"
#include "UI/VisualizerComponent.h"
#include "UI/Panels/HeaderBar.h"
#include "UI/Panels/ExciterPanel.h"
#include "UI/Panels/ResonatorPanel.h"
#include "UI/Panels/BodyPickupPanel.h"
#include "UI/Panels/FXPanel.h"
#include "UI/KeyboardComponent.h"

namespace ModelKeys
{
    class PluginEditor : public juce::AudioProcessorEditor
    {
    public:
        explicit PluginEditor(PluginProcessor&);
        ~PluginEditor() override;

        void paint(juce::Graphics&) override;
        void resized() override;

        bool keyPressed(const juce::KeyPress& key) override;

    private:
        PluginProcessor& processor;
        CustomLookAndFeel lookAndFeel;

        HeaderBar headerBar;
        VisualizerComponent visualizer;
        ExciterPanel exciterPanel;
        ResonatorPanel resonatorPanel;
        BodyPickupPanel bodyPickupPanel;
        FXPanel fxPanel;
        KeyboardComponent keyboardComponent;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
    };
}
