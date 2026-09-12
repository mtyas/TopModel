#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "CustomLookAndFeel.h"
#include "../PluginProcessor.h"
#include <array>

namespace ModelKeys
{
    class VisualizerComponent : public juce::Component, private juce::Timer
    {
    public:
        VisualizerComponent(PluginProcessor& p);
        ~VisualizerComponent() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;

    private:
        void timerCallback() override;

        PluginProcessor& processor;
        float animPhase = 0.0f;
        float bowDrawPhase = 0.0f;
        float impactAnim = 0.0f;
        bool lastVoiceActive = false;
        std::array<float, 8> smoothedAmps { 0.0f };
        float smoothedEnergy = 0.0f;

        bool draggingContact = false;
        bool draggingPickup = false;
    };
}
