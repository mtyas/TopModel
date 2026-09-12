#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../CustomLookAndFeel.h"
#include "../Controls/CustomRotarySlider.h"
#include "../../PluginProcessor.h"
#include <vector>

namespace ModelKeys
{
    class ExciterPanel : public juce::Component, private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        ExciterPanel(PluginProcessor& p)
            : processor(p),
              hardnessSlider("hardness", "HARDNESS", p.getMidiLearnManager()),
              contactSlider("contactPos", "POSITION", p.getMidiLearnManager()),
              stiffnessSlider("stiffness", "STIFFNESS", p.getMidiLearnManager()),
              bowSpeedSlider("bowSpeed", "BOW SPEED", p.getMidiLearnManager()),
              bowForceSlider("bowForce", "BOW FORCE", p.getMidiLearnManager()),
              strikeClickSlider("strikeClick", "MALLET CLICK", p.getMidiLearnManager()),
              rosinGritSlider("rosinGrit", "ROSIN BITE", p.getMidiLearnManager()),
              densitySlider("particleDensity", "DENSITY", p.getMidiLearnManager()),
              scatterSlider("particleScatter", "SCATTER", p.getMidiLearnManager())
        {
            addAndMakeVisible(typeBox);
            typeBox.addItem("Strike (Hammer/Mallet)", 1);
            typeBox.addItem("Pluck (Pick/Snap)", 2);
            typeBox.addItem("Bow (Continuous Friction)", 3);
            typeBox.addItem("Noise (Granular Cluster)", 4);

            typeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processor.getAPVTS(), "exciterType", typeBox);

            // Add all sliders as children
            auto setupSlider = [this](CustomRotarySlider& slider, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attach, const juce::String& paramId)
            {
                addChildComponent(slider);
                attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    processor.getAPVTS(), paramId, slider);
            };

            setupSlider(hardnessSlider, hardnessAttach, "hardness");
            setupSlider(contactSlider, contactAttach, "contactPos");
            setupSlider(stiffnessSlider, stiffnessAttach, "stiffness");
            setupSlider(bowSpeedSlider, bowSpeedAttach, "bowSpeed");
            setupSlider(bowForceSlider, bowForceAttach, "bowForce");
            setupSlider(strikeClickSlider, strikeClickAttach, "strikeClick");
            setupSlider(rosinGritSlider, rosinGritAttach, "rosinGrit");
            setupSlider(densitySlider, densityAttach, "particleDensity");
            setupSlider(scatterSlider, scatterAttach, "particleScatter");

            // Listen to exciterType changes for dynamic GUI updates
            processor.getAPVTS().addParameterListener("exciterType", this);

            typeBox.onChange = [this]() { updateControls(); };
            updateControls();
        }

        ~ExciterPanel() override
        {
            processor.getAPVTS().removeParameterListener("exciterType", this);
        }

        void parameterChanged(const juce::String& parameterID, float newValue) override
        {
            juce::ignoreUnused(parameterID, newValue);
            juce::MessageManager::callAsync([this]() { updateControls(); });
        }

        void updateControls()
        {
            const auto excType = static_cast<ExciterType>(static_cast<int>(*processor.getAPVTS().getRawParameterValue("exciterType")));

            // Hide all sliders initially
            hardnessSlider.setVisible(false);
            contactSlider.setVisible(false);
            stiffnessSlider.setVisible(false);
            bowSpeedSlider.setVisible(false);
            bowForceSlider.setVisible(false);
            strikeClickSlider.setVisible(false);
            rosinGritSlider.setVisible(false);
            densitySlider.setVisible(false);
            scatterSlider.setVisible(false);

            activeSliders.clear();

            switch (excType)
            {
                case ExciterType::Strike:
                    hardnessSlider.setLabelText("HARDNESS");
                    contactSlider.setLabelText("POSITION");
                    stiffnessSlider.setLabelText("STIFFNESS");
                    strikeClickSlider.setLabelText("MALLET CLICK");
                    activeSliders = { &hardnessSlider, &contactSlider, &stiffnessSlider, &strikeClickSlider };
                    break;

                case ExciterType::Pluck:
                    hardnessSlider.setLabelText("HARDNESS");
                    contactSlider.setLabelText("POSITION");
                    stiffnessSlider.setLabelText("SNAP");
                    strikeClickSlider.setLabelText("PICK NOISE");
                    activeSliders = { &hardnessSlider, &contactSlider, &stiffnessSlider, &strikeClickSlider };
                    break;

                case ExciterType::Bow:
                    bowSpeedSlider.setLabelText("BOW SPEED");
                    bowForceSlider.setLabelText("BOW FORCE");
                    rosinGritSlider.setLabelText("ROSIN BITE");
                    contactSlider.setLabelText("BOW POS");
                    activeSliders = { &bowSpeedSlider, &bowForceSlider, &rosinGritSlider, &contactSlider };
                    break;

                case ExciterType::Noise:
                    densitySlider.setLabelText("DENSITY");
                    scatterSlider.setLabelText("SCATTER");
                    hardnessSlider.setLabelText("GRAIN COLOR");
                    contactSlider.setLabelText("POSITION");
                    activeSliders = { &densitySlider, &scatterSlider, &hardnessSlider, &contactSlider };
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

            // Header title
            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.setColour(CustomLookAndFeel::accentAmber);
            g.drawText("EXCITER (CONTACT)", static_cast<int>(bounds.getX()) + 10, static_cast<int>(bounds.getY()) + 6, 160, 16, juce::Justification::left, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(8);
            area.removeFromTop(20); // Header gap

            // Type selector at top of panel
            typeBox.setBounds(area.removeFromTop(24).reduced(4, 0));
            area.removeFromTop(6);

            // Dynamically layout active rotary knobs evenly
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

        CustomRotarySlider hardnessSlider;
        CustomRotarySlider contactSlider;
        CustomRotarySlider stiffnessSlider;
        CustomRotarySlider bowSpeedSlider;
        CustomRotarySlider bowForceSlider;
        CustomRotarySlider strikeClickSlider;
        CustomRotarySlider rosinGritSlider;
        CustomRotarySlider densitySlider;
        CustomRotarySlider scatterSlider;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hardnessAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> contactAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stiffnessAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bowSpeedAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bowForceAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strikeClickAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rosinGritAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> densityAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scatterAttach;

        std::vector<CustomRotarySlider*> activeSliders;
    };
}
