#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../CustomLookAndFeel.h"
#include "../Controls/CustomRotarySlider.h"
#include "../../PluginProcessor.h"

namespace ModelKeys
{
    class BodyPickupPanel : public juce::Component
    {
    public:
        BodyPickupPanel(PluginProcessor& p)
            : processor(p),
              bodySizeSlider("bodySize", "SIZE", p.getMidiLearnManager()),
              bodyResonanceSlider("bodyResonance", "RESONANCE", p.getMidiLearnManager()),
              bodyMixSlider("bodyMix", "BODY MIX", p.getMidiLearnManager()),
              pickupPosSlider("pickupPos", "POSITION", p.getMidiLearnManager()),
              pickupDriveSlider("pickupDrive", "BARK / DRIVE", p.getMidiLearnManager()),
              pickupToneSlider("pickupTone", "TONE", p.getMidiLearnManager())
        {
            // Body Controls
            addAndMakeVisible(bodyTypeBox);
            bodyTypeBox.addItem("Body: Off", 1);
            bodyTypeBox.addItem("Acoustic Guitar", 2);
            bodyTypeBox.addItem("Piano Soundboard", 3);
            bodyTypeBox.addItem("Violin Body", 4);
            bodyTypeBox.addItem("Rhodes Tonebar", 5);
            bodyTypeBox.addItem("Drum Shell", 6);
            bodyTypeBox.addItem("Wurli Reed Bar", 7);
            bodyTypeBox.addItem("Harp Soundbox", 8);
            bodyTypeBox.addItem("Marimba Resonator", 9);
            bodyTypeBox.addItem("Steel Drum Barrel", 10);

            bodyTypeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processor.getAPVTS(), "bodyType", bodyTypeBox);

            addAndMakeVisible(bodySizeSlider);
            bodySizeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "bodySize", bodySizeSlider);

            addAndMakeVisible(bodyResonanceSlider);
            bodyResonanceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "bodyResonance", bodyResonanceSlider);

            addAndMakeVisible(bodyMixSlider);
            bodyMixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "bodyMix", bodyMixSlider);

            // Pickup Controls
            addAndMakeVisible(pickupTypeBox);
            pickupTypeBox.addItem("Electromagnetic (Bark)", 1);
            pickupTypeBox.addItem("Piezo (Contact)", 2);
            pickupTypeBox.addItem("Microphone (Air)", 3);
            pickupTypeBox.addItem("Electrostatic (Wurli)", 4);
            pickupTypeBox.addItem("Clavinet Dual-Coil", 5);

            pickupTypeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processor.getAPVTS(), "pickupType", pickupTypeBox);

            addAndMakeVisible(pickupPosSlider);
            pickupPosAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "pickupPos", pickupPosSlider);

            addAndMakeVisible(pickupDriveSlider);
            pickupDriveAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "pickupDrive", pickupDriveSlider);

            addAndMakeVisible(pickupToneSlider);
            pickupToneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "pickupTone", pickupToneSlider);
        }

        ~BodyPickupPanel() override = default;

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(2.0f);

            g.setColour(CustomLookAndFeel::bgPanel);
            g.fillRoundedRectangle(bounds, 6.0f);

            g.setColour(CustomLookAndFeel::borderSubtle);
            g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

            // Sub-divider in the middle between Body and Pickup
            const float midX = bounds.getCentreX();
            g.setColour(CustomLookAndFeel::borderSubtle);
            g.drawLine(midX, bounds.getY() + 8.0f, midX, bounds.getBottom() - 8.0f, 1.0f);

            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.setColour(CustomLookAndFeel::accentAmber);
            g.drawText("BODY (ACOUSTIC COUPLING)", static_cast<int>(bounds.getX()) + 10, static_cast<int>(bounds.getY()) + 6, 200, 16, juce::Justification::left, false);

            g.setColour(CustomLookAndFeel::accentOrange);
            g.drawText("PICKUP & TRANSDUCTION", static_cast<int>(midX) + 10, static_cast<int>(bounds.getY()) + 6, 200, 16, juce::Justification::left, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(8);
            area.removeFromTop(20);

            auto bodyArea = area.removeFromLeft(area.getWidth() / 2).reduced(4, 0);
            auto pickupArea = area.reduced(4, 0);

            // Body half
            bodyTypeBox.setBounds(bodyArea.removeFromTop(24).reduced(2, 0));
            bodyArea.removeFromTop(6);
            const int bKnobW = bodyArea.getWidth() / 3;
            bodySizeSlider.setBounds(bodyArea.removeFromLeft(bKnobW).reduced(2));
            bodyResonanceSlider.setBounds(bodyArea.removeFromLeft(bKnobW).reduced(2));
            bodyMixSlider.setBounds(bodyArea.removeFromLeft(bKnobW).reduced(2));

            // Pickup half
            pickupTypeBox.setBounds(pickupArea.removeFromTop(24).reduced(2, 0));
            pickupArea.removeFromTop(6);
            const int pKnobW = pickupArea.getWidth() / 3;
            pickupPosSlider.setBounds(pickupArea.removeFromLeft(pKnobW).reduced(2));
            pickupDriveSlider.setBounds(pickupArea.removeFromLeft(pKnobW).reduced(2));
            pickupToneSlider.setBounds(pickupArea.removeFromLeft(pKnobW).reduced(2));
        }

    private:
        PluginProcessor& processor;

        juce::ComboBox bodyTypeBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bodyTypeAttach;
        CustomRotarySlider bodySizeSlider;
        CustomRotarySlider bodyResonanceSlider;
        CustomRotarySlider bodyMixSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bodySizeAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bodyResonanceAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bodyMixAttach;

        juce::ComboBox pickupTypeBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pickupTypeAttach;
        CustomRotarySlider pickupPosSlider;
        CustomRotarySlider pickupDriveSlider;
        CustomRotarySlider pickupToneSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pickupPosAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pickupDriveAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pickupToneAttach;
    };
}
