#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../CustomLookAndFeel.h"
#include "../Controls/CustomRotarySlider.h"
#include "../../PluginProcessor.h"

namespace ModelKeys
{
    class FXPanel : public juce::Component
    {
    public:
        FXPanel(PluginProcessor& p)
            : processor(p),
              preampDriveSlider("preampDrive", "DRIVE", p.getMidiLearnManager()),
              preampToneSlider("preampTone", "TONE", p.getMidiLearnManager()),
              preampLevelSlider("preampLevel", "LEVEL", p.getMidiLearnManager()),
              chorusRateSlider("chorusRate", "RATE", p.getMidiLearnManager()),
              chorusDepthSlider("chorusDepth", "DEPTH", p.getMidiLearnManager()),
              chorusMixSlider("chorusMix", "MOD MIX", p.getMidiLearnManager()),
              reverbSizeSlider("reverbSize", "SIZE", p.getMidiLearnManager()),
              reverbDampSlider("reverbDamp", "DAMP", p.getMidiLearnManager()),
              reverbMixSlider("reverbMix", "REV MIX", p.getMidiLearnManager())
        {
            // Section 1: Preamp Saturation
            addAndMakeVisible(preampModeBox);
            preampModeBox.addItem("Clean", 1);
            preampModeBox.addItem("Warm Tube", 2);
            preampModeBox.addItem("Analog Tape", 3);
            preampModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processor.getAPVTS(), "preampMode", preampModeBox);

            addAndMakeVisible(preampDriveSlider);
            preampDriveAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "preampDrive", preampDriveSlider);

            addAndMakeVisible(preampToneSlider);
            preampToneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "preampTone", preampToneSlider);

            addAndMakeVisible(preampLevelSlider);
            preampLevelAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "preampLevel", preampLevelSlider);

            // Section 2: Modulation Multi-FX
            addAndMakeVisible(modModeBox);
            modModeBox.addItem("Chorus", 1);
            modModeBox.addItem("Tremolo", 2);
            modModeBox.addItem("Flanger", 3);
            modModeBox.addItem("Ensemble", 4);
            modModeBox.addItem("Phaser", 5);
            modModeBox.addItem("Wow & Flutter", 6);
            modModeBox.addItem("Tape Delay", 7);
            modModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processor.getAPVTS(), "chorusTremoloMode", modModeBox);

            addAndMakeVisible(chorusRateSlider);
            chorusRateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "chorusRate", chorusRateSlider);

            addAndMakeVisible(chorusDepthSlider);
            chorusDepthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "chorusDepth", chorusDepthSlider);

            addAndMakeVisible(chorusMixSlider);
            chorusMixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "chorusMix", chorusMixSlider);

            // Section 3: Reverb Types (Plate, Room, Hall, Spring)
            addAndMakeVisible(reverbTypeBox);
            reverbTypeBox.addItem("Plate", 1);
            reverbTypeBox.addItem("Room", 2);
            reverbTypeBox.addItem("Hall", 3);
            reverbTypeBox.addItem("Spring", 4);
            reverbTypeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processor.getAPVTS(), "reverbType", reverbTypeBox);

            addAndMakeVisible(reverbSizeSlider);
            reverbSizeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "reverbSize", reverbSizeSlider);

            addAndMakeVisible(reverbDampSlider);
            reverbDampAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "reverbDamp", reverbDampSlider);

            addAndMakeVisible(reverbMixSlider);
            reverbMixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), "reverbMix", reverbMixSlider);
        }

        ~FXPanel() override = default;

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(2.0f);

            g.setColour(CustomLookAndFeel::bgPanel);
            g.fillRoundedRectangle(bounds, 6.0f);

            g.setColour(CustomLookAndFeel::borderSubtle);
            g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

            // Section dividers
            const float thirdW = bounds.getWidth() / 3.0f;
            g.drawLine(bounds.getX() + thirdW, bounds.getY() + 8.0f, bounds.getX() + thirdW, bounds.getBottom() - 8.0f, 1.0f);
            g.drawLine(bounds.getX() + thirdW * 2.0f, bounds.getY() + 8.0f, bounds.getX() + thirdW * 2.0f, bounds.getBottom() - 8.0f, 1.0f);

            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.setColour(CustomLookAndFeel::accentAmber);
            g.drawText("ANALOG PREAMP", static_cast<int>(bounds.getX()) + 10, static_cast<int>(bounds.getY()) + 6, 130, 16, juce::Justification::left, false);

            g.setColour(CustomLookAndFeel::accentCyan);
            g.drawText("MODULATION FX", static_cast<int>(bounds.getX() + thirdW) + 10, static_cast<int>(bounds.getY()) + 6, 130, 16, juce::Justification::left, false);

            g.setColour(CustomLookAndFeel::textBright);
            g.drawText("REVERB ACOUSTICS", static_cast<int>(bounds.getX() + thirdW * 2.0f) + 10, static_cast<int>(bounds.getY()) + 6, 140, 16, juce::Justification::left, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(8);
            area.removeFromTop(20);

            const int thirdW = area.getWidth() / 3;

            // Section 1: Preamp (mode box + 3 knobs)
            auto preArea = area.removeFromLeft(thirdW).reduced(4, 0);
            preampModeBox.setBounds(preArea.removeFromTop(24).reduced(2, 0));
            preArea.removeFromTop(4);
            const int preKnobW = preArea.getWidth() / 3;
            preampDriveSlider.setBounds(preArea.removeFromLeft(preKnobW).reduced(2));
            preampToneSlider.setBounds(preArea.removeFromLeft(preKnobW).reduced(2));
            preampLevelSlider.setBounds(preArea.removeFromLeft(preKnobW).reduced(2));

            // Section 2: Modulation (mode box + 3 knobs)
            auto modArea = area.removeFromLeft(thirdW).reduced(4, 0);
            modModeBox.setBounds(modArea.removeFromTop(24).reduced(2, 0));
            modArea.removeFromTop(4);
            const int modKnobW = modArea.getWidth() / 3;
            chorusRateSlider.setBounds(modArea.removeFromLeft(modKnobW).reduced(2));
            chorusDepthSlider.setBounds(modArea.removeFromLeft(modKnobW).reduced(2));
            chorusMixSlider.setBounds(modArea.removeFromLeft(modKnobW).reduced(2));

            // Section 3: Reverb (type box + 3 knobs)
            auto revArea = area.reduced(4, 0);
            reverbTypeBox.setBounds(revArea.removeFromTop(24).reduced(2, 0));
            revArea.removeFromTop(4);
            const int revKnobW = revArea.getWidth() / 3;
            reverbSizeSlider.setBounds(revArea.removeFromLeft(revKnobW).reduced(2));
            reverbDampSlider.setBounds(revArea.removeFromLeft(revKnobW).reduced(2));
            reverbMixSlider.setBounds(revArea.removeFromLeft(revKnobW).reduced(2));
        }

    private:
        PluginProcessor& processor;

        juce::ComboBox preampModeBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> preampModeAttach;
        CustomRotarySlider preampDriveSlider;
        CustomRotarySlider preampToneSlider;
        CustomRotarySlider preampLevelSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preampDriveAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preampToneAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preampLevelAttach;

        juce::ComboBox modModeBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modModeAttach;
        CustomRotarySlider chorusRateSlider;
        CustomRotarySlider chorusDepthSlider;
        CustomRotarySlider chorusMixSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusRateAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusDepthAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusMixAttach;

        juce::ComboBox reverbTypeBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> reverbTypeAttach;
        CustomRotarySlider reverbSizeSlider;
        CustomRotarySlider reverbDampSlider;
        CustomRotarySlider reverbMixSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbSizeAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbDampAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbMixAttach;
    };
}
