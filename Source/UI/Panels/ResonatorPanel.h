#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../CustomLookAndFeel.h"
#include "../Controls/CustomRotarySlider.h"
#include "../../PluginProcessor.h"
#include <vector>

namespace ModelKeys
{
    class ResonatorPanel : public juce::Component, private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        ResonatorPanel(PluginProcessor& p)
            : processor(p),
              decaySlider("decayTime", "DECAY", p.getMidiLearnManager()),
              brightSlider("brightness", "BRIGHTNESS", p.getMidiLearnManager()),
              inharmSlider("inharmonicity", "INHARMONIC", p.getMidiLearnManager()),
              damperSlider("damperRelease", "DAMPER", p.getMidiLearnManager()),
              bloomSlider("beatingBloom", "BLOOM", p.getMidiLearnManager()),
              materialSlider("materialBalance", "MATERIAL", p.getMidiLearnManager()),
              pitchDropSlider("pitchDropAmount", "PITCH BEND", p.getMidiLearnManager()),
              pitchDecaySlider("pitchDropDecay", "BEND TIME", p.getMidiLearnManager())
        {
            addAndMakeVisible(typeBox);
            typeBox.addItem("String (Harmonic)", 1);
            typeBox.addItem("Clamped Tine (Rhodes)", 2);
            typeBox.addItem("Free Bar (Marimba)", 3);
            typeBox.addItem("Membrane (Drum/Tension)", 4);
            typeBox.addItem("Plate (Metallic)", 5);
            typeBox.addItem("Clamped Reed (Wurlitzer)", 6);
            typeBox.addItem("Steel Pan (Caribbean)", 7);

            typeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processor.getAPVTS(), "resonatorType", typeBox);

            auto setupSlider = [this](CustomRotarySlider& slider, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attach, const juce::String& paramId)
            {
                addChildComponent(slider);
                attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    processor.getAPVTS(), paramId, slider);
            };

            setupSlider(decaySlider, decayAttach, "decayTime");
            setupSlider(brightSlider, brightAttach, "brightness");
            setupSlider(inharmSlider, inharmAttach, "inharmonicity");
            setupSlider(damperSlider, damperAttach, "damperRelease");
            setupSlider(bloomSlider, bloomAttach, "beatingBloom");
            setupSlider(materialSlider, materialAttach, "materialBalance");
            setupSlider(pitchDropSlider, pitchDropAttach, "pitchDropAmount");
            setupSlider(pitchDecaySlider, pitchDecayAttach, "pitchDropDecay");

            processor.getAPVTS().addParameterListener("resonatorType", this);

            typeBox.onChange = [this]() { updateControls(); };
            updateControls();
        }

        ~ResonatorPanel() override
        {
            processor.getAPVTS().removeParameterListener("resonatorType", this);
        }

        void parameterChanged(const juce::String& parameterID, float newValue) override
        {
            juce::ignoreUnused(parameterID, newValue);
            juce::MessageManager::callAsync([this]() { updateControls(); });
        }

        void updateControls()
        {
            const auto resType = static_cast<ResonatorType>(static_cast<int>(*processor.getAPVTS().getRawParameterValue("resonatorType")));

            // Hide all sliders
            decaySlider.setVisible(false);
            brightSlider.setVisible(false);
            inharmSlider.setVisible(false);
            damperSlider.setVisible(false);
            bloomSlider.setVisible(false);
            materialSlider.setVisible(false);
            pitchDropSlider.setVisible(false);
            pitchDecaySlider.setVisible(false);

            activeSliders.clear();

            switch (resType)
            {
                case ResonatorType::String:
                    decaySlider.setLabelText("DECAY");
                    brightSlider.setLabelText("BRIGHTNESS");
                    inharmSlider.setLabelText("INHARMONIC");
                    bloomSlider.setLabelText("BEAT / BLOOM");
                    damperSlider.setLabelText("DAMPER");
                    activeSliders = { &decaySlider, &brightSlider, &inharmSlider, &bloomSlider, &damperSlider };
                    break;

                case ResonatorType::ClampedTine:
                    decaySlider.setLabelText("DECAY");
                    brightSlider.setLabelText("TINE BELL");
                    materialSlider.setLabelText("TONEBAR SUSTAIN");
                    bloomSlider.setLabelText("BLOOM");
                    damperSlider.setLabelText("DAMPER");
                    activeSliders = { &decaySlider, &brightSlider, &materialSlider, &bloomSlider, &damperSlider };
                    break;

                case ResonatorType::ClampedReed:
                    decaySlider.setLabelText("DECAY");
                    brightSlider.setLabelText("REED BITE");
                    materialSlider.setLabelText("REED DAMPING");
                    bloomSlider.setLabelText("BLOOM");
                    damperSlider.setLabelText("DAMPER");
                    activeSliders = { &decaySlider, &brightSlider, &materialSlider, &bloomSlider, &damperSlider };
                    break;

                case ResonatorType::FreeBar:
                    decaySlider.setLabelText("DECAY");
                    brightSlider.setLabelText("BRIGHTNESS");
                    materialSlider.setLabelText("WOOD DAMPING");
                    bloomSlider.setLabelText("RESONANCE");
                    damperSlider.setLabelText("MALLET DAMPER");
                    activeSliders = { &decaySlider, &brightSlider, &materialSlider, &bloomSlider, &damperSlider };
                    break;

                case ResonatorType::Membrane:
                    decaySlider.setLabelText("DECAY");
                    pitchDropSlider.setLabelText("PITCH BEND");
                    pitchDecaySlider.setLabelText("BEND TIME");
                    brightSlider.setLabelText("TENSION / TONE");
                    damperSlider.setLabelText("MUFFLE");
                    activeSliders = { &decaySlider, &pitchDropSlider, &pitchDecaySlider, &brightSlider, &damperSlider };
                    break;

                case ResonatorType::Plate:
                    decaySlider.setLabelText("DECAY");
                    brightSlider.setLabelText("BRIGHTNESS");
                    inharmSlider.setLabelText("METALLIC DENSITY");
                    bloomSlider.setLabelText("BLOOM");
                    damperSlider.setLabelText("DAMPING");
                    activeSliders = { &decaySlider, &brightSlider, &inharmSlider, &bloomSlider, &damperSlider };
                    break;

                case ResonatorType::SteelPan:
                    decaySlider.setLabelText("DECAY");
                    brightSlider.setLabelText("METALLIC RING");
                    materialSlider.setLabelText("RIM DAMPING");
                    bloomSlider.setLabelText("PAN BLOOM");
                    damperSlider.setLabelText("DAMPING");
                    activeSliders = { &decaySlider, &brightSlider, &materialSlider, &bloomSlider, &damperSlider };
                    break;
            }

            for (auto* s : activeSliders)
                s->setVisible(true);

            resized();
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(2.0f);

            g.setColour(CustomLookAndFeel::bgPanel);
            g.fillRoundedRectangle(bounds, 6.0f);

            g.setColour(CustomLookAndFeel::borderSubtle);
            g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.setColour(CustomLookAndFeel::accentCyan);
            g.drawText("RESONATOR (GEOMETRY)", static_cast<int>(bounds.getX()) + 10, static_cast<int>(bounds.getY()) + 6, 180, 16, juce::Justification::left, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(8);
            area.removeFromTop(20);

            typeBox.setBounds(area.removeFromTop(24).reduced(4, 0));
            area.removeFromTop(6);

            const int numActive = static_cast<int>(activeSliders.size());
            if (numActive > 0)
            {
                const int knobWidth = area.getWidth() / numActive;
                for (auto* s : activeSliders)
                {
                    s->setBounds(area.removeFromLeft(knobWidth).reduced(2));
                }
            }
        }

    private:
        PluginProcessor& processor;

        juce::ComboBox typeBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttach;

        CustomRotarySlider decaySlider;
        CustomRotarySlider brightSlider;
        CustomRotarySlider inharmSlider;
        CustomRotarySlider damperSlider;
        CustomRotarySlider bloomSlider;
        CustomRotarySlider materialSlider;
        CustomRotarySlider pitchDropSlider;
        CustomRotarySlider pitchDecaySlider;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> brightAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inharmAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> damperAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bloomAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> materialAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchDropAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchDecayAttach;

        std::vector<CustomRotarySlider*> activeSliders;
    };
}
