#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Midi/MidiLearnManager.h"
#include "../CustomLookAndFeel.h"

namespace ModelKeys
{
    class CustomRotarySlider : public juce::Slider
    {
    public:
        CustomRotarySlider(const juce::String& paramId, const juce::String& labelText, MidiLearnManager& midiLearn)
            : parameterId(paramId), label(labelText), midiManager(midiLearn)
        {
            setSliderStyle(juce::Slider::RotaryVerticalDrag);
            setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            setName(paramId);
        }

        ~CustomRotarySlider() override = default;

        const juce::String& getParameterId() const noexcept { return parameterId; }
        const juce::String& getLabelText() const noexcept { return label; }
        void setLabelText(const juce::String& newLabel) { label = newLabel; repaint(); }

        void paint(juce::Graphics& g) override
        {
            juce::Slider::paint(g);

            auto bounds = getLocalBounds();

            // Draw label below slider
            g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
            g.setColour(CustomLookAndFeel::textDimmed);
            auto labelArea = bounds.removeFromBottom(16);
            g.drawFittedText(label, labelArea, juce::Justification::centred, 1);

            // Draw MIDI CC badge if mapped
            const int cc = midiManager.getCCForParameter(parameterId.toStdString());
            if (cc >= 0)
            {
                auto badgeArea = juce::Rectangle<int>(bounds.getX() + 2, bounds.getY() + 2, 28, 12);
                g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.2f));
                g.fillRoundedRectangle(badgeArea.toFloat(), 3.0f);
                g.setColour(CustomLookAndFeel::accentAmber);
                g.drawRoundedRectangle(badgeArea.toFloat(), 3.0f, 1.0f);
                g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
                g.drawText("CC" + juce::String(cc), badgeArea, juce::Justification::centred, false);
            }

            // Draw glowing border if active learn target
            if (midiManager.getLearnActive() && midiManager.getTargetParameter() == parameterId.toStdString())
            {
                g.setColour(CustomLookAndFeel::accentCyan);
                g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 4.0f, 1.8f);
            }
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                showContextMenu();
                return;
            }

            if (midiManager.getLearnActive())
            {
                midiManager.setTargetParameter(parameterId.toStdString());
                repaint();
                return;
            }

            juce::Slider::mouseDown(e);
        }

    private:
        void showContextMenu()
        {
            juce::PopupMenu menu;
            const int cc = midiManager.getCCForParameter(parameterId.toStdString());

            menu.addItem(1, "MIDI Learn (" + label + ")");
            if (cc >= 0)
            {
                menu.addItem(2, "Clear MIDI CC " + juce::String(cc));
            }

            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                [this, cc](int result)
                {
                    if (result == 1)
                    {
                        midiManager.setTargetParameter(parameterId.toStdString());
                        midiManager.setLearnActive(true);
                        repaint();
                    }
                    else if (result == 2)
                    {
                        midiManager.clearParameter(parameterId.toStdString());
                        repaint();
                    }
                });
        }

        juce::String parameterId;
        juce::String label;
        MidiLearnManager& midiManager;
    };
}
